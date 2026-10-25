#include "Places.h"

namespace wb {

using namespace dsp;

MegacityPlace::MegacityPlace() : EnsemblePlace(2800) { setParts({ &bass, &arp, &pad, &drums, &stabs, &hum }); }

void MegacityPlace::prepareFx() {
    neon.prepare(rate);
    crush.prepare(rate, 2801);
    pingpong.prepare(rate, 1.2f);
}

void MegacityPlace::resetFx() {
    neon.clear();
    crush.clear();
    pingpong.clear();
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
    for (auto& f : rain) f.reset();
}

void MegacityPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2802);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == GritBass) {
        // The bass: the chord's root, low. Night Market: long notes with octave pops;
        // Corporate Tower: steady eighths; Riot: snarling sixteenths. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = -24;
        switch (in.character) {
            case NightMarket:
                for (int bar = 0; bar < 2; ++bar) {
                    const int at = bar * 16;
                    w.note(at, 0, 6, 0.85f * human(1, at));
                    if (ch(2, at) < 0.3f + 0.6f * busy) w.note(at + 7, size, 1, 0.6f * human(3, at));
                    if (ch(4, at) < 0.2f + 0.6f * busy) w.note(at + 10, 0, 4, 0.7f);
                    if (ch(5, at) < act * density) w.note(at + 14, fifth, 2, 0.55f);
                }
                break;
            case CorporateTower:
                for (int step = 0; step < 32; step += 2)
                    if (step % 8 == 0 || ch(6, step) < 0.2f + 0.75f * busy) w.note(step, step % 16 == 14 && ch(7, step) < 0.5f ? fifth : 0, 2, (step % 4 == 0 ? 0.75f : 0.55f) * human(8, step));
                break;
            default:   // Riot
                for (int step = 0; step < 32; ++step)
                    if (step % 4 == 0 || ch(9, step) < 0.15f + 0.65f * busy * lerp(0.7f, 1.2f, density))
                        w.note(step, (step % 8 == 7) ? size : (step % 16 == 11 ? 1 : 0), 1, (step % 4 == 0 ? 0.9f : 0.6f) * human(10, step));
                break;
        }
        return;
    }

    if (element == NeonArp) {
        // The neon arpeggio: chord tones in sixteenths, two octaves up, up and down. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = in.character == CorporateTower ? 24 : 12;
        static constexpr int up[8] { 0, 2, 4, 7, 9, 7, 4, 2 };
        const float chanceOf = in.character == Riot ? 0.3f + 0.6f * act : (in.character == NightMarket ? 0.15f + 0.75f * act : 0.25f + 0.7f * act);
        const int every = in.character == NightMarket ? 2 : 1;
        for (int step = 0; step < 32; step += every) {
            if (step % 8 != 0 && ch(20, step) >= chanceOf * lerp(0.7f, 1.3f, density)) continue;
            const int d = up[(step / every) % 8];
            w.note(step, d >= 7 ? size + d - 7 : d, every, (step % 4 == 0 ? 0.7f : 0.5f) * human(21, step));
        }
        return;
    }

    if (element == RainPad) {
        // The pad: the chord in three voices, held across four bars, following the changes.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 0;
        w.note(0, 0, 64, 0.7f);
        w.note(0, 2, 64, 0.55f);
        w.note(0, in.character == CorporateTower ? size + 1 : 4, 64, 0.5f);
        if (tension > 0.55f) w.note(32, 1, 32, 0.3f + 0.3f * tension);
        return;
    }

    if (element == Drums) {
        // The drum machine: kick (0), snare (1), hat (2), clap (3). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        for (int bar = 0; bar < 2; ++bar) {
            const int b = bar * 16;
            switch (in.character) {
                case NightMarket:   // half-time: kick, a snare on beat three, lazy hats
                    w.note(b, 0, 1, 0.9f);
                    if (ch(30, b) < 0.4f + 0.5f * busy) w.note(b + 10, 0, 1, 0.7f);
                    w.note(b + 8, 1, 1, 0.8f * human(31, b));
                    for (int h = 0; h < 16; h += 2)
                        if (ch(32, b + h) < 0.25f + 0.65f * busy) w.note(b + h, 2, 1, (h % 4 == 0 ? 0.55f : 0.35f) * human(33, b + h));
                    if (ch(34, b) < act * density) w.note(b + 15, 3, 1, 0.5f);
                    break;
                case CorporateTower:   // four on the floor, hats on the off-beats
                    for (int k = 0; k < 16; k += 4) {
                        w.note(b + k, 0, 1, 0.8f * human(35, b + k));
                        if (ch(36, b + k) < 0.4f + 0.6f * busy) w.note(b + k + 2, 2, 1, 0.5f);
                        if (ch(37, b + k) < act * act) w.note(b + k + 3, 2, 1, 0.3f);
                    }
                    if (ch(38, b) < 0.3f + 0.6f * act) w.note(b + 12, 3, 1, 0.55f);
                    break;
                default:   // Riot: a breakbeat
                    w.note(b, 0, 1, 1.0f);
                    w.note(b + 4, 1, 1, 0.9f);
                    w.note(b + 12, 1, 1, 0.9f);
                    if (ch(39, b) < 0.5f + 0.5f * busy) w.note(b + 10, 0, 1, 0.8f);
                    if (ch(40, b) < 0.3f + 0.5f * busy) w.note(b + 7, 0, 1, 0.6f);
                    for (int h = 0; h < 16; ++h)
                        if (ch(41, b + h) < (h % 2 == 0 ? 0.4f : 0.15f) + 0.55f * busy) w.note(b + h, 2, 1, (h % 2 == 0 ? 0.5f : 0.3f) * human(42, b + h));
                    if (ch(43, b) < 0.3f + 0.5f * act) w.note(b + 14, 1, 1, 0.45f);
                    break;
            }
        }
        if (tension > 0.6f) w.note(28, 3, 1, 0.4f + 0.4f * tension);
        return;
    }

    if (element == Stabs) {
        // Glitch stabs: short chords on syncopated sixteenths. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = 0;
        static constexpr int spots[6] { 3, 6, 11, 14, 19, 27 };
        const float chanceOf = in.character == Riot ? 0.35f + 0.6f * act : (in.character == CorporateTower ? 0.1f + 0.5f * act : 0.2f + 0.6f * act);
        for (int k = 0; k < 6; ++k) {
            const int at = spots[k];
            if (k > 0 && ch(50, at) >= chanceOf) continue;
            const float v = (k == 0 ? 0.75f : 0.6f) * human(51, at);
            w.note(at, 0, 1, v);
            w.note(at, 2, 1, v * 0.8f);
            w.note(at, ch(52, at) < tension ? 3 : 4, 1, v * 0.8f);
            // Glitch: a stuttered repeat.
            if (ch(53, at) < act * density) w.note(at + 1, 0, 1, v * 0.6f);
        }
        return;
    }

    if (element == Hum) {
        // The megastructure: the root two octaves down and the fifth, held four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = -24;
        w.note(0, 0, 64, 0.8f);
        w.note(32, fifth, 32, 0.5f);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void MegacityPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const bool market = s.character == NightMarket, tower = s.character == CorporateTower, riot = s.character == Riot;

    // The bass: detuned saws with a snapping filter, driven hard.
    {
        auto& p = bass.patch;
        p.held = true;
        p.legato = market;
        p.attack = 0.003f;
        p.decay = riot ? 0.15f : 0.4f;
        p.sustain = market ? 0.7f : 0.4f;
        p.release = 0.08f;
        p.detune = 12.0f;
        p.sub = 0.7f;
        p.cutoff = lerpExp(120.0f, 500.0f, tone) * (riot ? 1.4f : 1.0f);
        p.envOctaves = 2.5f + 1.5f * tension;
        p.filterEnvOctaves = 1.0f;
        p.filterDecay = riot ? 0.05f : 0.12f;
        p.q = 2.5f + 3.0f * tension;
        p.width = 0.15f;
        p.glide = 0.05f;
        p.spread = 0.05f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // The neon arp: a bright FM pluck, its index snapping shut.
    {
        auto& p = arp.patch;
        p.ratio = tower ? 3.0f : 1.0f;
        p.index = 1.5f + 2.5f * tone + 1.0f * tension;
        p.indexFloor = 0.1f;
        p.indexSeconds = 0.06f;
        p.feedback = riot ? 0.3f : 0.15f;
        p.attack = 0.001f;
        p.decay = tower ? 0.35f : 0.2f;
        p.sustain = 0.0f;
        p.release = 0.1f;
        p.held = false;
        p.spread = 0.5f;
        p.voices = byQuality(q, 3, 5, 6);
        p.gain = 1.0f;
    }
    // The rain-soaked pad: many detuned saws through a slow, soft filter.
    {
        auto& p = pad.patch;
        p.held = true;
        p.attack = 2.0f;
        p.release = 3.0f;
        p.decay = 1.0f;
        p.sustain = 1.0f;
        p.detune = 22.0f;
        p.sub = 0.1f;
        p.cutoff = lerpExp(500.0f, 2400.0f, tone) * (tower ? 1.3f : 1.0f);
        p.envOctaves = 0.0f;
        p.q = 0.8f;
        p.width = 1.0f;
        p.glide = 1.2f;
        p.vibratoHz = 0.3f;
        p.vibratoDepth = 0.05f;
        p.spread = 0.8f;
        p.voices = 4;
        p.gain = 1.0f;
    }
    // The drum machine: an 808-like kick, a noisy snare, a metallic hat and a clap.
    {
        auto& p = drums.patch;
        auto& kick = p.rows[0];
        kick = {};
        kick.kind = DrumMembrane;
        kick.semis = -24.0f;
        kick.bend = 2.2f;
        kick.decay = tower ? 0.35f : 0.55f;
        kick.tone = 1.0f;
        kick.noise = 0.05f;
        kick.noiseHz = 2000.0f;
        kick.noiseDecay = 0.01f;
        kick.position = 0.0f;
        kick.hardness = 0.6f;
        auto& snare = p.rows[1];
        snare = {};
        snare.kind = DrumMembrane;
        snare.semis = 0.0f;
        snare.bend = 1.2f;
        snare.decay = 0.15f;
        snare.tone = 0.4f;
        snare.noise = 0.5f;
        snare.noiseHz = lerpExp(1500.0f, 4000.0f, tone);
        snare.noiseQ = 0.6f;
        snare.noiseDecay = 0.12f;
        snare.snare = 0.8f;
        snare.hardness = 0.7f;
        auto& hat = p.rows[2];
        hat = {};
        hat.tuned = false;
        hat.hz = 3200.0f;
        hat.ratio = 1.47f;
        hat.partial = 0.6f;
        hat.decay = 0.03f;
        hat.tone = 0.1f;
        hat.noise = 0.35f;
        hat.noiseHz = lerpExp(6000.0f, 11000.0f, tone);
        hat.noiseQ = 1.2f;
        hat.noiseDecay = riot ? 0.04f : 0.03f;
        hat.pan = 0.3f;
        auto& clap = p.rows[3];
        clap = {};
        clap.tuned = false;
        clap.hz = 900.0f;
        clap.tone = 0.0f;
        clap.noise = 0.6f;
        clap.noiseHz = 1400.0f;
        clap.noiseQ = 1.4f;
        clap.noiseDecay = 0.1f;
        clap.decay = 0.1f;
        clap.pan = -0.25f;
        p.spread = 0.15f;
        p.voices = 6;
        p.gain = 1.0f;
    }
    // Glitch stabs: buzzy reeds, very short.
    {
        auto& p = stabs.patch;
        p.model = SynthReed;
        p.held = false;
        p.attack = 0.002f;
        p.decay = riot ? 0.09f : 0.14f;
        p.sustain = 0.0f;
        p.release = 0.05f;
        p.detune = 8.0f;
        p.sub = 0.0f;
        p.cutoff = lerpExp(700.0f, 1600.0f, tone);
        p.formantHz = 1600.0f;
        p.breath = 0.02f;
        p.spread = 0.6f;
        p.voices = byQuality(q, 4, 6, 8);
        p.gain = 1.0f;
    }
    // The megastructure: a huge bowed girder, far off, its overtones a little out.
    {
        auto& p = hum.patch;
        p.set({ 1.0f, 2.01f, 3.03f, 5.2f }, { 1.0f, 0.6f, 0.4f, 0.2f }, { 1.0f, 0.5f, 0.25f * (0.5f + tone), 0.1f });
        p.ring = 1.2f;
        p.rosin = 0.4f + 0.3f * tension;
        p.noisiness = 0.2f;
        p.attack = 1.5f;
        p.release = 2.0f;
        p.glide = 0.6f;
        p.vibratoHz = 0.2f;
        p.vibratoDepth = 0.04f;
        p.spread = 0.5f;
        p.voices = 2;
        p.held = true;
        p.gain = 1.0f;
    }
    sendScale.fill(market ? 1.2f : (riot ? 0.8f : 1.0f));
    level = { 0.10f * (riot ? 1.15f : 1.0f) * (0.8f + 0.4f * s.machinery), 0.10f * lerp(0.8f, 1.2f, s.life), 0.045f * (tower ? 1.2f : 1.0f),
              0.42f * (tower ? 0.85f : 1.0f) * (0.75f + 0.5f * tension), 0.06f * (riot ? 1.2f : 1.0f), 0.30f * (0.7f + 0.6f * s.machinery) };

    // Wet neon: a big bright reverb with a long tail (a street of glass and puddles).
    neon.set(lerp(0.5f, 1.0f, s.size), lerp(1.8f, 4.0f, s.size) * (market ? 1.2f : 1.0f) * (1.0f + 0.3f * s.magic), lerpExp(4000.0f, 12000.0f, tone));
    neonSend = (tower ? 0.35f : 0.45f) + 0.2f * s.fxDepth;
    // The crusher on the drums and stabs: Riot crushes hard.
    crush.set(riot ? 9000.0f : 16000.0f, riot ? 6.0f : 9.0f, lerpExp(5000.0f, 12000.0f, tone), (riot ? 0.6f : (market ? 0.3f : 0.15f)) * (0.6f + 0.6f * s.fxDepth));
    // A ping-pong on the arp and the stabs, dotted eighths.
    const float beat = 60.0f / std::max(c.bpm, 30.0f);
    pingpong.set(std::min(0.75f * beat, 1.1f), 0.4f + 0.2f * s.fxDepth, lerpExp(2500.0f, 7000.0f, tone));
    pingSend = (tower ? 0.5f : 0.35f) * (0.6f + 0.6f * s.fxDepth);
    drive = 1.0f + (riot ? 2.0f : 0.8f) * (0.5f + tension);
    rainLevel = (market ? 0.02f : 0.012f) * (0.5f + s.fxDepth);
    for (auto& f : rain) f.set(lerpExp(4000.0f, 8000.0f, tone), 0.7f, rate);
    for (auto& f : tilt) f.setCutoff(lerpExp(1000.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era), rate);
    presence = 1.2f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
    dry = lerp(1.15f, 0.75f, s.size);
}

void MegacityPlace::mix(const PlaceContext& c, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        // Drive on the bass: gritty.
        const float bl = softClip(busL[GritBass][k] * drive) / std::sqrt(drive), br = softClip(busR[GritBass][k] * drive) / std::sqrt(drive);
        // The drums and stabs through the crusher.
        float xl = busL[Drums][k] + busL[Stabs][k], xr = busR[Drums][k] + busR[Stabs][k];
        crush.process(xl, xr);
        // Rain on the pad.
        const uint64_t at = c.clock + uint64_t(i);
        const float padLevel = std::abs(busL[RainPad][k]) + std::abs(busR[RainPad][k]);
        const float rl = rain[0].bandpass(noise.white(at)) * rainLevel * std::min(1.0f, padLevel * 30.0f);
        const float rr = rain[1].bandpass(noise.white(at + (1ull << 44))) * rainLevel * std::min(1.0f, padLevel * 30.0f);
        float l = bl + xl + busL[NeonArp][k] + busL[RainPad][k] + busL[Hum][k] + rl;
        float r = br + xr + busR[NeonArp][k] + busR[RainPad][k] + busR[Hum][k] + rr;
        float pl = 0, pr = 0, wl = 0, wr = 0;
        pingpong.process((busL[NeonArp][k] + busL[Stabs][k]) * pingSend, (busR[NeonArp][k] + busR[Stabs][k]) * pingSend, pl, pr);
        l += pl;
        r += pr;
        neon.process(l * neonSend, r * neonSend, wl, wr);
        l = l * dry + 0.6f * wl;
        r = r * dry + 0.6f * wr;
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
