#include "Places.h"

namespace wb {

using namespace dsp;

NebulaPlace::NebulaPlace() : EnsemblePlace(3200) { setParts({ &pads, &wind, &bells, &drone, &dust, &flares }); }

void NebulaPlace::prepareFx() {
    infinite.prepare(rate);
    space.prepare(rate);
}

void NebulaPlace::resetFx() {
    infinite.clear();
    space.clear();
    for (auto& f : windSweep) f.reset();
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
}

void NebulaPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 3202);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Pads) {
        // The pads: wide chords, the top voice moving every two bars. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 0;
        w.note(0, 0, 64, 0.7f);
        w.note(0, 4, 64, 0.55f);
        if (in.character != Void) {
            w.note(0, size + 2, 32, 0.5f);
            w.note(32, size + (ch(1, 0) < 0.5f ? 1 : 4), 32, 0.45f);
        }
        if (tension > 0.55f) w.note(16, 1, 32, 0.3f + 0.3f * tension);
        return;
    }

    if (element == Wind) {
        // The cosmic wind: tuned breath on the root and fifth, very high, slowly alternating.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 24;
        w.note(0, 0, 40, 0.7f);
        w.note(40, fifth, 24, 0.6f);
        return;
    }

    if (element == StarBells) {
        // Star pulses: a slow bell every bar or two, on the chord. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 12;
        const float chanceOf = in.character == Void ? 0.15f + 0.4f * act : (in.character == StellarNursery ? 0.5f + 0.5f * act : 0.3f + 0.5f * act);
        for (int at = 0; at < 64; at += 8) {
            if (at > 0 && ch(10, at) >= chanceOf * (at % 16 == 0 ? 1.0f : 0.6f) * lerp(0.7f, 1.3f, density)) continue;
            const float pick = ch(11, at);
            w.note(at, pick < 0.4f ? 0 : (pick < 0.7f ? 4 : (pick < 0.9f ? 2 : size)), 8, 0.65f * human(12, at));
        }
        return;
    }

    if (element == SubDrone) {
        // The deep-space hum: the root far below, held all four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = -24;
        w.note(0, 0, 64, 0.85f);
        if (in.character != Void) w.note(0, fifth, 64, 0.35f);
        return;
    }

    if (element == Stardust) {
        // Stardust: tiny high grains on the chord, in scattered clusters. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = 24;
        const float chanceOf = in.character == StellarNursery ? 0.3f + 0.6f * act : (in.character == Void ? 0.03f + 0.2f * act : 0.1f + 0.45f * act);
        for (int step = 0; step < 32; ++step) {
            if (step != 0 && ch(20, step) >= chanceOf * lerp(0.7f, 1.3f, density)) continue;
            const float pick = ch(21, step);
            const int d = pick < 0.3f ? 0 : (pick < 0.55f ? 2 : (pick < 0.8f ? 4 : size));
            w.note(step, d, 1, (0.4f + 0.3f * ch(22, step)) * human(23, step));
        }
        return;
    }

    if (element == Flares) {
        // Solar flares: a slow swell (row 0) and a flare igniting (row 1). Eight bars.
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = false;
        switch (in.character) {
            case Drift:
                w.note(16, 0, 32, 0.7f);
                w.note(80, 0, 32, 0.6f, 0, 0.2f + 0.5f * act);
                break;
            case StellarNursery:
                w.note(0, 1, 24, 0.8f);
                w.note(40, 0, 24, 0.7f, 0, 0.5f + 0.5f * act);
                w.note(80, 1, 24, 0.75f, 0, 0.4f + 0.5f * act);
                break;
            default:   // Void: a far flare, rarely
                w.note(32, 0, 48, 0.6f, 0, 0.3f + 0.6f * act);
                break;
        }
        if (tension > 0.6f) w.note(112, 1, 16, 0.5f + 0.4f * tension);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void NebulaPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const bool drift_ = s.character == Drift, nursery = s.character == StellarNursery, empty = s.character == Void;
    const double t = double(c.clock) / rate;

    // The pads: many detuned saws, their filter drifting slowly open and shut.
    {
        auto& p = pads.patch;
        p.held = true;
        p.attack = 3.5f;
        p.release = 4.0f;
        p.decay = 1.0f;
        p.sustain = 1.0f;
        p.detune = empty ? 30.0f : 16.0f;
        p.sub = 0.15f;
        const float sweep = 0.5f + 0.5f * drift.smooth(t, 0.05);
        p.cutoff = lerpExp(300.0f, 2600.0f, tone) * lerp(0.6f, 1.5f, sweep) * (nursery ? 1.3f : (empty ? 0.6f : 1.0f));
        p.envOctaves = 0.0f;
        p.q = 1.0f + 1.5f * tension;
        p.width = 1.0f;
        p.glide = 2.0f;
        p.vibratoHz = 0.15f;
        p.vibratoDepth = 0.06f;
        p.spread = 0.9f;
        p.voices = byQuality(q, 3, 4, 5);
        p.gain = 1.0f;
    }
    // The cosmic wind: breath alone, very wide.
    {
        auto& p = wind.patch;
        p.held = true;
        p.tone = 0.0f;
        p.breath = 1.0f;
        p.breathQ = empty ? 7.0f : 4.0f;
        p.attack = 2.5f;
        p.release = 3.0f;
        p.scoop = 0.0f;
        p.vibratoHz = 0.1f;
        p.vibratoDepth = 0.8f;
        p.vibratoDelay = 0.0f;
        p.spread = 1.0f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Star pulses: soft, slow bells with a slow beat, the highs blooming after.
    {
        auto& p = bells.patch;
        p.set({ 1.0f, 2.0f, 3.0f, 4.2f, 5.4f }, { 1.0f, 0.7f, 0.5f, 0.35f, 0.25f }, { 1.0f, 0.5f, 0.3f * (0.4f + tone), 0.2f * tone, 0.12f * tone });
        p.decay = empty ? 6.0f : 4.0f;
        p.hardness = 0.1f;
        p.velocityHardness = 0.2f;
        p.toneHz = lerpExp(2500.0f, 10000.0f, tone);
        p.extras = { 0.6f, 0.0f, 0.4f, 0.4f, 0.2f };
        p.spread = 0.9f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.0f;
    }
    // The deep-space hum: a bowed resonance far below.
    {
        auto& p = drone.patch;
        p.set({ 1.0f, 2.0f, 3.0f, 4.0f }, { 1.0f, 0.7f, 0.5f, 0.3f }, { 1.0f, 0.35f, 0.15f * (0.5f + tone), 0.05f });
        p.ring = 1.5f;
        p.rosin = 0.3f + 0.3f * tension;
        p.noisiness = 0.1f;
        p.attack = 3.0f;
        p.release = 3.0f;
        p.glide = 1.0f;
        p.vibratoHz = 0.1f;
        p.vibratoDepth = 0.03f;
        p.spread = 0.4f;
        p.voices = 2;
        p.held = true;
        p.gain = 1.0f;
    }
    // Stardust: tiny glassy grains.
    {
        auto& p = dust.patch;
        p.decay = nursery ? 0.12f : 0.2f;
        p.rise = 0.0f;
        p.below = 0.0f;
        p.bloop = 0.1f;
        p.overtone = 0.2f + 0.3f * tone;
        p.spread = 1.0f;
        p.voices = byQuality(q, 4, 6, 8);
        p.gain = 1.0f;
    }
    // The wind's resonance sweeps slowly through four octaves, each side its own way: a
    // whooshing, phasing gale (not a surf or a hiss).
    for (int k = 0; k < 2; ++k)
        windSweep[std::size_t(k)].set(lerpExp(250.0f, 4500.0f, clamp01(0.5f + 0.6f * drift.smooth(t + 40.0 * k, 0.12))), 4.0f, rate);
    // Solar flares: a slow noise swell and a build that ignites.
    flares.patch.rows[0] = { SweepSwell, 0.0f, 36.0f, 3.0f, 1.0f, false, true };
    flares.patch.rows[1] = { SweepBuild, -12.0f, 48.0f, 2.5f, 1.0f, false, true };
    flares.patch.tension = tension;
    flares.patch.voices = 2;
    flares.patch.gain = 1.0f;

    sendScale.fill(empty ? 1.4f : 1.0f);
    level = { 0.09f * (empty ? 0.7f : 1.0f), 1.0f * (empty ? 1.4f : 1.0f), 0.1f * lerp(0.8f, 1.3f, s.magic), 0.25f * (0.8f + 0.5f * tension),
              0.13f * (nursery ? 1.2f : 1.0f) * lerp(0.8f, 1.3f, s.life), 0.6f * (0.7f + 0.6f * tension) * (drift_ ? 0.9f : 1.0f) };

    // An endless shimmer: a long tail that rises an octave and keeps rising.
    infinite.set(1.0f, lerp(8.0f, 9.0f, s.size) * (empty ? 1.3f : 1.0f) * (1.0f + 0.2f * s.magic), (nursery ? 0.8f : 0.6f) + 0.3f * s.fxDepth);
    infiniteSend = 0.35f + 0.25f * s.fxDepth;
    space.set(lerp(0.6f, 1.0f, s.size), lerp(3.5f, 4.5f, s.size), lerpExp(2000.0f, 8000.0f, tone));
    spaceSend_ = 0.3f + 0.2f * s.fxDepth;
    for (auto& f : tilt) f.setCutoff(lerpExp(800.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era), rate);
    presence = 1.4f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
    dry = lerp(1.15f, 0.6f, s.size);
    // Far away the whole cloud fades, its tail too (the World Space adds the distance).
    wet = lerp(1.1f, 0.8f, s.size);
}

void NebulaPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0.3f * busL[Wind][k] + 1.6f * windSweep[0].bandpass(busL[Wind][k]), r = 0.3f * busR[Wind][k] + 1.6f * windSweep[1].bandpass(busR[Wind][k]);
        for (int e : { Pads, StarBells, SubDrone, Stardust, Flares }) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        float hl = 0, hr = 0, wl = 0, wr = 0;
        // The bells, stardust and pads rise into the shimmer.
        const float fl = busL[StarBells][k] + busL[Stardust][k] + busL[Pads][k], fr = busR[StarBells][k] + busR[Stardust][k] + busR[Pads][k];
        infinite.process(fl * infiniteSend, fr * infiniteSend, hl, hr);
        space.process(l * spaceSend_, r * spaceSend_, wl, wr);
        l = l * dry + wet * (0.6f * wl + 0.7f * hl);
        r = r * dry + wet * (0.6f * wr + 0.7f * hr);
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
