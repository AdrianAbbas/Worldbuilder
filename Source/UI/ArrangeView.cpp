#include "ArrangeView.h"
#include "PlaceArt.h"
#include "Style.h"
#include "WorldLookAndFeel.h"

namespace wb::ui {

namespace {
juce::Font font(float size, bool bold = false) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }
juce::Colour sectionColour(const arr::Section& s) { return juce::Colour(arr::colours[std::size_t(std::min<int>(s.colour, arr::colourCount - 1))]); }
const char* colourNames[] = { "Brass", "Cyan", "Ember", "Moss", "Violet", "Amber", "Teal", "Bone", "Rose", "Cobalt" };
constexpr int labelWidth = 150, headerRow = 16;

// Starting values for a new section, by its name: an arc from a quiet intro to a climax
// and back down.
void presetFor(arr::Section& s, int preset) {
    struct P { float start, end; uint8_t curve; };
    static constexpr std::array<P, 8> table { { { 0.25f, 0.35f, arr::CurveLinear }, { 0.4f, 0.75f, arr::CurveEaseIn }, { 0.9f, 0.9f, arr::CurveLinear },
                                                { 0.7f, 0.35f, arr::CurveEaseOut }, { 0.3f, 0.1f, arr::CurveLinear }, { 0.5f, 0.5f, arr::CurveLinear },
                                                { 0.5f, 0.6f, arr::CurveSwell }, { 0.25f, 0.25f, arr::CurveLinear } } };
    const auto& p = table[std::size_t(std::clamp(preset, 0, 7))];
    s.intensityStart = p.start;
    s.intensityEnd = p.end;
    s.curve = p.curve;
}

juce::String repeatsText(int repeats) { return repeats == 0 ? juce::String("Loop (endless)") : (repeats == 1 ? juce::String("Once") : juce::String(repeats) + " times"); }
} // namespace

ArrangeView::ArrangeView(WorldbuilderProcessor& p) : processor(p) {
    setWantsKeyboardFocus(true);
    addButton.setTooltip("Add a section after the selected one (Intro, Build, Climax, Wind-down, Outro, Loop, Bridge, Break in turn; rename it below).");
    duplicateButton.setTooltip("Duplicate the selected section, with its instrument grid.");
    deleteButton.setTooltip("Delete the selected section.");
    undoButton.setTooltip("Undo the last change to the arrangement.");
    redoButton.setTooltip("Redo.");
    back.setTooltip("Back to the globe.");
    previewButton.setTooltip("PREVIEW: while the host is stopped, play the arrangement from the selected section on the plugin's own clock, "
                             "at the host tempo. Clicking another section jumps there. The host starting playback ends it.");
    previewButton.setColour(juce::TextButton::buttonOnColourId, colours::cyan);
    setIcon(undoButton, Icon::Undo);
    setIcon(redoButton, Icon::Redo);
    setIcon(back, Icon::Back);
    setIcon(duplicateButton, Icon::Copy);
    setIcon(deleteButton, Icon::Clear);
    addButton.onClick = [this] { addSection(); };
    duplicateButton.onClick = [this] { duplicateSection(); };
    deleteButton.onClick = [this] { deleteSection(); };
    undoButton.onClick = [this] { processor.arrangement.undo(); };
    redoButton.onClick = [this] { processor.arrangement.redo(); };
    back.onClick = [this] {
        if (onClose) onClose();
    };
    previewButton.setClickingTogglesState(false);
    previewButton.onClick = [this] {
        if (processor.previewing()) processor.stopPreview();
        else startPreview(std::max(selectedIndex, 0));
    };
    for (auto* b : std::initializer_list<juce::Component*> { &addButton, &duplicateButton, &deleteButton, &undoButton, &redoButton, &back, &previewButton })
        addAndMakeVisible(*b);

    endChoice.setTooltip("After the last section: stop (the instruments stop and the world fades out over two bars), or loop back to a section.");
    endChoice.onChange = [this] {
        if (updating) return;
        auto a = processor.arrangement.get();
        const int id = endChoice.getSelectedId();
        a.end = uint8_t(id <= 1 ? arr::EndStop : arr::EndLoop);
        a.loopTo = uint8_t(std::max(0, id - 2));
        processor.arrangement.set(a);
    };
    addAndMakeVisible(endChoice);

    nameChoice.setEditableText(true);
    for (int i = 0; i < int(arr::presetNames.size()); ++i) nameChoice.addItem(arr::presetNames[std::size_t(i)], i + 1);
    nameChoice.setTooltip("The section's name: pick one or type your own.");
    nameChoice.onChange = [this] {
        if (updating) return;
        const auto text = nameChoice.getText().trim().substring(0, arr::nameLength - 1);
        if (text.isEmpty()) return;
        edit([&](arr::Section& s) { s.setName(text.toRawUTF8()); });
    };
    addAndMakeVisible(nameChoice);

    for (std::size_t i = 0; i < arr::repeatChoices.size(); ++i) repeatsChoice.addItem(repeatsText(arr::repeatChoices[i]), int(i) + 1);
    repeatsChoice.setTooltip("How many times the section plays in a row. Loop (endless) repeats it for ever, like a game's looping cue: "
                             "the sections after it are only reached by moving it.");
    repeatsChoice.onChange = [this] {
        if (updating) return;
        const int i = std::clamp(repeatsChoice.getSelectedId() - 1, 0, int(arr::repeatChoices.size()) - 1);
        edit([&](arr::Section& s) { s.repeats = uint8_t(arr::repeatChoices[std::size_t(i)]); });
    };
    addAndMakeVisible(repeatsChoice);

    for (int i = 0; i < arr::CurveCount; ++i) curveChoice.addItem(arr::curveNames[std::size_t(i)], i + 1);
    curveChoice.setTooltip("The shape of the Score Intensity ramp across the section. Swell rises above both ends in the middle.");
    curveChoice.onChange = [this] {
        if (updating) return;
        edit([&](arr::Section& s) { s.curve = uint8_t(std::clamp(curveChoice.getSelectedId() - 1, 0, arr::CurveCount - 1)); });
    };
    addAndMakeVisible(curveChoice);

    const auto setupSlider = [this](juce::Slider& k, double lo, double hi, double step, const juce::String& tip, std::function<void(arr::Section&, double)> apply) {
        k.setSliderStyle(juce::Slider::LinearHorizontal);
        k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        k.setRange(lo, hi, step);
        k.setTooltip(tip);
        // One undo step per drag.
        k.onDragStart = [this] { gestureOpen = false; };
        k.onDragEnd = [this] { gestureOpen = false; };
        k.onValueChange = [this, &k, apply] {
            if (updating) return;
            const double v = k.getValue();
            edit([&](arr::Section& s) { apply(s, v); }, !gestureOpen);
            if (k.isMouseButtonDown()) gestureOpen = true;
            repaint(inspectorArea);
        };
        addAndMakeVisible(k);
    };
    setupSlider(lengthSlider, 1, arr::maxBars, 1, "The section's length in bars (1-64). Or drag its right edge on the timeline.",
                [](arr::Section& s, double v) { s.bars = uint8_t(juce::roundToInt(v)); });
    setupSlider(startSlider, 0, 1, 0.01, "Score Intensity at the start of the section (it ramps to the end value). Host automation of Score Intensity adds on top.",
                [](arr::Section& s, double v) { s.intensityStart = float(v); });
    setupSlider(endSlider, 0, 1, 0.01, "Score Intensity at the end of the section.", [](arr::Section& s, double v) { s.intensityEnd = float(v); });
    startSlider.setDoubleClickReturnValue(true, 0.5);
    endSlider.setDoubleClickReturnValue(true, 0.5);
    lengthSlider.setDoubleClickReturnValue(true, 8);

    shown = processor.arrangement.get();
    shownRevision = processor.arrangement.revision();
    shownTypes = processor.slotTypes();
    selectedIndex = shown.count > 0 ? 0 : -1;
    refreshInspector();
    startTimerHz(30);
}

ArrangeView::~ArrangeView() {
    stopTimer();
    processor.stopPreview();
}

//==============================================================================
void ArrangeView::select(int index) {
    selectedIndex = shown.count > 0 ? std::clamp(index, 0, shown.count - 1) : -1;
    refreshInspector();
    repaint();
}

void ArrangeView::edit(const std::function<void(arr::Section&)>& change, bool undoable) {
    if (selectedIndex < 0 || selectedIndex >= shown.count) return;
    auto a = processor.arrangement.get();
    if (selectedIndex >= a.count) return;
    change(a.sections[std::size_t(selectedIndex)]);
    processor.arrangement.set(a, undoable);
    shown = a;
    shownRevision = processor.arrangement.revision();
    layoutTimeline();
    repaint();
}

void ArrangeView::addSection() {
    const int preset = shown.count % int(arr::presetNames.size());
    auto s = arr::makeSection(arr::presetNames[std::size_t(preset)], 8, shown.count);
    presetFor(s, preset);
    const int index = processor.arrangement.add(s, selectedIndex < 0 ? -1 : selectedIndex + 1);
    shown = processor.arrangement.get();
    shownRevision = processor.arrangement.revision();
    select(index);
    layoutTimeline();
}

void ArrangeView::duplicateSection() {
    if (selectedIndex < 0) return;
    const int index = processor.arrangement.duplicate(selectedIndex);
    shown = processor.arrangement.get();
    shownRevision = processor.arrangement.revision();
    select(index);
    layoutTimeline();
}

void ArrangeView::deleteSection() {
    if (selectedIndex < 0) return;
    const int index = processor.arrangement.remove(selectedIndex);
    shown = processor.arrangement.get();
    shownRevision = processor.arrangement.revision();
    select(index);
    layoutTimeline();
}

void ArrangeView::setCell(int section, int slot, int element, const arr::Cell& cell) {
    auto a = processor.arrangement.get();
    if (section < 0 || section >= a.count || slot < 0 || slot >= placeSlots || element < 0 || element >= placeElements) return;
    a.sections[std::size_t(section)].cells[std::size_t(slot)][std::size_t(element)] = cell;
    processor.arrangement.set(a);
    shown = a;
    shownRevision = processor.arrangement.revision();
    repaint(gridArea);
}

void ArrangeView::clickCell(int section, int slot, int element, bool shift) {
    if (section < 0 || section >= shown.count) return;
    auto cell = shown.sections[std::size_t(section)].cells[std::size_t(slot)][std::size_t(element)];
    if (shift) cell.variant = uint8_t((cell.variant + 1) % 3);
    else cell.entry = uint8_t(cell.entry == arr::EntryOff ? arr::EntryOn : arr::EntryOff);
    setCell(section, slot, element, cell);
}

void ArrangeView::startPreview(int section) {
    if (shown.count <= 0) return;
    section = std::clamp(section, 0, shown.count - 1);
    processor.previewFrom(shown.startOf(section));
    select(section);
}

//==============================================================================
void ArrangeView::refreshInspector() {
    const juce::ScopedValueSetter<bool> guard(updating, true);
    const bool any = selectedIndex >= 0 && selectedIndex < shown.count;
    for (auto* c : std::initializer_list<juce::Component*> { &nameChoice, &repeatsChoice, &curveChoice, &lengthSlider, &startSlider, &endSlider, &duplicateButton, &deleteButton })
        c->setEnabled(any);
    if (any) {
        const auto& s = shown.sections[std::size_t(selectedIndex)];
        nameChoice.setText(juce::String::fromUTF8(s.nameText()), juce::dontSendNotification);
        int r = 1;
        for (std::size_t i = 0; i < arr::repeatChoices.size(); ++i)
            if (arr::repeatChoices[i] == s.repeats) r = int(i) + 1;
        repeatsChoice.setSelectedId(r, juce::dontSendNotification);
        curveChoice.setSelectedId(s.curve + 1, juce::dontSendNotification);
        lengthSlider.setValue(s.bars, juce::dontSendNotification);
        startSlider.setValue(s.intensityStart, juce::dontSendNotification);
        endSlider.setValue(s.intensityEnd, juce::dontSendNotification);
    }
    endChoice.clear(juce::dontSendNotification);
    endChoice.addItem("End: stop", 1);
    for (int i = 0; i < shown.count; ++i)
        endChoice.addItem("End: loop to " + juce::String(i + 1) + " " + juce::String::fromUTF8(shown.sections[std::size_t(i)].nameText()), i + 2);
    endChoice.setSelectedId(shown.end == arr::EndLoop && shown.count > 0 ? shown.loopTarget() + 2 : 1, juce::dontSendNotification);
    endChoice.setEnabled(shown.count > 0);
    addButton.setEnabled(shown.count < arr::maxSections);
    updateButtons();
}

void ArrangeView::updateButtons() {
    undoButton.setEnabled(processor.arrangement.canUndo());
    redoButton.setEnabled(processor.arrangement.canRedo());
    const bool hostPlaying = processor.engine.transportRolling.load(std::memory_order_relaxed) && !processor.previewing();
    previewButton.setEnabled(shown.count > 0 && !hostPlaying);
    previewButton.setToggleState(processor.previewing(), juce::dontSendNotification);
}

void ArrangeView::refresh() {
    shown = processor.arrangement.get();
    shownRevision = processor.arrangement.revision();
    shownTypes = processor.slotTypes();
    if (selectedIndex >= shown.count || (selectedIndex < 0 && shown.count > 0)) selectedIndex = shown.count > 0 ? 0 : -1;
    refreshInspector();
    layoutTimeline();
    layoutRows();
    repaint();
}

void ArrangeView::timerCallback() {
    if (!isShowing()) return;
    bool layout = false;
    if (processor.arrangement.revision() != shownRevision) {
        shown = processor.arrangement.get();
        shownRevision = processor.arrangement.revision();
        if (selectedIndex >= shown.count) selectedIndex = shown.count - 1;
        if (selectedIndex < 0 && shown.count > 0) selectedIndex = 0;
        if (drag == Drag::None) refreshInspector();
        layout = true;
    }
    const auto types = processor.slotTypes();
    if (types != shownTypes) {
        shownTypes = types;
        layout = true;
    }
    if (layout) {
        layoutTimeline();
        layoutRows();
        repaint();
    }
    updateButtons();
    // The playhead.
    const int section = processor.engine.arrangeSection.load(std::memory_order_relaxed);
    const double bar = section == -1 ? -1.0 : processor.engine.arrangeBar.load(std::memory_order_relaxed);
    if (section != playSection || std::abs(bar - playBar) * pxPerBar >= 0.5) {
        playSection = section;
        playBar = bar;
        repaint(timeline.getUnion(gridArea).getUnion(ruler).expanded(2));
    }
}

//==============================================================================
float ArrangeView::xForBar(double bar) const { return float(double(timeline.getX()) + (bar - scrollBars) * pxPerBar); }

int ArrangeView::barAt(float x) const { return int(std::floor(scrollBars + double(x - float(timeline.getX())) / pxPerBar)); }

void ArrangeView::layoutTimeline() {
    const int total = std::max(shown.totalBars(), 1);
    const double width = std::max(1, timeline.getWidth());
    pxPerBar = std::max(4.0, width / double(std::max(total + 2, 16)));
    const double maxScroll = std::max(0.0, double(total + 2) - width / pxPerBar);
    scrollBars = std::clamp(scrollBars, 0.0, maxScroll);
}

void ArrangeView::layoutRows() {
    rows.clear();
    int active = 0;
    for (int t : shownTypes) active += t != NoPlace ? 1 : 0;
    if (active == 0) return;
    const int rowHeight = std::clamp((gridArea.getHeight() - headerRow * active) / (placeElements * active), 11, 22);
    int y = gridArea.getY();
    for (int slot = 0; slot < placeSlots; ++slot) {
        if (shownTypes[std::size_t(slot)] == NoPlace) continue;
        rows.push_back({ slot, -1, { gridArea.getX(), y, gridArea.getWidth(), headerRow } });
        y += headerRow;
        for (int e = 0; e < placeElements; ++e) {
            rows.push_back({ slot, e, { gridArea.getX(), y, gridArea.getWidth(), rowHeight } });
            y += rowHeight;
        }
    }
}

juce::Rectangle<int> ArrangeView::sectionBounds(int index) const {
    if (index < 0 || index >= shown.count) return {};
    const double start = shown.startOf(index), span = shown.sections[std::size_t(index)].span();
    const int x0 = juce::roundToInt(xForBar(start)), x1 = juce::roundToInt(xForBar(start + span));
    return juce::Rectangle<int>(x0, timeline.getY(), x1 - x0, timeline.getHeight()).reduced(1, 0).getIntersection(timeline);
}

juce::Rectangle<int> ArrangeView::cellBounds(int section, int slot, int element) const {
    if (section < 0 || section >= shown.count) return {};
    for (const auto& row : rows)
        if (row.slot == slot && row.element == element) {
            const auto x = sectionBounds(section);
            return juce::Rectangle<int>(x.getX(), row.bounds.getY(), x.getWidth(), row.bounds.getHeight()).reduced(1, 1);
        }
    return {};
}

int ArrangeView::sectionAt(juce::Point<int> p, bool* edge) const {
    for (int i = 0; i < shown.count; ++i) {
        const auto b = sectionBounds(i);
        if (b.getX() <= p.x && p.x < b.getRight() + 4 && p.x >= timeline.getX()) {
            if (edge != nullptr) *edge = p.x >= b.getRight() - 5;
            if (p.x < b.getRight() || (edge != nullptr && *edge)) return i;
        }
    }
    return -1;
}

bool ArrangeView::cellAt(juce::Point<int> p, int& section, int& slot, int& element) const {
    if (!gridArea.contains(p) || p.x < timeline.getX()) return false;
    for (const auto& row : rows) {
        if (row.element < 0 || !row.bounds.contains(p)) continue;
        for (int i = 0; i < shown.count; ++i) {
            const auto b = sectionBounds(i);
            if (b.getX() <= p.x && p.x < b.getRight()) {
                section = i;
                slot = row.slot;
                element = row.element;
                return true;
            }
        }
    }
    return false;
}

//==============================================================================
void ArrangeView::resized() {
    auto r = getLocalBounds().reduced(14, 10);
    titleArea = r.removeFromTop(36);
    {
        auto t = titleArea;
        back.setBounds(t.removeFromRight(70).withSizeKeepingCentre(66, 24));
        t.removeFromRight(14);
        previewButton.setBounds(t.removeFromRight(86).withSizeKeepingCentre(84, 24));
        t.removeFromRight(14);
        redoButton.setBounds(t.removeFromRight(64).withSizeKeepingCentre(62, 24));
        undoButton.setBounds(t.removeFromRight(64).withSizeKeepingCentre(62, 24));
    }
    r.removeFromTop(4);
    toolbarArea = r.removeFromTop(26);
    {
        auto t = toolbarArea;
        addButton.setBounds(t.removeFromLeft(90).reduced(2, 1));
        duplicateButton.setBounds(t.removeFromLeft(100).reduced(2, 1));
        deleteButton.setBounds(t.removeFromLeft(80).reduced(2, 1));
        endChoice.setBounds(t.removeFromRight(210).reduced(2, 1));
    }
    r.removeFromTop(6);
    inspectorArea = r.removeFromTop(40);
    {
        auto t = inspectorArea.withTrimmedTop(14);
        const auto place = [&t](juce::Component& c, int w) {
            c.setBounds(t.removeFromLeft(w).reduced(2, 1));
            t.removeFromLeft(8);
        };
        place(nameChoice, 120);
        const int swatch = 13;
        auto sw = t.removeFromLeft(arr::colourCount * (swatch + 3));
        for (int i = 0; i < arr::colourCount; ++i) {
            swatches[std::size_t(i)] = sw.removeFromLeft(swatch + 3).withSizeKeepingCentre(swatch, swatch);
        }
        t.removeFromLeft(10);
        place(lengthSlider, 104);
        place(repeatsChoice, 112);
        place(startSlider, 96);
        place(endSlider, 84);
        place(curveChoice, std::max(70, std::min(96, t.getWidth() - 4)));
    }
    r.removeFromTop(6);
    statusArea = r.removeFromBottom(16);
    r.removeFromBottom(4);
    ruler = r.removeFromTop(16).withTrimmedLeft(labelWidth);
    timeline = r.removeFromTop(52).withTrimmedLeft(labelWidth);
    r.removeFromTop(6);
    gridArea = r;
    labelColumn = r.withWidth(labelWidth - 6);
    emptyArea = r;
    layoutTimeline();
    layoutRows();
}

//==============================================================================
void ArrangeView::paint(juce::Graphics& g) {
    const auto area = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(colours::panel.brighter(0.04f), 0.0f, 0.0f, colours::panel.darker(0.3f), 0.0f, area.getHeight(), false));
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(colours::brass.withAlpha(0.7f));
    g.drawRoundedRectangle(area.reduced(0.5f), 10.0f, 1.2f);

    {
        auto t = titleArea;
        g.setColour(colours::parchment);
        g.setFont(font(19.0f, true).withExtraKerningFactor(0.12f));
        g.drawText("ARRANGE", t.removeFromTop(22), juce::Justification::centredLeft, false);
        g.setColour(colours::brass);
        g.setFont(font(10.5f, true).withExtraKerningFactor(0.18f));
        const int total = shown.totalBars();
        juce::String sub = juce::String(shown.count) + (shown.count == 1 ? " SECTION" : " SECTIONS") + "  -  " + juce::String(total) + " BARS";
        if (shown.firstEndless() < shown.count) sub << "  -  ENDS IN A LOOP";
        g.drawText(sub, t, juce::Justification::centredLeft, false);
    }

    // The inspector's labels.
    g.setColour(colours::brass.withAlpha(0.85f));
    g.setFont(font(9.5f, true).withExtraKerningFactor(0.12f));
    const auto above = [&](juce::Component& c, const juce::String& text) {
        g.drawText(text, c.getBounds().translated(0, -13).withHeight(12).withWidth(c.getWidth() + 40), juce::Justification::centredLeft, false);
    };
    const bool any = selectedIndex >= 0 && selectedIndex < shown.count;
    above(nameChoice, any ? "SECTION " + juce::String(selectedIndex + 1) : juce::String("SECTION"));
    above(lengthSlider, "LENGTH " + juce::String(juce::roundToInt(lengthSlider.getValue())) + " BARS");
    above(repeatsChoice, "PLAYS");
    above(startSlider, "INTENSITY " + juce::String(juce::roundToInt(startSlider.getValue() * 100.0)) + "%");
    above(endSlider, "TO " + juce::String(juce::roundToInt(endSlider.getValue() * 100.0)) + "%");
    above(curveChoice, "CURVE");
    if (swatches[0].getWidth() > 0) {
        g.drawText("COLOUR", swatches[0].translated(0, -13).withHeight(12).withWidth(80), juce::Justification::centredLeft, false);
        for (int i = 0; i < arr::colourCount; ++i) {
            const auto b = swatches[std::size_t(i)].toFloat();
            g.setColour(juce::Colour(arr::colours[std::size_t(i)]).withAlpha(any ? 1.0f : 0.35f));
            g.fillRoundedRectangle(b, 2.5f);
            if (any && shown.sections[std::size_t(selectedIndex)].colour == i) {
                g.setColour(colours::parchment);
                g.drawRoundedRectangle(b.expanded(2.0f), 3.5f, 1.4f);
            }
        }
    }

    // The ruler: bar numbers (1-based), spaced to stay readable.
    {
        g.setColour(colours::background.withAlpha(0.6f));
        g.fillRect(ruler);
        int step = 1;
        while (double(step) * pxPerBar < 26.0) step *= 2;
        g.setFont(font(9.0f));
        const int first = std::max(0, int(std::floor(scrollBars)));
        const int last = int(scrollBars + double(ruler.getWidth()) / pxPerBar) + 1;
        for (int bar = first; bar <= last; ++bar) {
            if (bar % step != 0) continue;
            const float x = xForBar(bar);
            if (x < float(ruler.getX()) || x > float(ruler.getRight())) continue;
            g.setColour(colours::dim.withAlpha(0.7f));
            g.drawVerticalLine(int(x), float(ruler.getBottom() - 5), float(ruler.getBottom()));
            g.setColour(colours::dim);
            g.drawText(juce::String(bar + 1), juce::Rectangle<float>(x + 2.0f, float(ruler.getY()), 30.0f, float(ruler.getHeight() - 3)), juce::Justification::centredLeft,
                       false);
        }
    }

    // The timeline.
    g.setColour(colours::background.withAlpha(0.45f));
    g.fillRoundedRectangle(timeline.toFloat(), 4.0f);
    {
        g.saveState();
        g.reduceClipRegion(timeline.expanded(0, 2));
        for (int i = 0; i < shown.count; ++i) {
            const auto& s = shown.sections[std::size_t(i)];
            const auto b = sectionBounds(i).toFloat().reduced(0.0f, 3.0f);
            if (b.getWidth() <= 0) continue;
            const auto c = sectionColour(s);
            const bool sel = i == selectedIndex, hover = i == hoverSection, playing = i == playSection;
            g.setGradientFill(juce::ColourGradient(c.withAlpha(sel ? 0.75f : 0.55f), 0.0f, b.getY(), c.darker(0.6f).withAlpha(0.7f), 0.0f, b.getBottom(), false));
            g.fillRoundedRectangle(b, 4.0f);
            if (s.endless()) {
                // An endless loop: hatched.
                g.setColour(colours::background.withAlpha(0.25f));
                for (float x = b.getX() - b.getHeight(); x < b.getRight(); x += 7.0f)
                    g.drawLine(juce::Line<float>(x, b.getBottom(), x + b.getHeight(), b.getY()).withShortenedStart(0.0f), 1.0f);
            }
            // The intensity ramp, pass by pass.
            const int passes = std::max<int>(s.repeats, 1);
            const float passWidth = b.getWidth() / float(passes);
            const auto inner = b.reduced(3.0f, 6.0f);
            for (int pass = 0; pass < passes; ++pass) {
                juce::Path curve;
                const float x0 = b.getX() + passWidth * float(pass);
                for (int k = 0; k <= 24; ++k) {
                    const float t = float(k) / 24.0f;
                    const float y = inner.getBottom() - inner.getHeight() * arr::intensityAt(s, t);
                    const float x = x0 + passWidth * t;
                    k == 0 ? curve.startNewSubPath(x, y) : curve.lineTo(x, y);
                }
                g.setColour(colours::parchment.withAlpha(0.55f));
                g.strokePath(curve, juce::PathStrokeType(1.3f));
                if (pass > 0) {
                    g.setColour(colours::background.withAlpha(0.5f));
                    const float dash[] { 3.0f, 3.0f };
                    g.drawDashedLine(juce::Line<float>(x0, b.getY() + 2.0f, x0, b.getBottom() - 2.0f), dash, 2, 1.0f);
                }
            }
            g.setColour(sel || hover ? colours::parchment : c.brighter(0.4f).withAlpha(0.8f));
            g.drawRoundedRectangle(b, 4.0f, sel ? 1.8f : 1.0f);
            if (playing) drawGlow(g, b, 4.0f, colours::cyan, 6.0f);
            // Name and length.
            if (b.getWidth() > 24.0f) {
                auto text = b.reduced(5.0f, 3.0f);
                g.setColour(colours::parchment);
                g.setFont(font(11.0f, true));
                g.drawText(juce::String::fromUTF8(s.nameText()), text.removeFromTop(14.0f), juce::Justification::centredLeft, true);
                g.setFont(font(9.0f));
                g.setColour(colours::parchment.withAlpha(0.75f));
                juce::String info = juce::String(s.bars) + " bars";
                if (s.endless()) info << "  loop";
                else if (s.repeats > 1) info << "  x" << int(s.repeats);
                g.drawText(info, text.removeFromBottom(12.0f), juce::Justification::centredLeft, true);
            }
        }
        // Where a dragged section would go.
        if (drag == Drag::Move && dragStarted && dropIndex >= 0 && dropIndex != dragIndex) {
            const auto b = sectionBounds(dropIndex);
            const float x = dropIndex > dragIndex ? float(b.getRight()) : float(b.getX());
            g.setColour(colours::cyan);
            g.fillRect(juce::Rectangle<float>(x - 1.5f, float(timeline.getY()), 3.0f, float(timeline.getHeight())));
        }
        // The end: stop, or the way back to the loop's start.
        if (shown.count > 0 && shown.firstEndless() == shown.count) {
            const float x = xForBar(shown.totalBars());
            g.setColour(colours::brass.withAlpha(0.8f));
            g.setFont(font(9.0f, true));
            const auto label = shown.end == arr::EndStop ? juce::String("END") : "TO " + juce::String(shown.loopTarget() + 1);
            g.drawText(label, juce::Rectangle<float>(x + 4.0f, float(timeline.getY()), 60.0f, float(timeline.getHeight())), juce::Justification::centredLeft, false);
        }
        g.restoreState();
    }

    // The instrument grid.
    if (rows.empty() || shown.count == 0) {
        g.setColour(colours::dim);
        g.setFont(font(12.0f));
        const juce::String text = shown.count == 0
                                      ? "No sections yet. + SECTION adds one: give each a length, a Score Intensity ramp and the instruments that play in it."
                                      : "Add a Place to the world: its instruments appear here, one row each, with a cell per section.";
        g.drawFittedText(text, emptyArea.reduced(40, 20), juce::Justification::centred, 3);
    }
    if (!rows.empty()) {
        g.saveState();
        g.reduceClipRegion(gridArea);
        for (const auto& row : rows) {
            const int type = shownTypes[std::size_t(row.slot)];
            const auto accent = placeAccent(type);
            if (row.element < 0) {
                auto h = row.bounds;
                g.setColour(accent.withAlpha(0.18f));
                g.fillRect(h);
                drawPlaceIcon(g, type, h.removeFromLeft(18).toFloat().reduced(1.0f), colours::parchment, accent.withAlpha(0.5f));
                g.setColour(colours::parchment);
                g.setFont(font(10.5f, true));
                g.drawText("PLACE " + juce::String(row.slot + 1) + "  " + juce::String(placeNames[std::size_t(type)]).toUpperCase(), h.withTrimmedLeft(4),
                           juce::Justification::centredLeft, true);
                continue;
            }
            const auto& spec = placeInfo(type).element[std::size_t(row.element)];
            auto label = row.bounds.withWidth(labelWidth - 6);
            if (row.element % 2 == 1) {
                g.setColour(colours::background.withAlpha(0.25f));
                g.fillRect(row.bounds);
            }
            drawInstrumentIcon(g, spec.icon, label.removeFromLeft(18).toFloat().reduced(2.0f), accent);
            g.setColour(colours::parchment.withAlpha(0.85f));
            g.setFont(font(std::min(11.0f, float(row.bounds.getHeight()) - 1.0f)));
            g.drawText(spec.name, label.withTrimmedLeft(4), juce::Justification::centredLeft, true);
            for (int i = 0; i < shown.count; ++i) {
                const auto& s = shown.sections[std::size_t(i)];
                const auto cell = s.cells[std::size_t(row.slot)][std::size_t(row.element)];
                const auto b = cellBounds(i, row.slot, row.element).toFloat();
                if (b.getWidth() <= 1.0f) continue;
                const auto c = sectionColour(s);
                g.setColour(colours::background.withAlpha(0.55f));
                g.fillRoundedRectangle(b, 2.0f);
                const float passWidth = b.getWidth() / float(std::max<int>(s.repeats, 1));
                juce::Path shape;
                switch (cell.entry) {
                    case arr::EntryOn: shape.addRoundedRectangle(b, 2.0f); break;
                    case arr::EntryHalfway: {
                        // Each pass: from its middle bar.
                        const float half = passWidth * (s.bars >= 2 ? float(s.bars / 2) / float(s.bars) : 0.5f);
                        for (int pass = 0; pass < std::max<int>(s.repeats, 1); ++pass)
                            shape.addRectangle(b.getX() + passWidth * float(pass) + half, b.getY(), passWidth - half, b.getHeight());
                        break;
                    }
                    case arr::EntryFadeIn:
                    case arr::EntryFadeOut:
                        for (int pass = 0; pass < std::max<int>(s.repeats, 1); ++pass) {
                            const float x0 = b.getX() + passWidth * float(pass), x1 = x0 + passWidth;
                            if (cell.entry == arr::EntryFadeIn) shape.addTriangle(x0, b.getBottom(), x1, b.getY(), x1, b.getBottom());
                            else shape.addTriangle(x0, b.getY(), x1, b.getBottom(), x0, b.getBottom());
                        }
                        break;
                    default: break;
                }
                if (!shape.isEmpty()) {
                    g.setColour(c.withAlpha(0.6f));
                    g.fillPath(shape);
                }
                if (cell.fill && cell.entry != arr::EntryOff && shown.changeAfter(i)) {
                    // The Fill: the last bar.
                    const float w = std::min(b.getWidth(), float(pxPerBar));
                    g.setColour(colours::parchment.withAlpha(0.75f));
                    g.fillRect(juce::Rectangle<float>(b.getRight() - w, b.getY(), w, b.getHeight()).reduced(0.5f, 1.5f));
                }
                if (cell.entry != arr::EntryOff && b.getWidth() >= 12.0f && b.getHeight() >= 9.0f) {
                    g.setColour(colours::background.withAlpha(0.9f));
                    g.setFont(font(std::min(10.0f, b.getHeight() - 1.0f), true));
                    g.drawText(arr::variantNames[std::size_t(std::min<int>(cell.variant, 2))], b.withTrimmedLeft(3.0f), juce::Justification::centredLeft, false);
                }
                const bool hover = i == hoverCellSection && row.slot == hoverCellSlot && row.element == hoverCellElement;
                g.setColour(hover ? colours::parchment : c.withAlpha(cell.entry == arr::EntryOff ? 0.25f : 0.6f));
                g.drawRoundedRectangle(b, 2.0f, hover ? 1.3f : 0.8f);
            }
        }
        g.restoreState();
    }

    // The playhead over the timeline and the grid.
    if (playBar >= 0.0) {
        const float x = xForBar(playBar);
        if (x >= float(timeline.getX()) && x <= float(timeline.getRight())) {
            g.setColour(colours::cyan.withAlpha(0.9f));
            g.fillRect(juce::Rectangle<float>(x - 0.75f, float(ruler.getY()), 1.5f, float(gridArea.getBottom() - ruler.getY())));
        }
    }

    // Status: how the arrangement plays.
    g.setFont(font(10.5f));
    const int mode = juce::roundToInt(processor.parameters.getRawParameterValue(specs[PPlayMode].id)->load());
    juce::String status;
    if (processor.previewing()) status = "Previewing from bar " + juce::String(processor.engine.arrangeBar.load() >= 0 ? juce::roundToInt(std::floor(playBar)) + 1 : 1) + ".";
    else if (mode != PlayArrangement) status = "Play Mode is " + juce::String(mode == PlayFree ? "Free" : "Transport") + ": set it to Arrangement for the host to play these sections. ";
    else status = "Play Mode Arrangement: the host's song position plays the section there. ";
    if (!processor.previewing())
        status += "Click a cell: on/off. Shift-click: variant A/B/C. Right-click: enter halfway, fade, Fill.";
    g.setColour(colours::dim);
    g.drawText(status, statusArea, juce::Justification::centredLeft, true);
}

//==============================================================================
void ArrangeView::mouseDown(const juce::MouseEvent& e) {
    grabKeyboardFocus();
    const auto p = e.getPosition();
    const bool menu = e.mods.isPopupMenu() || e.mods.isAltDown();
    for (int i = 0; i < arr::colourCount; ++i)
        if (swatches[std::size_t(i)].expanded(2).contains(p)) {
            edit([i](arr::Section& s) { s.colour = uint8_t(i); });
            return;
        }
    if (timeline.contains(p)) {
        bool edge = false;
        const int index = sectionAt(p, &edge);
        if (index < 0) return;
        select(index);
        if (menu) {
            showSectionMenu(index);
            return;
        }
        if (processor.previewing()) startPreview(index);
        dragIndex = index;
        dragStart = p;
        dragStarted = false;
        dropIndex = index;
        dragStartBars = shown.sections[std::size_t(index)].bars;
        drag = edge ? Drag::Resize : Drag::Move;
        return;
    }
    if (ruler.contains(p) && processor.previewing()) {
        processor.previewFrom(std::max(0, barAt(float(p.x))));
        return;
    }
    int section = -1, slot = -1, element = -1;
    if (cellAt(p, section, slot, element)) {
        if (menu) showCellMenu(section, slot, element);
        else clickCell(section, slot, element, e.mods.isShiftDown());
        return;
    }
    if (labelColumn.contains(p) && menu)
        for (const auto& row : rows)
            if (row.element >= 0 && row.bounds.contains(p)) showRowMenu(row.slot, row.element);
}

void ArrangeView::mouseDrag(const juce::MouseEvent& e) {
    if (drag == Drag::None || dragIndex < 0 || dragIndex >= shown.count) return;
    const int dx = e.getPosition().x - dragStart.x;
    if (drag == Drag::Resize) {
        const auto& s = shown.sections[std::size_t(dragIndex)];
        const int bars = std::clamp(dragStartBars + juce::roundToInt(double(dx) / (pxPerBar * double(std::max<int>(s.repeats, 1)))), arr::minBars, arr::maxBars);
        if (bars != s.bars) {
            processor.arrangement.resize(dragIndex, bars, !dragStarted);
            dragStarted = true;
            shown = processor.arrangement.get();
            shownRevision = processor.arrangement.revision();
            refreshInspector();
            repaint();
        }
        return;
    }
    if (!dragStarted && std::abs(dx) < 6) return;
    dragStarted = true;
    // The section whose middle is nearest the mouse.
    int best = dragIndex;
    float bestDistance = 1.0e9f;
    for (int i = 0; i < shown.count; ++i) {
        const float d = std::abs(float(sectionBounds(i).getCentreX()) - float(e.getPosition().x));
        if (d < bestDistance) {
            bestDistance = d;
            best = i;
        }
    }
    if (best != dropIndex) {
        dropIndex = best;
        repaint(timeline.expanded(4));
    }
}

void ArrangeView::mouseUp(const juce::MouseEvent&) {
    if (drag == Drag::Move && dragStarted && dropIndex >= 0 && dropIndex != dragIndex) {
        const int to = processor.arrangement.move(dragIndex, dropIndex);
        shown = processor.arrangement.get();
        shownRevision = processor.arrangement.revision();
        select(to);
    }
    drag = Drag::None;
    dragIndex = dropIndex = -1;
    dragStarted = false;
    refreshInspector();
    repaint();
}

void ArrangeView::mouseMove(const juce::MouseEvent& e) {
    const auto p = e.getPosition();
    bool edge = false;
    const int section = timeline.contains(p) ? sectionAt(p, &edge) : -1;
    int cs = -1, slot = -1, element = -1;
    if (!cellAt(p, cs, slot, element)) cs = slot = element = -1;
    setMouseCursor(section >= 0 && edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    if (section != hoverSection || cs != hoverCellSection || slot != hoverCellSlot || element != hoverCellElement) {
        hoverSection = section;
        hoverCellSection = cs;
        hoverCellSlot = slot;
        hoverCellElement = element;
        repaint(timeline.getUnion(gridArea).expanded(2));
    }
}

void ArrangeView::mouseExit(const juce::MouseEvent&) {
    hoverSection = hoverCellSection = hoverCellSlot = hoverCellElement = -1;
    repaint(timeline.getUnion(gridArea).expanded(2));
}

void ArrangeView::mouseDoubleClick(const juce::MouseEvent& e) {
    if (!timeline.contains(e.getPosition())) return;
    const int index = sectionAt(e.getPosition());
    const bool hostPlaying = processor.engine.transportRolling.load(std::memory_order_relaxed) && !processor.previewing();
    if (index >= 0 && !hostPlaying) startPreview(index);
}

void ArrangeView::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) {
    const double delta = (std::abs(wheel.deltaX) > std::abs(wheel.deltaY) ? -wheel.deltaX : -wheel.deltaY) * 40.0 / pxPerBar;
    scrollBars += delta;
    layoutTimeline();
    repaint();
}

bool ArrangeView::keyPressed(const juce::KeyPress& key) {
    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) {
        deleteSection();
        return true;
    }
    if (mods.isCommandDown()) {
        const auto c = juce::CharacterFunctions::toLowerCase(key.getTextCharacter() != 0 ? key.getTextCharacter() : juce::juce_wchar(code));
        if (c == 'z') {
            mods.isShiftDown() ? processor.arrangement.redo() : processor.arrangement.undo();
            return true;
        }
        if (c == 'y') {
            processor.arrangement.redo();
            return true;
        }
        if (c == 'd') {
            duplicateSection();
            return true;
        }
    }
    if (code == juce::KeyPress::leftKey) {
        select(selectedIndex - 1);
        return true;
    }
    if (code == juce::KeyPress::rightKey) {
        select(selectedIndex + 1);
        return true;
    }
    if (code == juce::KeyPress::spaceKey) {
        previewButton.triggerClick();
        return true;
    }
    return false;
}

//==============================================================================
void ArrangeView::showSectionMenu(int index) {
    juce::PopupMenu m, colours_, plays;
    for (int i = 0; i < arr::colourCount; ++i)
        colours_.addItem(100 + i, colourNames[i], true, shown.sections[std::size_t(index)].colour == i);
    for (std::size_t i = 0; i < arr::repeatChoices.size(); ++i)
        plays.addItem(200 + int(i), repeatsText(arr::repeatChoices[i]), true, shown.sections[std::size_t(index)].repeats == arr::repeatChoices[i]);
    const bool hostPlaying = processor.engine.transportRolling.load(std::memory_order_relaxed) && !processor.previewing();
    m.addItem(1, "Preview from here", !hostPlaying);
    m.addSeparator();
    m.addItem(2, "Duplicate");
    m.addItem(3, "Delete");
    m.addSeparator();
    m.addSubMenu("Plays", plays);
    m.addSubMenu("Colour", colours_);
    m.addSeparator();
    m.addItem(4, "Every instrument on");
    m.addItem(5, "Every instrument off");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [this, index](int r) {
        select(index);
        if (r == 1) startPreview(index);
        else if (r == 2) duplicateSection();
        else if (r == 3) deleteSection();
        else if (r == 4 || r == 5)
            edit([r](arr::Section& s) {
                for (auto& row : s.cells)
                    for (auto& c : row) c.entry = uint8_t(r == 4 ? arr::EntryOn : arr::EntryOff);
            });
        else if (r >= 100 && r < 100 + arr::colourCount) edit([r](arr::Section& s) { s.colour = uint8_t(r - 100); });
        else if (r >= 200 && r < 200 + int(arr::repeatChoices.size())) edit([r](arr::Section& s) { s.repeats = uint8_t(arr::repeatChoices[std::size_t(r - 200)]); });
    });
}

void ArrangeView::showCellMenu(int section, int slot, int element) {
    const auto cell = shown.sections[std::size_t(section)].cells[std::size_t(slot)][std::size_t(element)];
    juce::PopupMenu m;
    m.addSectionHeader(juce::String(placeInfo(shownTypes[std::size_t(slot)]).element[std::size_t(element)].name) + " in "
                       + juce::String::fromUTF8(shown.sections[std::size_t(section)].nameText()));
    for (int i = 0; i < arr::EntryCount; ++i) m.addItem(1 + i, arr::entryNames[std::size_t(i)], true, cell.entry == i);
    m.addSeparator();
    for (int v = 0; v < 3; ++v) m.addItem(10 + v, "Pattern " + juce::String(arr::variantNames[std::size_t(v)]), true, cell.variant == v);
    m.addItem(20, "Fill in the last bar", shown.changeAfter(section), cell.fill);
    m.addSeparator();
    m.addItem(30, "The same in every section");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [this, section, slot, element, cell](int r) {
        auto c = cell;
        if (r >= 1 && r <= arr::EntryCount) c.entry = uint8_t(r - 1);
        else if (r >= 10 && r < 13) c.variant = uint8_t(r - 10);
        else if (r == 20) c.fill = !c.fill;
        else if (r == 30) {
            auto a = processor.arrangement.get();
            for (int i = 0; i < a.count; ++i) a.sections[std::size_t(i)].cells[std::size_t(slot)][std::size_t(element)] = cell;
            processor.arrangement.set(a);
            return;
        } else return;
        setCell(section, slot, element, c);
    });
}

void ArrangeView::showRowMenu(int slot, int element) {
    juce::PopupMenu m;
    m.addSectionHeader(placeInfo(shownTypes[std::size_t(slot)]).element[std::size_t(element)].name);
    m.addItem(1, "On in every section");
    m.addItem(2, "Off in every section");
    for (int v = 0; v < 3; ++v) m.addItem(10 + v, "Pattern " + juce::String(arr::variantNames[std::size_t(v)]) + " everywhere");
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition(), [this, slot, element](int r) {
        if (r == 0) return;
        auto a = processor.arrangement.get();
        for (int i = 0; i < a.count; ++i) {
            auto& c = a.sections[std::size_t(i)].cells[std::size_t(slot)][std::size_t(element)];
            if (r == 1) c.entry = arr::EntryOn;
            else if (r == 2) c.entry = arr::EntryOff;
            else c.variant = uint8_t(r - 10);
        }
        processor.arrangement.set(a);
    });
}

} // namespace wb::ui
