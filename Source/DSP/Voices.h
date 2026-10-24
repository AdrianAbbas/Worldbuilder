#pragma once
// Instruments for the Places: a band-limited saw, an envelope, a brass/pad synth
// voice, a plucked string, a modal bell (or wooden bar) and a tuned drum. All of
// them start from silence and ring out smoothly, so a new note never clicks.
// Nothing here allocates after prepare().
#include "Events.h"

namespace wb::dsp {

inline float polyBlep(float t, float dt) {
    if (t < dt) {
        const float x = t / dt;
        return x + x - x * x - 1.0f;
    }
    if (t > 1.0f - dt) {
        const float x = (t - 1.0f) / dt;
        return x * x + x + x + 1.0f;
    }
    return 0.0f;
}

// Soft saturation: about linear below 0.5, never above 1.
inline float softClip(float x) {
    const float c = std::clamp(x, -3.0f, 3.0f);
    return c * (27.0f + c * c) / (27.0f + 9.0f * c * c);
}

// Band-limited sawtooth (PolyBLEP).
struct Saw {
    float phase = 0, inc = 0;
    void setFrequency(float hz, float rate) { inc = std::clamp(hz / rate, 0.0f, 0.45f); }
    float tick() {
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        return 2.0f * phase - 1.0f - polyBlep(phase, inc);
    }
};

// Attack (linear, from wherever the level is, so a retrigger never clicks), decay
// to a sustain level, exponential release.
struct Env {
    enum Stage { Off, Attack, Decay, Release };
    Stage stage = Off;
    float value = 0, sustain = 0, attackStep = 1, decayCoef = 0, releaseCoef = 0;
    static float coefFor(float seconds, float rate) { return std::exp(-1.0f / (std::max(seconds, 0.0005f) * rate)); }
    void trigger(float attackSeconds, float decaySeconds, float sustainLevel, float rate) {
        attackStep = 1.0f / (std::max(attackSeconds, 0.0005f) * rate);
        decayCoef = coefFor(decaySeconds, rate);
        sustain = sustainLevel;
        stage = Attack;
    }
    void release(float seconds, float rate) {
        if (stage == Off) return;
        releaseCoef = coefFor(seconds, rate);
        stage = Release;
    }
    float tick() {
        switch (stage) {
            case Attack:
                value += attackStep;
                if (value >= 1.0f) {
                    value = 1.0f;
                    stage = Decay;
                }
                break;
            case Decay: value = sustain + (value - sustain) * decayCoef; if (sustain <= 0.0f && value < 1.0e-5f) { value = 0; stage = Off; } break;
            case Release:
                value *= releaseCoef;
                if (value < 1.0e-5f) {
                    value = 0;
                    stage = Off;
                }
                break;
            case Off: break;
        }
        return value;
    }
    bool active() const { return stage != Off; }
    void stop() { stage = Off; value = 0; }
};

// A synth voice: three detuned saws through a stereo low-pass whose cutoff opens with
// the envelope (brass swells) and with a decaying filter envelope (plucky arps),
// with glide and delayed vibrato. Brass drones, horn calls, pads, arps and engines.
struct SynthVoice {
    std::array<Saw, 3> osc;
    std::array<Svf, 2> filter;
    Env env;
    float hz = 110, targetHz = 110, glide = 0;   // glide: fraction left per chunk (0 = jump)
    float detuneCents = 7, cutoff = 1000, envOctaves = 1, filterEnvOctaves = 0, filterEnv = 0, filterDecay = 0, q = 0.7f;
    float vibratoHz = 5, vibratoDepth = 0, vibratoDelay = 0.3f, age = 0, vibratoPhase = 0;
    float subLevel = 0.5f, gain = 1, width = 0.6f, releaseAt = 1.0e30f;
    PanGains pan { 0.7071f, 0.7071f };
    bool active() const { return env.active(); }
    float level() const { return env.active() ? env.value * gain : 0.0f; }
    void reset() {
        env.stop();
        for (auto& f : filter) f.reset();
        releaseAt = 1.0e30f;
        age = vibratoPhase = filterEnv = 0;
        for (auto& o : osc) o.phase = 0;
    }
    // Starts a note. If the voice is sounding and `legato`, it glides to the new pitch.
    void start(float startHz, float attack, float decay, float sustain, float rate, bool legato = false) {
        targetHz = startHz;
        if (!(legato && env.active())) {
            hz = startHz;
            age = 0;
            for (std::size_t i = 0; i < osc.size(); ++i) osc[i].phase = 0.31f * float(i);
            if (!env.active()) for (auto& f : filter) f.reset();
        }
        env.trigger(attack, decay, sustain, rate);
        filterEnv = 1;
        releaseAt = 1.0e30f;
    }
    void release(float seconds, float rate) { env.release(seconds, rate); releaseAt = 1.0e30f; }
    // Once per chunk.
    void update(float rate) {
        hz = glide > 0 ? targetHz * std::pow(hz / targetHz, glide) : targetHz;
        const float chunkSeconds = float(chunk) / rate;
        age += chunkSeconds;
        vibratoPhase += twoPi * vibratoHz * chunkSeconds;
        if (vibratoPhase > twoPi) vibratoPhase -= twoPi;
        const float vib = vibratoDepth * smoothstep(vibratoDelay, vibratoDelay + 0.4f, age) * std::sin(vibratoPhase);
        const float f = hz * std::exp2(vib / 12.0f);
        const float d = detuneCents / 1200.0f;
        osc[0].setFrequency(f * std::exp2(-d), rate);
        osc[1].setFrequency(f * std::exp2(d), rate);
        osc[2].setFrequency(f * 0.5f, rate);
        const float c = cutoff * std::exp2(envOctaves * env.value + filterEnvOctaves * filterEnv);
        for (auto& flt : filter) flt.set(std::min(c, 0.45f * rate), q, rate);
        filterEnv *= filterDecay;
    }
    void tick(float& l, float& r) {
        const float e = env.tick();
        const float a = osc[0].tick(), b = osc[1].tick(), s = osc[2].tick() * subLevel;
        if (e <= 0.0f) return;
        const float inL = a * (0.5f + 0.5f * width) + b * (0.5f - 0.5f * width) + s;
        const float inR = b * (0.5f + 0.5f * width) + a * (0.5f - 0.5f * width) + s;
        const float g = e * gain * 1.4142f;
        l += filter[0].tick(inL).lp * g * pan.l;
        r += filter[1].tick(inR).lp * g * pan.r;
    }
};

// Plucked string (Karplus-Strong): a burst of filtered noise circulating through a
// tuned delay with a damping low-pass. Lutes, harps, plucked ostinatos.
struct Pluck {
    DelayLine line;
    float delay = 100, feedback = 0.99f, damp = 0.5f, z = 0, exciteLp = 0, exciteA = 0.5f, amp = 0, peak = 0;
    int excite = 0;
    uint64_t noiseIndex = 0;
    PanGains pan { 0.7071f, 0.7071f };
    bool active = false;
    void prepare(float rate, float lowestHz) { line.prepare(std::size_t(rate / lowestHz) + 8); }
    void reset() {
        line.clear();
        active = false;
        excite = 0;
        z = 0;
        peak = 0;
    }
    float level() const { return active ? peak + float(excite) : 0.0f; }
    // brightness 0..1: duller to brighter attack and slower to faster damping of highs.
    void start(float hz, float decaySeconds, float brightness, float amplitude, float panPosition, float rate, uint64_t noiseAt) {
        line.clear();
        z = 0;
        exciteLp = 0;
        damp = lerp(0.25f, 0.9f, brightness);
        exciteA = lerp(0.15f, 0.8f, brightness);
        // A one-pole in the loop delays low frequencies by about (1 - a) / a samples.
        const float period = rate / std::max(hz, 20.0f);
        delay = std::max(2.0f, period - (1.0f - damp) / damp);
        feedback = std::exp(std::log(0.001f) * (period / rate) / std::max(decaySeconds, 0.05f));
        excite = int(period);
        amp = amplitude;
        noiseIndex = noiseAt;
        pan = panGains(panPosition);
        active = true;
        peak = amplitude;
    }
    float tick(const Stream& noise) {
        float in = 0;
        if (excite > 0) {
            exciteLp += exciteA * (noise.white(noiseIndex++) - exciteLp);
            in = exciteLp * amp;
            --excite;
        }
        const float y = line.read(delay);
        z += damp * (y - z);
        line.push(in + feedback * z);
        peak = std::max(peak * 0.9999f, std::abs(y));
        if (excite == 0 && peak < 1.0e-4f) active = false;
        return y;
    }
};

// A struck modal object: up to six resonant modes excited by a short raised-cosine
// strike (soft mallets strike for longer, so the attack swells over a few
// milliseconds and the highest modes are barely excited). With bell ratios it is a
// tuned bell, with bar ratios a wooden tone, with harmonic ratios a soft chime.
struct Bell {
    static constexpr int maxModes = 6;
    std::array<Mode, maxModes> mode;
    std::array<float, maxModes> gain {};
    int modes = 0, strikeAt = 0, strikeLength = 1;
    float strikeAmp = 0, strikeStep = 0;
    Phasor strikeShape;
    PanGains pan { 0.7071f, 0.7071f };
    bool active = false;
    float energy = 0;
    void reset() {
        for (auto& m : mode) m.clear();
        active = false;
        energy = 0;
        strikeAt = strikeLength = 1;
    }
    bool striking() const { return strikeAt < strikeLength; }
    float level() const { return active ? std::sqrt(energy) + (striking() ? 1.0f : 0.0f) : 0.0f; }
    // decaySeconds: time for the lowest mode to fall by 60 dB. ratios, decays (relative
    // to it) and gains for each mode; hardness 0..1 from a soft felt mallet (a 4 ms
    // strike) to a hard one (0.15 ms).
    void start(float hz, float decaySeconds, float hardness, float amplitude, float panPosition, float rate, const float* ratios,
               const float* decays, const float* gains, int count) {
        modes = std::min(count, maxModes);
        for (int k = 0; k < modes; ++k) {
            const float f = hz * ratios[k];
            const float d = decaySeconds * decays[k] / 6.91f;   // T60 to time constant
            mode[std::size_t(k)].set(f, d, rate);
            gain[std::size_t(k)] = f < 0.45f * rate ? gains[k] : 0.0f;
        }
        // At most half a period of the note, so the fundamental is always fully struck.
        strikeLength = std::max(1, int(std::min(lerpExp(0.004f, 0.00015f, clamp01(hardness)), 0.5f / hz) * rate));
        strikeAt = 0;
        // A raised cosine of this length and peak 2 A / N has an area of A.
        strikeAmp = 2.0f * amplitude / float(strikeLength);
        strikeShape.setFrequency(rate / float(strikeLength), rate);
        strikeShape.setPhase(pi);
        pan = panGains(panPosition);
        active = true;
        energy = amplitude * amplitude;
    }
    float tick() {
        float x = 0;
        if (strikeAt < strikeLength) {
            strikeShape.step();
            x = strikeAmp * 0.5f * (1.0f + strikeShape.re);
            ++strikeAt;
        }
        float y = 0;
        for (int k = 0; k < modes; ++k) y += gain[std::size_t(k)] * mode[std::size_t(k)].tick(x);
        return y;
    }
    // Once per chunk: retire the voice when it has rung out.
    void update() {
        float e = 0;
        for (int k = 0; k < modes; ++k) e += gain[std::size_t(k)] * gain[std::size_t(k)] * mode[std::size_t(k)].energy();
        energy = e;
        if (!striking() && e < 1.0e-9f) active = false;
    }
};

// A tuned drum: two sine partials with a falling pitch envelope plus a noise burst
// through a band-pass. Timpani, frame drums, toms, and (tone off, noise high) shakers.
struct Drum {
    Phasor a, b;
    Svf noiseBand;
    float hz = 80, endHz = 80, pitchCoef = 0.9f, ratio = 1.5f, partial = 0.4f;
    float amp = 0, decay = 1, noise = 0, noiseDecay = 1;
    uint64_t noiseIndex = 0;
    PanGains pan { 0.7071f, 0.7071f };
    bool active() const { return amp > 1.0e-4f || noise > 1.0e-4f; }
    float level() const { return amp + noise; }
    void reset() {
        amp = noise = 0;
        noiseBand.reset();
    }
    // decaySeconds and noiseSeconds: time to fall by 60 dB.
    void start(float toneHz, float bend, float decaySeconds, float toneAmp, float noiseAmp, float noiseHz, float noiseQ, float noiseSeconds,
               float panPosition, float rate, uint64_t noiseAt) {
        endHz = toneHz;
        hz = toneHz * bend;
        pitchCoef = std::exp(-float(chunk) / (0.035f * rate));
        // Alternate polarity from hit to hit: a run of decaying sines that all start
        // upward would add up to a DC offset.
        const float phase = ((noiseAt >> 24) & 1) != 0 ? pi : 0.0f;
        a.setPhase(phase);
        b.setPhase(phase);
        a.setFrequency(hz, rate);
        b.setFrequency(hz * ratio, rate);
        amp = toneAmp;
        decay = Env::coefFor(decaySeconds / 6.91f, rate);
        noise = noiseAmp;
        noiseDecay = Env::coefFor(noiseSeconds / 6.91f, rate);
        noiseBand.set(noiseHz, noiseQ, rate);
        noiseIndex = noiseAt;
        pan = panGains(panPosition);
    }
    void update(float rate) {
        hz = endHz + (hz - endHz) * pitchCoef;
        a.setFrequency(hz, rate);
        b.setFrequency(hz * ratio, rate);
        a.normalise();
        b.normalise();
    }
    float tick(const Stream& noiseSource) {
        a.step();
        b.step();
        float y = amp * (a.im + partial * b.im * amp);
        amp *= decay;
        if (noise > 1.0e-4f) {
            y += noiseBand.bandpass(noiseSource.white(noiseIndex++)) * noise;
            noise *= noiseDecay;
        }
        return y;
    }
};

} // namespace wb::dsp
