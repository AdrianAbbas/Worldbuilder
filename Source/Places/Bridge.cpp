#include "Places.h"

namespace wb {

using namespace dsp;

BridgePlace::BridgePlace() : EnsemblePlace(2900) { setParts({ &consoles, &core, &tactical, &brass, &chatter, &pings }); }

void BridgePlace::prepareFx() {
    hall.prepare(rate);
    intercom.prepare(rate);
    echo.prepare(rate, 1.5f);
    riserGain.setTime(0.05f, rate);
}

void BridgePlace::resetFx() {
    hall.clear();
    intercom.clear();
    echo.clear();
    for (auto& f : riser) f.reset();
    riserGain.snap(0);
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
}

void BridgePlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 2902);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Consoles) {
        // Command consoles: patterned arpeggios on the chord, in two voices that answer.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = 12;
        static constexpr int figure[8] { 0, 4, 2, 7, 4, 0, 2, 4 };
        const int every = in.character == BattleStations ? 1 : 2;
        const float chanceOf = in.character == Cruising ? 0.15f + 0.75f * act : 0.3f + 0.65f * act;
        for (int step = 0; step < 32; step += every) {
            if (step % 8 != 0 && ch(1, step) >= chanceOf * lerp(0.7f, 1.3f, density)) continue;
            const int d = figure[(step / every) % 8];
            w.note(step, d == 7 ? size : d, every, (step % 8 == 0 ? 0.7f : 0.5f) * human(2, step));
        }
        return;
    }

    if (element == WarpCore) {
        // The warp core: the root and fifth far below, held, with the chords.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -24;
        w.note(0, 0, 64, 0.9f);
        w.note(0, fifth, 64, 0.5f);
        return;
    }

    if (element == Tactical) {
        // The tactical pulse: a tuned pulse (row 0), ticks (row 1), a sub hit (row 2). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        for (int bar = 0; bar < 2; ++bar) {
            const int b = bar * 16;
            switch (in.character) {
                case Cruising:
                    w.note(b, 2, 1, 0.7f);
                    for (int k = 4; k < 16; k += 4)
                        if (ch(10, b + k) < 0.3f + 0.6f * busy) w.note(b + k, 0, 1, 0.5f * human(11, b + k));
                    if (ch(12, b) < act * density) w.note(b + 14, 1, 1, 0.4f);
                    break;
                case BattleStations:   // a driving 3-3-2 pulse under ticking sixteenths
                    for (int k : { 0, 3, 6, 8, 11, 14 }) w.note(b + k, k == 0 || k == 8 ? 2 : 0, 1, (k % 8 == 0 ? 0.9f : 0.65f) * human(13, b + k));
                    for (int k = 1; k < 16; k += 2)
                        if (ch(14, b + k) < 0.2f + 0.6f * busy) w.note(b + k, 1, 1, 0.4f * human(15, b + k));
                    break;
                default:   // Warp Jump: the pulse doubling as the build goes on (eight bars)
                    break;
            }
        }
        if (in.character == WarpJump) {
            out.setGrid(pat::Sixteenth, 8);
            for (int bar = 0; bar < 8; ++bar) {
                const int every = bar < 4 ? 8 : (bar < 6 ? 4 : 2);
                for (int k = 0; k < 16; k += every) {
                    if (k > 0 && ch(16, bar * 16 + k) >= 0.4f + 0.6f * busy) continue;
                    w.note(bar * 16 + k, k == 0 ? 2 : 0, 1, (0.5f + 0.06f * float(bar)) * human(17, bar * 16 + k));
                }
                if (bar >= 6 && ch(18, bar) < 0.3f + 0.7f * act)
                    for (int k = 1; k < 16; k += 2) w.note(bar * 16 + k, 1, 1, 0.35f);
            }
        }
        if (tension > 0.6f) w.note(in.character == WarpJump ? 120 : 30, 1, 1, 0.4f + 0.4f * tension);
        return;
    }

    if (element == Alerts) {
        // Brass alert swells: the chord's root and fifth, slowly rising in volume.
        out.followChords = true;
        out.base = -12;
        switch (in.character) {
            case Cruising:   // a distant horn now and then
                out.setGrid(pat::Sixteenth, 4);
                w.note(32, 0, 24, 0.5f, 0, 0.4f + 0.5f * act);
                if (ch(20, 0) < act) w.note(32, fifth, 24, 0.4f, 0, 0.5f);
                break;
            case BattleStations:   // an alert every two bars
                out.setGrid(pat::Sixteenth, 4);
                for (int at = 0; at < 64; at += 32) {
                    w.note(at, 0, 12, 0.75f * human(21, at));
                    w.note(at, fifth, 12, 0.6f);
                    if (ch(22, at) < 0.3f + 0.6f * busy) w.note(at + 16, 1, 8, 0.55f);
                }
                break;
            default:   // Warp Jump: one long swell over the last four bars to the jump
                out.setGrid(pat::Sixteenth, 8);
                w.note(64, 0, 64, 0.85f);
                w.note(64, fifth, 64, 0.7f);
                w.note(96, size, 32, 0.6f, 0, 0.3f + 0.7f * act);
                break;
        }
        if (tension > 0.5f && in.character != WarpJump) w.note(48, 1, 12, 0.35f + 0.3f * tension);
        return;
    }

    if (element == Chatter) {
        // Crew chatter: the bridge crew murmuring (abstract, no words), on the chord.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 0;
        const float chanceOf = in.character == BattleStations ? 0.6f + 0.4f * act : 0.25f + 0.6f * act;
        for (int at = 0; at < 64; at += 8) {
            if (at > 0 && ch(30, at) >= chanceOf * lerp(0.7f, 1.3f, density)) continue;
            w.note(at, ch(31, at) < 0.5f ? 0 : 2, 6, 0.6f * human(32, at));
        }
        return;
    }

    if (element == Pings) {
        // Sensor pings: a sonar-like note, high, every bar or two; a contact answers.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        out.base = 24;
        for (int bar = 0; bar < 4; ++bar) {
            const float chanceOf = in.character == BattleStations ? 0.5f + 0.5f * act : 0.2f + 0.6f * act;
            if (bar > 0 && ch(40, bar) >= chanceOf) continue;
            w.note(bar * 16, bar % 2 == 0 ? 0 : fifth, 8, 0.7f * human(41, bar));
            if (ch(42, bar) < act * density) w.note(bar * 16 + 10, size, 4, 0.4f);
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void BridgePlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const bool cruising = s.character == Cruising, battle = s.character == BattleStations, warp = s.character == WarpJump;
    // Where the warp build is (0..1 over eight bars).
    const float build = warp ? float(std::fmod(std::max(c.beat, 0.0), buildBars * 4.0) / (buildBars * 4.0)) : 0.0f;

    // Consoles: glassy FM tones, a short ring.
    {
        auto& p = consoles.patch;
        p.ratio = 4.0f;
        p.index = 0.6f + 1.2f * tone + 0.5f * tension;
        p.indexFloor = 0.25f;
        p.indexSeconds = 0.15f;
        p.feedback = 0.0f;
        p.attack = 0.002f;
        p.decay = battle ? 0.12f : 0.25f;
        p.sustain = 0.0f;
        p.release = 0.1f;
        p.held = false;
        p.spread = 0.7f;
        p.voices = byQuality(q, 3, 5, 6);
        p.gain = 1.0f;
    }
    // The warp core: a deep throbbing drone that spins up through the build.
    {
        auto& p = core.patch;
        p.held = p.legato = true;
        p.attack = 1.5f;
        p.release = 2.5f;
        p.glide = 1.0f;
        p.detune = 9.0f;
        p.sub = 0.7f;
        p.cutoff = lerpExp(110.0f, 420.0f, tone) * (1.0f + 2.5f * build) * (0.8f + 0.4f * s.machinery);
        p.envOctaves = 0.0f;
        p.q = 1.5f + 2.0f * tension;
        p.width = 0.6f;
        p.tremoloPerBeat = warp ? 1.0f + 7.0f * build * build : (battle ? 2.0f : 1.0f);
        p.tremoloDepth = (cruising ? 0.2f : 0.35f) + 0.2f * tension;
        p.spread = 0.2f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Tactical: a tuned electronic pulse, a tick and a sub hit.
    {
        auto& p = tactical.patch;
        auto& pulse = p.rows[0];
        pulse = {};
        pulse.semis = -12.0f;
        pulse.bend = 1.05f;
        pulse.decay = 0.18f;
        pulse.tone = 0.8f;
        pulse.ratio = 2.0f;
        pulse.partial = 0.3f + 0.3f * tone;
        pulse.noise = 0.05f;
        pulse.noiseHz = 2000.0f;
        pulse.noiseDecay = 0.01f;
        auto& tick = p.rows[1];
        tick = {};
        tick.semis = 24.0f;
        tick.bend = 1.0f;
        tick.decay = 0.03f;
        tick.tone = 0.4f;
        tick.ratio = 1.5f;
        tick.partial = 0.5f;
        tick.noise = 0.25f;
        tick.noiseHz = 7000.0f;
        tick.noiseQ = 2.0f;
        tick.noiseDecay = 0.015f;
        tick.pan = 0.35f;
        auto& sub = p.rows[2];
        sub = {};
        sub.kind = DrumMembrane;
        sub.semis = -24.0f;
        sub.bend = 1.8f;
        sub.decay = 0.7f;
        sub.tone = 1.0f;
        sub.noise = 0.05f;
        sub.noiseHz = 400.0f;
        sub.noiseDecay = 0.02f;
        sub.position = 0.0f;
        sub.hardness = 0.4f;
        p.spread = 0.2f;
        p.voices = 5;
        p.gain = 1.0f;
    }
    // Alert brass: a synth-brass section swelling in slowly, brighter with Tension.
    {
        auto& p = brass.patch;
        p.model = SynthBrass;
        p.held = true;
        p.attack = warp ? 3.0f : 0.9f;
        p.decay = 1.0f;
        p.sustain = 1.0f;
        p.release = 1.2f;
        p.detune = 10.0f;
        p.sub = 0.2f;
        p.cutoff = lerpExp(600.0f, 1600.0f, tone) * (1.0f + 0.5f * tension);
        p.formantHz = 1100.0f;
        p.breath = 0.04f;
        p.vibratoDepth = 0.05f;
        p.spread = 0.5f;
        p.voices = 4;
        p.gain = 1.0f;
    }
    // Crew chatter: a murmuring group through the intercom (no words).
    {
        auto& p = chatter.patch;
        p.vowel = 1.5f;
        p.vowelDrift = 1.2f;
        p.babble = battle ? 7.0f : 5.0f;
        p.chatter = 0.85f;
        p.attack = 0.2f;
        p.release = 0.4f;
        p.detune = 14.0f;
        p.vibratoDepth = 0.05f;
        p.brightness = 0.6f;
        p.spread = 0.8f;
        p.voices = byQuality(q, 2, 3, 4);
        p.gain = 1.0f;
    }
    // Sensor pings: a pure tone with a long ring and a faint fifth above.
    {
        auto& p = pings.patch;
        p.set({ 1.0f, 1.5f, 2.0f }, { 1.0f, 0.4f, 0.3f }, { 1.0f, 0.08f, 0.15f * tone });
        p.decay = cruising ? 2.5f : 1.6f;
        p.hardness = 0.2f;
        p.velocityHardness = 0.1f;
        p.toneHz = lerpExp(4000.0f, 12000.0f, tone);
        p.spread = 0.8f;
        p.voices = 3;
        p.gain = 1.0f;
    }
    sendScale.fill(cruising ? 1.2f : (battle ? 0.8f : 1.0f));
    level = { 0.13f * lerp(0.8f, 1.2f, s.life), 0.08f * (0.7f + 0.6f * s.machinery) * (1.0f + 0.5f * build), 0.4f * (cruising ? 0.8f : 1.0f) * (0.75f + 0.5f * tension),
              0.10f * (battle ? 1.2f : 1.0f) * (0.7f + 0.6f * tension), 0.6f * lerp(0.8f, 1.3f, s.life), 0.3f * (0.8f + 0.4f * s.magic) };

    // The wide cinematic hall.
    hall.set(lerp(0.6f, 1.0f, s.size), lerp(2.0f, 4.5f, s.size) * (cruising ? 1.2f : 1.0f) * (1.0f + 0.3f * s.magic), lerpExp(3500.0f, 10000.0f, tone));
    hallSend = (cruising ? 0.5f : 0.4f) + 0.2f * s.fxDepth;
    width = 1.2f + 0.3f * s.size;
    intercom.set(lerpExp(400.0f, 600.0f, tone), lerpExp(2500.0f, 3800.0f, tone), 1.5f + tension, 0.8f, battle ? 0.5f : 0.2f);
    const float beat = 60.0f / std::max(c.bpm, 30.0f);
    echo.set(std::min(1.5f * beat, 1.4f), 0.35f + 0.2f * s.fxDepth, lerpExp(2000.0f, 6000.0f, tone));
    echoSend = 0.35f * (0.6f + 0.6f * s.fxDepth);
    // The riser: a noise band sweeping up. In a warp jump it spans the build; otherwise a
    // short one closes every eight bars, deeper with Tension.
    float x = build;
    if (!warp) {
        const float cycle = float(std::fmod(std::max(c.beat, 0.0), 32.0));
        x = cycle >= 24.0f ? (cycle - 24.0f) / 8.0f : 0.0f;
    }
    riserTarget = (warp ? 0.05f : 0.025f * (0.3f + tension)) * x * x * x * (0.6f + 0.6f * s.fxDepth);
    for (auto& f : riser) f.set(lerpExp(300.0f, 7000.0f, x), 2.5f, rate);
    for (auto& f : tilt) f.setCutoff(lerpExp(1000.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era), rate);
    presence = 1.2f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
    dry = lerp(1.15f, 0.75f, s.size);
}

void BridgePlace::mix(const PlaceContext& c, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        // The crew through the intercom.
        float il = busL[Chatter][k], ir = busR[Chatter][k];
        intercom.process(il, ir);
        float l = il, r = ir;
        for (int e : { Consoles, WarpCore, Tactical, Alerts, Pings }) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        const uint64_t at = c.clock + uint64_t(i);
        const float g = riserGain.next(riserTarget);
        l += riser[0].bandpass(noise.white(at)) * g;
        r += riser[1].bandpass(noise.white(at + (1ull << 44))) * g;
        float el = 0, er = 0, wl = 0, wr = 0;
        echo.process((busL[Consoles][k] + busL[Pings][k]) * echoSend, (busR[Consoles][k] + busR[Pings][k]) * echoSend, el, er);
        l += el;
        r += er;
        hall.process(l * hallSend, r * hallSend, wl, wr);
        // Cinematic width: the hall spread wide.
        const float m = 0.5f * (wl + wr), sd = 0.5f * (wl - wr) * width;
        l = l * dry + 0.6f * (m + sd);
        r = r * dry + 0.6f * (m - sd);
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
