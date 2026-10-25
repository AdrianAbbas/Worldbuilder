#pragma once
// Capture and export (Milestone 8): getting WORLDBUILDER's output into the song.
//
// A Scene is a snapshot of everything that makes the world sound: the parameter
// values, every slot's patterns, the host's tempo and sample rate. From it:
// - render() plays the world offline, faster than real time, from bar 1 exactly as the
//   host plays it in Transport mode (a fresh engine, restarted at song position 0), so
//   a render matches real-time playback from the start sample for sample;
// - passes() lists what to render: the full mix, or one stem per audible Place plus
//   the environment layers;
// - sceneTracks() and sceneMidi() write the patterns of every active instrument as a
//   multi-track MIDI file, one named track each, aligned to bar 1 at the scene's tempo.
//
// Nothing here touches the processor's own engine: renders run on their own engine
// (on a background thread from the editor), so the plugin keeps playing meanwhile.
#include "../Patterns/PatternMidi.h"
#include "../WorldEngine.h"
#include <atomic>
#include <functional>
#include <juce_core/juce_core.h>
#include <vector>

namespace wb::capture {

constexpr int beatsPerBar = 4;   // the patterns and the chord progression count in 4/4
inline constexpr std::array<int, 6> barChoices { 4, 8, 16, 24, 32, 64 };

struct Scene {
    EngineControls controls;   // gate open; hostBpm set to `bpm`
    std::array<pat::SlotPatterns, placeSlots> patterns {};
    double sampleRate = 48000;
    float bpm = WorldEngine::defaultBpm;
};

// Samples in `bars` bars of 4/4 at the scene's tempo and rate.
int64_t lengthInSamples(const Scene& scene, int bars);

// One file to render: its name ("Mix", a Place's name, "Environment") and the controls
// that leave only its part audible.
struct Pass {
    juce::String name;
    EngineControls controls;
};
// The mix alone, or stems: one per audible Place slot and one for the environment
// layers when any of them is audible. Mutes and solos are respected as in the mix.
std::vector<Pass> passes(const Scene& scene, bool stems);

// Renders `samples` of one pass from bar 1 in blocks of up to `block`, handing each to
// `sink(left, right, n)`. `sink` returning false cancels; so does `cancel`. `progress`
// (optional) runs from 0 to 1. Returns true when every sample was rendered.
using Sink = std::function<bool(const float* left, const float* right, int n)>;
bool render(const Scene& scene, const EngineControls& controls, int64_t samples, const Sink& sink, std::atomic<bool>* cancel = nullptr,
            std::atomic<float>* progress = nullptr, int block = 512);

// Renders one pass of `bars` bars into a 24-bit stereo WAV file at the scene's rate
// (replacing the file). False when cancelled or the file cannot be written (the
// partial file is deleted).
bool renderToWav(const Scene& scene, const Pass& pass, int bars, const juce::File& file, std::atomic<bool>* cancel = nullptr,
                 std::atomic<float>* progress = nullptr);

// One MIDI track: the instrument's name ("Castle - Bells"), the pattern it plays from
// bar 1, and how to write it out (key, kit or melodic).
struct Track {
    juce::String name;
    int slot = 0, element = 0;
    pat::Pattern pattern;
    pat::MidiTarget target;
};
// Every active instrument with a pattern: audible slots (mutes and solos as in the mix),
// switched on, not muted in the Pattern Editor, with at least one note. Generated
// patterns come from a fresh engine started at bar 1, so they are the ones a render
// (or Transport playback from the start) plays; edited patterns are used as edited.
std::vector<Track> sceneTracks(const Scene& scene);
// A type-1 MIDI file: a tempo track, then one named track per instrument with its
// pattern repeated over `bars` bars from bar 1 (960 ticks per beat).
juce::MidiFile sceneMidi(const std::vector<Track>& tracks, int bars, float bpm);

} // namespace wb::capture
