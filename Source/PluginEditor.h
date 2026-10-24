#pragma once
// Milestone 1 placeholder interface: every control is real and custom drawn, but the
// layout is simple. The Living Globe, structure palette and Location slots arrive
// in Milestone 6.
#include "PluginProcessor.h"
#include "UI/WorldLookAndFeel.h"

class WorldbuilderEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit WorldbuilderEditor(WorldbuilderProcessor&);
    ~WorldbuilderEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int baseWidth = 1000, baseHeight = 620;

private:
    struct LayerStrip {
        juce::Slider level;
        juce::ToggleButton mute { "M" }, solo { "S" };
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment, soloAttachment;
        juce::Rectangle<int> row;
    };

    void timerCallback() override;
    juce::Slider& addKnob(int param);
    void paintGlobe(juce::Graphics&, juce::Rectangle<float> area);

    WorldbuilderProcessor& processor;
    wb::ui::WorldLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> knobAttachments;
    std::array<juce::Slider*, wb::PCount> knobFor {};
    std::array<LayerStrip, wb::LayerCount> strips;
    juce::ToggleButton power { "POWER" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    juce::ComboBox root, mood, quality;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttachment, moodAttachment, qualityAttachment;
    juce::Slider seed;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> seedAttachment;
    juce::TextButton dice { "New world" };

    juce::Rectangle<int> header, controlsPanel, globePanel, mixerPanel, footer;
    std::array<juce::Rectangle<int>, 4> groupAreas;
    juce::Rectangle<float> globeArea;
    float spin = 0;
    std::array<float, wb::LayerCount> meters {};
    float masterMeter = 0;
};
