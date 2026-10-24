#include "WorldEngine.h"

namespace wb {

using namespace dsp;

WorldEngine::WorldEngine() {
    layers[Air] = &air;
    layers[Bed] = &bed;
}

void WorldEngine::prepare(double sampleRate, const EngineControls& initial) {
    rate = sampleRate;
    const float r = float(rate), controlRate = r / float(chunk);
    for (auto* layer : layers)
        if (layer != nullptr) layer->prepare(rate);
    space.prepare(rate);
    glue.prepare(r);
    limiter.prepare(r);
    for (auto& s : worldSmooth) s.setTime(0.25f, controlRate);
    for (auto& s : layerGain) s.setTime(0.03f, r);
    master.setTime(0.05f, r);
    width.setTime(0.1f, r);
    spaceMix.setTime(0.1f, r);
    powerStep = 1.0f / (2.0f * r);   // Power fades over 2 s
    reset(initial);
}

void WorldEngine::reset(const EngineControls& initial) {
    for (auto* layer : layers)
        if (layer != nullptr) layer->reset();
    space.reset();
    for (auto& f : distance) f.reset();
    glue.reset();
    limiter.reset();
    for (std::size_t i = 0; i < worldSmooth.size(); ++i) {
        worldSmooth[i].snap(clamp01(initial.world[i]));
        world.c[i] = worldSmooth[i].value;
    }
    for (auto& g : layerGain) g.snap(0);
    master.snap(dbToGain(initial.masterDb));
    width.snap(initial.width);
    spaceMix.snap(initial.spaceMix);
    seed = initial.seed + 1;   // forces setSeed on the first chunk
    // A freshly loaded world fades in, like switching Power on.
    powerRamp = 0;
    masterPeak = 0;
    clock = 0;
    fifoRead = chunk;
    layerPeak.fill(0);
}

void WorldEngine::resync(uint64_t position) {
    // Continue from the new position at once; the few samples already rendered ahead
    // belong to the old one.
    clock = position;
    fifoRead = chunk;
    for (auto* layer : layers)
        if (layer != nullptr) layer->resync(clock);
}

void WorldEngine::process(float* left, float* right, int n, const EngineControls& controls) {
    layerPeak.fill(0);
    masterPeak = 0;
    // The world is rendered in whole chunks into a small FIFO: control updates then
    // happen at the same positions whatever block size the host uses. An instrument
    // has no input, so rendering a chunk ahead adds no latency.
    for (int done = 0; done < n;) {
        if (fifoRead == chunk) {
            renderChunk(fifoL.data(), fifoR.data(), chunk, clock, controls);
            clock += chunk;
            fifoRead = 0;
        }
        const int count = std::min(chunk - fifoRead, n - done);
        std::copy_n(fifoL.data() + fifoRead, count, left + done);
        std::copy_n(fifoR.data() + fifoRead, count, right + done);
        fifoRead += count;
        done += count;
    }
    for (std::size_t i = 0; i < layerMeter.size(); ++i)
        layerMeter[i].store(std::max(layerPeak[i], layerMeter[i].load(std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
    masterMeter.store(std::max(masterPeak, masterMeter.load(std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
    powerLevel.store(powerRamp, std::memory_order_relaxed);
}

void WorldEngine::renderChunk(float* left, float* right, int n, uint64_t clock, const EngineControls& controls) {
    // Fully powered down: silence, and no work.
    if (!controls.power && powerRamp <= 0.0f) {
        std::fill(left, left + n, 0.0f);
        std::fill(right, right + n, 0.0f);
        return;
    }

    if (controls.seed != seed) {
        seed = controls.seed;
        for (auto* layer : layers)
            if (layer != nullptr) layer->setSeed(seed);
    }
    for (std::size_t i = 0; i < worldSmooth.size(); ++i) world.c[i] = worldSmooth[i].next(clamp01(finiteOr(controls.world[i], 0.5f)));
    world.root = controls.root;
    world.mood = controls.mood;
    world.quality = controls.quality;
    world.seed = seed;

    // Scale: colossal worlds sound further away, darker and more reverberant.
    const float scale = world[Scale];
    const float distanceSend = lerp(0.6f, 1.5f, scale);
    for (auto& f : distance) f.setCutoff(lerpExp(20000.0f, 6500.0f, scale * std::sqrt(scale)), float(rate));

    bool anySolo = false;
    for (bool s : controls.solo) anySolo |= s;

    std::fill(dryL.begin(), dryL.begin() + n, 0.0f);
    std::fill(dryR.begin(), dryR.begin() + n, 0.0f);
    std::fill(sendL.begin(), sendL.begin() + n, 0.0f);
    std::fill(sendR.begin(), sendR.begin() + n, 0.0f);

    for (std::size_t li = 0; li < layers.size(); ++li) {
        Generator* layer = layers[li];
        if (layer == nullptr) continue;
        const bool audible = !controls.mute[li] && (!anySolo || controls.solo[li]);
        const float target = audible ? dbToGain(std::clamp(controls.layerDb[li], -60.0f, 6.0f)) : 0.0f;
        auto& gain = layerGain[li];
        if (target == 0.0f && gain.value < 1.0e-5f) {
            gain.snap(0);
            continue;
        }
        layer->render(world, clock, layerL.data(), layerR.data(), n);
        const float send = layer->spaceSend(world) * distanceSend;
        float peak = layerPeak[li];
        for (int i = 0; i < n; ++i) {
            const float g = gain.next(target);
            const float l = finiteOr(layerL[std::size_t(i)]) * g, r = finiteOr(layerR[std::size_t(i)]) * g;
            peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
            dryL[std::size_t(i)] += l;
            dryR[std::size_t(i)] += r;
            sendL[std::size_t(i)] += l * send;
            sendR[std::size_t(i)] += r * send;
        }
        layerPeak[li] = peak;
    }

    space.update(world);
    std::array<float, chunk> wetL {}, wetR {};
    space.process(sendL.data(), sendR.data(), wetL.data(), wetR.data(), n);

    const float masterTarget = dbToGain(std::clamp(controls.masterDb, -60.0f, 6.0f));
    const float widthTarget = std::clamp(controls.width, 0.0f, 2.0f);
    const float mixTarget = clamp01(controls.spaceMix);
    const float powerTarget = controls.power ? 1.0f : 0.0f;
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        const float mix = spaceMix.next(mixTarget);
        float l = distance[0].lp(dryL[k]) * (1.0f - 0.4f * mix) + wetL[k] * mix * 1.6f;
        float r = distance[1].lp(dryR[k]) * (1.0f - 0.4f * mix) + wetR[k] * mix * 1.6f;

        const float w = width.next(widthTarget);
        const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
        l = mid + side;
        r = mid - side;

        l = finiteOr(l);
        r = finiteOr(r);
        glue.process(l, r);

        powerRamp = powerRamp < powerTarget ? std::min(powerTarget, powerRamp + powerStep) : std::max(powerTarget, powerRamp - powerStep);
        const float fade = 0.5f - 0.5f * std::cos(pi * powerRamp);
        const float g = master.next(masterTarget) * fade;
        l *= g;
        r *= g;

        limiter.process(l, r);
        l = finiteOr(l);
        r = finiteOr(r);
        masterPeak = std::max(masterPeak, std::max(std::abs(l), std::abs(r)));
        left[i] = l;
        right[i] = r;
    }
}

} // namespace wb
