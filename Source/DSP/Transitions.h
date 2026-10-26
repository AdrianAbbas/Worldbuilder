#pragma once
// The Arrangement's transition sounds (Milestone 11b), tuned to the world's Root and
// Mood: a Riser (noise through a rising band-pass and the chord gliding up an octave),
// a Reverse swell (the chord swelling into the downbeat, opening as it grows) and the
// sub hit that lands after a Drop. The Filter sweep closes the whole mix down and opens
// it again (applied by the engine to its master, see sweepCutoff).
//
// Everything follows the song position through the arrangement's State (transitionT,
// arrived, sinceStart), so a start or locate anywhere gives the same sound. Their
// envelopes glide over a few milliseconds, so nothing clicks where they begin or end.
#include "Core.h"
#include "Music.h"
#include <array>

namespace wb::dsp {

class TransitionVoice {
public:
    void prepare(float sampleRate) {
        rate = sampleRate;
        glide = 1.0f - std::exp(-1.0f / (0.006f * rate));
        reset();
    }
    void reset() {
        for (auto& f : noiseBand) f.reset();
        for (auto& f : swellTone) f.reset();
        phase.fill(0.0);
        subPhase = 0;
        riserAmp = swellAmp = subAmp = 0;
        index = 0;
    }
    bool silent() const { return riserAmp < 1.0e-6f && swellAmp < 1.0e-6f && subAmp < 1.0e-6f; }

    // Adds n samples to left/right. kind/t: the transition under way and how far
    // (t < 0: none); t advances by dt a sample. arrived/since: the transition that led
    // into this section and the seconds since its start (advancing by 1/rate).
    void render(int kind, float t, float dt, int arrived, float since, int root, int mood, float* left, float* right, int n) {
        const bool riser = kind == 2 && t >= 0.0f, swell = kind == 5 && t >= 0.0f, sub = arrived == 4 && since < 3.0f;
        if (!riser && !swell && !sub && silent()) return;
        // The chord: the root (around 65-125 Hz), its fifth and octave in the Mood.
        const float base = music::rootHz(root) * 0.5f;
        const float fifth = music::noteHz(root, float(music::degree(mood, 4))) * 0.5f;
        std::array<float, 4> freq { base, fifth, base * 2.0f, fifth * 2.0f };
        for (int i = 0; i < n; ++i) {
            const float ti = t + dt * float(i);
            const float s = since + float(i) / rate;
            ++index;
            float l = 0, r = 0;

            // Riser: louder, brighter and higher towards the downbeat.
            const float riserTarget = riser ? 0.32f * ti * ti * (0.3f + 0.7f * ti) : 0.0f;
            riserAmp += glide * (riserTarget - riserAmp);
            if (riserAmp > 1.0e-6f) {
                const float tt = riser ? clamp01(ti) : 1.0f;
                if ((index & 15) == 0) {
                    const float centre = lerpExp(260.0f, 7000.0f, tt);
                    noiseBand[0].set(centre, 2.2f, rate);
                    noiseBand[1].set(centre * 1.07f, 2.2f, rate);
                }
                const float nl = noiseBand[0].bandpass(bipolar(mix64(index * 2 + 0x51)));
                const float nr = noiseBand[1].bandpass(bipolar(mix64(index * 2 + 0x93)));
                const float pitch = std::exp2(tt);   // up an octave
                float tone = 0;
                for (int k = 0; k < 3; ++k) tone += oscillate(k, freq[std::size_t(k)] * 2.0f * pitch) * (k == 2 ? 0.5f : 1.0f);
                l += riserAmp * (nl * 2.4f + tone * 0.22f);
                r += riserAmp * (nr * 2.4f + tone * 0.22f);
            }

            // Reverse swell: the chord rising like a reversed reverb tail.
            const float swellTarget = swell ? 0.2f * ti * ti * ti : 0.0f;
            swellAmp += glide * (swellTarget - swellAmp);
            if (swellAmp > 1.0e-6f) {
                const float tt = swell ? clamp01(ti) : 1.0f;
                if ((index & 15) == 0) {
                    swellTone[0].set(lerpExp(220.0f, 6500.0f, tt), 0.8f, rate);
                    swellTone[1].set(lerpExp(240.0f, 7000.0f, tt), 0.8f, rate);
                }
                float a = 0, b = 0;
                for (int k = 0; k < 4; ++k) {
                    const float o = saw(k + 3, freq[std::size_t(k)] * (k % 2 == 0 ? 1.0f : 1.003f));
                    (k % 2 == 0 ? a : b) += o;
                }
                const float breath = bipolar(mix64(index + 0x7777)) * 0.15f;
                l += swellAmp * swellTone[0].tick(a + 0.4f * b + breath).lp;
                r += swellAmp * swellTone[1].tick(b + 0.4f * a + breath).lp;
            }

            // Sub hit: an octave pitch drop into the root, a two-second decay.
            const float subTarget = sub ? 0.55f * std::exp(-s / 0.8f) * clamp01(s / 0.002f) : 0.0f;
            subAmp = sub ? subTarget : subAmp + glide * (0.0f - subAmp);
            if (subAmp > 1.0e-6f) {
                const float f = base * 0.5f * std::exp2(std::exp(-s / 0.05f));
                subPhase += double(f) / double(rate);
                subPhase -= std::floor(subPhase);
                const float x = std::sin(float(subPhase) * twoPi);
                const float o = std::tanh(1.6f * x) * subAmp;
                l += o;
                r += o;
            }
            left[i] += finiteOr(l);
            right[i] += finiteOr(r);
        }
    }

    // The Filter sweep's low-pass cutoff (Hz; 0: open). t: the sweep under way (closing
    // over its length); since: beats into the section after one (opening over a beat).
    static float sweepCutoff(int kind, float t, int arrived, double sinceBeats) {
        if (kind == 6 && t >= 0.0f) return lerpExp(18000.0f, 320.0f, std::pow(clamp01(t), 0.7f));
        if (arrived == 6 && sinceBeats < 1.0) return lerpExp(320.0f, 18000.0f, float(sinceBeats));
        return 0.0f;
    }

private:
    float oscillate(int k, float f) {
        auto& p = phase[std::size_t(k)];
        p += double(f) / double(rate);
        p -= std::floor(p);
        return std::sin(float(p) * twoPi);
    }
    // A soft saw (a few harmonics, no aliasing at these pitches).
    float saw(int k, float f) {
        auto& p = phase[std::size_t(k)];
        p += double(f) / double(rate);
        p -= std::floor(p);
        const float x = float(p) * twoPi;
        return (std::sin(x) + 0.5f * std::sin(2.0f * x) + 0.33f * std::sin(3.0f * x) + 0.25f * std::sin(4.0f * x)) * 0.5f;
    }

    float rate = 48000, glide = 0.01f;
    std::array<Svf, 2> noiseBand, swellTone;
    std::array<double, 8> phase {};
    double subPhase = 0;
    float riserAmp = 0, swellAmp = 0, subAmp = 0;
    uint64_t index = 0;
};

} // namespace wb::dsp
