#include "Places.h"

namespace wb {

using namespace dsp;

RuinsPlace::RuinsPlace() : EnsemblePlace(2000) { setParts({ &shards, &box, &bells, &wind, &ghost, &toms }); }

void RuinsPlace::prepareFx() {
    lofi.prepare(rate, 2001);
    reverse.prepare(rate, 0.9f);
    hall.prepare(rate);
}

void RuinsPlace::resetFx() {
    lofi.clear();
    reverse.clear();
    hall.clear();
    for (auto& f : tilt) f.reset();
}

void RuinsPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2001);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == PianoShards) {
        // Fragments of a piano piece, heard backwards: two to four chord tones that swell
        // in and stop. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        float chanceOf = 0;
        int step = 4;
        switch (in.character) {
            case Overgrown: chanceOf = 0.2f + 0.8f * act; step = 4; out.base = 0; break;
            case Haunted: chanceOf = 0.5f + 0.5f * act; step = 6; out.base = -5; break;
            default: chanceOf = 0.15f + 0.35f * act; step = 8; out.base = 7; break;
        }
        for (int bar = 0; bar < 4; ++bar) {
            if (ch(1, bar) >= chanceOf * lerp(0.7f, 1.3f, density) && bar != 1) continue;
            const int at = bar * 16 + 2 * int(ch(2, bar) * 3.0f), notes = 2 + int(ch(3, bar) * (1.0f + 2.0f * busy));
            const bool falling = ch(4, bar) < 0.6f;
            for (int k = 0; k < notes && at + k * step < 64; ++k) {
                const int d = falling ? 2 * (notes - 1 - k) : 2 * k;
                w.note(at + k * step, d > 4 ? d - 6 + size : d, step, (0.5f + 0.1f * float(k)) * human(5, at + k));
            }
        }
        if (tension > 0.55f) w.note(56, size + 1, 8, 0.45f);   // a stray, uneasy note
        return;
    }

    if (element == MusicBox) {
        // A music box winding down: a little tune high up, in eighths; haunted, it slows and
        // breaks off; desolate, a few notes are all that is left. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 24;
        static constexpr int tune[16] { 4, 2, 0, 2, 4, 4, 4, -1, 2, 2, 2, -1, 4, 7, 7, -1 };
        int every = 2;
        float keep = 1;
        switch (in.character) {
            case Overgrown: every = 2; keep = 0.3f + 0.7f * act; break;
            case Haunted: every = 4; keep = 0.45f + 0.35f * act; break;
            default: every = 4; keep = 0.15f + 0.3f * act; break;
        }
        for (int k = 0; k < 64 / every; ++k) {
            const int d = tune[k % 16];
            if (d < 0 || (ch(10, k) >= keep * lerp(0.8f, 1.2f, density) && k % 8 != 0)) continue;
            const int degree = d == 7 ? size : d;
            w.note(k * every, degree, every, (k % 4 == 0 ? 0.65f : 0.5f) * human(11, k));
        }
        if (out.count == 0) w.note(0, 0, 4, 0.5f);
        return;
    }

    if (element == BrokenBells) {
        // Cracked bells on the root and the fifth, tolling slowly; a broken peal when tense.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 0;
        float chanceOf = 0;
        switch (in.character) {
            case Overgrown: chanceOf = 0.2f + 0.3f * act; break;
            case Haunted: chanceOf = 0.35f + 0.4f * act; break;
            default: chanceOf = 0.4f + 0.5f * act; break;
        }
        for (int bar = 0; bar < 4; ++bar)
            for (int half = 0; half < 2; ++half) {
                const int at = bar * 16 + half * 8;
                if (ch(20, at) >= chanceOf * (half == 0 ? 1.0f : 0.5f * busy) && at != 0) continue;
                w.note(at, half == 0 ? 0 : fifth, 8, (half == 0 ? 0.7f : 0.5f) * human(21, at));
            }
        if (ch(22, 0) < tension) {
            for (int k = 0; k < 3; ++k) w.note(52 + 2 * k, size - 2 * k, 2, 0.45f);
        }
        return;
    }

    if (element == Wind) {
        // Cold wind through the stones: high harmonics of the key, held all four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 12;
        w.note(0, size, 64, 0.8f);
        w.note(0, size + fifth, 64, in.character == Desolate ? 0.7f : 0.5f);
        if (in.character != Overgrown || act > 0.6f) w.note(0, 2 * size, 64, 0.45f);
        return;
    }

    if (element == GhostPad) {
        // A ghostly pad on the chord, two bars a note.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -12;
        w.note(0, 0, 64, 0.8f);
        w.note(0, 4, 32, 0.65f);
        w.note(32, ch(30, 0) < 0.5f ? 2 : 4, 32, 0.65f);
        if (in.character == Haunted) w.note(0, size + 2, 64, 0.5f);
        return;
    }

    if (element == LowToms) {
        // Sparse low toms (row 0) and a floor tom (row 1), like a far procession. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        switch (in.character) {
            case Overgrown:   // a soft heartbeat now and then
                for (int bar = 0; bar < 4; ++bar)
                    if (ch(40, bar) < 0.2f + 0.8f * act || bar == 0) {
                        w.note(bar * 16, 1, 1, 0.6f * human(41, bar));
                        if (ch(42, bar) < busy * density) w.note(bar * 16 + 3, 0, 1, 0.4f);
                        if (ch(48, bar) < act * act) w.note(bar * 16 + 8, 1, 1, 0.45f);
                    }
                break;
            case Haunted:   // a slow, uneven march
                for (int bar = 0; bar < 4; ++bar) {
                    w.note(bar * 16, 1, 1, 0.7f * human(43, bar));
                    if (ch(44, bar) < 0.3f + 0.6f * busy) w.note(bar * 16 + 10, 0, 1, 0.5f * human(45, bar));
                }
                break;
            default:   // Desolate: a lone floor tom, far away
                for (int bar = 0; bar < 4; bar += 2) w.note(bar * 16, 1, 1, 0.65f);
                if (ch(46, 0) < act) w.note(40, 0, 1, 0.45f);
                break;
        }
        // Tension: the procession comes closer.
        for (int bar = 0; bar < 4; ++bar)
            if (ch(47, bar) < tension * 0.8f) w.note(bar * 16 + 12, 0, 1, 0.55f + 0.2f * tension);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void RuinsPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool overgrown = s.character == Overgrown, haunted = s.character == Haunted, desolate = s.character == Desolate;

    // Piano shards: an FM piano whose notes swell in and stop, as if played backwards.
    {
        auto& p = shards.patch;
        p.ratio = 1.0f;
        p.index = 0.8f + 1.2f * tone;
        p.indexFloor = 0.6f + 0.6f * tone;
        p.indexSeconds = 1.0f;
        p.feedback = 0.04f;
        p.attack = haunted ? 0.9f : 0.6f;
        p.decay = 0.09f;
        p.sustain = 0.0f;
        p.release = 0.08f;
        p.held = false;
        p.vibratoHz = 0.7f;
        p.vibratoDepth = 0.06f;   // a warped, slightly seasick swell
        p.spread = 0.6f;
        p.voices = byQuality(q, 4, 5, 6);
        p.gain = 1.0f;
    }
    // Music box: small hard tines, the upper partial a quarter-tone off any note.
    {
        auto& p = box.patch;
        // A worn music box: steel comb teeth (a tine's modes, 1 : 6.27 : 17.6), bent and
        // rusted so they beat and strike unevenly, the spring running down (each note sags).
        p.set({ 1.0f, 6.27f, 17.55f }, { 1.0f, 0.2f, 0.07f }, { 1.0f, 0.35f * (0.4f + tone), 0.1f * tone });
        p.extras.beatHz = 4.0f;
        p.extras.variation = 0.6f;
        p.extras.glideSemis = -0.15f;
        p.extras.glideSeconds = 1.5f;
        p.decay = overgrown ? 1.8f : 2.4f;
        p.hardness = 0.75f + 0.2f * tone;
        p.toneHz = lerpExp(3000.0f, 11000.0f, tone);
        p.spread = 0.35f;
        p.pan = haunted ? -0.3f : 0.15f;
        p.voices = byQuality(q, 3, 4, 5);
        p.gain = 1.0f;
    }
    // Broken bells: a cracked bell, its partials short and a quarter-tone off.
    {
        auto& p = bells.patch;
        p.set({ 0.5f, 1.0f, 1.225f, 2.0f, 2.749f, 4.12f }, { 1.2f, 1.0f, 0.3f, 0.6f, 0.2f, 0.15f },
              { 0.3f, 1.0f, 0.25f, 0.45f, 0.2f * tone, 0.12f * tone });
        p.decay = desolate ? 4.0f : 3.0f;
        p.hardness = 0.45f + 0.3f * tone;
        p.toneHz = lerpExp(2000.0f, 8000.0f, tone);
        p.spread = 0.5f;
        p.voices = byQuality(q, 3, 4, 5);
        p.gain = 1.0f;
        // Cracked: the partials beat fast and strike unevenly.
        p.extras.beatHz = 3.5f;
        p.extras.variation = 0.5f;
    }
    // Wind: tuned breath alone through narrow bands, so it whistles on the key's harmonics.
    const float gust = clamp01(0.55f + 0.6f * drift.smooth(t + 40.0, 0.2));
    {
        auto& p = wind.patch;
        p.tone = 0.0f;
        p.breath = 1.0f;
        p.breathQ = lerp(5.0f, 11.0f, tone) * (desolate ? 1.3f : 1.0f);
        p.attack = 0.6f;
        p.release = 2.0f;
        p.scoop = 0.0f;
        p.vibratoDepth = 0.0f;
        p.spread = 0.9f;
        p.voices = 3;
        p.held = true;
        p.gain = 0.7f + 0.6f * gust * gust;
    }
    // The ghost pad: a hollow "oo" choir, far detuned, with a slow uneasy vibrato.
    {
        auto& p = ghost.patch;
        p.vowel = (haunted ? 0.2f : 0.6f) + 0.5f * tone;
        p.vowelDrift = 0.5f;
        p.attack = 2.5f;
        p.release = 3.0f;
        p.detune = haunted ? 24.0f : 14.0f;
        p.vibratoDepth = 0.2f + 0.2f * tension;
        p.brightness = 0.15f + 0.3f * tone;
        p.glide = 1.0f;
        p.spread = 0.9f;
        p.voices = byQuality(q, 3, 4, 4);
        p.gain = 1.0f;
    }
    // Low toms: soft skins tuned to the root and the fifth below.
    {
        auto& p = toms.patch;
        auto& low = p.rows[0];
        low = {};
        low.semis = -12.0f + float(music::degree(frame.mood, 4)) - 12.0f;
        low.bend = 1.12f;
        low.decay = 0.7f;
        low.tone = 0.8f;
        low.noise = 0.12f;
        low.noiseHz = lerpExp(300.0f, 900.0f, tone);
        low.noiseQ = 0.8f;
        low.noiseDecay = 0.05f;
        low.ratio = 1.5f;
        low.partial = 0.25f;
        low.pan = 0.25f;
        low.kind = dsp::DrumMembrane;
        low.position = 0.3f;
        low.hardness = 0.4f;
        auto& floor = p.rows[1];
        floor = {};
        floor.semis = -24.0f;
        floor.bend = 1.15f;
        floor.decay = 1.0f;
        floor.tone = 0.9f;
        floor.noise = 0.1f;
        floor.noiseHz = lerpExp(200.0f, 700.0f, tone);
        floor.noiseQ = 0.8f;
        floor.noiseDecay = 0.06f;
        floor.ratio = 1.5f;
        floor.partial = 0.2f;
        floor.pan = -0.2f;
        floor.kind = dsp::DrumMembrane;
        floor.position = 0.25f;
        floor.hardness = 0.4f;
        p.spread = 0.2f;
        p.voices = 3;
        p.gain = 1.0f;
    }
    // Tension: the procession comes closer and the ghosts rise; Machinery wears the
    // recording further down; Life lets the moss grow back over the music box.
    // Desolate ruins echo emptily; overgrown ones are muffled by leaves and moss.
    sendScale.fill(desolate ? 1.5f : (haunted ? 1.2f : 0.75f));
    level = { 0.2f * (haunted ? 1.2f : 1.0f), 0.14f * (overgrown ? 1.3f : (desolate ? 0.8f : 1.0f)) * lerp(0.8f, 1.3f, s.life),
              0.12f * (desolate ? 1.4f : 1.0f), 1.4f * (desolate ? 1.6f : (overgrown ? 0.8f : 1.0f)) * (0.7f + 0.8f * tension),
              0.14f * (haunted ? 1.6f : (overgrown ? 0.7f : 0.9f)) * (0.6f + 0.9f * tension),
              0.5f * (0.4f + 1.6f * tension) * (1.0f + 1.0f * s.machinery) * (overgrown ? 0.7f : 1.0f) };

    // Lo-fi decay on the old instruments; a futuristic Era plays them cleaner.
    const float worn = clamp01((overgrown ? 0.45f : (haunted ? 0.6f : 0.75f)) + 0.35f * s.machinery - 0.4f * s.era);
    lofi.set(lerp(18000.0f, 9000.0f, worn), lerp(12.0f, 8.0f, worn), lerpExp(4000.0f, 12000.0f, tone) * (0.8f + 0.5f * s.era), worn);
    // Reverse echoes: long swells in the haunted ruin, a trace elsewhere.
    reverse.set(haunted ? 0.75f : 0.5f, (haunted ? 0.45f : 0.25f) + 0.25f * tension, lerpExp(1500.0f, 6000.0f, tone));
    reverseSend = (haunted ? 0.55f : (desolate ? 0.35f : 0.2f)) * (0.6f + 0.6f * s.fxDepth);
    // A roofless hall: long and dark, open to the sky.
    hall.set(lerp(0.55f, 1.0f, s.size), lerp(2.0f, 5.0f, s.size) * (desolate ? 1.3f : 1.0f) * (1.0f + 0.3f * s.magic), lerpExp(1200.0f, 5000.0f, tone));
    hallSend = (desolate ? 0.55f : 0.4f) + 0.2f * s.fxDepth;
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era), rate);
}

bool RuinsPlace::shape(const PlaceContext&, const Scheduled& x, NoteOn& on) {
    // Bells and shards come from around the broken walls.
    if (x.element == BrokenBells || x.element == PianoShards) {
        on.hasPan = true;
        on.pan = 0.7f * (2.0f * on.random - 1.0f);
    }
    return true;
}

void RuinsPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        // The old instruments through the worn recording.
        float ol = busL[PianoShards][k] + busL[MusicBox][k] + busL[BrokenBells][k], orr = busR[PianoShards][k] + busR[MusicBox][k] + busR[BrokenBells][k];
        lofi.process(ol, orr);
        float rl = 0, rr = 0, wl = 0, wr = 0;
        reverse.process(ol * reverseSend, orr * reverseSend, rl, rr);
        float l = ol + rl + busL[Wind][k] + busL[GhostPad][k] + busL[LowToms][k];
        float r = orr + rr + busR[Wind][k] + busR[GhostPad][k] + busR[LowToms][k];
        hall.process(l * hallSend, r * hallSend, wl, wr);
        l += 0.7f * wl;
        r += 0.7f * wr;
        left[i] = tilt[2].lp(tilt[0].lp(l));
        right[i] = tilt[3].lp(tilt[1].lp(r));
    }
}

} // namespace wb
