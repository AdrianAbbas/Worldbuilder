#pragma once
#include "Events.h"
#include "Generator.h"

namespace wb {

// Ice & Earth: the ground itself. Frozen worlds creak (stick-slip pulse trains
// through modal resonators) and their ice sheets crack and sing (a sharp tick, then
// a falling dispersive "pew"). Every world has a low rumble; hostile, hot or
// colossal ones add a tectonic groan and distant rockfalls (clusters of stone
// impacts heard through distance).
class EarthLayer final : public Generator {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setSeed(uint32_t seed) override;
    void render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) override;
    float spaceSend(const WorldFrame& world) const override;

    static constexpr int maxCreaks = 4, maxSheets = 4, maxStones = 12, modes = 3;

    struct Amounts { float ice, rumble, groan, rockfalls; };
    static Amounts amounts(const WorldFrame& world);

private:
    struct Creak {
        std::array<dsp::Mode, modes> mode;
        std::array<float, modes> modeGain {};
        float age = 0, length = 0, pulseHz = 50, pulseGlide = 1, amp = 0;
        float untilPulse = 0;
        dsp::PanGains pan { 0.7071f, 0.7071f };
        bool active = false;
    };
    struct Sheet {   // an ice sheet cracking and singing
        std::array<dsp::Ping, 2> tone;
        float click = 0, clickDecay = 0;
        dsp::PanGains pan { 0.7071f, 0.7071f };
        bool active() const { return tone[0].active() || click > 1.0e-4f; }
    };
    struct Stone {
        std::array<dsp::Mode, 2> mode;
        float strike = 0;   // impulse waiting for the next sample
        dsp::PanGains pan { 0.7071f, 0.7071f };
        bool active = false;
    };
    struct Rockfall {
        float age = 0, length = 0, pan = 0, distance = 0.5f, size = 1;
        bool active = false;
    };

    void startStone(float amplitude, float pan, float distance, uint64_t e);

    float rate = 48000;
    dsp::Stream noiseL, noiseR, events, drift, jitter;
    std::array<Creak, maxCreaks> creaks;
    std::array<Sheet, maxSheets> sheets;
    std::array<Stone, maxStones> stones;
    Rockfall fall;
    int stoneVoices = maxStones;
    std::array<dsp::Rumble, 2> rumble, fallNoise;
    dsp::Svf groan;
    std::array<dsp::OnePole, 2> sheetClickHigh, fallDistance;
    dsp::Smoother rumbleGain, groanGain, fallGain, groanHz;
};

} // namespace wb
