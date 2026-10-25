#include "Places.h"

namespace wb {

using namespace dsp;

StationPlace::StationPlace() : EnsemblePlace(2700) { setParts({ &hull, &clamps, &comms, &air, &reactor, &lead }); }

void StationPlace::prepareFx() {
    room.prepare(rate);
    seal.prepare(rate);
    delay.prepare(rate, 1.0f);
}

void StationPlace::resetFx() {
    room.clear();
    seal.clear();
    delay.clear();
    rotation.setPhase(0.0f);
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
}

void StationPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2701);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == HullDrone) {
        // The hull: the chord's root and fifth far below, held, changing with the chords.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -24;
        w.note(0, 0, 64, 0.9f);
        if (in.character != Abandoned) w.note(0, fifth, 64, 0.45f);
        if (tension > 0.6f) w.note(32, 1, 32, 0.3f + 0.3f * tension);
        return;
    }

    if (element == Clamps) {
        // Docking clamps (row 0), latches (row 1) and the thud of a mass locking on (row 2).
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        switch (in.character) {
            case RoutineOrbit:   // a slow docking cycle: clamp, latches, thud
                for (int bar = 0; bar < 2; ++bar) {
                    const int at = bar * 16;
                    w.note(at, 0, 1, 0.8f * human(1, at));
                    if (ch(2, at) < 0.3f + 0.6f * act) w.note(at + 6, 1, 1, 0.5f * human(3, at));
                    if (ch(4, at) < 0.2f + 0.6f * act) w.note(at + 10, 1, 1, 0.4f);
                    if (ch(5, at) < act * density) w.note(at + 12, 2, 1, 0.5f);
                }
                break;
            case Emergency:   // clamps slamming on the beat, latches rattling
                for (int at = 0; at < 32; at += 4) {
                    w.note(at, at % 8 == 0 ? 0 : 2, 1, (at % 8 == 0 ? 0.9f : 0.65f) * human(6, at));
                    if (ch(7, at) < 0.35f + 0.55f * busy) w.note(at + 2, 1, 1, 0.5f * human(8, at));
                    if (ch(9, at) < act * act) w.note(at + 3, 1, 1, 0.35f);
                }
                break;
            default:   // Abandoned: a loose clamp knocking now and then
                w.note(0, 2, 1, 0.6f);
                if (ch(10, 0) < 0.3f + 0.6f * act) w.note(10, 0, 1, 0.5f);
                if (ch(11, 0) < 0.2f + 0.6f * act) w.note(22, 1, 1, 0.4f);
                if (ch(12, 0) < act * act) w.note(27, 1, 1, 0.3f);
                break;
        }
        return;
    }

    if (element == Comms) {
        // Comms beeps: short motifs on the chord, high up, a phrase every bar or two.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 24;
        static constexpr int motifs[4][4] { { 4, 2, 0, -1 }, { 0, 4, 7, -1 }, { 7, 4, 7, 4 }, { 2, 0, 2, -1 } };
        for (int bar = 0; bar < 4; ++bar) {
            float chanceOf = 0;
            int every = 2;
            switch (in.character) {
                case RoutineOrbit: chanceOf = 0.2f + 0.7f * act; break;
                case Emergency: chanceOf = 0.5f + 0.5f * act; every = 1; break;
                default: chanceOf = 0.1f + 0.5f * act; every = 3; break;
            }
            if (bar > 0 && ch(20, bar) >= chanceOf) continue;
            const int m = int(ch(21, bar) * 4.0f) % 4, at = bar * 16 + (in.character == Abandoned ? 4 : 0) + 2 * int(ch(22, bar) * 3.0f);
            for (int k = 0; k < 4; ++k) {
                const int d = motifs[m][k];
                if (d < 0) break;
                w.note(at + k * every, d == 7 ? size : d, every, (k == 0 ? 0.7f : 0.55f) * human(23, at + k));
            }
            // Busy channels: a reply later in the bar.
            if (ch(24, bar) < act * density) w.note(bar * 16 + 12, 0, 2, 0.45f);
        }
        if (tension > 0.5f)
            for (int bar = 1; bar < 4; bar += 2) w.note(bar * 16 + 8, 1, 1, 0.4f + 0.3f * tension);
        return;
    }

    if (element == Airflow) {
        // Airflow: long breaths through the vents, root and fifth, two bars each.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 12;
        if (in.character == Abandoned) {
            // A breach: one long hiss, the pitch sagging to the fourth.
            w.note(0, 0, 48, 0.8f);
            w.note(48, music::nearestDegree(in.mood, 5), 16, 0.6f);
        } else {
            w.note(0, 0, 32, 0.7f);
            w.note(32, fifth, 32, 0.6f);
        }
        return;
    }

    if (element == Reactor) {
        // The reactor: a pulsing bass on the chord's root, eighths (sixteenths in an
        // emergency), an octave leap now and then. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = -12;
        const int every = in.character == Emergency ? 1 : 2;
        const float chanceOf = in.character == Abandoned ? 0.08f + 0.4f * act : 0.05f + 0.8f * busy;
        for (int step = 0; step < 32; step += every) {
            const bool beat = step % 4 == 0;
            if (!(step % 16 == 0) && ch(30, step) >= chanceOf * (beat ? 1.4f : 1.0f) * lerp(0.7f, 1.2f, density)) continue;
            const bool leap = step % 8 == 6 && ch(31, step) < 0.4f + 0.3f * tension;
            w.note(step, leap ? size : 0, every, (beat ? 0.75f : 0.55f) * human(32, step));
        }
        return;
    }

    if (element == Shimmer) {
        // The zero-g lead: slow phrases that glide between chord tones, high. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 12;
        static constexpr int line[8] { 4, 2, 0, 2, 4, 7, 4, 2 };
        const int length = in.character == Emergency ? 4 : 8;
        int k = int(ch(40, 0) * 8.0f);
        for (int at = 0; at < 64; at += length) {
            if (at > 0 && ch(41, at) >= (in.character == Abandoned ? 0.25f : 0.4f) + 0.5f * act) continue;
            const int d = line[k++ % 8];
            w.note(at, d == 7 ? size : d, length, 0.6f * human(42, at));
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void StationPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const bool emergency = s.character == Emergency, abandoned = s.character == Abandoned;

    // The hull: wide detuned saws through a low filter, slowly breathing.
    {
        auto& p = hull.patch;
        p.held = p.legato = true;
        p.attack = 2.5f;
        p.release = 3.0f;
        p.glide = 1.5f;
        p.detune = abandoned ? 14.0f : 6.0f;
        p.sub = 0.5f;
        p.cutoff = lerpExp(120.0f, 520.0f, tone) * (0.8f + 0.4f * s.machinery) * (1.0f + 0.3f * tension);
        p.envOctaves = 0.0f;
        p.q = 1.2f + 1.5f * tension;
        p.width = 0.8f;
        p.tremoloPerBeat = 0.25f;
        p.tremoloDepth = 0.15f + (emergency ? 0.2f : 0.0f);
        p.spread = 0.3f;
        p.voices = 3;
        p.gain = 1.0f;
    }
    // Docking clamps: struck steel (an inharmonic pair), latches, a heavy thud.
    {
        auto& p = clamps.patch;
        auto& clamp = p.rows[0];
        clamp = {};
        clamp.semis = -12.0f;
        clamp.bend = 1.02f;
        clamp.decay = abandoned ? 0.9f : 0.5f;
        clamp.tone = 0.7f;
        clamp.ratio = 2.76f;
        clamp.partial = 0.55f;
        clamp.noise = 0.25f;
        clamp.noiseHz = lerpExp(1500.0f, 4000.0f, tone);
        clamp.noiseQ = 1.5f;
        clamp.noiseDecay = 0.05f;
        clamp.pan = -0.3f;
        auto& latch = p.rows[1];
        latch = {};
        latch.semis = 12.0f;
        latch.bend = 1.0f;
        latch.decay = 0.12f;
        latch.tone = 0.5f;
        latch.ratio = 1.48f;
        latch.partial = 0.6f;
        latch.noise = 0.2f;
        latch.noiseHz = 5000.0f;
        latch.noiseQ = 2.0f;
        latch.noiseDecay = 0.02f;
        latch.pan = 0.4f;
        auto& thud = p.rows[2];
        thud = {};
        thud.kind = DrumMembrane;
        thud.semis = -24.0f;
        thud.bend = 1.3f;
        thud.decay = 0.6f;
        thud.tone = 0.9f;
        thud.noise = 0.1f;
        thud.noiseHz = 300.0f;
        thud.noiseQ = 0.7f;
        thud.noiseDecay = 0.04f;
        thud.position = 0.1f;
        thud.hardness = 0.3f;
        p.spread = 0.2f;
        p.voices = 5;
        p.gain = 1.0f;
    }
    // Comms: clean sine beeps with a faint octave, short.
    {
        auto& p = comms.patch;
        p.set({ 1.0f, 2.0f, 3.0f }, { 1.0f, 0.6f, 0.3f }, { 1.0f, 0.12f + 0.15f * tone, 0.04f * tone });
        p.decay = abandoned ? 0.5f : 0.22f;
        p.hardness = 0.15f;
        p.velocityHardness = 0.1f;
        p.toneHz = lerpExp(3000.0f, 12000.0f, tone);
        p.spread = 0.5f;
        p.voices = byQuality(q, 3, 4, 5);
        p.gain = 1.0f;
    }
    // Airflow: tuned breath through the vents (a whisper of pitch).
    {
        auto& p = air.patch;
        p.held = true;
        p.tone = abandoned ? 0.08f : 0.18f;
        p.breath = 1.0f;
        p.breathQ = 5.0f + 6.0f * tension;
        p.h2 = 0.1f;
        p.h3 = 0.0f;
        p.attack = 1.5f;
        p.release = 2.0f;
        p.scoop = 0.0f;
        p.vibratoDepth = 0.0f;
        p.wander = 0.0f;
        p.spread = 0.8f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // The reactor: a round filtered pulse bass, its filter snapping on each note.
    {
        auto& p = reactor.patch;
        p.held = false;
        p.attack = 0.004f;
        p.decay = emergency ? 0.18f : 0.32f;
        p.sustain = 0.0f;
        p.release = 0.12f;
        p.detune = 4.0f;
        p.sub = 0.8f;
        p.cutoff = lerpExp(140.0f, 600.0f, tone) * (0.8f + 0.5f * s.machinery);
        p.envOctaves = 2.0f + 1.5f * tension;
        p.filterEnvOctaves = 1.0f;
        p.filterDecay = 0.08f;
        p.q = 2.0f + 2.0f * tension;
        p.width = 0.2f;
        p.spread = 0.1f;
        p.voices = 3;
        p.gain = 1.0f;
    }
    // The zero-g lead: a soft FM tone that glides between notes, slowly wavering.
    {
        auto& p = lead.patch;
        p.ratio = 3.0f;
        p.index = 0.25f + 0.8f * tone + 0.4f * tension;
        p.indexFloor = 0.3f;
        p.indexSeconds = 1.2f;
        p.feedback = 0.05f;
        p.attack = 0.6f;
        p.decay = 1.5f;
        p.sustain = 0.7f;
        p.release = 1.6f;
        p.held = true;
        p.glide = 0.35f;
        p.vibratoHz = 0.7f;
        p.vibratoDepth = abandoned ? 0.25f : 0.1f;
        p.pan = 0.2f;
        p.spread = 0.3f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Machinery drives the hull and the reactor; Tension the clamps and the calls.
    sendScale.fill(abandoned ? 1.5f : (emergency ? 0.75f : 1.0f));
    level = { 0.11f * (0.7f + 0.6f * s.machinery) * (abandoned ? 1.3f : 1.0f), 0.6f * (emergency ? 1.0f : 1.0f) * (0.7f + 0.6f * tension),
              0.4f * lerp(0.8f, 1.3f, s.life), 0.42f * (abandoned ? 1.3f : 1.0f), 0.15f * (0.6f + 0.8f * s.machinery) * (abandoned ? 0.7f : 1.0f),
              0.12f * (emergency ? 0.8f : 1.0f) * (0.8f + 0.4f * s.magic) };

    // The sealed metallic room: small and bright, its walls ringing on the chord.
    room.set(lerp(0.12f, 0.45f, s.size), lerp(0.6f, 1.6f, s.size) * (abandoned ? 1.7f : 1.0f) * (1.0f + 0.3f * s.magic), lerpExp(3500.0f, 10000.0f, tone));
    roomSend = (abandoned ? 0.6f : 0.42f) + 0.2f * s.fxDepth;
    const int chordRoot = music::inRange(music::chordTone(frame.mood, c.chord, 0), 0);
    seal.set(music::noteHz(frame.root, float(chordRoot + 12)), music::noteHz(frame.root, float(chordRoot + 19)) * 1.002f, 0.75f + 0.1f * tension,
             lerpExp(2000.0f, 5000.0f, tone));
    sealSend = (emergency ? 0.12f : 0.08f) * (0.6f + 0.6f * s.fxDepth);
    // A filtered delay on the beeps and the lead, its band sweeping; dotted eighths.
    const float beat = 60.0f / std::max(c.bpm, 30.0f);
    delay.set(std::min(0.75f * beat, 0.95f), (abandoned ? 0.6f : 0.45f) + 0.15f * s.fxDepth, lerpExp(600.0f, 1200.0f, tone), lerpExp(2500.0f, 6000.0f, tone),
              emergency ? 0.25f : 0.05f, 1.6f);
    delaySend = (abandoned ? 0.65f : 0.45f) * (0.6f + 0.6f * s.fxDepth);
    // The hull turns: one revolution every 16 beats (faster when spinning out of control).
    rotation.setFrequency(float(frame.beatsPerSecond) / (emergency ? 8.0f : 16.0f), rate);
    rotation.normalise();
    turn = 0.35f + 0.25f * s.size;
    for (auto& f : tilt) f.setCutoff(lerpExp(1000.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era), rate);
    presence = 1.2f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
    dry = lerp(1.15f, 0.75f, s.size);
}

void StationPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        // The hull swings round the listener as the station turns.
        rotation.step();
        const float hl = busL[HullDrone][k] * (1.0f - turn * rotation.re), hr = busR[HullDrone][k] * (1.0f + turn * rotation.re);
        float l = hl, r = hr;
        for (int e = 1; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        float dl = 0, dr = 0, cl = 0, cr = 0, wl = 0, wr = 0;
        delay.process((busL[Comms][k] + busL[Shimmer][k]) * delaySend, (busR[Comms][k] + busR[Shimmer][k]) * delaySend, dl, dr);
        l += dl;
        r += dr;
        seal.process((l + r) * 0.5f * sealSend, cl, cr);
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
