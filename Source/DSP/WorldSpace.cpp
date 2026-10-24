#include "WorldSpace.h"

namespace wb {

using namespace dsp;

namespace {
// Mutually prime-ish line lengths in milliseconds at size 1.
constexpr float lineMs[WorldSpace::lines] { 29.7f, 37.1f, 41.1f, 43.7f, 53.3f, 59.9f, 67.7f, 73.1f };
constexpr float diffuserMs[2][4] { { 4.7f, 3.6f, 12.7f, 9.3f }, { 5.3f, 3.1f, 11.9f, 8.7f } };
constexpr float maxSize = 2.4f, maxPreDelayMs = 150.0f, modDepthMs = 0.35f;
}

void WorldSpace::prepare(double sampleRate) {
    rate = float(sampleRate);
    for (int i = 0; i < lines; ++i) {
        baseLength[std::size_t(i)] = lineMs[i] * 0.001f * rate;
        delay[std::size_t(i)].prepare(std::size_t(baseLength[std::size_t(i)] * maxSize + modDepthMs * 0.001f * rate * 2 + 8));
        modRate[std::size_t(i)] = 0.07f + 0.045f * float(i);
    }
    for (int c = 0; c < 2; ++c)
        for (int s = 0; s < 4; ++s) diffuser[std::size_t(c)][std::size_t(s)].prepare(std::size_t(diffuserMs[c][s] * 0.001f * rate));
    for (auto& p : preDelay) p.prepare(std::size_t(maxPreDelayMs * 0.001f * rate + 8));
    const float controlRate = rate / float(chunk);
    size.setTime(2.0f, controlRate);   // slow: size changes bend pitch while they happen
    decay.setTime(0.5f, controlRate);
    preDelaySamples.setTime(0.5f, controlRate);
    dampHz.setTime(0.3f, controlRate);
    reset();
}

void WorldSpace::reset() {
    for (auto& d : delay) d.clear();
    for (auto& f : damping) f.reset();
    for (auto& f : lowCut) f.reset();
    for (auto& side : diffuser)
        for (auto& a : side) a.clear();
    for (auto& p : preDelay) p.clear();
    for (int i = 0; i < lines; ++i) modPhase[std::size_t(i)] = 0.9f * float(i);
    size.snap(1);
    decay.snap(2.5f);
    preDelaySamples.snap(0.02f * rate);
    dampHz.snap(6000);
}

void WorldSpace::update(const WorldFrame& world) {
    const float scale = world[Scale], temp = world[Temperature], wet = world[Wetness];
    diffusionStages = world.quality == 0 ? 2 : 4;
    modulate = world.quality > 0;

    const float s = size.next(lerpExp(0.45f, maxSize, scale));
    // Ice reverberates longer, a drenched or scorching world shorter; magic makes the air ring.
    const float magic = world[Magic];
    const float target = lerpExp(0.7f, 9.0f, scale) * lerp(1.35f, 0.85f, temp) * lerp(1.0f, 0.8f, wet) * lerp(1.0f, 1.5f, magic);
    t60 = decay.next(std::clamp(target, 0.3f, 14.0f));
    preDelaySamples.next(lerp(4.0f, 110.0f, scale * scale) * 0.001f * rate);
    const float damp = dampHz.next(lerpExp(11000.0f, 3500.0f, clamp01(0.75f * temp + 0.5f * wet - 0.4f * magic)));
    for (int i = 0; i < lines; ++i) {
        const float length = baseLength[std::size_t(i)] * s;
        feedback[std::size_t(i)] = std::pow(10.0f, -3.0f * length / (t60 * rate));
        damping[std::size_t(i)].setCutoff(damp, rate);
        lowCut[std::size_t(i)].setCutoff(40.0f, rate);
    }
    for (auto& side : diffuser)
        for (auto& a : side) a.g = lerp(0.55f, 0.7f, wet);
}

void WorldSpace::process(const float* inL, const float* inR, float* outL, float* outR, int n) {
    const float s = size.value;
    const float pre = std::max(1.0f, preDelaySamples.value);
    const float modDepth = modulate ? modDepthMs * 0.001f * rate : 0.0f;
    const float controlDt = float(n) / rate;

    // Modulated lengths, computed once per chunk (the chunk is short).
    std::array<float, lines> length {};
    for (int i = 0; i < lines; ++i) {
        auto& ph = modPhase[std::size_t(i)];
        ph += twoPi * modRate[std::size_t(i)] * controlDt;
        if (ph > twoPi) ph -= twoPi;
        length[std::size_t(i)] = baseLength[std::size_t(i)] * s + modDepth * (1.0f + std::sin(ph));
    }

    for (int k = 0; k < n; ++k) {
        preDelay[0].push(inL[k]);
        preDelay[1].push(inR[k]);
        float xl = preDelay[0].read(pre), xr = preDelay[1].read(pre);
        for (int d = 0; d < diffusionStages; ++d) {
            xl = diffuser[0][std::size_t(d)].process(xl);
            xr = diffuser[1][std::size_t(d)].process(xr);
        }

        std::array<float, lines> y {};
        float sum = 0;
        for (int i = 0; i < lines; ++i) {
            float v = delay[std::size_t(i)].read(length[std::size_t(i)]);
            v = damping[std::size_t(i)].lp(v);
            v = lowCut[std::size_t(i)].hp(v);
            y[std::size_t(i)] = v * feedback[std::size_t(i)];
            sum += y[std::size_t(i)];
        }
        // Householder reflection: y - (2/N) * sum(y). Lossless, dense mixing.
        const float h = sum * (2.0f / float(lines));
        float wl = 0, wr = 0;
        for (int i = 0; i < lines; ++i) {
            const float in = (i % 2 == 0 ? xl : xr) * ((i & 2) ? -1.0f : 1.0f);
            delay[std::size_t(i)].push(finiteOr(y[std::size_t(i)] - h + in));
            const float tap = y[std::size_t(i)] * ((i & 4) ? -1.0f : 1.0f);
            if (i % 2 == 0) wl += tap; else wr += tap;
        }
        outL[k] += wl * 0.5f;
        outR[k] += wr * 0.5f;
    }
}

} // namespace wb
