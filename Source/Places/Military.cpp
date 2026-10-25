#include "Places.h"

namespace wb {

using namespace dsp;

MilitaryPlace::MilitaryPlace() : EnsemblePlace(2600) { setParts({ &snare, &bugle, &drums, &radio, &clanks, &strings }); }

void MilitaryPlace::prepareFx() {
    room.prepare(rate);
    slap.prepare(rate);
    band.prepare(rate);
}

void MilitaryPlace::resetFx() {
    room.clear();
    slap.clear();
    band.clear();
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
}

void MilitaryPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2601);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == SnareMarch) {
        // A field snare (row 0) and rim clicks (row 1). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        switch (in.character) {
            case MorningDrill:   // a brisk march: eighths with accents, a roll into each bar
                for (int step = 0; step < 32; step += 2) {
                    const int inBar = step % 16;
                    const bool accent = inBar == 0 || inBar == 6 || inBar == 12;
                    if (accent || ch(1, step) < 0.45f + 0.5f * busy) w.note(step, 0, 1, (accent ? 0.8f : 0.45f) * human(2, step));
                }
                for (int step = 12; step < 32; step += 16)
                    if (ch(3, step) < (0.3f + 0.5f * density) * busy)
                        for (int k = 1; k < 4; k += 2) w.note(step + k, 0, 1, 0.4f + 0.1f * float(k));
                break;
            case OnAlert:   // driving sixteenths, accented in threes
                for (int step = 0; step < 32; ++step) {
                    const bool accent = step % 16 == 0 || step % 16 == 3 || step % 16 == 6 || step % 16 == 10 || step % 16 == 12;
                    if (accent || ch(4, step) < (0.2f + 0.5f * density) * busy) w.note(step, accent ? 0 : 1, 1, (accent ? 0.75f : 0.4f) * human(5, step));
                }
                break;
            default:   // Night Watch: rim clicks on the off-beats, a soft snare now and then
                for (int step = 4; step < 32; step += 8) w.note(step, 1, 1, 0.45f * human(6, step));
                for (int step = 0; step < 32; step += 2)
                    if (step % 8 != 4 && ch(7, step) < 0.15f * busy) w.note(step, 1, 1, 0.3f);
                if (ch(8, 0) < 0.3f + 0.5f * act) w.note(28, 0, 1, 0.35f);
                break;
        }
        if (tension > 0.5f)
            for (int step = 28; step < 32; ++step) w.note(step, 0, 1, 0.3f + 0.4f * tension);
        return;
    }

    if (element == Bugle) {
        // Bugle calls on the notes a bugle has (root, third, fifth, octave). Eight bars.
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = false;
        const int third = 2, octave = size, high = size + 2;
        switch (in.character) {
            case MorningDrill: {   // reveille: quick, bright, bouncing on the fifth
                static constexpr int call[12] { 0, 1, 2, 3, 2, 1, 0, 3, 2, 1, 2, 4 };
                const int notes[5] { fifth, octave, high, octave, fifth };
                const auto play = [&](int from, float velocity) {
                    for (int k = 0; k < 12 && from + k * 2 < 128; ++k)
                        w.note(from + k * 2, notes[call[k] % 5], k == 11 ? 6 : 2, velocity * (k % 4 == 0 ? 1.0f : 0.8f));
                };
                play(0, 0.75f);
                if (ch(10, 0) < 0.2f + 0.6f * act) play(64, 0.6f * human(11, 64));
                break;
            }
            case OnAlert: {   // a call to arms: repeated notes rising
                const int notes[4] { 0, third, fifth, octave };
                for (int rep = 0; rep < 2; ++rep) {
                    const int from = rep * 64;
                    if (rep == 1 && ch(12, 0) >= 0.3f + 0.6f * act) continue;
                    for (int k = 0; k < 4; ++k)
                        for (int r = 0; r < 3; ++r) w.note(from + k * 8 + r * 2, notes[k], 2, (r == 0 ? 0.75f : 0.55f) * human(13, from + k * 8 + r));
                    w.note(from + 32, octave, 12, 0.7f);
                }
                break;
            }
            default: {   // Night Watch: taps, slow and far off
                w.note(0, fifth - size, 6, 0.55f);
                w.note(6, fifth - size, 2, 0.45f);
                w.note(8, 0, 16, 0.6f);
                w.note(32, fifth - size, 6, 0.5f);
                w.note(38, 0, 2, 0.45f);
                w.note(40, third, 16, 0.6f);
                if (ch(14, 0) < 0.3f + 0.6f * act) {
                    w.note(64, 0, 6, 0.5f);
                    w.note(70, third, 2, 0.45f);
                    w.note(72, fifth, 24, 0.55f);
                }
                break;
            }
        }
        return;
    }

    if (element == LowDrums) {
        // A bass drum (row 0) and a low tom (row 1). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        switch (in.character) {
            case MorningDrill:   // a march: one and three, a tom pickup
                for (int beat = 0; beat < 8; beat += 2) w.note(beat * 4, 0, 1, 0.75f * human(20, beat));
                if (ch(21, 0) < 0.3f + 0.6f * act) w.note(14, 1, 1, 0.5f);
                if (ch(22, 0) < 0.3f * busy * density) w.note(30, 1, 1, 0.5f);
                break;
            case OnAlert:   // every beat, toms filling
                for (int beat = 0; beat < 8; ++beat) w.note(beat * 4, 0, 1, (beat % 2 == 0 ? 0.8f : 0.6f) * human(23, beat));
                for (int step = 2; step < 32; step += 4)
                    if (ch(24, step) < (0.2f + 0.5f * density) * busy) w.note(step, 1, 1, 0.5f * human(25, step));
                break;
            default:   // Night Watch: a slow heartbeat
                w.note(0, 0, 1, 0.65f);
                w.note(3, 0, 1, 0.4f);
                if (ch(26, 0) < 0.3f + 0.6f * act) w.note(16, 0, 1, 0.55f);
                if (ch(27, 0) < 0.2f * busy) w.note(24, 1, 1, 0.4f);
                break;
        }
        return;
    }

    if (element == RadioBlips) {
        // Radio traffic: short tuned beeps in Morse-like groups, high in the key. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        float chanceOf = 0;
        switch (in.character) {
            case MorningDrill: chanceOf = 0.15f + 0.35f * act; break;
            case OnAlert: chanceOf = 0.35f + 0.5f * act; break;
            default: chanceOf = 0.1f + 0.25f * act; break;
        }
        const int notes[3] { 0, fifth, size };
        for (int beat = 0; beat < 16; ++beat) {
            if (beat != 2 && ch(30, beat) >= chanceOf * lerp(0.7f, 1.3f, density)) continue;
            const int pitch = notes[int(ch(31, beat) * 2.99f)];
            const int count = 1 + int(ch(32, beat) * (1.0f + 3.0f * busy));
            int at = beat * 4;
            for (int k = 0; k < count && at < 64; ++k) {
                const bool dash = ch(33, beat * 8 + k) < 0.35f;
                w.note(at, pitch, dash ? 2 : 1, 0.6f * human(34, at));
                at += dash ? 3 : 2;
            }
        }
        return;
    }

    if (element == Clanks) {
        // Metal clanks (row 0: mess tins, rifles, a gate) and rattles (row 1: chains,
        // gear). Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        float chanceOf = 0;
        switch (in.character) {
            case MorningDrill: chanceOf = 0.25f + 0.4f * act; break;
            case OnAlert: chanceOf = 0.3f + 0.45f * act; break;
            default: chanceOf = 0.08f + 0.2f * act; break;
        }
        for (int step = 0; step < 64; step += 2) {
            if (step != 6 && ch(40, step) >= chanceOf * lerp(0.5f, 1.2f, density) * lerp(0.7f, 1.3f, life)) continue;
            const bool rattle = ch(41, step) < (in.character == OnAlert ? 0.5f : 0.3f);
            w.note(step, rattle ? 1 : 0, 1, (0.45f + 0.3f * ch(42, step)) * human(43, step));
            if (!rattle && ch(44, step) < 0.3f && step + 1 < 64) w.note(step + 1, 0, 1, 0.35f);
        }
        return;
    }

    if (element == Strings) {
        // A tense string ostinato on the chord. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = -12;
        switch (in.character) {
            case MorningDrill:   // quarter notes, root and fifth
                for (int beat = 0; beat < 8; ++beat)
                    if (beat % 2 == 0 || ch(50, beat) < 0.3f + 0.6f * busy) w.note(beat * 4, beat % 2 == 0 ? 0 : 4, 3, 0.55f * human(51, beat));
                break;
            case OnAlert: {   // driving sixteenths, root, root, fifth, the semitone-ish neighbour of the scale
                static constexpr int figure[8] { 0, 0, 4, 0, 0, 5, 4, 0 };
                for (int step = 0; step < 32; ++step) {
                    if (step % 4 != 0 && ch(52, step) >= (0.4f + 0.5f * density) * busy) continue;
                    w.note(step, figure[step % 8], 1, (step % 4 == 0 ? 0.7f : 0.5f) * human(53, step));
                }
                break;
            }
            default:   // Night Watch: long low notes, a slow swell on the fifth
                w.note(0, 0, 16, 0.55f);
                w.note(16, ch(54, 0) < 0.5f ? 4 : 5, 16, 0.5f);
                for (int step = 2; step < 32; step += 4)
                    if (ch(55, step) < 0.35f * busy) w.note(step, size, 1, 0.3f);
                break;
        }
        if (tension > 0.6f)
            for (int step = 24; step < 32; step += 2) w.note(step, 1, 2, 0.4f + 0.3f * tension);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void MilitaryPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const bool alert = s.character == OnAlert, night = s.character == NightWatch;

    // The field snare: a tight head with crisp wires; rim clicks.
    {
        auto& p = snare.patch;
        auto& head = p.rows[0];
        head = {};
        head.semis = 12.0f + float(music::degree(frame.mood, 4));
        head.bend = 1.04f;
        head.decay = 0.1f;
        head.tone = 0.3f;
        head.noise = 0.9f;
        head.noiseHz = lerpExp(1800.0f, 4500.0f, tone);
        head.noiseQ = 0.9f;
        head.noiseDecay = night ? 0.1f : 0.14f;
        head.ratio = 1.6f;
        head.partial = 0.3f;
        head.pan = 0.1f;
        head.kind = dsp::DrumMembrane;
        head.position = 0.35f;
        head.hardness = 0.85f;
        head.snare = 0.8f;
        auto& rim = p.rows[1];
        rim = {};
        rim.semis = 24.0f;
        rim.bend = 1.01f;
        rim.decay = 0.04f;
        rim.tone = 0.4f;
        rim.noise = 0.35f;
        rim.noiseHz = 3500;
        rim.noiseQ = 2.0f;
        rim.noiseDecay = 0.02f;
        rim.ratio = 2.71f;
        rim.partial = 0.4f;
        rim.pan = -0.15f;
        p.spread = 0.1f;
        p.voices = 6;
        p.gain = 1.0f;
    }
    // The bugle: a bright, valveless brass call.
    {
        auto& p = bugle.patch;
        p.attack = 0.04f;
        p.decay = 0.3f;
        p.sustain = 0.75f;
        p.release = night ? 0.6f : 0.15f;
        p.held = p.legato = true;
        p.glide = 0.03f;
        p.detune = 3.0f;
        p.sub = 0.0f;
        p.cutoff = lerpExp(500.0f, 2000.0f, tone) * (night ? 0.7f : 1.0f);
        p.envOctaves = 1.8f;
        p.filterEnvOctaves = 0.6f;
        p.filterDecay = 0.1f;
        p.q = 1.0f;
        p.width = 0.25f;
        p.vibratoHz = 5.2f;
        p.vibratoDepth = (night ? 0.12f : 0.05f) + 0.1f * s.life;
        p.vibratoDelay = 0.3f;
        p.transpose = 12.0f;
        p.spread = 0.1f;
        p.pan = night ? -0.4f : 0.2f;
        p.voices = 2;
        // One bugler: a conical bore, a little darker than a trumpet.
        p.model = dsp::SynthBrass;
        p.detune = 0.0f;
        p.cutoff = lerpExp(800.0f, 1400.0f, tone) * (night ? 0.75f : 1.0f);
        p.formantHz = 1100.0f;
        p.formantLevel = 0.7f;
        p.breath = 0.05f;
        p.gain = 1.0f;
    }
    // A bass drum on the root and a low tom on the fifth.
    {
        auto& p = drums.patch;
        auto& kick = p.rows[0];
        kick = {};
        kick.semis = -24;
        kick.bend = 1.3f;
        kick.decay = night ? 1.0f : 0.6f;
        kick.tone = 0.9f;
        kick.noise = 0.1f;
        kick.noiseHz = 400;
        kick.noiseDecay = 0.05f;
        kick.ratio = 1.5f;
        kick.partial = 0.2f;
        kick.kind = dsp::DrumMembrane;
        kick.position = 0.2f;
        kick.hardness = 0.35f;
        auto& tom = p.rows[1];
        tom = {};
        tom.semis = -12.0f + float(music::degree(frame.mood, 4));
        tom.bend = 1.12f;
        tom.decay = 0.45f;
        tom.tone = 0.7f;
        tom.noise = 0.12f;
        tom.noiseHz = 700;
        tom.noiseDecay = 0.06f;
        tom.ratio = 1.5f;
        tom.partial = 0.3f;
        tom.pan = 0.3f;
        tom.kind = dsp::DrumMembrane;
        tom.position = 0.3f;
        tom.hardness = 0.5f;
        p.spread = 0.1f;
        p.voices = 4;
        p.gain = 1.0f;
    }
    // Radio blips: pure beeps with a little FM edge.
    {
        auto& p = radio.patch;
        p.ratio = 2.0f;
        p.index = 0.3f + 0.6f * tone;
        p.indexFloor = 0.2f;
        p.indexSeconds = 0.05f;
        p.feedback = 0.0f;
        p.attack = 0.002f;
        p.decay = 0.12f;
        p.sustain = 0.0f;
        p.release = 0.03f;
        p.held = false;
        p.transpose = 24.0f;
        p.spread = 0.2f;
        p.pan = 0.3f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Clanks: struck steel (partials a quarter-tone off any note) and rattling chains.
    {
        auto& p = clanks.patch;
        auto& clank = p.rows[0];
        clank = {};
        clank.semis = 24.0f + float(music::degree(frame.mood, 4));
        clank.bend = 1.0f;
        clank.decay = 0.35f;
        clank.tone = 0.5f;
        clank.noise = 0.3f;
        clank.noiseHz = 2500;
        clank.noiseQ = 1.5f;
        clank.noiseDecay = 0.03f;
        clank.ratio = 2.76f;
        clank.partial = 0.5f;
        clank.pan = -0.4f;
        auto& rattle = p.rows[1];
        rattle = {};
        rattle.tuned = false;
        rattle.hz = 100;
        rattle.tone = 0.0f;
        rattle.noise = 0.9f;
        rattle.noiseHz = lerpExp(2500.0f, 6000.0f, tone);
        rattle.noiseQ = 3.0f;
        rattle.noiseDecay = 0.12f;
        rattle.pan = 0.4f;
        p.spread = 0.5f;
        p.voices = 4;
        p.gain = 1.0f;
    }
    // Strings: short, hard bow strokes (long and trembling on the night watch).
    {
        auto& p = strings.patch;
        // A string section (violins and violas), short hard strokes, sul ponticello on alert.
        p.model = BowedString;
        p.size = dsp::Violin;
        p.players = 3;
        p.spreadCents = 7.0f;
        p.position = alert ? 0.07f : 0.12f;
        p.brightness = lerp(0.35f, 0.8f, tone) * (night ? 0.75f : 1.0f);
        p.noisiness = 0.2f + 0.2f * tension;
        p.attack = night ? 0.6f : 0.04f;
        p.release = night ? 1.0f : 0.15f;
        p.glide = 0.05f;
        p.vibratoDepth = night ? 0.15f + 0.15f * tension : 0.08f;
        p.spread = 0.4f;
        p.voices = byQuality(q, 3, 4, 5);
        p.held = true;
        p.gain = 1.0f;
    }
    // Tension: the strings and the snare press on, the drums grow; Machinery brings the
    // clanking gear; Life the bugler's breath and the camp's bustle.
    // The night watch rings over open ground; on alert the camp is tight and close.
    sendScale.fill(night ? 1.4f : (alert ? 0.7f : 1.0f));
    level = { 1.3f * (night ? 1.4f : 1.0f) * (0.7f + 0.6f * tension), 0.2f * (night ? 0.45f : 1.0f), 0.3f * (0.7f + 0.6f * tension),
              0.04f * (alert ? 1.1f : 1.0f), 1.1f * (0.5f + 1.2f * s.machinery) * lerp(0.7f, 1.3f, s.life) * (night ? 1.4f : 1.0f), 0.26f * (0.6f + 0.9f * tension) };

    // Over the radio: the blips always, the night's bugle from far off.
    bugleRadio = night ? 0.7f : 0.0f;
    band.set(lerpExp(250.0f, 500.0f, tension), lerpExp(2200.0f, 3800.0f, tone), 2.0f + 4.0f * tension, 1.0f, 0.3f + 0.5f * tension);
    // A tight room, a slapback off the barracks.
    room.set(lerp(0.1f, 0.4f, s.size), lerp(0.4f, 1.1f, s.size) * (night ? 1.4f : 1.0f) * (1.0f + 0.3f * s.magic), lerpExp(2500.0f, 7000.0f, tone));
    roomSend = 0.3f + 0.2f * s.fxDepth;
    slap.set(lerp(0.08f, 0.16f, s.size), 0.15f, lerpExp(1800.0f, 5000.0f, tone));
    dry = lerp(1.0f, 0.7f, s.size);
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era) * (night ? 0.75f : 1.0f), rate);
    presence = 1.2f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
}

bool MilitaryPlace::shape(const PlaceContext&, const Scheduled& x, NoteOn& on) {
    // Clanks come from all over the camp.
    if (x.element == Clanks) {
        on.hasPan = true;
        on.pan = 0.8f * (2.0f * on.random - 1.0f);
    }
    return true;
}

void MilitaryPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        // The radio: the blips, and some of the bugle at night.
        float rl = busL[RadioBlips][k] + bugleRadio * busL[Bugle][k], rr = busR[RadioBlips][k] + bugleRadio * busR[Bugle][k];
        band.process(rl, rr);
        float l = rl + (1.0f - bugleRadio) * busL[Bugle][k], r = rr + (1.0f - bugleRadio) * busR[Bugle][k];
        for (int e : { int(SnareMarch), int(LowDrums), int(Clanks), int(Strings) }) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        float sl = 0, sr = 0, wl = 0, wr = 0;
        slap.process(l, r, sl, sr);
        room.process(l * roomSend, r * roomSend, wl, wr);
        l = dry * l + 0.35f * sl + 0.6f * wl;
        r = dry * r + 0.35f * sr + 0.6f * wr;
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
