#pragma once
// More shared voices for the Places, next to the ones in Voices.h: a two-operator FM
// voice (electric pianos, glassy calls, PA tones, data blips), a formant choir voice
// (wordless "ah", "oh", "oo"), a wind voice (flutes, recorders, ney: a sine with a
// few harmonics and breath) and an octave-up shimmer for reverbs. They start from
// silence and ring out smoothly; nothing here allocates after prepare().
#include "Voices.h"
#include "InstrumentData.h"
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
// A tine electric piano. A felt hammer strikes a tine (its fundamental, and its
// cantilever modes near 6.3 and 17.6 times higher, which ring briefly: the bell of the
// attack) beside a tone bar that sustains the fundamental. A pickup reads the tine's
// motion; its field is not linear, so the harder the tine swings the more second
// harmonic it adds (the "bark"). The damper stops it when the key is let go.
struct TineVoice {
    std::array<Mode, 4> mode;   // tine fundamental, tone bar, tine modes 2 and 3
    std::array<float, 4> gain {}, freq {}, tau {};
    int strikeAt = 0, strikeLength = 1;
    float strikeAmp = 0, bark = 0.5f, velocity = 0.8f, rate = 48000, energy = 0, gainLevel = 1, dcZ = 0, dcA = 0.001f;
    Phasor strikeShape;
    PanGains pan { 0.7071f, 0.7071f };
    bool on = false;
    bool active() const { return on; }
    float level() const { return on ? std::sqrt(energy) * gainLevel + (strikeAt < strikeLength ? 1.0f : 0.0f) : 0.0f; }
    void reset() {
        for (auto& m : mode) m.clear();
        on = false;
        energy = 0;
        dcZ = 0;
        strikeAt = strikeLength = 1;
    }
    // decaySeconds: the fundamental's T60; brightness 0..1 (how much bell and bark).
    void start(float hz, float decaySeconds, float brightness, float vel, float sampleRate) {
        rate = sampleRate;
        velocity = clamp01(vel);
        // Low notes sustain longer.
        const float t = decaySeconds * std::clamp(std::pow(262.0f / hz, 0.4f), 0.5f, 2.0f);
        freq = { hz, hz * 1.0007f, hz * tineRatios[1], hz * tineRatios[2] };
        tau = { t / 6.91f, t * 1.3f / 6.91f, 0.12f / 6.91f, 0.04f / 6.91f };
        const float bell = (0.3f + 0.7f * brightness) * (0.4f + 0.6f * velocity);
        gain = { 1.0f, 0.45f, 0.35f * bell, 0.12f * bell };
        for (std::size_t k = 0; k < 4; ++k) {
            if (freq[k] >= 0.45f * rate) gain[k] = 0;
            if (!on) mode[k].clear();
            mode[k].set(freq[k], tau[k], rate);
        }
        // The hammer: softer (longer) when played gently.
        strikeLength = std::max(1, int(std::min(lerpExp(0.004f, 0.0012f, velocity), 0.5f / hz) * rate));
        strikeAt = 0;
        strikeAmp = 2.0f * (0.35f * velocity) / float(strikeLength);
        strikeShape.setFrequency(rate / float(strikeLength), rate);
        strikeShape.setPhase(pi);
        bark = (0.2f + 0.8f * brightness) * velocity * 1.6f;
        dcA = 1.0f - std::exp(-twoPi * 20.0f / rate);
        on = true;
    }
    void release(float seconds) {
        // The damper felt: a quick stop, the highs first.
        for (std::size_t k = 0; k < 4; ++k) {
            tau[k] = std::min(tau[k], seconds / 6.91f * (k >= 2 ? 0.5f : 1.0f));
            mode[k].retune(freq[k], tau[k], rate);
        }
    }
    float tick() {
        float x = 0;
        if (strikeAt < strikeLength) {
            strikeShape.step();
            x = strikeAmp * 0.5f * (1.0f + strikeShape.re);
            ++strikeAt;
        }
        float d = 0;
        for (std::size_t k = 0; k < 4; ++k) d += gain[k] * mode[k].tick(x);
        // The pickup: asymmetric, so the swing adds a second harmonic.
        const float y = d + bark * d * d;
        dcZ += dcA * (y - dcZ);
        return (y - dcZ) * gainLevel;
    }
    void update() {
        float e = 0;
        for (std::size_t k = 0; k < 4; ++k) e += gain[k] * gain[k] * mode[k].energy();
        energy = e;
        if (strikeAt >= strikeLength && e < 1.0e-10f) on = false;
    }
};

//==============================================================================
// A wordless choir voice: a few singers on one note. Each singer's voice is a
// band-limited glottal source (a sawtooth softened to the glottis's falling spectrum)
// with its own pitch, vibrato and tiny irregularities; the section shares a vocal
// tract of five formants, taken from sung-vowel measurements for the voice range the
// note lies in (bass, tenor, alto, soprano) and morphing between the vowels "oo",
// "oh", "ah", "eh", "ee" (0..4). Breath (aspiration) rides on the glottal pulses.
struct ChoirVoice {
    static constexpr int singers = 3;
    std::array<Saw, singers> osc;
    std::array<float, singers> offset { 0.0f, -1.0f, 1.0f }, vibPhase { 0.0f, 0.31f, 0.67f }, vibHz { 5.3f, 5.0f, 5.7f };
    std::array<Svf, 5> formant;
    std::array<float, 5> formantGain {};
    OnePole tilt1, tilt2, breathLowCut;
    Env env;
    float hz = 220, targetHz = 220, glide = 0, detuneCents = 9, vowel = 2, vibratoHz = 5.2f, vibratoDepth = 0.12f, age = 0;
    float gain = 1, brightness = 0.5f, breath = 0.08f, rate = 48000, velocity = 0.7f;
    uint64_t noiseIndex = 0;
    Stream jitter;
    PanGains pan { 0.7071f, 0.7071f };
    bool active() const { return env.active(); }
    float level() const { return env.active() ? env.value * gain : 0.0f; }
    void reset() {
        env.stop();
        for (auto& f : formant) f.reset();
        tilt1.reset();
        tilt2.reset();
        breathLowCut.reset();
        age = 0;
        vibPhase = { 0.0f, 0.31f, 0.67f };
        for (int k = 0; k < singers; ++k) osc[std::size_t(k)].phase = 0.37f * float(k);
    }
    void start(float startHz, float attack, float sampleRate, bool legato = false) {
        rate = sampleRate;
        targetHz = startHz;
        if (!(legato && env.active())) {
            hz = startHz;
            age = 0;
            if (!env.active()) {
                for (auto& f : formant) f.reset();
                tilt1.reset();
                tilt2.reset();
            }
            noiseIndex = uint64_t(startHz * 131.0f) << 26;
            jitter.setKey(uint32_t(startHz * 7.0f), 99);
        }
        env.trigger(attack, 1.0f, 1.0f, rate);
    }
    void release(float seconds, float sampleRate) { env.release(seconds, sampleRate); }
    // Once per chunk.
    void update(float sampleRate) {
        rate = sampleRate;
        hz = glide > 0 ? targetHz * std::pow(hz / targetHz, glide) : targetHz;
        const float chunkSeconds = float(chunk) / rate;
        age += chunkSeconds;
        const float depth = vibratoDepth * smoothstep(0.3f, 1.0f, age);
        for (int k = 0; k < singers; ++k) {
            auto& ph = vibPhase[std::size_t(k)];
            ph += vibHz[std::size_t(k)] * (vibratoHz / 5.2f) * chunkSeconds;
            ph -= std::floor(ph);
            // Each singer a little off and wandering (a few cents), with their own vibrato.
            const float cents = detuneCents * offset[std::size_t(k)] + 3.0f * jitter.smooth(double(age) + 17.0 * k, 3.0);
            osc[std::size_t(k)].setFrequency(hz * std::exp2((cents / 100.0f + depth * sinCycles(ph)) / 12.0f), rate);
        }
        // The vocal tract for this range, between the two nearest vowels.
        const int range = hz < 165.0f ? VoiceBass : (hz < 290.0f ? VoiceTenor : (hz < 440.0f ? VoiceAlto : VoiceSoprano));
        static constexpr int order[5] { 4, 3, 0, 1, 2 };   // oo, oh, ah, eh, ee in the table's a, e, i, o, u
        const float v = std::clamp(vowel, 0.0f, 3.999f);
        const auto i = std::size_t(v);
        const float t = v - float(i);
        const auto& from = vowelFormants[std::size_t(range)][std::size_t(order[i])];
        const auto& to = vowelFormants[std::size_t(range)][std::size_t(order[i + 1])];
        for (std::size_t k = 0; k < 5; ++k) {
            const float fc = lerp(from[k].hz, to[k].hz, t), bw = lerp(from[k].bw, to[k].bw, t), db = lerp(from[k].db, to[k].db, t);
            formant[k].set(std::min(fc, 0.45f * rate), fc / bw, rate);
            // Brightness lifts the upper formants (the singer's ring).
            // ... and singing louder (more vocal effort) does too.
            formantGain[k] = dbToGain(db + (k >= 1 ? lerp(-10.0f, 8.0f, brightness) * (k == 1 ? 0.5f : 1.0f) + 8.0f * (velocity - 0.6f) : 0.0f));
        }
        // Basses' low formants pass less of the source: level them with the other ranges.
        rangeGain = range == VoiceBass ? 1.8f : 1.0f;
        // The glottal source falls about 12 dB an octave above a few hundred Hz.
        tilt1.setCutoff(std::max(hz * 1.5f, 300.0f), rate);
        tilt2.setCutoff(lerpExp(900.0f, 6000.0f, clamp01(brightness + 0.5f * (velocity - 0.6f))), rate);
        breathLowCut.setCutoff(1500.0f, rate);
    }
    float tick() {
        const float e = env.tick();
        if (e <= 0.0f) return 0.0f;
        float x = 0;
        bool open = false;
        for (auto& o : osc) {
            x += o.tick();
            open |= o.phase > 0.6f;   // the glottis open: breath passes
        }
        x = tilt2.lp(tilt1.lp(x * (1.0f / float(singers)))) * 1.6f;
        const float n = jitter.white(noiseIndex++);
        x += (n - breathLowCut.lp(n)) * breath * (open ? 1.0f : 0.35f);
        float y = 0;
        for (std::size_t k = 0; k < 5; ++k) y += formantGain[k] * formant[k].bandpass(x);
        return y * e * gain * rangeGain;
    }
    float rangeGain = 1;
};

//==============================================================================
// A wind voice (flutes, recorders, ney, whistles; with tone 0, tuned breath alone for
// rustles, gusts and calls). An air jet against an edge: a near-sine with up to five
// harmonics that grow as the player blows harder (and the pitch rises a few cents), a
// "chiff" at the start (the jet briefly overblows: a burst of noise up at the second and
// third harmonics), turbulent breath tuned to the note plus broad air, vibrato that is
// mostly a pulse in the breath, and a slow wander in pitch.
struct WindVoice {
    Phasor osc;
    Svf breathBand, chiffBand;
    OnePole airLowCut;
    Env env;
    float hz = 440, targetHz = 440, scoop = 1, scoopCoef = 0.9f, h2 = 0.2f, h3 = 0.1f, h4 = 0, h5 = 0, breath = 0.2f;
    float tone = 1, breathQ = 3, scoopSeconds = 0.06f;   // tone 0: breath alone (a tuned rustle or gust)
    float chiff = 0.3f, chiffSeconds = 0.04f, air = 0.1f, velocity = 0.8f, wander = 3;
    float vibratoHz = 5, vibratoDepth = 0.15f, vibratoDelay = 0.25f, vibratoPhase = 0, age = 0, gain = 1, breathPulse = 1;
    uint64_t noiseIndex = 0;
    PanGains pan { 0.7071f, 0.7071f };
    bool active() const { return env.active(); }
    float level() const { return env.active() ? env.value * gain : 0.0f; }
    void reset() {
        env.stop();
        breathBand.reset();
        chiffBand.reset();
        airLowCut.reset();
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
            chiffBand.reset();
        } else {
            // A tongued note in a phrase: a smaller chiff.
            age = std::min(age, 0.5f * chiffSeconds);
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
        const float vib = vibratoDepth * smoothstep(vibratoDelay, vibratoDelay + 0.5f, age) * sinCycles(vibratoPhase);
        // Blown harder: a few cents sharp; and the player's pitch wanders slowly.
        const float cents = 4.0f * (velocity - 0.6f) + wander * sinCycles(0.13f * float(noiseIndex >> 24) + 0.21f * age);
        // Flute vibrato is mostly a pulse in the breath: a third in pitch, the rest in level.
        const float f = std::min(hz * scoop * std::exp2((0.35f * vib + cents / 100.0f) / 12.0f), 0.3f * rate);
        breathPulse = 1.0f + 0.25f * vib;
        osc.setFrequency(f, rate);
        osc.normalise();
        breathBand.set(f, breathQ, rate);
        chiffBand.set(std::min(f * 2.5f, 0.4f * rate), 2.5f, rate);
        airLowCut.setCutoff(1200.0f, rate);
    }
    float tick(const Stream& noise) {
        const float e = env.tick();
        osc.step();
        if (e <= 0.0f) return 0.0f;
        const float s = osc.im, c = osc.re;
        // Harmonics by the Chebyshev recurrence, louder the harder it is blown.
        const float push = 0.6f + 0.8f * velocity * e;
        const float s2 = 2.0f * s * c, c2 = 1.0f - 2.0f * s * s;
        const float s3 = s * (3.0f - 4.0f * s * s), s4 = 2.0f * s2 * c2;
        const float fifth = h5 > 0.0f ? s * (5.0f - 20.0f * s * s + 16.0f * s * s * s * s) : 0.0f;
        const float voiced = (s + push * (h2 * s2 + h3 * s3) + push * push * (h4 * s4 + h5 * fifth)) * tone * breathPulse;
        const float n = noise.white(noiseIndex++);
        // Breath: strongest at the attack, following the envelope.
        const float attackBoost = 1.0f - std::min(age * 4.0f, 1.0f);
        // Blowing harder moves more air.
        const float turbulence = (breathBand.bandpass(n) * breath * (0.6f + 1.4f * attackBoost) + (n - airLowCut.lp(n)) * air * 0.3f) * (0.4f + 0.8f * velocity);
        const float burst = chiff > 0.0f && age < 3.0f * chiffSeconds ? chiffBand.bandpass(n) * chiff * std::exp(-age / chiffSeconds) * (0.5f + velocity) : 0.0f;
        return (voiced + turbulence * breathPulse + burst) * e * gain;
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
// A bowed string (violin, viola, cello, double bass), one player or a section. The bow
// drags the string into Helmholtz motion: the force on the bridge is a sawtooth whose
// harmonics are combed by the bow's position and rounded off by its pressure. The bow
// scrapes (noise bursts at each slip, more in the first moments of a stroke), the
// pitch settles from a little flat, vibrato comes in after the note speaks. A section
// is several players a few cents apart, starting a moment apart. The instrument's body
// is the part's (see BowedPart).
struct StringVoice {
    static constexpr int maxPlayers = 3;
    struct Player {
        Saw saw;
        float detune = 0, delay = 0, vibratoPhase = 0, vibratoHz = 5.5f;
    };
    std::array<Player, maxPlayers> player;
    DelayLine comb;
    OnePole pressure, noiseTone;
    Svf scrape;
    Env env;
    int players = 1;
    float hz = 220, targetHz = 220, glide = 0, age = 0, settle = 1, rate = 48000;
    float position = 0.12f, brightness = 0.5f, bowNoise = 0.3f, vibratoDepth = 0.2f, vibratoDelay = 0.3f, spreadCents = 8, gain = 1;
    float combDelay = 10, combLevel = 0.7f, scratch = 0, velocity = 0.8f;
    uint64_t noiseIndex = 0;
    PanGains pan { 0.7071f, 0.7071f };
    bool active() const { return env.active(); }
    float level() const { return env.active() ? env.value * gain : 0.0f; }
    void prepare(float sampleRate, float lowestHz) {
        rate = sampleRate;
        comb.prepare(std::size_t(rate / lowestHz) + 8);
    }
    void reset() {
        env.stop();
        comb.clear();
        pressure.reset();
        noiseTone.reset();
        scrape.reset();
        age = 0;
    }
    // A new stroke (or, legato, the same bow moving on to the next note).
    void start(float startHz, float attack, float velocity, float random, uint64_t noiseAt, bool legato) {
        targetHz = startHz;
        const bool sounding = legato && env.active();
        if (!sounding) {
            hz = startHz;
            age = 0;
            comb.clear();
            for (int k = 0; k < maxPlayers; ++k) {
                auto& p = player[std::size_t(k)];
                p.saw.phase = unipolar(mix64(noiseAt + uint64_t(k) * 977));
                // Each player a little off the note and late, with a vibrato of their own.
                p.detune = k == 0 ? 0.0f : spreadCents * (k == 1 ? -0.6f : 0.75f) * (0.7f + 0.6f * random);
                p.delay = k == 0 ? 0.0f : 0.012f + 0.02f * float(k) * (0.5f + random);
                p.vibratoPhase = 0.37f * float(k) + random;
                p.vibratoHz = 5.2f + 0.35f * float(k) + 0.4f * random;
            }
        }
        // The pitch settles from a little flat as the bow takes the string.
        this->velocity = clamp01(velocity);
        settle = sounding ? 1.0f : std::exp2(-(6.0f + 6.0f * velocity) / 1200.0f);
        scratch = (sounding ? 0.3f : 1.0f) * (0.4f + 0.8f * velocity);
        env.trigger(attack, 1.0f, 1.0f, rate);
        noiseIndex = noiseAt;
    }
    void release(float seconds) { env.release(seconds, rate); }
    // Once per chunk.
    void update() {
        hz = glide > 0 ? targetHz * std::pow(hz / targetHz, glide) : targetHz;
        const float chunkSeconds = float(chunk) / rate;
        age += chunkSeconds;
        settle = 1.0f + (settle - 1.0f) * std::exp(-chunkSeconds / 0.06f);
        scratch *= std::exp(-chunkSeconds / 0.07f);
        const float vib = vibratoDepth * smoothstep(vibratoDelay, vibratoDelay + 0.6f, age);
        for (int k = 0; k < players; ++k) {
            auto& p = player[std::size_t(k)];
            p.vibratoPhase += p.vibratoHz * chunkSeconds;
            p.vibratoPhase -= std::floor(p.vibratoPhase);
            const float f = hz * settle * std::exp2((p.detune / 100.0f + vib * sinCycles(p.vibratoPhase)) / 12.0f);
            p.saw.setFrequency(age >= p.delay ? f : 0.0f, rate);
        }
        // The bow-position comb: harmonics near multiples of 1 / position are weak.
        combDelay = std::clamp(position * rate / std::max(hz, 20.0f), 1.0f, float(comb.mask - 4));
        // Pressure and brightness round off the corner of the sawtooth.
        // A firmer, faster bow (played louder) is brighter.
        pressure.setCutoff(std::min(hz * lerp(6.0f, 40.0f, clamp01(brightness * (0.6f + 0.4f * env.value) * (0.4f + 1.0f * velocity))), 0.42f * rate), rate);
        noiseTone.setCutoff(lerpExp(1500.0f, 7000.0f, brightness), rate);
        scrape.set(std::min(hz * 9.0f, 0.4f * rate), 0.8f, rate);
    }
    float tick(const Stream& noise) {
        const float e = env.tick();
        if (e <= 0.0f) return 0.0f;
        float saw = 0, slip = 0;
        for (int k = 0; k < players; ++k) {
            auto& p = player[std::size_t(k)];
            if (p.saw.inc <= 0.0f) continue;
            const float y = p.saw.tick();
            saw += y;
            // The slip: the moment the saw falls back.
            slip += p.saw.phase < p.saw.inc * 3.0f ? 1.0f : 0.0f;
        }
        saw *= 1.0f / std::sqrt(float(players));
        comb.push(saw);
        const float force = pressure.lp(saw - combLevel * comb.read(combDelay));
        const float n = noise.white(noiseIndex++);
        const float hiss = noiseTone.lp(n) * bowNoise * (0.35f + 0.65f * std::min(slip, 1.0f)) + scrape.bandpass(n) * scratch * 0.6f;
        return (force + hiss * 0.5f) * e * gain;
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
