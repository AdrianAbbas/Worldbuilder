#include "Places.h"

namespace wb {

using namespace dsp;

MiningPlace::MiningPlace() : EnsemblePlace(3000) { setParts({ &drill, &bass, &clanks, &dust, &hymn, &beacon }); }

void MiningPlace::prepareFx() {
    room.prepare(rate);
    slap.prepare(rate);
    grit.prepare(rate);
}

void MiningPlace::resetFx() {
    room.clear();
    slap.clear();
    grit.clear();
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
}

void MiningPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 3002);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Drill) {
        // The drill: a whining ostinato on the root, bursts of sixteenths that bite and
        // let go. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        out.base = 0;
        int burst = 0;
        for (int step = 0; step < 32; ++step) {
            float chanceOf = 0;
            switch (in.character) {
                case ShiftChange: chanceOf = step < 16 ? 0.3f + 0.6f * busy : 0.1f + 0.4f * busy; break;   // winding down
                case CaveIn: chanceOf = 0.4f + 0.55f * busy; break;
                default: chanceOf = 0.05f + 0.45f * busy; break;   // one drill far off
            }
            if (step % 8 == 0 || ch(1, step) < chanceOf * lerp(0.7f, 1.2f, density)) {
                // Bursts climb a step when the bit bites.
                const int d = burst % 4 == 3 ? 1 : 0;
                w.note(step, d, 1, (step % 4 == 0 ? 0.75f : 0.55f) * human(2, step));
                ++burst;
            } else {
                burst = 0;
            }
        }
        if (in.character == CaveIn && tension > 0.4f) w.note(28, fifth, 4, 0.6f);
        return;
    }

    if (element == GrindBass) {
        // The rock-grind bass: the chord's root, low and heavy, a grinding figure.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = -24;
        switch (in.character) {
            case ShiftChange:
                for (int bar = 0; bar < 2; ++bar) {
                    w.note(bar * 16, 0, 8, 0.8f * human(10, bar));
                    if (ch(11, bar) < 0.3f + 0.6f * busy) w.note(bar * 16 + 10, 0, 4, 0.6f);
                    if (ch(12, bar) < act * density) w.note(bar * 16 + 14, size, 2, 0.5f);
                }
                break;
            case CaveIn:
                for (int step = 0; step < 32; step += 2)
                    if (step % 8 == 0 || ch(13, step) < 0.2f + 0.7f * busy) w.note(step, step % 16 == 12 ? 1 : 0, 2, (step % 8 == 0 ? 0.9f : 0.6f) * human(14, step));
                break;
            default:   // Night Shift: long low notes
                w.note(0, 0, 16, 0.7f);
                if (ch(15, 0) < 0.3f + 0.6f * act) w.note(16, 0, 12, 0.6f);
                if (ch(16, 0) < act * density) w.note(28, fifth - size, 4, 0.5f);
                break;
        }
        return;
    }

    if (element == Clanks) {
        // Tuned metal clanks: tools, rails and pipes, ringing on the chord. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = 0;
        const float chanceOf = in.character == CaveIn ? 0.25f + 0.6f * act : (in.character == ShiftChange ? 0.15f + 0.5f * act : 0.05f + 0.35f * act);
        for (int step = 0; step < 32; ++step) {
            if (step != 4 && ch(20, step) >= chanceOf * 0.35f * lerp(0.7f, 1.3f, density)) continue;
            const float pick = ch(21, step);
            const int d = pick < 0.4f ? 0 : (pick < 0.7f ? 2 : (pick < 0.9f ? 4 : size));
            w.note(step, d, 1, (step % 4 == 0 ? 0.7f : 0.5f) * human(22, step));
            // Falling metal in a cave-in: a quick run down.
            if (in.character == CaveIn && ch(23, step) < tension * 0.5f && step < 28) {
                w.note(step + 1, d - 1, 1, 0.45f);
                w.note(step + 2, d - 2, 1, 0.35f);
            }
        }
        return;
    }

    if (element == Dust) {
        // Dust hiss: breath through the shafts, tuned to the root and fifth, held.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 24;
        w.note(0, 0, 32, 0.7f);
        w.note(32, fifth, 32, 0.6f);
        return;
    }

    if (element == Hymn) {
        // The workers' hymn: a low choir holding the chord, a slow line in the top voice.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -12;
        w.note(0, 0, 64, 0.7f);
        w.note(0, 4, 64, 0.55f);
        static constexpr int line[4] { 2, 3, 2, 1 };
        for (int k = 0; k < 4; ++k)
            if (k == 0 || ch(30, k) < 0.3f + 0.6f * act || in.character == ShiftChange)
                w.note(k * 16, size + line[k], 16, 0.55f * human(31, k));
        return;
    }

    if (element == Beacon) {
        // The beacon: a lonely call, two or three slow notes falling to the root. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 12;
        const float chanceOf = in.character == NightShift ? 0.5f + 0.5f * act : 0.2f + 0.5f * act;
        for (int phrase = 0; phrase < 2; ++phrase) {
            const int at = phrase * 32;
            if (phrase > 0 && ch(40, phrase) >= chanceOf) continue;
            w.note(at, 4, 6, 0.7f * human(41, at));
            w.note(at + 6, 2, 4, 0.6f);
            w.note(at + 10, 0, 12, 0.65f);
            if (ch(42, phrase) < act * density) w.note(at + 24, fifth, 6, 0.5f);
        }
        if (tension > 0.6f) w.note(56, 1, 6, 0.4f + 0.3f * tension);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void MiningPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const bool shift = s.character == ShiftChange, cavein = s.character == CaveIn, night = s.character == NightShift;

    // The drill: FM with feedback, a whining, buzzing bite.
    {
        auto& p = drill.patch;
        p.ratio = 1.0f;
        p.index = 1.2f + 1.5f * tone + 1.0f * tension;
        p.indexFloor = 0.6f;
        p.indexSeconds = 0.05f;
        p.feedback = 0.5f + 0.3f * tension;
        p.attack = 0.004f;
        p.decay = cavein ? 0.1f : 0.16f;
        p.sustain = 0.0f;
        p.release = 0.06f;
        p.held = false;
        p.vibratoHz = 22.0f;
        p.vibratoDepth = 0.15f;
        p.pan = -0.3f;
        p.spread = 0.2f;
        p.voices = byQuality(q, 2, 3, 4);
        p.gain = 1.0f;
    }
    // The rock-grind bass: a buzzing reed, heavy and low, driven in the mix.
    {
        auto& p = bass.patch;
        p.model = SynthReed;
        p.held = true;
        p.attack = 0.02f;
        p.decay = 0.4f;
        p.sustain = 0.8f;
        p.release = 0.2f;
        p.detune = 10.0f;
        p.sub = 0.7f;
        p.cutoff = lerpExp(500.0f, 1100.0f, tone);
        p.formantHz = 500.0f;
        p.breath = 0.08f;
        p.spread = 0.1f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Clanks: tuned steel, inharmonic and ringing.
    {
        auto& p = clanks.patch;
        p.set({ 1.0f, 2.32f, 4.25f, 6.63f }, { 1.0f, 0.6f, 0.35f, 0.2f }, { 1.0f, 0.6f, 0.4f * (0.5f + tone), 0.25f * tone });
        p.decay = night ? 1.8f : 1.1f;
        p.hardness = 0.65f;
        p.velocityHardness = 0.35f;
        p.toneHz = lerpExp(3000.0f, 10000.0f, tone);
        p.extras = { 0.0f, 0.0f, 0.4f, 0.0f, 0.25f };
        p.spread = 0.8f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.0f;
    }
    // Dust: tuned breath alone, a gritty hiss.
    {
        auto& p = dust.patch;
        p.held = true;
        p.tone = 0.0f;
        p.breath = 1.0f;
        p.breathQ = 3.0f + 3.0f * tension;
        p.attack = 1.2f;
        p.release = 1.5f;
        p.scoop = 0.0f;
        p.vibratoDepth = 0.0f;
        p.spread = 0.9f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // The hymn: low male voices on an "oo", warm and steady, a synthetic sheen.
    {
        auto& p = hymn.patch;
        p.vowel = 4.0f;
        p.vowelDrift = 0.3f;
        p.attack = 1.6f;
        p.release = 2.0f;
        p.detune = 12.0f;
        p.vibratoDepth = 0.08f;
        p.brightness = lerp(0.2f, 0.5f, tone);
        p.glide = 0.8f;
        p.spread = 0.6f;
        p.voices = byQuality(q, 3, 4, 5);
        p.gain = 1.0f;
    }
    // The beacon: a pure, ocarina-like lead with a slow wide vibrato.
    {
        auto& p = beacon.patch;
        p.tone = 1.0f;
        p.h2 = 0.04f;
        p.h3 = 0.02f;
        p.breath = 0.06f;
        p.air = 0.02f;
        p.chiff = 0.05f;
        p.attack = 0.15f;
        p.release = 0.8f;
        p.scoop = 0.3f;
        p.vibratoHz = 4.2f;
        p.vibratoDepth = 0.25f;
        p.vibratoDelay = 0.4f;
        p.wander = 2.0f;
        p.pan = 0.35f;
        p.spread = 0.1f;
        p.voices = 1;
        p.gain = 1.0f;
    }
    sendScale.fill(night ? 1.3f : (cavein ? 0.8f : 1.0f));
    level = { 0.20f * (0.7f + 0.6f * s.machinery) * (night ? 0.7f : 1.0f), 0.16f * (cavein ? 1.2f : 1.0f) * (0.8f + 0.4f * s.machinery),
              0.15f * (0.7f + 0.6f * tension), 0.7f * (night ? 1.3f : 1.0f), 0.15f * lerp(0.8f, 1.3f, s.life) * (shift ? 1.2f : 1.0f),
              0.09f * (night ? 1.2f : 1.0f) };

    // A gritty industrial room: small, hard, a little driven.
    room.set(lerp(0.15f, 0.5f, s.size), lerp(0.7f, 1.8f, s.size) * (night ? 1.3f : 1.0f) * (1.0f + 0.3f * s.magic), lerpExp(2500.0f, 7000.0f, tone));
    roomSend = 0.4f + 0.2f * s.fxDepth;
    grit.set((cavein ? 0.6f : 0.35f) * (0.6f + 0.6f * s.fxDepth));
    slap.set(lerp(0.09f, 0.18f, s.size), 0.25f + 0.15f * s.fxDepth, lerpExp(1500.0f, 4000.0f, tone));
    slapSend = (night ? 0.45f : 0.3f) * (0.6f + 0.6f * s.fxDepth);
    drive = 1.0f + (cavein ? 2.5f : 1.2f) * (0.5f + tension);
    for (auto& f : tilt) f.setCutoff(lerpExp(1000.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era), rate);
    presence = 1.2f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
    dry = lerp(1.15f, 0.75f, s.size);
}

void MiningPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        const float bl = softClip(busL[GrindBass][k] * drive) / std::sqrt(drive), br = softClip(busR[GrindBass][k] * drive) / std::sqrt(drive);
        // The machines through the grit.
        float ml = busL[Drill][k] + bl + busL[Clanks][k], mr = busR[Drill][k] + br + busR[Clanks][k];
        grit.process(ml, mr);
        float l = ml + busL[Dust][k] + busL[Hymn][k] + busL[Beacon][k];
        float r = mr + busR[Dust][k] + busR[Hymn][k] + busR[Beacon][k];
        float sl = 0, sr = 0, wl = 0, wr = 0;
        slap.process((busL[Drill][k] + busL[Clanks][k] + busL[Beacon][k]) * slapSend, (busR[Drill][k] + busR[Clanks][k] + busR[Beacon][k]) * slapSend, sl, sr);
        l += sl;
        r += sr;
        room.process(l * roomSend, r * roomSend, wl, wr);
        l = l * dry + 0.6f * wl;
        r = r * dry + 0.6f * wr;
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
