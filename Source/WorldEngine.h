#pragma once
// Everything that makes sound, independent of JUCE: the layers, their mixer, the
// World Space and the master section. The processor feeds it parameter values and
// the clock; tests can drive it through the processor's processBlock.
#include "DSP/AirLayer.h"
#include "DSP/EarthLayer.h"
#include "DSP/FireLayer.h"
#include "DSP/Master.h"
#include "DSP/ThunderLayer.h"
#include "DSP/TonalBed.h"
#include "DSP/WaterLayer.h"
#include "DSP/WorldSpace.h"
#include "Places/PlaceSlot.h"
#include <atomic>
#include <memory>

namespace wb {

struct EngineControls {
    std::array<float, WorldControlCount> world {};
    std::array<float, LayerCount> layerDb {};
    std::array<bool, LayerCount> mute {}, solo {};
    float masterDb = 0, width = 1, spaceMix = 0.4f;
    bool power = true;
    int root = 2, mood = 0, quality = 1;
    uint32_t seed = 1;
    float hostBpm = 0;   // 0 when the host gives no tempo: the world uses defaultBpm
    bool gate = true;    // Play Mode Transport: false while the host is stopped (the world fades out)
    std::array<PlaceSlotControls, placeSlots> places {};
};

class WorldEngine {
public:
    WorldEngine();
    void prepare(double sampleRate, const EngineControls& initial);
    void reset(const EngineControls& initial);
    // The host transport started or jumped: the world continues from `position`
    // (absolute samples) at beat `ppq` (negative: unknown, derived from the position).
    // Without a transport the world runs on its own clock.
    void resync(uint64_t position, double ppq = -1);
    // Play Mode Transport: the host started (or located) at `position` / `ppq`. The world
    // starts again from silence, from a fixed state, so the same song position always
    // gives the same sound (bounces match playback).
    void restart(uint64_t position, double ppq, const EngineControls& controls);

    static constexpr float defaultBpm = 96;
    void process(float* left, float* right, int n, const EngineControls& controls);

    // For the editor: per-layer and master peak meters, the smoothed world, power fade.
    std::array<std::atomic<float>, LayerCount> layerMeter {};
    std::array<std::atomic<float>, placeSlots> placeMeter {};
    std::atomic<float> masterMeter { 0 }, powerLevel { 1 };
    // The latest lightning flash (0..1), for the globe.
    std::atomic<float> lightning { 0 };
    const WorldFrame& frame() const { return world; }
    // For the tests: the strike log of the thunder layer (audio thread state).
    const ThunderLayer& thunderLayer() const { return thunder; }
    const PlaceSlot& placeSlot(int i) const { return slots[std::size_t(std::clamp(i, 0, placeSlots - 1))]; }

    // Patterns, lock-free both ways: the editor publishes each slot's patterns into
    // patternsIn (the audio thread picks up the newest at the next chunk); the slots
    // report the patterns their Places generate into patternsOut.
    std::array<pat::TripleBuffer<pat::SlotPatterns>, placeSlots> patternsIn;
    std::array<pat::TripleBuffer<pat::Generated>, placeSlots> patternsOut;
    // Each slot's beat (for the playhead) and whether the host transport is rolling.
    std::array<std::atomic<double>, placeSlots> slotBeat {};
    std::atomic<bool> transportRolling { false };

private:
    void renderChunk(float* left, float* right, int n, uint64_t clock, const EngineControls& controls);

    double rate = 48000;
    AirLayer air;
    WaterLayer water;
    FireLayer fire;
    EarthLayer earth;
    ThunderLayer thunder;
    TonalBed bed;
    std::array<Generator*, LayerCount> layers {};
    std::array<PlaceSlot, placeSlots> slots;
    std::array<float, placeSlots> placePeak {};
    double beat = 0;
    float bpm = defaultBpm;
    WorldFrame world;
    std::array<dsp::Smoother, WorldControlCount> worldSmooth;
    std::array<dsp::Smoother, LayerCount> layerGain;
    std::array<float, LayerCount> layerPeak {};
    WorldSpace space;
    std::array<dsp::OnePole, 2> distance;
    dsp::Glue glue;
    dsp::SafetyLimiter limiter;
    dsp::Smoother master, width, spaceMix;
    float powerRamp = 1, powerStep = 0;
    float gateRamp = 1, gateUp = 0, gateDown = 0;
    float masterPeak = 0;
    uint32_t seed = 0;
    uint64_t clock = 0;   // absolute position of the next chunk to render
    int fifoRead = dsp::chunk;
    std::array<float, dsp::chunk> fifoL {}, fifoR {};

    // Chunk scratch: one layer's output, the dry bus and the reverb send bus.
    std::array<float, dsp::chunk> layerL {}, layerR {}, dryL {}, dryR {}, sendL {}, sendR {};
};

} // namespace wb
