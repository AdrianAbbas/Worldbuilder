#pragma once
#include "Core.h"
#include "../Parameters.h"
#include <array>

namespace wb {

// The world as the layers see it for one control chunk: smoothed World Controls,
// the discrete settings and the musical clock.
struct WorldFrame {
    std::array<float, WorldControlCount> c {};
    int root = 2, mood = 0, quality = 1;   // quality: 0 Eco, 1 Standard, 2 High
    uint32_t seed = 1;
    double beat = 0;    // the world's beat position at the chunk start (host tempo, or the default)
    float bpm = 96;
    int chord = 0;      // scale degree of the current chord of the shared progression
    float intensity = 0.5f;   // Score Intensity, smoothed (0 calm, 0.5 as set, 1 climax)
    float operator[](WorldControl w) const { return c[std::size_t(w)]; }
};

// One procedural sound layer. render() writes (does not add) n <= dsp::chunk
// samples per channel for the chunk that starts at absolute sample `clock`.
class Generator {
public:
    virtual ~Generator() = default;
    virtual void prepare(double sampleRate) = 0;
    virtual void reset() = 0;
    virtual void setSeed(uint32_t seed) = 0;
    // The host transport jumped to `clock`: re-derive any phase state from it.
    virtual void resync(uint64_t clock) { (void) clock; }
    virtual void render(const WorldFrame& world, uint64_t clock, float* left, float* right, int n) = 0;
    // How much of this layer feeds the World Space reverb (0..1) at the current settings.
    virtual float spaceSend(const WorldFrame& world) const { (void) world; return 0.5f; }
};

} // namespace wb
