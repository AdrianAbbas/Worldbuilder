#pragma once
// The developer's recorded instruments (Milestone 15): the sample bank.
//
// tools/sample-prep/prepare.py turns the recordings into mono FLAC notes and one mapping
// file per instrument (key ranges, velocity layers, round-robins, loop and release
// points). They ship inside the plugin bundle (Contents/Resources/Samples), where
// GarageBand's sandbox lets the plugin read them.
//
// One bank per process, shared by every plugin instance. It loads on a background
// thread when the first instance is created (never on the audio thread); prepareToPlay
// waits for it briefly so the first notes are already sampled. Once Ready, nothing in
// it changes: the audio thread reads it without locks. Notes are kept as 16-bit
// integers (the recordings are MP3s, so 16 bits lose nothing) to halve the memory.
#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

namespace wb::samples {

// One recorded note.
struct Zone {
    const int16_t* data = nullptr;   // 2 samples of silence before it and 4 after
    int length = 0;
    float root = 60;                 // its pitch (MIDI, as detected, so in tune)
    int loKey = 0, hiKey = 127;      // the keys it plays
    float loVel = 0, hiVel = 1;      // the velocities it plays
    int layer = 0, rr = 0;
    int loopStart = -1, loopEnd = -1;   // a sustain loop (end exclusive), -1: none
    int release = -1;                // where the recorded release tail starts, -1: none
    float rmsDb = -20;               // level of its first 300 ms
    float loudnessDb = -20;          // how loud it sounds (K-weighted, its loudest 400 ms; rmsDb in older maps)
    bool loops() const { return loopEnd > loopStart && loopStart >= 0; }
};

// One recorded instrument (or articulation): its notes and how they map.
class Instrument {
public:
    juce::String id, name;
    float rate = 44100;
    int layers = 1;
    std::vector<Zone> zones;
    int lowKey = 0, highKey = 127;   // the keys the zones cover
    float lowRoot = 60, highRoot = 60;
    // The median loudness of each velocity layer's notes: notes are evened out within
    // their own layer, so the layers keep their steps.
    std::array<float, 8> layerMedianDb {};
    float medianDb(int layer) const { return layerMedianDb[std::size_t(std::clamp(layer, 0, int(layerMedianDb.size()) - 1))]; }

    // The zone for a key and velocity (0..1); `pick` (0..1) chooses among the
    // round-robin takes. Keys outside the instrument fold in by octaves.
    const Zone* find(int key, float velocity, float pick) const;
    int foldKey(int key) const;
    // Built once after loading.
    void index();

private:
    // Per key (0..127) and layer: the first zone and how many takes.
    struct Slot { int16_t first = -1, count = 0; };
    std::vector<Slot> table;   // 128 * layers
    std::vector<int> order;    // zone indices grouped per (key, layer)
};

class Bank {
public:
    enum State { Idle, Loading, Ready, Missing, Failed };

    static Bank& get();
    ~Bank();

    // Starts loading on a background thread (once; later calls do nothing).
    void load();
    // Waits up to `ms` for loading to finish; true when Ready.
    bool waitUntilLoaded(int ms) const;
    State state() const { return State(status.load(std::memory_order_acquire)); }
    bool ready() const { return state() == Ready && !bypassed.load(std::memory_order_relaxed); }
    // Tests: new parts act as if there were no samples (their synthesised fallbacks play).
    static void bypassForTests(bool on) { bypassed = on; }
    // nullptr unless Ready and the instrument exists. Safe on the audio thread.
    const Instrument* instrument(const char* id) const;

    // Where the samples are looked for (a test or developer folder first).
    static void setFolder(const juce::File& folder);
    juce::File folder() const { return from; }
    juce::String message() const;
    std::size_t memoryBytes() const { return bytes.load(); }
    double loadSeconds() const { return seconds.load(); }
    int zoneCount() const;

private:
    Bank() = default;
    void run();
    bool loadInstrument(const juce::File& map, Instrument& into, std::vector<int16_t>& pool);
    static juce::File locate();

    std::vector<std::unique_ptr<Instrument>> instruments;
    std::vector<std::vector<int16_t>> pools;   // one per instrument, never resized after loading
    std::thread thread;
    std::atomic<int> status { Idle };
    std::atomic<bool> started { false }, cancel { false };
    std::atomic<std::size_t> bytes { 0 };
    std::atomic<double> seconds { 0 };
    juce::File from;
    juce::String error;
    static inline std::atomic<bool> bypassed { false };
};

} // namespace wb::samples
