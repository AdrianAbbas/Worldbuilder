#pragma once
#include "Core.h"

namespace wb::dsp {

// Gentle stereo-linked glue compressor: slow RMS detector, 2:1 above -18 dBFS
// with a soft knee. It evens out the world rather than squashing it.
struct Glue {
    float env = 0, gain = 1, envCoeff = 0, attack = 0, release = 0;
    void prepare(float rate) {
        envCoeff = 1.0f - std::exp(-1.0f / (0.05f * rate));
        attack = 1.0f - std::exp(-1.0f / (0.03f * rate));
        release = 1.0f - std::exp(-1.0f / (0.4f * rate));
        reset();
    }
    void reset() { env = 0; gain = 1; }
    void process(float& l, float& r) {
        env += envCoeff * (0.5f * (l * l + r * r) - env);
        const float levelDb = 10.0f * std::log10(env + 1.0e-12f);
        constexpr float threshold = -18.0f, knee = 6.0f, ratio = 2.0f;
        const float over = levelDb - threshold;
        float reductionDb = 0;
        if (over > knee * 0.5f) reductionDb = over * (1.0f - 1.0f / ratio);
        else if (over > -knee * 0.5f) reductionDb = (1.0f - 1.0f / ratio) * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
        const float target = dbToGain(-reductionDb);
        gain += (target < gain ? attack : release) * (target - gain);
        l *= gain;
        r *= gain;
    }
};

// Transparent safety limiter: instant attack on the stereo peak, 150 ms release,
// ceiling -1 dBFS, then a final clamp that can never be exceeded.
struct SafetyLimiter {
    float gain = 1, release = 0;
    static constexpr float ceiling = 0.891f;
    void prepare(float rate) {
        release = 1.0f - std::exp(-1.0f / (0.15f * rate));
        reset();
    }
    void reset() { gain = 1; }
    void process(float& l, float& r) {
        const float peak = std::max(std::abs(l), std::abs(r));
        const float needed = peak * gain > ceiling ? ceiling / peak : 1.0f;
        if (needed < gain) gain = needed;
        else gain += release * (std::min(1.0f, needed) - gain);
        l = std::clamp(l * gain, -ceiling, ceiling);
        r = std::clamp(r * gain, -ceiling, ceiling);
    }
};

} // namespace wb::dsp
