#include "Places.h"

namespace wb {

using namespace dsp;

MallPlace::MallPlace() : EnsemblePlace(2500) { setParts({ &piano, &vibes, &chimes, &escalator, &fountain, &drums }); }

void MallPlace::prepareFx() {
    hall.prepare(rate);
    detune.prepare(rate);
}

void MallPlace::resetFx() {
    hall.clear();
    detune.clear();
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
    motorPhase = 0;
}

void MallPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2501);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == EPiano) {
        // Smooth electric-piano chords on the shared chord: sevenths and ninths. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -2;
        const auto chord = [&](int step, int length, float velocity, bool ninth) {
            w.note(step, 0, length, velocity);
            w.note(step, 2, length, velocity * 0.85f);
            w.note(step, 4, length, velocity * 0.8f);
            w.note(step, 6, length, velocity * 0.75f);
            if (ninth) w.note(step, size + 1, length, velocity * 0.6f);
        };
        switch (in.character) {
            case BusyWeekend:   // a light, syncopated comp
                for (int bar = 0; bar < 4; ++bar) {
                    chord(bar * 16, 5, 0.6f * human(1, bar), false);
                    for (int k : { 6, 10, 14 })
                        if (ch(2, bar * 16 + k) < (0.3f + 0.5f * density) * busy) chord(bar * 16 + k, 3, 0.45f * human(3, bar * 16 + k), ch(4, bar * 16 + k) < 0.4f);
                }
                break;
            case EmptyNight:   // long ninth chords
                for (int bar = 0; bar < 4; bar += 2) {
                    chord(bar * 16, 26, 0.55f * human(5, bar), true);
                    if (ch(6, bar) < act * 0.8f) chord(bar * 16 + 26, 6, 0.4f, false);
                }
                break;
            default:   // Dead Mall: one slowed chord a bar, held, a lazy echo
                for (int bar = 0; bar < 4; ++bar) {
                    chord(bar * 16, 12, 0.5f * human(7, bar), true);
                    if (ch(8, bar) < 0.2f + 0.6f * act) chord(bar * 16 + 12, 4, 0.35f, false);
                }
                break;
        }
        return;
    }

    if (element == Vibraphone) {
        // Vibraphone melodies: a running line in the scale, phrases that rest. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        float keep = 0;
        int every = 2;
        switch (in.character) {
            case BusyWeekend: keep = 0.4f + 0.5f * act; every = 2; break;
            case EmptyNight: keep = 0.2f + 0.4f * act; every = 4; break;
            default: keep = 0.3f + 0.5f * act; every = 4; break;
        }
        // A wandering line: steps and small leaps, resting at the end of each phrase.
        int d = 4;
        for (int k = 0; k < 64 / every; ++k) {
            const int step = k * every, inPhrase = step % 32;
            const float move = ch(10, k);
            d += move < 0.35f ? -1 : (move < 0.7f ? 1 : (move < 0.85f ? 2 : -2));
            d = std::clamp(d, 0, size + 4);
            if (inPhrase >= 26) continue;
            if (inPhrase != 0 && ch(11, k) >= keep * lerp(0.7f, 1.3f, density)) continue;
            w.note(step, d, every * (inPhrase >= 20 ? 3 : 1), (step % 8 == 0 ? 0.6f : 0.45f) * human(12, k));
        }
        if (in.character == DeadMall && tension > 0.5f) w.note(56, fifth, 8, 0.4f);
        return;
    }

    if (element == PAChimes) {
        // The PA chime before an announcement: three notes falling (or rising), every
        // few bars. Eight bars.
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = false;
        const auto ding = [&](int at, bool down, float velocity) {
            const int notes[3] { down ? size + 2 : 0, down ? size : 2, down ? 4 : 4 };
            for (int k = 0; k < 3; ++k) w.note(at + k * (in.character == DeadMall ? 4 : 3), notes[k] + (down ? 0 : size), 4, velocity * (k == 2 ? 0.9f : 1.0f));
        };
        float chanceOf = 0;
        switch (in.character) {
            case BusyWeekend: chanceOf = 0.35f + 0.4f * act; break;
            case EmptyNight: chanceOf = 0.1f + 0.25f * act; break;
            default: chanceOf = 0.1f + 0.2f * act; break;
        }
        ding(8, true, 0.7f);   // one announcement always
        for (int bar = 2; bar < 8; bar += 2)
            if (ch(20, bar) < chanceOf * lerp(0.7f, 1.3f, density)) ding(bar * 16 + 8, ch(21, bar) < 0.6f, 0.6f * human(22, bar));
        if (tension > 0.6f) ding(120, false, 0.5f);
        return;
    }

    if (element == Escalator) {
        // The escalator's hum: a low drone on the root and fifth, held all four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        w.note(0, 0, 64, 0.8f);
        w.note(0, fifth, 64, in.character == DeadMall ? 0.65f : 0.45f);
        return;
    }

    if (element == Fountain) {
        // Fountain shimmer: high tuned spray on the chord's notes, two bars each.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 12;
        w.note(0, 0, 32, 0.7f);
        w.note(0, 4, 32, 0.55f);
        w.note(32, 2, 32, 0.6f);
        w.note(32, 4, 32, 0.5f);
        if (in.character == EmptyNight) w.note(0, size, 64, 0.45f);
        return;
    }

    if (element == Drums) {
        // A soft drum groove: kick (row 0), snare (row 1), hat (row 2). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        switch (in.character) {
            case BusyWeekend:
                for (int step = 0; step < 32; ++step) {
                    const int inBar = step % 16;
                    if (inBar == 0 || inBar == 10 || (inBar == 7 && ch(30, step) < 0.5f * busy)) w.note(step, 0, 1, 0.75f * human(31, step));
                    if (inBar == 4 || inBar == 12) w.note(step, 1, 1, 0.6f * human(32, step));
                    if (step % 4 == 0 || (step % 2 == 0 && ch(42, step) < 0.2f + 0.7f * busy) || ch(33, step) < 0.9f * act * act * (0.5f + density)) w.note(step, 2, 1, (step % 4 == 2 ? 0.55f : 0.4f) * human(34, step));
                }
                break;
            case EmptyNight:   // brushes: a rim on two and four, a soft hat
                for (int step = 0; step < 32; step += 2) {
                    const int inBar = step % 16;
                    if (inBar == 0 && ch(35, step) < 0.4f + 0.5f * act) w.note(step, 0, 1, 0.55f);
                    if (inBar == 4 || inBar == 12) w.note(step, 1, 1, 0.4f * human(36, step));
                    if (ch(37, step) < (0.4f + 0.4f * density) * busy) w.note(step, 2, 1, 0.35f * human(38, step));
                }
                break;
            default:   // Dead Mall: half time, slowed
                w.note(0, 0, 1, 0.75f);
                w.note(16, 1, 1, 0.6f);
                if (ch(39, 0) < 0.3f + 0.6f * act) w.note(22, 0, 1, 0.55f);
                for (int step = 0; step < 32; step += 4)
                    if (ch(40, step) < 0.4f + 0.6f * busy) w.note(step + 2, 2, 1, 0.35f * human(41, step));
                break;
        }
        if (tension > 0.5f)
            for (int step = 14; step < 32; step += 16) w.note(step, 1, 1, 0.3f + 0.3f * tension);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void MallPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool busyWeekend = s.character == BusyWeekend, empty = s.character == EmptyNight, dead = s.character == DeadMall;

    // Electric piano: a glassy FM tine, a soft bell on the attack.
    {
        auto& p = piano.patch;
        p.ratio = 1.0f;
        p.index = 0.9f + 1.5f * tone + 0.6f * s.era;
        p.indexFloor = 0.12f;
        p.indexSeconds = 0.35f;
        p.feedback = 0.05f;
        p.attack = dead ? 0.02f : 0.002f;
        p.decay = empty ? 2.8f : (dead ? 3.2f : 1.5f);
        p.sustain = 0.0f;
        p.release = 0.5f;
        p.held = false;
        p.vibratoHz = dead ? 0.4f : 5.0f;
        p.vibratoDepth = dead ? 0.1f + 0.15f * tension : 0.0f;
        p.spread = 0.5f;
        p.voices = byQuality(q, 5, 7, 8);
        p.gain = 1.0f;
    }
    // Vibraphone: a metal bar (its fourth partial, the bar's hard ring), long sustain.
    {
        auto& p = vibes.patch;
        p.set({ 1.0f, 3.99f, 10.1f }, { 1.0f, 0.4f, 0.12f }, { 1.0f, 0.3f * (0.4f + tone), 0.08f * tone });
        p.decay = dead ? 3.5f : 2.4f;
        p.hardness = 0.4f + 0.35f * tone;
        p.toneHz = lerpExp(3000.0f, 11000.0f, tone);
        p.transpose = 12.0f;
        p.spread = 0.4f;
        p.pan = 0.15f;
        p.voices = byQuality(q, 4, 5, 6);
        p.gain = 1.0f;
    }
    // PA chimes: a clean, round chime, the store's announcement ding.
    {
        auto& p = chimes.patch;
        p.set({ 1.0f, 2.0f, 3.01f, 4.2f }, { 1.0f, 0.5f, 0.3f, 0.15f }, { 1.0f, 0.25f, 0.12f * (0.4f + tone), 0.05f * tone });
        p.decay = dead ? 3.0f : 1.8f;
        p.hardness = 0.3f + 0.3f * tone;
        p.toneHz = lerpExp(2500.0f, 9000.0f, tone);
        p.transpose = 12.0f;
        p.spread = 0.1f;
        p.pan = -0.1f;
        p.voices = 3;
        p.gain = 1.0f;
    }
    // The escalator: a low motor hum whose steps tick round on the beat.
    {
        auto& p = escalator.patch;
        p.held = p.legato = true;
        p.attack = 1.5f;
        p.release = 1.5f;
        p.glide = 1.0f;
        p.detune = dead ? 14.0f : 5.0f;
        p.sub = 0.5f;
        p.cutoff = lerpExp(160.0f, 520.0f, tone) * (1.0f + 0.6f * s.machinery) * std::exp2(0.2f * drift.smooth(t, 0.05));
        p.envOctaves = 0.0f;
        p.q = 1.2f;
        p.width = 0.4f;
        p.tremoloPerBeat = 4.0f;
        p.tremoloDepth = 0.15f + 0.25f * s.machinery;
        p.transpose = -24.0f;
        p.spread = 0.2f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // The fountain: high tuned spray (breath alone) that glitters as the water falls.
    {
        auto& p = fountain.patch;
        p.tone = 0.0f;
        p.breath = 1.0f;
        p.breathQ = lerp(4.0f, 9.0f, tone);
        p.attack = 0.8f;
        p.release = 1.5f;
        p.scoop = 0.0f;
        p.vibratoDepth = 0.0f;
        p.transpose = 12.0f;
        p.spread = 0.9f;
        p.voices = 4;
        p.held = true;
        p.gain = 0.7f + 0.5f * (0.5f + 0.5f * noise.smooth(t * 3.0, 0.9));
    }
    // A soft groove: a round kick on the root, a brushed snare, a hat.
    {
        auto& p = drums.patch;
        auto& kick = p.rows[0];
        kick = {};
        kick.semis = -24;
        kick.bend = 1.6f;
        kick.decay = dead ? 0.6f : 0.35f;
        kick.tone = 1.0f;
        kick.noise = 0.04f;
        kick.noiseHz = 600;
        kick.noiseDecay = 0.02f;
        kick.ratio = 1.5f;
        kick.partial = 0.1f;
        kick.kind = dsp::DrumMembrane;
        kick.position = 0.15f;
        kick.hardness = 0.6f;
        auto& snare = p.rows[1];
        snare = {};
        snare.semis = float(music::degree(frame.mood, 4));
        snare.bend = 1.05f;
        snare.decay = 0.12f;
        snare.tone = 0.25f;
        snare.noise = empty ? 0.5f : 0.8f;
        snare.noiseHz = lerpExp(1500.0f, 3500.0f, tone);
        snare.noiseQ = 0.8f;
        snare.noiseDecay = empty ? 0.18f : 0.12f;
        snare.ratio = 1.6f;
        snare.partial = 0.3f;
        snare.pan = 0.1f;
        snare.kind = dsp::DrumMembrane;
        snare.position = 0.4f;
        snare.hardness = 0.2f;
        snare.snare = 0.6f;
        auto& hat = p.rows[2];
        hat = {};
        hat.tuned = false;
        hat.hz = 100;
        hat.tone = 0.0f;
        hat.noise = 0.6f;
        hat.noiseHz = lerpExp(4000.0f, 9000.0f, tone);
        hat.noiseQ = 1.0f;
        hat.noiseDecay = 0.05f;
        hat.pan = -0.3f;
        p.spread = 0.15f;
        p.voices = 5;
        p.gain = 1.0f;
    }
    // Tension: the escalator grinds louder, the drums push and the piano retreats; Life
    // fills the fountain and the vibraphone; Machinery runs the escalators.
    level = { 0.2f * (1.0f - 0.35f * tension), 0.28f * lerp(0.7f, 1.3f, s.life) * (empty ? 1.2f : 1.0f), 0.3f * (busyWeekend ? 1.1f : 1.0f),
              0.1f * (0.5f + 1.3f * s.machinery) * (0.7f + 0.9f * tension) * (empty ? 1.3f : 1.0f), 1.0f * lerp(0.6f, 1.4f, s.life) * (empty ? 1.3f : (dead ? 1.5f : 1.0f)),
              0.45f * (busyWeekend ? 1.0f : (empty ? 1.1f : 0.75f)) * (0.7f + 0.7f * tension) };

    // The vibraphone's motor: a slow tremolo (slower still when the mall is dead).
    motorHz = (dead ? 2.0f : 5.0f) * (0.8f + 0.4f * s.activity);
    motorDepth = 0.35f + 0.2f * s.fxDepth;
    // Glossy reverb: a big bright atrium (a dim, long hall when the mall is dead).
    hall.set(lerp(0.5f, 1.0f, s.size), lerp(1.6f, 3.6f, s.size) * (dead ? 1.6f : 1.0f) * (1.0f + 0.3f * s.magic),
             lerpExp(3000.0f, 11000.0f, tone) * (dead ? 0.55f : 1.0f));
    hallSend = (dead ? 0.6f : 0.4f) + 0.2f * s.fxDepth;
    dry = lerp(1.0f, 0.7f, s.size);
    // Slight detune, each side its own way; Dead Mall: the vaporwave slowdown, flat and wobbling.
    if (dead) detune.set(-28.0f, -22.0f, 0.75f, 9.0f + 10.0f * tension, 0.35f);
    else detune.set(-7.0f, 7.0f, (busyWeekend ? 0.3f : 0.4f) * (0.6f + 0.6f * s.fxDepth), 0.0f, 0.3f);
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era) * (dead ? 0.55f : 1.0f) * (1.0f - 0.3f * tension), rate);
    // Tone and Era also lift or soften the presence above about 2 kHz.
    presence = 1.2f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
}

bool MallPlace::shape(const PlaceContext&, const Scheduled& x, NoteOn& on) {
    // The PA speakers hang overhead on both sides; the vibraphone plays across the atrium.
    if (x.element == PAChimes) {
        on.hasPan = true;
        on.pan = 0.3f * (2.0f * on.random - 1.0f);
    }
    return true;
}

void MallPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        motorPhase += motorHz / rate;
        if (motorPhase > 1.0f) motorPhase -= 1.0f;
        const float motor = 1.0f - motorDepth * (0.5f + 0.5f * std::sin(twoPi * motorPhase));
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            const float g = e == Vibraphone ? motor : 1.0f;
            l += busL[std::size_t(e)][k] * g;
            r += busR[std::size_t(e)][k] * g;
        }
        detune.process(l, r);
        float wl = 0, wr = 0;
        hall.process(l * hallSend, r * hallSend, wl, wr);
        l = dry * l + 0.7f * wl;
        r = dry * r + 0.7f * wr;
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
