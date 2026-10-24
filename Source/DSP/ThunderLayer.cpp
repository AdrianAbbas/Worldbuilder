#include "ThunderLayer.h"

namespace wb {

using namespace dsp;

float ThunderLayer::strikeRate(const WorldFrame& world) {
    const float storm = smoothstep(0.4f, 0.95f, world[Weather] + 0.25f * world[Danger]);
    return storm * lerp(0.03f, 0.14f, storm) * lerp(0.6f, 1.6f, world[Activity]);
}

void ThunderLayer::prepare(double sampleRate) {
    rate = float(sampleRate);
    for (auto& s : strike) {
        s.gain.setTime(0.005f, rate);
        s.boomGain.setTime(0.003f, rate);
        s.boom.setCutoff(60.0f, rate);
        for (auto& f : s.crackHigh) f.setCutoff(700.0f, rate);
    }
    flashDecay = std::exp(-1.0f / (0.25f * rate / float(chunk)));
    crackDecay = std::exp(-1.0f / (0.008f * rate));
    reset();
}

void ThunderLayer::reset() {
    for (auto& s : strike) {
        s.active = false;
        for (auto& f : s.body) f.reset();
        for (auto& f : s.tone) f.reset();
        for (auto& f : s.crackHigh) f.reset();
        s.boom.reset();
        s.gain.snap(0);
        s.boomGain.snap(0);
    }
    flashLevel = 0;
    strikes = 0;
    strikeLog.fill(0);
}

void ThunderLayer::setSeed(uint32_t seed) {
    seedValue = seed;
    noiseL.setKey(seed, 701);
    noiseR.setKey(seed, 702);
    events.setKey(seed, 703);
}

float ThunderLayer::spaceSend(const WorldFrame& world) const { return 0.6f + 0.3f * world[Scale]; }

void ThunderLayer::render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) {
    const float storm = smoothstep(0.4f, 0.95f, world[Weather] + 0.25f * world[Danger]);
    const float scale = world[Scale];
    const int voices = world.quality == 0 ? 2 : maxStrikes;
    const uint64_t e = clock * 32;

    flashLevel *= flashDecay;
    // A strike needs a free voice: cutting off a sounding rumble would click, so while
    // every voice is busy the storm simply holds its next strike back.
    int free = -1;
    for (int i = voices - 1; i >= 0; --i)
        if (!strike[std::size_t(i)].active) free = i;
    if (free >= 0 && events.unit(e) < trialChance(strikeRate(world), n, rate)) {
        auto& s = strike[std::size_t(free)];
        s.gain.snap(0);
        s.boomGain.snap(0);
        // Storms bring strikes closer; colossal worlds hear them from further off.
        const float d = lerp(std::pow(events.unit(e + 1), lerp(0.7f, 1.6f, storm)), 1.0f, 0.45f * scale);
        s.active = true;
        s.distance = d;
        s.size = 0.6f + 0.4f * events.unit(e + 2);
        s.wait = d * 2.2f * rate;   // light first, sound later
        s.age = 0;
        s.length = lerp(4.0f, 9.0f, events.unit(e + 3)) * lerp(0.8f, 1.2f, d);
        s.attack = lerp(0.02f, 0.5f, d);
        s.pan = panGains(0.6f * events.white(e + 4));
        s.roll.setKey(seedValue, uint32_t(800 + (e & 0xffffff)));
        s.noiseOffset = uint64_t(events.unit(e + 5) * 16777216.0f) << 20;
        s.crackLevel = std::max(0.0f, 1.0f - d / 0.45f);
        s.boomLevel = std::max(0.0f, 1.0f - d / 0.5f);
        s.bursts = s.crackLevel > 0 ? 3 + int(events.unit(e + 6) * float(maxBursts - 3)) : 0;
        const float spread = lerp(0.12f, 0.35f, events.unit(e + 7));
        for (int b = 0; b < s.bursts; ++b) {
            // Bursts tear across the sky, mostly early, each weaker than the last.
            s.burstAt[std::size_t(b)] = b == 0 ? 0.0f : spread * std::pow(events.unit(e + 8 + uint64_t(b)), 1.5f);
            s.burstAmp[std::size_t(b)] = (b == 0 ? 1.0f : 0.3f + 0.6f * events.unit(e + 16 + uint64_t(b)));
        }
        std::sort(s.burstAt.begin() + 1, s.burstAt.begin() + std::max(1, s.bursts));
        s.nextBurst = 0;
        s.crack = 0;
        flashLevel = std::max(flashLevel, s.size * (1.0f - 0.6f * d));
        strikeLog[std::size_t(strikes % logSize)] = clock;
        ++strikes;
    }

    // Per chunk: each strike's envelope and tone.
    struct Frame { float gain, boom; };
    std::array<Frame, maxStrikes> frame {};
    for (int i = 0; i < voices; ++i) {
        auto& s = strike[std::size_t(i)];
        if (!s.active || s.wait > 0) continue;
        const float a = s.age, x = a / s.length;
        const float env = a < s.attack ? (a / s.attack) * (a / s.attack) : std::exp(-(a - s.attack) / (s.length / 3.0f));
        // Rolling: the sound of each stretch of the bolt arriving in turn, as uneven swells.
        const float fast = clamp01(0.5f + 0.8f * s.roll.smooth(a, lerp(3.5, 1.5, s.distance)));
        const float slow = clamp01(0.55f + 0.7f * s.roll.smooth(a + 50.0f, 0.7));
        const float rolling = 0.1f + 0.9f * fast * fast * (0.35f + 0.65f * slow);
        const float tail = 1.0f - smoothstep(0.75f, 1.0f, x);
        const float loud = s.size * lerp(1.0f, 0.25f, s.distance);
        frame[std::size_t(i)].gain = 0.55f * loud * env * rolling * tail;
        frame[std::size_t(i)].boom = 0.6f * loud * s.boomLevel * std::exp(-a / 0.45f);
        const float bright = lerp(1.0f, 0.5f, x);
        for (auto& f : s.body) f.setCutoff(lerpExp(500.0f, 110.0f, s.distance) * bright, rate);
        for (auto& f : s.tone) f.set(lerpExp(1400.0f, 260.0f, s.distance) * bright, 0.6f, rate);
    }

    for (int k = 0; k < n; ++k) {
        const uint64_t index = clock + uint64_t(k);
        float l = 0, r = 0;
        for (int i = 0; i < voices; ++i) {
            auto& s = strike[std::size_t(i)];
            if (!s.active) continue;
            if (s.wait > 0) {
                s.wait -= 1.0f;
                continue;
            }
            const float wl = noiseL.white(index + s.noiseOffset), wr = noiseR.white(index + s.noiseOffset);
            const float g = s.gain.next(frame[std::size_t(i)].gain);
            float sl = s.tone[0].tick(s.body[0].tick(wl)).lp * g;
            float sr = s.tone[1].tick(s.body[1].tick(wr)).lp * g;

            const float bg = s.boomGain.next(frame[std::size_t(i)].boom);
            if (bg > 1.0e-5f) {
                const float b = s.boom.tick(0.5f * (wl + wr)) * bg;
                sl += b;
                sr += b;
            }
            if (s.bursts > 0) {
                while (s.nextBurst < s.bursts && s.age >= s.burstAt[std::size_t(s.nextBurst)]) s.crack += s.burstAmp[std::size_t(s.nextBurst++)];
                if (s.crack > 1.0e-4f) {
                    const float c = s.crack * s.crackLevel * s.size * 0.5f;
                    sl += s.crackHigh[0].hp(wl * c);
                    sr += s.crackHigh[1].hp(wr * c);
                    s.crack *= crackDecay;
                }
            }
            l += sl * s.pan.l * 1.4142f;
            r += sr * s.pan.r * 1.4142f;
            s.age += 1.0f / rate;
            if (s.age >= s.length) s.active = false;
        }
        left[k] = l;
        right[k] = r;
    }
}

} // namespace wb
