#include "AirLayer.h"

namespace wb {

using namespace dsp;

void AirLayer::prepare(double sampleRate) {
    rate = float(sampleRate);
    const float controlRate = rate / float(chunk);
    gust.setTime(0.15f, controlRate);
    pan.setTime(0.3f, controlRate);
    for (auto* s : { &bodyGain, &whistleGain, &howlGain }) s->setTime(0.05f, rate);
    reset();
}

void AirLayer::reset() {
    for (auto& f : body) f.reset();
    for (auto& f : thin) f.reset();
    for (auto& f : whistle) f.reset();
    howl.reset();
    pinkL.reset();
    pinkR.reset();
    gust.snap(0.3f);
    pan.snap(0);
    for (auto* s : { &bodyGain, &whistleGain, &howlGain }) s->snap(0);
}

void AirLayer::setSeed(uint32_t seed) {
    noiseL.setKey(seed, 101);
    noiseR.setKey(seed, 102);
    noiseC.setKey(seed, 103);
    gustSlow.setKey(seed, 110);
    gustFast.setKey(seed, 111);
    swirl.setKey(seed, 112);
    howlDrift.setKey(seed, 113);
    for (int i = 0; i < maxWhistles; ++i) whistleDrift[std::size_t(i)].setKey(seed, 120 + uint32_t(i));
}

float AirLayer::spaceSend(const WorldFrame& world) const {
    // Thin icy wind carries further; a close hot rush is drier.
    return lerp(0.6f, 0.3f, world[Temperature]);
}

void AirLayer::render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) {
    const double t = double(clock) / rate;
    const float temp = world[Temperature], weather = world[Weather], activity = world[Activity];

    // Gust envelope. Fixed-rate random curves, blended by Activity, so the shape is a
    // pure function of time (changing a knob never jumps the phase).
    const float strength = 0.10f + 0.90f * std::pow(weather, 1.3f);
    const float gustiness = (0.3f + 0.7f * weather) * (0.4f + 1.2f * activity);
    const float wander = (1.0f - activity) * gustSlow.smooth(t, 0.07) + activity * gustFast.smooth(t, 0.6);
    const float swell = 0.6f * gustSlow.smooth(t + 31.7, 0.22);
    const float g = gust.next(std::clamp(strength * (1.0f + gustiness * (0.8f * wander + swell)), 0.03f, 1.6f));

    // Body band: frozen = high and thin, scorching = low rush.
    const float lo = lerpExp(900.0f, 90.0f, temp), hi = lerpExp(3200.0f, 420.0f, temp);
    const float centre = lerpExp(lo, hi, clamp01(g / 1.3f));
    const float q = lerp(1.4f, 0.6f, temp);
    const float hot = smoothstep(0.55f, 1.0f, temp);
    for (auto& f : body) f.set(centre, q, rate);
    for (auto& f : thin) f.setCutoff(lerpExp(350.0f, 25.0f, temp), rate);
    // Humid air absorbs the highs; arid air is bright and dusty.
    const float air = lerpExp(4000.0f, 600.0f, temp) * lerpExp(1.8f, 0.4f, world[Wetness]);
    pinkL.setCutoff(air, rate);
    pinkR.setCutoff(air, rate);
    // Band-limited noise power grows with bandwidth (centre / Q); normalise it.
    const float bodyTarget = 2.2f * g * std::sqrt(q * 500.0f / centre);

    // Whistling resonances.
    const int whistles = 1 + std::clamp(world.quality, 0, 2);
    const float wLo = lerpExp(1400.0f, 450.0f, temp), wHi = lerpExp(3800.0f, 1100.0f, temp);
    // Ancient air breathes; futuristic air sings through metal, with tighter resonances.
    const float era = world[Era];
    const float wq = (16.0f + 10.0f * (1.0f - temp)) * lerpExp(0.5f, 2.2f, era);
    for (int i = 0; i < whistles; ++i) {
        const float drift = 0.5f + 0.5f * whistleDrift[std::size_t(i)].smooth(t, 0.05 + 0.02 * i);
        whistle[std::size_t(i)].set(lerpExp(wLo, wHi, clamp01(drift)) * (1.0f + 0.15f * g), wq, rate);
    }
    const float whistleTarget = (0.1f + 0.6f * weather) * (1.0f + 0.6f * (1.0f - temp)) * lerp(0.5f, 1.8f, era) * std::pow(g, 1.5f)
                                * 10.0f * std::sqrt(wq * 500.0f / wHi) / std::sqrt(float(whistles));
    static constexpr float whistlePan[maxWhistles] { -0.6f, 0.55f, 0.05f };

    // Storm howl.
    const float howlAmount = smoothstep(0.5f, 0.95f, weather + 0.3f * world[Danger]);
    const float howlHz = lerpExp(170.0f, 420.0f, clamp01(g / 1.3f)) * (1.0f + 0.05f * howlDrift.smooth(t, 0.2));
    howl.set(howlHz, 9.0f, rate);
    const float howlTarget = howlAmount * std::pow(g, 1.5f) * 2.5f * std::sqrt(9.0f * 500.0f / howlHz);

    const float p = pan.next(0.35f * swirl.smooth(t, 0.09));
    const float gainL = std::sqrt(0.5f * (1.0f - p)) * 1.4142f, gainR = std::sqrt(0.5f * (1.0f + p)) * 1.4142f;

    for (int i = 0; i < n; ++i) {
        const uint64_t index = clock + uint64_t(i);
        const float c = noiseC.white(index);
        const float inL = pinkL.lp(0.6f * c + 0.4f * noiseL.white(index)) * 2.0f;
        const float inR = pinkR.lp(0.6f * c + 0.4f * noiseR.white(index)) * 2.0f;

        const auto bl = body[0].tick(inL), br = body[1].tick(inR);
        const float k = body[0].k;
        float l = lerp(k * bl.bp, 0.7f * bl.lp, hot);
        float r = lerp(k * br.bp, 0.7f * br.lp, hot);
        l = thin[0].hp(l);
        r = thin[1].hp(r);
        const float bg = bodyGain.next(bodyTarget);
        l *= bg * gainL;
        r *= bg * gainR;

        const float wg = whistleGain.next(whistleTarget);
        for (int w = 0; w < whistles; ++w) {
            const float s = whistle[std::size_t(w)].bandpass(c) * wg;
            l += s * (1.0f - whistlePan[w]);
            r += s * (1.0f + whistlePan[w]);
        }
        const float hg = howlGain.next(howlTarget);
        if (hg > 1.0e-5f) {
            const float h = howl.bandpass(c) * hg;
            l += h;
            r += h;
        }
        left[i] = l;
        right[i] = r;
    }
}

} // namespace wb
