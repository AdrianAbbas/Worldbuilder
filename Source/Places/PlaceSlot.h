#pragma once
#include "Places.h"

namespace wb {

constexpr int placeSlots = 4;

// One slot's parameter values, as read from the host.
struct PlaceSlotControls {
    int type = NoPlace, character = 0;
    float levelDb = 0, distance = 0.35f, pan = 0;
    bool mute = false, solo = false, sync = true;
    float activity = 0.5f, tension = 0.3f, density = 0.5f, swing = 0.15f, tone = 0.5f, size = 0.5f, variation = 0.35f;
    uint32_t seed = 1;
    std::array<float, placeElements> element { 0.8f, 0.8f, 0.8f, 0.8f, 0.8f };
    std::array<bool, placeElements> elementOn { true, true, true, true, true };
};

// Hosts one Place: switches between Place engines with a short fade, folds the
// World Controls into the Place's settings, keeps the Place's beat clock (the host's
// or its own free tempo) and applies level, distance (quieter, darker, narrower,
// more reverberant) and pan.
class PlaceSlot {
public:
    void prepare(double sampleRate, int slotIndex);
    void reset();
    // Renders n <= dsp::chunk samples (writes). Returns false, writing nothing, when the
    // slot is silent.
    bool render(const WorldFrame& world, const PlaceSlotControls& controls, bool audible, uint64_t clock, double hostBeat, float hostBpm,
                float* left, float* right, int n);
    // World Space send for the last rendered chunk.
    float spaceSend() const { return send; }
    int playingType() const { return currentType; }
    // The Place playing in this slot (nullptr when empty), for the tests.
    const Place* place() const { return current; }

    // The settings the Place sees for a slot's controls in a world. Public for the tests.
    static PlaceSettings settingsFor(const WorldFrame& world, const PlaceSlotControls& controls);
    static float effectiveDistance(const WorldFrame& world, float distance);

private:
    Place* engineFor(int type);

    float rate = 48000;
    int slot = 0;
    CastlePlace castle;
    VillagePlace village;
    SpaceportPlace spaceport;
    Place* current = nullptr;
    int currentType = NoPlace;
    float fade = 0, fadeStep = 0.001f;
    double freeBeat = 0;
    uint32_t seedKey = 0;
    bool seeded = false;
    PlaceSettings settings;
    std::array<dsp::Smoother, 8> smooth;   // activity, tension, density, swing, tone, size, variation, fxDepth
    std::array<dsp::Smoother, placeElements> elementSmooth;
    dsp::Smoother gain, distance, pan;
    std::array<dsp::OnePole, 2> distanceLp, dcBlock;
    float send = 0;
    bool fresh = true;
};

} // namespace wb
