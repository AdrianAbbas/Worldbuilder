#include "Places.h"

namespace wb {

using namespace dsp;

void VillagePlace::prepare(double sampleRate) {
    rate = float(sampleRate);
    for (auto& p : pluck) p.prepare(rate, 40.0f);
    room.prepare(rate);
    warmth.prepare(rate);
    for (auto& f : dcBlock) f.setCutoff(20.0f, rate);
    reset();
}

void VillagePlace::reset() {
    noiseAt = 0;
    for (auto& p : pluck) p.reset();
    for (auto& v : drone) v.reset();
    for (auto& d : drum) d.reset();
    for (auto& b : wood) b.reset();
    room.clear();
    warmth.clear();
    for (auto& f : dcBlock) f.reset();
}

void VillagePlace::setSeed(uint32_t key) {
    noise.setKey(key, 1202);
    drift.setKey(key, 1203);
}

void VillagePlace::playWood(const PlaceContext& c, int semitones, float velocity, float pan) {
    // A marimba-like bar: the strong fourth harmonic gives the wooden "tock".
    static constexpr float ratios[3] { 1.0f, 3.93f, 9.2f };
    static constexpr float decays[3] { 1.0f, 0.3f, 0.1f };
    static constexpr float gains[3] { 1.0f, 0.35f, 0.12f };
    auto& b = wood[pickVoice(wood, woods, [](const Bell& v) { return v.level(); })];
    b.start(music::noteHz(c.world->root, float(semitones)), lerp(0.5f, 1.4f, 1.0f - c.s.tension), 0.35f + 0.4f * c.s.tone, velocity, pan, rate,
            ratios, decays, gains, 3);
    b.update();
}

void VillagePlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 1201);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size;
    const auto add = [&](int step, int degree, int length, float velocity) {
        pat::Note n;
        n.step = int16_t(step);
        n.degree = int8_t(degree);
        n.length = int16_t(length);
        n.velocity = uint8_t(std::clamp(int(std::lround(velocity * 127.0f)), 1, 127));
        out.add(n);
    };

    if (element == Plucks) {
        // A two-bar ostinato over the shared chord. Strong steps take chord tones; weak
        // ones may pass through the scale.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = int8_t(in.character == 1 ? -2 : 3);
        for (int step = 0; step < out.lengthSteps; ++step) {
            const int pos = step % 16;
            float p = 0, vel = 0.7f;
            switch (in.character) {
                case 0:   // Market Day: eighth notes
                    if ((pos & 1) == 0) p = (pos % 4 == 0) ? 0.92f : (0.25f + 0.65f * density) * busy;
                    else p = tension * (0.2f + 0.6f * density) * busy;
                    vel = pos % 8 == 0 ? 0.95f : 0.7f;
                    break;
                case 1:   // Hearth: slow quarter-note arpeggios
                    if (pos % 4 == 0) p = (0.35f + 0.5f * density) * busy;
                    else if (pos % 2 == 0) p = tension * 0.3f * density;
                    vel = 0.6f;
                    break;
                default:   // Festival: swung sixteenths
                    if ((pos & 1) == 0) p = 0.9f * std::max(busy, 0.6f);
                    else p = (0.2f + 0.6f * density) * busy;
                    vel = pos % 4 == 0 ? 0.95f : 0.65f;
                    break;
            }
            if (p <= 0 || ch(1, step) >= p) continue;
            const bool strong = pos % 4 == 0;
            const int tone = int(ch(3, step) * 3.0f);
            int degree = (strong || ch(2, step) < 0.65f) ? 2 * tone : 2 * tone + 1;   // a chord tone, or a passing note above it
            if (ch(4, step) < 0.2f) degree += size;
            const float v = vel * (1.0f + (ch(5, step) - 0.5f) * (0.2f + 0.5f * life));
            add(step, degree, 2, v);
            // Life: now and then a grace note, a scale step above, rings alongside.
            if (life > 0.15f && ch(7, step) < (life - 0.15f) * 0.6f) add(step, degree + 1, 1, v * 0.6f);
        }
        return;
    }

    if (element == HandDrums) {
        // A frame drum (tuned to the root), a hand drum and a shaker. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        for (int step = 0; step < out.lengthSteps; ++step) {
            const int pos = step % 16;
            float doum = 0, tek = 0, shake = 0;
            switch (in.character) {
                case 0:
                    doum = (pos == 0) ? 0.95f : (pos == 6 || pos == 8) ? 0.5f + 0.4f * density : 0.0f;
                    tek = (pos == 4 || pos == 12) ? 0.85f : ((pos & 1) == 0 ? density * 0.3f * busy : 0.0f);
                    shake = (pos & 1) == 0 ? density * 0.6f * busy : 0.0f;
                    break;
                case 1:   // a soft heartbeat
                    doum = (pos == 0 || pos == 3) ? (0.3f + 0.5f * act) : 0.0f;
                    tek = pos == 10 ? density * 0.3f : 0.0f;
                    break;
                default:
                    doum = (pos == 0 || pos == 8) ? 0.95f : (pos == 3 || pos == 11) ? 0.6f * density + 0.2f : 0.0f;
                    tek = (pos == 4 || pos == 12) ? 0.9f : (pos == 14 ? 0.6f : 0.0f);
                    shake = (0.35f + 0.6f * density) * busy;
                    break;
            }
            tek += tension * 0.45f * float((pos & 1) == 0);
            shake += tension * 0.3f * busy;
            // Tension: the frame drum doubles into an insistent eighth-note pulse.
            if (tension > 0.4f && (pos & 1) == 0 && doum == 0.0f) doum = (tension - 0.4f) * 1.2f * busy;
            if (doum > 0 && ch(10, step) < doum) add(step, 0, 1, pos == 0 ? 1.0f : 0.7f);
            if (tek > 0 && ch(11, step) < tek) add(step, 1, 1, (pos == 4 || pos == 12) ? 0.8f : 0.45f);
            if (shake > 0 && ch(12, step) < shake) add(step, 2, 1, ((pos % 4 == 2) ? 0.75f : 0.45f) * (0.8f + 0.4f * ch(13, step)));
        }
        return;
    }

    if (element == Wood) {
        // Wooden tones: counter-melodies, wind-chime wood or xylophone runs. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        switch (in.character) {
            case 0:   // a counter-melody on chord tones in the second half
                out.followChords = true;
                out.base = 17;
                for (int step = 32; step < 64; step += 2)
                    if (ch(20, step) < act * 0.65f * lerp(0.4f, 1.6f, life)) add(step, 2 * int(ch(21, step) * 3.0f), 2, 0.5f);
                break;
            case 1:   // wind-chime wood, anywhere in the key
                out.followChords = false;
                for (int step = 0; step < 64; ++step)
                    if (ch(22, step) < (0.03f + 0.08f * act) * lerp(0.4f, 1.6f, life)) add(step, size + int(ch(23, step) * 8.0f), 2, 0.35f);
                break;
            default:   // xylophone runs up the scale from the chord
                out.followChords = true;
                out.base = 12;
                for (int bar = 0; bar < 4; ++bar) {
                    if (ch(24, bar) >= act * 0.6f) continue;
                    const int start = int(ch(25, bar) * 3.0f);
                    for (int k = 0; k < 6; ++k) add(bar * 16 + 8 + k, start + k, 1, 0.4f + 0.1f * float(k));
                }
                break;
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void VillagePlace::trigger(const PlaceContext& c, const Scheduled& x) {
    const auto& s = c.s;
    const int mood = c.world->mood, root = c.world->root;
    const auto& ev = x.event;
    const uint64_t clock = c.clock + uint64_t(x.offset);
    const float vel = ev.velocity;
    const float pan = noise.unit(noiseAt) - 0.5f;
    if (x.element == -1) {
        // Machinery: a mill's wooden clack on every eighth note.
        auto& d = drum[pickVoice(drum, drums, [](const Drum& v) { return v.level(); })];
        logEvent(clock, -1, 0, vel);
        d.ratio = 2.4f;
        d.partial = 0.5f;
        d.start(music::noteHz(root, 24.0f), 1.02f, 0.12f, 0.6f * s.machinery * vel, 0.45f * s.machinery, 3200.0f, 2.0f, 0.05f, -0.35f, rate, noiseAt);
        d.update(rate);
        noiseAt += 1ull << 24;
        return;
    }
    if (x.element == HandDrums) {
        auto& d = drum[pickVoice(drum, drums, [](const Drum& v) { return v.level(); })];
        const int kind = std::clamp(ev.degree, 0, 2);
        logEvent(clock, x.element, float(kind), vel);
        if (kind == 0) {
            d.ratio = 1.6f;
            d.partial = 0.3f;
            d.start(music::noteHz(root, -12.0f), 1.08f, lerp(1.4f, 0.7f, s.machinery), 0.5f * vel, 0.05f * vel, 500.0f, 0.7f, 0.12f, -0.15f, rate, noiseAt);
        } else if (kind == 1) {
            d.ratio = 1.5f;
            d.partial = 0.2f;
            d.start(music::noteHz(root, 12.0f + float(music::degree(mood, 4))), 1.03f, 0.3f, 0.25f * vel, 0.3f * vel, 1800.0f, 1.2f, 0.12f, 0.25f, rate,
                    noiseAt);
        } else {
            d.start(100.0f, 1.0f, 0.05f, 0.0f, 0.35f * vel, 7000.0f, 0.8f, lerp(0.2f, 0.12f, s.machinery), 0.45f, rate, noiseAt);
        }
        d.update(rate);
        noiseAt += 1ull << 24;
        return;
    }
    const auto& pattern = *c.play[std::size_t(x.element)].pattern;
    const int semis = pat::semitonesFor(pattern, mood, x.chord, ev.degree, ev.alter);
    if (x.element == Plucks) {
        auto& v = pluck[pickVoice(pluck, pluckVoices, [](const Pluck& p) { return p.level(); })];
        logEvent(clock, x.element, float(semis), vel);
        const float decay = (s.character == 1 ? 2.6f : 1.5f) * lerp(1.0f, 0.35f, s.tension) * (ev.extra ? 0.5f : 1.0f);
        v.start(music::noteHz(root, float(semis)), decay, 0.25f + 0.6f * s.tone, vel, 0.5f * pan, rate, noiseAt);
        noiseAt += 1ull << 24;
        return;
    }
    if (x.element == Wood) {
        logEvent(clock, x.element, float(semis), vel);
        playWood(c, semis, vel, 0.7f * pan);
        noiseAt += 1ull << 24;
    }
}

void VillagePlace::render(const PlaceContext& c, float* left, float* right, int n) {
    const auto& w = *c.world;
    const auto& s = c.s;
    const int root = w.root;
    const double span = c.beatsPerSample * double(n);
    const double t = double(c.clock) / rate;
    const int q = std::clamp(w.quality, 0, 2);
    pluckVoices = q == 0 ? 4 : (q == 1 ? 6 : plucks);
    const float tone = s.tone;

    std::array<Scheduled, maxScheduled> notes;
    int scheduled = schedule(c, n, notes);
    // Machinery: a mill's clack on the eighth notes (not part of the pattern).
    if (s.machinery > 0.05f && s.element[HandDrums] > 0.001f) {
        forEachStep(c.beat, span, 2, 0.0f, [&](int64_t step) {
            if (scheduled >= maxScheduled) return;
            Scheduled x;
            x.offset = std::clamp(int(std::ceil((double(step) / 2.0 - c.beat) / c.beatsPerSample - 1.0e-9)), 0, n - 1);
            x.element = -1;
            x.event.velocity = (step & 1) == 0 ? 1.0f : 0.6f;
            notes[std::size_t(scheduled++)] = x;
        });
        sortScheduled(notes, scheduled);
    }

    // Folk drone on the root and fifth: a narrow, nasal buzz like a hurdy-gurdy, not
    // following the chords (a drone holds).
    {
        // As Tension rises the fifth sinks to a semitone above the root: an uneasy drone.
        const float semis[drones] { -12.0f, s.tension > 0.6f ? -11.0f : -5.0f };
        const float buzz = s.character == 2 ? 0.85f + 0.15f * std::cos(float(c.beat * double(twoPi))) : 1.0f;
        for (int i = 0; i < drones; ++i) {
            auto& v = drone[std::size_t(i)];
            const float hz = music::noteHz(root, semis[i]);
            const float breath = drift.smooth(t + 11.0 * i, 0.06);
            if (!v.active()) v.start(hz, 2.0f, 1.0f, 1.0f, rate);
            v.targetHz = hz;
            v.glide = glideFor(1.2f, rate);
            v.detuneCents = 3.0f;
            v.subLevel = 0.0f;
            v.cutoff = lerpExp(350.0f, 1500.0f, tone) * (s.character == 1 ? 0.7f : 1.0f) * std::exp2(0.3f * breath);
            v.envOctaves = 0.0f;
            v.q = 1.6f;
            v.width = 0.5f;
            const float uneasy = i == 1 ? (s.tension > 0.6f ? clamp01((s.tension - 0.6f) * 3.0f) : clamp01((0.6f - s.tension) * 3.0f)) : 1.0f;
            v.gain = s.element[1] * (i == 0 ? 1.0f : 0.6f * uneasy) * (0.85f + 0.15f * breath) * buzz * (s.character == 1 ? 0.6f : 1.0f)
                     * lerp(0.8f, 1.7f, s.tension);
            v.update(rate);
        }
    }
    for (auto& d : drum)
        if (d.active()) d.update(rate);
    for (auto& b : wood)
        if (b.active) b.update();

    // A small wooden room, and tape warmth whose depth follows Activity and Population.
    room.set(lerp(0.05f, 0.55f, s.size), lerp(0.5f, 2.4f, s.size) * (1.0f + 0.3f * s.magic), lerpExp(2000.0f, 6000.0f, tone));
    warmth.set(s.element[4] * (0.35f + 0.65f * s.fxDepth));
    const float pluckLevel = 0.8f * s.element[0], droneLevel = 0.09f, drumLevel = 0.5f * s.element[2], woodLevel = 0.4f * s.element[3];
    const float roomSend = 0.35f + 0.25f * s.size;

    // Render up to each scheduled note, then start it: notes begin on their exact sample.
    int done = 0;
    for (int next = 0; next <= scheduled; ++next) {
        const int until = next < scheduled ? notes[std::size_t(next)].offset : n;
        for (int i = done; i < until; ++i) {
            float l = 0, r = 0;
            for (int k = 0; k < pluckVoices; ++k) {
                auto& p = pluck[std::size_t(k)];
                if (!p.active) continue;
                const float y = p.tick(noise) * pluckLevel;
                l += y * p.pan.l;
                r += y * p.pan.r;
            }
            float dl = 0, dr = 0;
            for (auto& v : drone) v.tick(dl, dr);
            l += dl * droneLevel;
            r += dr * droneLevel;
            for (auto& d : drum) {
                if (!d.active()) continue;
                const float y = d.tick(noise) * drumLevel;
                l += y * d.pan.l;
                r += y * d.pan.r;
            }
            for (auto& b : wood) {
                if (!b.active) continue;
                const float y = b.tick() * woodLevel;
                l += y * b.pan.l;
                r += y * b.pan.r;
            }
            float wl = 0, wr = 0;
            room.process(l * roomSend, r * roomSend, wl, wr);
            l += 0.7f * wl;
            r += 0.7f * wr;
            warmth.process(l, r);
            // A plucked string's loop passes DC; block it.
            left[i] = 1.25f * dcBlock[0].hp(l);
            right[i] = 1.25f * dcBlock[1].hp(r);
        }
        done = until;
        if (next < scheduled) trigger(c, notes[std::size_t(next)]);
    }
}

} // namespace wb
