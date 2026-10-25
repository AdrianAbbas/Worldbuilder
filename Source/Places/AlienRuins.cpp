#include "Places.h"

namespace wb {

using namespace dsp;

AlienRuinsPlace::AlienRuinsPlace() : EnsemblePlace(3100) { setParts({ &crystals, &pulses, &choir, &drone, &ritual, &whispers }); }

void AlienRuinsPlace::prepareFx() {
    vast.prepare(rate);
    warp.prepare(rate, 1.6f, 3101);
    reverse.prepare(rate, 0.9f);
}

void AlienRuinsPlace::resetFx() {
    vast.clear();
    warp.clear();
    reverse.clear();
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
}

void AlienRuinsPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 3102);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Crystals) {
        // Eroded crystals: single tones and pairs ringing out, scattered, high. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 12;
        const float chanceOf = in.character == Dormant ? 0.06f + 0.2f * act : (in.character == Reawakening ? 0.12f + 0.35f * act : 0.1f + 0.3f * act);
        for (int step = 0; step < 64; step += 2) {
            if (step != 0 && ch(1, step) >= chanceOf * lerp(0.7f, 1.3f, density)) continue;
            const float pick = ch(2, step);
            const int d = pick < 0.3f ? 0 : (pick < 0.55f ? 2 : (pick < 0.8f ? 4 : size));
            w.note(step, d, 4, 0.65f * human(3, step));
            if (ch(4, step) < 0.3f + 0.4f * act) w.note(step + 3, d + 1, 3, 0.45f);
        }
        return;
    }

    if (element == Pulses) {
        // The ancient machine: a slow pulse on the root, quickening as it wakes. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        out.base = -12;
        int every = 8;
        float chanceOf = 0.2f + 0.6f * act;
        switch (in.character) {
            case Dormant: every = 8; chanceOf = 0.1f + 0.6f * act; break;
            case Reawakening: every = 4; chanceOf = 0.2f + 0.7f * busy; break;
            default: every = 2; chanceOf = 0.3f + 0.6f * busy; break;
        }
        for (int step = 0; step < 32; step += every) {
            if (step % 16 != 0 && ch(10, step) >= chanceOf) continue;
            const bool high = step % 16 == 12 && ch(11, step) < 0.4f;
            w.note(step, high ? fifth : 0, every / 2 + 1, (step % 16 == 0 ? 0.8f : 0.55f) * human(12, step));
        }
        if (act > 0.6f)
            for (int step = 3; step < 32; step += 8)
                if (ch(13, step) < act * density) w.note(step, 0, 1, 0.35f);
        return;
    }

    if (element == Choir) {
        // The alien choir: long held chords, heard backwards (the reverse echo).
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 0;
        const int notes = in.character == Guardian ? 3 : 2;
        static constexpr int voicing[3] { 0, 4, 2 };
        for (int k = 0; k < notes; ++k) w.note(0, voicing[k] + (k == 2 ? size : 0), 32, 0.6f);
        if (in.character != Dormant || ch(20, 0) < 0.5f + 0.5f * act)
            for (int k = 0; k < notes; ++k) w.note(32, voicing[k] + (k == 1 ? 1 : 0), 32, 0.55f);
        return;
    }

    if (element == SubDrone) {
        // The sub-harmonic drone: the root an octave below the bass, and its fifth.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = -36;
        w.note(0, 0, 64, 0.9f);
        if (in.character == Guardian || tension > 0.5f) w.note(0, fifth, 64, 0.4f + 0.3f * tension);
        return;
    }

    if (element == Ritual) {
        // Ritual percussion: a low skin (row 0), struck stone (row 1), a rattle (row 2). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        switch (in.character) {
            case Dormant:   // a distant drum, a stone
                w.note(0, 0, 1, 0.6f);
                if (ch(30, 0) < 0.3f + 0.6f * act) w.note(20, 1, 1, 0.45f);
                if (ch(31, 0) < act) w.note(28, 2, 1, 0.35f);
                break;
            case Reawakening:   // a heartbeat and stones answering
                for (int bar = 0; bar < 2; ++bar) {
                    w.note(bar * 16, 0, 1, 0.75f * human(32, bar));
                    w.note(bar * 16 + 3, 0, 1, 0.5f);
                    if (ch(33, bar) < 0.3f + 0.6f * busy) w.note(bar * 16 + 10, 1, 1, 0.5f);
                    if (ch(34, bar) < act * density) w.note(bar * 16 + 13, 2, 1, 0.4f);
                }
                break;
            default:   // Guardian: a ritual in 3+3+2, rattles between
                for (int bar = 0; bar < 2; ++bar)
                    for (int k : { 0, 3, 6, 8, 11, 14 }) {
                        const int at = bar * 16 + k;
                        w.note(at, k == 0 || k == 8 ? 0 : 1, 1, (k % 8 == 0 ? 0.85f : 0.6f) * human(35, at));
                        if (ch(36, at) < 0.2f + 0.6f * busy) w.note(at + 1, 2, 1, 0.35f);
                    }
                break;
        }
        if (tension > 0.6f) w.note(30, 0, 1, 0.5f + 0.3f * tension);
        return;
    }

    if (element == Whispers) {
        // Whispers: breathy formant-like hisses, tuned, drifting between two notes.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 24;
        w.note(0, 0, 24, 0.6f);
        w.note(28, fifth, 20, 0.5f);
        if (in.character != Dormant) w.note(52, 2, 12, 0.45f, 0, 0.4f + 0.6f * act);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void AlienRuinsPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const bool dormant = s.character == Dormant, waking = s.character == Reawakening, guardian = s.character == Guardian;

    // Eroded crystals: glassy partials, the lowest pair split (slow beating), each strike
    // a little different, the highs blooming after.
    {
        auto& p = crystals.patch;
        p.set({ 1.0f, 2.76f, 5.40f, 8.93f }, { 1.0f, 0.55f, 0.3f, 0.15f }, { 1.0f, 0.2f + 0.5f * tone, 0.4f * tone, 0.25f * tone });
        p.decay = dormant ? 3.0f : 2.2f;
        p.hardness = 0.3f;
        p.velocityHardness = 0.3f;
        p.toneHz = lerpExp(3000.0f, 11000.0f, tone);
        p.extras = { 1.3f + 1.5f * tension, 0.0f, 0.4f, 0.3f, 0.4f };
        p.spread = 0.9f;
        p.voices = byQuality(q, 3, 5, 6);
        p.gain = 1.0f;
    }
    // The machine pulses: a deep FM throb (sub-octave modulator), dark.
    {
        auto& p = pulses.patch;
        p.ratio = 0.5f;
        p.index = 1.0f + 1.5f * tone + 1.5f * tension;
        p.indexFloor = 0.3f;
        p.indexSeconds = 0.2f;
        p.feedback = 0.15f;
        p.attack = 0.01f;
        p.decay = guardian ? 0.35f : 0.6f;
        p.sustain = 0.0f;
        p.release = 0.2f;
        p.held = false;
        p.spread = 0.3f;
        p.voices = 3;
        p.gain = 1.0f;
    }
    // The alien choir: dark open vowels, wide and slow, heard mostly reversed.
    {
        auto& p = choir.patch;
        p.vowel = guardian ? 2.5f : 0.5f;
        p.vowelDrift = 1.0f;
        p.attack = 0.8f;
        p.release = 2.5f;
        p.detune = 18.0f;
        p.vibratoDepth = 0.2f;
        p.brightness = lerp(0.1f, 0.9f, tone);
        p.glide = 1.0f;
        p.spread = 0.9f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.0f;
    }
    // The sub drone: nearly pure, pressing down; its filter opens with Tension.
    {
        auto& p = drone.patch;
        p.held = p.legato = true;
        p.attack = 3.0f;
        p.release = 3.0f;
        p.glide = 2.0f;
        p.detune = 3.0f;
        p.sub = 1.0f;
        p.cutoff = 120.0f * (1.0f + 1.5f * tension);
        p.envOctaves = 0.0f;
        p.q = 0.9f;
        p.width = 0.4f;
        p.tremoloPerBeat = guardian ? 0.5f : 0.125f;
        p.tremoloDepth = 0.2f;
        p.spread = 0.1f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Ritual percussion: a deep skin, struck stone, a seed rattle.
    {
        auto& p = ritual.patch;
        auto& skin = p.rows[0];
        skin = {};
        skin.kind = DrumMembrane;
        skin.semis = -24.0f;
        skin.bend = 1.15f;
        skin.decay = 0.9f;
        skin.tone = 0.9f;
        skin.noise = 0.1f;
        skin.noiseHz = 500.0f;
        skin.noiseDecay = 0.03f;
        skin.position = 0.25f;
        skin.hardness = 0.35f;
        skin.pan = -0.2f;
        auto& stone = p.rows[1];
        stone = {};
        stone.semis = 12.0f;
        stone.bend = 1.0f;
        stone.decay = 0.15f;
        stone.tone = 0.6f;
        stone.ratio = 2.41f;
        stone.partial = 0.5f;
        stone.noise = 0.3f;
        stone.noiseHz = 2500.0f;
        stone.noiseQ = 1.2f;
        stone.noiseDecay = 0.02f;
        stone.pan = 0.3f;
        auto& rattle = p.rows[2];
        rattle = {};
        rattle.tuned = false;
        rattle.hz = 4000.0f;
        rattle.tone = 0.0f;
        rattle.noise = 0.3f;
        rattle.noiseHz = lerpExp(4000.0f, 8000.0f, tone);
        rattle.noiseQ = 1.0f;
        rattle.noiseDecay = 0.09f;
        rattle.decay = 0.1f;
        rattle.pan = 0.5f;
        p.spread = 0.3f;
        p.voices = 5;
        p.gain = 1.0f;
    }
    // Whispers: breath alone, narrow and vowel-like, drifting.
    {
        auto& p = whispers.patch;
        p.held = true;
        p.tone = 0.0f;
        p.breath = 1.0f;
        p.breathQ = 9.0f + 6.0f * tension;
        p.attack = 1.0f;
        p.release = 1.8f;
        p.scoop = 0.0f;
        p.vibratoHz = 0.4f;
        p.vibratoDepth = 0.6f;
        p.vibratoDelay = 0.0f;
        p.spread = 0.9f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    sendScale.fill(dormant ? 1.4f : (guardian ? 0.85f : 1.0f));
    level = { 0.14f * lerp(0.8f, 1.3f, s.magic), 0.14f * (dormant ? 0.8f : 1.0f) * (0.8f + 0.4f * s.machinery), 0.3f * (guardian ? 1.2f : 1.0f),
              0.05f * (0.8f + 0.5f * tension) * (guardian ? 1.7f : 1.0f), 0.35f * (dormant ? 0.7f : 1.0f) * (0.7f + 0.6f * tension), 1.5f * lerp(0.8f, 1.3f, s.life) * (waking ? 1.2f : 1.0f) };

    // A vast, strange reverb: a shimmer whose tail rises and keeps rising.
    vast.set(0.95f, lerp(4.0f, 9.0f, s.size) * (dormant ? 1.2f : 1.0f) * (1.0f + 0.3f * s.magic), (dormant ? 0.5f : 0.35f) + 0.4f * s.fxDepth);
    vastSend = (guardian ? 0.2f : 0.3f) + 0.25f * s.fxDepth;
    const float beat = 60.0f / std::max(c.bpm, 30.0f);
    warp.set(std::min(1.5f * beat, 1.5f), 0.35f + 0.3f * s.fxDepth, lerpExp(1500.0f, 5000.0f, tone), 0.03f + 0.05f * tension + (waking ? 0.03f : 0.0f));
    warpSend = (guardian ? 0.3f : 0.45f) * (0.6f + 0.6f * s.fxDepth);
    reverse.set(guardian ? 0.5f : 0.75f, 0.35f + 0.25f * s.fxDepth, lerpExp(1500.0f, 5000.0f, tone));
    reverseSend = 0.9f;
    for (auto& f : tilt) f.setCutoff(lerpExp(700.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era), rate);
    presence = 1.6f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
    dry = lerp(1.15f, 0.75f, s.size);
}

void AlienRuinsPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        // The choir: a little of it forwards, most of it backwards.
        float rl = 0, rr = 0;
        reverse.process(busL[Choir][k] * reverseSend, busR[Choir][k] * reverseSend, rl, rr);
        float l = 0.5f * busL[Choir][k] + rl, r = 0.5f * busR[Choir][k] + rr;
        for (int e : { Crystals, Pulses, SubDrone, Ritual, Whispers }) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        float dl = 0, dr = 0, wl = 0, wr = 0;
        warp.process((busL[Crystals][k] + busL[Pulses][k]) * warpSend, (busR[Crystals][k] + busR[Pulses][k]) * warpSend, dl, dr);
        l += dl;
        r += dr;
        vast.process(l * vastSend, r * vastSend, wl, wr);
        l = l * dry + 0.7f * wl;
        r = r * dry + 0.7f * wr;
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
