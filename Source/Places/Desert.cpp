#include "Places.h"

namespace wb {

using namespace dsp;

DesertPlace::DesertPlace() : EnsemblePlace(2800) { setParts({ &oudRec, &drums, &ney, &tanpura, &wind, &chimes }); }

void DesertPlace::prepareFx() {
    space.prepare(rate);
    echo.prepare(rate, 2.5f);
}

void DesertPlace::resetFx() {
    space.clear();
    echo.clear();
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
}

void DesertPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2801);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Oud) {
        // The oud: ornamented phrases around the lower notes of the mode. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        switch (in.character) {
            case Oasis: {   // lilting phrases: a turn around a note, then a step on
                int d = 0;
                for (int beat = 0; beat < 16; ++beat) {
                    const int at = beat * 4;
                    if (beat % 4 == 3) continue;   // breathe at the end of each bar
                    if (beat % 4 != 0 && ch(1, beat) >= (0.45f + 0.5f * density) * busy) continue;
                    const float move = ch(2, beat);
                    d = std::clamp(d + (move < 0.4f ? 1 : (move < 0.75f ? -1 : 2)), -2, size);
                    w.note(at, d, 2, 0.65f * human(3, at));
                    if (ch(4, beat) < 0.3f + 0.5f * busy) {
                        w.note(at + 2, d + 1, 1, 0.45f);
                        w.note(at + 3, d, 1, 0.5f);
                    }
                }
                break;
            }
            case EndlessDunes: {   // a slow taqsim: long notes, a run now and then
                static constexpr int line[8] { 0, 1, 0, 2, 1, 3, 2, 0 };
                for (int k = 0; k < 8; ++k) {
                    const int at = k * 8;
                    if (k % 2 == 1 && ch(5, k) >= 0.3f + 0.6f * busy) continue;
                    w.note(at, line[k], 8, 0.6f * human(6, k));
                    if (ch(7, k) < 0.25f * busy) w.note(at + 6, line[k] + 1, 1, 0.4f);
                }
                break;
            }
            default:   // Sandstorm: tremolo on the root and the second, the mode's colour
                for (int step = 0; step < 64; ++step) {
                    if (step % 2 == 1 && ch(8, step) >= (0.4f + 0.5f * density) * busy) continue;
                    const int bar = step / 16;
                    w.note(step, bar % 2 == 1 && step % 16 >= 8 ? 1 : 0, 1, (step % 4 == 0 ? 0.65f : 0.45f) * human(9, step));
                }
                break;
        }
        if (tension > 0.6f) w.note(60, 1, 2, 0.5f);
        return;
    }

    if (element == FrameDrums) {
        // Doum (row 0, the deep centre), tek (row 1, the rim) and a riq's jingles (row 2). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        switch (in.character) {
            case Oasis:   // maqsum: D T - T D - T -
                for (int bar = 0; bar < 2; ++bar) {
                    const int b = bar * 16;
                    w.note(b, 0, 1, 0.75f * human(10, b));
                    w.note(b + 2, 1, 1, 0.55f * human(11, b));
                    w.note(b + 6, 1, 1, 0.55f * human(12, b));
                    w.note(b + 8, 0, 1, 0.7f * human(13, b));
                    w.note(b + 12, 1, 1, 0.55f * human(14, b));
                    for (int k = 1; k < 16; k += 2)
                        if (ch(15, b + k) < (0.2f + 0.5f * density) * busy) w.note(b + k, k % 4 == 3 ? 2 : 1, 1, 0.35f * human(16, b + k));
                }
                break;
            case EndlessDunes:   // a slow, far-off pulse
                w.note(0, 0, 1, 0.65f);
                if (ch(17, 0) < 0.3f + 0.6f * act) w.note(10, 1, 1, 0.45f);
                w.note(16, 0, 1, 0.55f);
                if (ch(18, 0) < 0.3f * busy) w.note(24, 2, 1, 0.4f);
                break;
            default:   // Sandstorm: ayoub, driving, the riq shimmering
                for (int step = 0; step < 32; ++step) {
                    const int inBeat = step % 8;
                    if (inBeat == 0 || inBeat == 3) w.note(step, 0, 1, 0.7f * human(19, step));
                    else if (inBeat == 4 || inBeat == 6) w.note(step, 1, 1, 0.55f * human(20, step));
                    if (ch(21, step) < (0.3f + 0.5f * density) * busy) w.note(step, 2, 1, 0.35f * human(22, step));
                }
                break;
        }
        if (tension > 0.5f)
            for (int step = 28; step < 32; ++step) w.note(step, 1, 1, 0.3f + 0.4f * tension);
        return;
    }

    if (element == Ney) {
        // The ney: long breathy notes that scoop into place, phrases resting between. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        float keep = 0;
        int length = 4;
        switch (in.character) {
            case Oasis: keep = 0.35f + 0.5f * act; length = 4; break;
            case EndlessDunes: keep = 0.2f + 0.4f * act; length = 8; break;
            default: keep = 0.25f + 0.4f * act; length = 2; break;
        }
        int d = size;
        for (int at = 0; at < 64; at += length) {
            if (at % 32 >= 24) continue;   // a breath
            if (at % 16 != 0 && ch(30, at) >= keep * lerp(0.7f, 1.3f, density)) continue;
            const float move = ch(31, at);
            d = std::clamp(d + (move < 0.45f ? -1 : (move < 0.8f ? 1 : -2)), 2, size + 4);
            w.note(at, d, length, (at % 16 == 0 ? 0.65f : 0.5f) * human(32, at));
        }
        return;
    }

    if (element == Tanpura) {
        // A tanpura-like drone: the fifth below and the root, held all four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        w.note(0, 0, 64, 0.8f);
        w.note(0, fifth - size, 64, 0.65f);
        if (in.character != Sandstorm) w.note(0, size, 64, 0.4f);
        return;
    }

    if (element == Wind) {
        // Wind whistling over the dunes: high tuned breath on the root and fifth, in gusts.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        w.note(0, size, 32, 0.7f);
        w.note(32, size + (ch(40, 0) < 0.5f ? 4 : 2), 32, 0.6f);
        if (in.character == Sandstorm) w.note(0, fifth, 64, 0.7f);
        return;
    }

    if (element == Chimes) {
        // Sparse metal chimes on a tent or a camel's harness. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        float chanceOf = 0;
        switch (in.character) {
            case Oasis: chanceOf = 0.1f + 0.25f * act; break;
            case EndlessDunes: chanceOf = 0.05f + 0.15f * act; break;
            default: chanceOf = 0.25f + 0.4f * act; break;
        }
        for (int beat = 0; beat < 16; ++beat) {
            if (beat != 1 && ch(50, beat) >= chanceOf * lerp(0.6f, 1.4f, life)) continue;
            const int at = beat * 4 + int(ch(51, beat) * 3.0f), notes = 1 + int(ch(52, beat) * (1.0f + 2.0f * busy));
            for (int k = 0; k < notes && at + k < 64; ++k) w.note(at + k, int(ch(53, at + k) * 5.0f), 2, (0.45f + 0.25f * ch(54, at + k)) * human(55, at + k));
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void DesertPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool oasis = s.character == Oasis, dunes = s.character == EndlessDunes, storm = s.character == Sandstorm;

    // The oud: a short, woody pluck, darker than a guitar.
    {
        auto& p = oud.patch;
        p.decay = (dunes ? 1.6f : 0.9f) * lerp(1.0f, 0.6f, tension);
        p.brightness = (storm ? 0.2f : 0.3f) + 0.45f * tone;
        p.spread = 0.3f;
        p.pan = -0.15f;
        p.voices = byQuality(q, 4, 6, 8);
        // Paired courses struck near the bridge with a risha (a quill), a deep bowl body.
        p.body = dsp::BodyOud;
        p.shape = { 0.14f, 0.4f, 0.08f, 1.0f, 3.5f, 0.45f, 0.0f };
        p.sympathetic = 0.15f;
        p.gain = 1.0f;
    }
    // The recorded oud: the developer's classical guitar, revoiced. Paired courses a few
    // cents apart (as an oud's are), its deep bowl body, a darker and more nasal tone, a
    // slightly harder pluck (the risha) and, with Scoop / Glide on, a hint of a fretless
    // slide into each note; shorter than a guitar, longest in the dunes.
    {
        auto& p = oudRec.patch;
        p.instrument = "guitar";
        p.decay = (dunes ? 3.6f : 2.2f) * lerp(1.0f, 0.6f, tension);
        p.toneHz = lerpExp(2400.0f, 9000.0f, tone) * (storm ? 0.8f : 1.0f);
        p.velocityTone = 0.7f;
        p.velocityGain = 0.45f;
        p.courseCents = 6.0f;
        p.courseLevel = 0.6f;
        p.courseMs = 0.8f;
        p.body = dsp::BodyOud;
        p.bodyAmount = 0.5f;
        p.sympathetic = 0.08f;
        p.scoopSemis = -0.18f;
        p.scoopSeconds = 0.03f;
        p.release = 0.2f;
        p.even = 0.8f;
        p.spread = 0.3f;
        p.pan = -0.15f;
        p.voices = byQuality(q, 4, 6, 8);
        // As loud as the Desert's other instruments (measured, Tests "balance"); the dunes'
        // sparse phrases need the most.
        p.gain = dunes ? 1.9f : (storm ? 0.96f : 1.08f);
    }
    // Frame drums: a deep doum on the root, a sharp tek, a riq's jingles.
    {
        auto& p = drums.patch;
        auto& doum = p.rows[0];
        doum = {};
        doum.semis = -12;
        doum.bend = 1.1f;
        doum.decay = dunes ? 1.4f : 0.9f;
        doum.tone = 0.6f;
        doum.noise = 0.06f;
        doum.noiseHz = 500;
        doum.noiseDecay = 0.1f;
        doum.ratio = 1.6f;
        doum.partial = 0.3f;
        doum.pan = -0.1f;
        doum.kind = dsp::DrumMembrane;
        doum.position = 0.1f;
        doum.hardness = 0.25f;
        auto& tek = p.rows[1];
        tek = {};
        tek.semis = 12.0f + float(music::degree(frame.mood, 4));
        tek.bend = 1.03f;
        tek.decay = 0.2f;
        tek.tone = 0.3f;
        tek.noise = 0.35f;
        tek.noiseHz = lerpExp(1500.0f, 3000.0f, tone);
        tek.noiseQ = 1.2f;
        tek.noiseDecay = 0.06f;
        tek.ratio = 1.5f;
        tek.partial = 0.2f;
        tek.pan = 0.2f;
        tek.kind = dsp::DrumMembrane;
        tek.position = 0.95f;
        tek.hardness = 0.8f;
        auto& riq = p.rows[2];
        riq = {};
        riq.tuned = false;
        riq.hz = 100;
        riq.tone = 0.0f;
        riq.noise = 0.8f;
        riq.noiseHz = lerpExp(5000.0f, 9000.0f, tone);
        riq.noiseQ = 2.5f;
        riq.noiseDecay = 0.1f;
        riq.pan = 0.35f;
        p.spread = 0.15f;
        p.voices = 6;
        p.gain = 1.0f;
    }
    // The ney: a reed flute, very breathy, scooping into its notes (with Scoop / Glide on).
    {
        auto& p = ney.patch;
        p.h2 = 0.12f + 0.1f * tone;
        p.h3 = 0.05f;
        p.breath = 0.35f + 0.15f * (1.0f - tone);
        p.attack = 0.08f;
        p.release = dunes ? 0.5f : 0.2f;
        p.scoop = 1.2f;
        p.scoopSeconds = 0.12f;
        p.vibratoHz = 5.0f;
        p.vibratoDepth = (dunes ? 0.25f : 0.15f) + 0.1f * s.life;
        p.vibratoDelay = 0.3f;
        p.transpose = 12.0f;
        p.spread = 0.25f;
        p.pan = 0.15f;
        p.voices = 1;
        // The ney: an end-blown reed flute, more breath than tone at the edges, a soft
        // attack (no fipple chiff) and a pitch that leans and wanders.
        p.chiff = 0.12f;
        p.chiffSeconds = 0.06f;
        p.air = 0.3f;
        p.wander = 6.0f;
        p.gain = 1.0f;
    }
    // The tanpura: a buzzing drone whose bright jawari shimmers as the strings are plucked round.
    {
        auto& p = tanpura.patch;
        p.held = p.legato = true;
        p.attack = 1.5f;
        p.release = 2.0f;
        p.glide = 1.0f;
        p.detune = 4.0f;
        p.sub = 0.2f;
        const float pluck = 0.5f + 0.5f * std::sin(float(t) * twoPi * 0.55f);
        p.cutoff = lerpExp(250.0f, 900.0f, tone) * (1.0f + 0.8f * pluck * pluck) * (storm ? 0.8f : 1.0f);
        p.envOctaves = 0.0f;
        p.q = 2.2f;
        p.width = 0.6f;
        p.vibratoDepth = 0.0f;
        p.transpose = -12.0f;
        p.spread = 0.3f;
        p.voices = 3;
        p.gain = 1.0f;
    }
    // Wind: tuned breath, gusting.
    const float gust = 0.5f + 0.5f * drift.smooth(t, storm ? 0.4 : 0.12);
    {
        auto& p = wind.patch;
        p.tone = 0.0f;
        p.breath = 1.0f;
        p.breathQ = lerp(6.0f, 12.0f, tone) * (storm ? 0.9f : 1.0f);
        p.attack = 0.5f;
        p.release = 1.5f;
        p.scoop = 0.0f;
        p.vibratoHz = 0.3f;
        p.vibratoDepth = storm ? 0.4f : 0.15f;
        p.spread = 0.9f;
        p.voices = 3;
        p.held = true;
        p.gain = 0.4f + 1.0f * gust * gust;
    }
    // Metal chimes: small brass bells, partials a quarter-tone off any note.
    {
        auto& p = chimes.patch;
        p.set({ 1.0f, 2.32f, 4.25f }, { 1.0f, 0.6f, 0.4f }, { 1.0f, 0.4f, 0.25f * (0.4f + tone) });
        p.decay = dunes ? 3.0f : 1.6f;
        p.hardness = 0.7f + 0.2f * tone;
        p.toneHz = lerpExp(5000.0f, 13000.0f, tone);
        p.transpose = 24.0f;
        p.spread = 0.8f;
        p.voices = byQuality(q, 3, 4, 5);
        p.gain = 1.0f;
    }
    // Tension: the wind rises and the drums press in, the oud and ney grow sparse and
    // distant; Life brings the chimes and the ney's breath; Machinery the drummers.
    // The open dunes echo far; the oasis is close and sheltered; the storm swallows echoes.
    sendScale.fill(dunes ? 1.7f : (storm ? 0.8f : 0.6f));
    level = { 1.2f * (1.0f - 0.3f * tension), 0.5f * (0.7f + 0.6f * tension) * (0.7f + 0.6f * s.machinery) * (dunes ? 0.8f : 1.0f),
              0.1f * lerp(0.8f, 1.2f, s.life) * (1.0f - 0.3f * tension), 0.04f, 1.6f * (storm ? 1.25f : 1.0f) * (0.4f + 1.6f * tension),
              0.14f * lerp(0.6f, 1.4f, s.life) };

    // A wide, dry space, and a long sparse echo (a dotted half off the dunes).
    space.set(lerp(0.6f, 1.0f, s.size), lerp(0.6f, 1.6f, s.size) * (1.0f + 0.3f * s.magic), lerpExp(2500.0f, 8000.0f, tone) * (storm ? 0.7f : 1.0f));
    roomSend = 0.25f + 0.15f * s.fxDepth;
    const float beatSeconds = 60.0f / std::max(c.bpm, 20.0f);
    echo.set(std::min(3.0f * beatSeconds, 2.2f), (dunes ? 0.45f : 0.3f) + 0.25f * s.fxDepth, lerpExp(1500.0f, 4500.0f, tone), 0.4f);
    echoSend = (dunes ? 0.5f : (oasis ? 0.3f : 0.2f)) * (0.5f + s.fxDepth);
    dry = lerp(1.0f, 0.7f, s.size);
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era) * (storm ? 0.7f : 1.0f) * (1.0f - 0.3f * tension), rate);
    presence = 1.2f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
}

bool DesertPlace::shape(const PlaceContext&, const Scheduled& x, NoteOn& on) {
    // The chimes hang all round the camp.
    if (x.element == Chimes) {
        on.hasPan = true;
        on.pan = 0.7f * (2.0f * on.random - 1.0f);
    }
    return true;
}

void DesertPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        // The oud, the ney and the chimes call out over the dunes.
        const float el = busL[Oud][k] + busL[Ney][k] + busL[Chimes][k], er = busR[Oud][k] + busR[Ney][k] + busR[Chimes][k];
        float yl = 0, yr = 0, wl = 0, wr = 0;
        echo.process(el * echoSend, er * echoSend, yl, yr);
        space.process(l * roomSend, r * roomSend, wl, wr);
        // Wide: the space spreads further than the camp.
        l = dry * l + 0.6f * yl + 0.5f * (wl - 0.3f * wr);
        r = dry * r + 0.6f * yr + 0.5f * (wr - 0.3f * wl);
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
