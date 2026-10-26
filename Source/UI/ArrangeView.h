#pragma once
// The ARRANGE view (Milestone 11): the arrangement as a timeline of sections locked to
// the host's bars, and the instrument grid under it.
// - Timeline: click a section to select it; drag it sideways to reorder; drag its right
//   edge to resize (1-64 bars); double-click to preview from it; right-click (or
//   Alt-click) for rename, duplicate, delete, colour, plays and preview.
// - The selected section's settings: name, colour, length, how many times it plays
//   (or endlessly, a game loop), Score Intensity at its start and end, and the curve.
// - Instrument grid: a row for every instrument of every active Place, a cell per
//   section. Click a cell to switch the instrument on or off there; Shift-click steps
//   its pattern variant (A, B, C); right-click (or Alt-click) for every option (enter
//   halfway, fade in, fade out, variant, Fill).
// - PREVIEW plays the arrangement from the selected section while the host is
//   stopped; the playhead and the current section show while it, or the host, plays.
// Every edit can be undone.
#include "../PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace wb::ui {

class ArrangeView final : public juce::Component, private juce::Timer {
public:
    explicit ArrangeView(WorldbuilderProcessor&);
    ~ArrangeView() override;
    std::function<void()> onClose;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

    // Reads the arrangement and the Places again (the editor calls it when the view opens).
    void refresh();
    // Also used by the tests.
    int selected() const { return selectedIndex; }
    void select(int index);
    void addSection();
    void duplicateSection();
    void deleteSection();
    // Where a section sits on the timeline, and a cell of the grid (empty when hidden).
    juce::Rectangle<int> sectionBounds(int index) const;
    juce::Rectangle<int> cellBounds(int section, int slot, int element) const;
    juce::Rectangle<int> timelineBounds() const { return timeline; }
    // A plain click on a cell (on / off), as the mouse does.
    void clickCell(int section, int slot, int element, bool shift = false);
    void setCell(int section, int slot, int element, const arr::Cell& cell);

private:
    void timerCallback() override;
    void refreshInspector();
    void updateButtons();
    void edit(const std::function<void(arr::Section&)>& change, bool undoable = true);
    int sectionAt(juce::Point<int>, bool* edge = nullptr) const;
    bool cellAt(juce::Point<int>, int& section, int& slot, int& element) const;
    int barAt(float x) const;
    float xForBar(double bar) const;
    void layoutTimeline();
    void showSectionMenu(int index);
    void showCellMenu(int section, int slot, int element);
    void showRowMenu(int slot, int element);
    void startPreview(int section);
    // The grid's rows: a header per active slot, then its six instruments.
    struct Row {
        int slot = 0, element = -1;   // element -1: the slot's header
        juce::Rectangle<int> bounds;
    };
    void layoutRows();

    WorldbuilderProcessor& processor;
    arr::Arrangement shown;   // what was last read from the store
    uint32_t shownRevision = 0;
    int selectedIndex = -1, hoverSection = -1, hoverCellSection = -1, hoverCellSlot = -1, hoverCellElement = -1;
    std::array<int, placeSlots> shownTypes {};

    // Dragging a section: moving it (to `dropIndex`) or resizing its right edge.
    enum class Drag { None, Move, Resize };
    Drag drag = Drag::None;
    int dragIndex = -1, dropIndex = -1, dragStartBars = 0;
    bool dragStarted = false;
    juce::Point<int> dragStart;

    double scrollBars = 0, pxPerBar = 12;
    double playBar = -1;   // the playhead (timeline bars), -1: not playing
    int playSection = -1;

    juce::TextButton addButton { "+ SECTION" }, duplicateButton { "DUPLICATE" }, deleteButton { "DELETE" };
    juce::TextButton undoButton { "UNDO" }, redoButton { "REDO" }, back { "BACK" };
    juce::ToggleButton previewButton { "PREVIEW" };
    juce::ComboBox endChoice, nameChoice, repeatsChoice, curveChoice;
    juce::Slider lengthSlider, startSlider, endSlider;
    std::array<juce::Rectangle<int>, arr::colourCount> swatches;
    juce::Rectangle<int> titleArea, toolbarArea, inspectorArea, ruler, timeline, gridArea, labelColumn, statusArea, emptyArea;
    std::vector<Row> rows;
    bool updating = false, gestureOpen = false;
};

} // namespace wb::ui
