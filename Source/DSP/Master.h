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

// How much a limiter turned the signal down (for the tests and the bench; it never
// changes the sound): the samples seen, those it reduced, the sum and the largest of the
// reduction in dB.
struct Reduction {
    uint64_t samples = 0, engaged = 0;
    double sumDb = 0;
    float maxDb = 0;
    void note(float gain) {
        ++samples;
        if (gain < 0.99999f) {
            const float db = -20.0f * std::log10(std::max(gain, 1.0e-6f));
            ++engaged;
            sumDb += double(db);
            maxDb = std::max(maxDb, db);
        }
    }
    double averageDb() const { return samples > 0 ? sumDb / double(samples) : 0.0; }
    double engagedShare() const { return samples > 0 ? double(engaged) / double(samples) : 0.0; }
    void clear() { *this = Reduction {}; }
};

// Transparent safety limiter: instant attack on the stereo peak, 150 ms release,
// ceiling -1 dBFS, then a final clamp that can never be exceeded.
struct SafetyLimiter {
    float gain = 1, release = 0;
    static constexpr float ceiling = 0.891f;
    // The Drama loudness pass: with drama playing the ceiling is -1.5 dBFS (sample peak), so
    // the true peak between the samples stays under -1 dBFS (it has no look-ahead: no latency).
    static constexpr float dramaCeiling = 0.841f;
    float limit = ceiling;
    Reduction reduction;
    void prepare(float rate) {
        release = 1.0f - std::exp(-1.0f / (0.15f * rate));
        reset();
    }
    void reset() { gain = 1; }
    void process(float& l, float& r) {
        const float peak = std::max(std::abs(l), std::abs(r));
        const float needed = peak * gain > limit ? limit / peak : 1.0f;
        if (needed < gain) gain = needed;
        else gain += release * (std::min(1.0f, needed) - gain);
        reduction.note(gain);
        l = std::clamp(l * gain, -limit, limit);
        r = std::clamp(r * gain, -limit, limit);
    }
};

// The Drama loudness pass: a transparent look-ahead limiter (stereo linked). The signal is
// delayed by the look-ahead (1.5 ms); the gain it needs is held over the look-ahead and
// smoothed by a moving average of the same length, so it is already down when a peak
// arrives (no clipped onsets, no distortion) and never lets a sample past the ceiling;
// it recovers over the release. Below the ceiling it is a pure delay (gain exactly 1).
// Nothing allocates after prepare().
struct LookaheadLimiter {
    static constexpr int maxLook = 512;
    float ceiling = 0.5f, release = 0;
    int look = 72;
    std::array<float, maxLook + 2> delayL {}, delayR {}, hold {};
    std::array<float, maxLook + 2> box {};
    int write = 0, holdCount = 0;
    float env = 1;
    double boxSum = 0;
    int boxAt = 0;
    // The sliding minimum over look + 1 samples (a monotonic queue in fixed storage).
    std::array<float, maxLook + 2> qValue {};
    std::array<int64_t, maxLook + 2> qTime {};
    int qHead = 0, qSize = 0;
    int64_t now = 0;
    bool idle = true;
    Reduction reduction;
    void prepare(float rate, float lookSeconds = 0.0015f, float releaseSeconds = 0.15f) {
        look = std::clamp(int(std::lround(lookSeconds * rate)), 1, maxLook);
        release = 1.0f - std::exp(-1.0f / (releaseSeconds * rate));
        reset();
    }
    void reset() {
        delayL.fill(0);
        delayR.fill(0);
        box.fill(1.0f);
        boxSum = double(look);
        boxAt = 0;
        write = 0;
        env = 1;
        qHead = qSize = 0;
        now = 0;
        idle = true;
    }
    int latency() const { return look; }
    void process(float& l, float& r) {
        const int size = look + 1;
        // The gain this sample needs, with the release (instant down, slow up).
        const float peak = std::max(std::abs(l), std::abs(r));
        const float need = peak > ceiling ? ceiling / peak : 1.0f;
        env = need < env ? need : env + release * (need - env);
        if (env > 0.99999f && need >= 1.0f) env = 1.0f;
        // The minimum over the last look + 1 samples.
        while (qSize > 0 && qValue[std::size_t((qHead + qSize - 1) % size)] >= env) --qSize;
        qValue[std::size_t((qHead + qSize) % size)] = env;
        qTime[std::size_t((qHead + qSize) % size)] = now;
        ++qSize;
        while (qTime[std::size_t(qHead)] <= now - size) {
            qHead = (qHead + 1) % size;
            --qSize;
        }
        const float held = qValue[std::size_t(qHead)];
        ++now;
        // Its moving average over the look-ahead: the gain at the delayed sample.
        boxSum += double(held) - double(box[std::size_t(boxAt)]);
        box[std::size_t(boxAt)] = held;
        boxAt = (boxAt + 1) % look;
        float g = held >= 1.0f && boxSum >= double(look) - 1.0e-6 ? 1.0f : float(boxSum / double(look));
        if (g > 1.0f) g = 1.0f;
        // The delay line.
        const float dl = delayL[std::size_t(write)], dr = delayR[std::size_t(write)];
        delayL[std::size_t(write)] = l;
        delayR[std::size_t(write)] = r;
        write = (write + 1) % look;
        reduction.note(g);
        l = g >= 1.0f ? dl : std::clamp(dl * g, -ceiling, ceiling);
        r = g >= 1.0f ? dr : std::clamp(dr * g, -ceiling, ceiling);
    }
};

} // namespace wb::dsp
