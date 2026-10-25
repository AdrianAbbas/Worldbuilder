#pragma once
// Each Place's own effect character: a small room or hall, a tape echo with wow,
// flutter and saturation, a ping-pong delay and tape warmth. Nothing here
// allocates after prepare().
#include "Voices.h"

namespace wb::dsp {

// A compact 4-line feedback delay network with input diffusion: the Place's own
// space (a stone hall, a wooden room, a metal hangar). Size scales the lines, decay
// is a T60, damping sets how dark the tail is.
struct SmallSpace {
    static constexpr int lines = 4;
    std::array<DelayLine, lines> delay;
    std::array<OnePole, lines> damping;
    std::array<Allpass, 2> diffuser;
    std::array<float, lines> length {}, feedback {};
    float rate = 48000, modPhase = 0;
    void prepare(float sampleRate) {
        rate = sampleRate;
        for (auto& d : delay) d.prepare(std::size_t(0.12f * rate));
        diffuser[0].prepare(std::size_t(0.0047f * rate));
        diffuser[1].prepare(std::size_t(0.0113f * rate));
        for (auto& d : diffuser) d.g = 0.6f;
        set(0.5f, 1.5f, 5000.0f);
        clear();
    }
    void clear() {
        for (auto& d : delay) d.clear();
        for (auto& d : diffuser) d.clear();
        for (auto& f : damping) f.reset();
        modPhase = 0;
    }
    // size 0..1, decay in seconds, damping cutoff in Hz. Once per chunk.
    void set(float size, float decaySeconds, float dampHz) {
        static constexpr float base[lines] { 0.0297f, 0.0371f, 0.0411f, 0.0437f };
        const float scale = lerp(0.35f, 2.6f, clamp01(size));
        for (int i = 0; i < lines; ++i) {
            length[std::size_t(i)] = std::min(base[i] * scale * rate, 0.115f * rate);
            feedback[std::size_t(i)] = std::pow(10.0f, -3.0f * length[std::size_t(i)] / (std::max(decaySeconds, 0.1f) * rate));
            damping[std::size_t(i)].setCutoff(dampHz, rate);
        }
    }
    void process(float inL, float inR, float& outL, float& outR) {
        const float x = diffuser[1].process(diffuser[0].process(0.5f * (inL + inR)));
        modPhase += 0.7f / rate;
        if (modPhase > 1.0f) modPhase -= 1.0f;
        const float wobble = 1.0f + 0.002f * std::sin(twoPi * modPhase);
        std::array<float, lines> y {};
        for (int i = 0; i < lines; ++i) y[std::size_t(i)] = damping[std::size_t(i)].lp(delay[std::size_t(i)].read(length[std::size_t(i)] * (i == 0 ? wobble : 1.0f)));
        // 4x4 Hadamard, normalised.
        const float a = y[0] + y[1], b = y[0] - y[1], c = y[2] + y[3], d = y[2] - y[3];
        const std::array<float, lines> mixed { 0.5f * (a + c), 0.5f * (b + d), 0.5f * (a - c), 0.5f * (b - d) };
        const float inputSign[lines] { 1.0f, -1.0f, 1.0f, -1.0f };
        for (int i = 0; i < lines; ++i) delay[std::size_t(i)].push(x * inputSign[i] + mixed[std::size_t(i)] * feedback[std::size_t(i)]);
        outL = y[0] + 0.6f * y[2];
        outR = y[1] + 0.6f * y[3];
    }
};

// Tape echo: a stereo delay whose read point wobbles (wow and flutter), with a
// darkening, softly saturating feedback path. Castle halls and old machinery.
struct TapeEcho {
    std::array<DelayLine, 2> line;
    std::array<OnePole, 2> tone, lowCut;
    Smoother time;
    float rate = 48000, feedback = 0.4f, wow = 0, wowPhase = 0, flutterPhase = 0, maxSamples = 1;
    void prepare(float sampleRate, float maxSeconds) {
        rate = sampleRate;
        maxSamples = maxSeconds * rate;
        for (auto& l : line) l.prepare(std::size_t(maxSamples) + 64);
        time.setTime(0.4f, rate);
        time.snap(0.4f * rate);
        clear();
    }
    // Back to the state after prepare(): a cleared echo always continues the same way.
    void clear() {
        for (auto& l : line) l.clear();
        for (auto& f : tone) f.reset();
        for (auto& f : lowCut) f.reset();
        time.snap(0.4f * rate);
        targetSamples = 0.4f * rate;
        wowPhase = flutterPhase = 0;
    }
    // Once per chunk.
    void set(float seconds, float feedbackAmount, float toneHz, float wowAmount) {
        time.value = std::min(time.value, maxSamples - 8.0f);
        targetSamples = std::clamp(seconds * rate, 8.0f, maxSamples - 8.0f);
        feedback = std::clamp(feedbackAmount, 0.0f, 0.85f);
        for (auto& f : tone) f.setCutoff(toneHz, rate);
        for (auto& f : lowCut) f.setCutoff(120.0f, rate);
        wow = wowAmount;
    }
    void process(float inL, float inR, float& outL, float& outR) {
        wowPhase += 0.55f / rate;
        flutterPhase += 6.3f / rate;
        if (wowPhase > 1.0f) wowPhase -= 1.0f;
        if (flutterPhase > 1.0f) flutterPhase -= 1.0f;
        const float t = time.next(targetSamples) * (1.0f + wow * (0.004f * std::sin(twoPi * wowPhase) + 0.0012f * std::sin(twoPi * flutterPhase)));
        const float yl = line[0].read(t), yr = line[1].read(t * 1.031f);
        line[0].push(inL + softClip(lowCut[0].hp(tone[0].lp(yr)) * feedback));
        line[1].push(inR + softClip(lowCut[1].hp(tone[1].lp(yl)) * feedback));
        outL = yl;
        outR = yr;
    }
    float targetSamples = 0.4f * 48000;
};

// Ping-pong delay: repeats bounce between left and right. Spaceport.
struct PingPong {
    std::array<DelayLine, 2> line;
    std::array<OnePole, 2> tone;
    Smoother time;
    float rate = 48000, feedback = 0.4f, targetSamples = 1000, maxSamples = 1;
    void prepare(float sampleRate, float maxSeconds) {
        rate = sampleRate;
        maxSamples = maxSeconds * rate;
        for (auto& l : line) l.prepare(std::size_t(maxSamples) + 64);
        time.setTime(0.3f, rate);
        time.snap(0.3f * rate);
        clear();
    }
    void clear() {
        for (auto& l : line) l.clear();
        for (auto& f : tone) f.reset();
        time.snap(0.3f * rate);
    }
    void set(float seconds, float feedbackAmount, float toneHz) {
        targetSamples = std::clamp(seconds * rate, 8.0f, maxSamples - 8.0f);
        feedback = std::clamp(feedbackAmount, 0.0f, 0.8f);
        for (auto& f : tone) f.setCutoff(toneHz, rate);
    }
    void process(float inL, float inR, float& outL, float& outR) {
        const float t = time.next(targetSamples);
        const float yl = line[0].read(t), yr = line[1].read(t);
        line[0].push(0.5f * (inL + inR) + tone[0].lp(yr) * feedback);
        line[1].push(tone[1].lp(yl) * feedback);
        outL = yl;
        outR = yr;
    }
};

// Tape warmth: soft saturation, a gentle high cut and a slowly wobbling short delay
// mixed with the dry signal (wow and flutter). Village.
struct Warmth {
    std::array<DelayLine, 2> line;
    std::array<OnePole, 2> tone;
    float rate = 48000, depth = 0, phase = 0, flutter = 0;
    void prepare(float sampleRate) {
        rate = sampleRate;
        for (auto& l : line) l.prepare(std::size_t(0.02f * rate));
        clear();
    }
    void clear() {
        for (auto& l : line) l.clear();
        for (auto& f : tone) f.reset();
        phase = flutter = 0;
    }
    void set(float amount) {
        depth = clamp01(amount);
        for (auto& f : tone) f.setCutoff(lerpExp(16000.0f, 5500.0f, depth), rate);
    }
    void process(float& l, float& r) {
        phase += 0.45f / rate;
        flutter += 5.1f / rate;
        if (phase > 1.0f) phase -= 1.0f;
        if (flutter > 1.0f) flutter -= 1.0f;
        const float drive = 1.0f + 2.5f * depth;
        const float sl = softClip(l * drive) / drive * (1.0f + 0.3f * depth), sr = softClip(r * drive) / drive * (1.0f + 0.3f * depth);
        line[0].push(sl);
        line[1].push(sr);
        const float wobble = std::sin(twoPi * phase) + 0.25f * std::sin(twoPi * flutter);
        const float d = (0.006f + 0.0018f * depth * wobble) * rate;
        const float wl = line[0].read(d), wr = line[1].read(d * 1.05f);
        l = tone[0].lp(lerp(sl, wl, 0.55f * depth));
        r = tone[1].lp(lerp(sr, wr, 0.55f * depth));
    }
};

// A metallic comb resonance: two damped feedback combs tuned to notes (one each
// side), so whatever passes through rings at their harmonics. Factory.
struct CombResonance {
    std::array<DelayLine, 2> line;
    std::array<OnePole, 2> damp;
    Smoother periodA, periodB;
    float rate = 48000, feedback = 0.6f, targetA = 100, targetB = 100;
    void prepare(float sampleRate) {
        rate = sampleRate;
        for (auto& l : line) l.prepare(std::size_t(rate / 30.0f) + 8);
        periodA.setTime(0.05f, rate);
        periodB.setTime(0.05f, rate);
        clear();
    }
    void clear() {
        for (auto& l : line) l.clear();
        for (auto& f : damp) f.reset();
        periodA.snap(rate / 110.0f);
        periodB.snap(rate / 165.0f);
        targetA = periodA.value;
        targetB = periodB.value;
    }
    // Once per chunk: the two notes (Hz), feedback 0..0.95, damping cutoff.
    void set(float hzA, float hzB, float feedbackAmount, float dampHz) {
        targetA = std::clamp(rate / std::max(hzA, 35.0f), 2.0f, float(line[0].mask - 4));
        targetB = std::clamp(rate / std::max(hzB, 35.0f), 2.0f, float(line[1].mask - 4));
        feedback = std::clamp(feedbackAmount, 0.0f, 0.95f);
        for (auto& f : damp) f.setCutoff(dampHz, rate);
    }
    // Mono in, the two combs out (one each side).
    void process(float in, float& outL, float& outR) {
        const float a = damp[0].lp(line[0].read(periodA.next(targetA))), b = damp[1].lp(line[1].read(periodB.next(targetB)));
        line[0].push(in + feedback * a);
        line[1].push(in + feedback * b);
        outL = a;
        outR = b;
    }
};

// A gentle stereo chorus: two short delays swept slowly in opposite directions,
// mixed with the dry signal. City, Biodome, Beach.
struct Chorus {
    std::array<DelayLine, 2> line;
    float rate = 48000, phase = 0, speed = 0.45f, depth = 0.0025f, mix = 0.4f;
    void prepare(float sampleRate) {
        rate = sampleRate;
        for (auto& l : line) l.prepare(std::size_t(0.04f * rate));
        clear();
    }
    void clear() {
        for (auto& l : line) l.clear();
        phase = 0;
    }
    void set(float hz, float depthSeconds, float mixAmount) {
        speed = hz;
        depth = depthSeconds;
        mix = clamp01(mixAmount);
    }
    void process(float& l, float& r) {
        phase += speed / rate;
        if (phase > 1.0f) phase -= 1.0f;
        const float m = std::sin(twoPi * phase);
        line[0].push(l);
        line[1].push(r);
        const float wl = line[0].read((0.012f + depth * m) * rate), wr = line[1].read((0.014f - depth * m) * rate);
        l = lerp(l, wl, mix * 0.5f) + wr * mix * 0.15f;
        r = lerp(r, wr, mix * 0.5f) + wl * mix * 0.15f;
    }
};

// A warped delay: repeats whose time drifts with a slow random curve, so each one
// bends in pitch, through a darkening feedback path, crossing left and right. Hive.
struct WarpDelay {
    std::array<DelayLine, 2> line;
    std::array<OnePole, 2> tone;
    Smoother time;
    Stream wobble;
    float rate = 48000, feedback = 0.4f, warp = 0.02f, targetSamples = 1000, maxSamples = 1;
    double clock = 0;
    void prepare(float sampleRate, float maxSeconds, uint32_t seed) {
        rate = sampleRate;
        maxSamples = maxSeconds * rate;
        for (auto& l : line) l.prepare(std::size_t(maxSamples * 1.1f) + 64);
        time.setTime(0.3f, rate);
        wobble.setKey(seed, 77);
        clear();
    }
    void clear() {
        for (auto& l : line) l.clear();
        for (auto& f : tone) f.reset();
        time.snap(0.3f * rate);
        targetSamples = 0.3f * rate;
        clock = 0;
    }
    // Once per chunk: seconds, feedback, tone, warp (how far the time drifts, 0..0.1).
    void set(float seconds, float feedbackAmount, float toneHz, float warpAmount) {
        targetSamples = std::clamp(seconds * rate, 8.0f, maxSamples - 8.0f);
        feedback = std::clamp(feedbackAmount, 0.0f, 0.85f);
        warp = std::clamp(warpAmount, 0.0f, 0.1f);
        for (auto& f : tone) f.setCutoff(toneHz, rate);
        clock += double(chunk) / double(rate);
        bend = 1.0f + warp * wobble.smooth(clock, 0.6);
    }
    void process(float inL, float inR, float& outL, float& outR) {
        const float t = time.next(targetSamples) * bend;
        const float yl = line[0].read(t), yr = line[1].read(t * 1.07f);
        line[0].push(inL + softClip(tone[0].lp(yr) * feedback));
        line[1].push(inR + softClip(tone[1].lp(yl) * feedback));
        outL = yl;
        outR = yr;
    }
    float bend = 1;
};

// A slapback echo: one short, slightly dark repeat each side (a little feedback for a
// second, fainter one). Cavern, Military Camp.
struct Slapback {
    std::array<DelayLine, 2> line;
    std::array<OnePole, 2> tone;
    float rate = 48000, samples = 5000, feedback = 0.15f;
    void prepare(float sampleRate) {
        rate = sampleRate;
        for (auto& l : line) l.prepare(std::size_t(0.3f * rate));
        clear();
    }
    void clear() {
        for (auto& l : line) l.clear();
        for (auto& f : tone) f.reset();
    }
    void set(float seconds, float feedbackAmount, float toneHz) {
        samples = std::clamp(seconds * rate, 8.0f, 0.29f * rate);
        feedback = std::clamp(feedbackAmount, 0.0f, 0.5f);
        for (auto& f : tone) f.setCutoff(toneHz, rate);
    }
    void process(float inL, float inR, float& outL, float& outR) {
        const float yl = tone[0].lp(line[0].read(samples)), yr = tone[1].lp(line[1].read(samples * 1.13f));
        line[0].push(inL + yl * feedback);
        line[1].push(inR + yr * feedback);
        outL = yl;
        outR = yr;
    }
};

// Four all-passes in a row: smears attacks into a soft wash without a tail of its own
// (Forest's diffusion), each side its own lengths.
struct Diffuser {
    std::array<Allpass, 8> stage;
    void prepare(float rate) {
        static constexpr float ms[8] { 3.1f, 7.3f, 11.9f, 17.3f, 3.7f, 8.1f, 12.7f, 16.1f };
        for (int i = 0; i < 8; ++i) {
            stage[std::size_t(i)].prepare(std::size_t(ms[i] * 0.001f * rate));
            stage[std::size_t(i)].g = 0.62f;
        }
    }
    void clear() {
        for (auto& a : stage) a.clear();
    }
    void process(float& l, float& r) {
        for (int i = 0; i < 4; ++i) l = stage[std::size_t(i)].process(l);
        for (int i = 4; i < 8; ++i) r = stage[std::size_t(i)].process(r);
    }
};

// Lo-fi decay: a worn recording. The signal is held at a lower sample rate (gently,
// with a high cut after, so little aliasing), rounded to fewer levels, and its top end
// fades in and out slowly as if the tape were wearing through. Ruins.
struct LoFi {
    std::array<OnePole, 4> tone;   // two poles each side
    Stream wear;
    float rate = 48000, hold = 1, count = 0, heldL = 0, heldR = 0, levels = 4096, mix = 0, toneHz = 6000;
    double clock = 0;
    void prepare(float sampleRate, uint32_t seed) {
        rate = sampleRate;
        wear.setKey(seed, 91);
        clear();
    }
    void clear() {
        for (auto& f : tone) f.reset();
        count = 0;
        heldL = heldR = 0;
        clock = 0;
    }
    // Once per chunk: the held rate (Hz), bits, the top end (Hz) and the amount (0..1).
    void set(float holdHz, float bits, float topHz, float amount) {
        hold = std::max(1.0f, rate / std::max(holdHz, 1000.0f));
        levels = std::exp2(std::clamp(bits, 4.0f, 16.0f) - 1.0f);
        mix = clamp01(amount);
        clock += double(chunk) / double(rate);
        // The worn top end comes and goes.
        const float worn = 1.0f - 0.45f * mix * clamp01(0.5f + 0.9f * wear.smooth(clock, 0.15));
        for (auto& f : tone) f.setCutoff(topHz * worn, rate);
    }
    void process(float& l, float& r) {
        if (mix <= 0.0f) return;
        count += 1.0f;
        if (count >= hold) {
            count -= hold;
            heldL = std::round(l * levels) / levels;
            heldR = std::round(r * levels) / levels;
        }
        const float wl = tone[2].lp(tone[0].lp(heldL)), wr = tone[3].lp(tone[1].lp(heldR));
        l = lerp(l, wl, mix);
        r = lerp(r, wr, mix);
    }
};

// Reverse echoes: each grain of the recent past played backwards (two overlapping
// grains, so the swells flow on), softly fed back, crossing left and right. A sound
// echoes as a swell that rises into it. Ruins.
struct ReverseEcho {
    std::array<DelayLine, 2> line;
    std::array<OnePole, 2> tone;
    float rate = 48000, grain = 12000, phase = 0, feedback = 0.3f, maxGrain = 1;
    void prepare(float sampleRate, float maxSeconds) {
        rate = sampleRate;
        maxGrain = maxSeconds * rate;
        for (auto& l : line) l.prepare(std::size_t(2.0f * maxGrain) + 64);
        clear();
    }
    void clear() {
        for (auto& l : line) l.clear();
        for (auto& f : tone) f.reset();
        phase = 0;
    }
    // Once per chunk: the grain length (seconds), feedback and tone.
    void set(float seconds, float feedbackAmount, float toneHz) {
        grain = std::clamp(seconds * rate, 256.0f, maxGrain);
        feedback = std::clamp(feedbackAmount, 0.0f, 0.7f);
        for (auto& f : tone) f.setCutoff(toneHz, rate);
    }
    void process(float inL, float inR, float& outL, float& outR) {
        phase += 1.0f / grain;
        if (phase >= 1.0f) phase -= 1.0f;
        float yl = 0, yr = 0;
        for (int k = 0; k < 2; ++k) {
            float p = phase + 0.5f * float(k);
            if (p >= 1.0f) p -= 1.0f;
            // The read point falls back twice as fast as time moves on: backwards.
            const float d = 8.0f + 2.0f * p * grain, s = std::sin(pi * p), w = s * s;
            yl += line[0].read(d) * w;
            yr += line[1].read(d * 1.01f) * w;
        }
        line[0].push(inL + tone[0].lp(yr) * feedback);
        line[1].push(inR + tone[1].lp(yl) * feedback);
        outL = yl;
        outR = yr;
    }
};

// A filtered delay: repeats through a band that sweeps slowly up and down, so each
// one comes back thinner and in another colour, crossing left and right. Spaceship.
struct FilteredDelay {
    std::array<DelayLine, 2> line;
    std::array<Svf, 2> band;
    Smoother time;
    float rate = 48000, feedback = 0.4f, targetSamples = 1000, maxSamples = 1, q = 1.2f, lowHz = 500, highHz = 3000, sweepHz = 0.07f, sweep = 0;
    void prepare(float sampleRate, float maxSeconds) {
        rate = sampleRate;
        maxSamples = maxSeconds * rate;
        for (auto& l : line) l.prepare(std::size_t(maxSamples) + 64);
        time.setTime(0.3f, rate);
        clear();
    }
    void clear() {
        for (auto& l : line) l.clear();
        for (auto& f : band) f.reset();
        time.snap(0.3f * rate);
        targetSamples = 0.3f * rate;
        sweep = 0;
    }
    // Once per chunk: seconds, feedback, the band's range (Hz) and how fast it sweeps.
    void set(float seconds, float feedbackAmount, float fromHz, float toHz, float hz, float resonance) {
        targetSamples = std::clamp(seconds * rate, 8.0f, maxSamples - 8.0f);
        feedback = std::clamp(feedbackAmount, 0.0f, 0.85f);
        sweep += hz * float(chunk) / rate;
        if (sweep > 1.0f) sweep -= 1.0f;
        const float x = 0.5f - 0.5f * std::cos(twoPi * sweep);
        band[0].set(lerpExp(fromHz, toHz, x), resonance, rate);
        band[1].set(lerpExp(fromHz, toHz, 1.0f - x), resonance, rate);
    }
    void process(float inL, float inR, float& outL, float& outR) {
        const float t = time.next(targetSamples);
        const float yl = line[0].read(t), yr = line[1].read(t * 0.93f);
        line[0].push(inL + softClip(band[0].bandpass(yr) * feedback));
        line[1].push(inR + softClip(band[1].bandpass(yl) * feedback));
        outL = yl;
        outR = yr;
    }
};

// Rolling modulation: a ship's slow roll. The image sways from side to side, the
// level heaves, and a short modulated delay bends the pitch a few cents with the
// swell. Sea Ship.
struct Roll {
    std::array<DelayLine, 2> line;
    float rate = 48000, phase = 0, hz = 0.12f, depth = 0.5f;
    float pan = 0, heave = 1;
    void prepare(float sampleRate) {
        rate = sampleRate;
        for (auto& l : line) l.prepare(std::size_t(0.03f * rate));
        clear();
    }
    void clear() {
        for (auto& l : line) l.clear();
        phase = 0;
    }
    // Once per chunk: the roll's speed (Hz) and depth (0..1).
    void set(float rollHz, float amount) {
        hz = rollHz;
        depth = clamp01(amount);
    }
    void process(float& l, float& r) {
        phase += hz / rate;
        if (phase > 1.0f) phase -= 1.0f;
        const float s = std::sin(twoPi * phase), c = std::cos(twoPi * phase);
        line[0].push(l);
        line[1].push(r);
        const float d = (0.008f + 0.004f * depth * s) * rate;
        const float wl = line[0].read(d), wr = line[1].read(d);
        // Sway: one side swells as the other ebbs; heave: the whole lifts and falls.
        const float sway = 0.35f * depth * s, lift = 1.0f + 0.18f * depth * c;
        pan = sway;
        heave = lift;
        l = wl * (1.0f - sway) * lift;
        r = wr * (1.0f + sway) * lift;
    }
};

// Detune: a gentle pitch shifter (two crossfaded read heads gliding through a short
// delay), each side shifted by its own few cents and mixed with the dry signal, with
// an optional slow wow that bends the shift. A glossy, slightly detuned sheen; turned
// further down and wobbling it is a vaporwave slowdown. Mall.
struct Detune {
    std::array<DelayLine, 2> line;
    std::array<float, 2> phase {}, step {};
    float rate = 48000, window = 2400, mix = 0, wowPhase = 0, wowHz = 0.3f, wow = 0;
    std::array<float, 2> cents {};
    void prepare(float sampleRate) {
        rate = sampleRate;
        window = 0.05f * rate;
        for (auto& l : line) l.prepare(std::size_t(window * 1.5f) + 8);
        clear();
    }
    void clear() {
        for (auto& l : line) l.clear();
        phase = { 0.0f, 0.5f };
        wowPhase = 0;
    }
    // Once per chunk: the shift of each side (cents), the wet mix, the wow (cents) and its speed.
    void set(float centsLeft, float centsRight, float mixAmount, float wowCents, float wowSpeed) {
        cents = { centsLeft, centsRight };
        mix = clamp01(mixAmount);
        wow = wowCents;
        wowHz = wowSpeed;
    }
    void process(float& l, float& r) {
        wowPhase += wowHz / rate;
        if (wowPhase > 1.0f) wowPhase -= 1.0f;
        const float bend = wow * std::sin(twoPi * wowPhase);
        float* io[2] { &l, &r };
        for (std::size_t c = 0; c < 2; ++c) {
            line[c].push(*io[c]);
            // The delay grows (pitch falls) or shrinks (rises) by 1 - ratio a sample.
            const float ratio = std::exp2((cents[c] + bend) / 1200.0f);
            float& p = phase[c];
            p += (1.0f - ratio) / window;
            p -= std::floor(p);
            const float q = p + 0.5f - std::floor(p + 0.5f);
            const float a = std::sin(pi * p), b = std::sin(pi * q);
            const float wet = line[c].read(2.0f + p * window) * a * a + line[c].read(2.0f + q * window) * b * b;
            *io[c] = lerp(*io[c], wet, mix);
        }
    }
};

} // namespace wb::dsp
