#pragma once
// The Planet's sound colour (Milestone 10): a light master treatment that gives the
// whole world the air of the planet it stands on. Each planet type is a shape (tone,
// filtering, movement, chorus, drive, rumble, space and width); Planet Influence
// scales the shape from neutral. A new planet crossfades: the planets' weights glide,
// so every part of the shape moves smoothly. Cheap: a few filters and two short
// modulated delays per side, coefficients once per chunk.
#include "Core.h"
#include <array>

namespace wb::dsp {

enum PlanetType { Rocky, Ocean, IceGiant, GasGiant, DesertWorld, LavaWorld, ForestWorld, PlanetTypeCount };

// What a planet does to the master, at 100 % Influence.
struct PlanetShape {
    float lowDb = 0;        // low shelf (110 Hz): sub weight
    float lowMidDb = 0;     // bell at 260 Hz: body
    float highDb = 0;       // high shelf (4.5 kHz): air
    float cutHz = 20000;    // a low-pass (20 kHz: none)
    float sweep = 0;        // slow movement of that low-pass (octaves)
    float sweepHz = 0.1f;
    float chorus = 0;       // watery chorus (mix)
    float chorusHz = 0.3f;
    float shimmer = 0;      // a glassy, moving copy of the highs
    float drive = 0;        // saturation
    float pressure = 0;     // slow swell of level and weight
    float rumble = 0;       // a low rumble under the world
    float space = 1;        // the World Space's wet level, multiplied
    float width = 1;        // stereo width, multiplied
    float trimDb = 0;       // level trim, so planets stay about as loud as each other
};

// clang-format off
inline constexpr std::array<PlanetShape, PlanetTypeCount> planetShapes { {
    // Rocky: dry, close, grounded, a little low-mid weight.
    { 1.0f, 3.0f, -1.5f, 20000, 0, 0.1f, 0, 0.3f, 0, 0.05f, 0, 0, 0.55f, 0.85f, -1.0f },
    // Ocean: watery chorus and filtering, slow rolling movement.
    { 1.0f, 0, -1.0f, 9000, 1.3f, 0.07f, 0.55f, 0.21f, 0, 0, 0, 0, 1.2f, 1.15f, 0.3f },
    // Ice Giant: cold, bright, crystalline shimmer, airy highs; thin low end.
    { -3.0f, -2.0f, 6.0f, 20000, 0, 0.1f, 0.15f, 0.9f, 0.6f, 0, 0, 0, 1.3f, 1.2f, 0.0f },
    // Gas Giant: vast swirling space, slow pressure modulation, deep sub.
    { 7.0f, -1.0f, -2.0f, 12000, 0.6f, 0.05f, 0.3f, 0.12f, 0, 0, 0.55f, 0.35f, 1.8f, 1.25f, -1.0f },
    // Desert World: dry warmth, a dusty high cut, wide.
    { 0.5f, 1.5f, -2.0f, 6500, 0, 0.1f, 0, 0.3f, 0, 0.18f, 0, 0, 0.75f, 1.4f, 0.0f },
    // Lava World: heat distortion, saturation, a low rumble.
    { 4.5f, 2.0f, -1.0f, 12000, 0.3f, 0.6f, 0.2f, 1.7f, 0, 0.6f, 0.15f, 0.6f, 1.0f, 1.0f, -1.5f },
    // Forest World: soft, damped, organic.
    { 0.5f, 2.0f, -6.0f, 5500, 0.25f, 0.09f, 0.12f, 0.18f, 0, 0.04f, 0, 0, 0.9f, 1.05f, 0.5f },
} };
// clang-format on

inline constexpr std::array<const char*, PlanetTypeCount> planetNames { "Rocky", "Ocean", "Ice Giant", "Gas Giant", "Desert World", "Lava World", "Forest World" };

class PlanetColour {
public:
    void prepare(float sampleRate) {
        rate = sampleRate;
        const float controlRate = rate / float(chunk);
        for (auto& w : weight) w.setTime(0.45f, controlRate);   // a new planet crossfades in about 1.5 s
        influence.setTime(0.15f, controlRate);
        for (auto& c : chorusLine) c.prepare(std::size_t(0.05f * rate));
        for (auto& c : shimmerLine) c.prepare(std::size_t(0.03f * rate));
        for (auto& h : highSplit) h.setCutoff(4500.0f, rate);
        for (auto& h : shimmerSplit) h.setCutoff(2800.0f, rate);
        for (auto& h : chorusSplit) h.setCutoff(180.0f, rate);
        rumbleFilter.setCutoff(55.0f, rate);
        rumbleFilter2.setCutoff(70.0f, rate);
        noise.setKey(0x50a7e7u, 7);
        reset(0, 0.35f);
    }

    void reset(int planet, float amount) {
        for (std::size_t i = 0; i < weight.size(); ++i) weight[i].snap(int(i) == std::clamp(planet, 0, PlanetTypeCount - 1) ? 1.0f : 0.0f);
        influence.snap(clamp01(amount));
        for (auto& s : low) s.reset();
        for (auto& s : mid) s.reset();
        for (auto& s : cut) s.reset();
        for (auto& h : highSplit) h.reset();
        for (auto& h : shimmerSplit) h.reset();
        for (auto& h : chorusSplit) h.reset();
        for (auto& c : chorusLine) c.clear();
        for (auto& c : shimmerLine) c.clear();
        rumbleFilter.reset();
        rumbleFilter2.reset();
        update(planet, amount, 0);
    }

    // Once per chunk: glide toward `planet` at `amount` (0..1) and set the filters.
    // `seconds` is the world's clock (for the slow movements).
    void update(int planet, float amount, double seconds) {
        planet = std::clamp(planet, 0, PlanetTypeCount - 1);
        PlanetShape s {};
        s.sweepHz = s.chorusHz = 0;   // summed below (the weights add up to 1)
        float logCut = 0;
        const float k = settle(influence, clamp01(finiteOr(amount, 0.35f)));
        for (int i = 0; i < PlanetTypeCount; ++i) {
            const float w = settle(weight[std::size_t(i)], i == planet ? 1.0f : 0.0f);
            if (w < 1.0e-5f) continue;
            const auto& p = planetShapes[std::size_t(i)];
            s.lowDb += w * p.lowDb;
            s.lowMidDb += w * p.lowMidDb;
            s.highDb += w * p.highDb;
            logCut += w * std::log2(p.cutHz / 20000.0f);
            s.sweep += w * p.sweep;
            s.sweepHz += w * p.sweepHz;
            s.chorus += w * p.chorus;
            s.chorusHz += w * p.chorusHz;
            s.shimmer += w * p.shimmer;
            s.drive += w * p.drive;
            s.pressure += w * p.pressure;
            s.rumble += w * p.rumble;
            s.space += w * (p.space - 1.0f);
            s.width += w * (p.width - 1.0f);
            s.trimDb += w * p.trimDb;
        }
        shape = s;
        amountNow = k;

        lowGain = dbToGain(k * s.lowDb) - 1.0f;
        midGain = dbToGain(k * s.lowMidDb) - 1.0f;
        highGain = dbToGain(k * s.highDb) - 1.0f;
        trim = dbToGain(k * s.trimDb);
        // The low-pass: how far below 20 kHz it sits (octaves) sets how much of it is heard.
        const double t = seconds;
        const float roll = float(std::sin(twoPi * double(s.sweepHz) * t)) * 0.5f + 0.5f;
        const float octaves = k * (-logCut + s.sweep * roll);
        cutMix = clamp01(octaves);
        const float hz = 20000.0f * std::exp2(-std::max(octaves, 0.0f));
        for (auto& f : cut) f.set(hz, 0.6f, rate);
        for (auto& f : low) f.set(110.0f, 0.7f, rate);
        for (auto& f : mid) f.set(260.0f, 0.8f, rate);
        chorusMix = k * s.chorus;
        chorusStep = s.chorusHz / rate;
        shimmerMix = k * s.shimmer;
        drive = k * s.drive;
        // Pressure: a slow swell (about 14 s) of level and weight.
        pressureGain = 1.0f - k * s.pressure * 0.22f * (0.5f + 0.5f * float(std::sin(twoPi * 0.07 * t)));
        rumbleLevel = k * s.rumble * 0.035f * (0.6f + 0.4f * float(std::sin(twoPi * 0.11 * t + 1.3)));
        spaceGain = 1.0f + k * (s.space - 1.0f);
        widthGain = 1.0f + k * (s.width - 1.0f);
    }

    // The World Space's wet level and the stereo width, multiplied (for the engine).
    float space() const { return spaceGain; }
    float width() const { return widthGain; }
    float amount() const { return amountNow; }
    const PlanetShape& current() const { return shape; }

    void process(float& l, float& r, uint64_t position) {
        float x[2] { l, r };
        for (int c = 0; c < 2; ++c) {
            float v = x[c];
            v += lowGain * low[std::size_t(c)].tick(v).lp;
            v += midGain * mid[std::size_t(c)].bandpass(v);
            v += highGain * highSplit[std::size_t(c)].hp(v);
            v = lerp(v, cut[std::size_t(c)].tick(v).lp, cutMix);
            x[c] = v;
        }
        // Watery chorus: two short delays swaying in opposite directions, above the bass
        // (the low end stays solid).
        chorusPhase += chorusStep;
        if (chorusPhase >= 1.0f) chorusPhase -= 1.0f;
        if (chorusMix > 1.0e-4f || shimmerMix > 1.0e-4f) {
            const float m = std::sin(twoPi * chorusPhase);
            chorusLine[0].push(chorusSplit[0].hp(x[0]));
            chorusLine[1].push(chorusSplit[1].hp(x[1]));
            const float wl = chorusLine[0].read((0.016f + 0.004f * m) * rate), wr = chorusLine[1].read((0.019f - 0.004f * m) * rate);
            // The shimmer: the highs through faster, shorter swaying delays.
            const float hl = shimmerSplit[0].hp(x[0]), hr = shimmerSplit[1].hp(x[1]);
            shimmerLine[0].push(hl);
            shimmerLine[1].push(hr);
            const float s = std::sin(twoPi * chorusPhase * 7.0f);
            const float sl = shimmerLine[0].read((0.007f + 0.002f * s) * rate), sr = shimmerLine[1].read((0.009f - 0.002f * s) * rate);
            x[0] = x[0] * (1.0f - 0.35f * chorusMix) + (0.55f * wl + 0.2f * wr) * chorusMix + 0.6f * sr * shimmerMix;
            x[1] = x[1] * (1.0f - 0.35f * chorusMix) + (0.55f * wr + 0.2f * wl) * chorusMix + 0.6f * sl * shimmerMix;
        }
        if (drive > 1.0e-4f) {
            const float k = 1.0f + 5.0f * drive, norm = 1.0f / std::tanh(k * 0.25f) * 0.25f;
            for (float& v : x) v = lerp(v, std::tanh(k * v) * norm, std::min(1.0f, drive * 1.4f));
        }
        float rumble = 0;
        if (rumbleLevel > 1.0e-6f) rumble = rumbleFilter2.lp(rumbleFilter.lp(noise.white(position))) * rumbleLevel * 6.0f;
        const float g = trim * pressureGain;
        l = x[0] * g + rumble;
        r = x[1] * g + rumble * 0.85f;
    }

private:
    // A glide that lands exactly (so a settled colour is exactly repeatable).
    static float settle(Smoother& s, float target) {
        s.next(target);
        if (std::abs(s.value - target) < 1.0e-5f) s.snap(target);
        return s.value;
    }

    float rate = 48000;
    std::array<Smoother, PlanetTypeCount> weight;
    Smoother influence;
    PlanetShape shape {};
    float amountNow = 0;
    std::array<Svf, 2> low, mid, cut;
    std::array<OnePole, 2> highSplit, shimmerSplit, chorusSplit;
    std::array<DelayLine, 2> chorusLine, shimmerLine;
    OnePole rumbleFilter, rumbleFilter2;
    Stream noise;
    float lowGain = 0, midGain = 0, highGain = 0, cutMix = 0, chorusMix = 0, chorusStep = 0, chorusPhase = 0, shimmerMix = 0;
    float drive = 0, pressureGain = 1, rumbleLevel = 0, spaceGain = 1, widthGain = 1, trim = 1;
};

} // namespace wb::dsp
