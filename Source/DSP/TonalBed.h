#pragma once
#include "Generator.h"

namespace wb {

// Root note frequency and chord tones for Root and Mood. Magic and Machinery tonal
// elements (later milestones) use the same tables.
struct Harmony {
    static float rootHz(int root);   // the root in octave 3 (D3 = 146.8 Hz)
    // Semitone offsets from the root for the bed voices: sub, root, fifth, third, colour.
    static std::array<float, 5> bedIntervals(int mood);
};

// A quiet harmonic drone in the song's key. Each voice is a pair of slightly
// detuned additive oscillators that breathe slowly; root or mood changes glide.
// Oscillator phases run on across transport jumps (resetting a drone's phase would
// click), so the bed repeats exactly from a fresh instance or a reset.
class TonalBed final : public Generator {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setSeed(uint32_t seed) override;
    void render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) override;
    float spaceSend(const WorldFrame& world) const override;

    static constexpr int voices = 7;      // five chord voices, a Danger voice and a Magic halo
    static constexpr int maxHarmonics = 9;

private:
    struct Osc {
        dsp::Phasor phasor;
        float gainL = 0.7071f, gainR = 0.7071f;
    };
    struct Voice {
        std::array<Osc, 2> osc;
        dsp::Smoother semitones;   // glides between roots and moods (control rate)
        dsp::Smoother gain;        // audio rate
        dsp::Stream breath;
        float hz = 100, target = 0;
        bool initialised = false;
    };

    float rate = 48000;
    std::array<Voice, voices> voice;
    std::array<float, maxHarmonics> harmonicGain {};
    std::array<dsp::Svf, 2> tone;
    dsp::Stream haze;
    dsp::Smoother brightness;
    int lastRoot = -1, lastMood = -1;
};

} // namespace wb
