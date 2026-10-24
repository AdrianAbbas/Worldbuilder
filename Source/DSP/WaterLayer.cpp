#include "WaterLayer.h"

namespace wb {

using namespace dsp;

namespace {
// Offsets into the noise streams, so one stream can feed several independent sources.
constexpr uint64_t rushOffset = 1ull << 44, fallOffset = 2ull << 44, waveOffset = 3ull << 44, gurgleOffset = 4ull << 44;
constexpr float wavePeriod[WaterLayer::waveClocks] { 6.1f, 3.4f };   // calm swell, storm surf
}

WaterLayer::Amounts WaterLayer::amounts(const WorldFrame& world) {
    const float wet = world[Wetness], weather = world[Weather], temp = world[Temperature], scale = world[Scale];
    const float frozen = 1.0f - smoothstep(0.08f, 0.32f, temp);
    const float dry = smoothstep(0.78f, 1.0f, temp);   // scorching air evaporates it
    const float liquid = (1.0f - frozen) * (1.0f - 0.7f * dry);
    // Rain needs clouds: Weather turns drizzle into a downpour.
    const float precipitation = smoothstep(0.22f, 0.85f, wet) * lerp(0.2f, 1.0f, smoothstep(0.05f, 0.8f, weather));
    Amounts a;
    a.rain = precipitation * liquid;
    a.snow = precipitation * frozen;
    // Drips in caves and after rain: heard best when it is not pouring.
    a.drips = smoothstep(0.12f, 0.55f, wet) * (1.0f - 0.6f * a.rain) * (1.0f - 0.8f * frozen) * (1.0f - 0.8f * dry);
    a.stream = smoothstep(0.42f, 0.85f, wet) * liquid;
    a.waterfall = a.stream * smoothstep(0.72f, 1.0f, wet) * lerp(0.35f, 1.0f, std::max(scale, weather));
    a.waves = smoothstep(0.55f, 1.0f, wet) * liquid;
    return a;
}

void WaterLayer::prepare(double sampleRate) {
    rate = float(sampleRate);
    for (auto* s : { &hissGain, &snowGain, &patterGain, &rushGain, &fallGain }) s->setTime(0.08f, rate);
    for (auto& s : gurgleGain) s.setTime(0.02f, rate);
    for (auto& d : dripRoom) d.prepare(std::size_t(0.06f * rate));
    for (auto& s : shimmerHz) s.setTime(0.6f, rate / float(chunk));
    shimmerGain.setTime(0.1f, rate);
    for (auto& w : waves) {
        w.gain.setTime(0.01f, rate);
        w.fizzGain.setTime(0.01f, rate);
        w.cutoff.setTime(0.05f, rate / float(chunk));
    }
    reset();
}

void WaterLayer::reset() {
    for (auto& d : drops) d.stop();
    for (auto& d : drips) d.reset();
    for (auto& d : dripChirps) d.stop();
    for (auto& d : dripRoom) d.clear();
    for (auto& f : shimmer) f.reset();
    for (auto& s : shimmerHz) s.snap(1000.0f);
    shimmerGain.snap(0);
    echoWait = -1;
    for (auto& f : patterTone) f.reset();
    for (auto* bank : { &hissHigh, &hissLow, &snowLow, &rushLow, &rushHigh, &fallLow, &fallHigh })
        for (auto& f : *bank) f.reset();
    for (auto& f : gurgle) f.reset();
    for (auto& w : waves) {
        for (auto& f : w.body) f.reset();
        for (auto& f : w.fizz) f.reset();
        w.gain.snap(0);
        w.fizzGain.snap(0);
        w.cutoff.snap(400);
    }
    for (auto* s : { &hissGain, &snowGain, &patterGain, &rushGain, &fallGain }) s->snap(0);
    for (auto& s : gurgleGain) s.snap(0);
}

void WaterLayer::setSeed(uint32_t seed) {
    hissL.setKey(seed, 401);
    hissR.setKey(seed, 402);
    patter.setKey(seed, 403);
    patterShape.setKey(seed, 404);
    events.setKey(seed, 405);
    gurgleDrift.setKey(seed, 406);
    waveShape.setKey(seed, 407);
    dripShape.setKey(seed, 408);
}

float WaterLayer::spaceSend(const WorldFrame& world) const {
    // Drips ring through caverns; a close downpour is drier.
    const auto a = amounts(world);
    return 0.4f + 0.35f * a.drips - 0.1f * a.rain;
}

// A drip: a soft, resonant drop on a note of the key (mostly the current chord's
// tones, two octaves up; lower and longer in caverns), with the tiny rising "plink" of
// a real drop leading into it. Random velocity, decay and stereo position.
void WaterLayer::startDrip(const WorldFrame& world, uint64_t e, float velocity, int voices) {
    const float scale = world[Scale];
    int semis = 0;
    if (echoEvent == e) {
        semis = echoSemis;
    } else {
        const int low = int(lerp(24.0f, 14.0f, scale));
        semis = events.unit(e + 1) < 0.65f ? music::chordTone(world.mood, world.chord, int(events.unit(e + 2) * 3.0f))
                                           : music::degree(world.mood, int(events.unit(e + 2) * 7.0f));
        semis = music::inRange(semis, low) + (events.unit(e + 3) < 0.3f ? 12 : 0);
    }
    const float hz = music::noteHz(world.root, float(semis));
    const float decay = lerp(0.45f, 1.5f, events.unit(e + 4)) * lerp(0.8f, 1.6f, scale);
    const float pan = 0.85f * events.white(e + 5);
    static constexpr float ratios[3] { 1.0f, 2.0f, 4.07f };
    static constexpr float decays[3] { 1.0f, 0.3f, 0.1f };
    static constexpr float gains[3] { 1.0f, 0.12f, 0.035f };
    const auto quietest = [](const Bell& b) { return b.level(); };
    const auto v = pickVoice(drips, voices, quietest);
    drips[v].start(hz, decay, 0.12f, velocity, pan, rate, ratios, decays, gains, 3);
    dripChirps[v].start(hz * 0.84f, 0.012f, 1.25f, 0.25f * velocity, pan, rate, 0.002f);
    // Now and then a second, smaller drop follows: the same note or an octave up.
    if (echoEvent != e && events.unit(e + 6) < 0.22f) {
        echoWait = lerp(0.06f, 0.18f, events.unit(e + 7)) * rate;
        echoAmp = velocity * 0.45f;
        echoSemis = semis + (events.unit(e + 8) < 0.35f ? 12 : 0);
        echoEvent = e + 1000;
    }
}

void WaterLayer::render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) {
    const double t = double(clock) / rate;
    const auto a = amounts(world);
    const float weather = world[Weather], scale = world[Scale], activity = world[Activity];
    const int quality = std::clamp(world.quality, 0, 2);
    const int dropVoices = quality == 0 ? 6 : (quality == 1 ? 10 : maxDrops);
    const int dripVoices = quality == 0 ? 3 : (quality == 1 ? 5 : maxDrips);
    // Distance: big worlds hear the water from further off.
    const float near = lerp(1.0f, 0.55f, scale);

    // --- Events, once per chunk: drops and stream bubbles share a pool; drips have their own.
    const uint64_t e = clock * 32;
    const auto quietest = [](const Ping& p) { return p.amp; };
    const float dropRate = a.rain * lerp(12.0f, 150.0f, a.rain);
    if (events.unit(e) < trialChance(dropRate, n, rate)) {
        auto& d = drops[pickVoice(drops, dropVoices, quietest)];
        const float size = events.unit(e + 1);
        const float polarity = events.unit(e + 10) < 0.5f ? -1.0f : 1.0f;
        // Drops land on notes of the key, as tiny pitched plucks.
        const float free = 12.0f * std::log2(lerpExp(4200.0f, 1500.0f, size) * near / music::rootHz(world.root));
        d.start(music::noteHz(world.root, float(music::quantise(world.mood, free))), lerp(0.008f, 0.022f, size), 1.04f,
                polarity * (0.2f + 0.6f * size * size), 0.85f * events.white(e + 2), rate);
    }
    const float bubbleRate = a.stream * lerp(25.0f, 110.0f, a.stream) * lerp(0.7f, 1.3f, activity);
    if (events.unit(e + 3) < trialChance(bubbleRate, n, rate)) {
        auto& d = drops[pickVoice(drops, dropVoices, quietest)];
        const float size = events.unit(e + 4);
        const float polarity = events.unit(e + 11) < 0.5f ? -1.0f : 1.0f;
        const float free = 12.0f * std::log2(lerpExp(1500.0f, 380.0f, size) * near / music::rootHz(world.root));
        d.start(music::noteHz(world.root, float(music::quantise(world.mood, free))), lerp(0.01f, 0.03f, size), 1.3f,
                polarity * (0.3f + 0.5f * size), 0.6f * events.white(e + 5), rate);
    }
    // Drips arrive in loose clusters: a slow random curve swells and thins their rate.
    const float cluster = 0.35f + 1.3f * smoothstep(-0.4f, 0.8f, dripShape.smooth(t, 0.12));
    const float dripRate = a.drips * lerpExp(0.3f, 6.0f, activity) * lerp(0.8f, 1.4f, scale) * cluster;
    // Rain plucks: now and then a raindrop rings out as a note of the key.
    const float pluckRate = a.rain * lerp(1.0f, 7.0f, a.rain);
    const uint64_t dripEvent = e + (1ull << 40);
    if (events.unit(e + 6) < trialChance(dripRate + pluckRate, n, rate))
        startDrip(world, dripEvent, (0.25f + 0.75f * std::pow(events.unit(e + 7), 1.5f)) * (events.unit(e + 8) * (dripRate + pluckRate) < pluckRate ? 0.6f : 1.0f),
                  dripVoices);
    if (echoWait >= 0) {
        echoWait -= float(n);
        if (echoWait < 0) {
            echoEvent = dripEvent;
            startDrip(world, dripEvent, echoAmp, dripVoices);
            echoEvent = 0;
        }
    }
    for (int i = 0; i < dropVoices; ++i)
        if (drops[std::size_t(i)].active()) drops[std::size_t(i)].update(rate);
    for (int i = 0; i < dripVoices; ++i) {
        if (drips[std::size_t(i)].active) drips[std::size_t(i)].update();
        if (dripChirps[std::size_t(i)].active()) dripChirps[std::size_t(i)].update(rate);
    }

    // Shimmer: four narrow resonances on the current chord, rung by the rain.
    for (int k = 0; k < shimmers; ++k) {
        static constexpr int low[shimmers] { 24, 24, 31, 36 };
        const int semis = music::inRange(music::chordTone(world.mood, world.chord, k % 3), low[k]);
        shimmer[std::size_t(k)].set(shimmerHz[std::size_t(k)].next(music::noteHz(world.root, float(semis))), 45.0f, rate);
    }
    const float shimmerTarget = 3.0f * (a.rain + 0.4f * a.stream);

    // --- Continuous parts.
    const float patterChance = trialChance(a.rain * lerpExp(150.0f, 3500.0f, a.rain), 1, rate);
    for (auto& f : patterTone) f.set(lerpExp(4200.0f, 2000.0f, scale), 0.8f, rate);
    for (auto& f : hissHigh) f.setCutoff(1100.0f, rate);
    for (auto& f : hissLow) f.setCutoff(lerpExp(9000.0f, 3200.0f, scale), rate);
    for (auto& f : snowLow) f.setCutoff(900.0f, rate);
    for (auto& f : rushLow) f.setCutoff(650.0f * near, rate);
    for (auto& f : rushHigh) f.setCutoff(110.0f, rate);
    for (auto& f : fallLow) f.setCutoff(2600.0f * near, rate);
    for (auto& f : fallHigh) f.setCutoff(80.0f, rate);
    const float hissTarget = 0.06f * std::pow(a.rain, 1.3f);
    const float snowTarget = 0.10f * a.snow * (0.75f + 0.25f * patterShape.smooth(t, 0.2));
    const float patterTarget = 1.2f * std::sqrt(a.rain);
    const float rushTarget = 0.1f * a.stream;
    const float fallTarget = 0.14f * a.waterfall;
    std::array<float, gurgles> gurgleTarget {};
    for (int g = 0; g < gurgles; ++g) {
        const auto drift = [&](double offset, double hz) { return gurgleDrift.smooth(t + offset, hz); };
        const float centre = lerpExp(280.0f, 1500.0f, clamp01(0.5f + 0.55f * float(drift(17.0 * g, 4.0 + 1.7 * g)))) * near;
        gurgle[std::size_t(g)].set(centre, 7.0f, rate);
        const float bubble = std::max(0.0f, float(drift(100.0 + 23.0 * g, 6.0 + 2.3 * g)));
        gurgleTarget[std::size_t(g)] = 0.9f * a.stream * bubble * bubble;
    }

    // Waves: two fixed-period clocks (calm swell, storm surf) blended by Weather, so
    // turning the knob never jumps a wave. Each cycle swells, breaks and washes back.
    std::array<float, waveClocks> waveEnv {}, waveFizz {};
    for (int w = 0; w < waveClocks; ++w) {
        auto& wave = waves[std::size_t(w)];
        const double phase = t / wavePeriod[w] + 0.37 * w;
        const double cycle = std::floor(phase);
        const float f = float(phase - cycle);
        const auto index = uint64_t(int64_t(cycle) + (int64_t(1) << 40)) * 4 + uint64_t(w);
        const float size = 0.55f + 0.45f * waveShape.unit(index);
        const float env = f < 0.4f ? std::pow(std::sin(0.5f * pi * f / 0.4f), 2.0f) : std::pow(1.0f - (f - 0.4f) / 0.6f, 2.5f);
        const float weight = w == 0 ? 1.0f - 0.7f * weather : weather * 1.3f;
        waveEnv[std::size_t(w)] = 0.5f * a.waves * weight * size * env;
        waveFizz[std::size_t(w)] = waveEnv[std::size_t(w)] * smoothstep(0.42f, 0.6f, f) * 0.6f;
        wave.pan = panGains(0.5f * waveShape.white(index + 2));
        const float cutoff = wave.cutoff.next(lerpExp(220.0f, 2400.0f, env) * lerp(1.0f, 0.6f, scale));
        for (auto& b : wave.body) b.set(cutoff, 0.7f, rate);
        for (auto& z : wave.fizz) z.setCutoff(2500.0f, rate);
    }

    const bool anyWaves = a.waves > 1.0e-4f || waves[0].gain.value > 1.0e-5f || waves[1].gain.value > 1.0e-5f;
    for (int i = 0; i < n; ++i) {
        const uint64_t index = clock + uint64_t(i);
        const float wl = hissL.white(index), wr = hissR.white(index);
        float l = 0, r = 0;

        // Rain hiss and snow hush.
        const float hg = hissGain.next(hissTarget), sg = snowGain.next(snowTarget);
        if (hg > 1.0e-5f) {
            l += hissLow[0].lp(hissHigh[0].hp(wl)) * hg;
            r += hissLow[1].lp(hissHigh[1].hp(wr)) * hg;
        }
        if (sg > 1.0e-5f) {
            l += snowLow[0].lp(wl) * sg;
            r += snowLow[1].lp(wr) * sg;
        }

        // Patter: tiny ticks with power-law sizes, scattered left and right.
        const float pg = patterGain.next(patterTarget);
        float tickL = 0, tickR = 0;
        if (patterChance > 0 && patter.unit(index) < patterChance) {
            const float size = patterShape.unit(index);
            const float amp = size * size * size;
            const float side = patterShape.unit(index + rushOffset);
            tickL = amp * std::sqrt(1.0f - side);
            tickR = amp * std::sqrt(side);
        }
        if (pg > 1.0e-5f) {
            l += patterTone[0].bandpass(tickL) * pg;
            r += patterTone[1].bandpass(tickR) * pg;
        }

        // Stream: a low rush, gurgling bands and (above) bubbles; then the waterfall roar.
        const float rg = rushGain.next(rushTarget);
        if (rg > 1.0e-5f) {
            const float nl = hissL.white(index + rushOffset), nr = hissR.white(index + rushOffset);
            l += rushHigh[0].hp(rushLow[0].lp(nl) * 3.0f) * rg;
            r += rushHigh[1].hp(rushLow[1].lp(nr) * 3.0f) * rg;
            const float source = hissL.white(index + gurgleOffset);
            for (int g = 0; g < gurgles; ++g) {
                const float s = gurgle[std::size_t(g)].bandpass(source) * gurgleGain[std::size_t(g)].next(gurgleTarget[std::size_t(g)]);
                const float p = 0.3f * float(g - 1);
                l += s * (1.0f - p);
                r += s * (1.0f + p);
            }
        }
        const float fg = fallGain.next(fallTarget);
        if (fg > 1.0e-5f) {
            const float nl = hissL.white(index + fallOffset), nr = hissR.white(index + fallOffset);
            l += fallHigh[0].hp(fallLow[0].lp(nl)) * fg;
            r += fallHigh[1].hp(fallLow[1].lp(nr)) * fg;
        }

        if (anyWaves) {
            const float nl = hissL.white(index + waveOffset), nr = hissR.white(index + waveOffset);
            for (int w = 0; w < waveClocks; ++w) {
                auto& wave = waves[std::size_t(w)];
                const float g = wave.gain.next(waveEnv[std::size_t(w)]), z = wave.fizzGain.next(waveFizz[std::size_t(w)]);
                const float bl = wave.body[0].tick(nl).lp * g + wave.fizz[0].hp(nl) * z;
                const float br = wave.body[1].tick(nr).lp * g + wave.fizz[1].hp(nr) * z;
                l += bl * wave.pan.l * 1.4142f;
                r += br * wave.pan.r * 1.4142f;
            }
        }

        // Shimmer, rung by the patter and a breath of the hiss.
        const float shg = shimmerGain.next(shimmerTarget);
        if (shg > 1.0e-5f) {
            const float x = (tickL + tickR) * 4.0f + 0.05f * wl;
            for (int k = 0; k < shimmers; ++k) {
                const float y = shimmer[std::size_t(k)].bandpass(x) * shg;
                l += y * (k & 1 ? 0.5f : 1.0f);
                r += y * (k & 1 ? 1.0f : 0.5f);
            }
        }

        // Drops and bubbles.
        for (int v = 0; v < dropVoices; ++v) {
            auto& d = drops[std::size_t(v)];
            if (!d.active()) continue;
            const float s = d.tick() * 0.21f;
            l += s * d.pan.l;
            r += s * d.pan.r;
        }
        // Drips, through a little room of early reflections.
        float dl = 0, dr = 0;
        for (int v = 0; v < dripVoices; ++v) {
            auto& d = drips[std::size_t(v)];
            auto& c = dripChirps[std::size_t(v)];
            if (!d.active && !c.active()) continue;
            const float s = (d.active ? d.tick() : 0.0f) * 0.14f + (c.active() ? c.tick() * 0.14f : 0.0f);
            dl += s * d.pan.l;
            dr += s * d.pan.r;
        }
        dripRoom[0].push(dl);
        dripRoom[1].push(dr);
        l += dl + 0.32f * dripRoom[0].read(0.017f * rate) + 0.18f * dripRoom[1].read(0.041f * rate);
        r += dr + 0.32f * dripRoom[1].read(0.023f * rate) + 0.18f * dripRoom[0].read(0.053f * rate);
        left[i] = l;
        right[i] = r;
    }
}

} // namespace wb
