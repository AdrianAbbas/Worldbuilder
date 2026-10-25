#pragma once
// The interface: World Controls on the left; the globe with the Place selector cards
// in the middle (a Place's detail panel opens over them); the environment layers
// and the four Place strips on the right; key, quality, seed and output below.
// The full Living Globe and resizable layout arrive in Milestone 6.
#include "PluginProcessor.h"
#include "UI/CapturePanel.h"
#include "UI/PatternEditor.h"
#include "UI/PlaceArt.h"
#include "UI/PlaceOverlays.h"
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
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int baseWidth = 1160, baseHeight = 720;

    // For the tests: add a Place to the first empty slot (as a card click does) and
    // open a slot's detail panel.
    int addPlace(int type);
    void openDetail(int slot);
    // Opens the Pattern Editor on a slot's Place (on `element`, or its first pattern).
    void openPatterns(int slot, int element = -1);
    wb::ui::PatternEditor& patternEditor() { return *patterns; }
    wb::ui::CapturePanel& capturePanel() { return *capture; }
    // Runs the animation (theme, overlays) on by `seconds`, as the timer does.
    void settle(float seconds);
    wb::ui::PlaceOverlays& placeOverlays() { return *overlays; }
    void setOverlayMode(int mode);
    // Lists one category of Place cards (-1: all), as its chip does; how many are listed.
    void showCategory(int c);
    int listedPlaces() const { return listedCount; }

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
        juce::TextButton dice { "New seed" }, close { "CLOSE" }, patterns { "PATTERNS" }, mood;
        std::array<juce::Slider, wb::placeElements> element;
        std::array<juce::ToggleButton, wb::placeElements> elementOn;
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
    // The selector: the Places listed in the chosen category, their card bounds (with
    // the scroll), the category chips.
    void listCards();
    juce::Rectangle<int> cardBounds(int index) const;
    float maxCardScroll() const;
    int categoryAt(juce::Point<int>) const;
    // One animation frame: theme, overlays, globe, meters.
    void stepAnimation(float seconds);
    // The interface takes on the colours of the active Places (blended, ~1 s); `snap`
    // jumps straight there (when the editor opens).
    void updateTheme(float seconds, bool snap = false);
    // Tells the overlays where the panels are and what they must keep clear.
    void updateOverlayFrames();
    void paintGlobeFeatures(juce::Graphics&, juce::Rectangle<float> area, float alpha);
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
    std::unique_ptr<wb::ui::CapturePanel> capture;
    juce::ComboBox playMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> playModeAttachment;
    juce::ComboBox overlayChoice;
    std::unique_ptr<wb::ui::PlaceOverlays> overlays;
    std::vector<juce::Rectangle<int>> overlayKeepClear;

    juce::Rectangle<int> header, controlsPanel, globePanel, cardsPanel, layersPanel, placesPanel, footer;
    std::array<juce::Rectangle<int>, 4> groupAreas;
    juce::Rectangle<int> cardGrid, headerTitle, headerSubtitle, headerOpen, intensityLabel;
    struct FrameOutline {
        juce::Rectangle<int> panel;
        juce::Path glow, edge;
    };
    std::vector<FrameOutline> frameOutlines;   // each panel's outlines, stroked once
    std::array<juce::Rectangle<int>, wb::PlaceCategoryCount + 1> categoryChips;
    std::array<int, wb::PlaceTypeCount> listed {};
    int listedCount = 0, category = -1;   // -1: every Place
    float cardScroll = 0, animTime = 0;
    std::array<float, wb::PlaceTypeCount> hoverAmount {};
    // How present each Place is in the look (0..1, follows its level, fades in and out).
    std::array<float, wb::PlaceTypeCount> presence {};
    juce::Colour themeTint, themeGlow, themeSky;
    float themeStrength = 0;
    bool themeMoving = false;
    std::array<float, wb::ui::GlobeFeatureCount> featureAmount {};
    std::array<float, 8> particleAmount {};
    juce::Rectangle<float> globeArea;
    float spin = 0, precipitation = 0, flash = 0;
    int boltSeed = 1, hoverCard = -1, dragLandmark = -1;
    double fullMessageUntil = 0;
    std::array<float, wb::LayerCount> meters {};
    std::array<float, wb::placeSlotCount> placeMeters {};
    float masterMeter = 0;
};
