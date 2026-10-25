#pragma once
// More shared voices for the Places, next to the ones in Voices.h: a two-operator FM
// voice (electric pianos, glassy calls, PA tones, data blips), a formant choir voice
// (wordless "ah", "oh", "oo"), a wind voice (flutes, recorders, ney: a sine with a
// few harmonics and breath) and an octave-up shimmer for reverbs. They start from
// silence and ring out smoothly; nothing here allocates after prepare().
#include "Voices.h"
#include <array>

namespace wb::dsp {

// A sine table (one cycle plus a guard point), filled when the program loads.
inline constexpr int sineSize = 2048;
inline const std::array<float, sineSize + 1> sineTable = [] {
    std::array<float, sineSize + 1> t {};
    for (int i = 0; i <= sineSize; ++i) t[std::size_t(i)] = float(std::sin(double(twoPi) * double(i) / double(sineSize)));
    return t;
}();
// sin(2 pi x) for any x (cycles).
inline float sinCycles(float x) {
    const float f = (x - std::floor(x)) * float(sineSize);
    const auto i = std::size_t(f);
    const float frac = f - float(i);
    return sineTable[i] + frac * (sineTable[i + 1] - sineTable[i]);
}

//==============================================================================
// Two-operator FM: a sine carrier whose phase is modulated by a sine at `ratio`
// times its frequency. The modulation index has its own decaying envelope (bright
// attack, mellow tail), so the same voice makes electric pianos (ratio 1), bells
// and glass (non-integer ratios) and short digital blips.
struct FmVoice {
    float phase = 0, modPhase = 0, hz = 440, targetHz = 440, glide = 0;
    float ratio = 2, index = 1, indexFloor = 0.2f, indexEnv = 0, indexCoef = 0.99f, feedback = 0, last = 0;
    float vibratoHz = 5, vibratoDepth = 0, vibratoPhase = 0, age = 0;
    float inc = 0, modInc = 0, gain = 1;
    Env env;
    PanGains pan { 0.7071f, 0.7071f };
    bool active() const { return env.active(); }
    float level() const { return env.active() ? env.value * gain : 0.0f; }
    void reset() {
        env.stop();
        phase = modPhase = last = indexEnv = age = vibratoPhase = 0;
    }
    // index: modulation depth in radians at the attack; indexSeconds: its T60.
    void start(float startHz, float attack, float decay, float sustain, float rate, float indexSeconds, bool legato = false) {
        targetHz = startHz;
        if (!(legato && env.active())) {
            hz = startHz;
            phase = modPhase = last = 0;
            age = 0;
        }
        env.trigger(attack, decay, sustain, rate);
        indexEnv = 1;
        indexCoef = std::exp(-6.91f * float(chunk) / (std::max(indexSeconds, 0.005f) * rate));
    }
    void release(float seconds, float rate) { env.release(seconds, rate); }
    // Once per chunk.
    void update(float rate) {
        hz = glide > 0 ? targetHz * std::pow(hz / targetHz, glide) : targetHz;
        const float chunkSeconds = float(chunk) / rate;
        age += chunkSeconds;
        vibratoPhase += vibratoHz * chunkSeconds;
        vibratoPhase -= std::floor(vibratoPhase);
        const float f = hz * std::exp2(vibratoDepth * smoothstep(0.2f, 0.6f, age) * sinCycles(vibratoPhase) / 12.0f);
        inc = std::min(f / rate, 0.45f);
        modInc = std::min(f * ratio / rate, 0.45f);
        indexEnv *= indexCoef;
    }
    float tick() {
        const float e = env.tick();
        if (e <= 0.0f) return 0.0f;
        const float depth = index * (indexFloor + (1.0f - indexFloor) * indexEnv) * (1.0f / twoPi);
        const float m = sinCycles(modPhase + feedback * last) * depth;
        const float y = sinCycles(phase + m);
        last = m;
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        modPhase += modInc;
        if (modPhase >= 1.0f) modPhase -= 1.0f;
        return y * e * gain;
    }
};

//==============================================================================
// A wordless choir voice: two detuned saws with vibrato through three formant
// band-passes. The vowel morphs between "oo", "oh", "ah", "eh" and "ee" (0..4).
struct ChoirVoice {
    std::array<Saw, 2> osc;
    std::array<Svf, 3> formant;
    Env env;
    float hz = 220, targetHz = 220, glide = 0, detuneCents = 9, vowel = 2, vibratoPhase = 0, vibratoHz = 5.2f, vibratoDepth = 0.12f, age = 0;
    float gain = 1, brightness = 0.5f;
    std::array<float, 3> formantGain { 1.0f, 0.5f, 0.2f };
    PanGains pan { 0.7071f, 0.7071f };
    bool active() const { return env.active(); }
    float level() const { return env.active() ? env.value * gain : 0.0f; }
    void reset() {
        env.stop();
        for (auto& f : formant) f.reset();
        age = vibratoPhase = 0;
        osc[0].phase = 0;
        osc[1].phase = 0.37f;
    }
    void start(float startHz, float attack, float rate, bool legato = false) {
        targetHz = startHz;
        if (!(legato && env.active())) {
            hz = startHz;
            age = 0;
            if (!env.active()) for (auto& f : formant) f.reset();
        }
        env.trigger(attack, 1.0f, 1.0f, rate);
    }
    void release(float seconds, float rate) { env.release(seconds, rate); }
    // Once per chunk.
    void update(float rate) {
        static constexpr float table[5][3] { { 325, 700, 2530 }, { 450, 800, 2830 }, { 750, 1150, 2800 }, { 450, 1700, 2600 }, { 300, 2100, 2900 } };
        static constexpr float width[3] { 70, 90, 130 };
        hz = glide > 0 ? targetHz * std::pow(hz / targetHz, glide) : targetHz;
        const float chunkSeconds = float(chunk) / rate;
        age += chunkSeconds;
        vibratoPhase += vibratoHz * chunkSeconds;
        vibratoPhase -= std::floor(vibratoPhase);
        const float f = hz * std::exp2(vibratoDepth * smoothstep(0.3f, 1.0f, age) * sinCycles(vibratoPhase) / 12.0f);
        const float d = detuneCents / 1200.0f;
        osc[0].setFrequency(f * std::exp2(-d), rate);
        osc[1].setFrequency(f * std::exp2(d), rate);
        const float v = std::clamp(vowel, 0.0f, 3.999f);
        const auto a = std::size_t(v);
        const float t = v - float(a);
        for (std::size_t k = 0; k < 3; ++k) {
            const float fc = lerp(table[a][k], table[a + 1][k], t);
            formant[k].set(fc, fc / width[k], rate);
        }
        formantGain = { 1.0f, 0.55f, 0.12f + 0.4f * brightness };
    }
    float tick() {
        const float e = env.tick();
        if (e <= 0.0f) return 0.0f;
        const float x = 0.5f * (osc[0].tick() + osc[1].tick());
        float y = 0;
        for (std::size_t k = 0; k < 3; ++k) y += formantGain[k] * formant[k].bandpass(x);
        return y * e * gain;
    }
};

//==============================================================================
// A wind voice: a sine with soft second and third harmonics, a breath noise band at
// the note, a little scoop into the pitch and delayed vibrato. Flutes, recorders, ney.
struct WindVoice {
    Phasor osc;
    Svf breathBand;
    Env env;
    float hz = 440, targetHz = 440, scoop = 1, scoopCoef = 0.9f, h2 = 0.2f, h3 = 0.1f, breath = 0.2f;
    float tone = 1, breathQ = 3, scoopSeconds = 0.06f;   // tone 0: breath alone (a tuned rustle or gust)
    float vibratoHz = 5, vibratoDepth = 0.15f, vibratoDelay = 0.25f, vibratoPhase = 0, age = 0, gain = 1;
    uint64_t noiseIndex = 0;
    PanGains pan { 0.7071f, 0.7071f };
    bool active() const { return env.active(); }
    float level() const { return env.active() ? env.value * gain : 0.0f; }
    void reset() {
        env.stop();
        breathBand.reset();
        osc.setPhase(0);
        age = vibratoPhase = 0;
    }
    void start(float startHz, float attack, float scoopSemitones, float rate, uint64_t noiseAt) {
        const bool sounding = env.active() && env.value > 0.05f;
        targetHz = startHz;
        // A new breath scoops up into the note; a slurred one moves over quickly.
        scoop = std::exp2(-(sounding ? 0.3f : 1.0f) * scoopSemitones / 12.0f);
        scoopCoef = std::exp(-float(chunk) / ((sounding ? 0.02f : scoopSeconds) * rate));
        if (!sounding) {
            hz = startHz;
            age = 0;
            osc.setPhase(0);
            breathBand.reset();
        }
        env.trigger(attack, 1.0f, 1.0f, rate);
        noiseIndex = noiseAt;
    }
    void release(float seconds, float rate) { env.release(seconds, rate); }
    void update(float rate) {
        hz = targetHz + (hz - targetHz) * 0.5f;
        scoop = 1.0f + (scoop - 1.0f) * scoopCoef;
        const float chunkSeconds = float(chunk) / rate;
        age += chunkSeconds;
        vibratoPhase += vibratoHz * chunkSeconds;
        vibratoPhase -= std::floor(vibratoPhase);
        const float f = std::min(hz * scoop * std::exp2(vibratoDepth * smoothstep(vibratoDelay, vibratoDelay + 0.5f, age) * sinCycles(vibratoPhase) / 12.0f),
                                 0.3f * rate);
        osc.setFrequency(f, rate);
        osc.normalise();
        breathBand.set(f, breathQ, rate);
    }
    float tick(const Stream& noise) {
        const float e = env.tick();
        osc.step();
        if (e <= 0.0f) return 0.0f;
        const float s = osc.im, c = osc.re;
        const float voiced = (s + h2 * 2.0f * s * c + h3 * s * (3.0f - 4.0f * s * s)) * tone;
        // Breath is strongest in the attack and follows the envelope.
        const float air = breathBand.bandpass(noise.white(noiseIndex++)) * breath * (0.6f + 1.4f * (1.0f - std::min(age * 4.0f, 1.0f)));
        return (voiced + air) * e * gain;
    }
};

//==============================================================================
// A bowed resonator: a sawtooth "stick-slip" pulse and bow noise drive up to four
// tuned modes that ring on while the bow moves. Harmonic ratios give bowed strings
// (a low bass, cellos); stretched or quarter-tone ratios with close doublets that beat
// give bowed metal, singing bowls and glassy hums. Each mode is normalised to unity
// gain at its centre, so the decay sets the bandwidth, not the level.
struct BowedVoice {
    static constexpr int maxModes = 4;
    std::array<Mode, maxModes> mode;
    std::array<float, maxModes> ratio { 1, 2, 3, 4 }, decay { 1, 1, 1, 1 }, gain { 1, 0, 0, 0 }, norm {};
    int modes = 1;
    Saw saw;
    OnePole bowLp;
    Env env;
    float hz = 220, targetHz = 220, glide = 0, tunedHz = 0, ringSeconds = 0.4f;
    float rosin = 0.5f, noisiness = 0.3f, vibratoHz = 5, vibratoDepth = 0, vibratoPhase = 0, age = 0, gainLevel = 1;
    uint64_t noiseIndex = 0;
    bool ringing = false;   // the modes ring on after the bow stops
    PanGains pan { 0.7071f, 0.7071f };
    bool active() const { return env.active() || ringing; }
    float level() const { return active() ? env.value * gainLevel + (ringing ? 1.0e-3f : 0.0f) : 0.0f; }
    void reset() {
        env.stop();
        for (auto& m : mode) m.clear();
        bowLp.reset();
        saw.phase = 0;
        age = vibratoPhase = 0;
        tunedHz = 0;
        ringing = false;
    }
    void start(float startHz, float attack, float rate, uint64_t noiseAt, bool legato = false) {
        targetHz = startHz;
        if (!(legato && env.active())) {
            hz = startHz;
            age = 0;
            if (!env.active()) {
                for (auto& m : mode) m.clear();
                saw.phase = 0;
            }
        }
        env.trigger(attack, 1.0f, 1.0f, rate);
        noiseIndex = noiseAt;
    }
    void release(float seconds, float rate) { env.release(seconds, rate); }
    // Once per chunk: glide, vibrato, and the modes retuned when the pitch moves.
    void update(float rate) {
        hz = glide > 0 ? targetHz * std::pow(hz / targetHz, glide) : targetHz;
        const float chunkSeconds = float(chunk) / rate;
        age += chunkSeconds;
        vibratoPhase += vibratoHz * chunkSeconds;
        vibratoPhase -= std::floor(vibratoPhase);
        const float f = hz * std::exp2(vibratoDepth * smoothstep(0.3f, 0.9f, age) * sinCycles(vibratoPhase) / 12.0f);
        saw.setFrequency(f, rate);
        bowLp.setCutoff(std::min(f * 6.0f, 0.4f * rate), rate);
        if (std::abs(f - tunedHz) > tunedHz * 0.0005f) {
            tunedHz = f;
            for (int k = 0; k < modes; ++k) {
                const float mf = f * ratio[std::size_t(k)];
                const float seconds = std::max(ringSeconds * decay[std::size_t(k)], 0.01f);
                mode[std::size_t(k)].set(mf, seconds / 6.91f, rate);
                // A two-pole resonator's centre gain is about 1 / (2 (1 - r)) of its input
                // (scaled by sin w inside Mode); normalise to unity.
                const float r = std::exp(-6.91f / (seconds * rate));
                norm[std::size_t(k)] = mf < 0.45f * rate ? 2.0f * (1.0f - r) * gain[std::size_t(k)] : 0.0f;
            }
        }
        float energy = 0;
        for (int k = 0; k < modes; ++k) energy += mode[std::size_t(k)].energy();
        ringing = energy > 1.0e-10f;
    }
    float tick(const Stream& noise) {
        if (!env.active() && !ringing) return 0.0f;
        const float e = env.tick();
        const float bow = (rosin * saw.tick() + noisiness * bowLp.lp(noise.white(noiseIndex++))) * e;
        float y = 0;
        for (int k = 0; k < modes; ++k) y += mode[std::size_t(k)].tick(bow * norm[std::size_t(k)]);
        return y * gainLevel;
    }
};

//==============================================================================
// Octave-up pitch shifter: two read heads sweep through a short delay line at twice
// the speed, crossfaded with sine windows. In a reverb's feedback path it gives a
// shimmer (each pass rises an octave).
struct OctaveUp {
    DelayLine line;
    float window = 2048, phase = 0;
    void prepare(float rate) {
        window = std::round(0.04f * rate);
        line.prepare(std::size_t(window) + 8);
        clear();
    }
    void clear() {
        line.clear();
        phase = 0;
    }
    float process(float x) {
        line.push(x);
        phase += 1.0f / window;
        if (phase >= 1.0f) phase -= 1.0f;
        const float p2 = phase + 0.5f >= 1.0f ? phase - 0.5f : phase + 0.5f;
        // Reading at double speed: the delay shrinks by one sample per sample.
        const float a = line.read(1.0f + window * (1.0f - phase)) * sinCycles(0.5f * phase);
        const float b = line.read(1.0f + window * (1.0f - p2)) * sinCycles(0.5f * p2);
        return a + b;
    }
};

// A shimmer reverb: a bright FDN whose output is shifted up an octave and fed back in.
struct Shimmer {
    SmallSpace space;
    OctaveUp shift;
    OnePole lowCut, highCut;
    float rate = 48000, feedback = 0.3f, last = 0;
    void prepare(float sampleRate) {
        rate = sampleRate;
        space.prepare(rate);
        shift.prepare(rate);
        lowCut.setCutoff(300.0f, rate);
        highCut.setCutoff(7000.0f, rate);
        clear();
    }
    void clear() {
        space.clear();
        shift.clear();
        lowCut.reset();
        highCut.reset();
        last = 0;
    }
    // size and decay as SmallSpace; amount 0..1: how much rises (the feedback).
    void set(float size, float decaySeconds, float amount) {
        space.set(size, decaySeconds, 6500.0f);
        feedback = 0.45f * clamp01(amount);
    }
    void process(float inL, float inR, float& outL, float& outR) {
        const float up = highCut.lp(lowCut.hp(shift.process(last))) * feedback;
        space.process(inL + up, inR + up, outL, outR);
        last = softClip(0.5f * (outL + outR));
    }
};

} // namespace wb::dsp
