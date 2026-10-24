#pragma once
// The interface: World Controls on the left; the globe with the Place selector cards
// in the middle (a Place's detail panel opens over them); the environment layers
// and the four Place strips on the right; key, quality, seed and output below.
// The full Living Globe and resizable layout arrive in Milestone 6.
#include "PluginProcessor.h"
#include "UI/PatternEditor.h"
#include "UI/WorldLookAndFeel.h"

class WorldbuilderEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit WorldbuilderEditor(WorldbuilderProcessor&);
    ~WorldbuilderEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

    static constexpr int baseWidth = 1160, baseHeight = 720;

    // For the tests: add a Place to the first empty slot (as a card click does) and
    // open a slot's detail panel.
    int addPlace(int type);
    void openDetail(int slot);
    // Opens the Pattern Editor on a slot's Place (on `element`, or its first pattern).
    void openPatterns(int slot, int element = -1);
    wb::ui::PatternEditor& patternEditor() { return *patterns; }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct LayerStrip {
        juce::Slider level;
        juce::ToggleButton mute { "M" }, solo { "S" };
        std::unique_ptr<SliderAttachment> levelAttachment;
        std::unique_ptr<ButtonAttachment> muteAttachment, soloAttachment;
        juce::Rectangle<int> row;
    };
    struct PlaceStrip {
        juce::Slider level, distance, pan;
        juce::ToggleButton mute { "M" }, solo { "S" };
        juce::TextButton edit { "EDIT" }, remove { "X" };
        std::unique_ptr<SliderAttachment> levelAttachment, distanceAttachment, panAttachment;
        std::unique_ptr<ButtonAttachment> muteAttachment, soloAttachment;
        juce::Rectangle<int> row;
        int shownType = -1;
    };
    struct Detail {
        int slot = -1, shownType = -1;
        std::array<juce::ToggleButton, 3> character;
        juce::Slider activity { "Activity" }, tension { "Tension" }, tone { "Tone" }, size { "Size" }, variation { "Variation" };
        juce::Slider density { "Density" }, swing { "Swing" }, seed;
        juce::ToggleButton sync { "SYNC TO HOST" };
        juce::TextButton dice { "New seed" }, close { "CLOSE" }, patterns { "PATTERNS" };
        std::array<juce::Slider, 5> element;
        std::array<juce::ToggleButton, 5> elementOn;
        std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
        std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
        juce::Rectangle<int> area, titleArea, characterArea, elementsArea, rhythmArea;
    };

    void timerCallback() override;
    juce::Slider& addKnob(int param);
    void setupRotary(juce::Slider&, const juce::String& tooltip);
    void paintGlobe(juce::Graphics&, juce::Rectangle<float> area);
    void paintCards(juce::Graphics&);
    void paintPlaceStrips(juce::Graphics&);
    void paintDetail(juce::Graphics&);
    void layoutDetail();
    void closeDetail();
    void updateDetailLabels();
    void refreshPlaceStrips();
    void setParam(int param, float value);
    float param(int param) const;
    int placeType(int slot) const;
    int cardAt(juce::Point<int>) const;
    juce::Point<float> landmarkPosition(int slot) const;
    int landmarkAt(juce::Point<float>) const;

    WorldbuilderProcessor& processor;
    wb::ui::WorldLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<SliderAttachment>> knobAttachments;
    std::array<juce::Slider*, wb::PCount> knobFor {};
    std::array<LayerStrip, wb::LayerCount> strips;
    std::array<PlaceStrip, wb::placeSlotCount> placeStrips;
    Detail detail;
    juce::ToggleButton power { "POWER" };
    std::unique_ptr<ButtonAttachment> powerAttachment;
    juce::ComboBox root, mood, quality;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rootAttachment, moodAttachment, qualityAttachment;
    juce::Slider seed;
    std::unique_ptr<SliderAttachment> seedAttachment;
    juce::TextButton dice { "New world" };
    std::unique_ptr<wb::ui::PatternEditor> patterns;
    juce::ComboBox playMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> playModeAttachment;

    juce::Rectangle<int> header, controlsPanel, globePanel, cardsPanel, layersPanel, placesPanel, footer;
    std::array<juce::Rectangle<int>, 4> groupAreas;
    std::array<juce::Rectangle<int>, 10> cards;
    juce::Rectangle<float> globeArea;
    float spin = 0, precipitation = 0, flash = 0;
    int boltSeed = 1, hoverCard = -1, dragLandmark = -1;
    double fullMessageUntil = 0;
    std::array<float, wb::LayerCount> meters {};
    std::array<float, wb::placeSlotCount> placeMeters {};
    float masterMeter = 0;
};
