#pragma once
#include "Events.h"
#include "Generator.h"

namespace wb {

// Thunder: Poisson-timed strikes, each at its own distance. The flash comes first
// and the sound follows after a delay that grows with distance. Each strike lands as
// a tuned low impact on the key's root with a swell on the root and fifth, like a
// timpani hit and a brass swell in a score. Close strikes also tear with a crack;
// distant ones are softer and darker. The rumble rolls on underneath: low-passed
// noise with an irregular rolling envelope that darkens as it fades. Weather (and Danger) set how often and how close, Scale how
// far away the storm sits.
class ThunderLayer final : public Generator {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setSeed(uint32_t seed) override;
    void render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) override;
    float spaceSend(const WorldFrame& world) const override;

    static constexpr int maxStrikes = 3, maxBursts = 7;

    // Strikes per second for a world.
    static float strikeRate(const WorldFrame& world);

    // The latest lightning flash (0..1), set when a strike is triggered and decaying.
    // For the editor.
    float flash() const { return flashLevel; }
    // Positions (samples) of the latest strikes, for the tests.
    static constexpr int logSize = 64;
    uint32_t strikeCount() const { return strikes; }
    uint64_t strikeClock(uint32_t i) const { return strikeLog[std::size_t(i % logSize)]; }

private:
    struct Strike {
        bool active = false;
        float wait = 0, age = 0, length = 1, distance = 0.5f, size = 1, attack = 0.1f;
        dsp::PanGains pan { 0.7071f, 0.7071f };
        dsp::Stream roll;
        std::array<float, maxBursts> burstAt {}, burstAmp {};
        int bursts = 0, nextBurst = 0;
        float crack = 0, crackLevel = 0, boomLevel = 0;
        uint64_t noiseOffset = 0;
        std::array<dsp::Rumble, 2> body;
        std::array<dsp::Svf, 2> tone;
        std::array<dsp::OnePole, 2> crackHigh;
        dsp::Rumble boom;
        dsp::Smoother gain, boomGain;
        // The musical part: a tuned low impact on the root and a swell on root and fifth.
        dsp::Phasor impact;
        std::array<dsp::Phasor, 2> swell;
        dsp::Smoother impactGain, swellGain;
        float impactHz = 40, loudness = 0;
        bool tuned = false;
    };

    float rate = 48000;
    uint32_t seedValue = 1;
    dsp::Stream noiseL, noiseR, events;
    std::array<Strike, maxStrikes> strike;
    float flashLevel = 0, flashDecay = 1, crackDecay = 0.99f;
    uint32_t strikes = 0;
    std::array<uint64_t, logSize> strikeLog {};
};

} // namespace wb
