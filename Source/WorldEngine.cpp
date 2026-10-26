#include "WorldEngine.h"

namespace wb {

using namespace dsp;

WorldEngine::WorldEngine() {
    layers[Air] = &air;
    layers[Water] = &water;
    layers[Fire] = &fire;
    layers[Earth] = &earth;
    layers[Thunder] = &thunder;
    layers[Bed] = &bed;
}

void WorldEngine::prepare(double sampleRate, const EngineControls& initial) {
    rate = sampleRate;
    const float r = float(rate), controlRate = r / float(chunk);
    for (auto* layer : layers)
        if (layer != nullptr) layer->prepare(rate);
    for (int i = 0; i < placeSlots; ++i) {
        slots[std::size_t(i)].prepare(rate, i);
        slots[std::size_t(i)].setFeedback(&patternsOut[std::size_t(i)]);
    }
    space.prepare(rate);
    sharedFx.prepare(r);
    planet.prepare(r);
    glue.prepare(r);
    limiter.prepare(r);
    for (auto& s : worldSmooth) s.setTime(0.25f, controlRate);
    // Score Intensity glides (a cue builds, it never jumps).
    intensity.setTime(0.5f, controlRate);
    for (auto& s : layerGain) s.setTime(0.03f, r);
    master.setTime(0.05f, r);
    width.setTime(0.1f, r);
    spaceMix.setTime(0.1f, r);
    powerStep = 1.0f / (2.0f * r);   // Power fades over 2 s
    seedStep = 1.0f / (0.03f * r);   // a new Seed: the environment dips out and back over 30 ms each
    gateUp = 1.0f / (0.01f * r);     // Transport: in over 10 ms, out over 250 ms
    gateDown = 1.0f / (0.25f * r);
    reset(initial);
}

void WorldEngine::reset(const EngineControls& initial) {
    for (auto* layer : layers)
        if (layer != nullptr) layer->reset();
    for (auto& slot : slots) slot.reset();
    space.reset();
    sharedFx.clear();
    for (auto& f : distance) f.reset();
    glue.reset();
    limiter.reset();
    planet.reset(initial.planet, initial.planetInfluence);
    for (std::size_t i = 0; i < worldSmooth.size(); ++i) {
        worldSmooth[i].snap(clamp01(initial.world[i]));
        world.c[i] = worldSmooth[i].value;
    }
    for (auto& g : layerGain) g.snap(0);
    master.snap(dbToGain(initial.masterDb));
    width.snap(initial.width);
    spaceMix.snap(initial.spaceMix);
    intensity.snap(clamp01(initial.intensity));
    world.intensity = intensity.value;
    seed = initial.seed + 1;   // forces setSeed on the first chunk
    seedFresh = true;
    seedGain = 1;
    // A freshly loaded world fades in, like switching Power on.
    powerRamp = 0;
    gateRamp = 1;
    masterPeak = 0;
    clock = 0;
    beat = 0;
    bpm = initial.hostBpm > 0 ? initial.hostBpm : defaultBpm;
    fifoRead = chunk;
    layerPeak.fill(0);
    placePeak.fill(0);
}

void WorldEngine::restart(uint64_t position, double ppq, const EngineControls& controls) {
    reset(controls);
    // No slow Power fade: the world is there from the first beat.
    powerRamp = controls.power ? 1.0f : 0.0f;
    gateRamp = 0;
    resync(position, ppq);
}

void WorldEngine::resync(uint64_t position, double ppq) {
    // Continue from the new position at once; the few samples already rendered ahead
    // belong to the old one.
    clock = position;
    beat = ppq >= 0 ? ppq : double(position) / rate * double(bpm) / 60.0;
    fifoRead = chunk;
    for (auto* layer : layers)
        if (layer != nullptr) layer->resync(clock);
}

void WorldEngine::process(float* left, float* right, int n, const EngineControls& controls) {
    layerPeak.fill(0);
    placePeak.fill(0);
    masterPeak = 0;
    if (controls.hostBpm >= 20.0f && controls.hostBpm <= 999.0f) bpm = controls.hostBpm;
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
    for (std::size_t i = 0; i < placeMeter.size(); ++i)
        placeMeter[i].store(std::max(placePeak[i], placeMeter[i].load(std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
    masterMeter.store(std::max(masterPeak, masterMeter.load(std::memory_order_relaxed) * 0.9f), std::memory_order_relaxed);
    powerLevel.store(powerRamp, std::memory_order_relaxed);
    // A muted thunder layer is not rendered, so its flash would stand still: show none.
    lightning.store(layerGain[Thunder].value > 1.0e-5f ? thunder.flash() * powerRamp : 0.0f, std::memory_order_relaxed);
}

void WorldEngine::renderChunk(float* left, float* right, int n, uint64_t clock, const EngineControls& controls) {
    // Fully powered down: silence, and no work.
    if ((!controls.power && powerRamp <= 0.0f) || (!controls.gate && gateRamp <= 0.0f)) {
        std::fill(left, left + n, 0.0f);
        std::fill(right, right + n, 0.0f);
        return;
    }

    // A new Seed reseeds the environment layers, whose noise and gusts then follow
    // other curves: they dip out over 30 ms, change at silence and come back, so the
    // change never clicks (a fresh world takes its seed at once).
    const bool reseed = controls.seed != seed;
    if (reseed && seedFresh) {
        seed = controls.seed;
        for (auto* layer : layers)
            if (layer != nullptr) layer->setSeed(seed);
    }
    seedFresh = false;
    const float duckFrom = seedGain;
    const float duckTo = controls.seed != seed ? std::max(0.0f, seedGain - seedStep * float(n)) : std::min(1.0f, seedGain + seedStep * float(n));
    seedGain = duckTo;
    for (std::size_t i = 0; i < worldSmooth.size(); ++i) world.c[i] = worldSmooth[i].next(clamp01(finiteOr(controls.world[i], 0.5f)));
    world.intensity = intensity.next(clamp01(finiteOr(controls.intensity, 0.5f)));
    world.root = controls.root;
    world.mood = controls.mood;
    world.quality = controls.quality;
    world.seed = seed;
    world.beat = beat;
    world.bpm = bpm;
    world.chord = music::chordDegree(beat, world[Danger]);

    // Scale: colossal worlds sound further away, darker and more reverberant.
    const float scale = world[Scale];
    const float distanceSend = lerp(0.6f, 1.5f, scale);
    for (auto& f : distance) f.setCutoff(lerpExp(20000.0f, 6500.0f, scale * std::sqrt(scale)), float(rate));

    bool anySolo = false;
    for (bool s : controls.solo) anySolo |= s;
    for (const auto& p : controls.places) anySolo |= p.solo && p.type != NoPlace;

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
        const bool ducking = duckFrom < 1.0f || duckTo < 1.0f;
        for (int i = 0; i < n; ++i) {
            const float duck = ducking ? 0.5f - 0.5f * std::cos(pi * lerp(duckFrom, duckTo, float(i + 1) / float(n))) : 1.0f;
            const float g = gain.next(target) * duck;
            const float l = finiteOr(layerL[std::size_t(i)]) * g, r = finiteOr(layerR[std::size_t(i)]) * g;
            peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
            dryL[std::size_t(i)] += l;
            dryR[std::size_t(i)] += r;
            sendL[std::size_t(i)] += l * send;
            sendR[std::size_t(i)] += r * send;
        }
        layerPeak[li] = peak;
    }

    if (controls.seed != seed && duckTo <= 0.0f) {
        seed = controls.seed;
        for (auto* layer : layers)
            if (layer != nullptr) layer->setSeed(seed);
    }

    // Places: each slot renders its Place with level, distance and pan applied, and
    // its instruments feed the shared reverbs and delays.
    sharedFx.begin(n);
    for (std::size_t si = 0; si < slots.size(); ++si) {
        const auto& pc = controls.places[si];
        const bool audible = !pc.mute && (!anySolo || pc.solo);
        patternsIn[si].update();
        const auto& slotPatterns = controls.patterns[si] != nullptr ? *controls.patterns[si] : patternsIn[si].read();
        const bool sounding = slots[si].render(world, pc, slotPatterns, audible, clock, beat, bpm, layerL.data(), layerR.data(), n);
        slotBeat[si].store(slots[si].beat(), std::memory_order_relaxed);
        if (!sounding) continue;
        sharedFx.add(slots[si].sends(), slots[si].sendGainL(), slots[si].sendGainR(), n);
        const float send = slots[si].spaceSend() * distanceSend;
        float peak = placePeak[si];
        for (int i = 0; i < n; ++i) {
            const float l = layerL[std::size_t(i)], r = layerR[std::size_t(i)];
            peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
            dryL[std::size_t(i)] += l;
            dryR[std::size_t(i)] += r;
            sendL[std::size_t(i)] += l * send;
            sendR[std::size_t(i)] += r * send;
        }
        placePeak[si] = peak;
    }
    beat += double(bpm) / 60.0 * double(n) / rate;

    space.update(world);
    std::array<float, chunk> wetL {}, wetR {};
    space.process(sendL.data(), sendR.data(), wetL.data(), wetR.data(), n);
    std::array<float, chunk> fxL {}, fxR {};
    sharedFx.process(fxL.data(), fxR.data(), n, bpm);

    // The Planet's colour (it glides to a new planet or Influence on its own).
    planet.update(controls.planet, controls.planetInfluence, double(clock) / rate);
    const float planetSpace = planet.space(), planetWidth = planet.width();

    const float masterTarget = dbToGain(std::clamp(controls.masterDb, -60.0f, 6.0f));
    const float widthTarget = std::clamp(controls.width, 0.0f, 2.0f);
    const float mixTarget = clamp01(controls.spaceMix);
    const float powerTarget = controls.power ? 1.0f : 0.0f;
    const bool gateOpen = controls.gate;
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        const float mix = spaceMix.next(mixTarget);
        float l = distance[0].lp(dryL[k]) * (1.0f - 0.4f * mix) + wetL[k] * mix * 1.6f * planetSpace + fxL[k];
        float r = distance[1].lp(dryR[k]) * (1.0f - 0.4f * mix) + wetR[k] * mix * 1.6f * planetSpace + fxR[k];
        planet.process(l, r, clock + uint64_t(i));

        const float w = width.next(widthTarget) * planetWidth;
        const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * w;
        l = mid + side;
        r = mid - side;

        l = finiteOr(l);
        r = finiteOr(r);
        glue.process(l, r);

        powerRamp = powerRamp < powerTarget ? std::min(powerTarget, powerRamp + powerStep) : std::max(powerTarget, powerRamp - powerStep);
        gateRamp = gateOpen ? std::min(1.0f, gateRamp + gateUp) : std::max(0.0f, gateRamp - gateDown);
        const float fade = (0.5f - 0.5f * std::cos(pi * powerRamp)) * gateRamp * gateRamp;
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
