#pragma once
// Building blocks for the event layers (water, fire, earth, thunder): random
// triggers, struck resonators and short pitched voices. Nothing here allocates.
//
// Triggers are hashed from the sample position like every other random value, so
// events land at the same places from the same start point at any block size. A
// Bernoulli trial per chunk (or per sample, for dense textures) approximates a
// Poisson process: irregular, never metronomic.
#include "Core.h"
#include "Music.h"

namespace wb::dsp {

// Probability per trial for a Poisson rate of `perSecond`, with trials `samples` apart.
inline float trialChance(float perSecond, int samples, float rate) { return std::clamp(perSecond * float(samples) / rate, 0.0f, 1.0f); }

struct PanGains { float l, r; };
// Equal power, pan in [-1, 1].
inline PanGains panGains(float pan) {
    const float p = std::clamp(pan, -1.0f, 1.0f);
    return { std::sqrt(0.5f * (1.0f - p)), std::sqrt(0.5f * (1.0f + p)) };
}

// White noise in [-1, 1) through a one-pole low-pass, rescaled to the white noise
// level: a cheap brown-ish rumble source.
struct Rumble {
    OnePole lp;
    float norm = 1;
    void setCutoff(float hz, float rate) {
        lp.setCutoff(hz, rate);
        norm = 1.0f / std::sqrt(lp.a / (2.0f - lp.a));
    }
    float tick(float white) { return lp.lp(white) * norm; }
    void reset() { lp.reset(); }
};

// One mode of a struck object: a two-pole resonator. An impulse of 1 rings at
// about amplitude 1 (the input is scaled by sin w).
struct Mode {
    float b1 = 0, b2 = 0, in = 0, y1 = 0, y2 = 0;
    void set(float hz, float decaySeconds, float rate) {
        const float w = twoPi * std::min(hz, 0.45f * rate) / rate;
        const float r = std::exp(-1.0f / (std::max(decaySeconds, 0.0005f) * rate));
        b1 = 2.0f * r * std::cos(w);
        b2 = -r * r;
        in = std::sin(w);
    }
    float tick(float x) {
        const float y = x * in + b1 * y1 + b2 * y2;
        y2 = y1;
        y1 = y;
        return y;
    }
    float energy() const { return y1 * y1 + y2 * y2; }
    void clear() { y1 = y2 = 0; }
    // New frequency and decay keeping the ringing state (a gliding gong).
    void retune(float hz, float decaySeconds, float rate) {
        const float w = twoPi * std::min(hz, 0.45f * rate) / rate;
        const float r = std::exp(-1.0f / (std::max(decaySeconds, 0.0005f) * rate));
        b1 = 2.0f * r * std::cos(w);
        b2 = -r * r;
        in = std::sin(w);
    }
};

// A decaying sine whose pitch glides: water drops and bubbles (pitch rising), ice
// sheets singing (falling), lava bloops. It starts at zero phase, so it never clicks;
// a negative amplitude starts it in the opposite polarity (random polarities keep a
// stream of drops free of DC).
struct Ping {
    Phasor osc;
    float amp = 0, decay = 1, hz = 1000, glide = 1, attack = 1, rise = 1;
    PanGains pan { 0.7071f, 0.7071f };
    bool active() const { return amp > 1.0e-4f; }
    // glide: pitch multiplier after one decay time (> 1 rises). attackSeconds 0 = instant.
    void start(float startHz, float decaySeconds, float glideOverDecay, float amplitude, float panPosition, float rate,
               float attackSeconds = 0) {
        osc.setPhase(amplitude < 0 ? pi : 0.0f);
        hz = startHz;
        amp = std::abs(amplitude);
        const float seconds = std::max(decaySeconds, 0.0005f);
        decay = std::exp(-1.0f / (seconds * rate));
        glide = std::pow(std::max(glideOverDecay, 0.01f), float(chunk) / (seconds * rate));
        rise = 0;
        attack = attackSeconds > 0 ? 1.0f / (attackSeconds * rate) : 1.0f;
        pan = panGains(panPosition);
        osc.setFrequency(hz, rate);
    }
    // Once per chunk: move the pitch and keep the oscillator on the unit circle.
    void update(float rate) {
        hz = std::clamp(hz * glide, 20.0f, 0.45f * rate);
        osc.setFrequency(hz, rate);
        osc.normalise();
        if (hz >= 0.45f * rate) amp = 0;
    }
    float tick() {
        osc.step();
        rise = std::min(1.0f, rise + attack);
        const float y = amp * rise * osc.im;
        amp *= decay;
        return y;
    }
    void stop() { amp = 0; }
};

// Picks a voice for a new event: a free one, or else the quietest.
template <typename Voices, typename Level>
std::size_t pickVoice(Voices& voices, int count, Level level) {
    std::size_t best = 0;
    float quietest = 1.0e30f;
    for (int i = 0; i < count; ++i) {
        const float l = level(voices[std::size_t(i)]);
        if (l < quietest) {
            quietest = l;
            best = std::size_t(i);
        }
        if (l <= 0) break;
    }
    return best;
}

} // namespace wb::dsp
