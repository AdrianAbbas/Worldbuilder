#pragma once
// The LOCATIONS row (Milestone 9): the eight Location slots and how to travel.
// - A Location chip: click a saved one to travel there; click an empty one to save the
//   world there. Right-click (or Alt-click) for Save here, Travel here, Rename, Colour
//   and Clear; double-click to rename. The chip of the Location the world stands at
//   glows; while travelling, the destination fills with the journey's progress.
// - TRAVEL: the Travel Time (0.5-60 s), or a number of bars of the host tempo.
// - JOURNEY: Journey Mode, its From and To Locations, and the Journey position (for
//   drawing a walk through the world as automation).
// - PATH: the Journey Path: the saved Locations in order, a leg every so many bars of
//   the song (arriving on the downbeat), one-way or looping.
// - SAVE / LOAD .world: the whole world (Places, patterns, effects, World Controls and
//   all eight Locations) as a file, to keep or share.
#include "../PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace wb::ui {

class LocationBar final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer {
public:
    explicit LocationBar(WorldbuilderProcessor&);
    ~LocationBar() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    juce::String getTooltip() override;

    // For the tests and the editor.
    juce::Rectangle<int> chipBounds(int index) const { return chips[std::size_t(juce::jlimit(0, locationCount - 1, index))]; }
    // A click on a chip: travel to a saved Location, save the world into an empty one.
    void clickChip(int index);
    // Whether the world may be saved now (not while a journey or Journey Mode runs).
    bool canSave() const;
    // .world files (the buttons open a file dialog and call these).
    bool saveWorld(const juce::File& file);
    bool loadWorld(const juce::File& file);
    static juce::File worldFolder();

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    void timerCallback() override;
    int chipAt(juce::Point<int>) const;
    void showMenu(int index);
    void rename(int index);

    WorldbuilderProcessor& processor;
    juce::Slider travelTime, journey;
    juce::ComboBox travelSync, from, to, pathLeg, pathEnd;
    juce::ToggleButton journeyMode { "JOURNEY" }, pathMode { "PATH" };
    juce::TextButton saveFile { "SAVE .world" }, loadFile { "LOAD .world" };
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<SliderAttachment> travelTimeAttachment, journeyAttachment;
    std::unique_ptr<ComboAttachment> travelSyncAttachment, fromAttachment, toAttachment, pathLegAttachment, pathEndAttachment;
    std::unique_ptr<ButtonAttachment> journeyModeAttachment, pathModeAttachment;
    std::unique_ptr<juce::AlertWindow> renameWindow;
    std::array<juce::Rectangle<int>, locationCount> chips;
    juce::Rectangle<int> title, travelLabel, travelValue, journeyLabel, arrow;
    int hover = -1;
    // What was last painted (repaint only on change).
    uint32_t shownRevision = 0;
    int shownMode = -1, shownTarget = -2, shownArrived = -2;
    float shownProgress = -1;
};

} // namespace wb::ui
