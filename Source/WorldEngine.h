#pragma once
// Everything that makes sound, independent of JUCE: the layers, their mixer, the
// World Space and the master section. The processor feeds it parameter values and
// the clock; tests can drive it through the processor's processBlock.
#include "Arrangement/Arrangement.h"
#include "DSP/AirLayer.h"
#include "DSP/EarthLayer.h"
#include "DSP/FireLayer.h"
#include "DSP/Master.h"
#include "DSP/Planet.h"
#include "DSP/ThunderLayer.h"
#include "DSP/Transitions.h"
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
    float environmentDb = 0;   // the environment layers' master level
    float intensity = 0.5f;   // Score Intensity
    bool power = true;
    int root = 2, mood = 0, quality = 1;
    int planet = 0;                 // dsp::PlanetType
    float planetInfluence = 0.35f;  // how strongly the Planet colours the world
    uint32_t seed = 1;
    float hostBpm = 0;   // 0 when the host gives no tempo: the world uses defaultBpm
    bool gate = true;    // Play Mode Transport: false while the host is stopped (the world fades out)
    std::array<PlaceSlotControls, placeSlots> places {};
    // Each slot's patterns during a journey between Locations (nullptr: the editor's,
    // from patternsIn).
    std::array<const pat::SlotPatterns*, placeSlots> patterns {};
    // Play Mode Arrangement, or the ARRANGE preview: the arrangement the world follows
    // (nullptr, or no sections: none). Its Score Intensity is the section's, plus the
    // Score Intensity parameter's offset from its middle (automation adds on top).
    const arr::Arrangement* arrangement = nullptr;
    // Separate outputs (Milestone 12): the slots whose Place also plays on its own output
    // (the host has that output bus), and Split: such a Place leaves the main mix (its
    // reverb and delay sends stay there, the shared effects are returns on the main).
    std::array<bool, placeSlots> slotOut {};
    bool splitOuts = false;
    // KEYS (Milestone 14): the Play target and its settings.
    keys::Controls keys;
};

// Each environment layer's balance trim (dB): at equal settings the layers sit about
// 10-15 dB under a typical Place, as background (the GarageBand fix round; measured by
// the `envbalance` tests).
inline constexpr std::array<float, LayerCount> environmentTrimDb { -21.3f, -21.0f, -13.1f, -15.3f, -15.9f, -14.3f };

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
    // The beat the next chunk starts on (audio thread).
    double nextBeat() const { return beat; }
    void process(float* left, float* right, int n, const EngineControls& controls);
    // As process(), also writing each slot's own output: slotOut[2 * s] and [2 * s + 1]
    // (nullptr: that slot has none). A Place's output carries its dry sound (its own
    // instruments' character effects, level, distance and pan) after the master level
    // and fades, through its own safety limiter.
    void process(float* left, float* right, int n, const EngineControls& controls, float* const* slotOut);

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
    const dsp::SharedFx& shared() const { return sharedFx; }
    const dsp::PlanetColour& planetColour() const { return planet; }

    // Patterns, lock-free both ways: the editor publishes each slot's patterns into
    // patternsIn (the audio thread picks up the newest at the next chunk); the slots
    // report the patterns their Places generate into patternsOut.
    std::array<pat::TripleBuffer<pat::SlotPatterns>, placeSlots> patternsIn;
    std::array<pat::TripleBuffer<pat::Generated>, placeSlots> patternsOut;
    // Each slot's beat (for the playhead) and whether the host transport is rolling.
    std::array<std::atomic<double>, placeSlots> slotBeat {};
    std::atomic<bool> transportRolling { false };

    // The arrangement, published by the editor (lock-free), and where it stands (for the
    // ARRANGE view's playhead): the section (-1: not arranging) and the timeline bar.
    pat::TripleBuffer<arr::Arrangement> arrangementIn;
    std::atomic<int> arrangeSection { -1 };
    std::atomic<double> arrangeBar { 0 };
    // Set while the Pattern Editor shows variants: every variant is generated.
    std::array<std::atomic<bool>, placeSlots> showVariants {};
    // Whether a Place's instrument is a rhythm part (a Drum fill transition plays its Fill).
    static bool isRhythm(int type, int element);
    // For the tests: the arrangement state of the last chunk.
    const arr::State& arrangeState() const { return arranged; }

    // KEYS (Milestone 14). The host's MIDI for this block (offsets in samples from the
    // block's first sample), before process(): the events land on their samples.
    void keysBlock(const keys::Event* events, int count, const EngineControls& controls);
    keys::Input keysInput;
    keys::Capture capture;
    // Whether the keyboard plays alone now (the host stopped in Transport: the world is
    // silent but the instrument played sounds), for the tests and the editor.
    bool keysAlone() const { return keysOnly; }
    // An instrument's pitch-bend range (semitones): a tone, an octave for sirens, risers
    // and gliding leads, none for a kit.
    static float bendRange(int type, int element);

private:
    // Fills the FIFO with one chunk; with an arrangement the chunk is split at beat
    // lines, so sections, entrances, variants and Fills change on the exact sample.
    void fillChunk(const EngineControls& controls);
    void renderChunk(float* left, float* right, int n, uint64_t clock, const EngineControls& controls, uint64_t at);
    // The keyboard's events for a piece starting at stream position `at`: the live notes
    // for the target and what a take records.
    void keysPiece(int n, uint64_t at, const EngineControls& controls);
    // The slot outputs of the chunk just rendered (at `offset` in the FIFO).
    void renderSlotOuts(int offset, int n, const EngineControls& controls);

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
    // The shared reverbs and delays every instrument sends to.
    dsp::SharedFx sharedFx;
    std::array<dsp::OnePole, 2> distance;
    dsp::PlanetColour planet;
    dsp::Glue glue;
    dsp::SafetyLimiter limiter;
    dsp::Smoother master, width, spaceMix, intensity;
    float powerRamp = 1, powerStep = 0;
    float gateRamp = 1, gateUp = 0, gateDown = 0;
    float masterPeak = 0;
    uint32_t seed = 0;
    float seedGain = 1, seedStep = 0;
    bool seedFresh = true;
    arr::State arranged;
    bool arranging = false;
    // The transitions' sounds and the Filter sweep on the master.
    dsp::TransitionVoice transitionVoice;
    std::array<dsp::Svf, 2> sweep;
    bool sweeping = false;
    float duckGain = 1;
    float endGain = 1;
    float arrangedIntensity(const EngineControls& controls) const;
    uint64_t clock = 0;   // absolute position of the next chunk to render
    uint64_t stream = 0;  // samples ever rendered into the FIFO (the keyboard's timeline; never jumps)
    keys::Live live;
    keys::Click click;
    bool keysOnly = false, gateNow = true;
    int keysSlot = -1, keysElementNow = 0;
    double clickBeat = 0;
    int keysAwake = 0;    // samples the keyboard keeps a stopped world awake for
    int fifoRead = dsp::chunk;
    std::array<float, dsp::chunk> fifoL {}, fifoR {};

    // Chunk scratch: one layer's output, the dry bus and the reverb send bus.
    std::array<float, dsp::chunk> layerL {}, layerR {}, dryL {}, dryR {}, sendL {}, sendR {};
    // Separate outputs: each slot's dry sound this chunk, the master gain and fades per
    // sample (applied to the slot outputs as to the main), the slots' FIFOs and limiters.
    std::array<std::array<float, dsp::chunk>, 2 * placeSlots> slotDry {};
    std::array<bool, placeSlots> slotSounding {};
    std::array<float, dsp::chunk> outGain {};
    std::array<std::array<float, dsp::chunk>, 2 * placeSlots> slotFifo {};
    std::array<dsp::SafetyLimiter, placeSlots> slotLimiter;
    bool slotOutsWanted = false;
};

} // namespace wb
