#pragma once
#include "Generator.h"

namespace wb {

// The shared stereo space every layer sends into: pre-delay, input diffusion and an
// 8-line feedback delay network with a Householder matrix, damping in the loop and
// slow delay modulation. Scale sets size, decay and pre-delay; Temperature and
// Wetness set the tone (ice caverns bright and long, a drenched jungle dense and
// damped); Magic makes it ring longer and brighter.
class WorldSpace {
public:
    void prepare(double sampleRate);
    void reset();
    // Called once per chunk.
    void update(const WorldFrame& world);
    // Adds the wet signal for `inL/inR` into `outL/outR`.
    void process(const float* inL, const float* inR, float* outL, float* outR, int n);

    float decaySeconds() const { return t60; }

    static constexpr int lines = 8;

private:
    float rate = 48000;
    std::array<dsp::DelayLine, lines> delay;
    std::array<float, lines> baseLength {}, feedback {};
    std::array<dsp::OnePole, lines> damping;
    std::array<dsp::OnePole, lines> lowCut;
    std::array<std::array<dsp::Allpass, 4>, 2> diffuser;
    std::array<dsp::DelayLine, 2> preDelay;
    dsp::Smoother size, decay, preDelaySamples, dampHz;
    std::array<float, lines> modPhase {}, modRate {};
    int diffusionStages = 4;
    bool modulate = true;
    float t60 = 2;
};

} // namespace wb
