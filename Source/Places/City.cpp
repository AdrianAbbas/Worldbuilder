#include "Places.h"

namespace wb {

using namespace dsp;

CityPlace::CityPlace() : EnsemblePlace(1500) { setParts({ &chimes, &bass, &piano, &crowd, &sirens, &traffic }); }

void CityPlace::prepareFx() {
    room.prepare(rate);
    chorus.prepare(rate);
    for (auto& r : rumble) r.setCutoff(110.0f, rate);
}

void CityPlace::resetFx() {
    room.clear();
    chorus.clear();
    for (auto& r : rumble) r.reset();
    for (auto& f : rain) f.reset();
    for (auto& f : tilt) f.reset();
    rumbleAt = 0;
}

void CityPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 1501);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size;
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Chimes) {
        // Muted bell chimes in the key, high above: a station chime, a lonely clock, rain
        // on a wind chime. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        switch (in.character) {
            case RushHour:   // a three-note platform chime every other bar, falling
                for (int bar = 0; bar < 4; bar += 2)
                    if (ch(1, bar) < 0.45f + 0.5f * act) {
                        const int top = 2 * size + (ch(2, bar) < 0.5f ? 4 : 2);
                        for (int k = 0; k < 3; ++k) w.note(bar * 16 + 2 * k, top - 2 * k, 4, (k == 2 ? 0.8f : 0.65f) * human(3, bar * 4 + k));
                    }
                if (ch(4, 0) < density * act) w.note(52, 2 * size, 2, 0.45f);
                break;
            case NightCity:   // a lone bell, now and then a falling pair
                for (int bar = 0; bar < 4; ++bar)
                    if (ch(5, bar) < 0.2f + 0.35f * act) {
                        const int d = size + 2 * int(ch(6, bar) * 3.0f);
                        w.note(bar * 16 + 4 * int(ch(7, bar) * 4.0f), d, 8, 0.6f * human(8, bar));
                        if (ch(9, bar) < life) w.note(bar * 16 + 14, d - 1, 2, 0.4f);
                    }
                break;
            default:   // Rain-soaked: soft, irregular chimes like rain on a wind chime
                for (int step = 0; step < 64; step += 2)
                    if (ch(10, step) < (0.06f + 0.18f * act) * lerp(0.6f, 1.4f, density)) w.note(step, size + int(ch(11, step) * float(size + 3)), 4, 0.35f + 0.25f * ch(12, step));
                break;
        }
        return;
    }

    if (element == Bass) {
        // The synth bass, two octaves below, on the chord. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = -24;
        switch (in.character) {
            case RushHour: {   // pulsing eighths with octave jumps
                for (int step = 0; step < out.lengthSteps; step += 2) {
                    const bool strong = step % 8 == 0;
                    if (ch(20, step) >= (strong ? 0.97f : (0.6f + 0.35f * density) * busy)) continue;
                    const bool octave = (step % 8 == 6) || (step % 16 == 10 && ch(21, step) < 0.5f);
                    w.note(step, octave ? size : 0, 2, (strong ? 0.9f : 0.65f) * human(22, step));
                }
                if (ch(23, 0) < tension) w.note(30, 4, 2, 0.7f);
                break;
            }
            case NightCity: {   // a walking line in quarter notes: chord tones and passing notes
                static constexpr int walk[2][4] { { 0, 2, 4, 5 }, { 4, 2, 1, -1 } };
                for (int beat = 0; beat < 8; ++beat) {
                    if (beat % 4 != 0 && ch(24, beat) >= 0.55f + 0.45f * busy) continue;
                    const int d = walk[beat / 4][beat % 4] + (ch(25, beat) < 0.15f * life ? size : 0);
                    w.note(beat * 4, d, 3, (beat % 2 == 0 ? 0.85f : 0.7f) * human(26, beat));
                }
                break;
            }
            default:   // Rain-soaked: long, soft notes
                w.note(0, 0, 14, 0.75f);
                if (ch(27, 0) < 0.4f + 0.5f * act) w.note(16, ch(28, 0) < 0.5f ? 4 : 2, 14, 0.65f);
                else w.note(16, 0, 14, 0.65f);
                break;
        }
        return;
    }

    if (element == Piano) {
        // Electric-piano chords on the shared chord (sevenths, rootless ninths). Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -2;
        const auto chord = [&](int step, int length, float velocity, bool rich) {
            const int voicing = int(ch(30, step) * 2.0f);
            static constexpr int seventh[4] { 0, 2, 4, 6 }, ninth[4] { 2, 4, 6, 8 };
            const int* d = voicing == 0 || !rich ? seventh : ninth;
            for (int k = 0; k < (rich ? 4 : 3); ++k) w.note(step, d[k + (rich ? 0 : 1)], length, velocity * (k == 0 ? 1.0f : 0.85f));
        };
        switch (in.character) {
            case RushHour:   // syncopated stabs
                for (int bar = 0; bar < 4; ++bar) {
                    static constexpr int stabs[5] { 0, 3, 6, 10, 12 };
                    for (int k = 0; k < 5; ++k)
                        if (ch(31, bar * 8 + k) < (k == 0 ? 0.9f : (0.25f + 0.5f * density) * busy)) chord(bar * 16 + stabs[k], 2, (k == 0 ? 0.75f : 0.55f) * human(32, bar * 8 + k), false);
                }
                break;
            case NightCity:   // slow, rich chords, sometimes anticipating the bar
                for (int bar = 0; bar < 4; bar += 2) {
                    const bool early = bar > 0 && ch(33, bar) < 0.4f;
                    chord(bar * 16 - (early ? 2 : 0), 28, 0.6f * human(34, bar), true);
                    if (ch(35, bar) < act * 0.6f) chord(bar * 16 + 22, 6, 0.4f, true);
                }
                break;
            default: {   // Rain-soaked: soft broken chords in eighths (sixteenths when tense)
                static constexpr int broken[8] { 0, 4, 2, 6, 4, 2, 8, 4 };
                for (int step = 0; step < 64; ++step) {
                    const bool eighth = (step & 1) == 0;
                    const float p = eighth ? (step % 8 == 0 ? 0.85f : (0.3f + 0.5f * density) * busy) : std::max(0.0f, tension - 0.4f) * density;
                    if (ch(36, step) < p) w.note(step, broken[(step / 2) % 8] + (eighth ? 0 : 2), eighth ? 4 : 2, (0.45f + 0.15f * float(step % 8 == 0)) * human(37, step));
                }
                break;
            }
        }
        return;
    }

    if (element == Crowd) {
        // The crowd murmurs on the chord tones, all four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = -7;
        w.note(0, 0, 64, 0.85f);
        w.note(0, 2, 64, 0.75f);
        w.note(0, 4, 64, 0.75f);
        if (in.character == RushHour || act > 0.6f) w.note(0, size, 64, 0.6f);
        return;
    }

    if (element == Sirens) {
        // A siren passing: its tone wails between two notes of the key, gliding. Eight
        // bars, at least one siren in them; busier and tenser cities have more.
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = false;
        const int low = size, high = size + music::nearestDegree(in.mood, 7);
        const auto wail = [&](int at, int tones, int steps, float velocity) {
            for (int k = 0; k < tones && at + k * steps < out.lengthSteps; ++k) w.note(at + k * steps, (k % 2 == 0) ? high : low, steps, velocity);
        };
        const int tones = 2 + int(std::lround(4.0f * std::max(act, tension)));
        switch (in.character) {
            case RushHour:
                wail(4 + 16 * int(ch(40, 0) * 3.0f), tones, 8, 0.8f);
                if (ch(41, 0) < 0.2f + 0.4f * act + 0.4f * tension) wail(68 + 16 * int(ch(42, 0) * 2.0f), tones, 8, 0.65f);
                break;
            case NightCity:   // a distant, slower wail
                wail(16 + 16 * int(ch(43, 0) * 4.0f), std::max(2, tones - 1), 16, 0.7f);
                if (ch(44, 0) < tension) wail(100, 2, 12, 0.5f);
                break;
            default:
                wail(32 + 16 * int(ch(45, 0) * 3.0f), std::max(2, tones - 2), 16, 0.6f);
                if (ch(46, 0) < tension) wail(4, 2, 16, 0.5f);
                break;
        }
        return;
    }

    if (element == Traffic) {
        // The traffic hum: the root two octaves down and its fifth (not the chords).
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        w.note(0, -2 * size, 64, 1.0f);
        w.note(0, music::nearestDegree(in.mood, 7) - 2 * size, 64, 0.5f);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void CityPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool rush = s.character == RushHour, night = s.character == NightCity, wet = s.character == RainSoaked;

    // Chimes: muted bells, soft and short.
    {
        auto& p = chimes.patch;
        p.set({ 1.0f, 2.0f, 3.0f, 4.899f }, { 1.0f, 0.6f, 0.4f, 0.25f }, { 1.0f, 0.35f * (0.4f + tone), 0.2f * tone, 0.12f * tone });
        p.decay = (night ? 2.2f : (wet ? 1.6f : 0.9f)) * lerp(1.1f, 0.7f, tension);
        p.hardness = 0.25f + 0.35f * tone;
        p.toneHz = lerpExp(1800.0f, 9000.0f, tone);
        p.spread = 0.7f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.0f;
    }
    // Synth bass: a plucky low-pass that opens on each note.
    {
        auto& p = bass.patch;
        p.held = true;
        p.legato = false;
        p.attack = 0.004f;
        p.decay = rush ? 0.16f : 0.35f;
        p.sustain = rush ? 0.35f : 0.55f;
        p.release = 0.1f;
        p.detune = 8.0f;
        p.sub = 0.5f;
        p.cutoff = lerpExp(90.0f, 420.0f, tone) * (wet ? 0.7f : 1.0f) * (0.8f + 0.6f * s.era);
        p.envOctaves = 0.3f;
        p.filterEnvOctaves = rush ? 2.4f : 1.2f;
        p.filterDecay = rush ? 0.06f : 0.12f;
        p.q = 1.3f + 1.2f * tension;
        p.width = 0.2f;
        p.spread = 0.05f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Electric piano: a sine carrier at the note, its modulator at the same pitch, the
    // bell of the attack fading to a mellow tone.
    {
        auto& p = piano.patch;
        p.ratio = 1.0f;
        p.index = 1.0f + 1.6f * tone + (rush ? 0.5f : 0.0f) + 0.8f * s.era;   // a futuristic city glitters
        p.indexFloor = 0.12f;
        p.indexSeconds = 0.5f;
        p.feedback = 0.05f;
        p.attack = 0.002f;
        p.decay = (night ? 2.6f : (wet ? 2.0f : 1.0f));
        p.sustain = 0.0f;
        p.release = 0.4f;
        p.held = false;
        p.vibratoDepth = 0.0f;
        p.spread = 0.5f;
        p.voices = byQuality(q, 4, 6, 8);
        p.gain = 1.0f;
    }
    // The crowd: many voices murmuring syllables on the chord tones, never words.
    {
        auto& p = crowd.patch;
        p.vowel = 1.6f;
        p.vowelDrift = 1.5f;
        p.babble = lerp(2.5f, 5.0f, tension) * (rush ? 1.2f : 1.0f);
        p.chatter = 0.85f;
        p.attack = 1.0f;
        p.release = 1.5f;
        p.detune = 22.0f;
        p.vibratoDepth = 0.35f;
        p.brightness = (wet ? 0.1f : 0.3f) + 0.4f * tone;
        p.glide = 0.3f;
        p.spread = 0.9f;
        p.voices = byQuality(q, 3, 4, rush ? 6 : 5);
        p.gain = 1.0f;
    }
    // Sirens: a pure wail that glides between its two notes.
    {
        auto& p = sirens.patch;
        p.held = p.legato = true;
        p.attack = 0.35f;
        p.decay = 1.0f;
        p.sustain = 1.0f;
        p.release = night ? 2.0f : 1.2f;
        p.glide = rush ? 0.35f : 0.8f;
        p.detune = 2.0f;
        p.sub = 0.0f;
        p.cutoff = lerpExp(500.0f, 1600.0f, tone);
        p.envOctaves = 0.0f;
        p.q = 0.7f;
        p.width = 0.2f;
        p.vibratoHz = 5.5f;
        p.vibratoDepth = 0.08f;
        p.vibratoDelay = 0.2f;
        p.pan = drift.smooth(t + 20.0, 0.05) * 0.7f;   // it passes across
        p.spread = 0.0f;
        p.voices = 1;
        p.gain = 1.0f;
    }
    // Traffic hum: detuned low saws that swell as traffic passes.
    const float swell = 0.5f + 0.5f * drift.smooth(t, 0.12);
    {
        auto& p = traffic.patch;
        p.held = p.legato = true;
        p.attack = 2.0f;
        p.release = 2.0f;
        p.glide = 1.0f;
        p.detune = 14.0f;
        p.sub = 0.4f;
        p.cutoff = lerpExp(70.0f, 240.0f, tone) * (1.0f + 0.9f * swell);
        p.envOctaves = 0.0f;
        p.q = 0.9f;
        p.width = 0.8f;
        p.spread = 0.2f;
        p.voices = 2;
        p.gain = 0.7f + 0.6f * swell;
    }
    // Tension: the crowd grows agitated and the sirens come closer.
    const float crowdLevel = (rush ? 0.6f : (night ? 0.4f : 0.3f)) * lerp(0.5f, 1.4f, s.life) * (0.6f + 1.0f * tension);
    // Tension: sirens and footsteps echo down the streets (more in the reverbs and echoes).
    sendScale.fill(lerp(0.7f, 1.5f, tension));
    level = { 0.65f, 0.2f, 0.22f, crowdLevel, (wet ? 0.1f : 0.13f) * (0.5f + 1.5f * tension), 0.07f * (0.7f + 0.6f * s.machinery) };
    trafficLevel = level[Traffic] * s.element[Traffic] * (0.5f + swell);

    // A dense, short city reverb and a subtle chorus; the rain blurs both.
    room.set(lerp(0.15f, 0.5f, s.size), lerp(0.7f, 1.8f, s.size) * (wet ? 1.6f : 1.0f) * (1.0f + 0.3f * s.magic), lerpExp(2500.0f, 9000.0f, tone));
    roomSend = (wet ? 0.6f : 0.4f) + 0.2f * s.fxDepth;
    chorus.set(0.35f + 0.3f * s.fxDepth, 0.0025f, (wet ? 0.6f : 0.35f) + 0.2f * s.fxDepth);
    rainLevel = wet ? 0.006f * (0.5f + s.fxDepth) : 0.0f;
    for (auto& f : rain) f.set(lerpExp(3000.0f, 7000.0f, tone), 0.7f, rate);
    for (auto& f : tilt) f.setCutoff(lerpExp(850.0f, 18000.0f, tone), rate);
}

void CityPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        // Piano, chimes and crowd go through the chorus; the rest stays dry.
        float sl = busL[Piano][k] + busL[Chimes][k] + busL[Crowd][k], sr = busR[Piano][k] + busR[Chimes][k] + busR[Crowd][k];
        chorus.process(sl, sr);
        float l = sl + busL[Bass][k] + busL[Sirens][k] + busL[Traffic][k];
        float r = sr + busR[Bass][k] + busR[Sirens][k] + busR[Traffic][k];
        // The road's low rumble under the hum; rain when it pours.
        const uint64_t at = rumbleAt + uint64_t(i);
        l += 0.2f * rumble[0].tick(noise.white(at)) * trafficLevel;
        r += 0.2f * rumble[1].tick(noise.white(at + (1ull << 44))) * trafficLevel;
        if (rainLevel > 0.0f) {
            l += rain[0].bandpass(noise.white(at + (1ull << 45))) * rainLevel;
            r += rain[1].bandpass(noise.white(at + (1ull << 46))) * rainLevel;
        }
        float wl = 0, wr = 0;
        room.process(l * roomSend, r * roomSend, wl, wr);
        // Tone: from a muffled street to a glittering one.
        left[i] = tilt[2].lp(tilt[0].lp(l + 0.65f * wl));
        right[i] = tilt[3].lp(tilt[1].lp(r + 0.65f * wr));
    }
    rumbleAt += uint64_t(n);
}

} // namespace wb
