#include "Places.h"

namespace wb {

using namespace dsp;

SuburbsPlace::SuburbsPlace() : EnsemblePlace(2400) { setParts({ &piano, &pulse, &chimes, &sprinkler, &box, &bass }); }

void SuburbsPlace::prepareFx() {
    tape.prepare(rate);
    room.prepare(rate);
}

void SuburbsPlace::resetFx() {
    tape.clear();
    room.clear();
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
}

void SuburbsPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2401);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == EPiano) {
        // Warm electric-piano chords on the shared chord. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -2;
        const auto chord = [&](int step, int length, float velocity, bool seventh) {
            w.note(step, 0, length, velocity);
            w.note(step, 2, length, velocity * 0.85f);
            w.note(step, 4, length, velocity * 0.85f);
            if (seventh) w.note(step, 6, length, velocity * 0.75f);
        };
        switch (in.character) {
            case SunnyAfternoon:   // a lazy off-beat comp
                for (int bar = 0; bar < 4; ++bar) {
                    chord(bar * 16, 6, 0.6f * human(1, bar), false);
                    for (int k : { 6, 10, 14 })
                        if (ch(2, bar * 16 + k) < (0.3f + 0.5f * density) * busy) chord(bar * 16 + k, 2, 0.45f * human(3, bar * 16 + k), false);
                }
                break;
            case Evening:   // long, soft sevenths
                for (int bar = 0; bar < 4; bar += 2) {
                    chord(bar * 16, 28, 0.55f * human(4, bar), true);
                    if (ch(5, bar) < act * 0.7f) chord(bar * 16 + 24, 6, 0.4f, true);
                }
                break;
            default:   // Uncanny: the same chord, held a little too long
                chord(0, 48, 0.5f, true);
                if (ch(6, 0) < 0.4f + 0.5f * act) chord(48, 16, 0.4f, false);
                break;
        }
        return;
    }

    if (element == PopPulse) {
        // A soft pop pulse: eighths on the chord, the octave and fifth. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = 0;
        float keep = 0;
        switch (in.character) {
            case SunnyAfternoon: keep = 0.55f + 0.45f * act; break;
            case Evening: keep = 0.3f + 0.45f * act; break;
            default: keep = 0.15f + 0.3f * act; break;
        }
        static constexpr int line[8] { 0, 4, 7, 4, 0, 4, 7, 9 };
        for (int step = 0; step < 32; step += 2) {
            const bool strong = step % 8 == 0;
            if (!strong && ch(10, step) >= keep * lerp(0.7f, 1.3f, density)) continue;
            const int d = line[(step / 2) % 8];
            w.note(step, d >= 7 ? d - 7 + size : d, 2, (strong ? 0.65f : 0.5f) * human(11, step));
        }
        return;
    }

    if (element == WindChimes) {
        // Wind chimes on a porch: little clusters high in the key when the breeze blows.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 24;
        float chanceOf = 0;
        switch (in.character) {
            case SunnyAfternoon: chanceOf = 0.25f + 0.4f * act; break;
            case Evening: chanceOf = 0.3f + 0.45f * act; break;
            default: chanceOf = 0.1f + 0.2f * act; break;
        }
        for (int beat = 0; beat < 16; ++beat) {
            if (ch(20, beat) >= chanceOf * lerp(0.6f, 1.4f, life) && beat != 4) continue;
            const int at = beat * 4 + int(ch(21, beat) * 3.0f), notes = 1 + int(ch(22, beat) * (1.0f + 2.5f * busy));
            for (int k = 0; k < notes && at + k < 64; ++k) w.note(at + k, int(ch(23, at + k) * 5.0f), 2, (0.4f + 0.3f * ch(24, at + k)) * human(25, at + k));
        }
        return;
    }

    if (element == Sprinkler) {
        // A lawn sprinkler: a hi-hat ticking round (row 0), a quicker tick on the way back
        // (row 1). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        switch (in.character) {
            case SunnyAfternoon:
                for (int step = 0; step < 32; ++step) {
                    const int inBar = step % 16;
                    if (inBar < 12) {
                        if (step % 2 == 0 || ch(30, step) < 0.5f * busy * density) w.note(step, 0, 1, (step % 4 == 0 ? 0.6f : 0.4f) * human(31, step));
                    } else if (ch(32, step) < 0.4f + 0.6f * busy) {
                        w.note(step, 1, 1, 0.45f * human(33, step));
                    }
                }
                break;
            case Evening:   // a slower round, evening hush
                for (int step = 0; step < 32; step += 2)
                    if (step % 8 == 0 || ch(34, step) < (0.3f + 0.4f * density) * busy) w.note(step, step % 16 >= 12 ? 1 : 0, 1, 0.4f * human(35, step));
                break;
            default:   // Uncanny: it ticks on and on, and stutters
                for (int step = 0; step < 32; step += 2) w.note(step, 0, 1, 0.4f);
                if (ch(36, 0) < 0.4f + 0.5f * act) {
                    for (int k = 0; k < 3; ++k) w.note(25 + k, 1, 1, 0.4f);
                }
                break;
        }
        if (tension > 0.5f)
            for (int step = 1; step < 32; step += 4) w.note(step, 1, 1, 0.3f + 0.3f * tension);
        return;
    }

    if (element == MusicBox) {
        // A music-box motif from a child's room: a little tune in eighths. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 24;
        static constexpr int tune[16] { 0, 2, 4, 0, 4, 5, 4, -1, 2, 4, 2, 0, 1, 2, 0, -1 };
        float keep = 0;
        int every = 2;
        switch (in.character) {
            case SunnyAfternoon: keep = 0.25f + 0.35f * act; every = 2; break;
            case Evening: keep = 0.4f + 0.45f * act; every = 2; break;
            default: keep = 0.6f + 0.4f * act; every = 4; break;   // Uncanny: slowed right down
        }
        for (int k = 0; k < 64 / every; ++k) {
            const int d = tune[k % 16];
            if (d < 0 || (ch(40, k) >= keep * lerp(0.8f, 1.2f, density) && k % 8 != 0)) continue;
            w.note(k * every, d, every, (k % 4 == 0 ? 0.6f : 0.45f) * human(41, k));
        }
        if (tension > 0.6f) w.note(60, fifth - 1, 4, 0.4f);   // a note that is not in the tune
        return;
    }

    if (element == Bass) {
        // A mellow bass, two octaves below, on the chord. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = -24;
        switch (in.character) {
            case SunnyAfternoon:
                for (int beat = 0; beat < 8; ++beat) {
                    if (beat % 2 == 1 && ch(50, beat) >= 0.4f + 0.5f * busy) continue;
                    w.note(beat * 4, beat % 4 == 3 ? 4 : 0, 3, (beat % 2 == 0 ? 0.8f : 0.6f) * human(51, beat));
                }
                if (ch(52, 0) < act * density) w.note(14, 4, 2, 0.55f);
                break;
            case Evening:
                w.note(0, 0, 12, 0.75f);
                w.note(12, ch(53, 0) < 0.5f ? 4 : 2, 4, 0.6f);
                w.note(16, 0, 14, 0.7f);
                break;
            default:
                w.note(0, 0, 30, 0.7f);
                break;
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void SuburbsPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const bool sunny = s.character == SunnyAfternoon, evening = s.character == Evening, uncanny = s.character == Uncanny;
    // Uncanny: everything wavers, as if the tape were stretching.
    const float waver = uncanny ? 0.12f + 0.25f * tension : 0.0f;

    // Electric piano: a mellow FM tine, the bell of the attack fading quickly.
    {
        auto& p = piano.patch;
        p.ratio = 1.0f;
        p.index = 0.8f + 1.4f * tone + 0.6f * s.era;
        p.indexFloor = 0.1f;
        p.indexSeconds = 0.4f;
        p.feedback = 0.04f;
        p.attack = 0.002f;
        p.decay = evening ? 2.6f : (uncanny ? 3.0f : 1.4f);
        p.sustain = 0.0f;
        p.release = 0.4f;
        p.held = false;
        p.vibratoHz = uncanny ? 0.45f : 5.0f;
        p.vibratoDepth = waver;
        p.spread = 0.5f;
        p.voices = byQuality(q, 4, 6, 8);
        // A mellow tine electric piano, played softly.
        p.model = FmTine;
        p.brightness = clamp01(0.1f + 0.4f * tone + 0.2f * s.era);
        p.gain = 1.0f;
    }
    // The pop pulse: a soft plucky synth.
    {
        auto& p = pulse.patch;
        p.held = true;
        p.legato = false;
        p.attack = 0.004f;
        p.decay = 0.18f;
        p.sustain = 0.2f;
        p.release = 0.12f;
        p.detune = 6.0f;
        p.sub = 0.2f;
        p.cutoff = lerpExp(400.0f, 1800.0f, tone) * (evening ? 0.7f : 1.0f);
        p.envOctaves = 0.4f;
        p.filterEnvOctaves = 1.6f;
        p.filterDecay = 0.08f;
        p.q = 0.8f;
        p.width = 0.4f;
        p.vibratoHz = 0.5f;
        p.vibratoDepth = waver;
        p.spread = 0.4f;
        p.voices = 4;
        p.gain = 1.0f;
    }
    // Wind chimes: thin tubes, their upper partials a quarter-tone off any note.
    {
        auto& p = chimes.patch;
        p.set({ 1.0f, 2.76f, 5.4f }, { 1.0f, 0.5f, 0.3f }, { 1.0f, 0.3f * (0.4f + tone), 0.15f * tone });
        p.decay = evening ? 2.8f : 2.0f;
        p.hardness = 0.5f + 0.3f * tone;
        p.toneHz = lerpExp(4000.0f, 13000.0f, tone);
        p.spread = 0.8f;
        p.voices = byQuality(q, 4, 5, 6);
        p.gain = 1.0f;
    }
    // The sprinkler: a closed hi-hat and a tick.
    {
        auto& p = sprinkler.patch;
        auto& hat = p.rows[0];
        hat = {};
        hat.tuned = false;
        hat.hz = 100;
        hat.tone = 0.0f;
        hat.noise = 1.0f;
        hat.noiseHz = lerpExp(3000.0f, 7500.0f, tone);
        hat.noiseQ = 1.0f;
        hat.noiseDecay = 0.09f;
        hat.pan = 0.4f;
        auto& tick = p.rows[1];
        tick = {};
        tick.tuned = false;
        tick.hz = 100;
        tick.tone = 0.0f;
        tick.noise = 0.9f;
        tick.noiseHz = lerpExp(2500.0f, 5500.0f, tone);
        tick.noiseQ = 2.0f;
        tick.noiseDecay = 0.04f;
        tick.pan = -0.4f;
        p.spread = 0.2f;
        p.voices = 4;
        p.gain = 1.0f;
    }
    // Music box: small hard tines, the upper partial a quarter-tone off any note.
    {
        auto& p = box.patch;
        p.set({ 1.0f, 2.0f, 5.19f }, { 1.0f, 0.4f, 0.2f }, { 1.0f, 0.2f, 0.25f * (0.4f + tone) });
        p.decay = uncanny ? 2.2f : 1.4f;
        p.hardness = 0.75f + 0.2f * tone;
        p.toneHz = lerpExp(3000.0f, 11000.0f, tone);
        p.spread = 0.3f;
        p.pan = -0.2f;
        p.voices = byQuality(q, 3, 4, 5);
        p.gain = 1.0f;
    }
    // A mellow bass: rounded saws, low-passed.
    {
        auto& p = bass.patch;
        p.held = true;
        p.legato = false;
        p.attack = 0.01f;
        p.decay = 0.4f;
        p.sustain = 0.6f;
        p.release = 0.15f;
        p.detune = 4.0f;
        p.sub = 0.6f;
        p.cutoff = lerpExp(100.0f, 400.0f, tone) * (0.8f + 0.5f * s.era);
        p.envOctaves = 0.2f;
        p.filterEnvOctaves = 0.8f;
        p.filterDecay = 0.15f;
        p.q = 0.7f;
        p.width = 0.2f;
        p.vibratoHz = 0.45f;
        p.vibratoDepth = waver * 0.6f;
        p.spread = 0.05f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Tension: the music box and the sprinkler come forward, something is off; Life brings
    // the breeze; Machinery runs the sprinklers.
    level = { 0.2f * (uncanny ? 0.8f : 1.0f), 0.12f * (sunny ? 1.2f : (evening ? 0.8f : 0.6f)), 0.12f * lerp(0.5f, 1.4f, s.life),
              2.2f * (0.5f + 1.0f * tension) * (0.6f + 1.0f * s.machinery) * (evening ? 0.7f : 1.0f),
              0.12f * (uncanny ? 1.6f : (sunny ? 0.8f : 1.0f)) * (0.6f + 1.0f * tension), 0.22f };

    // Warm tape (stretched further when uncanny) in a short room.
    tape.set((sunny ? 0.35f : (evening ? 0.5f : 0.85f)) * (0.6f + 0.6f * s.fxDepth) + 0.2f * tension * (uncanny ? 1.0f : 0.0f));
    room.set(lerp(0.12f, 0.45f, s.size), lerp(0.5f, 1.3f, s.size) * (uncanny ? 1.5f : 1.0f) * (1.0f + 0.3f * s.magic), lerpExp(2500.0f, 8000.0f, tone));
    roomSend = (uncanny ? 0.5f : 0.35f) + 0.2f * s.fxDepth;
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era) * (evening ? 0.75f : 1.0f), rate);
    // Tone and Era also lift or soften the presence above about 2 kHz.
    presence = 1.2f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
}

bool SuburbsPlace::shape(const PlaceContext&, const Scheduled& x, NoteOn& on) {
    // The chimes hang on a porch to one side.
    if (x.element == WindChimes) {
        on.hasPan = true;
        on.pan = 0.5f + 0.35f * (2.0f * on.random - 1.0f);
    }
    return true;
}

void SuburbsPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        tape.process(l, r);
        float wl = 0, wr = 0;
        room.process(l * roomSend, r * roomSend, wl, wr);
        l += 0.6f * wl;
        r += 0.6f * wr;
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
