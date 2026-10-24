#include "Places.h"

namespace wb {

using namespace dsp;

namespace {
constexpr double buildBars = 8;   // Launch: the length of one build cycle
}

void SpaceportPlace::prepare(double sampleRate) {
    rate = float(sampleRate);
    hangar.prepare(rate);
    delay.prepare(rate, 1.5f);
    riser.gain.setTime(0.02f, rate);
    murmurGain.setTime(0.03f, rate);
    f1.setTime(0.04f, rate / float(chunk));
    f2.setTime(0.04f, rate / float(chunk));
    for (auto& r : rumble) r.setCutoff(140.0f, rate);
    reset();
}

void SpaceportPlace::reset() {
    for (auto& v : arp) v.reset();
    for (auto& v : pad) v.reset();
    for (auto& v : engine) v.reset();
    for (auto& b : chime) b.reset();
    impact.reset();
    beep.reset();
    riser.active = false;
    riser.gain.snap(0);
    for (auto& f : riser.band) f.reset();
    chimePhrase.clear();
    murmurUntil = -1;
    murmurGain.snap(0);
    f1.snap(600);
    f2.snap(1400);
    for (auto& f : formant) f.reset();
    murmurBand.reset();
    for (auto& r : rumble) r.reset();
    hangar.clear();
    delay.clear();
}

void SpaceportPlace::setSeed(uint32_t key) {
    pattern.stream.setKey(key, 1301);
    noise.setKey(key, 1302);
    drift.setKey(key, 1303);
}

void SpaceportPlace::onStep(const PlaceContext& c, int64_t step) {
    const auto& s = c.s;
    const int mood = c.world->mood, root = c.world->root, chord = c.chord;
    const int pos = stepInBar(step);
    const int64_t bar = music::floorDiv(step, int64_t(16));
    const float act = s.activity, density = s.density, tension = s.tension;
    const float busy = lerp(0.35f, 1.15f, act);
    const auto chance = [&](uint32_t salt, int64_t length = 64) { return pattern.at(step, length, s.variation, salt); };
    const double stepBeat = double(step) / 4.0;
    const double cyclePos = std::fmod(stepBeat / (4.0 * buildBars), 1.0) + (stepBeat < 0 ? 1.0 : 0.0);

    // --- Pulses: an arpeggio over the shared chord.
    if (s.element[0] > 0.001f) {
        float p = 0, accent = pos % 4 == 0 ? 1.0f : 0.65f;
        switch (s.character) {
            case 0: p = (pos % 4 == 0) ? 0.97f : (0.45f + 0.5f * density) * busy; break;
            case 1: p = (pos & 1) == 0 ? (0.2f + 0.35f * density) * busy : 0.05f * density; break;
            default: p = 0.97f; accent = pos % 4 == 0 ? 1.0f : 0.55f + 0.4f * float(cyclePos); break;
        }
        p = std::min(1.0f, p + 0.2f * tension);
        if (chance(1) < p) {
            // Arp order: up, down or up-down over eight steps (fixed per pattern pass).
            const int order = int(chance(2, 128 * 4) * 3.0f);
            const int k = pos % 8;
            const int idx = order == 0 ? k : (order == 1 ? 7 - k : (k < 4 ? k : 8 - k));
            int semis = music::chordTone(mood, chord, idx % 3) + 12 * (idx / 3);
            semis = music::inRange(music::chordTone(mood, chord, 0), 0) + (semis - music::chordTone(mood, chord, 0));
            if (s.character == 1) semis -= 12;
            auto& v = arp[pickVoice(arp, arpVoices, [](const SynthVoice& x) { return x.level(); })];
            logEvent(c.clock);
            const float detune = s.character == 1 ? -0.25f * chance(3) : 0.0f;
            const float decay = (s.character == 1 ? 0.45f : 0.16f) * lerp(1.2f, 0.7f, tension);
            v.start(music::noteHz(root, float(semis) + detune), 0.002f, decay, 0.0f, rate);
            v.gain = accent * (1.0f + (chance(4) - 0.5f) * (0.1f + 0.8f * s.life));
            v.filterEnvOctaves = s.character == 2 ? 1.5f + 2.5f * float(cyclePos) : (1.8f + 1.4f * accent) * (s.character == 1 ? 0.6f : 1.0f);
            v.pan = panGains(0.5f * (chance(5) - 0.5f));
        }
    }

    // --- Risers: thruster pass-bys, launch builds, falling sweeps.
    if (s.element[2] > 0.001f && !riser.active && pos == 0) {
        const bool cycleStart = s.character == 2 && std::abs(cyclePos) < 1.0e-6;
        float p = 0;
        int shape = 0;
        double length = 8;
        if (s.character == 0 && (bar & 1) == 0) p = 0.15f + 0.45f * act;
        else if (s.character == 1 && bar % 4 == 0) { p = 0.1f + 0.3f * act; shape = 2; length = 16; }
        else if (cycleStart) { p = 1.0f; shape = 1; length = 4.0 * buildBars; }
        if (p > 0 && chance(10, 1024) < p) {
            logEvent(c.clock);
            riser.active = true;
            riser.shape = shape;
            riser.start = stepBeat;
            riser.length = length;
            riser.size = 0.7f + 0.3f * chance(11);
            const bool leftToRight = chance(12) < 0.5f;
            riser.panFrom = leftToRight ? -0.8f : 0.8f;
            riser.panTo = -riser.panFrom;
        }
    }

    // --- Announcements: a PA chime (and on a busy hub an abstract murmur); Launch counts down.
    if (s.element[3] > 0.001f) {
        if (pos == 0 && bar % 4 == 0 && !chimePhrase.active() && s.character != 2) {
            // Life: a livelier port announces more.
            const float p = (s.character == 0 ? 0.2f + 0.5f * act : 0.1f + 0.3f * act) * lerp(0.5f, 1.6f, s.life);
            if (chance(20, 1024) < p) {
                chimePhrase.clear();
                const int top = music::inRange(music::chordTone(mood, 0, 2), 12);
                const int mid = music::inRange(music::chordTone(mood, 0, 1), 12), low = music::inRange(music::chordTone(mood, 0, 0), 12);
                const bool down = chance(21) < 0.6f;
                const int notes[3] { down ? top : low, mid, down ? low : top };
                const int count = s.character == 1 ? 2 : 3;
                for (int k = 0; k < count; ++k) chimePhrase.add(float(k), notes[k], 1.0f, 0.8f);
                if (s.character == 1 && chance(22) < 0.5f) chimePhrase.add(1.25f, notes[1], 1.0f, 0.4f);   // a broken stutter
                chimePhrase.begin(stepBeat);
                if (s.character == 0 || s.life > 0.6f) murmurUntil = stepBeat + 4.0 + 8.0 * double(chance(23)) * double(0.5f + s.life);
            }
        }
        if (s.character == 2 && pos % 4 == 0 && cyclePos >= 0.75) {
            // Countdown beeps over the last two bars of the build, the last one higher.
            const bool last = cyclePos >= 1.0 - 1.0 / (4.0 * buildBars) - 1.0e-9;
            beep.ratio = 2.0f;
            beep.partial = 0.2f;
            logEvent(c.clock);
            beep.start(music::noteHz(root, float(24 + (last ? music::degree(mood, 4) : 0))), 1.0f, 0.25f, 0.3f, 0.0f, 1000.0f, 1.0f, 0.05f, 0.0f, rate, noiseAt);
            noiseAt += 1ull << 24;
        }
    }
}

void SpaceportPlace::render(const PlaceContext& c, float* left, float* right, int n) {
    const auto& w = *c.world;
    const auto& s = c.s;
    const int mood = w.mood, root = w.root, chord = c.chord;
    const double span = c.beatsPerSample * double(n);
    const double t = double(c.clock) / rate;
    const int q = std::clamp(w.quality, 0, 2);
    arpVoices = q == 0 ? 3 : (q == 1 ? 4 : arps);
    const float tension = s.tension, tone = s.tone;
    const double cyclePos = std::fmod(c.beat / (4.0 * buildBars), 1.0) + (c.beat < 0 ? 1.0 : 0.0);
    const float build = s.character == 2 ? float(cyclePos) : 0.0f;

    forEachStep(c.beat, span, 4, s.swing, [&](int64_t step) { onStep(c, step); });

    // Chimes: soft, nearly harmonic partials; an abandoned dock's are flat and broken.
    chimePhrase.play(c.beat, span, [&](const Phrase::Note& note) {
        static constexpr float ratios[4] { 1.0f, 2.0f, 3.0f, 4.12f };
        static constexpr float decays[4] { 1.0f, 0.55f, 0.3f, 0.2f };
        static constexpr float gains[4] { 1.0f, 0.3f, 0.12f, 0.06f };
        auto& b = chime[pickVoice(chime, chimes, [](const Bell& v) { return v.level(); })];
        logEvent(c.clock);
        const float flat = s.character == 1 ? -0.35f : 0.0f;
        b.start(music::noteHz(root, float(note.semitones) + flat), 3.0f, 0.25f + 0.3f * tone, note.velocity, 0.2f, rate, ratios, decays, gains, 4);
    });

    // Arp timbre: resonant low-pass pluck.
    for (auto& v : arp) {
        v.detuneCents = s.character == 1 ? 14.0f : 5.0f;
        v.subLevel = 0.0f;
        v.cutoff = lerpExp(180.0f, 900.0f, tone) * (s.character == 1 ? 0.6f : 1.0f) * lerp(1.3f, 0.6f, tension);
        v.envOctaves = 0.0f;
        v.filterDecay = std::exp(-float(chunk) / (0.07f * rate));
        v.q = lerp(1.6f, 3.2f, s.machinery * 0.5f + tension * 0.5f);
        v.width = 0.3f;
        v.vibratoDepth = 0;
        if (v.active()) v.update(rate);
    }

    // Pads: four wide voices on the shared chord, gliding when it changes.
    {
        // As Tension rises the third gives way to a semitone above the chord root.
        const int third = tension > 0.6f ? music::inRange(music::chordTone(mood, chord, 0) + 1, 7) : music::inRange(music::chordTone(mood, chord, 1), 7);
        const int notes[pads] { music::inRange(music::chordTone(mood, chord, 0), -5), music::inRange(music::chordTone(mood, chord, 2), 0), third,
                                music::inRange(music::chordTone(mood, chord, 3), 10) };
        const float uneasy = tension > 0.6f ? clamp01((tension - 0.6f) * 3.0f) : clamp01((0.6f - tension) * 3.0f);
        for (int i = 0; i < pads; ++i) {
            auto& v = pad[std::size_t(i)];
            const float hz = music::noteHz(root, float(notes[i]));
            const float breath = drift.smooth(t + 7.0 * i, 0.07);
            if (!v.active()) v.start(hz, 3.0f, 1.0f, 1.0f, rate);
            v.targetHz = hz;
            v.glide = glideFor(2.0f, rate);
            v.detuneCents = s.character == 1 ? 22.0f : 11.0f;
            v.subLevel = 0.0f;
            v.cutoff = lerpExp(300.0f, 2200.0f, tone) * std::exp2(0.5f * breath + 1.5f * build) * (s.character == 1 ? 0.6f : 1.0f);
            v.envOctaves = 0.0f;
            v.q = 0.7f;
            v.width = 1.0f;
            v.vibratoHz = s.character == 1 ? 0.23f : 0.15f;
            v.vibratoDepth = s.character == 1 ? 0.12f : 0.03f;
            v.vibratoDelay = 0;
            v.gain = s.element[1] * (0.8f + 0.2f * breath) * (i == 3 ? 0.6f : 1.0f) * (i == 2 ? uneasy : 1.0f);
            v.update(rate);
        }
    }

    // Engines: a sub drone on the root and fifth with a throb at half notes and a
    // low rumble. Launch swells them through the build.
    const float throbDepth = 0.25f + 0.45f * tension + 0.2f * s.machinery;
    const float throb = 1.0f - throbDepth * (0.5f + 0.5f * std::cos(float(c.beat * 0.5 * double(twoPi))));
    const float engineLevel = s.element[4] * throb * (s.character == 2 ? 0.6f + 1.2f * build * build : (s.character == 1 ? 0.6f : 1.0f))
                              * lerp(0.7f, 1.6f, tension);
    for (int i = 0; i < engines; ++i) {
        auto& v = engine[std::size_t(i)];
        const float wobble = s.character == 1 ? 0.15f * drift.smooth(t + 40.0 * i, 0.2) : 0.0f;
        const float hz = music::noteHz(root, (i == 0 ? -24.0f : -17.0f) + wobble);
        if (!v.active()) v.start(hz, 2.0f, 1.0f, 1.0f, rate);
        v.targetHz = hz;
        v.glide = glideFor(0.5f, rate);
        v.detuneCents = 9.0f;
        v.subLevel = 0.3f;
        v.cutoff = lerpExp(90.0f, 260.0f, tone) * (1.0f + build);
        v.envOctaves = 0.0f;
        v.q = 1.1f;
        v.width = 0.8f;
        v.gain = engineLevel * (i == 0 ? 1.0f : 0.5f);
        v.update(rate);
    }

    // Riser: a resonant noise band sweeping through the spectrum.
    float riserTarget = 0;
    if (riser.active) {
        const float x = float((c.beat - riser.start) / riser.length);
        float sweep = x, env = 0;
        switch (riser.shape) {
            case 0: env = std::pow(std::sin(pi * clamp01(x)), 2.0f); sweep = 0.25f + 0.5f * x; break;   // pass-by
            case 1: env = x * x * x; sweep = x; break;                                                    // build
            default: env = std::pow(std::sin(pi * clamp01(x)), 2.0f) * 0.7f; sweep = 1.0f - x; break;    // falling
        }
        riser.hz = music::noteHz(root, -12.0f) * std::exp2(5.0f * clamp01(sweep));
        riser.pan = riser.shape == 0 ? lerp(riser.panFrom, riser.panTo, clamp01(x)) : 0.3f * std::sin(float(x * 6.0f));
        riserTarget = env * riser.size * s.element[2];
        for (auto& f : riser.band) f.set(riser.hz, lerp(2.0f, 5.0f, tension), rate);
        if (x >= 1.0f) {
            riser.active = false;
            if (riser.shape == 1) {
                // Launch: the build lands on a tuned low impact.
                impact.ratio = 1.5f;
                impact.partial = 0.4f;
                impact.start(music::noteHz(root, -24.0f), 1.5f, 6.0f, 0.8f * s.element[2], 0.5f * s.element[2], 300.0f, 0.6f, 1.5f, 0.0f, rate, noiseAt);
                noiseAt += 1ull << 24;
            }
        }
    }
    const PanGains riserPan = panGains(riser.pan);

    // Murmur: two formants that jump per "syllable" and glide, never words.
    const bool murmuring = s.element[3] > 0.001f && c.beat < murmurUntil;
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
    const float murmurTarget = murmuring ? 0.08f * s.element[3] * syllableEnv * (0.6f + 0.4f * drift.unit(uint64_t(std::floor(c.beat)) + 99)) : 0.0f;

    for (auto& b : chime)
        if (b.active) b.update();
    if (impact.active()) impact.update(rate);
    if (beep.active()) beep.update(rate);

    hangar.set(lerp(0.25f, 0.95f, s.size), lerp(1.0f, 4.5f, s.size) * (1.0f + 0.4f * s.magic), lerpExp(3000.0f, 9000.0f, tone));
    const float beatSeconds = 60.0f / std::max(c.bpm, 20.0f);
    delay.set(std::min(0.75f * beatSeconds, 1.4f), 0.25f + 0.4f * s.fxDepth, lerpExp(2000.0f, 6000.0f, tone));
    const float delaySend = 0.6f * s.fxDepth;
    const float arpLevel = 0.2f * s.element[0], padLevel = 0.05f, engineOut = 0.085f, chimeLevel = 0.25f * s.element[3];

    for (int i = 0; i < n; ++i) {
        const uint64_t index = c.clock + uint64_t(i);
        float al = 0, ar = 0;
        for (int k = 0; k < arpVoices; ++k) {
            auto& v = arp[std::size_t(k)];
            if (v.active()) v.tick(al, ar);
        }
        al *= arpLevel;
        ar *= arpLevel;
        float pl = 0, pr = 0;
        for (auto& v : pad) v.tick(pl, pr);
        float el = 0, er = 0;
        for (auto& v : engine) v.tick(el, er);
        const float rl = rumble[0].tick(noise.white(index)), rr = rumble[1].tick(noise.white(index + (1ull << 44)));
        el = (el + 0.25f * rl * engineLevel) * engineOut;
        er = (er + 0.25f * rr * engineLevel) * engineOut;
        float cl = 0, cr = 0;
        for (auto& b : chime) {
            if (!b.active) continue;
            const float y = b.tick() * chimeLevel;
            cl += y * b.pan.l;
            cr += y * b.pan.r;
        }
        if (beep.active()) {
            const float y = beep.tick(noise) * 0.1f * s.element[3];
            cl += y;
            cr += y;
        }
        const float rg = riser.gain.next(riserTarget);
        float xl = 0, xr = 0;
        if (rg > 1.0e-5f) {
            xl = riser.band[0].bandpass(noise.white(index + (2ull << 44))) * rg * 3.0f * riserPan.l;
            xr = riser.band[1].bandpass(noise.white(index + (3ull << 44))) * rg * 3.0f * riserPan.r;
        }
        if (impact.active()) {
            const float y = impact.tick(noise) * 0.16f;
            xl += y;
            xr += y;
        }
        const float mg = murmurGain.next(murmurTarget);
        if (mg > 1.0e-5f) {
            const float buzz = murmurOsc.tick();
            const float voice = murmurBand.bandpass(formant[0].bandpass(buzz) + 0.7f * formant[1].bandpass(buzz)) * mg;
            cl += voice * 0.8f;
            cr += voice;
        }
        float dl = 0, dr = 0, wl = 0, wr = 0;
        delay.process((al + cl) * delaySend, (ar + cr) * delaySend, dl, dr);
        const float l = al + pl * padLevel + el + cl + xl, r = ar + pr * padLevel + er + cr + xr;
        hangar.process(l * 0.4f + 0.5f * dl, r * 0.4f + 0.5f * dr, wl, wr);
        left[i] = l + 0.7f * dl + 0.6f * wl;
        right[i] = r + 0.7f * dr + 0.6f * wr;
    }
}

} // namespace wb
