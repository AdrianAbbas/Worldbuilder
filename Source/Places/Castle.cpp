#include "Places.h"

namespace wb {

using namespace dsp;

CastlePlace::CastlePlace() : EnsemblePlace(1100) { setParts({ &drone, &bells, &horns, &timpani, &harp, &choir }); }

void CastlePlace::prepareFx() {
    hall.prepare(rate);
    echo.prepare(rate, 2.5f);
}

void CastlePlace::resetFx() {
    hall.clear();
    echo.clear();
}

void CastlePlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 1101);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size;
    const auto add = [&](int step, int degree, int length, float velocity, int alter = 0) {
        pat::Note n;
        n.step = int16_t(step);
        n.degree = int8_t(degree);
        n.alter = int8_t(alter);
        n.length = int16_t(length);
        n.velocity = uint8_t(std::clamp(int(std::lround(velocity * 127.0f)), 1, 127));
        out.add(n);
    };

    if (element == Pulse) {
        // Timpani on the root and the fifth below, a side drum for the march. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        for (int step = 0; step < out.lengthSteps; ++step) {
            const int pos = step % 16;
            float p = 0, vel = 0.8f;
            int kind = 0;   // kit row: 0 timpani root, 1 timpani fifth, 2 side drum
            switch (in.character) {
                case 0:   // Fortress: a slow processional, half notes, rolls when busy
                    if (pos == 0) { p = 0.97f; vel = 1.0f; }
                    else if (pos == 8) { p = 0.25f + 0.6f * density; kind = 1; }
                    else if (pos == 4 || pos == 12) { p = tension * density * 0.9f; vel = 0.55f; kind = 1; }
                    else if (pos >= 13) { p = act * density * 0.35f; vel = 0.35f; }
                    else if ((pos & 1) == 0) { p = std::max(0.0f, tension - 0.45f) * 1.4f * density; vel = 0.4f; }   // tension: an eighth-note pulse
                    break;
                case 1:   // Royal Court: a march
                    if (pos == 0) { p = 0.97f; vel = 1.0f; }
                    else if (pos == 8) { p = 0.85f; vel = 0.8f; kind = 1; }
                    else if (pos == 4 || pos == 12) { p = 0.85f; vel = 0.7f; kind = 2; }
                    else if (pos == 14 || pos == 6) { p = density * 0.45f * busy; vel = 0.35f; kind = 2; }
                    else if (pos == 15) { p = density * 0.35f * busy; vel = 0.3f; kind = 2; }
                    break;
                default:   // Besieged: a driving 3-3-2 ostinato
                    if (pos == 0 || pos == 6 || pos == 12) { p = 0.95f; vel = 1.0f; kind = pos == 6 ? 1 : 0; }
                    else if ((pos & 1) == 0) { p = (0.3f + 0.6f * density) * busy; vel = 0.5f; }
                    else { p = tension * density * 0.6f * busy; vel = 0.3f; }
                    if ((pos == 4 || pos == 12) && tension > 0.35f) { kind = 2; p = std::max(p, tension); vel = 0.6f; }
                    break;
            }
            if (p > 0 && ch(1, step) < p) add(step, kind, 1, vel * (0.85f + 0.3f * (ch(2, step) - 0.5f) * (0.4f + life)));
        }
        return;
    }

    if (element == Bells) {
        out.setGrid(pat::Sixteenth, 8);
        // The carillon is written in the key, the tolls and alarms on the chords.
        out.followChords = in.character != 1;
        out.base = 12;
        for (int bar = 0; bar < 8; ++bar) {
            const int at = bar * 16;
            switch (in.character) {
                case 0:   // Fortress: a slow toll, and now and then a higher bell
                    // (An eight-bar pattern, so a little denser than the old open-ended dice.)
                    if ((bar & 1) == 0 && ch(5, bar) < 0.4f + 0.6f * act) add(at, 0, 4, 0.8f);
                    if (ch(7, bar) < act * 0.45f) add(at + 8, 4, 4, 0.4f);
                    break;
                case 1:   // Royal Court: carillon runs down the scale, every other bar
                    if ((bar & 1) == 0 && ch(9, bar) < 0.2f + 0.6f * act) {
                        const int top = 7 + int(ch(10, bar) * 3.0f), length = 5 + int(ch(11, bar) * 4.0f);
                        const bool upward = ch(12, bar) < 0.3f;
                        for (int k = 0; k < length; ++k) {
                            const int d = upward ? top - length + 1 + k : top - k;
                            add(at + 2 * k, d + size, 2, k == length - 1 ? 0.8f : 0.55f);
                        }
                    }
                    break;
                default:   // Besieged: an alarm bell, faster with tension
                    if (tension > 0.3f) {
                        const int every = tension > 0.7f ? 2 : 4;
                        for (int pos = 0; pos < 16; pos += every)
                            if (ch(13, at + pos) < (tension - 0.2f) * (0.4f + 0.6f * act)) add(at + pos, 4, 2, 0.45f);
                    } else if ((bar & 1) == 0 && ch(14, bar) < 0.45f + 0.5f * act) {
                        add(at, 0, 4, 0.7f);
                    }
                    break;
            }
        }
        return;
    }

    if (element == Horns) {
        // Horn calls on even bars, when the castle is busy enough. Eight bars.
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = true;
        out.base = -3;
        for (int bar = 0; bar < 8; bar += 2) {
            const float p = (0.15f + 0.45f * act) * (in.character == 1 ? 1.3f : 1.0f) * lerp(0.6f, 1.5f, life);
            if (ch(15, bar) >= p) continue;
            // Life: a lively grace note from a scale step above leads into the call.
            const bool grace = life > 0.35f && ch(17, bar) < life;
            const int at = bar * 16 + (grace ? 1 : 0);
            const float variant = ch(16, bar);
            int first = 0;
            switch (in.character) {
                case 0:
                    if (variant < 0.5f) {
                        add(at, 0, 6, 1.0f);
                        add(at + 8, 4, 12, 1.0f);
                    } else {
                        first = 4;
                        add(at, 4, 4, 1.0f);
                        add(at + 4, 0, 2, 0.8f);
                        add(at + 6, 0, 12, 1.0f);
                    }
                    break;
                case 1:   // a fanfare in thirds: the second horn a third below
                    for (int shift : { 0, -2 }) {
                        add(at, shift, 2, 0.9f);
                        add(at + 3, shift, 1, 0.7f);
                        add(at + 4, 2 + shift, 4, 1.0f);
                        add(at + 8, 4 + shift, 10, 1.0f);
                    }
                    break;
                default:   // a low, uneasy call: a semitone above the root when tense
                    first = -size;
                    add(at, -size, 6, 1.0f);
                    if (tension > 0.5f) add(at + 6, -size, 2, 0.85f, 1);
                    else add(at + 6, -size + 1, 2, 0.85f);
                    add(at + 8, -size, 10, 1.0f);
                    break;
            }
            if (grace) add(at - 1, first + 1, 1, 0.6f);
        }
        return;
    }
    if (element == Drone) {
        // Held chord tones over four bars (one chord), gliding into the next chord.
        // The part adds the tonic pedal underneath.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        static constexpr int base[3] { -5, -2, -10 };
        out.base = int8_t(base[std::clamp(in.character, 0, 2)]);
        const int first = in.character == 1 ? 2 : 0;
        add(0, first, 64, 0.85f);
        // The upper voice fades to a semitone above the chord root as Tension rises.
        if (tension > 0.6f) add(0, 0, 64, 0.7f, 1);
        else add(0, 4, 64, 0.7f);
        return;
    }

    if (element == Harp) {
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        switch (in.character) {
            case 0:   // Fortress: rolled chords rising, every other bar (and between when busy)
                out.base = 0;
                for (int bar = 0; bar < 4; ++bar) {
                    const bool main = (bar & 1) == 0;
                    if (ch(30, bar) >= (main ? 0.45f + 0.5f * act : act * 0.5f * density)) continue;
                    const int at = bar * 16 + (main ? 0 : 8);
                    const int top = ch(31, bar) < 0.5f ? 4 : 2;
                    static constexpr int roll[5] { 0, 2, 4, 7, 9 };
                    for (int k = 0; k < top + 1 && k < 5; ++k) add(at + k, roll[k], 12 - k, (0.45f + 0.08f * float(k)) * (0.9f + 0.2f * ch(32, at + k)));
                }
                break;
            case 1: {   // Royal Court: flowing up-and-down arpeggios in eighths, sixteenths when dense
                out.base = 0;
                static constexpr int line[8] { 0, 2, 4, 7, 9, 7, 4, 2 };
                for (int step = 0; step < 64; ++step) {
                    const bool eighth = (step & 1) == 0;
                    const float p = eighth ? 0.55f + 0.45f * busy : density * 0.45f * busy;
                    if (ch(33, step) >= std::min(p, 0.98f)) continue;
                    const int degree = line[(step / 2) % 8] + ((step / 16) % 2 == 1 && ch(34, step) < 0.3f ? 2 : 0);
                    add(step, degree, 2, (step % 8 == 0 ? 0.7f : 0.5f) * (1.0f + (ch(35, step) - 0.5f) * (0.2f + 0.4f * life)));
                }
                break;
            }
            default:   // Besieged: a low, tense ostinato; a trembling semitone when tense
                out.base = -12;
                for (int step = 0; step < 64; ++step) {
                    const int pos = step % 16;
                    const bool eighth = (pos & 1) == 0;
                    const float p = eighth ? (0.3f + 0.6f * density) * busy : tension * density * 0.7f * busy;
                    if (ch(36, step) >= p) continue;
                    const bool upper = (pos % 4) == 2;
                    add(step, upper ? (tension > 0.5f ? 0 : 1) : 0, 1, pos % 4 == 0 ? 0.75f : 0.5f, upper && tension > 0.5f ? 1 : 0);
                }
                break;
        }
        return;
    }

    if (element == Choir) {
        // A wordless pad on the chords: open fifths, full triads, or uneasy swells.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        switch (in.character) {
            case 0:
                out.base = 0;
                add(0, 0, 64, 0.6f);
                add(0, 4, 64, 0.55f);
                if (act > 0.6f) add(0, 7, 64, 0.35f);
                break;
            case 1:
                out.base = 0;
                add(0, 0, 64, 0.55f);
                add(0, 2, 64, 0.5f);
                add(0, 4, 64, 0.5f);
                break;
            default:
                out.base = -5;
                for (int at : { 0, 32 }) {
                    add(at, 0, 24, 0.65f);
                    if (tension > 0.6f) add(at, 0, 24, 0.5f, 1);
                    else add(at, 4, 24, 0.5f);
                }
                break;
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}


void CastlePlace::configure(const PlaceContext& c) {
    const auto& w = *c.world;
    const auto& s = c.s;
    const int q = frame.quality, mood = w.mood;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const float breath = drift.smooth(t, 0.05);

    // Brass drone: three detuned saws per note on the chord, a tonic pedal below.
    {
        auto& p = drone.patch;
        p.attack = 2.5f;
        p.release = 2.5f;
        p.held = p.legato = true;
        p.glide = 1.4f;
        p.detune = 5.0f + 4.0f * s.era + 3.0f * s.life;
        p.sub = 0.25f;
        p.cutoff = lerpExp(160.0f, 900.0f, tone) * std::exp2(0.6f * breath + 0.8f * s.era) * (s.character == 1 ? 1.3f : 1.0f) * lerp(1.2f, 0.75f, tension);
        p.envOctaves = 0.6f;
        p.q = 0.8f;
        p.width = 0.7f;
        p.pedal = 0.9f;
        p.pedalSemis = -12;
        p.spread = 0.3f;
        // Besieged trembles, more with Tension.
        // Every castle trembles once Tension passes half way.
        p.tremoloPerBeat = s.character == 2 ? 1.0f : 2.0f;
        p.tremoloDepth = s.character == 2 ? 0.6f * tension : 0.8f * std::max(0.0f, tension - 0.5f);
        p.voices = 4;
        // A section of low horns and trombones (two players a few cents apart).
        p.model = dsp::SynthBrass;
        p.cutoff = lerpExp(550.0f, 1100.0f, tone) * lerp(0.9f, 1.3f, tension) * (s.character == 1 ? 1.15f : 1.0f);
        p.formantHz = 520.0f;
        p.formantLevel = 0.5f;
        p.breath = 0.03f;
        p.gain = 1.0f;
    }
    // Bells: partials tuned to the key (hum an octave down, prime, the scale's own
    // third and fifth, nominal and upper fifth).
    {
        auto& p = bells.patch;
        const float third = std::exp2(float(music::degree(mood, 2)) / 12.0f), fifth = std::exp2(float(music::degree(mood, 4)) / 12.0f);
        p.set({ 0.5f, 1.0f, third, fifth, 2.0f, 2.0f * fifth }, { 1.4f, 1.0f, 0.7f, 0.55f, 0.45f, 0.3f },
              { 0.35f, 1.0f, 0.5f, 0.35f, 0.55f * (0.5f + tone), 0.25f * tone });
        p.decay = lerp(0.7f, 1.3f, s.size);
        p.hardness = 0.35f + 0.5f * tone;
        p.toneHz = lerpExp(1800.0f, 7000.0f, tone);
        p.voices = byQuality(q, 3, 4, 6);
        // Cast bronze is never quite round: the lowest partials beat slowly.
        p.extras.beatHz = 0.6f;
    }
    // Horns: brassy swells that glide into each other's lines.
    {
        auto& p = horns.patch;
        p.attack = 0.09f;
        p.decay = 0.5f;
        p.sustain = 0.72f;
        p.release = 0.6f;
        p.held = p.legato = true;
        p.glide = 0.05f;
        p.detune = 4;
        p.sub = 0.2f;
        p.cutoff = lerpExp(300.0f, 1200.0f, tone);
        p.envOctaves = 1.6f;
        p.q = 0.9f;
        p.width = 0.4f;
        p.vibratoHz = 5.0f;
        p.vibratoDepth = 0.08f + 0.2f * s.life;
        p.vibratoDelay = 0.35f;
        p.spread = 0.2f;
        p.voices = 2;
        // French horns: dark, the bell turned away, a mellow formant low down.
        p.model = dsp::SynthBrass;
        p.cutoff = lerpExp(500.0f, 1000.0f, tone);
        p.formantHz = 420.0f;
        p.formantLevel = 0.7f;
        p.breath = 0.04f;
        p.gain = 0.5f;
    }
    // Timpani tuned to the root and the fifth below (tighter with Tension), a side drum,
    // and with Machinery an anvil-like ring on the firm hits.
    {
        auto& p = timpani.patch;
        auto& root = p.rows[0];
        root.semis = -12;
        root.bend = 1.05f;
        root.decay = lerp(4.0f, 1.5f, tension);
        root.tone = 0.55f;
        root.noise = 0.06f;
        root.noiseHz = 800;
        root.noiseQ = 0.7f;
        root.noiseDecay = 0.15f;
        root.ratio = 1.5f;
        root.partial = 0.35f;
        root.pan = -0.1f;
        root.kind = dsp::DrumTimpani;
        root.position = 0.25f;
        root.hardness = 0.45f;
        p.rows[1] = root;
        p.rows[1].semis = -5;
        p.rows[1].pan = 0.12f;
        auto& side = p.rows[2];
        side.tuned = false;
        side.hz = 190;
        side.bend = 1.1f;
        side.decay = 0.25f;
        side.tone = 0.12f;
        side.noise = 0.55f;
        side.noiseHz = 2600;
        side.noiseQ = 0.8f;
        side.noiseDecay = lerp(0.35f, 0.2f, tension);
        side.ratio = 2.3f;
        side.partial = 0.3f;
        side.pan = 0.15f;
        side.kind = dsp::DrumMembrane;
        side.position = 0.35f;
        side.hardness = 0.8f;
        side.snare = 0.6f;
        auto& anvil = p.rows[3];
        anvil.semis = float(music::inRange(music::chordTone(mood, c.chord, 2), 24));
        anvil.bend = 1.0f;
        anvil.decay = 2.5f;
        anvil.tone = 0.5f * s.machinery;
        anvil.noise = 0.04f;
        anvil.noiseHz = 5000;
        anvil.noiseQ = 1.0f;
        anvil.noiseDecay = 0.05f;
        anvil.ratio = 2.76f;
        anvil.partial = 0.6f;
        anvil.pan = 0.25f;
        p.voices = 5;
        p.gain = 1.0f;
    }
    // Harp: bright, ringing strings; shorter under Tension.
    {
        auto& p = harp.patch;
        p.decay = lerp(2.6f, 1.1f, tension) * (s.character == 1 ? 1.2f : 1.0f);
        p.brightness = 0.3f + 0.5f * tone;
        p.spread = 0.6f;
        p.voices = byQuality(q, 4, 6, 8);
        // A gut-strung harp plucked mid-string (round), its soundboard, open strings ringing.
        p.body = dsp::BodyHarp;
        p.shape = { 0.3f, 0.45f, 0.05f, 0.0f, 0.0f, 0.1f, 0.0f };
        p.sympathetic = 0.12f;
        p.gain = 1.0f;
    }
    // Choir: "oh" for the fortress, "ah" at court, a dark "oo" when besieged.
    {
        auto& p = choir.patch;
        static constexpr float vowels[3] { 1.0f, 2.0f, 0.3f };
        p.vowel = vowels[std::clamp(s.character, 0, 2)] + 0.8f * tone - 0.4f - 0.8f * std::max(0.0f, tension - 0.4f);
        p.vowelDrift = 0.35f;
        p.attack = lerp(2.0f, 0.8f, tension);
        p.release = 2.0f;
        p.detune = 8.0f + 6.0f * s.era;
        p.vibratoDepth = 0.08f + 0.12f * s.life;
        p.brightness = tone;
        p.spread = 0.7f;
        p.voices = byQuality(q, 2, 3, 4);
        p.gain = 1.0f;
    }
    level = { 0.11f, 0.16f, 0.22f, 0.4f, 0.6f, 0.5f };

    // The stone hall (Size, Tone, Magic) and a slow dotted-quarter tape echo whose
    // depth follows Activity and Population.
    hall.set(s.size, lerp(1.6f, 6.5f, s.size) * (1.0f + 0.4f * s.magic), lerpExp(1500.0f, 5500.0f, tone));
    const float beatSeconds = 60.0f / std::max(c.bpm, 20.0f);
    echo.set(std::min(1.5f * beatSeconds, 2.2f), 0.2f + 0.45f * s.fxDepth, lerpExp(1800.0f, 4500.0f, tone), lerp(1.0f, 0.4f, s.era));
    echoSend = 0.55f * s.fxDepth;
    hallWet = 0.64f;
    hallSend = 0.67f;
}

int CastlePlace::addNotes(const PlaceContext& c, int, std::array<Scheduled, maxScheduled>& notes, int count) {
    // Machinery: an anvil-like tuned ring on the firm timpani hits.
    if (c.s.machinery <= 0.05f) return count;
    const int scheduled = count;
    for (int i = 0; i < scheduled && count < maxScheduled; ++i) {
        const auto& x = notes[std::size_t(i)];
        if (x.element != Pulse || x.tag != 0 || x.event.velocity <= 0.5f || x.event.degree == 2) continue;
        Scheduled anvil = x;
        anvil.tag = 1;
        anvil.event.degree = 3;
        anvil.event.velocity = x.event.velocity * 1.4f;
        notes[std::size_t(count++)] = anvil;
    }
    return count;
}

bool CastlePlace::shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) {
    const auto& s = c.s;
    if (x.element == Bells) {
        const float pan = 0.7f * (on.random - 0.5f) * 2.0f;
        on.hasPan = true;
        switch (s.character) {
            case 0:
                on.decay = on.semis < 18 ? 9.0f : 6.0f;
                on.pan = pan;
                break;
            case 1:
                on.velocity *= 0.6f;
                on.decay = 4.5f;
                on.pan = 0.6f * std::sin(float(on.beat));
                break;
            default:
                on.decay = on.velocity < 0.6f ? 3.0f : 8.0f;
                on.pan = on.velocity < 0.6f ? 0.45f : -0.3f;
                break;
        }
    }
    return true;
}

void CastlePlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        // Bells, horns and harp feed the echo.
        const float sl = busL[Bells][k] + busL[Horns][k] + busL[Harp][k], sr = busR[Bells][k] + busR[Horns][k] + busR[Harp][k];
        float el = 0, er = 0, wl = 0, wr = 0;
        echo.process(sl * echoSend, sr * echoSend, el, er);
        hall.process((l + 0.5f * el) * hallSend, (r + 0.5f * er) * hallSend, wl, wr);
        left[i] = l * 0.8f + 0.6f * el + wl * hallWet;
        right[i] = r * 0.8f + 0.6f * er + wr * hallWet;
    }
}

} // namespace wb
