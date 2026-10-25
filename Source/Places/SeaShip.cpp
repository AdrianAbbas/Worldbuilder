#include "Places.h"

namespace wb {

using namespace dsp;

SeaShipPlace::SeaShipPlace() : EnsemblePlace(2300) { setParts({ &creaks, &concertina, &ropes, &bell, &waves, &gulls }); }

void SeaShipPlace::prepareFx() {
    roll.prepare(rate);
    sea.prepare(rate);
}

void SeaShipPlace::resetFx() {
    roll.clear();
    sea.clear();
    for (auto& f : tilt) f.reset();
}

void SeaShipPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2301);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Creaks) {
        // The hull creaking with the swell (row 0) and wood knocking (row 1). Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        float chanceOf = 0;
        switch (in.character) {
            case CalmSeas: chanceOf = 0.12f + 0.25f * act; break;
            case Storm: chanceOf = 0.3f + 0.45f * act; break;
            default: chanceOf = 0.2f + 0.35f * act; break;
        }
        for (int beat = 0; beat < 16; ++beat) {
            const int at = beat * 4 + (ch(1, beat) < 0.4f ? 2 : 0);
            // Creaks gather where the ship rolls over: the second half of each bar.
            const float lean = (beat % 4 >= 2) ? 1.3f : 0.7f;
            if (ch(2, beat) < chanceOf * lean * lerp(0.7f, 1.3f, density) || beat == 2) w.note(at, 0, 1, (0.5f + 0.3f * ch(3, beat)) * human(4, beat));
            if (ch(5, beat) < chanceOf * 0.5f * busy) w.note(beat * 4 + 1, 1, 1, 0.5f * human(6, beat));
        }
        for (int bar = 0; bar < 4; ++bar)
            if (ch(7, bar) < tension * 0.8f) w.note(bar * 16 + 14, 0, 1, 0.6f + 0.2f * tension);
        return;
    }

    if (element == Concertina) {
        // Concertina chords on the shared chord: a sailors' oom-pah on calm seas, long
        // slow chords on the ghost ship, short gasps in the storm. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -5;
        const auto chord = [&](int step, int length, float velocity) {
            w.note(step, 0, length, velocity);
            w.note(step, 2, length, velocity * 0.85f);
            w.note(step, 4, length, velocity * 0.85f);
        };
        switch (in.character) {
            case CalmSeas:
                for (int bar = 0; bar < 4; ++bar) {
                    w.note(bar * 16, -size, 4, 0.7f * human(10, bar));   // the bass button
                    for (int k = 1; k < 4; ++k)
                        if (ch(11, bar * 4 + k) < (k == 2 ? 0.95f : (0.35f + 0.5f * density) * busy)) chord(bar * 16 + 4 * k, 3, 0.55f * human(12, bar * 4 + k));
                }
                break;
            case Storm:
                for (int bar = 0; bar < 4; ++bar)
                    if (ch(13, bar) < 0.4f + 0.5f * busy || bar == 0) chord(bar * 16 + (ch(14, bar) < 0.5f ? 0 : 6), 4, 0.6f * human(15, bar));
                break;
            default:   // Ghost Ship: a slow, swaying lament
                chord(0, 28, 0.55f);
                if (ch(16, 0) < 0.4f + 0.5f * act) chord(32, 28, 0.5f);
                break;
        }
        return;
    }

    if (element == RopeSwells) {
        // Ropes pulling taut (row 0) and straining (row 1). Eight bars.
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = false;
        const auto chancy = [&](int step, int row, int steps, float probability) { w.note(step, row, steps, 0.8f, 0, probability); };
        switch (in.character) {
            case CalmSeas:
                chancy(16, 0, 24, 0.35f + 0.5f * act);
                chancy(80, 0, 24, 0.25f + 0.5f * act);
                break;
            case Storm:
                for (int k = 0; k < 4; ++k) chancy(k * 32, k % 2 == 0 ? 1 : 0, 24, 0.5f + 0.45f * act);
                break;
            default:
                chancy(32, 1, 32, 0.3f + 0.5f * act);
                chancy(96, 0, 24, 0.3f + 0.4f * act);
                break;
        }
        if (tension > 0.5f) chancy(64, 1, 24, tension);
        return;
    }

    if (element == ShipBell) {
        // The ship's bell, struck in pairs as it tells the time; the storm swings it wildly,
        // the ghost ship tolls it alone. Eight bars.
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = false;
        out.base = 12;
        switch (in.character) {
            case CalmSeas:
                for (int bar = 0; bar < 8; bar += 4) {
                    w.note(bar * 16, 0, 2, 0.7f);
                    w.note(bar * 16 + 2, 0, 2, 0.6f);
                    if (ch(20, bar) < 0.3f + 0.5f * act) {
                        w.note(bar * 16 + 8, 0, 2, 0.65f);
                        w.note(bar * 16 + 10, 0, 2, 0.55f);
                    }
                }
                break;
            case Storm:   // clanging as the ship pitches
                for (int beat = 0; beat < 32; ++beat)
                    if (ch(21, beat) < (0.15f + 0.35f * act) * lerp(0.7f, 1.3f, density) || beat == 0)
                        w.note(beat * 4 + (ch(22, beat) < 0.5f ? 0 : 2), ch(23, beat) < 0.7f ? 0 : fifth, 2, (0.5f + 0.3f * ch(24, beat)) * human(25, beat));
                break;
            default:   // Ghost Ship: a slow toll
                for (int bar = 0; bar < 8; bar += 2) w.note(bar * 16, bar % 4 == 0 ? 0 : -size, 16, 0.65f);
                if (ch(26, 0) < act) w.note(88, fifth, 8, 0.45f);
                break;
        }
        return;
    }

    if (element == Waves) {
        // The sea: a low drone on the root and fifth, held all four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = -12;
        w.note(0, 0, 64, 0.85f);
        w.note(0, fifth, 64, 0.6f);
        if (in.character == Storm) w.note(0, size, 64, 0.6f);
        return;
    }

    if (element == Gulls) {
        // Gulls calling overhead: falling pairs and short laughs, high in the key.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 24;
        float chanceOf = 0;
        switch (in.character) {
            case CalmSeas: chanceOf = 0.25f + 0.45f * act; break;
            case Storm: chanceOf = 0.08f + 0.2f * act; break;
            default: chanceOf = 0.05f + 0.15f * act; break;
        }
        for (int beat = 0; beat < 16; ++beat) {
            if (ch(30, beat) >= chanceOf * lerp(0.6f, 1.4f, life) && beat != 5) continue;
            const int at = beat * 4 + int(ch(31, beat) * 2.0f), top = 2 + int(ch(32, beat) * 3.0f);
            const int calls = 2 + int(ch(33, beat) * (1.0f + 2.0f * busy));
            for (int k = 0; k < calls && at + k * 2 < 64; ++k) w.note(at + k * 2, top - (k % 2), 2, (k == 0 ? 0.65f : 0.5f) * human(34, at + k));
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void SeaShipPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool calm = s.character == CalmSeas, storm = s.character == Storm, ghostShip = s.character == GhostShip;

    // Creaks: old timber groaning (a tuned squeak that bends up into its note) and knocks.
    {
        auto& p = creaks.patch;
        auto& creak = p.rows[0];
        creak = {};
        creak.semis = 12.0f + float(music::degree(frame.mood, 4));
        creak.bend = 0.94f;
        creak.decay = storm ? 0.25f : 0.4f;
        creak.tone = 0.25f;
        creak.noise = 0.35f;
        creak.noiseHz = lerpExp(700.0f, 1600.0f, tone);
        creak.noiseQ = 7.0f;
        creak.noiseDecay = 0.25f;
        creak.ratio = 2.0f;
        creak.partial = 0.2f;
        creak.pan = -0.3f;
        auto& knock = p.rows[1];
        knock = {};
        knock.semis = float(music::degree(frame.mood, 4)) - 12.0f;
        knock.bend = 1.05f;
        knock.decay = 0.12f;
        knock.tone = 0.6f;
        knock.noise = 0.18f;
        knock.noiseHz = lerpExp(600.0f, 1500.0f, tone);
        knock.noiseQ = 1.2f;
        knock.noiseDecay = 0.03f;
        knock.ratio = 2.755f;
        knock.partial = 0.3f;
        knock.pan = 0.35f;
        p.spread = 0.5f;
        p.voices = 4;
        p.gain = 1.0f;
    }
    // The concertina: two reeds a little apart, through a bellows' soft filter.
    {
        auto& p = concertina.patch;
        p.held = true;
        p.legato = false;
        p.attack = ghostShip ? 0.6f : 0.06f;
        p.decay = 0.4f;
        p.sustain = 0.8f;
        p.release = ghostShip ? 1.0f : 0.15f;
        p.detune = ghostShip ? 16.0f : 9.0f;
        p.sub = 0.0f;
        p.cutoff = lerpExp(700.0f, 3000.0f, tone) * (ghostShip ? 0.6f : 1.0f);
        p.envOctaves = 0.3f;
        p.q = 0.9f;
        p.width = 0.5f;
        p.vibratoHz = 5.5f;
        p.vibratoDepth = ghostShip ? 0.12f : 0.04f;
        p.tremoloPerBeat = 2.0f;
        p.tremoloDepth = (ghostShip ? 0.25f : 0.08f) + 0.35f * tension;   // the bellows shake
        p.spread = 0.3f;
        p.voices = byQuality(q, 4, 6, 7);
        // Free reeds in pairs (musette), the reed chamber's formant, the bellows' air.
        p.model = dsp::SynthReed;
        p.formantHz = 1700.0f;
        p.formantLevel = 0.5f;
        p.breath = 0.04f;
        p.gain = 1.0f;
    }
    // Ropes: noise swelling and straining in bands tuned to the key.
    ropes.patch.rows[0] = { SweepSwell, 0.0f, 36.0f, 4.0f, 0.9f, false, true };
    ropes.patch.rows[1] = { SweepBuild, -12.0f, 30.0f, 6.0f, 1.0f, false, true };
    ropes.patch.tension = tension;
    ropes.patch.voices = 2;
    ropes.patch.gain = 1.0f;
    // The ship's bell: bright brass, its minor partial a quarter-tone off any note; the
    // ghost ship's is cracked and dull.
    {
        auto& p = bell.patch;
        p.set({ 0.5f, 1.0f, 2.0f, 2.449f, 3.0f, 4.12f }, { 1.4f, 1.0f, 0.6f, 0.4f, 0.45f, 0.2f },
              { 0.35f, 1.0f, 0.5f, ghostShip ? 0.35f : 0.18f, 0.25f * (0.4f + tone), 0.12f * tone });
        p.decay = ghostShip ? 5.0f : 3.0f;
        p.hardness = (ghostShip ? 0.3f : 0.6f) + 0.3f * tone;
        p.toneHz = lerpExp(2500.0f, 10000.0f, tone) * (ghostShip ? 0.6f : 1.0f);
        p.spread = 0.3f;
        p.pan = 0.2f;
        p.voices = byQuality(q, 3, 4, 5);
        p.gain = 1.0f;
        p.extras.beatHz = 0.9f;
    }
    // Waves: low tuned breath that heaves with the swell.
    const float swell = 0.5f + 0.5f * std::sin(float(t) * twoPi * (storm ? 0.16f : 0.09f) + 1.3f * drift.smooth(t, 0.05));
    {
        auto& p = waves.patch;
        p.tone = 0.0f;
        p.breath = 1.0f;
        // The open sea heard through the hull: a deep swell an octave below the shore's
        // surf, rising and falling slowly.
        p.breathQ = lerp(3.0f, 5.0f, tone);
        p.transpose = -12.0f;
        p.attack = 0.9f;
        p.release = 3.0f;
        p.scoop = 0.0f;
        p.vibratoDepth = 0.0f;
        p.spread = 0.9f;
        p.voices = 3;
        p.held = true;
        // (The narrower band carries less noise: level kept.)
        p.gain = (0.6f + 0.7f * swell * swell) * 1.6f;
    }
    // Gulls: bright whistles that swoop up and fall away.
    {
        auto& p = gulls.patch;
        p.h2 = 0.25f;
        p.h3 = 0.1f;
        p.breath = 0.15f;
        p.attack = 0.01f;
        p.release = 0.08f;
        p.scoop = 5.0f;
        p.scoopSeconds = 0.05f;
        p.vibratoHz = 9.0f;
        p.vibratoDepth = 0.2f;
        p.vibratoDelay = 0.06f;
        p.minSeconds = 0.08f;
        p.spread = 0.9f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Tension: the timbers groan, the ropes strain, the sea rises and the bell rings out;
    // Life brings the gulls; Machinery works the ropes and timbers like rigging.
    // The ghost ship drifts in fog that rings; the storm's wind tears echoes away.
    sendScale.fill(ghostShip ? 1.6f : (storm ? 0.7f : 1.0f));
    level = { 0.6f * (storm ? 1.3f : (ghostShip ? 1.2f : 0.8f)) * (0.6f + 0.9f * tension) * (0.5f + 1.5f * s.machinery), 0.13f * (storm ? 0.7f : (ghostShip ? 0.9f : 1.0f)),
              0.45f * (storm ? 1.3f : 1.0f) * (0.6f + 0.8f * tension) * (0.5f + 1.2f * s.machinery), 0.13f * (storm ? 1.1f : 1.0f) * (0.8f + 0.6f * tension),
              0.9f * (storm ? 1.4f : (calm ? 0.8f : 1.0f)) * (0.5f + 1.2f * tension), 0.08f * lerp(0.5f, 1.5f, s.life) };

    // Rolling modulation: the ship sways with the sea; the storm throws it about.
    roll.set(storm ? 0.17f : (ghostShip ? 0.07f : 0.1f), (storm ? 0.9f : (ghostShip ? 0.7f : 0.45f)) * (0.6f + 0.6f * s.fxDepth) + 0.4f * tension);
    // A wide, wet space: the open sea, or the fog around the ghost ship.
    sea.set(lerp(0.7f, 1.0f, s.size), lerp(2.5f, 5.0f, s.size) * (ghostShip ? 1.4f : 1.0f) * (1.0f + 0.3f * s.magic), lerpExp(2000.0f, 7000.0f, tone));
    seaSend = (ghostShip ? 0.7f : 0.5f) + 0.2f * s.fxDepth;
    // Heavy weather darkens the sky.
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era) * (ghostShip ? 0.75f : 1.0f) * (1.0f - 0.4f * tension), rate);
}

bool SeaShipPlace::shape(const PlaceContext&, const Scheduled& x, NoteOn& on) {
    // Gulls wheel overhead; the creaks come from all along the hull.
    if (x.element == Gulls || x.element == Creaks) {
        on.hasPan = true;
        on.pan = 0.8f * (2.0f * on.random - 1.0f);
    }
    return true;
}

void SeaShipPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        // The ship and everything on it rolls; the gulls overhead do not.
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e)
            if (e != Gulls) {
                l += busL[std::size_t(e)][k];
                r += busR[std::size_t(e)][k];
            }
        roll.process(l, r);
        l += busL[Gulls][k];
        r += busR[Gulls][k];
        float wl = 0, wr = 0;
        sea.process(l * seaSend, r * seaSend, wl, wr);
        // Wide: the tail spreads further out than the ship.
        l += 0.75f * (wl - 0.25f * wr);
        r += 0.75f * (wr - 0.25f * wl);
        left[i] = tilt[2].lp(tilt[0].lp(l));
        right[i] = tilt[3].lp(tilt[1].lp(r));
    }
}

} // namespace wb
