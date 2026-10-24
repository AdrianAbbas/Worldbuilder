#pragma once
#include "Events.h"
#include "Generator.h"

namespace wb {

// Fire & Heat: crackles and pops (noise bursts through resonant bands, arriving in
// flickering clusters), a low flickering roar, the fizz of embers and, in scorching
// worlds, lava: low resonant bloops over a thick churn. Temperature brings it in,
// Wetness douses it, Weather fans it, Era turns wood fire into hotter, tighter
// plasma crackle.
class FireLayer final : public Generator {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setSeed(uint32_t seed) override;
    void render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) override;
    float spaceSend(const WorldFrame& world) const override;

    static constexpr int maxCrackles = 16, maxBlips = 6;

    struct Amounts { float crackle, roar, embers, lava; };
    static Amounts amounts(const WorldFrame& world);

private:
    struct Crackle {
        dsp::Svf band;
        float amp = 0, decay = 1, gain = 1;
        dsp::PanGains pan { 0.7071f, 0.7071f };
        uint64_t noiseOffset = 0;
        bool active() const { return amp > 1.0e-4f; }
    };

    float rate = 48000;
    dsp::Stream noiseL, noiseR, events, flicker, ember;
    std::array<Crackle, maxCrackles> crackles;
    std::array<dsp::Ping, maxBlips> blips;
    std::array<dsp::Rumble, 2> roarSource, churn;
    std::array<dsp::Svf, 2> roarBody;
    std::array<dsp::OnePole, 2> emberHigh;
    dsp::Smoother roarGain, emberGain, churnGain;
};

} // namespace wb
