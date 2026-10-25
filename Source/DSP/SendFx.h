#pragma once
// Each instrument's own effects (Milestone 6).
//
// Per instrument (cheap, in the slot): a character effect (saturation, chorus,
// phaser, resonant low-pass, lo-fi) with its amount, a dark-to-bright tilt and a
// stereo width, then sends to the shared buses.
// Shared (one of each type for the whole plugin, fed by every instrument's send):
// five reverbs (Room, Hall, Cathedral, Plate, Cave) and four tempo delays (Tape,
// Ping-Pong, Dotted, Reverse). A bus nothing has fed for longer than its tail sleeps.
//
// Changing a type never clicks: a character effect fades out, switches and fades
// back in; a send moves from one bus to the other with smoothed levels.
// Nothing here allocates after prepare().
#include "../FxDefaults.h"
#include "PlaceFx.h"
#include "Instruments.h"

namespace wb::dsp {

//==============================================================================
// A stereo feedback delay network: pre-delay, input diffusion, eight lines mixed by
// a Householder matrix with damping in the loop. The type sets the line lengths,
// decay, damping, diffusion and modulation; the Cave adds discrete early echoes.
struct SendReverb {
    static constexpr int lines = 8;
    std::array<DelayLine, lines> delay;
    std::array<OnePole, lines> damping;
    std::array<float, lines> length {}, feedback {};
    std::array<Allpass, 4> diffuseL, diffuseR;
    DelayLine pre;
    std::array<OnePole, 2> lowCut, highCut;
    float rate = 48000, preSamples = 0, modDepth = 0, modHz = 0.3f, modPhase = 0, outGain = 1, t60 = 1;
    float earlyA = 0, earlyB = 0, earlyGain = 0;
    int diffusers = 2, type = RevRoom;
    int quiet = 0, sleepAfter = 1;

    void prepare(float sampleRate, int reverbType) {
        rate = sampleRate;
        type = reverbType;
        // Line lengths (ms) and settings for each type.
        static constexpr float room[lines] { 7.1f, 9.3f, 11.7f, 13.9f, 16.3f, 18.7f, 21.1f, 23.9f };
        static constexpr float cave[lines] { 41.0f, 53.0f, 67.0f, 79.0f, 97.0f, 109.0f, 127.0f, 139.0f };
        float scale = 1, dampHz = 7000, preMs = 4, lowHz = 80, highHz = 16000;
        const float* base = room;
        switch (type) {
            case RevRoom: scale = 1.0f; t60 = 0.75f; dampHz = 7000; preMs = 4; diffusers = 2; modDepth = 0; outGain = 0.45f; break;
            case RevHall: scale = 2.6f; t60 = 2.4f; dampHz = 5200; preMs = 22; diffusers = 4; modDepth = 0.0004f; outGain = 0.42f; break;
            case RevCathedral: scale = 5.0f; t60 = 6.0f; dampHz = 3600; preMs = 45; diffusers = 4; modDepth = 0.0006f; highHz = 9000; outGain = 0.42f; break;
            case RevPlate: scale = 1.6f; t60 = 2.0f; dampHz = 10000; preMs = 0.5f; diffusers = 4; modDepth = 0.0001f; lowHz = 200; outGain = 0.38f; break;
            default:   // Cave: long irregular lines, dark, slapping early echoes
                base = cave;
                t60 = 3.8f;
                dampHz = 2400;
                preMs = 30;
                diffusers = 2;
                modDepth = 0.0002f;
                highHz = 7000;
                outGain = 0.55f;
                earlyA = 0.071f * rate;
                earlyB = 0.153f * rate;
                earlyGain = 0.35f;
                break;
        }
        float longest = 0;
        for (int i = 0; i < lines; ++i) {
            length[std::size_t(i)] = base[i] * 0.001f * scale * rate;
            longest = std::max(longest, length[std::size_t(i)]);
            feedback[std::size_t(i)] = std::pow(10.0f, -3.0f * length[std::size_t(i)] / (t60 * rate));
            damping[std::size_t(i)].setCutoff(dampHz, rate);
        }
        for (auto& d : delay) d.prepare(std::size_t(longest * (1.0f + modDepth * 2000.0f) + modDepth * rate + 8.0f));
        static constexpr float diffMs[4] { 1.7f, 3.9f, 6.1f, 9.3f };
        for (int k = 0; k < 4; ++k) {
            diffuseL[std::size_t(k)].prepare(std::size_t(diffMs[k] * 0.001f * rate));
            diffuseR[std::size_t(k)].prepare(std::size_t(diffMs[k] * 1.13f * 0.001f * rate));
            diffuseL[std::size_t(k)].g = diffuseR[std::size_t(k)].g = type == RevPlate ? 0.7f : 0.6f;
        }
        preSamples = std::max(1.0f, preMs * 0.001f * rate);
        pre.prepare(std::size_t(std::max(preSamples, earlyB) + 16.0f));
        for (auto& f : lowCut) f.setCutoff(lowHz, rate);
        for (auto& f : highCut) f.setCutoff(highHz, rate);
        // Asleep once nothing has come in for a tail of about 96 dB.
        sleepAfter = int((t60 * 1.6f + 0.2f) * rate);
        clear();
    }
    void clear() {
        for (auto& d : delay) d.clear();
        for (auto& f : damping) f.reset();
        for (auto& a : diffuseL) a.clear();
        for (auto& a : diffuseR) a.clear();
        pre.clear();
        for (auto& f : lowCut) f.reset();
        for (auto& f : highCut) f.reset();
        modPhase = 0;
        quiet = sleepAfter;
    }
    bool asleep() const { return quiet >= sleepAfter; }
    // Adds the reverb of inL/inR (n samples) to outL/outR.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n, bool fed) {
        quiet = fed ? 0 : std::min(quiet + n, sleepAfter);
        if (asleep()) return;
        const float modStep = modHz / rate, depth = modDepth * rate;
        for (int i = 0; i < n; ++i) {
            const float xl = fed ? highCut[0].lp(lowCut[0].hp(inL[i])) : highCut[0].lp(lowCut[0].hp(0.0f));
            const float xr = fed ? highCut[1].lp(lowCut[1].hp(inR[i])) : highCut[1].lp(lowCut[1].hp(0.0f));
            pre.push(0.5f * (xl + xr));
            float dl = pre.read(preSamples), dr = dl;
            float early = 0;
            if (earlyGain > 0.0f) early = earlyGain * (pre.read(earlyA) - 0.7f * pre.read(earlyB));
            // Keep the stereo picture of the input in the diffusion.
            dl += 0.5f * (xl - xr) * 0.6f;
            dr -= 0.5f * (xl - xr) * 0.6f;
            for (int k = 0; k < diffusers; ++k) {
                dl = diffuseL[std::size_t(k)].process(dl);
                dr = diffuseR[std::size_t(k)].process(dr);
            }
            modPhase += modStep;
            if (modPhase >= 1.0f) modPhase -= 1.0f;
            const float m = depth > 0.0f ? sinCycles(modPhase) * depth : 0.0f;
            std::array<float, lines> y {};
            float sum = 0;
            for (int k = 0; k < lines; ++k) {
                const bool modulated = depth > 0.0f && (k == 0 || k == 5);
                const float raw = modulated ? delay[std::size_t(k)].read(length[std::size_t(k)] + (k == 0 ? m : -m))
                                            : delay[std::size_t(k)].readInt(std::size_t(length[std::size_t(k)]));
                y[std::size_t(k)] = damping[std::size_t(k)].lp(raw);
                sum += y[std::size_t(k)];
            }
            sum *= 2.0f / float(lines);
            for (int k = 0; k < lines; ++k) {
                const float in = (k & 1) == 0 ? dl : dr;
                const float sign = (k & 2) == 0 ? 1.0f : -1.0f;
                delay[std::size_t(k)].push(sign * in + (y[std::size_t(k)] - sum) * feedback[std::size_t(k)]);
            }
            const float l = y[0] - y[2] + y[4] - y[6] + early, r = y[1] - y[3] + y[5] - y[7] + early * 0.8f;
            outL[i] += l * outGain;
            outR[i] += r * outGain;
        }
    }
};

//==============================================================================
// A tempo delay. Tape: a quarter note with wow, flutter, a darkening and saturating
// feedback path. Ping-Pong: eighth-note repeats bouncing left and right. Dotted: a
// dotted eighth, slightly offset each side. Reverse: each beat played backwards.
// Times follow the host tempo (folded into 60 ms - 1.8 s); a tempo change glides.
struct SendDelay {
    std::array<DelayLine, 2> line;
    std::array<OnePole, 2> tone, lowCut;
    Smoother time;
    float rate = 48000, maxSamples = 1, targetSamples = 1000, feedback = 0.4f, wowPhase = 0, flutterPhase = 0;
    float window = 1000, nextWindow = 1000, phase = 0;   // Reverse
    int type = DlyTape, quiet = 0, sleepAfter = 1;

    void prepare(float sampleRate, int delayType) {
        rate = sampleRate;
        type = delayType;
        // The reverse delay reads up to two windows back.
        maxSamples = 1.85f * rate * (type == DlyReverse ? 2.0f : 1.0f);
        for (auto& l : line) l.prepare(std::size_t(maxSamples) + 64);
        time.setTime(0.35f, rate);
        float toneHz = 5000, fb = 0.4f;
        switch (type) {
            case DlyTape: toneHz = 3200; fb = 0.42f; break;
            case DlyPingPong: toneHz = 6500; fb = 0.45f; break;
            case DlyDotted: toneHz = 5200; fb = 0.38f; break;
            default: toneHz = 7000; fb = 0.28f; break;
        }
        feedback = fb;
        for (auto& f : tone) f.setCutoff(toneHz, rate);
        for (auto& f : lowCut) f.setCutoff(type == DlyTape ? 160.0f : 110.0f, rate);
        clear();
    }
    void clear() {
        for (auto& l : line) l.clear();
        for (auto& f : tone) f.reset();
        for (auto& f : lowCut) f.reset();
        targetSamples = 0.5f * rate;
        time.snap(targetSamples);
        window = nextWindow = targetSamples;
        phase = 0;
        wowPhase = flutterPhase = 0;
        quiet = sleepAfter = int(8.0f * rate);
    }
    bool asleep() const { return quiet >= sleepAfter; }
    // The delay time for a tempo, folded into range.
    static float secondsFor(int type, float bpm) {
        const float beats = type == DlyTape ? 1.0f : (type == DlyPingPong ? 0.5f : (type == DlyDotted ? 0.75f : 1.0f));
        float s = beats * 60.0f / std::clamp(bpm, 20.0f, 999.0f);
        while (s > 1.8f) s *= 0.5f;
        while (s < 0.06f) s *= 2.0f;
        return s;
    }
    void process(const float* inL, const float* inR, float* outL, float* outR, int n, bool fed, float bpm) {
        quiet = fed ? 0 : std::min(quiet + n, sleepAfter);
        if (asleep()) return;
        targetSamples = std::min(secondsFor(type, bpm) * rate, maxSamples * (type == DlyReverse ? 0.5f : 1.0f) - 8.0f);
        // Repeats die away below -96 dB: sleep after that.
        sleepAfter = int(targetSamples * std::log(1.0e-5f) / std::log(feedback) + targetSamples * 2.0f);
        for (int i = 0; i < n; ++i) {
            const float xl = fed ? inL[i] : 0.0f, xr = fed ? inR[i] : 0.0f;
            float yl = 0, yr = 0;
            switch (type) {
                case DlyTape: {
                    wowPhase += 0.5f / rate;
                    flutterPhase += 6.1f / rate;
                    if (wowPhase >= 1.0f) wowPhase -= 1.0f;
                    if (flutterPhase >= 1.0f) flutterPhase -= 1.0f;
                    const float t = time.next(targetSamples) * (1.0f + 0.0035f * sinCycles(wowPhase) + 0.0009f * sinCycles(flutterPhase));
                    yl = line[0].read(t);
                    yr = line[1].read(t * 1.017f);
                    const float fl = lowCut[0].hp(tone[0].lp(0.8f * yl + 0.2f * yr)), fr = lowCut[1].hp(tone[1].lp(0.8f * yr + 0.2f * yl));
                    line[0].push(softClip(1.2f * xl) * 0.833f + softClip(fl * feedback * 1.3f) * 0.77f);
                    line[1].push(softClip(1.2f * xr) * 0.833f + softClip(fr * feedback * 1.3f) * 0.77f);
                    break;
                }
                case DlyPingPong: {
                    const float t = time.next(targetSamples);
                    yl = line[0].read(t);
                    yr = line[1].read(t);
                    line[0].push(0.5f * (xl + xr) + lowCut[0].hp(tone[0].lp(yr)) * feedback);
                    line[1].push(lowCut[1].hp(tone[1].lp(yl)));
                    break;
                }
                case DlyDotted: {
                    const float t = time.next(targetSamples);
                    yl = line[0].read(t);
                    yr = line[1].read(t + 0.011f * rate);
                    line[0].push(xl + lowCut[0].hp(tone[0].lp(yl)) * feedback);
                    line[1].push(xr + lowCut[1].hp(tone[1].lp(yr)) * feedback);
                    break;
                }
                default: {
                    // Two heads, half a window apart, each sweeping back through the last
                    // window (the delay grows from 0 to two windows), sine-windowed.
                    if (phase == 0.0f) window = nextWindow;
                    phase += 1.0f / window;
                    if (phase >= 1.0f) {
                        phase = 0.0f;
                        nextWindow = targetSamples;
                    }
                    const float p2 = phase + 0.5f >= 1.0f ? phase - 0.5f : phase + 0.5f;
                    const float wa = sinCycles(0.5f * phase), wb = sinCycles(0.5f * p2);
                    yl = line[0].read(1.0f + 2.0f * phase * window) * wa + line[0].read(1.0f + 2.0f * p2 * window) * wb;
                    yr = line[1].read(1.0f + 2.0f * phase * window) * wa + line[1].read(1.0f + 2.0f * p2 * window) * wb;
                    yl *= 0.75f;
                    yr *= 0.75f;
                    line[0].push(xl + lowCut[0].hp(tone[0].lp(yr)) * feedback);
                    line[1].push(xr + lowCut[1].hp(tone[1].lp(yl)) * feedback);
                    break;
                }
            }
            outL[i] += yl;
            outR[i] += yr;
        }
    }
};

//==============================================================================
// An instrument's character effect, tone tilt and width.
struct CharacterFx {
    int type = ChOff, wanted = ChOff;
    float rate = 48000, fade = 0, fadeStep = 0.001f, amount = 0, amountTarget = 0;
    // Saturation
    std::array<OnePole, 2> satTone, dcBlock;
    // Chorus
    std::array<DelayLine, 2> chorus;
    std::array<float, 2> chorusDelay {}, chorusStep {};
    float lfo = 0;
    // Phaser
    std::array<std::array<float, 4>, 2> ap {};
    std::array<float, 4> apCoef {};
    std::array<float, 2> apLast {};
    // Filter
    std::array<Svf, 2> lowpass;
    // Lo-Fi
    std::array<float, 2> hold {};
    std::array<OnePole, 2> lofiTone;
    float holdPhase = 1, holdStep = 1, steps = 4096;
    std::array<OnePole, 2> lofiLowCut;
    float drive = 1, makeUp = 1;
    // Width above 1: a little of the mid, delayed, added to the side (mono-safe).
    std::array<DelayLine, 1> haas;
    // Tilt and width
    std::array<OnePole, 2> tilt;
    float lowGain = 1, highGain = 1, toneNow = 0.5f, width = 1, haasSamples = 500;

    void prepare(float sampleRate) {
        rate = sampleRate;
        for (auto& c : chorus) c.prepare(std::size_t(0.03f * rate));
        for (auto& f : dcBlock) f.setCutoff(15.0f, rate);
        for (auto& f : tilt) f.setCutoff(700.0f, rate);
        haas[0].prepare(std::size_t(0.02f * rate));
        haasSamples = 0.011f * rate;
        fadeStep = 1.0f / (0.02f * rate);
        reset();
    }
    void reset() {
        clearState();
        type = wanted;
        fade = type == ChOff ? 0.0f : 1.0f;
        amount = amountTarget;
        for (auto& f : tilt) f.reset();
        haas[0].clear();
    }
    void clearState() {
        for (auto& f : satTone) f.reset();
        for (auto& f : dcBlock) f.reset();
        for (auto& c : chorus) c.clear();
        chorusDelay = { 0.010f * rate, 0.012f * rate };
        chorusStep.fill(0);
        lfo = 0;
        for (auto& a : ap) a.fill(0);
        apLast.fill(0);
        for (auto& f : lowpass) f.reset();
        hold.fill(0);
        for (auto& f : lofiTone) f.reset();
        for (auto& f : lofiLowCut) f.reset();
        holdPhase = 1;
    }
    // Once per chunk. tone 0..1 (0.5 flat), width 0..2 (1 as played).
    void set(int characterType, float characterAmount, float toneValue, float widthValue) {
        wanted = std::clamp(characterType, 0, CharacterTypeCount - 1);
        amountTarget = clamp01(characterAmount);
        amount += 0.08f * (amountTarget - amount);
        toneNow = clamp01(toneValue);
        const float t = 2.0f * toneNow - 1.0f;
        lowGain = dbToGain(-6.0f * t);
        highGain = dbToGain(6.0f * t);
        width = std::clamp(widthValue, 0.0f, 2.0f);
        const float a = amount;
        switch (type) {
            case ChSaturation:
                for (auto& f : satTone) f.setCutoff(lerpExp(18000.0f, 5500.0f, a), rate);
                drive = 1.0f + 9.0f * a;
                makeUp = std::pow(drive, -0.9f);
                break;
            case ChPhaser: {
                lfo += (0.1f + 0.35f * a) * float(chunk) / rate;
                if (lfo >= 1.0f) lfo -= 1.0f;
                const float centre = lerpExp(220.0f, 2400.0f, 0.5f + 0.5f * sinCycles(lfo));
                for (int k = 0; k < 4; ++k) {
                    const float g = std::tan(pi * std::min(centre * std::pow(1.7f, float(k)), 0.45f * rate) / rate);
                    apCoef[std::size_t(k)] = (g - 1.0f) / (g + 1.0f);
                }
                break;
            }
            case ChChorus: {
                // The sweep moves once per chunk; in between it glides.
                lfo += 0.55f * float(chunk) / rate;
                if (lfo >= 1.0f) lfo -= 1.0f;
                const float depth = (0.0012f + 0.0035f * a) * rate;
                const std::array<float, 2> to { 0.010f * rate + depth * sinCycles(lfo), 0.012f * rate + depth * sinCycles(lfo + 0.25f) };
                for (int c = 0; c < 2; ++c) chorusStep[std::size_t(c)] = (to[std::size_t(c)] - chorusDelay[std::size_t(c)]) / float(chunk);
                break;
            }
            case ChFilter:
                for (auto& f : lowpass) f.set(lerpExp(16000.0f, 280.0f, a), 0.7f + 2.8f * a, rate);
                break;
            case ChLoFi:
                holdStep = std::min(1.0f, lerpExp(0.5f * rate, 3000.0f, a) / rate);
                steps = std::exp2(lerp(13.0f, 4.5f, a));
                for (auto& f : lofiTone) f.setCutoff(lerpExp(15000.0f, 3800.0f, a), rate);
                for (auto& f : lofiLowCut) f.setCutoff(lerpExp(40.0f, 420.0f, a), rate);
                break;
            default: break;
        }
    }
    bool idle() const { return type == ChOff && wanted == ChOff && fade <= 0.0f && std::abs(toneNow - 0.5f) < 1.0e-3f && std::abs(width - 1.0f) < 1.0e-3f; }

    void process(float* l, float* r, int n) {
        for (int i = 0; i < n; ++i) {
            // Fade the old effect out before switching to the new one.
            if (type != wanted) {
                fade = std::max(0.0f, fade - fadeStep);
                if (fade <= 0.0f) {
                    type = wanted;
                    clearState();
                }
            } else if (type != ChOff) {
                fade = std::min(1.0f, fade + fadeStep);
            }
            float xl = l[i], xr = r[i];
            if (type != ChOff && fade > 0.0f) {
                float yl = xl, yr = xr;
                effect(yl, yr);
                xl += fade * (yl - xl);
                xr += fade * (yr - xr);
            }
            // Tone tilt around 700 Hz.
            const float lowL = tilt[0].lp(xl), lowR = tilt[1].lp(xr);
            xl = lowL * lowGain + (xl - lowL) * highGain;
            xr = lowR * lowGain + (xr - lowR) * highGain;
            // Width. Wider than played: the delayed mid joins the side, so even a centred
            // instrument spreads (and the mid, what mono hears, is untouched).
            const float mid = 0.5f * (xl + xr);
            float side = 0.5f * (xl - xr) * width;
            if (width > 1.0f) {
                haas[0].push(mid);
                side += 1.1f * (width - 1.0f) * haas[0].read(haasSamples);
            }
            l[i] = mid + side;
            r[i] = mid - side;
        }
    }

private:
    void effect(float& l, float& r) {
        const float a = amount;
        switch (type) {
            case ChSaturation: {
                // Driven into a soft curve with a little even harmonic, level kept about even.
                const auto sat = [&](float x, int k) {
                    const float d = x * drive;
                    const float y = softClip(d + 0.12f * a * d * d);
                    return satTone[std::size_t(k)].lp(dcBlock[std::size_t(k)].hp(y)) * makeUp;
                };
                l = sat(l, 0);
                r = sat(r, 1);
                break;
            }
            case ChChorus: {
                chorus[0].push(l);
                chorus[1].push(r);
                chorusDelay[0] += chorusStep[0];
                chorusDelay[1] += chorusStep[1];
                const float wl = chorus[0].read(chorusDelay[0]), wr = chorus[1].read(chorusDelay[1]);
                const float wet = 0.35f + 0.5f * a;
                l = l * (1.0f - 0.4f * wet) + wl * 0.75f * wet + wr * 0.2f * wet;
                r = r * (1.0f - 0.4f * wet) + wr * 0.75f * wet + wl * 0.2f * wet;
                break;
            }
            case ChPhaser: {
                const float fb = 0.2f + 0.45f * a;
                const auto phase = [&](float x, int c) {
                    float y = x + fb * apLast[std::size_t(c)];
                    for (int k = 0; k < 4; ++k) {
                        auto& z = ap[std::size_t(c)][std::size_t(k)];
                        const float out = apCoef[std::size_t(k)] * y + z;
                        z = y - apCoef[std::size_t(k)] * out;
                        y = out;
                    }
                    apLast[std::size_t(c)] = softClip(y);
                    return x + (0.5f + 0.5f * a) * (0.5f * (x + y) - x);
                };
                l = phase(l, 0);
                r = phase(r, 1);
                break;
            }
            case ChFilter:
                l = lowpass[0].tick(l).lp;
                r = lowpass[1].tick(r).lp;
                break;
            case ChLoFi: {
                holdPhase += holdStep;
                if (holdPhase >= 1.0f) {
                    holdPhase -= 1.0f;
                    hold[0] = std::round(l * steps) / steps;
                    hold[1] = std::round(r * steps) / steps;
                }
                const float mix = std::min(1.0f, 0.4f + 1.2f * a);
                l = lofiLowCut[0].hp(lofiTone[0].lp(lerp(l, hold[0], mix)));
                r = lofiLowCut[1].hp(lofiTone[1].lp(lerp(r, hold[1], mix)));
                break;
            }
            default: break;
        }
    }
};

//==============================================================================
// What one instrument's effect parameters say (from the host).
struct ElementFxControls {
    float reverb = 0.15f, delay = 0, amount = 0.35f, tone = 0.5f, width = 1;
    int reverbType = RevRoom, delayType = DlyTape, character = ChOff;
};

// Sends of every instrument of one slot: each a smoothed level per shared bus.
struct SendBuses {
    std::array<std::array<float, chunk>, ReverbTypeCount> revL {}, revR {};
    std::array<std::array<float, chunk>, DelayTypeCount> dlyL {}, dlyR {};
    std::array<bool, ReverbTypeCount> revFed {};
    std::array<bool, DelayTypeCount> dlyFed {};
    void clear(int n) {
        for (int b = 0; b < ReverbTypeCount; ++b)
            if (revFed[std::size_t(b)]) {
                std::fill_n(revL[std::size_t(b)].begin(), n, 0.0f);
                std::fill_n(revR[std::size_t(b)].begin(), n, 0.0f);
            }
        for (int b = 0; b < DelayTypeCount; ++b)
            if (dlyFed[std::size_t(b)]) {
                std::fill_n(dlyL[std::size_t(b)].begin(), n, 0.0f);
                std::fill_n(dlyR[std::size_t(b)].begin(), n, 0.0f);
            }
        revFed.fill(false);
        dlyFed.fill(false);
    }
};

// One slot's instruments: their character effects, tilt, width and sends.
class InstrumentFx {
public:
    static constexpr int elements = 6;
    void prepare(float sampleRate) {
        rate = sampleRate;
        for (auto& c : character) c.prepare(rate);
        const float controlRate = rate / float(chunk);
        coef = 1.0f - std::exp(-1.0f / (0.03f * controlRate));
        reset();
    }
    void reset() {
        for (auto& c : character) c.reset();
        for (auto& e : revLevel) e.fill(0);
        for (auto& e : dlyLevel) e.fill(0);
        for (auto& e : revFrom) e.fill(0);
        for (auto& e : dlyFrom) e.fill(0);
        lastScale.fill(1);
        sends.revFed.fill(true);
        sends.dlyFed.fill(true);
        sends.clear(chunk);
        fresh = true;
    }
    // Once per chunk, before the Place renders: the controls, smoothed.
    void begin(const std::array<ElementFxControls, elements>& controls, int n) {
        sends.clear(n);
        for (int e = 0; e < elements; ++e) {
            const auto& c = controls[std::size_t(e)];
            auto& ch = character[std::size_t(e)];
            if (fresh) {
                // Start settled: the type and amount first, then everything from them, so a
                // restarted world sounds exactly like a new one.
                ch.set(c.character, c.amount, c.tone, c.width);
                ch.reset();
            }
            ch.set(c.character, c.amount, c.tone, c.width);
            for (int b = 0; b < ReverbTypeCount; ++b) {
                const float target = b == c.reverbType ? clamp01(c.reverb) : 0.0f;
                auto& v = revLevel[std::size_t(e)][std::size_t(b)];
                revFrom[std::size_t(e)][std::size_t(b)] = v;
                v = fresh ? target : v + coef * (target - v);
                if (v < 1.0e-5f && target == 0.0f) v = 0;
            }
            for (int b = 0; b < DelayTypeCount; ++b) {
                const float target = b == c.delayType ? clamp01(c.delay) : 0.0f;
                auto& v = dlyLevel[std::size_t(e)][std::size_t(b)];
                dlyFrom[std::size_t(e)][std::size_t(b)] = v;
                v = fresh ? target : v + coef * (target - v);
                if (v < 1.0e-5f && target == 0.0f) v = 0;
            }
        }
        if (fresh) {
            revFrom = revLevel;
            dlyFrom = dlyLevel;
        }
        fresh = false;
    }
    // An instrument's bus (already at its level): effects in place, then its sends.
    // sendScale: the Place's own say in how much reaches the buses (its Character,
    // Tension...), on top of the instrument's sends.
    void process(int e, float* l, float* r, int n, float sendScale = 1.0f) {
        if (e < 0 || e >= elements) return;
        auto& ch = character[std::size_t(e)];
        if (!ch.idle()) ch.process(l, r, n);
        const float invN = 1.0f / float(n);
        if (std::abs(sendScale - lastScale[std::size_t(e)]) > 1.0e-6f) {
            // Glide to a new scale over this chunk.
            const float from = lastScale[std::size_t(e)];
            lastScale[std::size_t(e)] = sendScale;
            scaleFrom = from;
        } else {
            scaleFrom = sendScale;
        }
        scaleTo = sendScale;
        for (int b = 0; b < ReverbTypeCount; ++b) {
            const float from = revFrom[std::size_t(e)][std::size_t(b)], to = revLevel[std::size_t(e)][std::size_t(b)];
            if (from <= 0.0f && to <= 0.0f) continue;
            add(sends.revL[std::size_t(b)].data(), sends.revR[std::size_t(b)].data(), sends.revFed[std::size_t(b)], l, r, n, from * scaleFrom, to * scaleTo, invN);
        }
        for (int b = 0; b < DelayTypeCount; ++b) {
            const float from = dlyFrom[std::size_t(e)][std::size_t(b)], to = dlyLevel[std::size_t(e)][std::size_t(b)];
            if (from <= 0.0f && to <= 0.0f) continue;
            add(sends.dlyL[std::size_t(b)].data(), sends.dlyR[std::size_t(b)].data(), sends.dlyFed[std::size_t(b)], l, r, n, from * scaleFrom, to * scaleTo, invN);
        }
    }
    SendBuses sends;

private:
    static void add(float* outL, float* outR, bool& fed, const float* l, const float* r, int n, float from, float to, float invN) {
        if (!fed) {
            std::fill_n(outL, n, 0.0f);
            std::fill_n(outR, n, 0.0f);
            fed = true;
        }
        for (int i = 0; i < n; ++i) {
            const float g = from + (to - from) * float(i + 1) * invN;
            outL[i] += l[i] * g;
            outR[i] += r[i] * g;
        }
    }
    float rate = 48000, coef = 0.02f, scaleFrom = 1, scaleTo = 1;
    std::array<float, elements> lastScale { 1, 1, 1, 1, 1, 1 };
    bool fresh = true;
    std::array<CharacterFx, elements> character;
    std::array<std::array<float, ReverbTypeCount>, elements> revLevel {}, revFrom {};
    std::array<std::array<float, DelayTypeCount>, elements> dlyLevel {}, dlyFrom {};
};

//==============================================================================
// The shared buses: every slot's sends in, the reverbs and delays out.
class SharedFx {
public:
    void prepare(float sampleRate) {
        rate = sampleRate;
        for (int t = 0; t < ReverbTypeCount; ++t) reverb[std::size_t(t)].prepare(rate, t);
        for (int t = 0; t < DelayTypeCount; ++t) delay[std::size_t(t)].prepare(rate, t);
        clear();
    }
    void clear() {
        for (auto& r : reverb) r.clear();
        for (auto& d : delay) d.clear();
        in.revFed.fill(true);
        in.dlyFed.fill(true);
        in.clear(chunk);
    }
    // Starts a chunk: the inputs empty.
    void begin(int n) { in.clear(n); }
    // Adds one slot's sends, scaled per sample (its level, fade and pan).
    void add(const SendBuses& s, const float* gainL, const float* gainR, int n) {
        for (int b = 0; b < ReverbTypeCount; ++b)
            if (s.revFed[std::size_t(b)]) accumulate(in.revL[std::size_t(b)].data(), in.revR[std::size_t(b)].data(), in.revFed[std::size_t(b)],
                                                     s.revL[std::size_t(b)].data(), s.revR[std::size_t(b)].data(), gainL, gainR, n);
        for (int b = 0; b < DelayTypeCount; ++b)
            if (s.dlyFed[std::size_t(b)]) accumulate(in.dlyL[std::size_t(b)].data(), in.dlyR[std::size_t(b)].data(), in.dlyFed[std::size_t(b)],
                                                     s.dlyL[std::size_t(b)].data(), s.dlyR[std::size_t(b)].data(), gainL, gainR, n);
    }
    // Adds every bus's output to outL/outR.
    void process(float* outL, float* outR, int n, float bpm) {
        for (int b = 0; b < ReverbTypeCount; ++b)
            reverb[std::size_t(b)].process(in.revL[std::size_t(b)].data(), in.revR[std::size_t(b)].data(), outL, outR, n, in.revFed[std::size_t(b)]);
        for (int b = 0; b < DelayTypeCount; ++b)
            delay[std::size_t(b)].process(in.dlyL[std::size_t(b)].data(), in.dlyR[std::size_t(b)].data(), outL, outR, n, in.dlyFed[std::size_t(b)], bpm);
    }
    // For the tests and the meters: how many buses are awake.
    int awake() const {
        int count = 0;
        for (const auto& r : reverb) count += r.asleep() ? 0 : 1;
        for (const auto& d : delay) count += d.asleep() ? 0 : 1;
        return count;
    }

private:
    static void accumulate(float* outL, float* outR, bool& fed, const float* l, const float* r, const float* gl, const float* gr, int n) {
        if (!fed) {
            std::fill_n(outL, n, 0.0f);
            std::fill_n(outR, n, 0.0f);
            fed = true;
        }
        for (int i = 0; i < n; ++i) {
            outL[i] += l[i] * gl[i];
            outR[i] += r[i] * gr[i];
        }
    }
    float rate = 48000;
    SendBuses in;
    std::array<SendReverb, ReverbTypeCount> reverb;
    std::array<SendDelay, DelayTypeCount> delay;
};

} // namespace wb::dsp
