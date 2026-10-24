#pragma once
#include "Generator.h"

namespace wb {

// Wind: decorrelated noise through a gust-driven band, high whistling resonances
// and, when stormy, a low howl. Frozen worlds are thin and high, scorching ones a
// low dry rush.
class AirLayer final : public Generator {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setSeed(uint32_t seed) override;
    void render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) override;
    float spaceSend(const WorldFrame& world) const override;

    static constexpr int maxWhistles = 3;

private:
    float rate = 48000;
    dsp::Stream noiseL, noiseR, noiseC, gustSlow, gustFast, swirl;
    std::array<dsp::Stream, maxWhistles> whistleDrift;
    dsp::Stream howlDrift;
    dsp::OnePole pinkL, pinkR;
    std::array<dsp::Svf, 2> body;
    std::array<dsp::OnePole, 2> thin;
    std::array<dsp::Svf, maxWhistles> whistle;
    dsp::Svf howl;
    dsp::Smoother gust, bodyGain, whistleGain, howlGain, pan;
};

} // namespace wb
