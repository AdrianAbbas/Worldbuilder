#include "Places.h"

namespace wb {

using namespace dsp;

SpaceshipPlace::SpaceshipPlace() : EnsemblePlace(2100) { setParts({ &engine, &life, &computer, &airlock, &hull, &alarm }); }

void SpaceshipPlace::prepareFx() {
    room.prepare(rate);
    ring.prepare(rate);
    delay.prepare(rate, 0.8f);
}

void SpaceshipPlace::resetFx() {
    room.clear();
    ring.clear();
    delay.clear();
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
}

void SpaceshipPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2101);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Engine) {
        // The engine: the root and fifth far below, held all four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = -24;
        w.note(0, 0, 64, 0.9f);
        if (in.character != Derelict) w.note(0, fifth, 64, 0.5f);
        return;
    }

    if (element == LifeSupport) {
        // Life support breathing: a pump (row 0) and the hiss of air after it (row 1).
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        switch (in.character) {
            case Cruising:   // a calm cycle on every half note
                for (int at = 0; at < 32; at += 8) {
                    w.note(at, 0, 1, 0.75f * human(1, at));
                    if (ch(2, at) < 0.2f + 0.8f * act) w.note(at + 4, 1, 1, 0.5f);
                    if (ch(7, at) < act * act) w.note(at + 6, 1, 1, 0.35f);
                }
                break;
            case RedAlert:   // working hard: quarter notes, hissing in between
                for (int at = 0; at < 32; at += 4) {
                    w.note(at, 0, 1, (at % 8 == 0 ? 0.85f : 0.6f) * human(3, at));
                    if (ch(4, at) < 0.4f + 0.5f * busy) w.note(at + 2, 1, 1, 0.5f);
                }
                break;
            default:   // Derelict: failing, slow and irregular
                w.note(0, 0, 1, 0.6f);
                if (ch(5, 0) < 0.3f + 0.6f * act) w.note(14, 0, 1, 0.45f);
                if (ch(6, 0) < 0.3f + 0.6f * act) w.note(22, 1, 1, 0.4f);
                break;
        }
        if (density > 0.6f) w.note(30, 1, 1, 0.35f);
        return;
    }

    if (element == Computer) {
        // Computer tones: arpeggios on the chord, high up; Red Alert races, the derelict
        // ship's computer stutters. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = 12;
        static constexpr int arp[8] { 0, 2, 4, 7, 4, 2, 0, 4 };
        float chanceOf = 0;
        int every = 2;
        switch (in.character) {
            case Cruising: chanceOf = 0.1f + 0.8f * act; every = 2; break;
            case RedAlert: chanceOf = 0.45f + 0.5f * act; every = 1; break;
            default: chanceOf = 0.08f + 0.25f * act; every = 1; break;
        }
        for (int step = 0; step < 32; step += every) {
            const bool phraseStart = step % 8 == 0;
            if (!phraseStart && ch(10, step) >= chanceOf * lerp(0.7f, 1.3f, density)) continue;
            if (phraseStart && ch(11, step) >= 0.4f + 0.6f * busy && step != 0) continue;
            const int d = arp[(step / every) % 8];
            w.note(step, d == 7 ? size : d, every, (phraseStart ? 0.7f : 0.5f) * human(12, step));
        }
        return;
    }

    if (element == Airlock) {
        // The airlock: a long cycling swell (row 0) and the seal closing (row 1). Eight bars.
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = false;
        const auto chancy = [&](int step, int row, int steps, float probability) {
            w.note(step, row, steps, 0.8f, 0, probability);
        };
        switch (in.character) {
            case Cruising:
                chancy(16, 0, 32, 0.4f + 0.5f * act);
                chancy(48, 1, 16, 0.5f + 0.4f * act);
                break;
            case RedAlert:
                chancy(0, 0, 24, 0.6f + 0.4f * act);
                chancy(32, 1, 12, 0.8f);
                chancy(64, 0, 24, 0.4f + 0.5f * act);
                chancy(96, 1, 12, 0.6f + 0.3f * act);
                break;
            default:   // Derelict: a hatch that will not stay shut
                chancy(32, 0, 48, 0.5f + 0.4f * act);
                chancy(88, 1, 24, 0.3f + 0.5f * act + 0.2f * tension);
                break;
        }
        return;
    }

    if (element == Hull) {
        // Hull resonances: the ship's frame groaning on the root and fifth, low. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = -12;
        switch (in.character) {
            case Cruising:
                w.note(8, 0, 16, 0.55f);
                if (ch(20, 0) < 0.3f + 0.5f * act) w.note(40, fifth, 16, 0.5f);
                break;
            case RedAlert:
                for (int bar = 0; bar < 4; ++bar)
                    if (ch(21, bar) < 0.4f + 0.5f * busy || bar == 0) w.note(bar * 16 + 4, bar % 2 == 0 ? 0 : fifth, 10, 0.65f * human(22, bar));
                break;
            default:   // Derelict: long groans, the metal under strain
                w.note(0, 0, 24, 0.75f);
                w.note(32, ch(23, 0) < 0.5f ? fifth : size, 24, 0.7f);
                if (ch(24, 0) < act) w.note(56, 2, 8, 0.5f);
                break;
        }
        if (tension > 0.5f) w.note(20, fifth - size, 12, 0.55f + 0.3f * tension);
        return;
    }

    if (element == Alarm) {
        // A soft alarm motif: three notes falling to the root; Red Alert repeats a two-tone
        // call every bar. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 12;
        switch (in.character) {
            case Cruising:   // a gentle notification now and then
                w.note(0, 4, 2, 0.6f);
                w.note(2, 2, 2, 0.55f);
                w.note(4, 0, 4, 0.6f);
                if (ch(31, 0) < act * density) {
                    w.note(32, 4, 2, 0.5f);
                    w.note(34, size, 4, 0.5f);
                }
                break;
            case RedAlert:
                for (int bar = 0; bar < 4; ++bar)
                    if (bar % 2 == 0 || ch(32, bar) < 0.4f + 0.6f * busy)
                        for (int k = 0; k < 4; ++k) w.note(bar * 16 + 4 * k, k % 2 == 0 ? fifth : 0, 3, (k == 0 ? 0.8f : 0.65f) * human(33, bar * 4 + k));
                break;
            default:   // Derelict: a dying alarm, slowed down
                w.note(16, 4, 6, 0.5f);
                w.note(24, 2, 6, 0.45f);
                if (ch(34, 0) < 0.4f + 0.5f * act) w.note(32, 0, 12, 0.45f);
                break;
        }
        for (int bar = 1; bar < 4; bar += 2)
            if (ch(35, bar) < tension * 0.7f) w.note(bar * 16 + 8, fifth, 3, 0.5f + 0.3f * tension);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void SpaceshipPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool cruising = s.character == Cruising, alert = s.character == RedAlert, derelict = s.character == Derelict;

    // The engine: a deep hum of detuned saws; on red alert it throbs with the pumps, in
    // the derelict ship it sputters.
    const float sputter = derelict ? clamp01(0.55f + 0.8f * drift.smooth(t, 0.4)) : 1.0f;
    {
        auto& p = engine.patch;
        p.held = p.legato = true;
        p.attack = 2.0f;
        p.release = 2.5f;
        p.glide = 1.0f;
        p.detune = derelict ? 18.0f : 7.0f;
        p.sub = 0.6f;
        p.cutoff = lerpExp(80.0f, 300.0f, tone) * (alert ? 1.4f : 1.0f) * (0.6f + 0.4f * sputter) * (0.8f + 0.4f * s.machinery);
        p.envOctaves = 0.0f;
        p.q = 1.0f + 0.8f * tension;
        p.width = 0.5f;
        p.tremoloPerBeat = alert ? 2.0f : 0.5f;
        p.tremoloDepth = (alert ? 0.4f : 0.1f) + 0.3f * tension;
        p.spread = 0.2f;
        p.voices = 2;
        p.gain = sputter;
    }
    // Life support: a soft pump on the root and the hiss of air.
    {
        auto& p = life.patch;
        auto& pump = p.rows[0];
        pump = {};
        pump.semis = -24.0f;
        pump.bend = 1.25f;
        pump.decay = alert ? 0.18f : 0.3f;
        pump.tone = 0.8f;
        pump.noise = 0.1f;
        pump.noiseHz = 400.0f;
        pump.noiseQ = 0.7f;
        pump.noiseDecay = 0.06f;
        pump.ratio = 2.0f;
        pump.partial = 0.25f;
        pump.pan = -0.1f;
        auto& hiss = p.rows[1];
        hiss = {};
        hiss.tuned = false;
        hiss.hz = 100;
        hiss.tone = 0.0f;
        hiss.noise = 0.22f;
        hiss.noiseHz = lerpExp(2500.0f, 7000.0f, tone);
        hiss.noiseQ = 1.6f;
        hiss.noiseDecay = derelict ? 0.5f : 0.3f;
        hiss.pan = 0.3f;
        p.spread = 0.2f;
        p.voices = 4;
        p.gain = 1.0f;
    }
    // Computer: square-ish FM blips; the derelict one is glitchy and short.
    {
        auto& p = computer.patch;
        p.ratio = derelict ? 3.0f : 2.0f;
        p.index = 0.8f + 1.2f * tone + 0.6f * s.era;
        p.indexFloor = 0.4f;
        p.indexSeconds = 0.08f;
        p.feedback = 0.1f;
        p.attack = 0.002f;
        p.decay = derelict ? 0.06f : (alert ? 0.09f : 0.16f);
        p.sustain = 0.0f;
        p.release = 0.05f;
        p.held = false;
        p.spread = 0.6f;
        p.voices = byQuality(q, 3, 4, 5);
        p.gain = 1.0f;
    }
    // The airlock: a slow cycling swell, and the seal closing with a thud.
    airlock.patch.rows[0] = { SweepSwell, -12.0f, 36.0f, 2.5f, 1.0f, false, true };
    airlock.patch.rows[1] = { SweepFall, 0.0f, 48.0f, 3.0f, 0.9f, true, false };
    airlock.patch.tension = tension;
    airlock.patch.voices = 2;
    airlock.patch.gain = 1.0f;
    // The hull: bowed metal plates, their upper modes a quarter-tone off any note.
    {
        auto& p = hull.patch;
        p.set({ 1.0f, 2.0f, 2.749f, 4.12f }, { 1.0f, 0.7f, 0.5f, 0.35f }, { 1.0f, 0.4f, 0.25f * (0.4f + tone), 0.15f * tone });
        p.ring = derelict ? 1.4f : 0.8f;
        p.rosin = 0.6f + 0.3f * tension;
        p.noisiness = 0.3f + 0.3f * tension;
        p.attack = derelict ? 1.2f : 0.6f;
        p.release = 1.5f;
        p.glide = 0.4f;
        p.vibratoHz = 0.3f;
        p.vibratoDepth = derelict ? 0.15f : 0.05f;
        p.spread = 0.6f;
        p.voices = 3;
        p.held = true;
        p.gain = 1.0f;
    }
    // The alarm: a soft pure tone, beeping its notes' lengths.
    {
        auto& p = alarm.patch;
        p.ratio = 1.0f;
        p.index = 0.3f + 0.6f * tension + (alert ? 0.5f : 0.0f);
        p.indexFloor = 0.3f;
        p.indexSeconds = 0.2f;
        p.feedback = 0.0f;
        p.attack = 0.01f;
        p.decay = 0.3f;
        p.sustain = 0.8f;
        p.release = derelict ? 0.4f : 0.08f;
        p.held = true;
        p.vibratoHz = derelict ? 0.8f : 5.0f;
        p.vibratoDepth = derelict ? 0.15f : 0.0f;
        p.pan = 0.25f;
        p.spread = 0.1f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Tension: the hull strains and the alarm rises; Machinery drives the engine and pumps.
    // The derelict's dead corridors ring on; red alert slams the bulkheads shut.
    sendScale.fill(derelict ? 1.5f : (alert ? 0.7f : 1.0f));
    level = { 0.12f * (derelict ? 0.6f : 1.0f) * (0.7f + 0.6f * s.machinery), 0.4f * (0.6f + 0.8f * s.machinery) * (derelict ? 0.7f : 1.0f),
              0.13f * lerp(0.8f, 1.3f, s.life), 0.5f * (alert ? 1.2f : 1.0f), 0.14f * (derelict ? 1.6f : 1.0f) * (0.6f + 1.0f * tension),
              0.06f * (alert ? 2.2f : (cruising ? 0.8f : 1.0f)) * (0.6f + 1.0f * tension) };

    // A confined metallic room: a small bright space whose walls ring on the chord.
    room.set(lerp(0.08f, 0.35f, s.size), lerp(0.4f, 1.1f, s.size) * (derelict ? 1.6f : 1.0f) * (1.0f + 0.3f * s.magic), lerpExp(3000.0f, 9000.0f, tone));
    roomSend = (derelict ? 0.55f : 0.4f) + 0.2f * s.fxDepth;
    const int chordRoot = music::inRange(music::chordTone(frame.mood, c.chord, 0), 0);
    ring.set(music::noteHz(frame.root, float(chordRoot + 24)), music::noteHz(frame.root, float(chordRoot + 36)) * 1.003f, 0.8f, lerpExp(2500.0f, 6000.0f, tone));
    ringSend = (alert ? 0.12f : 0.08f) * (0.6f + 0.6f * s.fxDepth);
    // A filtered delay on the computer and the alarm, its band sweeping.
    const float beat = 60.0f / std::max(c.bpm, 30.0f);
    delay.set(std::min(0.75f * beat, 0.75f), (derelict ? 0.55f : 0.4f) + 0.15f * s.fxDepth, lerpExp(500.0f, 900.0f, tone), lerpExp(2000.0f, 5000.0f, tone),
              alert ? 0.2f : 0.06f, 1.4f);
    delaySend = (derelict ? 0.6f : 0.4f) * (0.6f + 0.6f * s.fxDepth);
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era), rate);
    // Tone and Era also lift or soften the presence above about 2 kHz; a larger world
    // puts the ship further off (less of it dry).
    presence = 1.2f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
    dry = lerp(1.15f, 0.75f, s.size);
}

void SpaceshipPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        float dl = 0, dr = 0, cl = 0, cr = 0, wl = 0, wr = 0;
        delay.process((busL[Computer][k] + busL[Alarm][k]) * delaySend, (busR[Computer][k] + busR[Alarm][k]) * delaySend, dl, dr);
        l += dl;
        r += dr;
        // The walls ring a little under everything.
        ring.process((l + r) * 0.5f * ringSend, cl, cr);
        room.process(l * roomSend, r * roomSend, wl, wr);
        l = l * dry + 0.6f * wl + cl;
        r = r * dry + 0.6f * wr + cr;
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
