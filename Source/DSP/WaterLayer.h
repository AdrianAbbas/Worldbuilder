#pragma once
#include "Events.h"
#include "Generator.h"

namespace wb {

// Water: rain (a dense patter of tiny ticks, bubbling drops and a hiss bed), drips
// (pitched resonant plinks), a babbling stream that grows into a waterfall, and
// lapping waves. Wetness brings it in, Weather turns drizzle into a downpour and
// calm water into surf, Scale moves it away (darker, lower, more reverberant).
// Frozen worlds turn the rain to a soft snow hush and still the water; scorching
// ones dry it up.
class WaterLayer final : public Generator {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setSeed(uint32_t seed) override;
    void render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) override;
    float spaceSend(const WorldFrame& world) const override;

    static constexpr int maxDrops = 16, maxDrips = 4, gurgles = 3, waveClocks = 2;

    // How much of each part plays (0..1) for a world. Public for the tests.
    struct Amounts { float rain, snow, drips, stream, waterfall, waves; };
    static Amounts amounts(const WorldFrame& world);

private:
    float rate = 48000;
    dsp::Stream hissL, hissR, patter, patterShape, events, gurgleDrift, waveShape;
    std::array<dsp::Ping, maxDrops> drops;
    std::array<dsp::Ping, maxDrips> drips;
    std::array<dsp::Svf, 2> patterTone;
    std::array<dsp::OnePole, 2> hissHigh, hissLow, snowLow, rushLow, rushHigh, fallLow, fallHigh;
    std::array<dsp::Svf, gurgles> gurgle;
    struct Wave {
        std::array<dsp::Svf, 2> body;
        std::array<dsp::OnePole, 2> fizz;
        dsp::Smoother gain, fizzGain, cutoff;
        dsp::PanGains pan { 0.7071f, 0.7071f };
    };
    std::array<Wave, waveClocks> waves;
    dsp::Smoother hissGain, snowGain, patterGain, rushGain, fallGain;
    std::array<dsp::Smoother, gurgles> gurgleGain;
};

} // namespace wb
