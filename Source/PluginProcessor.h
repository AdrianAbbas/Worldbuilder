#pragma once
#include "Parameters.h"
#include "Arrangement/ArrangementStore.h"
#include "Capture/Capture.h"
#include "Locations/LocationBank.h"
#include "Patterns/PatternStore.h"
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

private:
    wb::EngineControls readControls() const noexcept;
    wb::EngineControls controlsFor(const wb::WorldValues& values) const noexcept;
    wb::TravelInputs travelInputs(const wb::WorldValues& values) const noexcept;
    // A slot that holds another Place gets that Place's default patterns.
    void timerCallback() override {
        serviceLocations();
        patterns.syncTypes(slotTypes());
    }
    static constexpr int travelPiece = 128;   // samples per piece while a journey is under way
    uint32_t appliedArrival = 0;

    std::array<std::atomic<float>*, wb::PCount> raw {};
    double rate = 48000;
    int64_t expectedHostPosition = -1;
    bool hostWasPlaying = false;
    std::atomic<float> hostBpm { 0 };
    bool gateOpen = true;
    std::atomic<int> previewBar { 0 };
    std::atomic<uint32_t> previewSerial { 0 };
    std::atomic<bool> previewOn { false };
    uint32_t previewSeen = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WorldbuilderProcessor)
};
