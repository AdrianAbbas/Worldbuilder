#pragma once
// The Pattern Editor: a piano roll for one Place's patterned elements (a tab per
// element). Rows are the scale notes of the current Root and Mood (or all twelve
// semitones; or the drums of a kit), columns are steps. Velocity and probability
// lanes below. Tools: draw, select, copy and paste, clear, generate, transpose,
// double and halve, lock, undo and redo. MIDI files drop in and drag out.
//
// Until the composer edits it, an element shows the pattern its Place generates
// (which follows the Place's knobs). The first edit keeps a copy, which then stays
// as written.
#include "../PluginProcessor.h"
#include "KeysPanel.h"
#include "WorldLookAndFeel.h"

namespace wb::ui {

class PatternEditor final : public juce::Component, public juce::FileDragAndDropTarget, private juce::Timer, private juce::ScrollBar::Listener {
public:
    explicit PatternEditor(WorldbuilderProcessor& processor);
    ~PatternEditor() override;

    // Shows a slot's patterns, on the first patterned element (or `element`).
    void open(int slot, int element = -1);
    int slot() const { return slotIndex; }
    int element() const { return elementIndex; }
    void selectElement(int element);
    // The pattern variant shown and edited: A, B, C or the Fill (Milestone 11).
    void selectVariant(int variant);
    int variant() const { return variantIndex; }
    std::function<void()> onClose;

    // Also used by the tests.
    bool importMidi(const juce::File& file, int element, bool snap);
    bool saveMidi(const juce::File& file, int element);
    void generate();
    void addNote(int step, int degree, int length = 1, int velocity = 100);
    const pat::Element& shown() const { return working; }
    KeysPanel& keysPanel() { return *keys; }

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    class Grid;
    class Lane;
    class DragOut;
    class ElementTab;
    struct Row {
        int degree = 0, alter = 0;
        juce::String label;
        bool tonic = false, inScale = true;
    };

    void timerCallback() override;
    void scrollBarMoved(juce::ScrollBar*, double) override;
    void refreshFromStore();
    void refreshControls();
    void layoutTabs();
    void rebuildRows();
    int type() const;
    bool isKit() const;
    int mood() const;
    int root() const;

    // Editing: the first edit copies the generated pattern; each edit can be undone.
    void beginEdit();
    void commit();
    void pushUndo();
    void undo();
    void redo();
    void copySelection();
    void paste();
    void deleteSelection();
    void transpose(int direction);
    void resizeLength(bool doubleIt);
    void toggleLock();
    void selectAll(bool on);
    bool anySelected() const;
    void setGrid(int resolution, int bars, int steps);
    void setFollowChords(bool on);
    juce::File tempMidiFile(int element) const;

    // One variant of an element as the editor works on it (its pattern in `pattern`, its
    // edited flag and Generate count in the controls), and back into the element.
    static pat::Element viewOf(const pat::Element& element, int variant);
    static void mergeInto(pat::Element& element, const pat::Element& view, int variant);

    WorldbuilderProcessor& processor;
    int slotIndex = -1, elementIndex = -1, shownType = -1, variantIndex = 0;
    std::array<juce::TextButton, pat::variantCount> variantTabs;
    pat::Element working;
    bool chromatic = false, snap = true;
    int cursorStep = 0, lastLength = 2, lastVelocity = 100;
    bool drawTool = true;

    struct Snapshot {
        pat::Element element;
    };
    std::vector<Snapshot> undoStack, redoStack;
    std::vector<pat::Note> clipboard;
    std::vector<Row> rows;

    std::array<std::unique_ptr<ElementTab>, placeElements> tabs;
    juce::Slider elementLevel;
    juce::ToggleButton elementOn { "ON" }, muteButton { "MUTE" }, soloButton { "SOLO" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;
    juce::Rectangle<int> elementArea;
    // The instrument's own effects: reverb and delay sends (shared buses), a character
    // effect, tone and width.
    juce::ComboBox reverbType, delayType, characterType;
    juce::Slider reverbSend, delaySend, characterAmount, fxTone, fxWidth;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> fxCombos;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> fxSliders;
    juce::Rectangle<int> fxArea;
    void attachEffects(int element);
    void setMuteSolo(bool mute, bool solo);
    juce::TextButton back { "BACK" }, draw { "DRAW" }, select { "SELECT" }, undoButton { "UNDO" }, redoButton { "REDO" };
    juce::TextButton copy { "COPY" }, pasteButton { "PASTE" }, clear { "CLEAR" }, generateButton { "GENERATE" };
    juce::TextButton up { "UP" }, down { "DOWN" }, twice { "x2" }, half { "/2" }, lock { "LOCK" };
    juce::ToggleButton twelve { "12 TONE" }, follow { "CHORDS" }, snapButton { "SNAP" };
    juce::ComboBox length, grid, returnEvery;
    juce::Slider steps, swing, randomness, evolve;
    std::array<juce::ToggleButton, 5> vary;
    juce::TextButton importButton { "IMPORT" }, saveButton { "SAVE MIDI" };
    std::unique_ptr<DragOut> dragOut;
    std::unique_ptr<KeysPanel> keys;   // KEYS (Milestone 14)
    juce::Rectangle<int> keysArea;
    std::unique_ptr<Grid> gridView;
    std::unique_ptr<Lane> velocityLane, probabilityLane;
    juce::ScrollBar hScroll { false }, vScroll { true };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> titleArea, toolbarArea, settingsArea, statusArea;
    bool updating = false;
    uint32_t lastGeneratedSerial = 0;
};

} // namespace wb::ui
