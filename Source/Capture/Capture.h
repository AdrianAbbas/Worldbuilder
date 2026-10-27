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
#include "../Locations/Travel.h"
#include "../Patterns/PatternMidi.h"
#include "../WorldEngine.h"
#include <atomic>
#include <memory>
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
    // Milestone 11c: the arrangement (no sections: none), and what its sections'
    // Locations need: the parameter values, the eight Locations, the Travel Time, and
    // how a world's values become engine controls (the processor's mapping).
    arr::Arrangement arrangement {};
    WorldValues values {};
    // (Shared: eight Locations with their patterns are about 2 MB, too much to copy on a stack.)
    std::shared_ptr<const std::array<LocationData, locationCount>> locations;
    TravelInputs travel {};
    std::function<EngineControls(const WorldValues&)> controlsOf;
};

// Samples in `bars` bars of 4/4 at the scene's tempo and rate.
int64_t lengthInSamples(const Scene& scene, int bars);

// One file to render: its name ("Mix", a Place's name, "Environment") and the controls
// that leave only its part audible.
struct Pass {
    juce::String name;
    EngineControls controls;
    bool stem = false;   // its mutes and solos isolate one part (kept through a journey)
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
    juce::String name;   // trackName()
    int slot = 0, element = 0;
    pat::Pattern pattern;
    pat::MidiTarget target;
};
// An instrument's track name, "Place - Instrument" ("Spaceport - Pads"): the name a single-
// instrument WORLDBUILDER (KEYS > SINGLE) shows, so each GarageBand track's region and
// instance match. A plain hyphen: a MIDI track name has no defined text encoding.
juce::String trackName(int type, int element);
// Every active instrument with a pattern: audible slots (mutes and solos as in the mix),
// switched on, not muted in the Pattern Editor, with at least one note. Generated
// patterns come from a fresh engine started at bar 1, so they are the ones a render
// (or Transport playback from the start) plays; edited patterns are used as edited.
std::vector<Track> sceneTracks(const Scene& scene);
// A type-1 MIDI file: a tempo track, then one named track per instrument with its
// pattern repeated over `bars` bars from bar 1 (960 ticks per beat).
juce::MidiFile sceneMidi(const std::vector<Track>& tracks, int bars, float bpm);

//==============================================================================
// Milestone 11c: the arrangement, game files and stingers.

// Renders `samples` from song position `startBeat` exactly as the host plays the
// arrangement there in Play Mode Arrangement (its Locations through the same journeys),
// with the pass's mutes and solos. The other arguments as render().
bool renderArranged(const Scene& scene, const arr::Arrangement& arrangement, const Pass& pass, double startBeat, int64_t samples, const Sink& sink,
                    std::atomic<bool>* cancel = nullptr, std::atomic<float>* progress = nullptr, int block = 512);
// Samples in a number of bars (fractions allowed).
int64_t samplesFor(const Scene& scene, double bars);
// The bars a render of the whole arrangement takes: every section once through (an
// endless one, one pass), and with End: stop the two bars of its fade-out.
double arrangementBars(const arr::Arrangement& a);

// A file for a game engine.
struct GameFile {
    enum Kind { Whole, Section, Loop, Stinger };
    int kind = Whole;
    int section = -1;
    juce::String name;   // "Tavern_Intro", "Tavern_Loop", "Tavern_Victory_Stinger", "Tavern_Arrangement"
};
// The scene's name for files: its first audible Place ("Tavern", "CyberpunkMegacity").
juce::String sceneName(const Scene& scene);
// Every section as its own file (a section that repeats or loops endlessly is a
// seamless Loop), or every section's stinger.
std::vector<GameFile> gameFiles(const Scene& scene, bool stingers);
// The bars a stinger takes from its section's end (its transition's length, 1-2 bars),
// and the ring-out after it.
constexpr double stingerRingBars = 3.0;
double stingerBars(const arr::Section& s);
// Renders one game file of one pass into `sink`:
// - Whole: the arrangement from bar 1 (arrangementBars);
// - Section: the section from its start, every pass, exactly its length (files chain
//   back to back; the last section with End: stop keeps its fade-out);
// - Loop: one pass, seamless: rendered as the loop's second pass (so its start carries
//   what rings over from its end), with the first quarter beat crossfaded from the
//   music that follows its end, so the end joins the start without a click;
// - Stinger: the section's last bars and its transition, then the ring-out.
// Returns the samples written through `sink` (-1: cancelled).
int64_t renderGameFile(const Scene& scene, const GameFile& file, const Pass& pass, const Sink& sink, std::atomic<bool>* cancel = nullptr,
                       std::atomic<float>* progress = nullptr);
// The same into a 24-bit WAV file (false when cancelled or not written).
bool renderGameFileToWav(const Scene& scene, const GameFile& game, const Pass& pass, const juce::File& file, std::atomic<bool>* cancel = nullptr,
                         std::atomic<float>* progress = nullptr);

// Scene MIDI that follows the arrangement: every instrument's notes where its sections
// play it (with their variants, Fills, entries and fades as velocity), over
// arrangementBars without the fade.
juce::MidiFile arrangementMidi(const Scene& scene, const std::vector<Track>& tracks);

} // namespace wb::capture
