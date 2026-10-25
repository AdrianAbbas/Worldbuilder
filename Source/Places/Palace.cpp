#include "Places.h"

namespace wb {

using namespace dsp;

PalacePlace::PalacePlace() : EnsemblePlace(2900) { setParts({ &harpsichord, &strings, &brass, &glock, &timpani, &choir }); }

void PalacePlace::prepareFx() { hall.prepare(rate); }

void PalacePlace::resetFx() {
    hall.clear();
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
}

void PalacePlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2901);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    const bool ball = in.character == GrandBall;
    PatternWriter w { out };

    if (element == Harpsichord) {
        out.followChords = true;
        out.base = -2;
        switch (in.character) {
            case GrandBall: {   // a waltz: four bars of 3/4 (12 steps each) over three of 4/4
                out.setGrid(pat::Sixteenth, 3);
                for (int bar = 0; bar < 4; ++bar) {
                    const int b = bar * 12;
                    w.note(b, -size, 4, 0.7f * human(1, b));   // the bass on one
                    for (int beat = 1; beat < 3; ++beat) {
                        w.note(b + beat * 4, 2, 2, 0.5f * human(2, b + beat));
                        w.note(b + beat * 4, 4, 2, 0.5f * human(3, b + beat));
                        if (ch(4, b + beat) < (0.3f + 0.5f * density) * busy) w.note(b + beat * 4 + 2, size, 2, 0.4f);
                    }
                }
                break;
            }
            case ThroneRoom:   // stately chords, rolled upward, a dotted rhythm
                out.setGrid(pat::Sixteenth, 2);
                for (int bar = 0; bar < 2; ++bar)
                    for (int k : { 0, 6, 8, 12 }) {
                        const int at = bar * 16 + k;
                        if (k != 0 && ch(5, at) >= (0.3f + 0.5f * density) * busy) continue;
                        w.note(at, 0, 4, 0.65f * human(6, at));
                        if (at + 1 < 32) w.note(at + 1, 2, 3, 0.55f);
                        if (at + 2 < 32) w.note(at + 2, 4, 2, 0.55f);
                        if (k == 0 && at + 3 < 32) w.note(at + 3, size, 1, 0.5f);
                    }
                break;
            default: {   // Fallen Palace: a broken Alberti figure, slowed, notes missing
                out.setGrid(pat::Sixteenth, 2);
                static constexpr int alberti[4] { 0, 4, 2, 4 };
                for (int step = 0; step < 32; step += 2) {
                    if (step % 8 != 0 && ch(7, step) >= (0.25f + 0.4f * density) * busy) continue;
                    w.note(step, alberti[(step / 2) % 4], 3, 0.5f * human(8, step));
                }
                break;
            }
        }
        if (tension > 0.6f) w.note(int(out.lengthSteps) - 4, 1, 4, 0.45f);
        return;
    }

    if (element == Strings) {
        // A string-ensemble pad on the chord, its top line moving. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -12;
        w.note(0, 0, 64, 0.75f);
        w.note(0, 4, 64, 0.6f);
        w.note(0, size + 2, 32, 0.6f);
        w.note(32, ch(10, 0) < 0.5f ? size + 1 : size + 3, 32, 0.55f);
        if (in.character == ThroneRoom) w.note(0, -size, 64, 0.6f);
        return;
    }

    if (element == Fanfare) {
        // Fanfares on the notes of the chord, in dotted rhythms. Eight bars.
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = false;
        const auto fanfare = [&](int at, float velocity) {
            static constexpr int notes[7] { 0, 0, 2, 4, 4, 2, 4 };
            static constexpr int when[7] { 0, 3, 4, 6, 9, 10, 12 };
            for (int k = 0; k < 7; ++k) w.note(at + when[k], notes[k] + (k == 6 ? size - 4 : 0), k == 6 ? 12 : (k % 3 == 0 ? 3 : 1), velocity * (k == 0 || k == 6 ? 1.0f : 0.8f));
        };
        switch (in.character) {
            case GrandBall:   // a flourish to open the dance
                fanfare(0, 0.6f);
                if (ch(20, 0) < 0.2f + 0.5f * act) fanfare(64, 0.5f * human(21, 64));
                break;
            case ThroneRoom:   // the herald's fanfare, answered
                fanfare(0, 0.75f);
                fanfare(32, 0.65f);
                if (ch(22, 0) < 0.3f + 0.6f * act) fanfare(64, 0.7f * human(23, 64));
                if (ch(24, 0) < 0.2f + 0.5f * act * density) fanfare(96, 0.6f);
                break;
            default:   // Fallen Palace: a lone, distant call
                w.note(0, 0, 6, 0.5f);
                w.note(6, 4, 2, 0.45f);
                w.note(8, size, 16, 0.5f);
                if (ch(25, 0) < 0.3f + 0.5f * act) {
                    w.note(64, 4, 8, 0.45f);
                    w.note(72, 2, 16, 0.4f);
                }
                break;
        }
        return;
    }

    if (element == Glockenspiel) {
        // Glockenspiel: sparkling figures on the chord tones high above.
        out.setGrid(pat::Sixteenth, ball ? 3 : 4);
        out.followChords = false;
        const int steps = int(out.lengthSteps);
        float keep = 0;
        int every = 2;
        switch (in.character) {
            case GrandBall: keep = 0.35f + 0.5f * act; every = 2; break;
            case ThroneRoom: keep = 0.15f + 0.35f * act; every = 4; break;
            default: keep = 0.1f + 0.3f * act; every = 4; break;
        }
        static constexpr int line[8] { 4, 2, 0, 2, 4, 7, 4, 2 };
        for (int k = 0; k < steps / every; ++k) {
            const int at = k * every;
            if (at % 16 != 0 && ch(30, k) >= keep * lerp(0.7f, 1.3f, density)) continue;
            const int d = line[k % 8];
            w.note(at, d >= 7 ? d - 7 + size : d, every, (at % 8 == 0 ? 0.6f : 0.45f) * human(31, k));
        }
        if (tension > 0.6f) w.note(steps - 4, fifth, 4, 0.4f);
        return;
    }

    if (element == Timpani) {
        // Courtly timpani on the root (row 0) and the fifth (row 1).
        out.followChords = false;
        switch (in.character) {
            case GrandBall:   // the downbeat of each waltz bar, a second hit now and then
                out.setGrid(pat::Sixteenth, 3);
                for (int bar = 0; bar < 4; ++bar) {
                    w.note(bar * 12, bar % 2 == 0 ? 0 : 1, 1, 0.6f * human(40, bar));
                    if (ch(41, bar) < 0.3f * busy * density) w.note(bar * 12 + 8, 1, 1, 0.4f);
                }
                break;
            case ThroneRoom:   // hits with the fanfare, a roll into every other bar
                out.setGrid(pat::Sixteenth, 2);
                w.note(0, 0, 1, 0.8f);
                w.note(6, 1, 1, 0.55f);
                w.note(8, 0, 1, 0.65f);
                for (int step = 24; step < 32; ++step)
                    if (step >= 28 || ch(42, step) < (0.3f + 0.5f * density) * busy) w.note(step, 0, 1, 0.3f + 0.04f * float(step - 24));
                break;
            default:   // Fallen Palace: a muffled, distant hit
                out.setGrid(pat::Sixteenth, 2);
                w.note(0, 0, 1, 0.55f);
                if (ch(43, 0) < 0.3f + 0.6f * act) w.note(20, 1, 1, 0.4f);
                break;
        }
        if (tension > 0.5f)
            for (int step = int(out.lengthSteps) - 4; step < int(out.lengthSteps); ++step) w.note(step, 1, 1, 0.3f + 0.4f * tension);
        return;
    }

    if (element == Choir) {
        // Choir swells on the chord: two-bar breaths.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -2;
        switch (in.character) {
            case GrandBall:   // soft, high, now and then
                w.note(0, 2, 28, 0.5f);
                w.note(0, 4, 28, 0.45f);
                if (ch(50, 0) < 0.3f + 0.5f * act) w.note(32, 0, 28, 0.45f);
                break;
            case ThroneRoom:   // a full choir
                w.note(0, 0, 60, 0.7f);
                w.note(0, 2, 60, 0.6f);
                w.note(0, 4, 60, 0.6f);
                w.note(0, size, 30, 0.55f);
                w.note(32, size + 1, 28, 0.5f);
                break;
            default:   // Fallen Palace: a ghostly pair of voices
                w.note(0, 0, 30, 0.45f);
                w.note(32, 4, 30, 0.4f);
                break;
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void PalacePlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const bool ball = s.character == GrandBall, throne = s.character == ThroneRoom, fallen = s.character == FallenPalace;

    // Harpsichord: a bright, short pluck, plucked quills (a little dull when fallen).
    {
        auto& p = harpsichord.patch;
        p.decay = (fallen ? 1.6f : 1.0f) * lerp(1.0f, 0.7f, tension);
        p.brightness = (fallen ? 0.45f : 0.65f) + 0.3f * tone;
        p.spread = 0.5f;
        p.voices = byQuality(q, 5, 7, 8);
        // Two unison 8' choirs plucked by quills near the end of the string, the jack's
        // damper stopping each note as it ends, a bright soundboard.
        p.body = dsp::BodyHarpsichord;
        p.shape = { 0.07f, 0.12f, 0.25f, 1.0f, 1.5f, 0.5f, 0.12f };
        p.gain = 1.0f;
    }
    // A string ensemble: many slightly detuned saws, slow bows (thin and wavering when fallen).
    {
        auto& p = strings.patch;
        // A court string ensemble (thin, wavering and out of tune when fallen).
        p.model = BowedString;
        p.size = dsp::Violin;
        p.players = 3;
        p.spreadCents = fallen ? 22.0f : 9.0f;
        p.position = 0.12f;
        p.brightness = lerp(0.35f, 0.8f, tone) * (fallen ? 0.6f : 1.0f);
        p.noisiness = 0.15f;
        p.attack = fallen ? 2.5f : 1.2f;
        p.release = 1.8f;
        p.glide = 0.6f;
        p.vibratoDepth = (fallen ? 0.28f : 0.18f) + 0.1f * tension;
        p.spread = 0.8f;
        p.voices = byQuality(q, 5, 6, 6);
        p.held = true;
        p.gain = 1.0f;
    }
    // Fanfare brass: bright trumpets.
    {
        auto& p = brass.patch;
        p.attack = 0.05f;
        p.decay = 0.4f;
        p.sustain = 0.75f;
        p.release = fallen ? 0.8f : 0.25f;
        p.held = p.legato = true;
        p.glide = 0.03f;
        p.detune = 5.0f;
        p.sub = 0.0f;
        p.cutoff = lerpExp(450.0f, 1800.0f, tone) * (fallen ? 0.6f : 1.0f);
        p.envOctaves = 1.8f;
        p.filterEnvOctaves = 0.8f;
        p.filterDecay = 0.12f;
        p.q = 0.9f;
        p.width = 0.4f;
        p.vibratoHz = 5.0f;
        p.vibratoDepth = 0.05f + 0.1f * s.life;
        p.vibratoDelay = 0.35f;
        p.transpose = 12.0f;
        p.spread = 0.3f;
        p.voices = 3;
        // Natural trumpets: bright, the bell's formant up high.
        p.model = dsp::SynthBrass;
        p.cutoff = lerpExp(800.0f, 1400.0f, tone) * (fallen ? 0.7f : 1.0f);
        p.formantHz = 1300.0f;
        p.formantLevel = 0.6f;
        p.breath = 0.04f;
        p.gain = 1.0f;
    }
    // Glockenspiel: hard steel bars, the second partial a quarter-tone off any note.
    {
        auto& p = glock.patch;
        p.set({ 1.0f, 2.76f, 5.4f }, { 1.0f, 0.4f, 0.2f }, { 1.0f, 0.3f * (0.4f + tone), 0.12f * tone });
        p.decay = fallen ? 2.6f : 1.6f;
        p.hardness = 0.8f + 0.15f * tone;
        p.toneHz = lerpExp(5000.0f, 14000.0f, tone);
        p.transpose = 24.0f;
        p.spread = 0.5f;
        p.pan = 0.25f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.0f;
    }
    // Timpani on the root and the fifth below.
    {
        auto& p = timpani.patch;
        auto& root = p.rows[0];
        root = {};
        root.semis = -12;
        root.bend = 1.05f;
        root.decay = lerp(3.0f, 1.4f, tension) * (fallen ? 0.6f : 1.0f);
        root.tone = 0.55f;
        root.noise = 0.06f;
        root.noiseHz = 800;
        root.noiseQ = 0.7f;
        root.noiseDecay = 0.15f;
        root.ratio = 1.5f;
        root.partial = 0.35f;
        root.pan = -0.15f;
        root.kind = dsp::DrumTimpani;
        root.position = 0.25f;
        root.hardness = 0.5f;
        p.rows[1] = root;
        p.rows[1].semis = float(music::degree(frame.mood, music::nearestDegree(frame.mood, 7))) - 12.0f;
        p.rows[1].pan = 0.15f;
        p.voices = 4;
        p.gain = 1.0f;
    }
    // Choir: "ah" at the ball and the throne, a dark "oo" in the ruined halls.
    {
        auto& p = choir.patch;
        p.vowel = (fallen ? 0.4f : 2.0f) + 0.6f * tone - 0.3f;
        p.vowelDrift = 0.3f;
        p.attack = fallen ? 2.2f : 1.2f;
        p.release = 2.0f;
        p.detune = fallen ? 16.0f : 8.0f;
        p.vibratoDepth = 0.08f + 0.1f * s.life;
        p.brightness = tone;
        p.spread = 0.7f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.0f;
    }
    // Tension: timpani and brass come forward, the strings tremble, the harpsichord and
    // glockenspiel thin out; Life fills the hall's voices; Machinery the court's clockwork glock.
    // Tension: the hall answers louder (more of everything in the reverbs and echoes).
    sendScale.fill(lerp(0.6f, 1.6f, tension));
    level = { 1.0f * (1.0f - 0.35f * tension), 0.045f * (fallen ? 0.8f : 1.0f), 0.22f * (throne ? 1.1f : (fallen ? 0.7f : 0.9f)) * (0.7f + 0.6f * tension),
              0.16f * (1.0f - 0.3f * tension) * (0.7f + 0.8f * s.machinery), 0.13f * (0.7f + 0.8f * tension), 0.4f * lerp(0.7f, 1.3f, s.life) * (ball ? 1.3f : 1.0f) };

    // A grand hall: long, bright and wide (bigger and darker for the fallen palace).
    hall.set(lerp(0.7f, 1.0f, s.size), lerp(2.2f, 5.0f, s.size) * (fallen ? 1.4f : 1.0f) * (1.0f + 0.3f * s.magic), lerpExp(2500.0f, 9000.0f, tone) * (fallen ? 0.6f : 1.0f));
    hallSend = (fallen ? 0.7f : 0.5f) + 0.2f * s.fxDepth;
    hallWet = fallen ? 0.85f : 0.7f;
    dry = lerp(1.0f, 0.7f, s.size) * (fallen ? 0.85f : 1.0f);
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era) * (fallen ? 0.7f : 1.0f) * (1.0f - 0.3f * tension), rate);
    presence = 1.2f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
}

bool PalacePlace::shape(const PlaceContext&, const Scheduled&, NoteOn&) { return true; }

void PalacePlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        float wl = 0, wr = 0;
        hall.process(l * hallSend, r * hallSend, wl, wr);
        l = dry * l + hallWet * wl;
        r = dry * r + hallWet * wr;
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
