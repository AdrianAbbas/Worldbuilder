#include "Places.h"

namespace wb {

using namespace dsp;

VillagePlace::VillagePlace() : EnsemblePlace(1200) { setParts({ &lute, &hurdy, &drums, &marimba, &recorder, &jingles }); }

void VillagePlace::prepareFx() {
    room.prepare(rate);
    warmth.prepare(rate);
}

void VillagePlace::resetFx() {
    room.clear();
    warmth.clear();
    for (auto& f : shelf) f.reset();
}

void VillagePlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 1201);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size;
    // The ostinato, drum and wood figures were written for Market Day (0), Quiet Dawn
    // (1, slow and soft) and Harvest Night (2, swung and busy).
    const int figure = in.character == QuietDawn ? 1 : (in.character == HarvestNight ? 2 : 0);
    const auto add = [&](int step, int degree, int length, float velocity) {
        pat::Note n;
        n.step = int16_t(step);
        n.degree = int8_t(degree);
        n.length = int16_t(length);
        n.alter = int8_t(0);
        n.velocity = uint8_t(std::clamp(int(std::lround(velocity * 127.0f)), 1, 127));
        out.add(n);
    };

    if (element == Lute) {
        // A two-bar ostinato over the shared chord. Strong steps take chord tones; weak
        // ones may pass through the scale.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = int8_t(figure == 1 ? -2 : 3);
        for (int step = 0; step < out.lengthSteps; ++step) {
            const int pos = step % 16;
            float p = 0, vel = 0.7f;
            switch (figure) {
                case 0:   // Market Day: eighth notes
                    if ((pos & 1) == 0) p = (pos % 4 == 0) ? 0.92f : (0.25f + 0.65f * density) * busy;
                    else p = tension * (0.2f + 0.6f * density) * busy;
                    vel = pos % 8 == 0 ? 0.95f : 0.7f;
                    break;
                case 1:   // Quiet Dawn: slow quarter-note arpeggios
                    if (pos % 4 == 0) p = (0.35f + 0.5f * density) * busy;
                    else if (pos % 2 == 0) p = tension * 0.3f * density;
                    vel = 0.6f;
                    break;
                default:   // Harvest Night: swung sixteenths
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
            switch (figure) {
                case 0:
                    doum = (pos == 0) ? 0.95f : (pos == 6 || pos == 8) ? 0.5f + 0.4f * density : 0.0f;
                    tek = (pos == 4 || pos == 12) ? 0.85f : ((pos & 1) == 0 ? density * 0.3f * busy : 0.0f);
                    shake = (pos & 1) == 0 ? density * 0.6f * busy : 0.0f;
                    break;
                case 1:   // Quiet Dawn: a soft heartbeat
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

    if (element == Marimba) {
        // Wooden tones: counter-melodies, wind-chime wood or xylophone runs. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        switch (figure) {
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
    PatternWriter w { out };

    if (element == HurdyGurdy) {
        // The drone holds the root and the fifth below the key (it does not follow the
        // chords); as Tension rises the fifth sinks to a semitone above the root.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        w.note(0, -size, 64, 0.9f);
        if (tension > 0.6f) w.note(0, -size, 64, 0.6f, 1);
        else w.note(0, music::nearestDegree(in.mood, 7) - size, 64, 0.6f);
        return;
    }

    if (element == Recorder) {
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = true;
        out.base = 12;
        // A short tune that walks the scale from a chord tone and comes home to one.
        const auto phrase = [&](int at, int notes, int stepLength, uint32_t salt, float velocity) {
            int degree = 2 * int(ch(salt, at) * 3.0f);
            for (int k = 0; k < notes; ++k) {
                const bool last = k == notes - 1;
                const int length = last ? stepLength * 2 : stepLength;
                w.note(at + k * stepLength, degree, length, velocity * (k == 0 ? 1.0f : 0.85f + 0.2f * ch(salt + 1, at + k)));
                const float move = ch(salt + 2, at + k);
                degree += move < 0.4f ? -1 : (move < 0.8f ? 1 : (move < 0.9f ? 2 : -2));
                degree = std::clamp(degree, -2, size + 2);
                if (k == notes - 2) degree = 2 * int(std::round(float(degree) / 2.0f));   // land on a chord tone
            }
        };
        switch (in.character) {
            case MarketDay: {
                // A phrase every other bar or so; at least one in the eight bars.
                bool any = false;
                for (int bar = 0; bar < 8; bar += 2)
                    if (ch(40, bar) < 0.25f + 0.55f * act * lerp(0.6f, 1.3f, life) || (bar == 6 && !any)) {
                        phrase(bar * 16 + 4, 6 + int(ch(41, bar) * 3.0f), 2, 42, 0.75f);
                        any = true;
                    }
                break;
            }
            case HarvestNight:
                for (int bar = 0; bar < 8; ++bar)
                    if (ch(45, bar) < 0.45f + 0.5f * act) phrase(bar * 16, 7 + int(ch(46, bar) * 2.0f * density), 2, 47, 0.8f);
                break;
            default:   // long, birdlike notes high in the morning air
                for (int bar = 0; bar < 8; bar += 2)
                    if (ch(50, bar) < 0.25f + 0.5f * act || bar == 4) {
                        const int degree = size + 2 * int(ch(51, bar) * 3.0f);
                        if (ch(52, bar) < 0.5f + 0.4f * life) w.note(bar * 16 + 3, degree + 1, 1, 0.45f);
                        w.note(bar * 16 + 4, degree, 12, 0.6f);
                        if (ch(53, bar) < act) w.note(bar * 16 + 20, degree - 2, 8, 0.5f);
                    }
                break;
        }
        return;
    }

    if (element == Jingles) {
        // Small bells high in the key, two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        const auto bell = [&](uint32_t salt, int64_t at) { return 2 * size + 2 * int(ch(salt, at) * 3.0f); };
        switch (in.character) {
            case MarketDay:   // a shop bell now and then: a quick falling cluster
                for (int bar = 0; bar < 2; ++bar)
                    if (ch(60, bar) < 0.3f + 0.45f * act) {
                        const int at = bar * 16 + 4 * int(ch(61, bar) * 4.0f), top = bell(62, bar) + 2;
                        for (int k = 0; k < 3 + int(ch(63, bar) * 2.0f); ++k) w.note(at + k, top - k, 2, 0.65f - 0.1f * float(k));
                    }
                break;
            case HarvestNight:   // tambourine-like jingles on the off-beats
                for (int step = 2; step < 32; step += 4)
                    if (ch(64, step) < (0.4f + 0.5f * density) * busy) {
                        w.note(step, bell(65, step), 1, 0.6f);
                        if (ch(66, step) < 0.6f) w.note(step + 1, bell(67, step), 1, 0.35f);
                    }
                break;
            default:   // a distant, ringing bell, now and then (its chance is the note's)
                for (int bar = 0; bar < 2; ++bar) w.note(bar * 16 + int(ch(69, bar) * 4.0f) * 4, bell(70, bar) - size, 8, 0.55f, 0, 0.2f + 0.35f * act);
                break;
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}


void VillagePlace::configure(const PlaceContext& c) {
    const auto& w = *c.world;
    const auto& s = c.s;
    const int q = frame.quality, mood = w.mood;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool dawn = s.character == QuietDawn, night = s.character == HarvestNight;
    {
        auto& p = lute.patch;
        p.decay = (dawn ? 2.6f : 1.5f) * lerp(1.0f, 0.35f, tension);
        p.brightness = 0.25f + 0.6f * tone;
        p.spread = 0.5f;
        p.voices = byQuality(q, 4, 6, 8);
        // Gut courses (pairs of strings) plucked by the fingers, a lute's light body.
        p.body = dsp::BodyLute;
        p.shape = { 0.16f, 0.55f, 0.05f, 0.8f, 2.5f, 0.25f, 0.0f };
        p.sympathetic = 0.08f;
        p.gain = 1.0f;
    }
    // Hurdy-gurdy: a narrow, nasal buzz; at Harvest Night the wheel buzzes on the beats.
    {
        auto& p = hurdy.patch;
        // A hurdy-gurdy: a rosined wheel bowing the strings near the bridge (nasal,
        // grinding); at Harvest Night the wheel buzzes on the beats.
        p.model = BowedString;
        p.size = dsp::Viola;
        p.players = 1;
        p.position = 0.05f;
        p.brightness = lerp(0.2f, 0.95f, tone) * (dawn ? 0.7f : 1.0f) * std::exp2(0.2f * drift.smooth(t, 0.06));
        p.noisiness = 0.2f + 0.35f * tone;
        p.attack = 2.0f;
        p.release = 1.5f;
        p.glide = 1.2f;
        p.vibratoDepth = 0.0f;
        p.spread = 0.2f;
        p.tremoloPerBeat = night ? 2.0f : 0.0f;
        p.tremoloDepth = night ? 0.3f : 0.0f;
        p.voices = 2;
        p.held = true;
        p.gain = (dawn ? 0.6f : 1.0f) * lerp(0.8f, 1.7f, tension);
    }
    // Frame drum (tuned to the root), hand drum (on the fifth), shaker, and with
    // Machinery a mill's wooden clack.
    {
        auto& p = drums.patch;
        auto& doum = p.rows[0];
        doum = {};
        doum.semis = -12;
        doum.bend = 1.08f;
        doum.decay = lerp(1.4f, 0.7f, s.machinery);
        doum.tone = 0.5f;
        doum.noise = 0.05f;
        doum.noiseHz = 500;
        doum.noiseDecay = 0.12f;
        doum.ratio = 1.6f;
        doum.partial = 0.3f;
        doum.pan = -0.15f;
        doum.kind = dsp::DrumMembrane;
        doum.position = 0.15f;
        doum.hardness = 0.2f;
        auto& tek = p.rows[1];
        tek = {};
        tek.semis = 12.0f + float(music::degree(mood, 4));
        tek.bend = 1.03f;
        tek.decay = 0.3f;
        tek.tone = 0.25f;
        tek.noise = 0.3f;
        tek.noiseHz = 1800;
        tek.noiseQ = 1.2f;
        tek.noiseDecay = 0.12f;
        tek.ratio = 1.5f;
        tek.partial = 0.2f;
        tek.pan = 0.25f;
        tek.kind = dsp::DrumMembrane;
        tek.position = 0.9f;
        tek.hardness = 0.75f;
        auto& shaker = p.rows[2];
        shaker = {};
        shaker.tuned = false;
        shaker.hz = 100;
        shaker.bend = 1.0f;
        shaker.decay = 0.05f;
        shaker.tone = 0.0f;
        shaker.noise = 0.35f;
        shaker.noiseHz = 7000;
        shaker.noiseQ = 0.8f;
        shaker.noiseDecay = lerp(0.2f, 0.12f, s.machinery);
        shaker.pan = 0.45f;
        auto& clack = p.rows[3];
        clack = {};
        clack.semis = 24;
        clack.bend = 1.02f;
        clack.decay = 0.12f;
        clack.tone = 0.6f * s.machinery;
        clack.noise = 0.45f * s.machinery;
        clack.noiseHz = 3200;
        clack.noiseQ = 2.0f;
        clack.noiseDecay = 0.05f;
        clack.ratio = 2.4f;
        clack.partial = 0.5f;
        clack.pan = -0.35f;
        p.voices = 6;
        p.gain = 1.0f;
    }
    // A marimba-like bar: the strong fourth harmonic gives the wooden "tock".
    {
        auto& p = marimba.patch;
        // A rosewood bar tuned to two octaves (and ~9.2), its tube resonator holding the
        // fundamental.
        p.set({ 1.0f, 3.99f, 9.2f }, { 1.3f, 0.25f, 0.08f }, { 1.0f, 0.35f, 0.12f });
        p.decay = lerp(0.5f, 1.4f, 1.0f - tension);
        p.hardness = 0.35f + 0.4f * tone;
        p.spread = 0.7f;
        p.voices = 5;
        p.gain = 1.0f;
    }
    // Recorder: nearly pure, breathy, with a gentle vibrato (more at dawn).
    {
        auto& p = recorder.patch;
        p.h2 = 0.08f + 0.2f * tone;
        p.h3 = 0.03f + 0.1f * tone;
        p.h4 = 0.04f * tone;
        p.breath = 0.18f + 0.08f * (1.0f - tone);
        p.attack = 0.035f;
        p.release = dawn ? 0.3f : 0.1f;
        p.vibratoHz = 5.2f;
        p.vibratoDepth = (dawn ? 0.22f : 0.12f) + 0.1f * s.life;
        p.scoop = 0.5f;
        p.spread = 0.25f;
        p.voices = 1;
        // A fipple flute: a quick, clear chiff, little air beyond the tone, steady pitch.
        p.chiff = 0.45f;
        p.chiffSeconds = 0.025f;
        p.air = 0.04f;
        p.wander = 2.0f;
        p.gain = 1.0f;
    }
    // Small bells: bright, inharmonic, short.
    {
        auto& p = jingles.patch;
        p.set({ 1.0f, 2.32f, 4.25f, 6.63f }, { 1.0f, 0.6f, 0.4f, 0.3f }, { 1.0f, 0.5f, 0.35f, 0.2f });
        p.decay = dawn ? 2.4f : 0.6f;
        p.hardness = 0.9f;
        p.toneHz = lerpExp(6000.0f, 14000.0f, tone);
        p.spread = 0.8f;
        p.voices = byQuality(q, 3, 4, 6);
        p.extras.beatHz = 3.0f;
        p.extras.variation = 0.4f;
        p.gain = 1.0f;
    }
    level = { 0.8f, 0.09f, 0.5f, 0.4f, 0.08f, 0.14f };

    // A small wooden room, and tape warmth whose depth follows Activity and Population.
    room.set(lerp(0.05f, 0.55f, s.size), lerp(0.5f, 2.4f, s.size) * (1.0f + 0.3f * s.magic), lerpExp(2000.0f, 6000.0f, tone));
    warmth.set(0.8f * (0.35f + 0.65f * s.fxDepth));
    roomSend = 0.35f + 0.25f * s.size;
    // Tone: a presence shelf over the whole village (as the later Places have).
    presence = 1.4f * (s.tone - 0.4f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
}

int VillagePlace::addNotes(const PlaceContext& c, int n, std::array<Scheduled, maxScheduled>& notes, int count) {
    // Machinery: a mill's clack on every eighth note (not part of the pattern).
    if (c.s.machinery <= 0.05f || c.s.element[HandDrums] <= 0.001f) return count;
    forEachStep(c.beat, c.beatsPerSample * double(n), 2, 0.0f, [&](int64_t step) {
        if (count >= maxScheduled) return;
        Scheduled x;
        x.offset = std::clamp(int(std::ceil((double(step) / 2.0 - c.beat) / c.beatsPerSample - 1.0e-9)), 0, n - 1);
        x.element = HandDrums;
        x.tag = 1;
        x.event.beat = double(step) / 2.0;
        x.event.degree = 3;
        x.event.velocity = (step & 1) == 0 ? 1.0f : 0.6f;
        notes[std::size_t(count++)] = x;
    });
    return count;
}

bool VillagePlace::shape(const PlaceContext&, const Scheduled&, NoteOn&) { return true; }

void VillagePlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        float wl = 0, wr = 0;
        room.process(l * roomSend, r * roomSend, wl, wr);
        l += 0.7f * wl;
        r += 0.7f * wr;
        warmth.process(l, r);
        l += presence * (l - shelf[0].lp(l));
        r += presence * (r - shelf[1].lp(r));
        left[i] = 1.25f * l;
        right[i] = 1.25f * r;
    }
}

} // namespace wb
