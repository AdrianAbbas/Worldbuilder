#include "TonalBed.h"

namespace wb {

using namespace dsp;

float Harmony::rootHz(int root) { return 130.8128f * std::pow(2.0f, float(std::clamp(root, 0, 11)) / 12.0f); }

std::array<float, 5> Harmony::bedIntervals(int mood) {
    // sub, root, fifth, third (an octave up), colour tone that gives the mood its flavour
    static constexpr std::array<std::array<float, 5>, 9> table { {
        { -12, 0, 7, 15, 14 },   // Minor: add9
        { -12, 0, 7, 16, 14 },   // Major: add9
        { -12, 0, 7, 15, 21 },   // Dorian: major sixth
        { -12, 0, 7, 15, 13 },   // Phrygian: flat ninth
        { -12, 0, 7, 16, 18 },   // Lydian: sharp eleventh
        { -12, 0, 7, 16, 22 },   // Mixolydian: flat seventh
        { -12, 0, 8, 16, 18 },   // Whole-tone: augmented
        { -12, 0, 7, 15, 17 },   // Pentatonic: fourth
        { -12, 0, 6, 13, 11 },   // Chromatic: tritone cluster
    } };
    return table[std::size_t(std::clamp(mood, 0, 8))];
}

void TonalBed::prepare(double sampleRate) {
    rate = float(sampleRate);
    const float controlRate = rate / float(chunk);
    for (auto& v : voice) {
        v.semitones.setTime(1.2f, controlRate);
        v.gain.setTime(0.08f, rate);
    }
    brightness.setTime(0.3f, controlRate);
    reset();
}

void TonalBed::reset() {
    for (std::size_t i = 0; i < voice.size(); ++i) {
        auto& v = voice[i];
        v.initialised = false;
        v.gain.snap(0);
        for (std::size_t o = 0; o < 2; ++o) v.osc[o].phasor.setPhase(float(i * 2 + o) * 1.1f);
    }
    for (auto& f : tone) f.reset();
    lastRoot = lastMood = -1;
    fresh = true;
}

void TonalBed::setSeed(uint32_t seed) {
    for (std::size_t i = 0; i < voice.size(); ++i) voice[i].breath.setKey(seed, 300 + uint32_t(i));
    haze.setKey(seed, 320);
}

float TonalBed::spaceSend(const WorldFrame& world) const { return 0.75f + 0.2f * world[Magic]; }

void TonalBed::render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) {
    const double t = double(clock) / rate;
    const float temp = world[Temperature], magic = world[Magic], era = world[Era], danger = world[Danger];
    const float activity = world[Activity];

    const auto intervals = Harmony::bedIntervals(world.mood);
    const float rootSemis = float(std::clamp(world.root, 0, 11));

    // Voice levels: sub, root, fifth, third, colour, danger.
    const float scale = world[Scale];
    // Danger throbs: a pulse on the dissonant voice that quickens as the world turns
    // hostile (two fixed-rate pulses blended, so turning the knob never jumps the phase).
    const auto pulse = [t](double hz) { return 0.5f + 0.5f * std::sin(float(std::fmod(t * hz, 1.0)) * twoPi); };
    const float throb = lerp(pulse(0.35), pulse(1.7), danger);
    // Voice levels: sub, root, fifth, third, colour, danger, halo.
    const float base[voices] {
        0.55f * lerp(0.35f, 1.1f, temp) * (1.0f + 1.5f * danger) * lerp(0.6f, 1.5f, scale),   // low-end weight
        0.80f,
        0.55f * (1.0f - 0.4f * danger),
        0.40f * (1.0f - 0.5f * danger),
        lerp(0.12f, 0.9f, magic) * lerp(1.5f, 0.7f, temp),
        0.9f * danger * (0.6f + 0.4f * throb),
        1.0f * std::pow(magic, 1.2f),
    };
    const float spreadScale = lerp(0.6f, 1.3f, scale);
    static constexpr float spread[voices] { 0.1f, 0.3f, 0.5f, 0.6f, 0.75f, 0.4f, 0.65f };

    // Brightness: cold, futuristic and peaceful worlds are brighter. Ancient worlds are
    // near-pure tones, futuristic ones bright and hollow (odd harmonics).
    const float brightTarget = clamp01(0.15f + 0.25f * (1.0f - temp) + 0.6f * era - 0.35f * danger + 0.15f * magic);
    // A reset world starts from its settings, so a restart always sounds the same.
    if (fresh) brightness.snap(brightTarget);
    fresh = false;
    const float bright = brightness.next(brightTarget);
    const float tilt = lerp(3.4f, 0.9f, bright);
    const float evenScale = 1.0f - 0.85f * smoothstep(0.5f, 1.0f, era);
    static constexpr int harmonicsForQuality[3] { 4, 6, maxHarmonics };
    const int harmonics = harmonicsForQuality[std::clamp(world.quality, 0, 2)];
    for (int k = 0; k < maxHarmonics; ++k) {
        const float h = float(k + 1);
        harmonicGain[std::size_t(k)] = k < harmonics ? std::pow(h, -tilt) * ((k % 2 == 1) ? evenScale : 1.0f) : 0.0f;
    }
    for (auto& f : tone) f.set(lerpExp(500.0f, 8000.0f, bright), 0.6f, rate);

    // Heat haze: a slow shared pitch shimmer in scorching worlds.
    const float hazeSemis = smoothstep(0.6f, 1.0f, temp) * 0.08f * haze.smooth(t, 0.35);
    const float detune = (3.0f + 6.0f * magic + 3.0f * era) / 100.0f;   // semitones between the pair

    const float breathFast = activity;
    for (int vi = 0; vi < voices; ++vi) {
        auto& v = voice[std::size_t(vi)];
        const float interval = vi < 5 ? intervals[std::size_t(vi)] : (vi == 5 ? 13.0f : 24.0f);
        float target = rootSemis + interval;
        if (!v.initialised) {
            v.semitones.snap(target);
            v.initialised = true;
        } else {
            // Glide to the nearest octave of the new note, staying in a sensible register.
            const float current = v.semitones.value;
            while (target - current > 6.0f && target - 12.0f >= interval - 6.0f) target -= 12.0f;
            while (current - target > 6.0f && target + 12.0f <= interval + 18.0f) target += 12.0f;
        }
        const float semis = v.semitones.next(target) + hazeSemis;
        const float hz = Harmony::rootHz(0) * std::pow(2.0f, semis / 12.0f);
        for (int o = 0; o < 2; ++o) {
            auto& osc = v.osc[std::size_t(o)];
            osc.phasor.setFrequency(hz * std::pow(2.0f, (o == 0 ? -0.5f : 0.5f) * detune / 12.0f), rate);
            osc.phasor.normalise();
            const float pan = std::clamp((o == 0 ? -1.0f : 1.0f) * spread[vi] * spreadScale, -1.0f, 1.0f);
            osc.gainL = std::sqrt(0.5f * (1.0f - pan));
            osc.gainR = std::sqrt(0.5f * (1.0f + pan));
        }
        const float breath = (1.0f - breathFast) * v.breath.smooth(t, 0.03 + 0.007 * vi) + breathFast * v.breath.smooth(t + 101.0, 0.15);
        v.hz = hz;
        v.target = base[vi] * std::max(0.0f, 1.0f + lerp(0.2f, 0.6f, activity) * breath);
    }

    constexpr float level = 0.045f;
    for (int i = 0; i < n; ++i) {
        float l = 0, r = 0;
        for (int vi = 0; vi < voices; ++vi) {
            auto& v = voice[std::size_t(vi)];
            const float g = v.gain.next(v.target);
            const int limit = std::min(harmonics, int(0.42f * rate / v.hz));
            for (auto& osc : v.osc) {
                osc.phasor.step();
                if (g < 1.0e-6f) continue;
                // Chebyshev recurrence: sin((k+1)x) = 2 cos(x) sin(kx) - sin((k-1)x)
                const float c2 = 2.0f * osc.phasor.re;
                float prev = 0, cur = osc.phasor.im, sum = harmonicGain[0] * cur;
                for (int k = 1; k < limit; ++k) {
                    const float next = c2 * cur - prev;
                    prev = cur;
                    cur = next;
                    sum += harmonicGain[std::size_t(k)] * cur;
                }
                sum *= g;
                l += sum * osc.gainL;
                r += sum * osc.gainR;
            }
        }
        left[i] = tone[0].tick(l * level).lp;
        right[i] = tone[1].tick(r * level).lp;
    }
}

} // namespace wb
