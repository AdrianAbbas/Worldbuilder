#include "Places.h"

namespace wb {

using namespace dsp;

BeachPlace::BeachPlace() : EnsemblePlace(2700) { setParts({ &guitar, &steel, &waves, &shaker, &pad, &gulls }); }

void BeachPlace::prepareFx() {
    air.prepare(rate);
    chorus.prepare(rate);
}

void BeachPlace::resetFx() {
    air.clear();
    chorus.clear();
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
}

void BeachPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2701);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Guitar) {
        // A warm nylon guitar on the chord. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = -7;
        switch (in.character) {
            case TropicalDay: {   // a lilting strum: down, down-up, up, down-up
                static constexpr int strums[6] { 0, 4, 6, 10, 12, 14 };
                for (int bar = 0; bar < 2; ++bar)
                    for (int k = 0; k < 6; ++k) {
                        const int step = bar * 16 + strums[k];
                        if (k > 0 && ch(1, step) >= (0.35f + 0.5f * density) * busy) continue;
                        const float v = (k == 0 ? 0.7f : 0.5f) * human(2, step);
                        w.note(step, 0, 2, v);
                        w.note(step, 2, 2, v * 0.85f);
                        w.note(step, 4, 2, v * 0.85f);
                        if (k % 2 == 0) w.note(step, size, 2, v * 0.7f);
                    }
                break;
            }
            case Sunset: {   // slow fingerpicking: bass, then the chord tones climbing
                static constexpr int pick[8] { 0, 4, 7, 9, 11, 9, 7, 4 };
                for (int step = 0; step < 32; step += 2) {
                    if (step % 8 != 0 && ch(3, step) >= (0.4f + 0.5f * density) * busy) continue;
                    const int d = pick[(step / 2) % 8];
                    w.note(step, d >= 7 ? d - 7 + size : d, 4, (step % 8 == 0 ? 0.65f : 0.5f) * human(4, step));
                }
                break;
            }
            default:   // Stormy Shore: a nervous tremolo on the root and fifth
                for (int step = 0; step < 32; ++step) {
                    if (step % 4 != 0 && ch(5, step) >= (0.3f + 0.5f * density) * busy) continue;
                    w.note(step, (step / 8) % 2 == 0 ? 0 : 4, 1, (step % 4 == 0 ? 0.6f : 0.4f) * human(6, step));
                }
                break;
        }
        return;
    }

    if (element == SteelDrum) {
        // A steel-drum tune in the scale, bouncing between chord tones. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        static constexpr int tune[16] { 0, 2, 4, 2, 4, 5, 4, 2, 1, 2, 4, 6, 7, 6, 4, 2 };
        float keep = 0;
        int every = 2;
        switch (in.character) {
            case TropicalDay: keep = 0.45f + 0.5f * act; every = 2; break;
            case Sunset: keep = 0.3f + 0.4f * act; every = 4; break;
            default: keep = 0.15f + 0.3f * act; every = 4; break;
        }
        for (int k = 0; k < 64 / every; ++k) {
            const int step = k * every;
            if (step % 16 >= 14) continue;
            if (step % 16 != 0 && ch(10, k) >= keep * lerp(0.7f, 1.3f, density)) continue;
            const int d = tune[(k + (step / 16) * 3) % 16];
            w.note(step, d >= 7 ? d - 7 + size : d, every, (step % 8 == 0 ? 0.65f : 0.5f) * human(11, k));
            // Steel-drum players double quick notes (a roll on the same pitch).
            if (in.character == TropicalDay && every == 2 && ch(12, k) < 0.25f * busy) w.note(step + 1, d >= 7 ? d - 7 + size : d, 1, 0.4f);
        }
        if (tension > 0.6f) w.note(60, fifth, 4, 0.45f);
        return;
    }

    if (element == Waves) {
        // The surf: a low tuned swell on the root and fifth, held four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        w.note(0, 0, 64, 0.8f);
        w.note(0, fifth, 64, 0.55f);
        if (in.character == StormyShore) w.note(0, -size, 64, 0.65f);
        return;
    }

    if (element == Shaker) {
        // A shaker (row 0) and a clave (row 1). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        switch (in.character) {
            case TropicalDay: {
                for (int step = 0; step < 32; ++step)
                    if (step % 4 == 0 || (step % 2 == 0 && ch(27, step + 64) < 0.25f + 0.7f * busy) || ch(20, step) < 0.5f * busy * density) w.note(step, 0, 1, (step % 4 == 0 ? 0.6f : 0.4f) * human(21, step));
                static constexpr int clave[5] { 0, 6, 12, 20, 24 };   // 3-2 son clave
                for (int k : clave)
                    if (k == 0 || ch(22, k) < 0.5f + 0.5f * busy) w.note(k, 1, 1, 0.65f * human(23, k));
                break;
            }
            case Sunset:
                for (int step = 0; step < 32; step += 2)
                    if (step % 8 == 0 || ch(24, step) < (0.3f + 0.4f * density) * busy) w.note(step, 0, 1, 0.4f * human(25, step));
                if (ch(26, 0) < 0.3f + 0.5f * act) w.note(12, 1, 1, 0.5f);
                break;
            default:   // Stormy Shore: rain spattering
                for (int step = 0; step < 32; ++step)
                    if (ch(27, step) < (0.3f + 0.5f * density) * busy) w.note(step, 0, 1, (0.3f + 0.3f * ch(28, step)) * human(29, step));
                break;
        }
        if (tension > 0.5f)
            for (int step = 2; step < 32; step += 8) w.note(step, 1, 1, 0.3f + 0.3f * tension);
        return;
    }

    if (element == BreezePad) {
        // A breezy pad on the chord, its top voice moving every two bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -12;
        w.note(0, 0, 64, 0.75f);
        w.note(0, 4, 64, 0.6f);
        w.note(0, size + 2, 32, 0.55f);
        w.note(32, ch(30, 0) < 0.5f ? size + 1 : size + 3, 32, 0.5f);
        if (in.character == Sunset) w.note(0, size + 4, 64, 0.45f);
        return;
    }

    if (element == Gulls) {
        // Gulls calling over the water: falling pairs and laughs, high in the key.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        float chanceOf = 0;
        switch (in.character) {
            case TropicalDay: chanceOf = 0.25f + 0.45f * act; break;
            case Sunset: chanceOf = 0.12f + 0.3f * act; break;
            default: chanceOf = 0.1f + 0.25f * act; break;
        }
        for (int beat = 0; beat < 16; ++beat) {
            if (ch(40, beat) >= chanceOf * lerp(0.6f, 1.4f, life) && beat != 3) continue;
            const int at = beat * 4 + int(ch(41, beat) * 2.0f), top = 2 + int(ch(42, beat) * 3.0f);
            const int calls = 2 + int(ch(43, beat) * (1.0f + 2.0f * busy));
            for (int k = 0; k < calls && at + k * 2 < 64; ++k) w.note(at + k * 2, top - (k % 2), 2, (k == 0 ? 0.65f : 0.5f) * human(44, at + k));
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void BeachPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool tropical = s.character == TropicalDay, sunset = s.character == Sunset, stormy = s.character == StormyShore;

    // Nylon guitar: warm and round, brighter with Tone.
    {
        auto& p = guitar.patch;
        p.decay = (sunset ? 2.6f : 1.6f) * lerp(1.0f, 0.5f, tension);
        p.brightness = 0.2f + 0.5f * tone;
        p.spread = 0.4f;
        p.pan = -0.15f;
        p.voices = byQuality(q, 5, 7, 8);
        // Nylon strings, fingers, a classical guitar's body.
        p.body = dsp::BodyGuitar;
        p.shape = { 0.2f, 0.45f, 0.05f, 0.0f, 0.0f, 0.2f, 0.0f };
        p.sympathetic = 0.05f;
        p.gain = 1.0f;
    }
    // Steel drum: the pan's note, its octave and a twelfth, and a quarter-tone-off shimmer.
    {
        auto& p = steel.patch;
        p.set({ 1.0f, 2.0f, 3.0f, 4.36f }, { 1.0f, 0.75f, 0.5f, 0.25f }, { 1.0f, 0.6f, 0.3f * (0.4f + tone), 0.12f * tone });
        p.decay = sunset ? 1.8f : 1.0f;
        p.hardness = 0.45f + 0.35f * tone;
        p.toneHz = lerpExp(3000.0f, 10000.0f, tone);
        p.transpose = 12.0f;
        p.spread = 0.3f;
        p.pan = 0.25f;
        p.voices = byQuality(q, 4, 5, 6);
        p.gain = 1.0f;
    }
    // Waves: low tuned breath that heaves with the surf.
    const float surf = 0.5f + 0.5f * std::sin(float(t) * twoPi * (stormy ? 0.14f : 0.08f) + 1.3f * drift.smooth(t, 0.05));
    {
        auto& p = waves.patch;
        p.tone = 0.0f;
        p.breath = 1.0f;
        p.breathQ = lerp(1.5f, 3.0f, tone);
        p.attack = 0.7f;
        p.release = 2.5f;
        p.scoop = 0.0f;
        p.vibratoDepth = 0.0f;
        p.spread = 0.9f;
        p.voices = 3;
        p.held = true;
        p.gain = 0.5f + 0.8f * surf * surf;
    }
    // A shaker and a wooden clave.
    {
        auto& p = shaker.patch;
        auto& shake = p.rows[0];
        shake = {};
        shake.tuned = false;
        shake.hz = 100;
        shake.tone = 0.0f;
        shake.noise = 1.0f;
        shake.noiseHz = lerpExp(4000.0f, 8500.0f, tone);
        shake.noiseQ = 0.9f;
        shake.noiseDecay = stormy ? 0.05f : 0.08f;
        shake.pan = 0.35f;
        auto& clave = p.rows[1];
        clave = {};
        clave.semis = 24.0f + float(music::degree(frame.mood, 4));
        clave.bend = 1.0f;
        clave.decay = 0.08f;
        clave.tone = 0.6f;
        clave.noise = 0.1f;
        clave.noiseHz = 2500;
        clave.noiseQ = 2.0f;
        clave.noiseDecay = 0.01f;
        clave.ratio = 2.5f;
        clave.partial = 0.2f;
        clave.pan = -0.3f;
        p.spread = 0.2f;
        p.voices = 5;
        p.gain = 1.0f;
    }
    // A breezy pad: soft detuned saws, low-passed, swaying.
    {
        auto& p = pad.patch;
        p.held = true;
        p.legato = true;
        p.attack = 1.8f;
        p.release = 2.0f;
        p.glide = 0.8f;
        p.detune = 10.0f;
        p.sub = 0.3f;
        p.cutoff = lerpExp(380.0f, 1500.0f, tone) * (sunset ? 1.2f : 1.0f) * std::exp2(0.4f * drift.smooth(t * 0.7, 0.08));
        p.envOctaves = 0.0f;
        p.q = 0.8f;
        p.width = 0.8f;
        p.vibratoHz = 0.3f;
        p.vibratoDepth = 0.04f;
        p.spread = 0.7f;
        p.voices = byQuality(q, 4, 5, 6);
        p.gain = 1.0f;
    }
    // Gulls: bright whistles that swoop up and fall away.
    {
        auto& p = gulls.patch;
        p.h2 = 0.25f;
        p.h3 = 0.1f;
        p.breath = 0.15f;
        p.attack = 0.01f;
        p.release = 0.08f;
        p.scoop = 5.0f;
        p.scoopSeconds = 0.05f;
        p.vibratoHz = 9.0f;
        p.vibratoDepth = stormy ? 0.35f : 0.2f;
        p.vibratoDelay = 0.06f;
        p.minSeconds = 0.08f;
        p.transpose = 12.0f;
        p.spread = 0.9f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Tension: the surf rises and the shaker rattles, the steel drum and pad step back;
    // Life brings the gulls; Machinery a busier rhythm section.
    // Tension: the wind tears the echoes away (less in the reverbs and delays).
    sendScale.fill(lerp(1.25f, 0.55f, tension));
    level = { 0.5f * (stormy ? 0.8f : 1.0f), 0.24f * (1.0f - 0.6f * tension) * (tropical ? 1.1f : 1.0f),
              1.4f * (stormy ? 1.5f : (sunset ? 0.8f : 1.0f)) * (0.4f + 2.0f * tension), 1.6f * (stormy ? 3.0f : 1.0f) * (0.6f + 0.8f * tension) * (0.6f + 0.9f * s.machinery),
              0.04f * (sunset ? 1.3f : 1.0f) * (1.0f - 0.3f * tension), 0.08f * lerp(0.5f, 1.5f, s.life) };

    // Open, airy space: a wide bright tail, a gentle chorus.
    air.set(lerp(0.6f, 1.0f, s.size), lerp(1.5f, 3.5f, s.size) * (1.0f + 0.3f * s.magic), lerpExp(3500.0f, 12000.0f, tone) * (stormy ? 0.6f : 1.0f));
    airSend = (sunset ? 0.5f : 0.35f) + 0.2f * s.fxDepth;
    dry = lerp(1.0f, 0.7f, s.size);
    chorus.set(0.35f, 0.0025f, (stormy ? 0.25f : 0.45f) * (0.6f + 0.6f * s.fxDepth));
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era) * (stormy ? 0.7f : 1.0f) * (1.0f - 0.5f * tension), rate);
    presence = 1.2f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
}

bool BeachPlace::shape(const PlaceContext&, const Scheduled& x, NoteOn& on) {
    // Gulls wheel overhead.
    if (x.element == Gulls) {
        on.hasPan = true;
        on.pan = 0.8f * (2.0f * on.random - 1.0f);
    }
    return true;
}

void BeachPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        // The chorus on the guitar, the steel drum and the pad.
        float cl = busL[Guitar][k] + busL[SteelDrum][k] + busL[BreezePad][k], cr = busR[Guitar][k] + busR[SteelDrum][k] + busR[BreezePad][k];
        chorus.process(cl, cr);
        float l = cl + busL[Waves][k] + busL[Shaker][k] + busL[Gulls][k], r = cr + busR[Waves][k] + busR[Shaker][k] + busR[Gulls][k];
        float wl = 0, wr = 0;
        air.process(l * airSend, r * airSend, wl, wr);
        // Open: the tail spreads wider than the beach.
        l = dry * l + 0.7f * (wl - 0.2f * wr);
        r = dry * r + 0.7f * (wr - 0.2f * wl);
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
