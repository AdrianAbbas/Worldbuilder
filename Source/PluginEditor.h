#pragma once
// The interface: World Controls on the left; the globe with the Place selector cards
// in the middle (a Place's detail panel opens over them); the environment layers
// and the four Place strips on the right; key, quality, seed and output below.
// The full Living Globe and resizable layout arrive in Milestone 6.
#include "PluginProcessor.h"
#include "UI/ArrangeView.h"
#include "UI/CapturePanel.h"
#include "UI/FrameGovernor.h"
#include "UI/TinyPlanet.h"
#include "UI/LocationBar.h"
#include "UI/PatternEditor.h"
#include "UI/PlaceArt.h"
#include "UI/PlaceOverlays.h"
#include "UI/PixelLookAndFeel.h"
#include "UI/WorldLookAndFeel.h"

class WorldbuilderEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit WorldbuilderEditor(WorldbuilderProcessor&);
    ~WorldbuilderEditor() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static constexpr int baseWidth = 1160, baseHeight = 812;

    // For the tests: add a Place to the first empty slot (as a card click does) and
    // open a slot's detail panel.
    int addPlace(int type);
    void openDetail(int slot);
    // Opens the Pattern Editor on a slot's Place (on `element`, or its first pattern).
    void openPatterns(int slot, int element = -1);
    wb::ui::PatternEditor& patternEditor() { return *patterns; }
    // The ARRANGE view (Milestone 11), over the globe and the cards like the Pattern Editor.
    void openArrange();
    void closeArrange();
    wb::ui::ArrangeView& arrangeView() { return *arrange; }
    juce::Button& arrangeButton() { return arrangeToggle; }
    wb::ui::CapturePanel& capturePanel() { return *capture; }
    wb::ui::LocationBar& locationBar() { return *locationRow; }
    // Where Location i's marker sits on the globe's orbit.
    juce::Point<float> markerPosition(int index) const;
    // Runs the animation (theme, overlays) on by `seconds`, as the timer does.
    void settle(float seconds);
    // One tick of the animation timer at `nowMs` (the timer passes the real clock; the
    // stress test passes its own). The governor decides whether it draws a frame.
    void frameTick(double nowMs);
    const wb::ui::FrameGovernor& frameGovernor() const { return governor; }
    // True after slow frames dropped the overlays from Full to Subtle.
    bool overlaysDroppedToSubtle() const { return autoSubtle; }
    // For the tests: report a frame's paint cost as if it had taken `ms`.
    void reportFrameCost(double ms, double nowMs) { governor.painted(ms, nowMs); }
    wb::ui::PlaceOverlays& placeOverlays() { return *overlays; }
    void setOverlayMode(int mode);
    // UI size: 0 = 100 %, 1 = 150 %, 2 = 200 % (kept with the project).
    void setUiSize(int index);
    int uiSizeIndex() const { return uiSize.getSelectedId() - 1; }
    // Lists one category of Place cards (-1: all), as its chip does; how many are listed.
    void showCategory(int c);
    int listedPlaces() const { return listedCount; }
    // The tiny planet (its last frame, in art pixels) and the Planet it shows.
    const wb::ui::TinyPlanet& tinyPlanet() const { return tiny; }
    juce::Rectangle<int> planetArea() const { return planetScene; }
    // Turns the planet to `turn` radians (tests and snapshots: day is 0, night is 2 pi).
    void setPlanetTurn(float turn) { spin = turn / 0.3f; }
    float planetTurnNow() const { return planetTurn(); }
    // Whether a slot's Place shows on the planet now (not over the horizon).
    bool placeInView(int slot) const { return landmarkPosition(slot).x > -999.0f; }
    int shownPlanetType() const { return shownPlanet; }
    // Scenes (Milestone 12): the header's scene browser, INSPIRE and UNDO.
    void loadScene(int index);
    void inspire();
    void undoScene();
    juce::Button& inspireButton() { return inspireToggle; }
    juce::Button& sceneUndoButton() { return sceneUndo; }
    juce::Button& sceneNameButton() { return sceneMenu; }
    juce::Button& scenePrevButton() { return scenePrev; }
    juce::Button& sceneNextButton() { return sceneNext; }

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
    juce::Point<float> landmarkPosition(int slot) const;
    // How far the planet has turned (radians; it follows the spin), where a slot's Place
    // stands on it.
    float planetTurn() const;
    float landmarkLongitude(int slot) const;
    int landmarkAt(juce::Point<float>) const;
    // The Location markers in the planet's sky.
    int markerAt(juce::Point<float>) const;
    void paintLocationMarkers(juce::Graphics&, float alpha);

    WorldbuilderProcessor& processor;
    wb::ui::PixelLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<SliderAttachment>> knobAttachments;
    std::array<juce::Slider*, wb::PCount> knobFor {};
    std::array<LayerStrip, wb::LayerCount> strips;
    juce::Slider environmentLevel;
    std::unique_ptr<SliderAttachment> environmentAttachment;
    juce::Rectangle<int> environmentValue;
    juce::Rectangle<int> planetPlate;   // the Planet and Influence controls' backing, over the sky
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
    std::unique_ptr<wb::ui::ArrangeView> arrange;
    juce::TextButton arrangeToggle { "ARRANGE" };
    juce::TextButton scenePrev { "<" }, sceneNext { ">" }, sceneMenu, inspireToggle { "INSPIRE" }, sceneUndo { "UNDO" };
    // After a scene loads: the strips, the panels and the ARRANGE view show it.
    void sceneChanged();
    void showSceneMenu();
    // The globe and the cards are covered (by the Pattern Editor or the ARRANGE view).
    bool covered() const;
    std::unique_ptr<wb::ui::CapturePanel> capture;
    std::unique_ptr<wb::ui::LocationBar> locationRow;
    juce::ComboBox playMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> playModeAttachment;
    juce::ComboBox overlayChoice;
    // UI size (Milestone 13): 100, 150 or 200 %, whole art pixels at each.
    juce::ComboBox uiSize;
    int pixelFrame = -1;   // the decoration frame last shown
    // This editor's pixel look (the theme is shared; each paint sets this editor's).
    wb::ui::pixel::Mood pixelMood;
    int pixelDelights = 0;
    // Separate outputs (Milestone 12): Copy or Split.
    juce::ComboBox placeOuts;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> placeOutsAttachment;
    // The Planet, by the globe (Milestone 10).
    juce::ComboBox planetChoice;
    juce::Slider planetInfluence;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> planetAttachment;
    std::unique_ptr<SliderAttachment> planetInfluenceAttachment;
    juce::Rectangle<int> planetLabel, influenceLabel;
    wb::ui::TinyPlanet tiny;
    juce::Rectangle<int> planetScene;
    juce::Point<float> dragFrom;
    float dragPan = 0, dragDistance = 0;
    int shownPlanet = 0, previousPlanet = 0;
    float planetBlend = 1;   // 0..1: the crossfade from previousPlanet to shownPlanet
    std::unique_ptr<wb::ui::PlaceOverlays> overlays;
    wb::ui::FrameGovernor governor;
    double paintStarted = -1, owedSeconds = 0;
    bool autoSubtle = false;
    std::vector<juce::Rectangle<int>> overlayKeepClear;

    juce::Rectangle<int> header, controlsPanel, globePanel, cardsPanel, layersPanel, placesPanel, locationsPanel, footer;
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
    int boltSeed = 1, hoverCard = -1, hoverChip = -1, dragLandmark = -1;
    // The marker a journey on the globe set out from (-1: the top of the orbit).
    int journeyStart = -1, journeySeen = -1;
    double fullMessageUntil = 0;
    std::array<float, wb::LayerCount> meters {};
    std::array<float, wb::placeSlotCount> placeMeters {};
    float masterMeter = 0;
};
