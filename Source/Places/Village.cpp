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
    for (auto& p : pluck) p.reset();
    for (auto& v : drone) v.reset();
    for (auto& d : drum) d.reset();
    for (auto& b : wood) b.reset();
    woodPhrase.clear();
    room.clear();
    warmth.clear();
    for (auto& f : dcBlock) f.reset();
}

void VillagePlace::setSeed(uint32_t key) {
    pattern.stream.setKey(key, 1201);
    noise.setKey(key, 1202);
    drift.setKey(key, 1203);
}

void VillagePlace::playWood(const PlaceContext& c, int semitones, float velocity) {
    // A marimba-like bar: the strong fourth harmonic gives the wooden "tock".
    static constexpr float ratios[3] { 1.0f, 3.93f, 9.2f };
    static constexpr float decays[3] { 1.0f, 0.3f, 0.1f };
    static constexpr float gains[3] { 1.0f, 0.35f, 0.12f };
    auto& b = wood[pickVoice(wood, woods, [](const Bell& v) { return v.level(); })];
    logEvent(c.clock);
    b.start(music::noteHz(c.world->root, float(semitones)), lerp(0.5f, 1.4f, 1.0f - c.s.tension), 0.35f + 0.4f * c.s.tone, velocity,
            0.7f * (pattern.stream.unit(noiseAt) - 0.5f), rate, ratios, decays, gains, 3);
    noiseAt += 1ull << 24;
}

void VillagePlace::onStep(const PlaceContext& c, int64_t step) {
    const auto& s = c.s;
    const int mood = c.world->mood, root = c.world->root, chord = c.chord;
    const int pos = stepInBar(step);
    const int64_t bar = music::floorDiv(step, int64_t(16));
    const float act = s.activity, density = s.density, tension = s.tension;
    const float busy = lerp(0.35f, 1.15f, act);
    const auto chance = [&](uint32_t salt, int64_t length = 32) { return pattern.at(step, length, s.variation, salt); };

    // --- Plucked ostinato over the shared chord. Strong steps take chord tones; weak
    // ones may pass through the scale. A two-bar figure, repeating with Variation.
    if (s.element[0] > 0.001f) {
        float p = 0, vel = 0.7f;
        switch (s.character) {
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
        if (p > 0 && chance(1) < p) {
            const bool strong = pos % 4 == 0;
            const float pick = chance(2);
            int semis;
            if (strong || pick < 0.65f) {
                const int toneIndex = int(chance(3) * 3.0f);
                semis = music::chordTone(mood, chord, toneIndex);
            } else {
                // A passing note: one scale step above a chord tone.
                semis = music::degree(mood, chord + 2 * int(chance(3) * 3.0f) + 1);
            }
            const int low = s.character == 1 ? -2 : 3;
            semis = music::inRange(semis, low) + (chance(4) < 0.2f ? 12 : 0);
            auto& v = pluck[pickVoice(pluck, pluckVoices, [](const Pluck& x) { return x.level(); })];
            logEvent(c.clock);
            const float decay = (s.character == 1 ? 2.6f : 1.5f) * lerp(1.0f, 0.35f, tension);
            const float humanise = 1.0f + (chance(5) - 0.5f) * (0.2f + 0.5f * s.life);
            v.start(music::noteHz(root, float(semis)), decay, 0.25f + 0.6f * s.tone, vel * humanise, 0.5f * (chance(6) - 0.5f), rate, noiseAt);
            noiseAt += 1ull << 24;
            // Life: now and then a grace note, a scale step above, rings alongside.
            if (s.life > 0.15f && chance(7) < (s.life - 0.15f) * 0.6f) {
                auto& g = pluck[pickVoice(pluck, pluckVoices, [](const Pluck& x) { return x.level(); })];
                const int grace = semis + (music::degree(mood, 1) - music::degree(mood, 0));
                g.start(music::noteHz(root, float(music::quantise(mood, float(grace)))), decay * 0.5f, 0.6f, vel * 0.6f, 0.3f, rate, noiseAt);
                noiseAt += 1ull << 24;
            }
        }
    }

    // --- Hand drums: a frame drum (tuned to the root), a hand drum, a shaker.
    if (s.element[2] > 0.001f) {
        float doum = 0, tek = 0, shake = 0;
        switch (s.character) {
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
        const auto hit = [&](float p, uint32_t salt) { return p > 0 && chance(salt) < p; };
        // Machinery: a mill's wooden clack on every eighth note.
        if (s.machinery > 0.05f && (pos & 1) == 0) {
            auto& d = drum[pickVoice(drum, drums, [](const Drum& v) { return v.level(); })];
            logEvent(c.clock);
            d.ratio = 2.4f;
            d.partial = 0.5f;
            d.start(music::noteHz(root, 24.0f), 1.02f, 0.12f, 0.6f * s.machinery * (pos % 4 == 0 ? 1.0f : 0.6f), 0.45f * s.machinery, 3200.0f, 2.0f,
                    0.05f, -0.35f, rate, noiseAt);
            noiseAt += 1ull << 24;
        }
        // Tension: the frame drum doubles into an insistent eighth-note pulse.
        if (tension > 0.4f && (pos & 1) == 0 && doum == 0.0f) doum = (tension - 0.4f) * 1.2f * busy;
        if (hit(doum, 10)) {
            auto& d = drum[pickVoice(drum, drums, [](const Drum& v) { return v.level(); })];
            logEvent(c.clock);
            d.ratio = 1.6f;
            d.partial = 0.3f;
            const float vel = pos == 0 ? 1.0f : 0.7f;
            d.start(music::noteHz(root, -12.0f), 1.08f, lerp(1.4f, 0.7f, s.machinery), 0.5f * vel, 0.05f * vel, 500.0f, 0.7f, 0.12f, -0.15f, rate, noiseAt);
            noiseAt += 1ull << 24;
        }
        if (hit(tek, 11)) {
            auto& d = drum[pickVoice(drum, drums, [](const Drum& v) { return v.level(); })];
            logEvent(c.clock);
            d.ratio = 1.5f;
            d.partial = 0.2f;
            const float vel = (pos == 4 || pos == 12) ? 0.8f : 0.45f;
            d.start(music::noteHz(root, 12.0f + float(music::degree(mood, 4))), 1.03f, 0.3f, 0.25f * vel, 0.3f * vel, 1800.0f, 1.2f, 0.12f, 0.25f, rate, noiseAt);
            noiseAt += 1ull << 24;
        }
        if (hit(shake, 12)) {
            auto& d = drum[pickVoice(drum, drums, [](const Drum& v) { return v.level(); })];
            logEvent(c.clock);
            const float vel = (pos % 4 == 2) ? 0.75f : 0.45f;
            d.start(100.0f, 1.0f, 0.05f, 0.0f, 0.35f * vel * (0.8f + 0.4f * chance(13)), 7000.0f, 0.8f, lerp(0.2f, 0.12f, s.machinery), 0.45f, rate, noiseAt);
            noiseAt += 1ull << 24;
        }
    }

    // --- Wooden tones: counter-melodies, wind-chime wood or xylophone runs.
    if (s.element[3] > 0.001f) {
        switch (s.character) {
            case 0:
                if ((pos & 1) == 0 && (bar % 4) >= 2 && chance(20, 64) < act * 0.5f * lerp(0.4f, 1.6f, s.life))
                    playWood(c, 12 + music::inRange(music::chordTone(mood, chord, int(chance(21, 64) * 3.0f)), 5), 0.5f);
                break;
            case 1:
                if (chance(22) < (0.03f + 0.08f * act) * lerp(0.4f, 1.6f, s.life)) playWood(c, 12 + music::degree(mood, int(chance(23) * 8.0f)), 0.35f);
                break;
            default:
                if (pos == 8 && !woodPhrase.active() && chance(24, 256) < act * 0.6f) {
                    woodPhrase.clear();
                    const int start = chord + int(chance(25, 64) * 3.0f);
                    for (int k = 0; k < 6; ++k) woodPhrase.add(0.25f * float(k), 12 + music::degree(mood, start + k), 0.25f, 0.4f + 0.1f * float(k));
                    woodPhrase.begin(double(step) / 4.0);
                }
                break;
        }
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

    forEachStep(c.beat, span, 4, s.swing, [&](int64_t step) { onStep(c, step); });
    woodPhrase.play(c.beat, span, [&](const Phrase::Note& note) { playWood(c, note.semitones, note.velocity); });

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

    for (int i = 0; i < n; ++i) {
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
}

} // namespace wb
