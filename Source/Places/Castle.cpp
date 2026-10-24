#include "Places.h"

namespace wb {

using namespace dsp;

void CastlePlace::prepare(double sampleRate) {
    rate = float(sampleRate);
    hall.prepare(rate);
    echo.prepare(rate, 2.5f);
    reset();
}

void CastlePlace::reset() {
    noiseAt = 0;
    for (auto& v : drone) v.reset();
    for (auto& v : horn) v.reset();
    for (auto& b : bell) b.reset();
    for (auto& d : drum) d.reset();
    hornRelease.fill(0);
    hall.clear();
    echo.clear();
    for (auto& f : bellTone) f.reset();
}

void CastlePlace::setSeed(uint32_t key) {
    noise.setKey(key, 1102);
    drift.setKey(key, 1103);
}

void CastlePlace::playBell(const PlaceContext& c, int semitones, float velocity, float decay, float pan) {
    const int mood = c.world->mood;
    // Bell partials tuned to the key: hum an octave down, prime, the scale's own third
    // and fifth (the "tierce" and "quint"), nominal and the upper fifth.
    const float third = std::exp2(float(music::degree(mood, 2)) / 12.0f), fifth = std::exp2(float(music::degree(mood, 4)) / 12.0f);
    const float ratios[6] { 0.5f, 1.0f, third, fifth, 2.0f, 2.0f * fifth };
    static constexpr float decays[6] { 1.4f, 1.0f, 0.7f, 0.55f, 0.45f, 0.3f };
    const float bright = c.s.tone;
    const float gains[6] { 0.35f, 1.0f, 0.5f, 0.35f, 0.55f * (0.5f + bright), 0.25f * bright };
    auto& b = bell[pickVoice(bell, bellVoices, [](const Bell& v) { return v.level(); })];
    b.start(music::noteHz(c.world->root, float(semitones)), decay * lerp(0.7f, 1.3f, c.s.size), 0.35f + 0.5f * bright, velocity, pan, rate,
            ratios, decays, gains, 6);
    b.update();
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
    out.setGrid(pat::Sixteenth, 1);
}

void CastlePlace::trigger(const PlaceContext& c, const Scheduled& x) {
    const auto& s = c.s;
    const int mood = c.world->mood, root = c.world->root;
    const auto& ev = x.event;
    const auto& pattern = *c.play[std::size_t(x.element)].pattern;
    const uint64_t clock = c.clock + uint64_t(x.offset);
    const float vel = ev.velocity;
    if (x.element == Pulse) {
        const int kind = std::clamp(ev.degree, 0, 2);
        auto& d = drum[pickVoice(drum, drums, [](const Drum& v) { return v.level(); })];
        logEvent(clock, x.element, float(kind), vel);
        if (kind == 2) {
            d.ratio = 2.3f;
            d.partial = 0.3f;
            d.start(190.0f, 1.1f, 0.25f, 0.12f * vel, 0.55f * vel, 2600.0f, 0.8f, lerp(0.35f, 0.2f, s.tension), 0.15f, rate, noiseAt);
        } else {
            // Timpani: tuned to the key, their second mode a fifth up; tighter with tension.
            d.ratio = 1.5f;
            d.partial = 0.35f;
            const float semis = kind == 0 ? -12.0f : -5.0f;
            d.start(music::noteHz(root, semis), 1.05f, lerp(4.0f, 1.5f, s.tension), 0.55f * vel, 0.06f * vel, 800.0f, 0.7f, 0.15f,
                    kind == 0 ? -0.1f : 0.12f, rate, noiseAt);
        }
        d.update(rate);
        noiseAt += 1ull << 24;
        // Machinery: an anvil-like tuned ring on the firm hits.
        if (s.machinery > 0.05f && vel > 0.5f) {
            static constexpr float ratios[4] { 1.0f, 2.76f, 5.4f, 8.93f };
            static constexpr float decays[4] { 1.0f, 0.6f, 0.4f, 0.25f };
            static constexpr float gains[4] { 1.0f, 0.6f, 0.4f, 0.25f };
            auto& b = bell[pickVoice(bell, bellVoices, [](const Bell& v) { return v.level(); })];
            b.start(music::noteHz(root, float(music::inRange(music::chordTone(mood, x.chord, 2), 24))), 2.5f, 0.9f, 0.7f * s.machinery * vel,
                    0.3f * (noise.unit(noiseAt) - 0.5f), rate, ratios, decays, gains, 4);
            b.update();
            noiseAt += 1ull << 24;
        }
        return;
    }
    const int semis = pat::semitonesFor(pattern, mood, x.chord, ev.degree, ev.alter);
    if (x.element == Bells) {
        const float pan = 0.7f * (noise.unit(noiseAt) - 0.5f) * 2.0f;
        noiseAt += 1ull << 24;
        switch (s.character) {
            case 0: playBell(c, semis, vel, semis < 18 ? 9.0f : 6.0f, pan); break;
            case 1: playBell(c, semis, vel * 0.6f, 4.5f, 0.6f * std::sin(float(ev.beat))); break;
            default: playBell(c, semis, vel, vel < 0.6f ? 3.0f : 8.0f, vel < 0.6f ? 0.45f : -0.3f); break;
        }
        logEvent(clock, x.element, float(semis), vel);
        return;
    }
    if (x.element == Horns) {
        // A free horn takes the note (they glide into each other's lines); else the first.
        int h = 0;
        for (int k = 0; k < horns; ++k)
            if (hornRelease[std::size_t(k)] <= ev.beat + 1.0e-6) {
                h = k;
                break;
            }
        auto& v = horn[std::size_t(h)];
        v.start(music::noteHz(root, float(semis)), 0.09f, 0.5f, 0.72f, rate, true);
        v.gain = 0.5f * vel;
        v.update(rate);
        hornRelease[std::size_t(h)] = ev.beat + ev.length;
        logEvent(clock, x.element, float(semis), vel);
    }
}

void CastlePlace::render(const PlaceContext& c, float* left, float* right, int n) {
    const auto& w = *c.world;
    const auto& s = c.s;
    const int mood = w.mood, root = w.root, chord = c.chord;
    const double t = double(c.clock) / rate;
    const int q = std::clamp(w.quality, 0, 2);
    bellVoices = q == 0 ? 3 : (q == 1 ? 4 : bells);
    const float tension = s.tension, tone = s.tone;

    std::array<Scheduled, maxScheduled> notes;
    const int scheduled = schedule(c, n, notes);

    // Horns hold their notes for the pattern's lengths.
    for (int h = 0; h < horns; ++h) {
        auto& v = horn[std::size_t(h)];
        if (v.active() && c.beat >= hornRelease[std::size_t(h)] && v.env.stage != Env::Release) v.release(0.6f, rate);
    }

    // Drones: a tonic pedal plus two voices on the shared chord.
    {
        int droneNotes[drones];
        droneNotes[0] = -12;
        switch (s.character) {
            case 0:
                droneNotes[1] = music::inRange(music::chordTone(mood, chord, 0), -5);
                droneNotes[2] = tension > 0.6f ? 1 : music::inRange(music::chordTone(mood, chord, 2), 2);
                break;
            case 1:
                droneNotes[1] = music::inRange(music::chordTone(mood, chord, 1), -2);
                droneNotes[2] = tension > 0.6f ? 1 : music::inRange(music::chordTone(mood, chord, 2), 4);
                break;
            default:
                droneNotes[1] = music::inRange(music::chordTone(mood, chord, 0), -10);
                droneNotes[2] = tension > 0.6f ? 1 : music::inRange(music::chordTone(mood, chord, 2), -5);
                break;
        }
        const float tremolo = s.character == 2 ? 0.7f + 0.3f * std::cos(float(c.beat * 2.0 * double(twoPi))) * tension : 1.0f;
        static constexpr float level[drones] { 0.9f, 0.6f, 0.5f };
        for (int i = 0; i < drones; ++i) {
            auto& v = drone[std::size_t(i)];
            const float hz = music::noteHz(root, float(droneNotes[i]));
            const float breath = drift.smooth(t + 17.0 * i, 0.05 + 0.02 * i);
            if (!v.active()) v.start(hz, 2.5f, 1.0f, 1.0f, rate);
            v.targetHz = hz;
            v.glide = glideFor(1.4f, rate);
            v.detuneCents = 5.0f + 4.0f * s.era + 3.0f * s.life;
            v.subLevel = i == 0 ? 0.6f : 0.25f;
            v.cutoff = lerpExp(160.0f, 900.0f, tone) * std::exp2(0.6f * breath + 0.8f * s.era) * (s.character == 1 ? 1.3f : 1.0f);
            v.envOctaves = 0.6f;
            v.q = 0.8f;
            v.width = 0.7f;
            v.vibratoDepth = 0;
            // The third voice fades out as Tension nears 60 % and returns as a semitone
            // cluster above the root, so the switch is never heard as a jump.
            const float uneasy = i == 2 ? (tension > 0.6f ? clamp01((tension - 0.6f) * 3.0f) : clamp01((0.6f - tension) * 3.0f)) : 1.0f;
            v.gain = s.element[0] * level[i] * (0.8f + 0.2f * breath) * tremolo * uneasy;
            v.update(rate);
        }
    }
    for (auto& v : horn) {
        v.glide = glideFor(0.05f, rate);
        v.detuneCents = 4;
        v.subLevel = 0.2f;
        v.cutoff = lerpExp(300.0f, 1200.0f, tone);
        v.envOctaves = 1.6f;
        v.q = 0.9f;
        v.width = 0.4f;
        v.vibratoHz = 5.0f;
        v.vibratoDepth = 0.08f + 0.2f * s.life;
        v.vibratoDelay = 0.35f;
        if (v.active()) v.update(rate);
    }
    for (auto& d : drum)
        if (d.active()) d.update(rate);
    for (int i = 0; i < bellVoices; ++i)
        if (bell[std::size_t(i)].active) bell[std::size_t(i)].update();

    // Effects: the stone hall (Size, Tone, Magic) and a slow dotted-quarter tape echo
    // whose depth follows Activity and Population.
    hall.set(s.size, lerp(1.6f, 6.5f, s.size) * (1.0f + 0.4f * s.magic), lerpExp(1500.0f, 5500.0f, tone));
    const float beatSeconds = 60.0f / std::max(c.bpm, 20.0f);
    echo.set(std::min(1.5f * beatSeconds, 2.2f), 0.2f + 0.45f * s.fxDepth, lerpExp(1800.0f, 4500.0f, tone), lerp(1.0f, 0.4f, s.era));
    for (auto& f : bellTone) f.setCutoff(lerpExp(1800.0f, 7000.0f, tone), rate);
    const float echoSend = 0.55f * s.fxDepth, hallWet = s.element[4] * 0.8f, hallSend = 0.35f + 0.4f * s.element[4];
    const float droneLevel = 0.11f, bellLevel = 0.16f * s.element[1], hornLevel = 0.22f * s.element[2], drumLevel = 0.4f * s.element[3];

    // Render up to each scheduled note, then start it: notes begin on their exact sample.
    int done = 0;
    for (int next = 0; next <= scheduled; ++next) {
        const int until = next < scheduled ? notes[std::size_t(next)].offset : n;
        for (int i = done; i < until; ++i) {
            float dl = 0, dr = 0;
            for (auto& v : drone) v.tick(dl, dr);
            dl *= droneLevel;
            dr *= droneLevel;
            float hl = 0, hr = 0;
            for (auto& v : horn)
                if (v.active()) v.tick(hl, hr);
            hl *= hornLevel;
            hr *= hornLevel;
            float bl = 0, br = 0;
            for (int k = 0; k < bellVoices; ++k) {
                auto& b = bell[std::size_t(k)];
                if (!b.active) continue;
                const float y = b.tick();
                bl += y * b.pan.l;
                br += y * b.pan.r;
            }
            bl = bellTone[0].lp(bl) * bellLevel;
            br = bellTone[1].lp(br) * bellLevel;
            float pl = 0, pr = 0;
            for (auto& d : drum) {
                if (!d.active()) continue;
                const float y = d.tick(noise);
                pl += y * d.pan.l;
                pr += y * d.pan.r;
            }
            pl *= drumLevel;
            pr *= drumLevel;

            float el = 0, er = 0, wl = 0, wr = 0;
            echo.process((bl + hl) * echoSend, (br + hr) * echoSend, el, er);
            const float l = dl + hl + bl + pl, r = dr + hr + br + pr;
            hall.process((l + 0.5f * el) * hallSend, (r + 0.5f * er) * hallSend, wl, wr);
            left[i] = l * lerp(1.0f, 0.75f, s.element[4]) + 0.6f * el + wl * hallWet;
            right[i] = r * lerp(1.0f, 0.75f, s.element[4]) + 0.6f * er + wr * hallWet;
        }
        done = until;
        if (next < scheduled) trigger(c, notes[std::size_t(next)]);
    }
}

} // namespace wb
