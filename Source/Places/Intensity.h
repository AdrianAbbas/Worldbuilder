#pragma once
// Score Intensity (Milestone 7): one macro that takes every active Place from a calm
// bed (0) through the Place as set (0.5) to a climax (1). Each Place has its own
// response: how far Intensity moves its Activity, Tension, Density and Tone, and its
// level, on the way down and on the way up. At 0.5 nothing changes.
//
// Intensity only moves values towards their ends (down: towards none, up: towards
// full), so the response is monotonic whatever the Place's own settings are.
#include "Place.h"

namespace wb {

struct IntensityCurve {
    float activity = 0.9f, tension = 0.7f, density = 0.7f, tone = 0.6f;
    // The level at 0 and at 1. Going up, the extra notes already add 4-7 dB, so the lift
    // itself stays small (the climax keeps within about 6 dB of the Place as set).
    float quietDb = 7.0f, loudDb = 1.0f;
    float bloom = 1.0f;                    // > 1: the climax arrives late in the top half
};

inline const IntensityCurve& intensityCurve(int type) {
    // Rhythmic Places climb mostly in activity and density; ambient ones in tone and
    // tension (a pad does not get much busier, it gets brighter and darker in harmony)
    // and fall furthest when calm; the acoustic ensembles in between.
    constexpr IntensityCurve ensemble {}, rhythmic { 1.0f, 0.8f, 0.9f, 0.7f, 6.0f, 0.0f, 1.0f }, ambient { 0.7f, 0.6f, 0.5f, 0.7f, 8.0f, 0.0f, 1.3f };
    static const std::array<IntensityCurve, PlaceTypeCount> table {
        ensemble,   // empty
        ensemble,   // castle
        ensemble,   // village
        rhythmic,   // spaceport
        rhythmic,   // factory
        rhythmic,   // city
        ambient,    // forest
        ambient,    // alien hive
        ambient,    // temple
        ambient,    // cavern
        ambient,    // ruins
        rhythmic,   // spaceship
        ambient,    // biodome
        ensemble,   // sea ship
        ensemble,   // suburbs
        ensemble,   // mall
        rhythmic,   // military camp
        ensemble,   // beach
        ensemble,   // desert
        ensemble,   // palace
        rhythmic,   // tavern
        ensemble,   // space station
        rhythmic,   // cyberpunk megacity
        rhythmic,   // starship bridge
        rhythmic,   // asteroid mining colony
        ambient,    // alien ruins
        ambient,    // nebula
    };
    return table[std::size_t(std::clamp(type, 0, PlaceTypeCount - 1))];
}

// How far below (calm) and above (climax) the middle an Intensity is, each 0..1.
struct IntensityPosition {
    float below = 0, above = 0;
};
inline IntensityPosition intensityPosition(float intensity, const IntensityCurve& curve) {
    const float i = std::clamp(intensity, 0.0f, 1.0f);
    return { std::clamp(1.0f - 2.0f * i, 0.0f, 1.0f), std::pow(std::clamp(2.0f * i - 1.0f, 0.0f, 1.0f), curve.bloom) };
}
// A 0..1 value moved by Intensity: down towards a fraction of itself, up towards 1.
inline float intensify(float value, float weight, IntensityPosition at) {
    const float v = std::clamp(value, 0.0f, 1.0f);
    return std::clamp(v * (1.0f - 0.8f * weight * at.below) + (1.0f - v) * 0.85f * weight * at.above, 0.0f, 1.0f);
}
// The level change (dB) Intensity gives a Place.
inline float intensityDb(int type, float intensity) {
    const auto& curve = intensityCurve(type);
    const auto at = intensityPosition(intensity, curve);
    return -curve.quietDb * at.below + curve.loudDb * at.above;
}

} // namespace wb
