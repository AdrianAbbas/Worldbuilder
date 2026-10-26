#include "PatternEditor.h"
#include "../Patterns/PatternMidi.h"
#include "PlaceArt.h"
#include "Style.h"

namespace wb::ui {

namespace {
constexpr uint8_t selectedFlag = 0x80;   // editor-only: never published or saved
constexpr int keyWidth = 78;

bool isSelected(const pat::Note& n) { return (n.flags & selectedFlag) != 0; }
void setSelected(pat::Note& n, bool on) { n.flags = uint8_t(on ? (n.flags | selectedFlag) : (n.flags & ~selectedFlag)); }

juce::Colour accentFor(int type) { return placeAccent(type); }

juce::Font font(float size, bool bold = false) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }

// A small padlock, for locked notes.
void drawLock(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) {
    const auto body = r.removeFromBottom(r.getHeight() * 0.55f);
    g.setColour(c);
    g.fillRoundedRectangle(body, 1.0f);
    juce::Path shackle;
    shackle.addCentredArc(body.getCentreX(), body.getY(), body.getWidth() * 0.32f, r.getHeight() * 0.7f, 0.0f, -juce::MathConstants<float>::halfPi,
                          juce::MathConstants<float>::halfPi, true);
    g.strokePath(shackle, juce::PathStrokeType(1.2f));
}
} // namespace

//==============================================================================
// The piano roll.
class PatternEditor::Grid final : public juce::Component {
public:
    explicit Grid(PatternEditor& o) : owner(o) { setWantsKeyboardFocus(false); }

    double firstStep = 0, stepWidth = 12;
    int firstRow = 0, rowHeight = 14;

    int visibleRows() const { return std::max(1, getHeight() / rowHeight); }
    int visibleSteps() const { return std::max(1, int((getWidth() - keyWidth) / stepWidth)); }
    float xForStep(double step) const { return float(keyWidth + (step - firstStep) * stepWidth); }
    double stepAt(float x) const { return firstStep + double(x - keyWidth) / stepWidth; }
    // Rows are listed top (highest) first.
    int rowAt(float y) const { return std::clamp(firstRow + int(std::floor(y / float(rowHeight))), 0, int(owner.rows.size()) - 1); }
    float yForRow(int row) const { return float((row - firstRow) * rowHeight); }

    int rowOf(const pat::Note& n) const {
        const auto& rows = owner.rows;
        if (owner.isKit()) {
            for (std::size_t i = 0; i < rows.size(); ++i)
                if (rows[i].degree == n.degree) return int(i);
            return -1;
        }
        if (owner.chromatic) {
            const int semis = music::degree(owner.mood(), n.degree) + n.alter;
            for (std::size_t i = 0; i < rows.size(); ++i)
                if (music::degree(owner.mood(), rows[i].degree) + rows[i].alter == semis) return int(i);
            return -1;
        }
        for (std::size_t i = 0; i < rows.size(); ++i)
            if (rows[i].degree == n.degree) return int(i);
        return -1;
    }

    juce::Rectangle<float> noteBounds(const pat::Note& n) const {
        const int row = rowOf(n);
        if (row < 0) return {};
        return { xForStep(n.step) + 0.5f, yForRow(row) + 1.0f, float(std::max(2.0, double(n.length) * stepWidth - 1.0)), float(rowHeight - 2) };
    }

    // The note under a point, and whether the point is on its right edge (resize).
    int noteAt(juce::Point<float> p, bool& edge) const {
        const auto& pattern = owner.working.pattern;
        for (int i = pattern.count - 1; i >= 0; --i) {
            const auto b = noteBounds(pattern.notes[std::size_t(i)]);
            if (b.expanded(0, 0.5f).contains(p)) {
                edge = p.x > b.getRight() - std::min(6.0f, b.getWidth() * 0.4f);
                return i;
            }
        }
        edge = false;
        return -1;
    }

    void paint(juce::Graphics& g) override {
        const auto& pattern = owner.working.pattern;
        const auto accent = accentFor(owner.type());
        const auto area = getLocalBounds().toFloat();
        g.setColour(colours::background.darker(0.25f));
        g.fillRoundedRectangle(area, 6.0f);

        const auto& rows = owner.rows;
        const int perBar = pat::stepsPerBar(pattern.resolution);
        const double perBeat = pat::stepsPerBeat(pattern.resolution);
        const float right = float(getWidth());
        // Rows: tonic rows lit in brass, out-of-scale rows (12 TONE) darker.
        for (int r = firstRow; r < std::min(int(rows.size()), firstRow + visibleRows() + 1); ++r) {
            const float y = yForRow(r);
            const auto& row = rows[std::size_t(r)];
            auto fill = (r & 1) ? colours::panel.darker(0.2f) : colours::panel.darker(0.35f);
            if (!row.inScale) fill = fill.darker(0.5f);
            if (row.tonic) fill = fill.interpolatedWith(colours::brass, 0.12f);
            g.setColour(fill);
            g.fillRect(float(keyWidth), y, right - float(keyWidth), float(rowHeight));
            // The key column.
            g.setColour(row.tonic ? colours::brass.withAlpha(0.3f) : (row.inScale ? colours::panel.brighter(0.08f) : colours::background));
            g.fillRect(0.0f, y, float(keyWidth - 4), float(rowHeight) - 1.0f);
            g.setColour(row.tonic ? colours::parchment : (row.inScale ? colours::parchment.withAlpha(0.75f) : colours::dim));
            g.setFont(font(std::min(11.0f, float(rowHeight) - 2.0f), row.tonic));
            g.drawText(row.label, juce::Rectangle<float>(4.0f, y, float(keyWidth - 10), float(rowHeight)), juce::Justification::centredLeft, false);
        }
        // Columns: steps, beats, bars (brass).
        const int first = int(std::floor(firstStep)), last = first + visibleSteps() + 1;
        for (int s = std::max(0, first); s <= std::min(last, int(pattern.lengthSteps)); ++s) {
            const float x = xForStep(s);
            const bool bar = s % perBar == 0, beat = std::abs(std::fmod(double(s), perBeat)) < 1.0e-9;
            g.setColour(bar ? colours::brass.withAlpha(0.55f) : (beat ? colours::panelEdge.brighter(0.2f) : colours::panelEdge.withAlpha(0.45f)));
            g.drawVerticalLine(int(x), 0.0f, float(getHeight()));
            if (bar && s < pattern.lengthSteps) {
                g.setColour(colours::brass.withAlpha(0.8f));
                g.setFont(font(9.5f, true));
                g.drawText(juce::String(s / perBar + 1), juce::Rectangle<float>(x + 3.0f, 1.0f, 30.0f, 11.0f), juce::Justification::centredLeft, false);
            }
        }
        // Beyond the end of the pattern.
        const float end = xForStep(pattern.lengthSteps);
        if (end < right) {
            g.setColour(colours::background.withAlpha(0.7f));
            g.fillRect(end, 0.0f, right - end, float(getHeight()));
        }

        // Notes: opacity is probability, brightness velocity.
        g.saveState();
        g.reduceClipRegion(keyWidth, 0, getWidth() - keyWidth, getHeight());
        for (int i = 0; i < pattern.count; ++i) {
            const auto& n = pattern.notes[std::size_t(i)];
            const auto b = noteBounds(n);
            if (b.isEmpty()) continue;
            const float alpha = 0.25f + 0.75f * float(n.probability) / 100.0f;
            const float v = float(n.velocity) / 127.0f;
            const auto fill = accent.withMultipliedBrightness(0.6f + 0.5f * v).withAlpha(alpha);
            g.setGradientFill(juce::ColourGradient(fill.brighter(0.3f), b.getX(), b.getY(), fill.darker(0.3f), b.getX(), b.getBottom(), false));
            g.fillRoundedRectangle(b, 3.0f);
            g.setColour(isSelected(n) ? colours::cyan : colours::parchment.withAlpha(0.35f + 0.4f * alpha));
            g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, isSelected(n) ? 1.6f : 1.0f);
            if (n.alter != 0 && !owner.chromatic && !owner.isKit()) {
                g.setColour(colours::background);
                g.setFont(font(9.0f, true));
                g.drawText(n.alter > 0 ? "#" : "b", b.withTrimmedLeft(2.0f), juce::Justification::centredLeft, false);
            }
            if (n.locked() && b.getWidth() > 7.0f) drawLock(g, b.withSizeKeepingCentre(7.0f, 8.0f).withX(b.getRight() - 9.0f), colours::background);
        }
        if (band) {
            g.setColour(colours::cyan.withAlpha(0.12f));
            g.fillRect(bandRect);
            g.setColour(colours::cyan.withAlpha(0.7f));
            g.drawRect(bandRect, 1.0f);
        }

        // The playhead.
        const double beat = owner.processor.engine.slotBeat[std::size_t(std::max(0, owner.slotIndex))].load(std::memory_order_relaxed);
        if (pattern.lengthSteps > 0) {
            const double steps = beat * perBeat;
            const double pos = steps - std::floor(steps / pattern.lengthSteps) * pattern.lengthSteps;
            const float x = xForStep(pos);
            const bool rolling = owner.processor.engine.transportRolling.load(std::memory_order_relaxed);
            const auto c = colours::cyan.withAlpha(rolling ? 0.95f : 0.45f);
            g.setGradientFill(juce::ColourGradient(c.withAlpha(0.0f), x - 10.0f, 0.0f, c.withAlpha(rolling ? 0.25f : 0.1f), x, 0.0f, false));
            g.fillRect(x - 10.0f, 0.0f, 10.0f, float(getHeight()));
            g.setColour(c);
            g.drawVerticalLine(int(x), 0.0f, float(getHeight()));
        }
        g.restoreState();
        g.setColour(colours::panelEdge);
        g.drawRoundedRectangle(area.reduced(0.5f), 6.0f, 1.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        owner.grabKeyboardFocus();
        if (e.x < keyWidth || owner.rows.empty()) return;
        auto& pattern = owner.working.pattern;
        bool edge = false;
        const int hit = noteAt(e.position, edge);
        const int step = std::clamp(int(std::floor(stepAt(e.position.x))), 0, std::max(0, int(pattern.lengthSteps) - 1));
        const int row = rowAt(e.position.y);
        owner.cursorStep = step;
        mode = None;
        if (e.mods.isPopupMenu() || e.mods.isAltDown()) {
            if (hit >= 0) {
                owner.pushUndo();
                owner.beginEdit();
                owner.working.pattern.remove(hit);
                owner.commit();
            }
            return;
        }
        if (hit >= 0 && e.getNumberOfClicks() >= 2) {
            owner.pushUndo();
            owner.beginEdit();
            auto& n = owner.working.pattern.notes[std::size_t(hit)];
            n.setLocked(!n.locked());
            owner.commit();
            return;
        }
        anchorStep = stepAt(e.position.x);
        anchorRow = row;
        if (hit >= 0) {
            owner.pushUndo();
            owner.beginEdit();
            auto& n = owner.working.pattern.notes[std::size_t(hit)];
            if (e.mods.isShiftDown()) setSelected(n, !isSelected(n));
            else if (!isSelected(n)) {
                owner.selectAll(false);
                setSelected(owner.working.pattern.notes[std::size_t(hit)], true);
            }
            mode = edge ? Resize : Move;
            origin = owner.working.pattern;
            moved = false;
            owner.lastVelocity = owner.working.pattern.notes[std::size_t(hit)].velocity;
            repaint();
            return;
        }
        if (owner.drawTool) {
            owner.pushUndo();
            owner.beginEdit();
            owner.selectAll(false);
            pat::Note n;
            n.step = int16_t(step);
            n.length = int16_t(owner.isKit() ? 1 : owner.lastLength);
            n.degree = int8_t(owner.rows[std::size_t(row)].degree);
            n.alter = int8_t(owner.rows[std::size_t(row)].alter);
            n.velocity = uint8_t(owner.lastVelocity);
            setSelected(n, true);
            owner.working.pattern.add(n);
            owner.commit();
            mode = Resize;
            origin = owner.working.pattern;
            anchorStep = double(step + n.length);
            moved = true;
            return;
        }
        if (!e.mods.isShiftDown()) owner.selectAll(false);
        mode = Band;
        band = true;
        bandStart = e.position;
        bandRect = {};
        repaint();
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (mode == Move || mode == Resize) {
            const int dSteps = int(std::lround(stepAt(e.position.x) - anchorStep));
            const int dRows = rowAt(e.position.y) - anchorRow;
            auto& p = owner.working.pattern;
            p = origin;
            const int rowCount = int(owner.rows.size());
            for (int i = 0; i < p.count; ++i) {
                auto& n = p.notes[std::size_t(i)];
                if (!isSelected(n)) continue;
                if (mode == Move) {
                    n.step = int16_t(std::clamp(int(n.step) + dSteps, 0, int(p.lengthSteps) - 1));
                    const int r = rowOf(n);
                    if (r >= 0 && dRows != 0) {
                        const auto& to = owner.rows[std::size_t(std::clamp(r + dRows, 0, rowCount - 1))];
                        n.degree = int8_t(to.degree);
                        n.alter = int8_t(to.alter);
                    }
                } else {
                    n.length = int16_t(std::clamp(int(n.length) + dSteps, 1, int(p.lengthSteps)));
                    owner.lastLength = n.length;
                }
            }
            moved = moved || dSteps != 0 || dRows != 0;
            owner.commit();
        } else if (mode == Band) {
            bandRect = juce::Rectangle<float>(bandStart, e.position);
            auto& p = owner.working.pattern;
            for (int i = 0; i < p.count; ++i) {
                auto& n = p.notes[std::size_t(i)];
                if (noteBounds(n).intersects(bandRect)) setSelected(n, true);
                else if (!e.mods.isShiftDown()) setSelected(n, false);
            }
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent&) override {
        if ((mode == Move || mode == Resize) && !moved && !owner.undoStack.empty()) owner.undoStack.pop_back();   // a click only selected
        mode = None;
        band = false;
        repaint();
    }

    void mouseMove(const juce::MouseEvent& e) override {
        bool edge = false;
        const int hit = noteAt(e.position, edge);
        setMouseCursor(hit >= 0 ? (edge ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::DraggingHandCursor)
                                : (owner.drawTool ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor));
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override {
        if (e.mods.isCommandDown() || e.mods.isCtrlDown()) {
            stepWidth = std::clamp(stepWidth * (w.deltaY > 0 ? 1.15 : 1.0 / 1.15), 4.0, 48.0);
            owner.resized();
        } else if (e.mods.isShiftDown() || std::abs(w.deltaX) > std::abs(w.deltaY)) {
            owner.hScroll.setCurrentRangeStart(owner.hScroll.getCurrentRangeStart() - (w.deltaX != 0 ? w.deltaX : w.deltaY) * 24.0);
        } else {
            owner.vScroll.setCurrentRangeStart(owner.vScroll.getCurrentRangeStart() - w.deltaY * 6.0);
        }
    }

private:
    enum Mode { None, Move, Resize, Band };
    PatternEditor& owner;
    Mode mode = None;
    pat::Pattern origin;
    double anchorStep = 0;
    int anchorRow = 0;
    bool moved = false, band = false;
    juce::Point<float> bandStart;
    juce::Rectangle<float> bandRect;
};

//==============================================================================
// Velocity or probability lane under the grid.
class PatternEditor::Lane final : public juce::Component {
public:
    Lane(PatternEditor& o, bool velocity) : owner(o), isVelocity(velocity) {}

    void paint(juce::Graphics& g) override {
        const auto area = getLocalBounds().toFloat();
        g.setColour(colours::background.darker(0.25f));
        g.fillRoundedRectangle(area, 5.0f);
        g.setColour(colours::brass.withAlpha(0.8f));
        g.setFont(font(9.5f, true));
        g.drawText(isVelocity ? "VELOCITY" : "CHANCE", juce::Rectangle<float>(4.0f, 0.0f, float(keyWidth - 8), area.getHeight()),
                   juce::Justification::centredLeft, false);
        const auto& grid = *owner.gridView;
        const auto& p = owner.working.pattern;
        const auto accent = accentFor(owner.type());
        g.saveState();
        g.reduceClipRegion(keyWidth, 0, getWidth() - keyWidth, getHeight());
        const float h = area.getHeight() - 4.0f;
        for (int i = 0; i < p.count; ++i) {
            const auto& n = p.notes[std::size_t(i)];
            const float value = isVelocity ? float(n.velocity) / 127.0f : float(n.probability) / 100.0f;
            const float x = grid.xForStep(n.step);
            const float w = float(std::max(3.0, std::min(grid.stepWidth - 2.0, 8.0)));
            const auto bar = juce::Rectangle<float>(x + 1.0f, 2.0f + h * (1.0f - value), w, h * value);
            g.setColour((isSelected(n) ? colours::cyan : accent).withAlpha(0.85f));
            g.fillRoundedRectangle(bar, 1.5f);
            g.fillEllipse(juce::Rectangle<float>(w + 2.0f, w + 2.0f).withCentre({ bar.getCentreX(), bar.getY() }));
        }
        g.restoreState();
        g.setColour(colours::panelEdge);
        g.drawRoundedRectangle(area.reduced(0.5f), 5.0f, 1.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        owner.pushUndo();
        owner.beginEdit();
        apply(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override { apply(e); }

private:
    void apply(const juce::MouseEvent& e) {
        if (e.x < keyWidth) return;
        auto& p = owner.working.pattern;
        const int step = int(std::floor(owner.gridView->stepAt(e.position.x)));
        const float value = juce::jlimit(0.0f, 1.0f, 1.0f - (e.position.y - 2.0f) / float(getHeight() - 4));
        bool anySelectedHere = false;
        for (int i = 0; i < p.count; ++i) anySelectedHere |= p.notes[std::size_t(i)].step == step && isSelected(p.notes[std::size_t(i)]);
        bool changed = false;
        for (int i = 0; i < p.count; ++i) {
            auto& n = p.notes[std::size_t(i)];
            if (n.step != step || (anySelectedHere && !isSelected(n))) continue;
            if (isVelocity) {
                n.velocity = uint8_t(juce::jlimit(1, 127, juce::roundToInt(value * 127.0f)));
                owner.lastVelocity = n.velocity;
            } else {
                n.probability = uint8_t(juce::jlimit(0, 100, juce::roundToInt(value * 100.0f)));
            }
            changed = true;
        }
        if (changed) owner.commit();
    }

    PatternEditor& owner;
    bool isVelocity;
};

//==============================================================================
// A tab per instrument: its glyph and name, and whether it is muted or soloed.
class PatternEditor::ElementTab final : public juce::Button {
public:
    ElementTab(PatternEditor& o, int e) : juce::Button("tab"), owner(o), element(e) {}
    void paintButton(juce::Graphics& g, bool highlighted, bool) override {
        const auto r = getLocalBounds().toFloat().reduced(0.5f);
        const auto accent = accentFor(owner.type());
        const bool on = getToggleState();
        const auto controls = owner.processor.patterns.element(owner.slotIndex, owner.type(), element).controls;
        // A tab: raised, lit in the Place's colour when selected.
        const SurfaceState st { highlighted, false, on, true };
        drawSurface(g, r.reduced(1.0f), 5.0f, colours::panel.brighter(0.06f), accent, st);
        const auto ink = on ? inkFor(st, accent) : (controls.mute ? colours::dim : colours::parchment);
        auto inner = r.reduced(5.0f, 3.0f);
        drawInstrumentIcon(g, placeInfo(owner.type()).element[std::size_t(element)].icon, inner.removeFromLeft(16.0f), ink);
        inner.removeFromLeft(4.0f);
        g.setColour(ink);
        g.setFont(font(10.0f, true));
        g.drawFittedText(placeInfo(owner.type()).element[std::size_t(element)].name, inner.toNearestInt(), juce::Justification::centredLeft, 2, 0.8f);
        if (controls.mute || controls.solo) {
            const auto badge = juce::Rectangle<float>(11.0f, 11.0f).withPosition(r.getRight() - 12.0f, r.getY() + 1.0f);
            g.setColour(controls.solo ? colours::cyan : colours::danger);
            g.fillEllipse(badge);
            g.setColour(colours::background);
            g.setFont(font(8.0f, true));
            g.drawText(controls.solo ? "S" : "M", badge, juce::Justification::centred, false);
        }
    }

private:
    PatternEditor& owner;
    int element;
};

//==============================================================================
// Drag a pattern out into the host as a MIDI clip.
class PatternEditor::DragOut final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit DragOut(PatternEditor& o) : owner(o) {
        setTooltip("Drag this into a track to place the pattern as a MIDI region (where the host accepts dropped files). "
                   "Otherwise use SAVE MIDI and import the file.");
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
    void paint(juce::Graphics& g) override {
        const auto r = getLocalBounds().toFloat().reduced(2.0f);
        drawSurface(g, r, r.getHeight() * 0.5f, colours::panel.interpolatedWith(colours::cyan, 0.18f), colours::cyan, SurfaceState { isMouseOver(), dragging, false, true });
        g.setFont(font(10.5f, true));
        g.setColour(colours::parchment);
        const float w = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), "DRAG MIDI");
        auto row = juce::Rectangle<float>(w + 18.0f, r.getHeight()).withCentre(r.getCentre());
        g.drawText("DRAG MIDI", row.removeFromLeft(w), juce::Justification::centred, false);
        drawIcon(g, Icon::DragOut, row.withTrimmedLeft(4.0f), colours::cyan.brighter(0.3f));
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (dragging || e.getDistanceFromDragStart() < 5) return;
        dragging = true;
        const auto file = owner.tempMidiFile(owner.elementIndex);
        if (owner.saveMidi(file, owner.elementIndex))
            juce::DragAndDropContainer::performExternalDragDropOfFiles({ file.getFullPathName() }, false, this, [this] { dragging = false; });
        else
            dragging = false;
    }
    void mouseUp(const juce::MouseEvent&) override { dragging = false; }
    void mouseEnter(const juce::MouseEvent&) override { repaint(); }
    void mouseExit(const juce::MouseEvent&) override { repaint(); }

private:
    PatternEditor& owner;
    bool dragging = false;
};

//==============================================================================
PatternEditor::PatternEditor(WorldbuilderProcessor& p) : processor(p) {
    setWantsKeyboardFocus(true);
    gridView = std::make_unique<Grid>(*this);
    velocityLane = std::make_unique<Lane>(*this, true);
    probabilityLane = std::make_unique<Lane>(*this, false);
    dragOut = std::make_unique<DragOut>(*this);
    addAndMakeVisible(*gridView);
    addAndMakeVisible(*velocityLane);
    addAndMakeVisible(*probabilityLane);
    addAndMakeVisible(*dragOut);
    for (auto* bar : { &hScroll, &vScroll }) {
        bar->setAutoHide(false);
        bar->setColour(juce::ScrollBar::thumbColourId, colours::brass.withAlpha(0.5f));
        addAndMakeVisible(*bar);
    }
    hScroll.addListener(this);
    vScroll.addListener(this);

    for (auto* b : { &draw, &select }) b->setColour(juce::TextButton::buttonOnColourId, colours::brass);
    // One icon per tool, the same set as the rest of the interface.
    for (auto [b, icon] : { std::pair<juce::Component*, Icon> { &back, Icon::Back }, { &draw, Icon::Draw }, { &select, Icon::Select }, { &undoButton, Icon::Undo },
                            { &redoButton, Icon::Redo }, { &copy, Icon::Copy }, { &pasteButton, Icon::Paste }, { &clear, Icon::Clear },
                            { &generateButton, Icon::Generate }, { &up, Icon::Up }, { &down, Icon::Down }, { &twice, Icon::Double }, { &half, Icon::Half },
                            { &lock, Icon::Lock }, { &twelve, Icon::Tone12 }, { &follow, Icon::Chords }, { &snapButton, Icon::Snap },
                            { &importButton, Icon::Import }, { &saveButton, Icon::Save } })
        setIcon(*b, icon);
    for (int e = 0; e < placeElements; ++e) {
        tabs[std::size_t(e)] = std::make_unique<ElementTab>(*this, e);
        auto& tab = *tabs[std::size_t(e)];
        tab.setClickingTogglesState(false);
        tab.onClick = [this, e] { selectElement(e); };
        addChildComponent(tab);
    }
    // The instrument's own strip: its level and switch (host parameters), mute and solo.
    elementLevel.setSliderStyle(juce::Slider::LinearHorizontal);
    elementLevel.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    elementLevel.setDoubleClickReturnValue(true, 0.8);
    elementLevel.setTooltip("This instrument's level");
    addAndMakeVisible(elementLevel);
    {
        // The instrument's effects row.
        const auto combo = [this](juce::ComboBox& box, const char* choices, const char* tip) {
            box.addItemList(juce::StringArray::fromTokens(choices, "|", ""), 1);
            box.setTooltip(tip);
            addAndMakeVisible(box);
        };
        combo(reverbType, reverbChoices, "Which shared reverb this instrument sends to: Room, Hall, Cathedral, Plate or Cave");
        combo(delayType, delayChoices, "Which shared delay this instrument sends to (times follow the host tempo): Tape (a quarter note, warm and "
                                       "wobbling), Ping-Pong (eighths bouncing left and right), Dotted (a dotted eighth), Reverse (each beat backwards)");
        combo(characterType, characterFxChoices, "Character: Off, Saturation, Chorus, Phaser, Filter (a resonant low-pass) or Lo-Fi");
        const auto knob = [this](juce::Slider& k, const char* tip) {
            k.setSliderStyle(juce::Slider::LinearHorizontal);
            k.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            k.setTooltip(tip);
            k.onValueChange = [this] { repaint(fxArea); };
            addAndMakeVisible(k);
        };
        knob(reverbSend, "Reverb send");
        knob(delaySend, "Delay send");
        knob(characterAmount, "How much of the character effect");
        knob(fxTone, "Tone: dark to bright (the middle leaves it as it is)");
        knob(fxWidth, "Width: mono to extra wide (the middle leaves it as it is)");
    }
    elementOn.setColour(juce::TextButton::buttonOnColourId, colours::brass);
    elementOn.setTooltip("Switch this instrument on or off (automatable)");
    addAndMakeVisible(elementOn);
    muteButton.setColour(juce::TextButton::buttonOnColourId, colours::danger);
    muteButton.setTooltip("Mute this instrument (saved with the project)");
    muteButton.onClick = [this] { setMuteSolo(muteButton.getToggleState(), soloButton.getToggleState()); };
    addAndMakeVisible(muteButton);
    soloButton.setColour(juce::TextButton::buttonOnColourId, colours::cyan);
    soloButton.setTooltip("Hear this instrument alone within its Place (other solos add to it)");
    soloButton.onClick = [this] { setMuteSolo(muteButton.getToggleState(), soloButton.getToggleState()); };
    addAndMakeVisible(soloButton);
    back.setTooltip("Back to the Place's controls");
    back.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(back);

    const auto button = [this](juce::Button& b, const char* tip, std::function<void()> f) {
        b.setTooltip(tip);
        b.onClick = std::move(f);
        addAndMakeVisible(b);
    };
    button(draw, "Draw: click to add a note, drag to lengthen it. Alt-click or right-click deletes; double-click locks.", [this] {
        drawTool = true;
        refreshControls();
    });
    button(select, "Select: drag a box around notes, then move, lengthen, transpose, copy or lock them.", [this] {
        drawTool = false;
        refreshControls();
    });
    button(undoButton, "Undo (Cmd/Ctrl-Z)", [this] { undo(); });
    button(redoButton, "Redo (Shift-Cmd/Ctrl-Z)", [this] { redo(); });
    button(copy, "Copy the selected notes (Cmd/Ctrl-C)", [this] { copySelection(); });
    button(pasteButton, "Paste at the last clicked step (Cmd/Ctrl-V)", [this] { paste(); });
    button(clear, "Remove every note", [this] {
        pushUndo();
        beginEdit();
        working.pattern.clear();
        commit();
    });
    button(generateButton, "A fresh pattern from the Place's generator (its Seed and settings). It follows the Place's knobs until you edit it.",
           [this] { generate(); });
    button(up, "Transpose the selection (or everything) up a scale step (a semitone in 12 TONE)", [this] { transpose(1); });
    button(down, "Transpose the selection (or everything) down", [this] { transpose(-1); });
    button(twice, "Double the length: the notes repeat in the new half", [this] { resizeLength(true); });
    button(half, "Halve the length", [this] { resizeLength(false); });
    button(lock, "Lock the selected notes: randomness and Evolve never change them (L)", [this] { toggleLock(); });
    for (auto* t : { &twelve, &follow, &snapButton }) {
        t->setColour(juce::TextButton::buttonOnColourId, colours::cyan);
        addAndMakeVisible(*t);
    }
    twelve.setTooltip("Show all twelve semitones, not only the scale's notes");
    twelve.onClick = [this] {
        chromatic = twelve.getToggleState();
        rebuildRows();
        resized();
    };
    follow.setTooltip("Follow the chords: degrees count from the root of the shared progression's current chord. Off: from the key's root.");
    follow.onClick = [this] { setFollowChords(follow.getToggleState()); };
    snapButton.setTooltip("Imported MIDI notes snap to the current scale. Off: they keep their exact pitches.");
    snapButton.setToggleState(true, juce::dontSendNotification);
    snapButton.onClick = [this] { snap = snapButton.getToggleState(); };

    for (int b = 1; b <= pat::maxBars; ++b) length.addItem(juce::String(b) + (b == 1 ? " bar" : " bars"), b);
    length.addItem("Free", 99);
    length.setTooltip("Pattern length: 1-8 bars, or a free number of steps");
    length.onChange = [this] {
        if (updating) return;
        const int id = length.getSelectedId();
        setGrid(working.pattern.resolution, id == 99 ? 0 : id, int(steps.getValue()));
    };
    for (int r = 0; r < pat::ResolutionCount; ++r) grid.addItem(pat::resolutionNames[std::size_t(r)], r + 1);
    grid.setTooltip("Step resolution");
    grid.onChange = [this] {
        if (!updating) setGrid(grid.getSelectedId() - 1, working.pattern.bars, working.pattern.lengthSteps);
    };
    returnEvery.addItem("Never", 1);
    for (std::size_t i = 1; i < pat::returnChoices.size(); ++i)
        returnEvery.addItem("Every " + juce::String(pat::returnChoices[i]) + (pat::returnChoices[i] == 1 ? " bar" : " bars"), int(i) + 1);
    returnEvery.setTooltip("Evolve returns to the original pattern every N bars");
    returnEvery.onChange = [this] {
        if (updating) return;
        working.controls.returnBars = uint8_t(pat::returnChoices[std::size_t(std::clamp(returnEvery.getSelectedId() - 1, 0, 5))]);
        commit();
    };
    for (auto* c : { &length, &grid, &returnEvery }) addAndMakeVisible(*c);

    const auto slider = [this](juce::Slider& s, double lo, double hi, const char* tip) {
        s.setSliderStyle(juce::Slider::LinearHorizontal);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setRange(lo, hi, 0.0);
        s.setTooltip(tip);
        addAndMakeVisible(s);
    };
    slider(steps, 1, 128, "Free length, in steps");
    steps.setRange(1, 128, 1);
    steps.setSliderStyle(juce::Slider::LinearBar);
    steps.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 40, 18);
    steps.onValueChange = [this] {
        if (!updating && working.pattern.bars == 0) setGrid(working.pattern.resolution, 0, int(steps.getValue()));
    };
    slider(swing, 0, 1, "Swing: delays the off-beat steps");
    swing.onDragStart = [this] { pushUndo(); };
    swing.onValueChange = [this] {
        if (updating) return;
        beginEdit();
        working.pattern.swing = float(swing.getValue());
        commit();
    };
    slider(randomness, 0, 1, "Randomness: 0 % plays the pattern exactly; higher lets each note change, only in the ways switched on to the right. "
                              "The Place's Variation knob scales it (as set at its default).");
    randomness.onValueChange = [this] {
        if (updating) return;
        working.controls.randomness = float(randomness.getValue());
        commit();
    };
    slider(evolve, 0, 1, "Evolve: the pattern slowly mutates over the bars (in the ways switched on), and can return to the original");
    evolve.onValueChange = [this] {
        if (updating) return;
        working.controls.evolve = float(evolve.getValue());
        commit();
    };
    static constexpr std::array<const char*, 5> varyNames { "T", "V", "P", "S", "+" };
    static constexpr std::array<const char*, 5> varyTips { "Timing may vary (notes come a little late)", "Velocity may vary",
                                                           "Pitch may vary, within the scale", "Notes may be skipped", "Extra notes may be added" };
    for (int k = 0; k < 5; ++k) {
        auto& b = vary[std::size_t(k)];
        b.setButtonText(varyNames[std::size_t(k)]);
        b.setTooltip(varyTips[std::size_t(k)]);
        b.setColour(juce::TextButton::buttonOnColourId, colours::brass);
        b.onClick = [this, k] {
            const auto bit = uint8_t(1 << k);
            working.controls.vary = uint8_t(vary[std::size_t(k)].getToggleState() ? (working.controls.vary | bit) : (working.controls.vary & ~bit));
            commit();
        };
        addAndMakeVisible(b);
    }
    button(importButton, "Import a MIDI file into this element (you can also drop a .mid file on the editor or on a tab)", [this] {
        chooser = std::make_unique<juce::FileChooser>("Import a MIDI pattern", juce::File(), "*.mid;*.midi");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& c) {
            if (c.getResult().existsAsFile()) importMidi(c.getResult(), elementIndex, snap);
        });
    });
    button(saveButton, "Save this pattern as a MIDI file", [this] {
        const auto name = juce::String(placeNames[std::size_t(type())]) + " " + placeInfo(type()).element[std::size_t(std::max(0, elementIndex))].name + ".mid";
        chooser = std::make_unique<juce::FileChooser>("Save the pattern as MIDI", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(name),
                                                      "*.mid");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this](const juce::FileChooser& c) {
            if (c.getResult() != juce::File()) saveMidi(c.getResult().withFileExtension("mid"), elementIndex);
        });
    });
}

PatternEditor::~PatternEditor() {
    stopTimer();
    hScroll.removeListener(this);
    vScroll.removeListener(this);
}

void PatternEditor::scrollBarMoved(juce::ScrollBar* bar, double start) {
    if (bar == &hScroll) gridView->firstStep = start;
    else gridView->firstRow = int(start);
    gridView->repaint();
    velocityLane->repaint();
    probabilityLane->repaint();
}

int PatternEditor::type() const {
    if (slotIndex < 0) return NoPlace;
    return juce::roundToInt(processor.parameters.getRawParameterValue(specs[std::size_t(placeParam(slotIndex, PlType))].id)->load());
}
bool PatternEditor::isKit() const { return patternInfo(type(), elementIndex).kind == Kit; }
int PatternEditor::mood() const { return juce::roundToInt(processor.parameters.getRawParameterValue("mood")->load()); }
int PatternEditor::root() const { return juce::roundToInt(processor.parameters.getRawParameterValue("root")->load()); }

void PatternEditor::open(int slot, int element) {
    slotIndex = slot;
    shownType = type();
    int first = -1;
    for (int e = 0; e < placeElements; ++e)
        if (patternInfo(shownType, e).kind != NoPattern && first < 0) first = e;
    undoStack.clear();
    redoStack.clear();
    layoutTabs();
    selectElement(element >= 0 && patternInfo(shownType, element).kind != NoPattern ? element : first);
    startTimerHz(30);
}

void PatternEditor::selectElement(int element) {
    if (element < 0 || patternInfo(type(), element).kind == NoPattern) return;
    elementIndex = element;
    undoStack.clear();
    redoStack.clear();
    for (int e = 0; e < placeElements; ++e) tabs[std::size_t(e)]->setToggleState(e == element, juce::dontSendNotification);
    // Attach the instrument's strip to its parameters.
    levelAttachment.reset();
    onAttachment.reset();
    if (slotIndex >= 0) {
        levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.parameters, specs[std::size_t(placeParam(slotIndex, PlElement + element))].id, elementLevel);
        onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.parameters, specs[std::size_t(placeParam(slotIndex, PlElementOn + element))].id, elementOn);
    }
    attachEffects(element);
    refreshFromStore();
    // Fit the pattern, and show the rows around its notes.
    const int count = working.pattern.lengthSteps;
    const int width = std::max(100, gridView->getWidth() - keyWidth);
    gridView->stepWidth = std::clamp(double(width) / double(std::max(1, count)), 8.0, 40.0);
    gridView->firstStep = 0;
    resized();
    int top = int(rows.size()) / 3;
    if (working.pattern.count > 0) {
        int highest = int(rows.size());
        for (int i = 0; i < working.pattern.count; ++i) highest = std::min(highest, std::max(0, gridView->rowOf(working.pattern.notes[std::size_t(i)])));
        top = std::max(0, highest - 2);
    }
    vScroll.setCurrentRangeStart(std::min(top, std::max(0, int(rows.size()) - gridView->visibleRows())));
    repaint();
}

void PatternEditor::attachEffects(int element) {
    fxCombos.clear();
    fxSliders.clear();
    if (slotIndex < 0 || element < 0) return;
    const auto id = [&](int field) { return specs[std::size_t(placeParam(slotIndex, fxField(element, field)))].id; };
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    for (auto [box, field] : { std::pair<juce::ComboBox*, int> { &reverbType, FxReverbType }, { &delayType, FxDelayType }, { &characterType, FxCharacter } })
        fxCombos.push_back(std::make_unique<ComboAttachment>(processor.parameters, id(field), *box));
    const int t = type();
    for (auto [slider, field] : { std::pair<juce::Slider*, int> { &reverbSend, FxReverb }, { &delaySend, FxDelay }, { &characterAmount, FxAmount },
                                  { &fxTone, FxTone }, { &fxWidth, FxWidth } }) {
        fxSliders.push_back(std::make_unique<SliderAttachment>(processor.parameters, id(field), *slider));
        // Double-click: this instrument's own default.
        slider->setDoubleClickReturnValue(true, instrumentFxDefault(t, element, field));
    }
}

void PatternEditor::layoutTabs() {
    for (int e = 0; e < placeElements; ++e) {
        auto& tab = *tabs[std::size_t(e)];
        tab.setVisible(patternInfo(shownType, e).kind != NoPattern);
        tab.setButtonText(placeInfo(shownType).element[std::size_t(e)].name);
        tab.setTooltip("Edit the " + juce::String(placeInfo(shownType).element[std::size_t(e)].name) + " pattern. Drop a MIDI file here to import it.");
    }
    resized();
}

void PatternEditor::refreshFromStore() {
    if (slotIndex < 0 || elementIndex < 0) return;
    const int t = type();
    working = processor.patterns.element(slotIndex, t, elementIndex);
    if (!working.controls.edited) {
        working.pattern = processor.patterns.playing(slotIndex, t, elementIndex);
        working.pattern.swing = processor.parameters.getRawParameterValue(specs[std::size_t(placeParam(slotIndex, PlSwing))].id)->load();
    }
    lastGeneratedSerial = processor.patterns.generated(slotIndex).serial;
    rebuildRows();
    refreshControls();
}

void PatternEditor::refreshControls() {
    const juce::ScopedValueSetter<bool> guard(updating, true);
    const auto& p = working.pattern;
    muteButton.setToggleState(working.controls.mute, juce::dontSendNotification);
    soloButton.setToggleState(working.controls.solo, juce::dontSendNotification);
    length.setSelectedId(p.bars == 0 ? 99 : p.bars, juce::dontSendNotification);
    grid.setSelectedId(p.resolution + 1, juce::dontSendNotification);
    steps.setValue(p.lengthSteps, juce::dontSendNotification);
    steps.setEnabled(p.bars == 0);
    swing.setValue(p.swing, juce::dontSendNotification);
    randomness.setValue(working.controls.randomness, juce::dontSendNotification);
    evolve.setValue(working.controls.evolve, juce::dontSendNotification);
    int ret = 1;
    for (std::size_t i = 0; i < pat::returnChoices.size(); ++i)
        if (pat::returnChoices[i] == working.controls.returnBars) ret = int(i) + 1;
    returnEvery.setSelectedId(ret, juce::dontSendNotification);
    for (int k = 0; k < 5; ++k) vary[std::size_t(k)].setToggleState((working.controls.vary & (1 << k)) != 0, juce::dontSendNotification);
    follow.setToggleState(p.followChords, juce::dontSendNotification);
    follow.setEnabled(!isKit());
    twelve.setEnabled(!isKit());
    vary[2].setEnabled(!isKit());
    draw.setToggleState(drawTool, juce::dontSendNotification);
    select.setToggleState(!drawTool, juce::dontSendNotification);
    undoButton.setEnabled(!undoStack.empty());
    redoButton.setEnabled(!redoStack.empty());
    pasteButton.setEnabled(!clipboard.empty());
    repaint(statusArea);
}

void PatternEditor::rebuildRows() {
    rows.clear();
    const int t = type();
    const auto& info = patternInfo(t, elementIndex);
    if (info.kind == Kit) {
        for (int r = info.rows - 1; r >= 0; --r) rows.push_back({ r, 0, info.kit[std::size_t(r)], r == 0, true });
    } else {
        const int m = mood(), size = music::scaleFor(m).size;
        const auto& p = working.pattern;
        int lo = -size, hi = 2 * size;
        for (int i = 0; i < p.count; ++i) {
            lo = std::min(lo, int(p.notes[std::size_t(i)].degree) - 2);
            hi = std::max(hi, int(p.notes[std::size_t(i)].degree) + 2);
        }
        const auto label = [&](int degree, int alter) {
            // Names on the tonic chord (rows follow the chords when CHORDS is on).
            const int semis = pat::semitonesFor(p, m, 0, degree, alter);
            return juce::MidiMessage::getMidiNoteName(48 + root() + semis, true, true, 3);
        };
        if (chromatic) {
            for (int semis = music::degree(m, hi); semis >= music::degree(m, lo); --semis) {
                int d = 0, a = 0;
                pat::degreeFor(m, semis, false, d, a);
                rows.push_back({ d, a, label(d, a), a == 0 && music::floorDiv(d, size) * size == d, a == 0 });
            }
        } else {
            for (int d = hi; d >= lo; --d) rows.push_back({ d, 0, label(d, 0), music::floorDiv(d, size) * size == d, true });
        }
    }
}

//==============================================================================
void PatternEditor::pushUndo() {
    undoStack.push_back({ working });
    if (undoStack.size() > 100) undoStack.erase(undoStack.begin());
    redoStack.clear();
}

void PatternEditor::beginEdit() {
    if (!working.controls.edited) {
        working.controls.edited = true;
        working.pattern.swing = processor.parameters.getRawParameterValue(specs[std::size_t(placeParam(slotIndex, PlSwing))].id)->load();
    }
}

void PatternEditor::commit() {
    if (slotIndex < 0 || elementIndex < 0) return;
    working.pattern.normalise();
    auto published = working;
    for (int i = 0; i < published.pattern.count; ++i) published.pattern.notes[std::size_t(i)].flags &= uint8_t(~selectedFlag);
    processor.patterns.setElement(slotIndex, type(), elementIndex, published);
    rebuildRows();
    refreshControls();
    resized();
    repaint();
}

void PatternEditor::undo() {
    if (undoStack.empty()) return;
    redoStack.push_back({ working });
    working = undoStack.back().element;
    undoStack.pop_back();
    commit();
}

void PatternEditor::redo() {
    if (redoStack.empty()) return;
    undoStack.push_back({ working });
    working = redoStack.back().element;
    redoStack.pop_back();
    commit();
}

bool PatternEditor::anySelected() const {
    for (int i = 0; i < working.pattern.count; ++i)
        if (isSelected(working.pattern.notes[std::size_t(i)])) return true;
    return false;
}

void PatternEditor::selectAll(bool on) {
    for (int i = 0; i < working.pattern.count; ++i) setSelected(working.pattern.notes[std::size_t(i)], on);
    repaint();
}

void PatternEditor::copySelection() {
    clipboard.clear();
    int first = INT_MAX;
    for (int i = 0; i < working.pattern.count; ++i)
        if (isSelected(working.pattern.notes[std::size_t(i)])) first = std::min(first, int(working.pattern.notes[std::size_t(i)].step));
    for (int i = 0; i < working.pattern.count; ++i) {
        auto n = working.pattern.notes[std::size_t(i)];
        if (!isSelected(n)) continue;
        n.step = int16_t(n.step - first);
        clipboard.push_back(n);
    }
    refreshControls();
}

void PatternEditor::paste() {
    if (clipboard.empty()) return;
    pushUndo();
    beginEdit();
    selectAll(false);
    for (auto n : clipboard) {
        n.step = int16_t(n.step + cursorStep);
        setSelected(n, true);
        working.pattern.add(n);
    }
    commit();
}

void PatternEditor::deleteSelection() {
    if (!anySelected()) return;
    pushUndo();
    beginEdit();
    for (int i = working.pattern.count - 1; i >= 0; --i)
        if (isSelected(working.pattern.notes[std::size_t(i)])) working.pattern.remove(i);
    commit();
}

void PatternEditor::transpose(int direction) {
    pushUndo();
    beginEdit();
    const bool all = !anySelected();
    const auto& info = patternInfo(type(), elementIndex);
    for (int i = 0; i < working.pattern.count; ++i) {
        auto& n = working.pattern.notes[std::size_t(i)];
        if (!all && !isSelected(n)) continue;
        if (info.kind == Kit) {
            n.degree = int8_t(std::clamp(n.degree + direction, 0, info.rows - 1));
        } else if (chromatic) {
            int d = 0, a = 0;
            pat::degreeFor(mood(), music::degree(mood(), n.degree) + n.alter + direction, false, d, a);
            n.degree = int8_t(std::clamp(d, -40, 40));
            n.alter = int8_t(a);
        } else {
            n.degree = int8_t(std::clamp(n.degree + direction, -40, 40));
        }
    }
    commit();
}

void PatternEditor::resizeLength(bool doubleIt) {
    pushUndo();
    beginEdit();
    auto& p = working.pattern;
    const int oldLength = p.lengthSteps;
    if (doubleIt) {
        const int bars = p.bars > 0 ? std::min(int(pat::maxBars), p.bars * 2) : 0;
        const int newLength = bars > 0 ? bars * pat::stepsPerBar(p.resolution) : std::min(pat::maxSteps, oldLength * 2);
        if (newLength == oldLength) return;
        const int count = p.count;
        p.setGrid(p.resolution, bars, newLength);
        for (int i = 0; i < count; ++i) {
            auto n = p.notes[std::size_t(i)];
            for (int s = int(n.step) + oldLength; s < p.lengthSteps; s += oldLength) {
                auto copy = n;
                copy.step = int16_t(s);
                p.add(copy);
            }
        }
    } else {
        // One bar halves into a free length of half a bar.
        p.setGrid(p.resolution, p.bars > 1 ? p.bars / 2 : 0, std::max(1, oldLength / 2));
    }
    commit();
}

void PatternEditor::toggleLock() {
    if (!anySelected()) return;
    pushUndo();
    beginEdit();
    bool allLocked = true;
    for (int i = 0; i < working.pattern.count; ++i)
        if (isSelected(working.pattern.notes[std::size_t(i)])) allLocked &= working.pattern.notes[std::size_t(i)].locked();
    for (int i = 0; i < working.pattern.count; ++i)
        if (isSelected(working.pattern.notes[std::size_t(i)])) working.pattern.notes[std::size_t(i)].setLocked(!allLocked);
    commit();
}

void PatternEditor::setGrid(int resolution, int bars, int stepCount) {
    pushUndo();
    beginEdit();
    auto& p = working.pattern;
    const int oldPerBar = pat::stepsPerBar(p.resolution), newPerBar = pat::stepsPerBar(resolution);
    if (oldPerBar != newPerBar) {
        // Keep the notes where they are in time.
        for (int i = 0; i < p.count; ++i) {
            auto& n = p.notes[std::size_t(i)];
            n.step = int16_t(std::lround(double(n.step) * newPerBar / oldPerBar));
            n.length = int16_t(std::max(1L, std::lround(double(n.length) * newPerBar / oldPerBar)));
        }
        if (bars == 0) stepCount = int(std::lround(double(stepCount) * newPerBar / oldPerBar));
    }
    p.setGrid(resolution, bars, stepCount);
    commit();
}

void PatternEditor::setFollowChords(bool on) {
    if (on == working.pattern.followChords) return;
    pushUndo();
    beginEdit();
    auto& p = working.pattern;
    // Keep the notes sounding the same on the tonic chord.
    const int size = music::scaleFor(mood()).size;
    const int octaves = music::inRange(0, p.base) / 12;
    for (int i = 0; i < p.count; ++i) {
        auto& n = p.notes[std::size_t(i)];
        n.degree = int8_t(std::clamp(int(n.degree) + (on ? -1 : 1) * octaves * size, -40, 40));
    }
    p.followChords = on;
    commit();
}

void PatternEditor::generate() {
    if (slotIndex < 0 || elementIndex < 0) return;
    pushUndo();
    working.controls.variant = uint8_t(working.controls.variant + 1);
    working.controls.edited = false;
    commit();
}

void PatternEditor::addNote(int step, int degree, int noteLength, int velocity) {
    pushUndo();
    beginEdit();
    pat::Note n;
    n.step = int16_t(step);
    n.degree = int8_t(degree);
    n.length = int16_t(noteLength);
    n.velocity = uint8_t(velocity);
    working.pattern.add(n);
    commit();
}

//==============================================================================
juce::File PatternEditor::tempMidiFile(int element) const {
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("WORLDBUILDER");
    dir.createDirectory();
    const auto name = juce::String(placeNames[std::size_t(type())]) + " " + placeInfo(type()).element[std::size_t(std::clamp(element, 0, placeElements - 1))].name;
    return dir.getChildFile(juce::File::createLegalFileName(name) + ".mid");
}

bool PatternEditor::saveMidi(const juce::File& file, int element) {
    if (slotIndex < 0 || element < 0) return false;
    const int t = type();
    const auto pattern = element == elementIndex ? working.pattern : processor.patterns.playing(slotIndex, t, element);
    pat::MidiTarget target;
    target.root = root();
    target.mood = mood();
    target.kit = patternInfo(t, element).kind == Kit;
    target.kitRows = patternInfo(t, element).rows;
    target.bpm = processor.engine.frame().bpm;
    target.danger = processor.parameters.getRawParameterValue("danger")->load();
    const auto midi = pat::toMidi(pattern, target);
    file.deleteFile();
    juce::FileOutputStream out(file);
    if (!out.openedOk()) return false;
    const bool ok = midi.writeTo(out);
    out.flush();
    return ok;
}

bool PatternEditor::importMidi(const juce::File& file, int element, bool snapToScale) {
    if (slotIndex < 0 || element < 0 || patternInfo(type(), element).kind == NoPattern) return false;
    juce::FileInputStream in(file);
    juce::MidiFile midi;
    if (!in.openedOk() || !midi.readFrom(in)) return false;
    const int t = type();
    pat::MidiTarget target;
    target.root = root();
    target.mood = mood();
    target.kit = patternInfo(t, element).kind == Kit;
    target.kitRows = patternInfo(t, element).rows;
    target.bpm = processor.engine.frame().bpm;
    if (element != elementIndex) selectElement(element);
    auto p = working.pattern;
    if (pat::fromMidi(midi, p, target, snapToScale, working.controls.edited ? working.pattern.resolution : pat::Sixteenth) <= 0) return false;
    pushUndo();
    beginEdit();
    working.pattern = p;
    commit();
    selectElement(element);
    return true;
}

bool PatternEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    for (const auto& f : files)
        if (f.endsWithIgnoreCase(".mid") || f.endsWithIgnoreCase(".midi")) return true;
    return false;
}

void PatternEditor::filesDropped(const juce::StringArray& files, int x, int y) {
    int target = elementIndex;
    for (int e = 0; e < placeElements; ++e)
        if (tabs[std::size_t(e)]->isVisible() && tabs[std::size_t(e)]->getBounds().contains(x, y)) target = e;
    for (const auto& f : files)
        if (f.endsWithIgnoreCase(".mid") || f.endsWithIgnoreCase(".midi")) {
            importMidi(juce::File(f), target, snap);
            return;
        }
}

bool PatternEditor::keyPressed(const juce::KeyPress& key) {
    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) { deleteSelection(); return true; }
    if (mods.isCommandDown()) {
        const auto c = juce::CharacterFunctions::toLowerCase(key.getTextCharacter() != 0 ? key.getTextCharacter() : juce::juce_wchar(code));
        if (c == 'z') { mods.isShiftDown() ? redo() : undo(); return true; }
        if (c == 'y') { redo(); return true; }
        if (c == 'c') { copySelection(); return true; }
        if (c == 'v') { paste(); return true; }
        if (c == 'a') { selectAll(true); return true; }
    }
    if (code == juce::KeyPress::upKey) { transpose(1); return true; }
    if (code == juce::KeyPress::downKey) { transpose(-1); return true; }
    if (key.getTextCharacter() == 'l' || key.getTextCharacter() == 'L') { toggleLock(); return true; }
    return false;
}

void PatternEditor::timerCallback() {
    if (!isShowing() || slotIndex < 0) return;
    const int t = type();
    if (t != shownType) {
        if (t == NoPlace || !hasPatterns(t)) {
            if (onClose) onClose();
            return;
        }
        open(slotIndex);
    }
    // A generated pattern follows the Place's knobs: show the newest.
    if (!working.controls.edited && processor.patterns.generated(slotIndex).serial != lastGeneratedSerial) {
        refreshFromStore();
        resized();
    }
    gridView->repaint();
}

void PatternEditor::setMuteSolo(bool mute, bool solo) {
    if (slotIndex < 0 || elementIndex < 0) return;
    const int t = type();
    auto el = processor.patterns.element(slotIndex, t, elementIndex);
    el.controls.mute = mute;
    el.controls.solo = solo;
    processor.patterns.setElement(slotIndex, t, elementIndex, el);
    working.controls.mute = mute;
    working.controls.solo = solo;
    for (auto& tab : tabs) tab->repaint();
}

//==============================================================================
void PatternEditor::resized() {
    auto r = getLocalBounds().reduced(14, 10);
    titleArea = r.removeFromTop(40);
    {
        auto t = titleArea;
        back.setBounds(t.removeFromRight(70).withSizeKeepingCentre(66, 24));
        t.removeFromRight(10);
        t.removeFromLeft(190);   // icon and title
        int visible = 0;
        for (const auto& tab : tabs) visible += tab->isVisible() ? 1 : 0;
        const int w = std::min(100, t.getWidth() / std::max(visible, 1));
        for (int e = placeElements - 1; e >= 0; --e) {
            auto& tab = *tabs[std::size_t(e)];
            if (!tab.isVisible()) continue;
            tab.setBounds(t.removeFromRight(w).withSizeKeepingCentre(w - 4, 30));
        }
    }
    r.removeFromTop(4);
    elementArea = r.removeFromTop(24);
    {
        auto t = elementArea.withTrimmedLeft(200);
        soloButton.setBounds(t.removeFromRight(56).reduced(2, 1));
        muteButton.setBounds(t.removeFromRight(60).reduced(2, 1));
        elementOn.setBounds(t.removeFromRight(48).reduced(2, 1));
        t.removeFromRight(8);
        t.removeFromLeft(44);   // "LEVEL"
        elementLevel.setBounds(t.reduced(0, 4));
    }
    r.removeFromTop(2);
    fxArea = r.removeFromTop(36);
    {
        // EFFECTS | REVERB [type][send] | DELAY [type][send] | CHARACTER [type][amount] | TONE | WIDTH,
        // labels above (as in the settings rows).
        auto t = fxArea.withTrimmedLeft(64).withTrimmedTop(13);
        const int slider = std::clamp((t.getWidth() - 3 * 92 - 4 * 14) / 5, 50, 96);
        const auto place = [&t](juce::Component& c, int w) { c.setBounds(t.removeFromLeft(w).reduced(2, 1)); };
        place(reverbType, 92);
        place(reverbSend, slider);
        t.removeFromLeft(14);
        place(delayType, 92);
        place(delaySend, slider);
        t.removeFromLeft(14);
        place(characterType, 92);
        place(characterAmount, slider);
        t.removeFromLeft(14);
        place(fxTone, slider);
        t.removeFromLeft(14);
        place(fxWidth, slider);
    }
    r.removeFromTop(4);
    toolbarArea = r.removeFromTop(28);
    {
        auto t = toolbarArea;
        const auto place = [&t](juce::Component& c, int w) { c.setBounds(t.removeFromLeft(w).reduced(2, 2)); };
        place(draw, 58);
        place(select, 64);
        t.removeFromLeft(10);
        place(undoButton, 54);
        place(redoButton, 54);
        t.removeFromLeft(10);
        place(copy, 52);
        place(pasteButton, 56);
        place(clear, 56);
        t.removeFromLeft(10);
        place(up, 40);
        place(down, 54);
        place(twice, 36);
        place(half, 36);
        place(lock, 50);
        generateButton.setBounds(t.removeFromRight(92).reduced(2, 2));
    }
    r.removeFromTop(4);
    settingsArea = r.removeFromTop(84);
    {
        // The pattern's grid on the left, MIDI on the right; randomness below.
        auto rowA = settingsArea.removeFromTop(42).withTrimmedTop(14);
        auto rowB = settingsArea.withTrimmedTop(14);
        const auto place = [](juce::Rectangle<int>& t, juce::Component& c, int w) {
            c.setBounds(t.removeFromLeft(w).reduced(2, 1));
            t.removeFromLeft(6);
        };
        place(rowA, length, 80);
        place(rowA, steps, 66);
        place(rowA, grid, 70);
        place(rowA, swing, 84);
        place(rowA, twelve, 76);
        place(rowA, follow, 76);
        dragOut->setBounds(rowA.removeFromRight(100).reduced(0, 1));
        rowA.removeFromRight(6);
        saveButton.setBounds(rowA.removeFromRight(78).reduced(1, 1));
        importButton.setBounds(rowA.removeFromRight(64).reduced(1, 1));
        snapButton.setBounds(rowA.removeFromRight(60).reduced(1, 1));
        place(rowB, randomness, 150);
        for (auto& v : vary) v.setBounds(rowB.removeFromLeft(28).reduced(1, 1));
        rowB.removeFromLeft(14);
        place(rowB, evolve, 130);
        place(rowB, returnEvery, 110);
    }
    r.removeFromTop(6);
    statusArea = r.removeFromBottom(16);
    r.removeFromBottom(2);
    auto lanes = r.removeFromBottom(30);
    probabilityLane->setBounds(lanes.withTrimmedRight(12));
    r.removeFromBottom(3);
    velocityLane->setBounds(r.removeFromBottom(44).withTrimmedRight(12));
    r.removeFromBottom(3);
    hScroll.setBounds(r.removeFromBottom(10).withTrimmedLeft(keyWidth).withTrimmedRight(12));
    r.removeFromBottom(2);
    vScroll.setBounds(r.removeFromRight(10));
    r.removeFromRight(2);
    // Rows: as tall as fits, between 10 and 22 px.
    const int count = std::max(1, int(rows.size()));
    gridView->rowHeight = std::clamp(r.getHeight() / count, isKit() ? 22 : 12, isKit() ? 96 : 18);
    gridView->setBounds(r);

    const int total = working.pattern.lengthSteps;
    hScroll.setRangeLimits(0.0, double(std::max(total, 1)), juce::dontSendNotification);
    hScroll.setCurrentRange(gridView->firstStep, double(gridView->visibleSteps()), juce::dontSendNotification);
    gridView->firstStep = hScroll.getCurrentRangeStart();
    vScroll.setRangeLimits(0.0, double(rows.size()), juce::dontSendNotification);
    vScroll.setCurrentRange(double(gridView->firstRow), double(gridView->visibleRows()), juce::dontSendNotification);
    gridView->firstRow = int(vScroll.getCurrentRangeStart());
}

void PatternEditor::paint(juce::Graphics& g) {
    const int t = type();
    const auto accent = accentFor(t);
    const auto area = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(colours::panel.brighter(0.04f), 0.0f, 0.0f, colours::panel.darker(0.3f), 0.0f, area.getHeight(), false));
    g.fillRoundedRectangle(area, 10.0f);
    g.setColour(accent.withAlpha(0.7f));
    g.drawRoundedRectangle(area.reduced(0.5f), 10.0f, 1.2f);

    auto title = titleArea;
    drawPlaceIcon(g, t, title.removeFromLeft(40).toFloat(), colours::parchment, accent.withAlpha(0.6f));
    title.removeFromLeft(8);
    g.setColour(colours::parchment);
    g.setFont(font(19.0f, true).withExtraKerningFactor(0.12f));
    g.drawText(juce::String(placeNames[std::size_t(std::clamp(t, 0, PlaceTypeCount - 1))]).toUpperCase(), title.removeFromTop(24), juce::Justification::centredLeft,
               false);
    g.setColour(colours::brass);
    g.setFont(font(10.5f, true).withExtraKerningFactor(0.18f));
    g.drawText("PATTERNS  -  PLACE " + juce::String(slotIndex + 1), title, juce::Justification::centredLeft, false);

    g.setColour(colours::brass.withAlpha(0.85f));
    g.setFont(font(9.5f, true).withExtraKerningFactor(0.12f));
    const auto label = [&](juce::Component& c, const char* text) {
        g.drawText(text, c.getBounds().translated(0, -13).withHeight(12).withWidth(140), juce::Justification::centredLeft, false);
    };
    label(length, "LENGTH");
    label(steps, "STEPS");
    label(grid, "GRID");
    label(swing, "SWING");
    g.drawText("RANDOMNESS " + juce::String(juce::roundToInt(working.controls.randomness * 100.0f)) + "%",
               randomness.getBounds().translated(0, -13).withHeight(12).withWidth(140), juce::Justification::centredLeft, false);
    g.drawText("MAY VARY", vary[0].getBounds().translated(0, -13).withHeight(12).withWidth(130), juce::Justification::centredLeft, false);
    g.drawText("EVOLVE " + juce::String(juce::roundToInt(working.controls.evolve * 100.0f)) + "%",
               evolve.getBounds().translated(0, -13).withHeight(12).withWidth(140), juce::Justification::centredLeft, false);
    label(returnEvery, "RETURN");
    {
        // The instrument strip: glyph and name, then LEVEL.
        auto strip = elementArea;
        const auto& spec = placeInfo(t).element[std::size_t(std::clamp(elementIndex, 0, placeElements - 1))];
        drawInstrumentIcon(g, spec.icon, strip.removeFromLeft(20).toFloat().reduced(1.0f), accent);
        strip.removeFromLeft(6);
        g.setColour(colours::parchment);
        g.setFont(font(12.0f, true));
        g.drawText(spec.name, strip.removeFromLeft(170), juce::Justification::centredLeft, true);
        g.setColour(colours::brass.withAlpha(0.85f));
        g.setFont(font(9.5f, true).withExtraKerningFactor(0.12f));
        g.drawText("LEVEL", elementLevel.getBounds().withX(elementLevel.getX() - 44).withWidth(40), juce::Justification::centredLeft, false);
    }
    label(snapButton, "MIDI FILES");
    {
        // The effects row's labels, above each control (sliders with their values).
        g.setColour(colours::brass.withAlpha(0.85f));
        g.setFont(font(9.5f, true).withExtraKerningFactor(0.12f));
        g.drawText("EFFECTS", fxArea.withWidth(60).withTrimmedTop(13), juce::Justification::centredLeft, false);
        const auto above = [&](juce::Component& c, const juce::String& text) {
            g.drawText(text, c.getBounds().translated(0, -13).withHeight(12).withWidth(c.getWidth() + 30), juce::Justification::centredLeft, false);
        };
        const auto percent = [](const juce::Slider& k) { return juce::String(juce::roundToInt(k.getValue() * 100.0)) + "%"; };
        above(reverbType, "REVERB");
        above(reverbSend, "SEND " + percent(reverbSend));
        above(delayType, "DELAY");
        above(delaySend, "SEND " + percent(delaySend));
        above(characterType, "CHARACTER");
        above(characterAmount, "AMOUNT " + percent(characterAmount));
        const int tone = juce::roundToInt((fxTone.getValue() - 0.5) * 200.0);
        above(fxTone, "TONE " + juce::String(tone == 0 ? "FLAT" : (tone < 0 ? "DARK " : "BRIGHT ") + juce::String(std::abs(tone))));
        above(fxWidth, "WIDTH " + percent(fxWidth));
        g.setColour(colours::panelEdge.withAlpha(0.5f));
        g.drawHorizontalLine(fxArea.getBottom() + 1, float(fxArea.getX()), float(fxArea.getRight()));
    }

    // Status: generated or edited, and what the rows mean.
    g.setFont(font(10.5f));
    juce::String status;
    if (working.controls.edited)
        status = "Edited: this pattern plays as written (with its Randomness), whatever the Place's knobs do. GENERATE replaces it.";
    else
        status = "Generated from the Place's Seed: it follows Character, Activity, Density, Tension and Life until you edit it.";
    if (!isKit() && working.pattern.followChords) status += "  Rows follow the chords (named on the tonic chord).";
    g.setColour(working.controls.edited ? colours::parchment.withAlpha(0.85f) : colours::dim);
    g.drawText(status, statusArea, juce::Justification::centredLeft, true);
    g.setColour(colours::dim);
    g.drawText(juce::String(working.pattern.count) + " notes", statusArea, juce::Justification::centredRight, false);
}

} // namespace wb::ui
