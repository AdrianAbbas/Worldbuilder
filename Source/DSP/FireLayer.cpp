#include "FireLayer.h"

namespace wb {

using namespace dsp;

FireLayer::Amounts FireLayer::amounts(const WorldFrame& world) {
    const float temp = world[Temperature], wet = world[Wetness], weather = world[Weather];
    // A drenched world puts fires out; wind fans them.
    const float douse = 1.0f - 0.65f * smoothstep(0.35f, 1.0f, wet);
    const float fan = 1.0f + 0.5f * weather;
    Amounts a;
    a.crackle = smoothstep(0.3f, 0.95f, temp) * douse;
    a.roar = smoothstep(0.55f, 1.0f, temp) * douse * fan;
    a.embers = smoothstep(0.6f, 1.0f, temp) * douse;
    a.lava = smoothstep(0.74f, 1.0f, temp) * (1.0f - 0.45f * smoothstep(0.4f, 1.0f, wet));
    return a;
}

void FireLayer::prepare(double sampleRate) {
    rate = float(sampleRate);
    for (auto* s : { &roarGain, &emberGain, &churnGain }) s->setTime(0.06f, rate);
    for (auto& f : churn) f.setCutoff(80.0f, rate);
    for (auto& f : roarSource) f.setCutoff(450.0f, rate);
    for (auto& f : emberHigh) f.setCutoff(3200.0f, rate);
    reset();
}

void FireLayer::reset() {
    for (auto& c : crackles) {
        c.amp = 0;
        c.band.reset();
    }
    for (auto& b : blips) b.stop();
    for (auto& f : roarSource) f.reset();
    for (auto& f : churn) f.reset();
    for (auto& f : roarBody) f.reset();
    for (auto& f : emberHigh) f.reset();
    for (auto* s : { &roarGain, &emberGain, &churnGain }) s->snap(0);
}

void FireLayer::setSeed(uint32_t seed) {
    noiseL.setKey(seed, 501);
    noiseR.setKey(seed, 502);
    events.setKey(seed, 503);
    flicker.setKey(seed, 504);
    ember.setKey(seed, 505);
}

float FireLayer::spaceSend(const WorldFrame& world) const { return 0.3f + 0.25f * world[Scale]; }

void FireLayer::render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) {
    const double t = double(clock) / rate;
    const auto a = amounts(world);
    const float activity = world[Activity], era = world[Era], scale = world[Scale];
    const int quality = std::clamp(world.quality, 0, 2);
    const int crackleVoices = quality == 0 ? 6 : (quality == 1 ? 10 : maxCrackles);
    const int blipVoices = quality == 0 ? 3 : (quality == 1 ? 4 : maxBlips);
    const float spread = lerp(0.5f, 0.9f, scale);

    // Flicker: crackles come in clusters as the flames flare and settle.
    const float flare = clamp01(0.5f + 0.6f * flicker.smooth(t, 2.6));
    const uint64_t e = clock * 32;
    const float crackleRate = a.crackle * lerp(5.0f, 55.0f, a.crackle) * lerpExp(0.3f, 2.4f, activity) * (0.2f + 1.6f * flare * flare);
    if (events.unit(e) < trialChance(crackleRate, n, rate)) {
        auto& c = crackles[pickVoice(crackles, crackleVoices, [](const Crackle& v) { return v.amp; })];
        const float kind = events.unit(e + 1), size = events.unit(e + 2);
        float hz, q, seconds;
        if (kind < 0.13f) {   // pop: lower, rounder, louder
            hz = lerpExp(260.0f, 950.0f, events.unit(e + 3));
            q = 2.5f;
            seconds = lerp(0.008f, 0.025f, size);
            c.amp = 0.6f + 0.4f * size;
            c.gain = 0.55f;
        } else {   // crackle: a bright tick; futuristic fire is hotter and tighter
            hz = lerpExp(1300.0f, 5600.0f, events.unit(e + 3)) * lerp(1.0f, 1.35f, era);
            q = lerp(1.2f, 3.2f, era);
            seconds = lerp(0.0006f, 0.003f, size);
            c.amp = 0.15f + 0.85f * size * size;
            c.gain = 0.8f;
        }
        c.band.reset();
        c.band.set(hz, q, rate);
        c.decay = std::exp(-1.0f / (seconds * rate));
        c.pan = panGains(spread * events.white(e + 4));
        c.noiseOffset = uint64_t(events.unit(e + 5) * 16777216.0f) << 20;
    }
    const float blipRate = a.lava * lerp(0.6f, 4.5f, a.lava) * lerp(0.5f, 1.5f, activity);
    if (events.unit(e + 6) < trialChance(blipRate, n, rate)) {
        auto& b = blips[pickVoice(blips, blipVoices, [](const Ping& p) { return p.amp; })];
        const float size = events.unit(e + 7);
        // Low resonant bloops that rise as the bubble swells and bursts, starting on a
        // note of the key.
        const float free = 12.0f * std::log2(lerpExp(210.0f, 65.0f, size) / music::rootHz(world.root));
        const float tuned = music::noteHz(world.root, float(music::quantise(world.mood, free)));
        b.start(tuned, lerp(0.09f, 0.24f, size), events.unit(e + 8) < 0.5f ? 1.5f : 2.0f,
                (events.unit(e + 10) < 0.5f ? -1.0f : 1.0f) * (0.55f + 0.45f * size),
                0.6f * events.white(e + 9), rate, 0.012f);
    }
    for (int i = 0; i < blipVoices; ++i)
        if (blips[std::size_t(i)].active()) blips[std::size_t(i)].update(rate);

    const float roarFlicker = std::max(0.0f, 0.65f + 0.35f * flicker.smooth(t + 40.0, 4.3) + 0.2f * flicker.smooth(t + 90.0, 9.7));
    for (auto& f : roarBody) f.set(lerpExp(110.0f, 230.0f, flare) * lerp(1.0f, 1.25f, era), 0.8f, rate);
    const float roarTarget = 0.07f * a.roar * roarFlicker;
    const float emberTarget = 0.35f * a.embers;
    const float emberChance = trialChance(a.embers * 900.0f * (0.4f + flare), 1, rate);
    const float churnTarget = 0.06f * a.lava * (0.7f + 0.3f * flicker.smooth(t + 7.0, 0.3));

    for (int i = 0; i < n; ++i) {
        const uint64_t index = clock + uint64_t(i);
        const float wl = noiseL.white(index), wr = noiseR.white(index);
        float l = 0, r = 0;

        for (int v = 0; v < crackleVoices; ++v) {
            auto& c = crackles[std::size_t(v)];
            if (!c.active()) continue;
            const float s = c.band.bandpass(noiseL.white(index + c.noiseOffset) * c.amp) * c.gain;
            c.amp *= c.decay;
            l += s * c.pan.l;
            r += s * c.pan.r;
        }

        const float rg = roarGain.next(roarTarget);
        if (rg > 1.0e-5f) {
            l += roarBody[0].bandpass(roarSource[0].tick(wl)) * rg;
            r += roarBody[1].bandpass(roarSource[1].tick(wr)) * rg;
        }
        const float eg = emberGain.next(emberTarget);
        if (eg > 1.0e-5f) {
            float spark = 0;
            if (ember.unit(index) < emberChance) spark = ember.white(index + (1ull << 44));
            const bool toLeft = spark > 0;
            l += emberHigh[0].hp(toLeft ? spark : 0.0f) * eg;
            r += emberHigh[1].hp(toLeft ? 0.0f : -spark) * eg;
        }
        const float cg = churnGain.next(churnTarget);
        if (cg > 1.0e-5f) {
            l += churn[0].tick(wl) * cg;
            r += churn[1].tick(wr) * cg;
        }
        for (int v = 0; v < blipVoices; ++v) {
            auto& b = blips[std::size_t(v)];
            if (!b.active()) continue;
            const float raw = b.tick();
            // A little saturation gives the bloop harmonics small speakers can play.
            const float s = 0.3f * (raw - 0.3f * raw * raw * raw);
            l += s * b.pan.l;
            r += s * b.pan.r;
        }
        left[i] = l;
        right[i] = r;
    }
}

} // namespace wb
