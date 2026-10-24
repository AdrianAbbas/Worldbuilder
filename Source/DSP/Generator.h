#pragma once
#include "Core.h"
#include "../Parameters.h"
#include <array>

namespace wb {

// The world as the layers see it for one control chunk: smoothed World Controls
// plus the discrete settings.
struct WorldFrame {
    std::array<float, WorldControlCount> c {};
    int root = 2, mood = 0, quality = 1;   // quality: 0 Eco, 1 Standard, 2 High
    uint32_t seed = 1;
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
