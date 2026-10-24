#include "Places.h"

namespace wb {

using namespace dsp;

SpaceportPlace::SpaceportPlace() : EnsemblePlace(1300) { setParts({ &pulses, &pads, &risers, &pa, &engine, &blips }); }

void SpaceportPlace::prepareFx() {
    hangar.prepare(rate);
    delay.prepare(rate, 1.5f);
    shimmer.prepare(rate);
    murmurGain.setTime(0.03f, rate);
    f1.setTime(0.04f, rate / float(chunk));
    f2.setTime(0.04f, rate / float(chunk));
    for (auto& r : rumble) r.setCutoff(140.0f, rate);
}

void SpaceportPlace::resetFx() {
    hangar.clear();
    delay.clear();
    shimmer.clear();
    murmurUntil = -1;
    murmurOsc.phase = 0;
    murmurGain.snap(0);
    murmurTarget = 0;
    f1.snap(600);
    f2.snap(1400);
    for (auto& f : formant) f.reset();
    murmurBand.reset();
    for (auto& r : rumble) r.reset();
    rumbleAt = 0;
}

void SpaceportPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 1301);
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

    if (element == Pulses) {
        // An arpeggio over the shared chord: up, down or up-down over eight steps.
        // Launch writes the whole eight-bar build, its accents rising.
        out.setGrid(pat::Sixteenth, in.character == 2 ? int(buildBars) : 4);
        out.followChords = true;
        out.base = int8_t(in.character == 1 ? -12 : 0);
        const int order = int(ch(2, 0) * 3.0f);
        for (int step = 0; step < out.lengthSteps; ++step) {
            const int pos = step % 16;
            const float cyclePos = float(step) / float(out.lengthSteps);
            float p = 0, accent = pos % 4 == 0 ? 1.0f : 0.65f;
            switch (in.character) {
                case 0: p = (pos % 4 == 0) ? 0.97f : (0.45f + 0.5f * density) * busy; break;
                case 1: p = (pos & 1) == 0 ? (0.2f + 0.35f * density) * busy : 0.05f * density; break;
                default: p = 0.97f; accent = pos % 4 == 0 ? 1.0f : 0.55f + 0.4f * cyclePos; break;
            }
            p = std::min(1.0f, p + 0.2f * tension);
            if (ch(1, step) >= p) continue;
            const int k = pos % 8;
            const int idx = order == 0 ? k : (order == 1 ? 7 - k : (k < 4 ? k : 8 - k));
            add(step, 2 * (idx % 3) + size * (idx / 3), 1, 0.8f * accent * (1.0f + (ch(4, step) - 0.5f) * (0.1f + 0.8f * life)));
        }
        return;
    }

    if (element == Risers) {
        // Kit rows: a pass-by, the launch build, a falling sweep. The note's length is
        // the riser's.
        out.setGrid(pat::Sixteenth, int(buildBars));
        out.followChords = false;
        // Rare events: a note at each place one may start, with its chance as the note's
        // probability, so the odds hold over a long cue rather than per eight bars.
        const auto chancy = [&](int step, int row, int steps, float probability) {
            add(step, row, steps, 0.8f);
            for (int i = 0; i < out.count; ++i)
                if (out.notes[std::size_t(i)].step == step) out.notes[std::size_t(i)].probability = uint8_t(std::clamp(int(std::lround(probability * 100.0f)), 1, 100));
        };
        if (in.character == 0) for (int bar = 0; bar < int(buildBars); bar += 4) chancy(bar * 16, 0, 32, 0.15f + 0.45f * act);
        else if (in.character == 1) for (int bar = 0; bar < int(buildBars); bar += 4) chancy(bar * 16, 2, 64, 0.1f + 0.3f * act);
        else add(0, 1, 16 * int(buildBars), 1.0f);
        return;
    }

    if (element == Announce) {
        // PA chimes (and on a busy hub an abstract murmur); Launch counts down.
        out.setGrid(pat::Sixteenth, int(buildBars));
        out.followChords = false;
        if (in.character == 2) {
            // Countdown beeps over the last two bars of the build, the last one higher.
            const int from = out.lengthSteps - 32;
            for (int step = from; step < out.lengthSteps; step += 4) add(step, step + 4 >= out.lengthSteps ? 2 * size + 4 : 2 * size, 1, 0.8f);
            return;
        }
        for (int bar = 0; bar < int(buildBars); bar += 2) {
            // Life: a livelier port announces more.
            const float p = (in.character == 0 ? 0.25f + 0.5f * act : 0.15f + 0.3f * act) * lerp(0.5f, 1.6f, life);
            if (ch(20, bar) >= p) continue;
            const int top = size + 4, mid = size + 2, low = size;
            const bool down = ch(21, bar) < 0.6f;
            const int notes[3] { down ? top : low, mid, down ? low : top };
            const int count = in.character == 1 ? 2 : 3;
            for (int k = 0; k < count; ++k) add(bar * 16 + 4 * k, notes[k], 4, 0.8f);
            if (in.character == 1 && ch(22, bar) < 0.5f) add(bar * 16 + 5, notes[1], 4, 0.4f);   // a broken stutter
        }
        return;
    }
    PatternWriter w { out };

    if (element == Pads) {
        // Four wide voices on the chord: root, fifth, the third an octave up and the
        // seventh. As Tension rises the third gives way to a semitone above the root.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -5;
        w.note(0, 0, 64, 0.8f);
        w.note(0, 4, 64, 0.8f);
        if (in.character != 1) {
            if (tension > 0.6f) w.note(0, size, 64, 0.7f, 1);
            else w.note(0, 2 + size, 64, 0.8f);
            w.note(0, 6, 64, 0.5f);
        }
        return;
    }

    if (element == Engines) {
        // A sub drone two octaves below on the root and the fifth (not the chords).
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        w.note(0, -2 * size, 64, 1.0f);
        w.note(0, music::nearestDegree(in.mood, 7) - 2 * size, 64, 0.5f);
        return;
    }

    if (element == Blips) {
        out.followChords = false;
        const auto high = [&](uint32_t salt, int64_t at) { return 2 * size + int(ch(salt, at) * float(size)); };
        if (in.character == 2) {
            // Telemetry through the build: every beat, then every eighth, then sixteenths.
            out.setGrid(pat::Sixteenth, int(buildBars));
            const int total = out.lengthSteps;
            for (int step = 0; step < total; ++step) {
                const float x = float(step) / float(total);
                const int every = x < 0.5f ? 4 : (x < 0.75f ? 2 : 1);
                if (step % every != 0 || ch(80, step) >= 0.5f + 0.5f * act) continue;
                w.note(step, 2 * size + ((step / every) % 2 == 0 ? 0 : 4), 1, 0.45f + 0.4f * x);
            }
            return;
        }
        out.setGrid(pat::Sixteenth, 2);
        for (int step = 0; step < out.lengthSteps; ++step) {
            if (in.character == 0) {
                // Busy Hub: chattering bursts of two to four blips.
                if ((step & 3) != 0 || ch(81, step) >= (0.2f + 0.45f * act) * lerp(0.6f, 1.4f, density)) continue;
                const int count = 2 + int(ch(82, step) * 3.0f);
                for (int k = 0; k < count && step + k < out.lengthSteps; ++k) w.note(step + k, high(83, step + k), 1, 0.55f + 0.35f * ch(84, step + k));
                step += count - 1;
            } else if ((step & 1) == 0) {
                // Abandoned Dock: a stray glitch now and then (its chance is the note's), and
                // sometimes a stutter.
                const int degree = high(86, step);
                const float chance = 0.08f + 0.2f * act;
                w.note(step, degree, 1, 0.5f, 0, chance);
                if (ch(87, step) < 0.4f) w.note(step + 1, degree, 1, 0.3f, 0, chance);
            }
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}


void SpaceportPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality, root = c.world->root;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool hub = s.character == 0, dock = s.character == 1, launch = s.character == 2;
    const double cyclePos = std::fmod(c.beat / (4.0 * buildBars), 1.0) + (c.beat < 0 ? 1.0 : 0.0);
    const float build = launch ? float(cyclePos) : 0.0f;

    // Pulses: a resonant low-pass pluck; the launch opens it through the build.
    {
        auto& p = pulses.patch;
        p.held = p.legato = false;
        p.attack = 0.002f;
        p.decay = (dock ? 0.45f : 0.16f) * lerp(1.2f, 0.7f, tension);
        p.sustain = 0.0f;
        p.detune = dock ? 14.0f : 5.0f;
        p.sub = 0.0f;
        p.cutoff = lerpExp(180.0f, 900.0f, tone) * (dock ? 0.6f : 1.0f) * lerp(1.3f, 0.6f, tension);
        p.envOctaves = 0.0f;
        p.filterEnvOctaves = launch ? 1.5f + 2.5f * build : 2.6f * (dock ? 0.6f : 1.0f);
        p.filterDecay = 0.07f;
        p.q = lerp(1.6f, 3.2f, s.machinery * 0.5f + tension * 0.5f);
        p.width = 0.3f;
        p.vibratoDepth = 0;
        p.spread = 0.25f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.0f;
    }
    // Pads: wide, slowly breathing, gliding with the chords.
    {
        auto& p = pads.patch;
        p.attack = 3.0f;
        p.release = 3.0f;
        p.held = p.legato = true;
        p.glide = 2.0f;
        p.detune = dock ? 22.0f : 11.0f;
        p.sub = 0.0f;
        p.cutoff = lerpExp(300.0f, 2200.0f, tone) * std::exp2(0.5f * drift.smooth(t, 0.07) + 1.5f * build) * (dock ? 0.6f : 1.0f);
        p.envOctaves = 0.0f;
        p.q = 0.7f;
        p.width = 1.0f;
        p.vibratoHz = dock ? 0.23f : 0.15f;
        p.vibratoDepth = dock ? 0.12f : 0.03f;
        p.vibratoDelay = 0;
        p.spread = 0.5f;
        p.voices = byQuality(q, 3, 4, 4);
        p.gain = 1.0f;
    }
    risers.patch.rows[0] = { SweepPassBy, -12.0f, 48.0f, 2.0f, 1.0f, false, true };
    risers.patch.rows[1] = { SweepBuild, -12.0f, 48.0f, 2.0f, 1.0f, true, false };
    risers.patch.rows[2] = { SweepFall, -12.0f, 48.0f, 2.0f, 1.0f, false, false };
    risers.patch.tension = tension;
    risers.patch.voices = 1;
    // PA tones: soft, nearly harmonic chimes (an abandoned dock's are flat and
    // broken); the launch counts down in short beeps.
    {
        auto& p = pa.patch;
        if (launch) {
            p.set({ 1.0f, 2.0f }, { 1.0f, 0.8f }, { 1.0f, 0.2f });
            p.decay = 0.25f;
            p.hardness = 1.0f;
        } else {
            p.set({ 1.0f, 2.0f, 3.0f, 4.12f }, { 1.0f, 0.55f, 0.3f, 0.2f }, { 1.0f, 0.3f, 0.12f, 0.06f });
            p.decay = 3.0f;
            p.hardness = 0.25f + 0.3f * tone;
        }
        p.transpose = dock ? -0.35f : 0.0f;
        p.pan = 0.2f;
        p.spread = 0.0f;
        p.toneHz = 12000.0f;
        p.voices = 4;
        p.gain = 1.0f;
    }
    // Engine: a sub drone that throbs at half notes; the launch swells it.
    {
        auto& p = engine.patch;
        p.attack = 2.0f;
        p.release = 2.0f;
        p.held = p.legato = true;
        p.glide = 0.5f;
        p.detune = 9.0f;
        p.sub = 0.3f;
        p.cutoff = lerpExp(90.0f, 260.0f, tone) * (1.0f + build);
        p.envOctaves = 0.0f;
        p.q = 1.1f;
        p.width = 0.8f;
        p.spread = 0.1f;
        p.transpose = dock ? 0.15f * drift.smooth(t + 40.0, 0.2) : 0.0f;
        p.tremoloPerBeat = 0.5f;
        p.tremoloDepth = std::min(0.25f + 0.45f * tension + 0.2f * s.machinery, 0.95f);
        p.voices = 2;
        engineLevel = (launch ? 0.6f + 1.2f * build * build : (dock ? 0.6f : 1.0f)) * lerp(0.7f, 1.6f, tension);
        p.gain = engineLevel;
    }
    // Data blips: tiny FM chirps, glassy on a busy hub, flat and grainy when abandoned.
    {
        auto& p = blips.patch;
        p.ratio = hub ? 3.0f : (dock ? 1.41f : 2.0f);
        p.index = 2.5f + 2.0f * tone;
        p.indexFloor = 0.1f;
        p.indexSeconds = 0.05f;
        p.attack = 0.001f;
        p.decay = dock ? 0.15f : 0.08f;
        p.sustain = 0;
        p.held = false;
        p.transpose = dock ? -0.3f : 0.0f;
        p.spread = 0.7f;
        p.voices = byQuality(q, 2, 3, 4);
        p.gain = 1.0f;
    }
    level = { 0.2f, 0.05f, 1.0f, launch ? 0.5f : 0.25f, 0.085f, 0.12f };

    // Murmur: two formants that jump per "syllable" and glide, never words.
    const bool murmuring = s.element[Announce] > 0.001f && c.beat < murmurUntil;
    if (murmuring || murmurGain.value > 1.0e-5f) {
        const double syllable = std::floor(c.beat * 3.0);
        const auto sy = uint64_t(int64_t(syllable) + (int64_t(1) << 40));
        f1.next(lerp(300.0f, 800.0f, drift.unit(sy * 2)));
        f2.next(lerp(900.0f, 2200.0f, drift.unit(sy * 2 + 1)));
        formant[0].set(f1.value, 6.0f, rate);
        formant[1].set(f2.value, 8.0f, rate);
        murmurBand.set(1300.0f, 0.5f, rate);
        murmurOsc.setFrequency(music::noteHz(root, -12.0f) * std::exp2(0.25f * drift.smooth(t, 0.8)), rate);
    }
    const float syllableEnv = std::pow(std::max(0.0f, std::sin(float(std::fmod(c.beat * 3.0, 1.0)) * pi)), 1.5f);
    murmurTarget = murmuring ? 0.08f * s.element[Announce] * syllableEnv * (0.6f + 0.4f * drift.unit(uint64_t(std::floor(c.beat)) + 99)) : 0.0f;
    engineLevel *= level[Engines] * s.element[Engines];

    // A metal hangar, a dotted-eighth ping-pong delay and a shimmer whose depth
    // follows Activity and Population.
    hangar.set(lerp(0.25f, 0.95f, s.size), lerp(1.0f, 4.5f, s.size) * (1.0f + 0.4f * s.magic), lerpExp(3000.0f, 9000.0f, tone));
    const float beatSeconds = 60.0f / std::max(c.bpm, 20.0f);
    delay.set(std::min(0.75f * beatSeconds, 1.4f), 0.25f + 0.4f * s.fxDepth, lerpExp(2000.0f, 6000.0f, tone));
    shimmer.set(0.8f, lerp(2.5f, 6.0f, s.size), 0.4f + 0.5f * s.fxDepth);
    delaySend = 0.6f * s.fxDepth;
    shimmerSend = 0.12f + 0.25f * s.fxDepth;
}

bool SpaceportPlace::shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) {
    const auto& s = c.s;
    if (x.element == Pulses) {
        if (s.character == 1) on.semis -= 0.25f * on.random2;
        on.velocity = std::min(on.velocity / 0.8f, 1.6f);
    } else if (x.element == Announce && s.character != 2) {
        // A chime on a bar line opens an announcement: on a busy hub (or a lively port)
        // an abstract murmur follows.
        const double inBar = on.beat / 4.0 - std::floor(on.beat / 4.0);
        if (!on.extra && inBar < 1.0e-6 && (s.character == 0 || s.life > 0.6f))
            murmurUntil = on.beat + 4.0 + 8.0 * double(on.random2) * double(0.5f + s.life);
    }
    return true;
}

void SpaceportPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        // A low rumble under the engine.
        const uint64_t at = rumbleAt + uint64_t(i);
        l += 0.25f * rumble[0].tick(noise.white(at)) * engineLevel;
        r += 0.25f * rumble[1].tick(noise.white(at + (1ull << 44))) * engineLevel;
        const float mg = murmurGain.next(murmurTarget);
        float cl = busL[Announce][k], cr = busR[Announce][k];
        if (mg > 1.0e-5f) {
            const float buzz = murmurOsc.tick();
            const float voice = murmurBand.bandpass(formant[0].bandpass(buzz) + 0.7f * formant[1].bandpass(buzz)) * mg;
            l += voice * 0.8f;
            r += voice;
            cl += voice * 0.8f;
            cr += voice;
        }
        const float sl = busL[Pulses][k] + cl + busL[Blips][k], sr = busR[Pulses][k] + cr + busR[Blips][k];
        float dl = 0, dr = 0, wl = 0, wr = 0, hl = 0, hr = 0;
        delay.process(sl * delaySend, sr * delaySend, dl, dr);
        shimmer.process((busL[Pads][k] + cl) * shimmerSend, (busR[Pads][k] + cr) * shimmerSend, hl, hr);
        hangar.process(l * 0.4f + 0.5f * dl, r * 0.4f + 0.5f * dr, wl, wr);
        left[i] = l + 0.7f * dl + 0.6f * wl + 0.5f * hl;
        right[i] = r + 0.7f * dr + 0.6f * wr + 0.5f * hr;
    }
    rumbleAt += uint64_t(n);
}

} // namespace wb
