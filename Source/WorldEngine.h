#pragma once
// Everything that makes sound, independent of JUCE: the layers, their mixer, the
// World Space and the master section. The processor feeds it parameter values and
// the clock; tests can drive it through the processor's processBlock.
#include "Arrangement/Arrangement.h"
#include "Arrangement/Templates.h"
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
    // The Drama tier (after the performance pass): 0 Classic (as before), up to 1 the full
    // dramatic range at the top of Score Intensity; its style (drama::Style).
    float drama = 0;
    int dramaStyle = 0;
    // The Drama design pass: the project's Drama setting (Off: the whole Drama system off, the
    // world plays as before the Drama patches: no Drama layer, no shaping, no make-up).
    bool dramaSystem = true;
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
    // Instrument mode: slot 0 plays this instrument from the keys (which then target it,
    // single), like a normal software instrument: no world shaping it, no patterns, no
    // arrangement; the environment and the World Space only with `ambience`.
    struct Instrument {
        bool on = false, ambience = false;
        int type = 0, element = 0;   // type 0: none
        float attack = 0.5f, release = 0.5f;
        int drone = DroneRoot;   // the instrument's built-in drone under the played notes
    } instrument;
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
    // The beat of the next sample out (the FIFO holds a few samples rendered ahead).
    double outputBeat() const { return beat - double(dsp::chunk - fifoRead) * double(bpm) / 60.0 / rate; }
    void process(float* left, float* right, int n, const EngineControls& controls);
    // As process(), also writing each slot's own output: slotOut[2 * s] and [2 * s + 1]
    // (nullptr: that slot has none). A Place's output carries its dry sound (its own
    // instruments' character effects, level, distance and pan) after the master level
    // and fades, through its own safety limiter.
    void process(float* left, float* right, int n, const EngineControls& controls, float* const* slotOut);

    // The Drama loudness pass: how much the master's and the slot outputs' limiters (and
    // each slot's Drama layer limiter) have turned the sound down since the last clear
    // (tests and bench; the sound is never changed by asking).
    const dsp::Reduction& masterReduction() const { return limiter.reduction; }
    const dsp::Reduction& slotOutReduction(int slot) const { return slotLimiter[std::size_t(std::clamp(slot, 0, placeSlots - 1))].reduction; }
    void clearReductions();
    // The arrangement section's balance now (dB, the Drama loudness pass; for the tests).
    float sectionBalanceNow() const { return sectionBalance; }
    // Tests only (the loudness probe): no glue and no limiter, so renders subtract exactly.
    static inline bool probeLinear = false;
    // Tests only: the arrangement's section balance off (the probe measures what it corrects).
    static inline bool probeNoSectionBalance = false;

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
    // Set while the ARRANGE view is open: every slot generates every variant (its clips
    // are made from them).
    std::atomic<bool> arrangeVariants { false };
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
    // Instrument mode: the instrument playing now (after a change's fade), for the tests
    // and the editor.
    int instrumentType() const { return instType; }
    int instrumentElement() const { return instElement; }

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
    void shapeArrangement(const arr::Arrangement& a, const EngineControls& controls);
    // The Drama loudness pass: per slot, the arrangement's reshaping by drama balanced back
    // (dB; 0 at Drama 0), and the transitions' own sounds following the slots' balance.
    std::array<float, placeSlots> shapeDb {};
    float transitionBalance = 1;
    // The orchestration pass: per slot, what drama's reshaping adds, taken from the Drama layer (dB, <= 0).
    std::array<float, placeSlots> layerShapeDb {};
    std::array<std::array<uint8_t, slotElements>, placeSlots> writtenVariant {};   // the variants before drama shaped them
    uint64_t balanceKey = 0;
    float sectionBalance = 0, nextBalance = 0;
    double placePower(const arr::State& st, int slot, const EngineControls& controls, bool full = false) const;
    float shapeBalanceDb(const arr::State& shaped, const arr::State& written, int slot, const EngineControls& controls) const;
    float sectionBalanceDb(const arr::Arrangement& a, int section, const arr::Shaping& shaping, const EngineControls& controls) const;
    // The world's drama tier for the shared harmony, latched on the four-bar lines.
    int harmony = 0;
    int64_t harmonyLine = INT64_MIN;
    uint64_t clock = 0;   // absolute position of the next chunk to render
    uint64_t stream = 0;  // samples ever rendered into the FIFO (the keyboard's timeline; never jumps)
    keys::Live live;
    keys::Click click;
    bool keysOnly = false, gateNow = true;
    // Instrument mode: the instrument slot 0 plays now (type 0: none yet), the fade while
    // it changes (out over 12 ms, the new one in at once with the keys still held), and
    // whether the held keys start again on the new one.
    int instType = 0, instElement = -1;
    float instDuck = 1, instDown = 0.002f;
    bool instRetrigger = false, instWas = false;
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
