#pragma once
#include "Parameters.h"
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

    // The Place type in each slot, from the parameters.
    std::array<int, wb::placeSlots> slotTypes() const;

private:
    wb::EngineControls readControls() const noexcept;
    // A slot that holds another Place gets that Place's default patterns.
    void timerCallback() override { patterns.syncTypes(slotTypes()); }

    std::array<std::atomic<float>*, wb::PCount> raw {};
    double rate = 48000;
    int64_t expectedHostPosition = -1;
    bool hostWasPlaying = false;
    float hostBpm = 0;
    bool gateOpen = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WorldbuilderProcessor)
};
