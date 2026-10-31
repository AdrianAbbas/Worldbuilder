#pragma once
#include "Parameters.h"
#include "Arrangement/ArrangementStore.h"
#include "Capture/Capture.h"
#include "Keys/Recorder.h"
#include "Locations/LocationBank.h"
#include "Patterns/PatternStore.h"
#include "Scenes/Scenes.h"
#include "WorldEngine.h"
#include <juce_audio_processors/juce_audio_processors.h>

class WorldbuilderProcessor final : public juce::AudioProcessor, private juce::Timer {
public:
    WorldbuilderProcessor();
    ~WorldbuilderProcessor() override { stopTimer(); }

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    // A continuous generator: hosts should keep calling it, so report a long tail.
    double getTailLengthSeconds() const override { return 20.0; }

    // One program, the current world. Locations and factory worlds come later; a host
    // calling setCurrentProgram(0) on load must never wipe the user's world.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Current World"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();

    juce::AudioProcessorValueTreeState parameters;
    wb::WorldEngine engine;
    // The composer's patterns (message thread), published to the engine.
    wb::PatternStore patterns { engine };
    std::atomic<float> cpuLoad { 0 };
    // The arrangement (Milestone 11), published to the engine.
    wb::ArrangementStore arrangement { engine.arrangementIn };

    // The ARRANGE view's preview: plays the arrangement from a bar on the plugin's own
    // clock (at the host tempo) while the host is stopped, in any Play Mode. The host
    // starting playback ends it.
    void previewFrom(int bar);
    void stopPreview();
    bool previewing() const { return previewOn.load(std::memory_order_relaxed); }
    // Whether the world follows the arrangement now (Play Mode Arrangement with the host
    // playing, or the preview).
    bool arranging() const { return engine.arrangeSection.load(std::memory_order_relaxed) != -1; }

    // A snapshot of the world as it sounds now, for offline renders and scene MIDI
    // (message thread): the parameters, the patterns, the host's tempo and sample rate.
    wb::capture::Scene captureScene();

    // The Place type in each slot, from the parameters.
    std::array<int, wb::placeSlots> slotTypes() const;

    // Locations and Journeys: the audio thread's journeys and the eight Location slots.
    wb::Travel travel;
    wb::LocationBank locations { travel };
    // Saves the world as it is set (parameters and patterns) into a Location slot.
    void saveLocation(int index);
    // Travels to a saved Location (sets the Destination; again if it already names it).
    void travelTo(int index);
    // Writes an arrived world into the parameters and patterns (the timer calls it; the
    // tests call it directly).
    void serviceLocations();
    // A .world file: the whole world (parameters, patterns, Locations) as text, and
    // loading one (Power, Master Level, Quality and Play Mode stay as they are).
    juce::String exportWorld();
    bool importWorld(const juce::String& text);
    // The host's values for every parameter, clamped.
    wb::WorldValues liveValues() const noexcept;

    // Scenes (Milestone 12): loads a whole scene (parameters, patterns, Locations and the
    // arrangement); the world before it can be brought back with undoScene(). The session
    // settings (Power, Master Level, Quality, Place Outputs) stay; Play Mode becomes
    // Arrangement unless it is Free.
    void loadScene(const wb::scenes::Scene& scene);
    bool loadFactoryScene(int index);
    // Inspire: moves the World Seed on and builds a random, musical scene from it.
    void inspire();
    bool undoScene();
    bool canUndoScene() const { return !sceneUndo.isEmpty(); }
    // The name of the scene loaded last (empty: none, or the world was edited since...
    // the editor only shows it).
    juce::String sceneName() const;
    int factoryIndex() const;   // -1: not a factory scene

    // The arrangement plays when the host plays (the fix after Milestone 13): the first
    // sections built switch Play Mode to Arrangement; a Play Mode chosen afterwards is
    // kept (the arrangement stays saved either way). The timer calls it; the tests too.
    void serviceArrangement();
    // Whether the host's playback follows the arrangement (it has sections and Play Mode
    // is Arrangement), and switching that on or off (off: back to the Play Mode before).
    bool arrangementOn() const;
    void setArrangementOn(bool on);

    // Milestone 16: section clips. What a section's instrument plays there, as a clip
    // (its own clip, or the variant it is linked to laid over the section; nullptr: no
    // Place or no pattern); making it unique (a copy of what it plays: nothing changes
    // until it is edited), linking it to its variant again, and clearing it (unique and
    // empty). Each undoable.
    wb::arr::ClipRef sectionClip(int section, int slot, int element);
    bool makeClipUnique(int section, int slot, int element);
    void relinkClip(int section, int slot, int element);
    void clearClip(int section, int slot, int element);

    // KEYS (Milestone 14): the Play target and REC. Set on the message thread, read by
    // the audio thread each block; saved with the project (REC's arm is not).
    struct KeysSettings {
        std::atomic<int> slot { -1 }, element { 0 }, scale { wb::keys::Chromatic }, octave { 0 };
        std::atomic<bool> monitor { true }, hostRegions { true }, armed { false }, countIn { false }, autoLength { false };
        std::atomic<bool> single { false };   // single-instrument mode: the instance plays only the target, from host MIDI
        std::atomic<int> mode { wb::keys::Replace }, variant { 0 }, quantise { wb::keys::QuantiseOff };
        std::atomic<float> strength { 1.0f };
        bool tipShown = false;         // the first-time tip about host regions
        bool regionsNotice = false;    // a take with the transport switched host regions off (the editor says so)
    };
    KeysSettings keys;
    wb::keys::Recorder recorder { patterns, engine.capture };
    // Takes what REC captured into the patterns (the timer and the editor call it; the tests too).
    void serviceKeys();
    // REC on or off for the Play target (never in single-instrument mode).
    void setArmed(bool on);

    // Single-instrument mode (the hotfix after Milestone 14), for multi-track MIDI in
    // GarageBand: a MIDI file cannot choose the instrument of a new GarageBand track, so
    // each track gets its own WORLDBUILDER that plays one instrument of the scene (the
    // Play target, e.g. "Spaceport - Pads") from the host's MIDI, through its effects
    // (its own, its Place's and the shared reverbs and delays). The environment, the other
    // Places and instruments, its own pattern and the arrangement are silent; host
    // regions always play it; REC is off. Saved with the project (and a GarageBand patch).
    void setSingle(bool on);
    bool single() const { return keys.single.load(); }
    // The instrument it plays, "Spaceport - Pads" (as the scene MIDI names its track);
    // empty when there is none.
    juce::String singleName() const;
    wb::keys::RecordSettings recordSettings() const;

    // The Pattern Editor's own transport (the hotfix after Milestone 14), so recording
    // works like a small DAW without touching the host: Play and Stop from a playhead,
    // a loop region, count-in and punch. It runs while the host is stopped, at the host
    // tempo, on the world's own clock (as the ARRANGE preview does); the host starting
    // playback ends it and the editor follows the host. Positions are beats from bar 1
    // (the song's beats: a pattern repeats from bar 1, so they are the pattern's too).
    enum TransportState { TransportStopped, TransportCountIn, TransportRolling };
    static constexpr double countInBeats = 4;   // one bar (4/4)
    struct EditorTransport {
        std::atomic<double> playhead { 0 };             // where Play starts
        std::atomic<bool> loop { true };
        std::atomic<double> loopStart { 0 }, loopEnd { 16 };
        std::atomic<bool> loopSet { false };            // dragged by the composer (else it follows the pattern's length)
        std::atomic<bool> punch { false };              // REC records only inside the punch region
        std::atomic<double> punchIn { 0 }, punchOut { 16 };
        std::atomic<double> length { 16 };              // the target pattern's length (punch repeats with it)
        // The audio thread's side: where the transport stands.
        std::atomic<int> state { TransportStopped };
        std::atomic<double> beat { 0 };                 // the beat playing now (rolling)
        std::atomic<float> countLeft { 0 };             // beats of count-in left
        // Requests from the editor (the serial says there is a new one).
        std::atomic<bool> wanted { false };
        std::atomic<uint32_t> serial { 0 };
    };
    EditorTransport transport;
    // Play from the playhead (inside the loop region when Loop is on; after a bar of
    // count-in when REC is armed with COUNT-IN). While rolling: plays on from the
    // playhead again. Does nothing while the host plays.
    void transportPlay();
    // Stops, and REC with it (the take ends). Stopped already: the playhead goes back to
    // the loop's start (or bar 1).
    void transportStop();
    // Moves the playhead; while rolling the transport plays on from there.
    void transportLocate(double beat);
    int transportState() const { return transport.state.load(std::memory_order_relaxed); }
    // Whether the host's transport is playing (as the last block said).
    bool hostRolling() const { return hostRollingOut.load(std::memory_order_relaxed); }

private:
    wb::EngineControls readControls() const noexcept;
    wb::EngineControls controlsFor(const wb::WorldValues& values) const noexcept;
public:
    // The engine controls a world's parameter values make (no tempo or transport).
    static wb::EngineControls controlsFromValues(const wb::WorldValues& values) noexcept;
private:
    wb::TravelInputs travelInputs(const wb::WorldValues& values) const noexcept;
    // A slot that holds another Place gets that Place's default patterns.
    void timerCallback() override {
        serviceLocations();
        patterns.syncTypes(slotTypes());
        serviceArrangement();
        serviceKeys();
    }
    int arrangementSeen = 0;   // sections when last looked at (0 -> some: Play Mode Arrangement)
    float modeBeforeArrangement = float(wb::PlayTransport);
    static constexpr int travelPiece = 128;   // samples per piece while a journey is under way
    uint32_t appliedArrival = 0;
    juce::String sceneUndo;   // the world before the last scene load or Inspire (a .world text)
    void setValue(int param, float value);

    std::array<std::atomic<float>*, wb::PCount> raw {};
    double rate = 48000;
    int64_t expectedHostPosition = -1;
    bool hostWasPlaying = false, hostPlayingNow = false;
    std::array<wb::keys::Event, 512> keyEvents {};
    std::atomic<float> hostBpm { 0 };
    bool gateOpen = true;
    std::atomic<int> previewBar { 0 };
    std::atomic<uint32_t> previewSerial { 0 };
    std::atomic<bool> previewOn { false };
    uint32_t previewSeen = 0;
    // The editor's transport on the audio thread: its state, the count-in, and where a
    // take started (a loop wraps back to its start).
    int txState = TransportStopped;
    uint32_t txSeen = 0;
    double txFrom = 0, txCountBeat = 0;
    wb::keys::Click countClick;
    std::array<float, 256> clickScratch {}, clickScratchR {};
    std::atomic<bool> hostRollingOut { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WorldbuilderProcessor)
};
