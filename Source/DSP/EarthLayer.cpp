#include "EarthLayer.h"

namespace wb {

using namespace dsp;

EarthLayer::Amounts EarthLayer::amounts(const WorldFrame& world) {
    const float temp = world[Temperature], danger = world[Danger], scale = world[Scale], activity = world[Activity];
    const float heat = smoothstep(0.6f, 1.0f, temp);
    Amounts a;
    a.ice = 1.0f - smoothstep(0.08f, 0.42f, temp);
    a.rumble = std::min(1.2f, 0.2f + 0.45f * danger + 0.35f * heat + 0.3f * scale);
    a.groan = smoothstep(0.3f, 1.0f, 0.75f * danger + 0.4f * heat + 0.25f * scale);
    // Rockfalls per second.
    a.rockfalls = (0.02f + 0.14f * danger + 0.05f * activity) * (0.5f + scale) * (0.6f + 0.6f * heat);
    return a;
}

void EarthLayer::prepare(double sampleRate) {
    rate = float(sampleRate);
    for (auto* s : { &rumbleGain, &groanGain, &fallGain }) s->setTime(0.1f, rate);
    groanHz.setTime(2.0f, rate / float(chunk));
    for (auto& f : fallNoise) f.setCutoff(180.0f, rate);
    for (auto& f : sheetClickHigh) f.setCutoff(2000.0f, rate);
    reset();
}

void EarthLayer::reset() {
    for (auto& c : creaks) {
        c.active = false;
        for (auto& m : c.mode) m.clear();
    }
    for (auto& s : sheets) {
        for (auto& p : s.tone) p.stop();
        s.click = 0;
    }
    for (auto& s : stones) {
        s.active = false;
        s.strike = 0;
        for (auto& m : s.mode) m.clear();
    }
    fall.active = false;
    for (auto& f : rumble) f.reset();
    for (auto& f : fallNoise) f.reset();
    for (auto& f : sheetClickHigh) f.reset();
    for (auto& f : fallDistance) f.reset();
    groan.reset();
    for (auto* s : { &rumbleGain, &groanGain, &fallGain }) s->snap(0);
    groanHz.snap(55.0f);
}

void EarthLayer::setSeed(uint32_t seed) {
    noiseL.setKey(seed, 601);
    noiseR.setKey(seed, 602);
    events.setKey(seed, 603);
    drift.setKey(seed, 604);
    jitter.setKey(seed, 605);
}

float EarthLayer::spaceSend(const WorldFrame& world) const {
    // Ice caverns ring; rumble is felt more than heard in the space.
    return 0.45f + 0.3f * amounts(world).ice;
}

void EarthLayer::startStone(float amplitude, float pan, float distance, uint64_t e) {
    auto& s = stones[pickVoice(stones, stoneVoices, [](const Stone& v) { return v.active ? v.mode[0].energy() + 1.0e-9f : 0.0f; })];
    const float hz = lerpExp(170.0f, 800.0f, events.unit(e)) * lerp(1.0f, 0.65f, distance);
    const float seconds = lerp(0.02f, 0.06f, events.unit(e + 1));
    s.mode[0].set(hz, seconds, rate);
    s.mode[1].set(hz * lerp(2.3f, 2.9f, events.unit(e + 2)), seconds * 0.6f, rate);
    s.strike = amplitude;
    s.pan = panGains(pan);
    s.active = true;
}

void EarthLayer::render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) {
    const double t = double(clock) / rate;
    const auto a = amounts(world);
    const float activity = world[Activity], weather = world[Weather], scale = world[Scale];
    const int quality = std::clamp(world.quality, 0, 2);
    const int creakVoices = quality == 0 ? 2 : (quality == 1 ? 3 : maxCreaks);
    const int sheetVoices = quality == 0 ? 2 : (quality == 1 ? 3 : maxSheets);
    stoneVoices = quality == 0 ? 6 : (quality == 1 ? 9 : maxStones);
    const float dt = float(n) / rate;
    const uint64_t e = clock * 32;

    // --- Ice: creaks (wind loads the ice) and singing cracks.
    const float creakRate = a.ice * lerp(0.2f, 1.6f, activity) * (1.0f + 0.6f * weather);
    if (events.unit(e) < trialChance(creakRate, n, rate)) {
        std::size_t pick = 0;
        for (int i = 0; i < creakVoices; ++i)
            if (!creaks[std::size_t(i)].active) { pick = std::size_t(i); break; }
        auto& c = creaks[pick];
        c.active = true;
        c.age = 0;
        c.length = lerp(0.18f, 0.7f, events.unit(e + 1));
        c.pulseHz = lerpExp(28.0f, 90.0f, events.unit(e + 2));
        c.pulseGlide = std::pow(lerpExp(0.6f, 1.7f, events.unit(e + 3)), 1.0f / (c.length * rate));
        c.untilPulse = 0;
        c.amp = 0.4f + 0.6f * events.unit(e + 4);
        c.pan = panGains(0.8f * events.white(e + 5));
        const float base = lerpExp(300.0f, 1100.0f, events.unit(e + 6)) * lerp(0.8f, 1.2f, a.ice);
        static constexpr float ratio[modes] { 1.0f, 2.71f, 5.3f }, decay[modes] { 0.014f, 0.009f, 0.005f }, gain[modes] { 1.0f, 0.55f, 0.3f };
        for (int m = 0; m < modes; ++m) {
            c.mode[std::size_t(m)].set(base * ratio[m] * (1.0f + 0.04f * events.white(e + 7 + uint64_t(m))), decay[m], rate);
            c.modeGain[std::size_t(m)] = gain[m];
        }
    }
    const float sheetRate = a.ice * lerp(0.08f, 0.6f, activity);
    if (events.unit(e + 12) < trialChance(sheetRate, n, rate)) {
        auto& s = sheets[pickVoice(sheets, sheetVoices, [](const Sheet& v) { return v.tone[0].amp + v.click; })];
        // The sheet sings from a note of the key, with a second tone on the scale's fifth.
        const float free = 12.0f * std::log2(lerpExp(1800.0f, 4600.0f, events.unit(e + 13)) / music::rootHz(world.root));
        const float hz = music::noteHz(world.root, float(music::quantise(world.mood, free))), seconds = lerp(0.12f, 0.3f, events.unit(e + 14));
        const float amp = 0.5f + 0.5f * events.unit(e + 15), pan = 0.85f * events.white(e + 16);
        const float fifth = std::exp2(float(music::degree(world.mood, music::scaleFor(world.mood).size >= 7 ? 4 : 3)) / 12.0f);
        s.tone[0].start(hz, seconds, 0.4f, amp, pan, rate);
        s.tone[1].start(hz * fifth, seconds * 0.7f, 0.4f, amp * 0.5f, pan, rate);
        s.click = amp * 0.8f;
        s.clickDecay = std::exp(-1.0f / (0.003f * rate));
        s.pan = panGains(pan);
    }
    for (int i = 0; i < sheetVoices; ++i)
        for (auto& p : sheets[std::size_t(i)].tone)
            if (p.active()) p.update(rate);

    // --- Rockfalls: a cluster of stone impacts that builds and thins out.
    if (!fall.active && events.unit(e + 20) < trialChance(a.rockfalls, n, rate)) {
        fall.active = true;
        fall.age = 0;
        fall.length = lerp(1.0f, 3.2f, events.unit(e + 21));
        fall.pan = 0.8f * events.white(e + 22);
        fall.distance = lerp(std::pow(events.unit(e + 23), 0.7f) * 0.65f + 0.35f, 1.0f, 0.4f * scale);
        fall.size = 0.5f + 0.5f * events.unit(e + 24);
    }
    float fallEnv = 0;
    if (fall.active) {
        fall.age += dt;
        const float x = fall.age / fall.length;
        fallEnv = x < 0.12f ? x / 0.12f : std::pow(std::max(0.0f, 1.0f - (x - 0.12f) / 0.88f), 1.6f);
        if (events.unit(e + 25) < trialChance(lerp(20.0f, 60.0f, fall.size) * fallEnv, n, rate)) {
            const float u = events.unit(e + 26);
            startStone(fall.size * (0.25f + 0.75f * u * u) * lerp(1.0f, 0.35f, fall.distance), fall.pan + 0.3f * events.white(e + 27),
                       fall.distance, e + 28);
        }
        if (x >= 1.0f) fall.active = false;
    }
    for (auto& f : fallDistance) f.setCutoff(lerpExp(5000.0f, 800.0f, fall.distance), rate);
    const float fallTarget = 0.10f * fallEnv * fall.size * lerp(1.0f, 0.5f, fall.distance);
    const auto fallPan = panGains(fall.pan);

    // --- The ground: rumble and tectonic groan.
    for (auto& f : rumble) f.setCutoff(lerpExp(120.0f, 55.0f, scale), rate);
    // The ground heaves slowly in a calm world and restlessly in a chaotic one (two
    // fixed-rate curves blended, so turning Activity never jumps the phase).
    const float heave = lerp(float(drift.smooth(t, 0.08)), float(drift.smooth(t + 200.0, 0.45)), activity);
    const float rumbleTarget = 0.1f * a.rumble * std::max(0.0f, 1.0f + lerp(0.25f, 0.7f, activity) * heave);
    // The groan drifts between the low root, fifth and octave of the key.
    {
        const float x = clamp01(0.5f + 0.5f * drift.smooth(t + 30.0, 0.05));
        const float semis = x < 0.4f ? -24.0f : (x < 0.75f ? -24.0f + float(music::degree(world.mood, music::scaleFor(world.mood).size >= 7 ? 4 : 3)) : -12.0f);
        groanHz.next(music::noteHz(world.root, semis));
        groan.set(groanHz.value, 6.0f, rate);
    }
    const float groanTarget = 2.2f * a.groan * (0.45f + 0.55f * std::max(0.0f, drift.smooth(t + 60.0, 0.12)));

    for (int i = 0; i < n; ++i) {
        const uint64_t index = clock + uint64_t(i);
        const float wl = noiseL.white(index), wr = noiseR.white(index);
        float l = 0, r = 0;

        const float rg = rumbleGain.next(rumbleTarget);
        if (rg > 1.0e-5f) {
            l += rumble[0].tick(wl) * rg;
            r += rumble[1].tick(wr) * rg;
        }
        const float gg = groanGain.next(groanTarget);
        if (gg > 1.0e-5f) {
            const float g = groan.bandpass(0.5f * (wl + wr)) * gg;
            l += g;
            r += g;
        }

        for (int v = 0; v < creakVoices; ++v) {
            auto& c = creaks[std::size_t(v)];
            if (!c.active) continue;
            c.age += 1.0f / rate;
            float x = 0;
            if (c.age < c.length && (c.untilPulse -= 1.0f) <= 0.0f) {
                const float env = std::sin(pi * c.age / c.length);
                x = env * c.amp * (0.3f + 0.7f * jitter.unit(index));
                c.untilPulse += rate / c.pulseHz * (1.0f + 0.5f * jitter.white(index + (1ull << 44)));
                c.pulseHz *= std::pow(c.pulseGlide, rate / c.pulseHz);
            }
            float s = 0;
            for (int m = 0; m < modes; ++m) s += c.mode[std::size_t(m)].tick(x) * c.modeGain[std::size_t(m)];
            s *= 0.3f;
            l += s * c.pan.l;
            r += s * c.pan.r;
            if (c.age > c.length + 0.15f) {
                c.active = false;
                for (auto& m : c.mode) m.clear();
            }
        }

        for (int v = 0; v < sheetVoices; ++v) {
            auto& s = sheets[std::size_t(v)];
            if (!s.active()) continue;
            float y = 0;
            for (auto& p : s.tone)
                if (p.active()) y += p.tick();
            const float click = s.click * noiseL.white(index + (2ull << 44));
            s.click *= s.clickDecay;
            l += (y * 0.16f) * s.tone[0].pan.l + sheetClickHigh[0].hp(click * s.pan.l) * 0.3f;
            r += (y * 0.16f) * s.tone[0].pan.r + sheetClickHigh[1].hp(click * s.pan.r) * 0.3f;
        }

        float fl = 0, fr = 0;
        for (int v = 0; v < stoneVoices; ++v) {
            auto& s = stones[std::size_t(v)];
            if (!s.active) continue;
            const float y = (s.mode[0].tick(s.strike) + 0.5f * s.mode[1].tick(s.strike)) * 0.35f;
            s.strike = 0;
            fl += y * s.pan.l;
            fr += y * s.pan.r;
            if (s.mode[0].energy() < 1.0e-10f && s.mode[1].energy() < 1.0e-10f) s.active = false;
        }
        const float fg = fallGain.next(fallTarget);
        if (fg > 1.0e-5f) {
            fl += fallNoise[0].tick(wl) * fg * fallPan.l;
            fr += fallNoise[1].tick(wr) * fg * fallPan.r;
        }
        l += fallDistance[0].lp(fl);
        r += fallDistance[1].lp(fr);

        left[i] = l;
        right[i] = r;
    }
}

} // namespace wb
