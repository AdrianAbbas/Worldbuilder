#include "Places.h"

namespace wb {

using namespace dsp;

CavernPlace::CavernPlace() : EnsemblePlace(1900) { setParts({ &drips, &drone, &taps, &bass, &crystals, &rumble }); }

void CavernPlace::prepareFx() {
    cave.prepare(rate);
    slap.prepare(rate);
}

void CavernPlace::resetFx() {
    cave.clear();
    slap.clear();
    for (auto& f : murk) f.reset();
    for (auto& f : tilt) f.reset();
}

void CavernPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 1901);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Drips) {
        // Water dripping in the key: bright and high in the crystal cave, slow in the mine,
        // everywhere when flooded. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        float chanceOf = 0;
        int low = 0, span = size;
        switch (in.character) {
            case CrystalCave: chanceOf = 0.06f + 0.2f * act; low = 2 * size; span = size + 3; break;
            case DeepMine: chanceOf = 0.03f + 0.12f * act; low = size; span = size; break;
            default: chanceOf = 0.12f + 0.3f * act; low = size; span = 2 * size; break;
        }
        for (int step = 0; step < 64; ++step) {
            if (ch(1, step) >= chanceOf * lerp(0.7f, 1.3f, density)) continue;
            const int d = low + int(ch(2, step) * float(span));
            w.note(step, d, 2, (0.45f + 0.4f * ch(3, step)) * human(4, step));
            // Now and then a drop falls twice, a little lower.
            if (step + 3 < 64 && ch(5, step) < 0.25f) w.note(step + 3, d - 1, 2, 0.35f);
        }
        if (out.count == 0) w.note(8, low, 2, 0.5f);
        return;
    }

    if (element == SubDrone) {
        // The sub drone: the key's root two octaves down (and in the crystal cave its fifth).
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        w.note(0, -2 * size, 64, 0.95f);
        if (in.character == CrystalCave) w.note(0, fifth - 2 * size, 64, 0.4f);
        return;
    }

    if (element == Taps) {
        // Metal taps on the chord tones, an octave and more above; the slapback answers.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 12;
        switch (in.character) {
            case CrystalCave:   // a distant hammer, now and then
                for (int bar = 0; bar < 4; bar += 2)
                    if (ch(10, bar) < 0.3f + 0.5f * act || bar == 0) {
                        w.note(bar * 16 + 4, 2 * int(ch(11, bar) * 3.0f), 2, 0.55f);
                        if (ch(12, bar) < act) w.note(bar * 16 + 8, 0, 2, 0.45f);
                    }
                break;
            case DeepMine:   // the pick-axe: a strike on 1 and 3, a glancing second blow
                for (int bar = 0; bar < 4; ++bar)
                    for (int half = 0; half < 2; ++half) {
                        const int at = bar * 16 + half * 8;
                        if (ch(13, at) >= (half == 0 ? 0.9f : 0.4f + 0.5f * busy)) continue;
                        w.note(at, half == 0 ? 0 : 4, 2, 0.85f * human(14, at));
                        if (ch(15, at) < density * busy) w.note(at + 3, half == 0 ? 0 : 4, 1, 0.4f);
                    }
                break;
            default:   // Flooded: dull taps, rarely
                for (int bar = 0; bar < 4; ++bar)
                    if (ch(16, bar) < 0.2f + 0.4f * act || bar == 1) w.note(bar * 16 + 4 * int(ch(17, bar) * 4.0f), 2 * int(ch(18, bar) * 3.0f), 2, 0.5f);
                break;
        }
        return;
    }

    if (element == BowedBass) {
        // A bowed low bass on the chord: slow notes, two octaves down. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -24;
        switch (in.character) {
            case CrystalCave:
                w.note(0, 0, 28, 0.7f);
                if (ch(20, 0) < 0.4f + 0.5f * act) w.note(32, 4, 28, 0.6f);
                break;
            case DeepMine:   // heavy, falling to the fifth below and back
                w.note(0, 0, 32, 0.85f);
                w.note(32, fifth - size, 16, 0.75f);
                if (ch(21, 0) < 0.4f + 0.5f * act) w.note(48, 0, 16, 0.7f);
                break;
            default:   // Flooded: long slow swells
                w.note(0, 0, 32, 0.7f);
                w.note(32, ch(22, 0) < 0.5f ? 2 : 4, 32, 0.6f);
                break;
        }
        if (tension > 0.6f) w.note(16, 0, 16, 0.5f, 1);   // a grinding semitone
        return;
    }

    if (element == Crystals) {
        // Crystal resonances high on the chord: a ringing arpeggio now and then.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 24;
        float chanceOf = 0;
        switch (in.character) {
            case CrystalCave: chanceOf = 0.45f + 0.5f * act; break;
            case DeepMine: chanceOf = 0.1f + 0.3f * act; break;
            default: chanceOf = 0.25f + 0.4f * act; break;
        }
        for (int bar = 0; bar < 4; ++bar) {
            if (ch(30, bar) >= chanceOf * lerp(0.7f, 1.3f, life) && bar != 0) continue;
            const int at = bar * 16 + 2 * int(ch(31, bar) * 4.0f), notes = 2 + int(ch(32, bar) * 3.0f);
            const bool down = ch(33, bar) < 0.4f;
            for (int k = 0; k < notes; ++k) {
                const int d = down ? 2 * (notes - 1 - k) : 2 * k;
                w.note(at + k, d > 4 ? d - 6 + size : d, 8, (0.5f + 0.1f * float(k)) * human(34, at + k));
            }
        }
        return;
    }

    if (element == Rumble) {
        // A distant rumble (row 0) and a low thud (row 1). Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        switch (in.character) {
            case CrystalCave:
                if (ch(40, 0) < 0.4f + 0.5f * act) w.note(32, 0, 16, 0.6f);
                if (ch(41, 0) < act * density) w.note(8, 1, 1, 0.5f);
                break;
            case DeepMine:   // a distant pulse: rumble every other bar, a thud on the one
                for (int bar = 0; bar < 4; bar += 2) w.note(bar * 16, 0, 16, 0.85f);
                for (int bar = 0; bar < 4; ++bar)
                    if (ch(42, bar) < 0.5f + 0.45f * busy) w.note(bar * 16 + 8, 1, 1, 0.7f * human(43, bar));
                break;
            default:   // the weight of the water: a slow rumble every bar
                for (int bar = 0; bar < 4; ++bar)
                    if (ch(44, bar) < 0.5f + 0.5f * act || bar == 0) w.note(bar * 16, 0, 16, 0.6f);
                break;
        }
        if (ch(45, 0) < tension) w.note(56, 1, 1, 0.8f);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void CavernPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool crystal = s.character == CrystalCave, mine = s.character == DeepMine, flooded = s.character == Flooded;

    // Drips: plinks that rise through their note.
    {
        auto& p = drips.patch;
        p.decay = flooded ? 0.3f : 0.22f;
        p.rise = 5.0f;
        p.below = 4.0f;
        p.bloop = 0.5f;
        p.overtone = 0.25f * (0.3f + tone);
        p.spread = 0.9f;
        p.voices = byQuality(q, 4, 6, 8);
        p.gain = 1.0f;
    }
    // The sub drone: almost a sine, very low; in the mine it pulses slowly.
    {
        auto& p = drone.patch;
        p.attack = 3.0f;
        p.release = 3.0f;
        p.held = p.legato = true;
        p.glide = 1.0f;
        p.detune = 3.0f;
        p.sub = 0.9f;
        p.cutoff = lerpExp(50.0f, 160.0f, tone) * std::exp2(0.3f * drift.smooth(t, 0.05));
        p.envOctaves = 0.0f;
        p.q = 0.8f;
        p.width = 0.3f;
        p.tremoloPerBeat = mine ? 0.25f : 0.0f;
        p.tremoloDepth = mine ? 0.35f + 0.3f * tension : 0.0f;
        p.spread = 0.1f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Metal taps: small hard strikes, their upper partials a quarter-tone off any note.
    {
        auto& p = taps.patch;
        p.set({ 1.0f, 2.755f, 4.899f, 6.0f }, { 1.0f, 0.6f, 0.4f, 0.3f }, { 1.0f, 0.6f, 0.4f * tone, 0.3f * tone });
        p.decay = mine ? 1.0f : 0.6f;
        p.hardness = 0.85f;
        p.toneHz = lerpExp(2500.0f, 10000.0f, tone) * (flooded ? 0.4f : 1.0f);
        p.spread = 0.6f;
        p.voices = 4;
        p.gain = 1.0f;
    }
    // The bowed bass: harmonic modes under a slow bow.
    {
        auto& p = bass.patch;
        // Double basses, two players, slow heavy bows.
        p.model = BowedString;
        p.size = dsp::DoubleBass;
        p.players = 2;
        p.spreadCents = 6.0f;
        p.position = 0.15f;
        p.brightness = lerp(0.2f, 0.55f, tone);
        p.noisiness = 0.25f + 0.2f * tension;
        p.attack = flooded ? 1.6f : 0.8f;
        p.release = 1.5f;
        p.glide = 0.2f;
        p.vibratoDepth = 0.1f;
        p.spread = 0.3f;
        p.voices = 2;
        p.held = true;
        p.gain = 1.0f;
    }
    // Crystals: glassy resonances, their overtones a quarter-tone off any note.
    {
        auto& p = crystals.patch;
        p.set({ 1.0f, 2.32f, 4.899f, 6.0f }, { 1.0f, 0.7f, 0.5f, 0.4f }, { 1.0f, 0.35f, 0.3f * tone, 0.15f });
        p.decay = crystal ? 5.0f : 3.0f;
        p.hardness = 0.55f + 0.3f * tone;
        p.toneHz = lerpExp(4000.0f, 14000.0f, tone) * (flooded ? 0.35f : 1.0f);
        p.spread = 0.8f;
        p.voices = byQuality(q, 4, 6, 8);
        p.gain = 1.0f;
    }
    // Rumble: a long low roll of noise over the root, and a distant thud.
    {
        auto& p = rumble.patch;
        auto& roll = p.rows[0];
        roll = {};
        roll.semis = -24.0f;
        roll.bend = 1.0f;
        roll.decay = 2.5f;
        roll.tone = 0.5f;
        roll.noise = 0.6f;
        roll.noiseHz = 90.0f;
        roll.noiseQ = 0.6f;
        roll.noiseDecay = 2.0f;
        roll.ratio = 1.5f;
        roll.partial = 0.2f;
        auto& thud = p.rows[1];
        thud = {};
        thud.semis = -12.0f;
        thud.bend = 1.4f;
        thud.decay = 0.5f;
        thud.tone = 0.8f;
        thud.noise = 0.2f;
        thud.noiseHz = 200.0f;
        thud.noiseQ = 0.7f;
        thud.noiseDecay = 0.2f;
        thud.ratio = 1.5f;
        thud.partial = 0.3f;
        thud.pan = -0.2f;
        p.spread = 0.4f;
        p.voices = 3;
        p.gain = 1.0f;
    }
    // Tension: the rumble and the drone press in; Machinery: the mine works harder.
    // The crystal cave rings; the deep mine is close, dry rock and timber; flooded, the
    // water carries every sound far.
    sendScale.fill(crystal ? 1.4f : (mine ? 0.7f : 1.2f));
    level = { 0.22f * lerp(0.8f, 1.2f, s.life), 0.05f * (0.7f + 0.6f * tension), 0.3f * (1.0f + 1.5f * s.machinery), crystal ? 0.3f : 0.2f, 0.12f,
              0.5f * (0.5f + 1.5f * tension) * (1.0f + 1.0f * s.machinery) };

    // A long dark reverb and a slapback off the far wall; the flood muffles everything.
    cave.set(lerp(0.75f, 1.0f, s.size), lerp(3.5f, 7.0f, s.size) * (1.0f + 0.3f * s.magic) * (crystal ? 1.2f : 1.0f), lerpExp(900.0f, 3200.0f, tone));
    caveSend = 0.45f + 0.2f * s.fxDepth;
    slap.set(lerp(0.09f, 0.16f, s.size), 0.2f + 0.15f * s.fxDepth, lerpExp(1500.0f, 4500.0f, tone));
    slapSend = (mine ? 0.6f : 0.4f) * (0.6f + 0.4f * s.fxDepth);
    murkiness = flooded ? 0.85f : 0.0f;
    for (auto& f : murk) f.setCutoff(lerpExp(500.0f, 1800.0f, tone), rate);
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era), rate);
}

bool CavernPlace::shape(const PlaceContext&, const Scheduled& x, NoteOn& on) {
    // Drips and crystals sit all around the cave.
    if (x.element == Drips || x.element == Crystals) {
        on.hasPan = true;
        on.pan = 0.85f * (2.0f * on.random - 1.0f);
    }
    return true;
}

void CavernPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        // Taps, drips and crystals answer from the far wall.
        float sl = 0, sr = 0, wl = 0, wr = 0;
        slap.process((busL[Taps][k] + busL[Drips][k] + busL[Crystals][k]) * slapSend, (busR[Taps][k] + busR[Drips][k] + busR[Crystals][k]) * slapSend, sl, sr);
        l += sl;
        r += sr;
        cave.process(l * caveSend, r * caveSend, wl, wr);
        l += 0.75f * wl;
        r += 0.75f * wr;
        // Under water: a deep low-pass blended in.
        if (murkiness > 0.0f) {
            l = lerp(l, murk[0].lp(l), murkiness);
            r = lerp(r, murk[1].lp(r), murkiness);
        }
        left[i] = tilt[2].lp(tilt[0].lp(l));
        right[i] = tilt[3].lp(tilt[1].lp(r));
    }
}

} // namespace wb
