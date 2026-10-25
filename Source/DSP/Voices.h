#pragma once
// Instruments for the Places: a band-limited saw, an envelope, a brass/pad synth
// voice, a plucked string, a modal bell (or wooden bar) and a tuned drum. All of
// them start from silence and ring out smoothly, so a new note never clicks.
// Nothing here allocates after prepare().
#include "Events.h"
#include "InstrumentData.h"

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

// A synth voice in one of three models.
// Analog: three detuned saws through a stereo low-pass whose cutoff opens with the
// envelope and with a decaying filter envelope, glide, delayed vibrato. Pads, arps,
// basses, engines, sirens: the synthetic sounds.
// Brass: a lip-buzzed horn. The buzz is a sawtooth whose brightness rises steeply with
// how hard it is blown (a horn's spectrum grows with level; loud, the air column's
// nonlinearity makes it brassy), a lip "blat" at the attack (the pitch scooping up into
// the note, a burst of buzz), and the bell and mouthpiece formant. Horns, trumpets,
// bugles, trombones.
// Reed: free reeds (accordion, concertina, harmonium): each reed a narrow pulse, two
// reeds a few cents apart beating (musette), the reed chamber's formant, a valve
// click and the bellows' air.
enum SynthModel { SynthAnalog, SynthBrass, SynthReed };
struct SynthVoice {
    std::array<Saw, 4> osc;
    std::array<Svf, 2> filter;
    Svf formant;
    Env env;
    int model = SynthAnalog;
    float hz = 110, targetHz = 110, glide = 0;   // glide: fraction left per chunk (0 = jump)
    float detuneCents = 7, cutoff = 1000, envOctaves = 1, filterEnvOctaves = 0, filterEnv = 0, filterDecay = 0, q = 0.7f;
    float vibratoHz = 5, vibratoDepth = 0, vibratoDelay = 0.3f, age = 0, vibratoPhase = 0;
    float subLevel = 0.5f, gain = 1, width = 0.6f, releaseAt = 1.0e30f;
    // Brass and Reed.
    float velocity = 0.8f, formantHz = 1200, formantLevel = 0.6f, breath = 0.05f, blat = 0, drive = 1, rate = 48000;
    float scoopCents = 0;
    uint64_t noiseIndex = 0;
    Stream air;
    PanGains pan { 0.7071f, 0.7071f };
    bool active() const { return env.active(); }
    float level() const { return env.active() ? env.value * gain : 0.0f; }
    void reset() {
        env.stop();
        for (auto& f : filter) f.reset();
        formant.reset();
        releaseAt = 1.0e30f;
        age = vibratoPhase = filterEnv = 0;
        blat = 0;
        scoopCents = 0;
        for (auto& o : osc) o.phase = 0;
    }
    // Starts a note. If the voice is sounding and `legato`, it glides to the new pitch.
    void start(float startHz, float attack, float decay, float sustain, float sampleRate, bool legato = false) {
        rate = sampleRate;
        targetHz = startHz;
        const bool slur = legato && env.active();
        if (!slur) {
            hz = startHz;
            age = 0;
            for (std::size_t i = 0; i < osc.size(); ++i) osc[i].phase = 0.31f * float(i);
            if (!env.active()) {
                for (auto& f : filter) f.reset();
                formant.reset();
            }
        }
        // A horn's lips find the note from below; a slurred note only a little.
        noiseIndex = uint64_t(startHz * 977.0f) << 24;
        scoopCents = model == SynthBrass ? -(slur ? 12.0f : 35.0f) * (1.2f - 0.4f * velocity) : 0.0f;
        blat = model == SynthBrass ? (slur ? 0.3f : 1.0f) * velocity : (model == SynthReed ? 0.5f : 0.0f);
        env.trigger(attack, decay, sustain, rate);
        filterEnv = 1;
        releaseAt = 1.0e30f;
    }
    void release(float seconds, float sampleRate) { env.release(seconds, sampleRate); releaseAt = 1.0e30f; }
    // Once per chunk.
    void update(float sampleRate) {
        rate = sampleRate;
        hz = glide > 0 ? targetHz * std::pow(hz / targetHz, glide) : targetHz;
        const float chunkSeconds = float(chunk) / rate;
        age += chunkSeconds;
        vibratoPhase += twoPi * vibratoHz * chunkSeconds;
        if (vibratoPhase > twoPi) vibratoPhase -= twoPi;
        float vib = vibratoDepth * smoothstep(vibratoDelay, vibratoDelay + 0.4f, age) * std::sin(vibratoPhase);
        if (model == SynthBrass) {
            // The scoop settles in about 30 ms with a slight overshoot.
            vib += (scoopCents * std::exp(-age / 0.028f) + 6.0f * velocity * (std::exp(-age / 0.07f) - std::exp(-age / 0.028f))) / 100.0f;
        }
        const float f = hz * std::exp2(vib / 12.0f);
        const float d = detuneCents / 1200.0f;
        switch (model) {
            case SynthBrass: {
                osc[0].setFrequency(f, rate);
                osc[1].setFrequency(f * std::exp2(d), rate);
                // Brightness grows faster than level: soft, little above the fundamental;
                // loud, harmonics far up. The cutoff follows the note.
                const float blow = std::pow(clamp01(env.value * velocity), 1.4f);
                const float c = std::min(f * (1.3f + 16.0f * blow) * (cutoff / 1000.0f), 0.42f * rate);
                for (auto& flt : filter) flt.set(c, 0.6f, rate);
                drive = 1.0f + 3.5f * blow;
                formant.set(formantHz, 1.4f, rate);
                blat *= std::exp(-chunkSeconds / 0.03f);
                break;
            }
            case SynthReed: {
                // Two reeds per note (the second `detune` sharp: musette); each a pulse made
                // of two saws a fraction of a period apart.
                osc[0].setFrequency(f, rate);
                osc[1].setFrequency(f, rate);
                osc[2].setFrequency(f * std::exp2(d), rate);
                osc[3].setFrequency(f * std::exp2(d), rate);
                // More bellows pressure: louder and brighter.
                const float c = std::min(cutoff * std::exp2(envOctaves * (env.value - 1.0f)) * (0.5f + 0.8f * velocity), 0.42f * rate);
                for (auto& flt : filter) flt.set(c, 0.7f, rate);
                formant.set(formantHz, 2.0f, rate);
                blat *= std::exp(-chunkSeconds / 0.012f);
                break;
            }
            default: {
                osc[0].setFrequency(f * std::exp2(-d), rate);
                osc[1].setFrequency(f * std::exp2(d), rate);
                osc[2].setFrequency(f * 0.5f, rate);
                const float c = cutoff * std::exp2(envOctaves * env.value + filterEnvOctaves * filterEnv);
                for (auto& flt : filter) flt.set(std::min(c, 0.45f * rate), q, rate);
                break;
            }
        }
        filterEnv *= filterDecay;
    }
    void tick(float& l, float& r) {
        const float e = env.tick();
        if (model == SynthBrass) {
            const float a = osc[0].tick(), b = osc[1].tick();
            if (e <= 0.0f) return;
            const float n = air.white(noiseIndex++);
            // The buzz (and a second player when detuned), the blat's burst of noise.
            const float buzz = a + (detuneCents > 0.01f ? 0.7f * b : 0.0f) + n * (breath + 0.6f * blat);
            const float lp = filter[0].tick(buzz).lp;
            const float hot = softClip(lp * drive * 0.6f) / (0.6f * std::sqrt(drive));
            const float y = (hot + formantLevel * formant.bandpass(hot)) * e * gain * 0.9f;
            l += y * pan.l;
            r += y * pan.r;
            return;
        }
        if (model == SynthReed) {
            // Each pulse: a saw minus the same saw about a third of a period on (the
            // phases start 0.31 apart, see start()).
            const auto pulse = [](Saw& s, Saw& t) {
                const float y = s.tick();
                const float z = t.tick();
                return 0.5f * (y - z);
            };
            const float p1 = pulse(osc[0], osc[1]), p2 = pulse(osc[2], osc[3]);
            if (e <= 0.0f) return;
            const float n = air.white(noiseIndex++);
            const float reeds = p1 + 0.85f * p2 + n * (breath * e + 0.5f * blat);
            const float lp = filter[0].tick(reeds).lp;
            const float y = (lp + formantLevel * formant.bandpass(lp)) * e * gain * 0.8f;
            l += y * pan.l;
            r += y * pan.r;
            return;
        }
        const float a = osc[0].tick(), b = osc[1].tick(), s = osc[2].tick() * subLevel;
        if (e <= 0.0f) return;
        const float inL = a * (0.5f + 0.5f * width) + b * (0.5f - 0.5f * width) + s;
        const float inR = b * (0.5f + 0.5f * width) + a * (0.5f - 0.5f * width) + s;
        const float g = e * gain * 1.4142f;
        l += filter[0].tick(inL).lp * g * pan.l;
        r += filter[1].tick(inR).lp * g * pan.r;
    }
};

// One string of a plucked instrument: a digital waveguide (a delay loop) with a
// frequency-dependent loss filter, a stiffness all-pass (the upper partials run a
// little sharp, as on real strings) and a fractional-delay all-pass solved at the
// note's own frequency, so every note is in tune at any rate.
struct WaveguideString {
    DelayLine line;
    int length = 2;
    float fracC = 0, fracX = 0, fracY = 0;
    float dispC = 0;
    int dispStages = 0;
    std::array<float, 2> dispX {}, dispY {};
    float lossA = 0.5f, lossZ = 0, gain = 0.99f, period = 100;
    void prepare(float rate, float lowestHz) { line.prepare(std::size_t(rate / lowestHz) + 8); }
    void clear() {
        line.clear();
        fracX = fracY = lossZ = 0;
        dispX.fill(0);
        dispY.fill(0);
    }
    // Phase delay (samples) and magnitude of a first-order all-pass / one-pole low-pass at w.
    static float allpassDelay(float c, float w) {
        // H = (c + e^-jw) / (1 + c e^-jw)
        const float re = c + std::cos(w), im = -std::sin(w), dre = 1.0f + c * std::cos(w), dim = -c * std::sin(w);
        return -(std::atan2(im, re) - std::atan2(dim, dre)) / w;
    }
    static void lowpassAt(float a, float w, float& delay, float& magnitude) {
        // H = a / (1 - (1 - a) e^-jw)
        const float dre = 1.0f - (1.0f - a) * std::cos(w), dim = (1.0f - a) * std::sin(w);
        delay = std::atan2(dim, dre) / w;
        magnitude = a / std::sqrt(dre * dre + dim * dim);
    }
    // hz: the note; t60: seconds for the fundamental to fall 60 dB; loss 0..1: how much
    // faster the upper partials die (0 bright steel, 1 dull gut); stiffness 0..1.
    void tune(float hz, float rate, float t60, float loss, float stiffness) {
        period = rate / std::clamp(hz, 20.0f, 0.3f * rate);
        const float w = twoPi * hz / rate;
        lossA = lerp(0.95f, 0.22f, clamp01(loss));
        dispStages = stiffness > 0.02f ? (stiffness > 0.6f ? 2 : 1) : 0;
        dispC = -lerp(0.0f, 0.42f, clamp01(stiffness)) * std::min(1.0f, 60.0f / period + 0.25f);
        float lossDelay = 0, lossGain = 1;
        lowpassAt(lossA, w, lossDelay, lossGain);
        const float dispDelay = float(dispStages) * (dispStages > 0 ? allpassDelay(dispC, w) : 0.0f);
        const float rest = period - lossDelay - dispDelay;
        length = std::max(1, int(rest - 0.5f));
        const float d = rest - float(length);   // in [0.5, 1.5)
        fracC = std::sin(0.5f * w * (1.0f - d)) / std::sin(0.5f * w * (1.0f + d));
        setDecay(t60, rate, lossGain);
    }
    void setDecay(float t60, float rate, float lossGain) {
        gain = std::min(0.9995f, std::pow(10.0f, -3.0f * period / (std::max(t60, 0.02f) * rate)) / std::max(lossGain, 1.0e-3f));
    }
    float tick(float in) {
        const float y = line.readInt(std::size_t(length));
        float f = fracC * y + fracX - fracC * fracY;
        fracX = y;
        fracY = f;
        for (int k = 0; k < dispStages; ++k) {
            const float a = dispC * f + dispX[std::size_t(k)] - dispC * dispY[std::size_t(k)];
            dispX[std::size_t(k)] = f;
            dispY[std::size_t(k)] = a;
            f = a;
        }
        lossZ += lossA * (f - lossZ);
        line.push(in + gain * lossZ);
        return y;
    }
};

// A plucked string (or a course of two): the pluck is the force a finger or plectrum
// releases at its position on the string (a pulse whose spectrum has the comb of the
// pluck point), softened by the finger or quill and brightened by velocity; a short
// noise of nail or pick; a damper that stops the string when the note ends (a
// harpsichord's jack). Lutes, ouds, guitars, harps, zithers, harpsichords.
struct Pluck {
    struct Shape {
        float position = 0.18f;     // pluck point, fraction of the string from the bridge
        float loss = 0.5f;          // 0 bright steel to 1 dull gut
        float stiffness = 0.15f;    // inharmonicity
        float course = 0;           // 0 one string; else a second, `courseCents` sharp
        float courseCents = 3.0f;
        float pickNoise = 0.3f;     // nail, pick or quill noise
        float dampSeconds = 0;      // > 0: the string is damped this long after the note ends
    };
    std::array<WaveguideString, 2> string;
    float rate = 48000, hz = 220, amp = 0, peak = 0, exciteLevel = 0, exciteZ = 0, exciteA = 0.5f, noiseAmp = 0, noiseDecay = 0.99f;
    float lowCutZ = 0, t60 = 1, noiseRamp = 0, noiseRampStep = 0.05f;
    int excite = 0, exciteLength = 1, flip = 1, courseDelay = 0, dampIn = -1, dampSamples = 0;
    Shape shape;
    uint64_t noiseIndex = 0;
    PanGains pan { 0.7071f, 0.7071f };
    bool active = false;
    void prepare(float sampleRate, float lowestHz) {
        rate = sampleRate;
        for (auto& s : string) s.prepare(rate, lowestHz);
    }
    void reset() {
        for (auto& s : string) s.clear();
        active = false;
        excite = 0;
        peak = 0;
        noiseAmp = 0;
        dampIn = -1;
    }
    float level() const { return active ? peak + (excite > 0 ? amp : 0.0f) : 0.0f; }
    // brightness 0..1 (finger to plectrum), velocity 0..1, length: samples until the note
    // ends (for the damper; < 0 rings out).
    void start(float noteHz, float decaySeconds, float brightness, float velocity, float panPosition, float sampleRate, uint64_t noiseAt,
               const Shape& s, int lengthSamples = -1) {
        rate = sampleRate;
        shape = s;
        hz = noteHz;
        t60 = decaySeconds;
        for (auto& st : string) st.clear();
        // Higher notes die sooner (shorter strings lose more per second).
        const float t = decaySeconds * std::clamp(std::pow(220.0f / std::max(hz, 30.0f), 0.35f), 0.35f, 1.8f);
        // Brightness is also the strings' age: new strings keep their highs longer.
        const float loss = clamp01(shape.loss + 0.5f * (0.5f - brightness));
        string[0].tune(hz, rate, t, loss, shape.stiffness);
        if (shape.course > 0.0f) string[1].tune(hz * std::exp2(shape.courseCents / 1200.0f), rate, t * 0.9f, loss, shape.stiffness);
        exciteLength = std::max(2, int(string[0].period + 0.5f));
        excite = exciteLength;
        flip = std::max(1, int(clamp01(shape.position) * float(exciteLength)));
        // The pulse's two levels give it no DC: +(1 - p) for the first p of it, -p after.
        exciteLevel = 1.0f;
        exciteZ = 0;
        const float b = clamp01(0.65f * brightness + 0.35f * velocity);
        exciteA = 1.0f - std::exp(-twoPi * lerpExp(500.0f, 14000.0f, b) / rate);
        amp = std::max(velocity, 0.0f) * 0.24f;
        noiseAmp = shape.pickNoise * velocity * (0.15f + 0.3f * b);
        noiseRamp = 0;
        noiseRampStep = 1.0f / (0.0004f * rate);
        noiseDecay = std::exp(-1.0f / (0.0035f * rate));
        courseDelay = shape.course > 0.0f ? int(0.0012f * rate) : 0;
        if (shape.course > 0.0f) courseIn.fill(0.0f);
        dampIn = lengthSamples;
        dampSamples = 0;
        lowCutZ = 0;
        noiseIndex = noiseAt;
        pan = panGains(panPosition);
        active = true;
        peak = amp;
    }
    float tick(const Stream& noise) {
        float in = 0;
        if (excite > 0) {
            const int at = exciteLength - excite;
            const float p = float(flip) / float(exciteLength);
            const float pulse = at < flip ? (1.0f - p) : -p;
            exciteZ += exciteA * (pulse * 2.0f + 0.15f * noise.white(noiseIndex + 7777) - exciteZ);
            in = exciteZ * amp;
            --excite;
        }
        float y = string[0].tick(in);
        if (shape.course > 0.0f) {
            // The second string of the course is plucked a moment later.
            static constexpr int ring = 4096;
            courseIn[std::size_t(courseAt & (ring - 1))] = in;
            const float delayed = courseDelay > 0 ? courseIn[std::size_t((courseAt - courseDelay) & (ring - 1))] : in;
            ++courseAt;
            y = 0.5f * (y + shape.course * string[1].tick(delayed)) * (2.0f / (1.0f + shape.course));
        }
        // Nail or pick: a short bright noise, outside the string.
        if (noiseAmp > 1.0e-5f) {
            const float n = noise.white(noiseIndex++);
            lowCutZ += 0.3f * (n - lowCutZ);
            noiseRamp = std::min(1.0f, noiseRamp + noiseRampStep);
            y += (n - lowCutZ) * noiseAmp * noiseRamp;
            noiseAmp *= noiseDecay;
        }
        if (dampIn >= 0 && --dampIn == 0 && shape.dampSeconds > 0.0f) {
            // The damper (a felt or a jack's cloth) stops the string, the highs first.
            for (auto& st : string) {
                st.lossA = std::min(st.lossA, 0.35f);
                st.setDecay(shape.dampSeconds, rate, 0.8f);
            }
        }
        peak = std::max(peak * 0.9999f, std::abs(y));
        if (excite == 0 && peak < 1.0e-4f) active = false;
        return y;
    }

private:
    std::array<float, 4096> courseIn {};
    int courseAt = 0;
};

// A struck modal object: up to eight resonant modes excited by a short raised-cosine
// strike (soft mallets strike for longer, so the attack swells over a few milliseconds
// and the highest modes are barely excited). With bell ratios it is a tuned bell, with
// bar ratios a wooden or metal bar, with harmonic ratios a soft chime.
// Extras (Extras, set before start): the lowest modes split into close doublets that
// beat (bells and bowls are never perfectly round), a pitch glide after the strike
// (gongs), a bloom (a tam-tam's shimmer swelling after the strike as energy climbs into
// the high modes), and each strike landing in a slightly different place (the modes'
// strengths vary a little from note to note).
struct Bell {
    static constexpr int maxModes = 8, maxDoublets = 3;
    struct Extras {
        float beatHz = 0;           // doublet split of the lowest modes
        float glideSemis = 0;       // pitch glide after the strike (+ up, - down)
        float glideSeconds = 0.4f;
        float bloom = 0;            // the high modes swell after the strike
        float variation = 0;        // strike-to-strike variation of the modes' strengths
    };
    std::array<Mode, maxModes> mode;
    std::array<Mode, maxDoublets> partner;
    std::array<float, maxModes> gain {}, freq {}, tau {};
    std::array<float, maxDoublets> partnerGain {};
    int modes = 0, strikeAt = 0, strikeLength = 1;
    float strikeAmp = 0, rate = 48000, age = 0, bloomLevel = 0;
    Phasor strikeShape;
    Extras extras;
    uint64_t noiseIndex = 0;
    PanGains pan { 0.7071f, 0.7071f };
    bool active = false;
    float energy = 0;
    void reset() {
        for (auto& m : mode) m.clear();
        for (auto& m : partner) m.clear();
        active = false;
        energy = 0;
        strikeAt = strikeLength = 1;
    }
    bool striking() const { return strikeAt < strikeLength; }
    float level() const { return active ? std::sqrt(energy) + (striking() ? 1.0f : 0.0f) : 0.0f; }
    // decaySeconds: time for the lowest mode to fall by 60 dB. ratios, decays (relative
    // to it) and gains for each mode; hardness 0..1 from a soft felt mallet (a 4 ms
    // strike) to a hard one (0.15 ms).
    void start(float hz, float decaySeconds, float hardness, float amplitude, float panPosition, float sampleRate, const float* ratios,
               const float* decays, const float* gains, int count) {
        rate = sampleRate;
        modes = std::min(count, maxModes);
        const uint64_t h = mix64(uint64_t(hz * 1000.0f) ^ (uint64_t(amplitude * 65536.0f) << 20) ^ (uint64_t(panPosition * 1000.0f + 5000.0f) << 40));
        for (int k = 0; k < modes; ++k) {
            const float f = hz * ratios[k];
            freq[std::size_t(k)] = f;
            tau[std::size_t(k)] = decaySeconds * decays[k] / 6.91f;   // T60 to time constant
            mode[std::size_t(k)].set(f, tau[std::size_t(k)], rate);
            // Where the strike lands: the upper modes a little stronger or weaker each time.
            const float vary = k == 0 ? 1.0f : 1.0f + extras.variation * bipolar(mix64(h + uint64_t(k)));
            // A soft mallet barely wakes the upper modes; a hard one brings them out.
            const float mallet = k == 0 ? 1.0f : lerp(0.35f, 1.4f, clamp01(hardness));
            gain[std::size_t(k)] = f < 0.45f * rate ? gains[k] * vary * mallet : 0.0f;
        }
        for (int k = 0; k < maxDoublets; ++k) {
            partnerGain[std::size_t(k)] = 0;
            if (extras.beatHz <= 0.0f || k >= modes) continue;
            const float f = freq[std::size_t(k)] + extras.beatHz * (1.0f + 0.6f * float(k));
            partner[std::size_t(k)].clear();
            partner[std::size_t(k)].set(f, tau[std::size_t(k)] * 0.9f, rate);
            partnerGain[std::size_t(k)] = f < 0.45f * rate ? 0.6f * gain[std::size_t(k)] : 0.0f;
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
        age = 0;
        bloomLevel = 0;
        noiseIndex = h;
    }
    float tick() {
        float x = 0;
        if (strikeAt < strikeLength) {
            strikeShape.step();
            x = strikeAmp * 0.5f * (1.0f + strikeShape.re);
            ++strikeAt;
        }
        // Noise feeding the upper half of the modes: the swell of a tam-tam.
        const float n = bloomLevel > 1.0e-5f ? bipolar(mix64(noiseIndex++)) * bloomLevel * 0.02f : 0.0f;
        float y = 0;
        for (int k = 0; k < modes; ++k) y += gain[std::size_t(k)] * mode[std::size_t(k)].tick(k >= modes / 2 ? x + n : x);
        for (int k = 0; k < maxDoublets; ++k)
            if (partnerGain[std::size_t(k)] > 0.0f) y += partnerGain[std::size_t(k)] * partner[std::size_t(k)].tick(x);
        return y;
    }
    // Once per chunk: the glide and bloom, and retire the voice when it has rung out.
    void update() {
        age += float(chunk) / rate;
        if (extras.glideSemis != 0.0f) {
            const float bend = std::exp2(extras.glideSemis * (1.0f - std::exp(-age / std::max(extras.glideSeconds, 0.01f))) / 12.0f);
            for (int k = 0; k < modes; ++k) mode[std::size_t(k)].retune(freq[std::size_t(k)] * bend, tau[std::size_t(k)], rate);
        }
        bloomLevel = extras.bloom * (1.0f - std::exp(-age / 0.3f)) * std::exp(-age / 2.0f);
        float e = 0;
        for (int k = 0; k < modes; ++k) e += gain[std::size_t(k)] * gain[std::size_t(k)] * mode[std::size_t(k)].energy();
        for (int k = 0; k < maxDoublets; ++k) e += partnerGain[std::size_t(k)] * partnerGain[std::size_t(k)] * partner[std::size_t(k)].energy();
        energy = e;
        if (!striking() && e < 1.0e-9f && bloomLevel < 1.0e-4f) active = false;
    }
};

// A drum. Kind TwoTone: two sine partials with a falling pitch envelope plus a noise
// burst through a band-pass (wood, stomps, clanks, and with the tone off, shakers).
// Kinds Membrane and Timpani: a skin's modes (the ideal circular membrane's, or the
// timpani's air-loaded ones near 1 : 1.5 : 2 : 2.44) struck by a stick or mallet of
// some hardness at some point between the centre and the edge (the edge brings out the
// higher modes), the pitch dropping as the skin's tension relaxes after a hard hit, a
// slap of noise, snare wires buzzing with the skin, and the shell's own knock.
enum DrumKind { DrumTwoTone, DrumMembrane, DrumTimpani };
struct Drum {
    static constexpr int maxModes = 6;
    Phasor a, b;
    Svf noiseBand, snareBand;
    std::array<Mode, maxModes> mode;
    std::array<float, maxModes> modeRatio {}, modeGain {}, modeTau {};
    Mode shellMode;
    int kind = DrumTwoTone, modes = 0, strikeAt = 0, strikeLength = 1;
    float hz = 80, endHz = 80, pitchCoef = 0.9f, ratio = 1.5f, partial = 0.4f, rate = 48000;
    float amp = 0, decay = 1, noise = 0, noiseDecay = 1, strikeAmp = 0, snareLevel = 0, snareDecay = 1, shellGain = 0, retuned = 0;
    // Membrane and Timpani settings (before start).
    float position = 0.3f, hardness = 0.5f, snare = 0, shellHz = 0, shell = 0;
    // The stick's slap on the skin: brighter and louder the harder the hit.
    Svf slapBand;
    float slap = 0, slapDecay = 1;
    Phasor strikeShape;
    uint64_t noiseIndex = 0;
    PanGains pan { 0.7071f, 0.7071f };
    bool active() const { return kind == DrumTwoTone ? (amp > 1.0e-4f || noise > 1.0e-4f) : (amp > 1.0e-4f || noise > 1.0e-4f || snareLevel > 1.0e-4f); }
    float level() const { return amp + noise; }
    void reset() {
        amp = noise = snareLevel = slap = 0;
        noiseBand.reset();
        snareBand.reset();
        slapBand.reset();
        for (auto& m : mode) m.clear();
        shellMode.clear();
    }
    // decaySeconds and noiseSeconds: time to fall by 60 dB.
    void start(float toneHz, float bend, float decaySeconds, float toneAmp, float noiseAmp, float noiseHz, float noiseQ, float noiseSeconds,
               float panPosition, float sampleRate, uint64_t noiseAt) {
        rate = sampleRate;
        endHz = toneHz;
        hz = toneHz * bend;
        pitchCoef = std::exp(-float(chunk) / (0.035f * rate));
        noise = noiseAmp;
        noiseDecay = Env::coefFor(noiseSeconds / 6.91f, rate);
        noiseBand.set(noiseHz, noiseQ, rate);
        noiseIndex = noiseAt;
        pan = panGains(panPosition);
        if (kind == DrumTwoTone) {
            // Alternate polarity from hit to hit: a run of decaying sines that all start
            // upward would add up to a DC offset.
            const float phase = ((noiseAt >> 24) & 1) != 0 ? pi : 0.0f;
            a.setPhase(phase);
            b.setPhase(phase);
            a.setFrequency(hz, rate);
            b.setFrequency(hz * ratio, rate);
            amp = toneAmp;
            decay = Env::coefFor(decaySeconds / 6.91f, rate);
            return;
        }
        const bool timpani = kind == DrumTimpani;
        modes = timpani ? 5 : maxModes;
        const float p = clamp01(position);
        for (int k = 0; k < modes; ++k) {
            const auto i = std::size_t(k);
            modeRatio[i] = timpani ? timpaniRatios[i] : membraneRatios[i];
            // The centre favours the lowest mode; towards the edge the others come up.
            const float g = timpani ? (k == 0 ? 1.0f : std::pow(0.72f, float(k)) * (0.6f + 0.8f * p))
                                    : (k == 0 ? 1.0f - 0.45f * p : std::pow(0.78f, float(k)) * (0.3f + 0.9f * p));
            // `partial` sets the overtones' level (0.4 as measured).
            modeGain[i] = hz * modeRatio[i] < 0.45f * rate ? g * (k == 0 ? 1.0f : partial / 0.4f) : 0.0f;
            // Higher modes die sooner (the timpani's principal tones ring on).
            modeTau[i] = decaySeconds / 6.91f / (timpani ? 1.0f + 0.35f * float(k) : 1.0f + 0.8f * float(k));
            mode[i].set(hz * modeRatio[i], modeTau[i], rate);
        }
        retuned = hz;
        // Mallet or stick: a raised cosine from 6 ms (felt) to 0.4 ms (wood).
        strikeLength = std::max(1, int(std::min(lerpExp(0.006f, 0.0004f, clamp01(hardness)), 0.5f / hz) * rate));
        strikeAt = 0;
        strikeAmp = 2.0f * toneAmp / float(strikeLength);
        strikeShape.setFrequency(rate / float(strikeLength), rate);
        strikeShape.setPhase(pi);
        amp = toneAmp;
        decay = Env::coefFor(decaySeconds / 6.91f, rate);
        snareLevel = snare * toneAmp;
        slap = 2.0f * toneAmp * toneAmp * (0.2f + 0.8f * hardness * hardness);
        slapDecay = Env::coefFor(lerp(0.09f, 0.05f, hardness) / 6.91f, rate);
        slapBand.set(std::min(lerpExp(900.0f, 4500.0f, clamp01(hardness)), 0.4f * rate), 0.9f, rate);
        snareDecay = Env::coefFor(0.22f / 6.91f, rate);
        snareBand.set(std::min(5200.0f, 0.4f * rate), 0.6f, rate);
        shellGain = shellHz > 0.0f ? shell : 0.0f;
        if (shellGain > 0.0f) shellMode.set(shellHz, 0.05f / 6.91f, rate);
    }
    void update(float sampleRate) {
        rate = sampleRate;
        hz = endHz + (hz - endHz) * pitchCoef;
        if (kind == DrumTwoTone) {
            a.setFrequency(hz, rate);
            b.setFrequency(hz * ratio, rate);
            a.normalise();
            b.normalise();
            return;
        }
        // The skin relaxes: every mode follows the falling pitch.
        if (std::abs(hz - retuned) > retuned * 0.002f) {
            retuned = hz;
            for (int k = 0; k < modes; ++k) mode[std::size_t(k)].retune(hz * modeRatio[std::size_t(k)], modeTau[std::size_t(k)], rate);
        }
    }
    float tick(const Stream& noiseSource) {
        if (kind == DrumTwoTone) {
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
        float x = 0;
        if (strikeAt < strikeLength) {
            strikeShape.step();
            x = strikeAmp * 0.5f * (1.0f + strikeShape.re);
            ++strikeAt;
        }
        float y = 0;
        for (int k = 0; k < modes; ++k) y += modeGain[std::size_t(k)] * mode[std::size_t(k)].tick(x);
        if (shellGain > 0.0f) y += shellGain * shellMode.tick(x);
        amp *= decay;   // only to know when the skin has died away
        if (noise > 1.0e-4f || snareLevel > 1.0e-4f || slap > 1.0e-4f) {
            const float n = noiseSource.white(noiseIndex++);
            if (slap > 1.0e-4f) {
                y += slapBand.bandpass(n) * slap;
                slap *= slapDecay;
            }
            if (noise > 1.0e-4f) {
                y += noiseBand.bandpass(n) * noise;
                noise *= noiseDecay;
            }
            if (snareLevel > 1.0e-4f) {
                // The wires rattle with the skin's motion.
                y += snareBand.bandpass(n) * snareLevel * (0.4f + std::min(std::abs(y) * 4.0f, 1.2f));
                snareLevel *= snareDecay;
            }
        }
        return y;
    }
};

} // namespace wb::dsp
