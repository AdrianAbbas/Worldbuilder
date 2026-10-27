#include "Places.h"

namespace wb {

using namespace dsp;

BiodomePlace::BiodomePlace() : EnsemblePlace(2200) { setParts({ &pads, &mallets, &birds, &waterRec, &vents, &shaker }); }

void BiodomePlace::prepareFx() {
    dome.prepare(rate);
    chorus.prepare(rate);
}

void BiodomePlace::resetFx() {
    dome.clear();
    chorus.clear();
    for (auto& f : tilt) f.reset();
}

void BiodomePlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2201);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Pads) {
        // Lush pads on the chord: a full voicing that changes every two bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -12;
        w.note(0, 0, 64, 0.8f);
        w.note(0, 4, 64, 0.7f);
        w.note(0, size + 2, 32, 0.65f);
        w.note(32, ch(1, 0) < 0.5f ? size + 1 : size + 3, 32, 0.6f);
        if (in.character == Thriving) w.note(0, size + 4, 64, 0.5f);
        return;
    }

    if (element == Mallets) {
        // Bright mallet arpeggios on the chord, in sixteenths when the dome thrives,
        // slow eighths at night, broken when failing. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = 12;
        static constexpr int up[8] { 0, 2, 4, 7, 9, 7, 4, 2 };
        float keep = 0;
        int every = 1;
        switch (in.character) {
            case Thriving: keep = 0.45f + 0.5f * act; every = 1; break;
            case NightCycle: keep = 0.35f + 0.45f * act; every = 2; break;
            default: keep = 0.2f + 0.35f * act; every = 2; break;
        }
        for (int step = 0; step < 32; step += every) {
            const bool strong = step % 8 == 0;
            if (!strong && ch(10, step) >= keep * lerp(0.7f, 1.3f, density)) continue;
            const int d = up[(step / every) % 8];
            w.note(step, d >= 7 ? d - 7 + size : d, every, (strong ? 0.7f : 0.5f) * human(11, step));
        }
        return;
    }

    if (element == Birdsong) {
        // Synthetic birdsong: short calls of two to five notes high up, from all around.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 24;
        float chanceOf = 0;
        switch (in.character) {
            case Thriving: chanceOf = 0.35f + 0.5f * act; break;
            case NightCycle: chanceOf = 0.12f + 0.25f * act; break;
            default: chanceOf = 0.1f + 0.2f * act; break;
        }
        for (int beat = 0; beat < 16; ++beat) {
            if (ch(20, beat) >= chanceOf * lerp(0.6f, 1.4f, life) && beat != 2) continue;
            const int at = beat * 4 + int(ch(21, beat) * 2.0f), notes = 2 + int(ch(22, beat) * (1.0f + 2.0f * busy));
            const int top = (in.character == NightCycle ? 0 : 2) + int(ch(23, beat) * 4.0f);
            for (int k = 0; k < notes && at + k < 64; ++k) w.note(at + k, top + (k % 2 == 0 ? 0 : (ch(24, beat) < 0.5f ? 2 : -1)), 1, (0.55f + 0.1f * float(k % 2)) * human(25, at + k));
        }
        return;
    }

    if (element == WaterPlucks) {
        // Water dripping through the irrigation: plucks in the key, now and then a run.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 12;
        float chanceOf = 0;
        switch (in.character) {
            case Thriving: chanceOf = 0.1f + 0.2f * act; break;
            case NightCycle: chanceOf = 0.08f + 0.15f * act; break;
            default: chanceOf = 0.04f + 0.1f * act; break;
        }
        for (int step = 0; step < 64; ++step) {
            if (ch(30, step) >= chanceOf * lerp(0.7f, 1.3f, density)) continue;
            w.note(step, int(ch(31, step) * float(size + 3)), 2, (0.45f + 0.35f * ch(32, step)) * human(33, step));
        }
        if (out.count == 0) w.note(4, 0, 2, 0.5f);
        return;
    }

    if (element == Vents) {
        // Ventilation: a steady hum on the root and fifth, all four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = -12;
        w.note(0, 0, 64, 0.8f);
        w.note(0, fifth, 64, in.character == Failing ? 0.7f : 0.45f);
        return;
    }

    if (element == Shaker) {
        // A gentle shaker (row 0) and ticks (row 1): the dome's soft clockwork; at night, crickets.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        switch (in.character) {
            case Thriving:
                for (int step = 0; step < 32; step += 2) {
                    if (ch(40, step) >= (step % 4 == 0 ? 0.95f : (0.4f + 0.5f * density) * busy)) continue;
                    w.note(step, 0, 1, (step % 8 == 4 ? 0.7f : 0.45f) * human(41, step));
                }
                for (int step = 6; step < 32; step += 8)
                    if (ch(42, step) < 0.3f + 0.5f * busy) w.note(step, 1, 1, 0.5f);
                break;
            case NightCycle:   // crickets: quick tick pairs
                for (int step = 0; step < 32; step += 4)
                    if (ch(43, step) < 0.35f + 0.55f * busy || step == 0) {
                        w.note(step, 1, 1, 0.55f * human(44, step));
                        w.note(step + 1, 1, 1, 0.4f);
                    }
                break;
            default:   // Failing: an irregular shake, missing beats
                for (int step = 0; step < 32; step += 4)
                    if (ch(45, step) < 0.25f + 0.5f * busy || step == 0) w.note(step + (ch(46, step) < 0.3f ? 2 : 0), 0, 1, 0.5f * human(47, step));
                break;
        }
        if (tension > 0.5f)
            for (int step = 2; step < 32; step += 8) w.note(step, 1, 1, 0.4f + 0.3f * tension);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void BiodomePlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool thriving = s.character == Thriving, night = s.character == NightCycle, failing = s.character == Failing;
    // A failing dome flickers: the power sags now and then.
    const float sag = failing ? clamp01(0.6f + 0.8f * drift.smooth(t, 0.3)) : 1.0f;

    // Lush pads: wide detuned saws through a soft filter; failing, they waver and sag. With
    // Scoop / Glide on, a note bows on from the last one and glides (else it moves at once).
    {
        auto& p = pads.patch;
        p.held = p.legato = true;
        p.attack = 1.5f;
        p.release = 2.5f;
        p.glide = 1.0f;
        p.detune = failing ? 20.0f : 12.0f;
        p.sub = 0.1f;
        p.cutoff = lerpExp(500.0f, 3500.0f, tone) * (night ? 0.55f : 1.0f) * (0.6f + 0.4f * sag) * std::exp2(0.3f * drift.smooth(t + 9.0, 0.07));
        p.envOctaves = 0.3f;
        p.q = 0.6f;
        p.width = 1.0f;
        p.vibratoHz = failing ? 0.6f : 4.5f;
        p.vibratoDepth = failing ? 0.15f + 0.2f * tension : 0.05f;
        p.vibratoDelay = 0.5f;
        p.spread = 0.7f;
        p.voices = byQuality(q, 4, 5, 6);
        p.gain = 0.6f + 0.4f * sag;
    }
    // Mallets: a bright marimba-like bar, its third partial a quarter-tone off any note.
    {
        auto& p = mallets.patch;
        // Synthetic glass mallets: a hollow series no real bar has (1, 3, 6, 9: the fifth
        // and its octaves, so it stays in key) with a fast digital shimmer.
        p.set({ 1.0f, 3.0f, 6.0f, 9.0f }, { 1.0f, 0.5f, 0.3f, 0.2f }, { 1.0f, 0.3f * (0.4f + tone), 0.15f * tone, 0.07f * tone });
        p.extras.beatHz = 6.0f;
        p.decay = night ? 1.2f : 0.7f;
        p.hardness = 0.45f + 0.4f * tone;
        p.toneHz = lerpExp(3000.0f, 12000.0f, tone);
        p.spread = 0.7f;
        p.voices = byQuality(q, 4, 5, 6);
        p.gain = 1.0f;
    }
    // Synthetic birdsong: pure whistles that swoop and trill.
    {
        auto& p = birds.patch;
        p.h2 = 0.0f;
        p.h3 = 0.0f;
        p.breath = 0.04f;
        p.attack = 0.006f;
        p.release = night ? 0.12f : 0.04f;
        p.scoop = night ? 1.0f : 4.0f;
        p.scoopSeconds = 0.02f;
        p.vibratoHz = failing ? 6.0f : 22.0f;
        p.vibratoDepth = 0.35f;
        p.vibratoDelay = 0.02f;
        p.minSeconds = 0.05f;
        p.spread = 0.9f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Water plucks: drops that rise into their note.
    {
        auto& p = water.patch;
        p.decay = night ? 0.35f : 0.25f;
        p.rise = 4.0f;
        p.below = 3.0f;
        p.bloop = 0.45f;
        p.overtone = 0.2f * (0.3f + tone);
        p.spread = 0.8f;
        p.voices = byQuality(q, 4, 5, 6);
        p.gain = 1.0f;
    }
    // Water plucks from the recorded kalimba: its click softened, darker and short, ringing
    // a little longer in the night cycle. With Scoop / Glide on, each tine slides up into
    // its note like a drop (0.6 semitone in about 10 ms, quick so the notes stay in the key).
    {
        auto& p = waterRec.patch;
        p.instrument = "kalimba";
        p.decay = night ? 1.6f : (failing ? 0.7f : 1.0f);
        p.scoopSemis = -0.6f;
        p.scoopSeconds = 0.01f;
        p.attack = 0.004f;
        p.toneHz = lerpExp(2500.0f, 9000.0f, tone);
        p.velocityTone = 0.8f;
        p.velocityGain = 0.5f;
        p.even = 0.8f;
        p.release = 0.2f;
        p.lowestKey = 53;
        p.highestKey = 95;
        p.spread = 0.8f;
        p.voices = byQuality(q, 4, 5, 6);
        p.gain = 7.5f;
    }
    // Vents: a hum with air in it; failing, it sputters with the power.
    {
        auto& p = vents.patch;
        p.tone = 0.35f;
        p.h2 = 0.2f;
        p.h3 = 0.1f;
        p.breath = 0.8f;
        p.breathQ = lerp(2.0f, 5.0f, tone);
        p.attack = 1.5f;
        p.release = 1.5f;
        p.scoop = 0.0f;
        p.vibratoDepth = 0.0f;
        p.spread = 0.6f;
        p.voices = 2;
        p.held = true;
        p.gain = failing ? 0.4f + 0.8f * (1.0f - sag) : 1.0f;
    }
    // The shaker: seeds in a gourd, and small ticks.
    {
        auto& p = shaker.patch;
        auto& shake = p.rows[0];
        shake = {};
        shake.tuned = false;
        shake.hz = 100;
        shake.tone = 0.0f;
        shake.noise = 0.9f;
        shake.noiseHz = lerpExp(3000.0f, 7000.0f, tone);
        shake.noiseQ = 0.9f;
        shake.noiseDecay = 0.1f;
        shake.pan = 0.3f;
        auto& tick = p.rows[1];
        tick = {};
        tick.semis = 36.0f + float(music::degree(frame.mood, 4));
        tick.bend = 1.0f;
        tick.decay = 0.03f;
        tick.tone = 0.3f;
        tick.noise = 0.45f;
        tick.noiseHz = lerpExp(2500.0f, 6000.0f, tone);
        tick.noiseQ = 2.0f;
        tick.noiseDecay = 0.03f;
        tick.ratio = 2.0f;
        tick.partial = 0.1f;
        tick.pan = -0.35f;
        p.spread = 0.3f;
        p.voices = 4;
        p.gain = 1.0f;
    }
    // Tension: the systems strain (vents and shaker up) while the birds go quiet; Life
    // fills the dome with birds; Machinery runs the vents harder.
    const float hush = 1.0f - 0.6f * tension;
    // At night the dome rings long; failing, its air is dead and close.
    sendScale.fill(night ? 1.5f : (failing ? 0.5f : 1.0f));
    level = { 0.07f * (thriving ? 1.2f : (night ? 0.8f : 1.0f)), 0.12f * (night ? 0.7f : 1.0f), 0.1f * lerp(0.5f, 1.5f, s.life) * hush * (night ? 0.7f : 1.0f),
              0.2f, 0.18f * (failing ? 1.5f : (night ? 0.7f : 1.0f)) * (0.6f + 1.0f * tension) * (0.7f + 0.8f * s.machinery),
              1.0f * (night ? 0.8f : 1.0f) * (0.6f + 1.0f * tension) };

    // A bright, airy dome and a gentle chorus on the pads and mallets.
    dome.set(lerp(0.6f, 1.0f, s.size), lerp(1.8f, 4.0f, s.size) * (night ? 1.3f : 1.0f) * (1.0f + 0.3f * s.magic), lerpExp(4000.0f, 12000.0f, tone));
    domeSend = (night ? 0.55f : 0.4f) + 0.2f * s.fxDepth;
    chorus.set(0.3f + 0.2f * s.fxDepth, failing ? 0.004f : 0.0025f, 0.35f + 0.25f * s.fxDepth);
    // Strain darkens the dome's light.
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era) * (night ? 0.7f : 1.0f) * (1.0f - 0.4f * tension), rate);
}

bool BiodomePlace::shape(const PlaceContext&, const Scheduled& x, NoteOn& on) {
    // Birds and drops all around the dome.
    if (x.element == Birdsong || x.element == WaterPlucks) {
        on.hasPan = true;
        on.pan = 0.85f * (2.0f * on.random - 1.0f);
    }
    return true;
}

void BiodomePlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float sl = busL[Pads][k] + busL[Mallets][k], sr = busR[Pads][k] + busR[Mallets][k];
        chorus.process(sl, sr);
        float l = sl + busL[Birdsong][k] + busL[WaterPlucks][k] + busL[Vents][k] + busL[Shaker][k];
        float r = sr + busR[Birdsong][k] + busR[WaterPlucks][k] + busR[Vents][k] + busR[Shaker][k];
        float wl = 0, wr = 0;
        dome.process(l * domeSend, r * domeSend, wl, wr);
        l += 0.7f * wl;
        r += 0.7f * wr;
        left[i] = tilt[2].lp(tilt[0].lp(l));
        right[i] = tilt[3].lp(tilt[1].lp(r));
    }
}

} // namespace wb
