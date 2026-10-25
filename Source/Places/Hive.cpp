#include "Places.h"

namespace wb {

using namespace dsp;

HivePlace::HivePlace() : EnsemblePlace(1700) { setParts({ &pulses, &calls, &chitter, &throb, &formants, &swarm }); }

void HivePlace::prepareFx() {
    shimmer.prepare(rate);
    warp.prepare(rate, 1.6f, 1700);
}

void HivePlace::resetFx() {
    shimmer.clear();
    warp.clear();
    ring.setPhase(0.0f);
    for (auto& f : tilt) f.reset();
}

void HivePlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 1701);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size;
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Pulses) {
        // Organic pulses on the chord tones: breathing, quickening, racing. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = 0;
        for (int step = 0; step < out.lengthSteps; ++step) {
            const int pos = step % 16;
            float p = 0;
            int length = 1;
            switch (in.character) {
                case Dormant:   // slow breaths on every other beat, stirring with Activity
                    p = pos % 8 == 0 ? 0.9f : (pos % 8 == 4 ? 0.6f * act : (pos % 4 == 2 ? 0.35f * act * density : 0.0f));
                    length = 3;
                    break;
                case Awakening: {   // eighths, accented in threes against the bar
                    p = (pos & 1) == 0 ? (pos % 6 == 0 ? 0.95f : (0.45f + 0.45f * density) * busy) : 0.0f;
                    length = 2;
                    break;
                }
                default:   // Swarm: racing sixteenths
                    p = (pos & 1) == 0 ? 0.9f : (0.5f + 0.4f * density) * busy;
                    break;
            }
            p = std::min(1.0f, p + 0.25f * tension * float((pos & 1) == 0));
            if (p <= 0 || ch(1, step) >= p) continue;
            const int d = 2 * int(ch(2, step) * 3.0f);
            w.note(step, d, length, (pos % 4 == 0 ? 0.85f : 0.6f) * human(3, step));
        }
        return;
    }

    if (element == Calls) {
        // Glassy calls high in the key: a distant cry, calls answering each other, a
        // frantic chorus. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        const auto high = [&](uint32_t salt, int64_t at) { return size + int(ch(salt, at) * float(size + 2)); };
        switch (in.character) {
            case Dormant:   // a falling pair, once in a while
                for (int bar = 0; bar < 4; bar += 2)
                    if (ch(10, bar) < 0.4f + 0.5f * act + 0.5f * tension || bar == 0) {
                        const int d = high(11, bar) + 2;
                        w.note(bar * 16 + 4, d, 6, 0.6f);
                        w.note(bar * 16 + 10, d - 2, 10, 0.5f);
                    }
                break;
            case Awakening:   // a call, and an answer a step lower
                for (int bar = 0; bar < 4; bar += 2) {
                    if (ch(12, bar) >= 0.5f + 0.45f * act * lerp(0.7f, 1.3f, life) && bar != 0) continue;
                    const int d = high(13, bar);
                    for (int k = 0; k < 3; ++k) w.note(bar * 16 + 2 * k, d + k, 2, 0.65f);
                    for (int k = 0; k < 3; ++k) w.note(bar * 16 + 16 + 2 * k, d + k - 1, 2, 0.55f);
                }
                break;
            default:   // Swarm: short cries everywhere
                for (int step = 0; step < 64; step += 2)
                    if (ch(14, step) < (0.2f + 0.35f * act) * lerp(0.7f, 1.4f, life)) w.note(step, high(15, step) + size, 1 + int(ch(16, step) * 2.0f), 0.5f * human(17, step));
                break;
        }
        return;
    }

    if (element == Chitter) {
        // Clicks (row 0) and chirps (row 1) in bursts. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        for (int step = 0; step < out.lengthSteps; ++step) {
            const int pos = step % 16;
            float click = 0, chirp = 0;
            switch (in.character) {
                case Dormant:
                    click = (pos & 1) != 0 ? 0.05f + 0.45f * act * act : 0.0f;
                    chirp = pos == 14 ? 0.5f * act : 0.0f;
                    break;
                case Awakening: {   // bursts of three to five
                    const int burst = int(ch(20, step / 8) * 4.0f);
                    click = (pos % 8) < 3 + burst ? (0.5f + 0.4f * density) * busy : 0.0f;
                    chirp = pos == 12 ? 0.7f : 0.0f;
                    break;
                }
                default:   // a continuous rattle
                    click = (0.65f + 0.3f * density) * busy;
                    chirp = pos % 4 == 3 ? 0.5f * busy : 0.0f;
                    break;
            }
            click += tension * 0.3f;
            if (ch(21, step) < click) w.note(step, 0, 1, (pos % 4 == 0 ? 0.8f : 0.5f) * human(22, step));
            if (chirp > 0 && ch(23, step) < chirp) w.note(step, 1, 1, 0.7f * human(24, step));
        }
        return;
    }

    if (element == Throb) {
        // The throbbing bass, two octaves down: long notes; the part makes them throb.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = -24;
        switch (in.character) {
            case Dormant:
                w.note(0, 0, 32, 0.9f);
                break;
            case Awakening:
                w.note(0, 0, 16, 0.9f);
                w.note(16, ch(30, 0) < 0.5f ? 4 : 0, 16, 0.8f);
                break;
            default:
                for (int at = 0; at < 32; at += 8) w.note(at, at == 24 && ch(31, 0) < density ? size : 0, 8, at == 0 ? 0.95f : 0.8f);
                break;
        }
        return;
    }

    if (element == Formants) {
        // Formant drones on the root and fifth of the chord, the vowels morphing.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -12;
        w.note(0, 0, 64, 0.85f);
        w.note(0, 4, 64, 0.7f);
        if (in.character == Swarming || tension > 0.5f) w.note(0, 2 + size, 64, 0.55f);
        return;
    }

    if (element == Swarm) {
        // The swarm: a buzzing cluster of chord tones high above.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 12;
        w.note(0, 0, 64, 0.8f);
        w.note(0, 2, 64, 0.7f);
        w.note(0, 4, 64, 0.7f);
        if (in.character == Swarming) w.note(0, size, 64, 0.6f);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void HivePlace::configure(const PlaceContext& c) {
    const auto& w = *c.world;
    const auto& s = c.s;
    const int q = frame.quality, mood = w.mood;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool dormant = s.character == Dormant, swarming = s.character == Swarming;
    // How awake the hive is: its Character, and Tension (danger wakes it).
    const float stir = clamp01((dormant ? 0.0f : (swarming ? 1.0f : 0.5f)) + 0.6f * tension);

    // Pulses: wet, resonant plucks that wobble in pitch.
    {
        auto& p = pulses.patch;
        p.held = p.legato = false;
        p.attack = dormant ? 0.06f : 0.005f;
        p.decay = (dormant ? 0.7f : 0.22f) * lerp(1.1f, 0.7f, tension);
        p.sustain = 0.0f;
        p.detune = 10.0f;
        p.sub = 0.2f;
        p.cutoff = lerpExp(200.0f, 1100.0f, tone) * std::exp2(0.6f * drift.smooth(t, 0.2));
        p.envOctaves = 0.5f;
        p.filterEnvOctaves = 1.6f + stir;
        p.filterDecay = 0.1f;
        p.q = 2.6f + 1.5f * tension;
        p.width = 0.6f;
        p.vibratoHz = 6.5f;
        p.vibratoDepth = 0.25f;
        p.vibratoDelay = 0.0f;
        p.spread = 0.4f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.0f;
    }
    // Glass calls: FM at a stretched ratio, so the tone is glassy, never a chord.
    {
        auto& p = calls.patch;
        p.ratio = 4.5f;
        p.index = 0.8f + 1.4f * tone;
        p.indexFloor = 0.3f;
        p.indexSeconds = 0.8f;
        p.feedback = 0.0f;
        p.attack = 0.03f;
        p.decay = dormant ? 2.0f : 1.0f;
        p.sustain = 0.0f;
        p.release = 0.5f;
        p.vibratoHz = 6.0f + 4.0f * stir;
        p.vibratoDepth = 0.3f;
        p.spread = 0.85f;
        p.voices = byQuality(q, 2, 3, 4);
        p.gain = 1.0f;
    }
    // Chitter: tuned clicks high on the fifth and falling chirps.
    {
        auto& p = chitter.patch;
        auto& click = p.rows[0];
        click = {};
        click.semis = 36.0f + float(music::degree(mood, 4));
        click.bend = 1.0f;
        click.decay = 0.03f;
        click.tone = 0.3f;
        click.noise = 0.35f;
        click.noiseHz = lerpExp(3000.0f, 8000.0f, tone);
        click.noiseQ = 3.0f;
        click.noiseDecay = 0.02f;
        click.ratio = 2.755f;
        click.partial = 0.3f;
        click.pan = 0.4f;
        auto& chirp = p.rows[1];
        chirp = {};
        chirp.semis = 24.0f + float(music::degree(mood, 2));
        chirp.bend = 1.6f;
        chirp.decay = 0.09f;
        chirp.tone = 0.5f;
        chirp.noise = 0.05f;
        chirp.noiseHz = 4000;
        chirp.noiseQ = 2.0f;
        chirp.noiseDecay = 0.03f;
        chirp.ratio = 1.5f;
        chirp.partial = 0.2f;
        chirp.pan = -0.35f;
        p.spread = 0.5f;
        p.voices = 6;
        p.gain = 0.8f + 0.6f * s.machinery;
    }
    // The throbbing bass: a sub that pulses in time, faster as the hive wakes.
    {
        auto& p = throb.patch;
        p.held = true;
        p.legato = true;
        p.attack = 0.15f;
        p.decay = 0.5f;
        p.sustain = 0.85f;
        p.release = 0.5f;
        p.glide = 0.3f;
        p.detune = 5.0f;
        p.sub = 0.7f;
        p.cutoff = lerpExp(80.0f, 350.0f, tone);
        p.envOctaves = 0.3f;
        p.q = 1.5f;
        p.width = 0.3f;
        p.tremoloPerBeat = (dormant ? 1.0f : (swarming ? 4.0f : 2.0f)) * (tension > 0.5f ? 2.0f : 1.0f);
        p.tremoloDepth = 0.6f + 0.3f * tension;
        p.spread = 0.05f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Formant drones: vowels morphing slowly through every shape.
    {
        auto& p = formants.patch;
        p.vowel = 1.5f;
        p.vowelDrift = 2.2f;
        p.babble = 0.25f + 0.5f * stir;
        p.chatter = 0.0f;
        p.attack = 2.0f;
        p.release = 2.5f;
        p.detune = 15.0f;
        p.vibratoDepth = 0.25f;
        p.brightness = 0.3f + 0.5f * tone;
        p.glide = 0.8f;
        p.spread = 0.8f;
        p.voices = byQuality(q, 2, 3, 3);
        p.gain = 1.0f;
    }
    // The swarm: buzzing feedback FM that flutters.
    {
        auto& p = swarm.patch;
        p.held = true;
        p.ratio = 1.0f;
        p.index = 1.5f + tone;
        p.indexFloor = 1.0f;
        p.indexSeconds = 1.0f;
        p.feedback = 0.6f;
        p.attack = 1.5f;
        p.decay = 1.0f;
        p.sustain = 1.0f;
        p.release = 1.5f;
        p.glide = 0.5f;
        p.vibratoHz = 9.0f + 6.0f * stir + 4.0f * tension;
        p.vibratoDepth = 0.35f;
        p.spread = 1.0f;
        p.voices = byQuality(q, 3, 4, 4);
        p.gain = 1.0f;
    }
    // Dormant, the hive is a vast echoing cavity; the swarm fills it and deadens it.
    sendScale.fill(dormant ? 1.6f : (swarming ? 0.6f : 1.0f));
    level = { 0.12f * (0.8f + 0.6f * tension), 0.13f * lerp(0.7f, 1.3f, s.life), 2.2f, 0.16f, 0.28f * (0.5f + 1.2f * tension), (0.03f + 0.12f * stir) * (0.6f + 0.8f * tension) * lerp(0.7f, 1.3f, s.life) };

    // A shimmer whose tail is ring-modulated (a fluttering, metallic halo), and a warped
    // delay whose repeats bend.
    shimmer.set(0.85f, lerp(3.0f, 7.0f, s.size) * (1.0f + 0.3f * s.magic), 0.4f + 0.4f * s.fxDepth);
    shimmerSend = 0.25f + 0.25f * s.fxDepth;
    ring.setFrequency(lerp(7.0f, 33.0f, stir), rate);
    ring.normalise();
    ringDepth = 0.5f + 0.4f * stir;
    const float beatSeconds = 60.0f / std::max(c.bpm, 20.0f);
    warp.set(std::min(0.75f * beatSeconds, 1.4f), 0.3f + 0.35f * s.fxDepth + 0.15f * tension, lerpExp(1500.0f, 6000.0f, tone), 0.015f + 0.05f * tension + 0.02f * stir);
    delaySend = 0.3f + 0.3f * s.fxDepth;
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era), rate);
}

void HivePlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        // Calls and pulses into the warped delay; calls, formants and swarm into the
        // ring-modulated shimmer.
        float dl = 0, dr = 0, hl = 0, hr = 0;
        warp.process((busL[Calls][k] + busL[Pulses][k]) * delaySend, (busR[Calls][k] + busR[Pulses][k]) * delaySend, dl, dr);
        shimmer.process((busL[Calls][k] + busL[Formants][k] + busL[Swarm][k]) * shimmerSend, (busR[Calls][k] + busR[Formants][k] + busR[Swarm][k]) * shimmerSend, hl, hr);
        ring.step();
        const float mod = 1.0f - ringDepth + ringDepth * ring.im;
        l += 0.6f * dl + 0.7f * hl * mod;
        r += 0.6f * dr + 0.7f * hr * mod;
        left[i] = tilt[2].lp(tilt[0].lp(l));
        right[i] = tilt[3].lp(tilt[1].lp(r));
    }
}

} // namespace wb
