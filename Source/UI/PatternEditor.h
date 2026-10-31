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
//
// Milestone 16: the same editor edits an arrangement section's clip (CLIP: the
// section's own notes for the instrument, at the section's real bars in the song; its
// transport, playhead and loop inside the clip; REC records into it) or one
// instrument's clips across the whole arrangement as one roll (SONG: one pass of every
// section, its boundaries marked). Editing a clip that is still linked to its variant
// asks once: make it unique (this section only) or edit the variant (everywhere it is
// linked).
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
    // Milestone 16: a section's clip for an instrument, and one instrument's whole song.
    void openClip(int slot, int element, int section);
    void openSong(int slot, int element, int section = -1);
    enum class View { Pattern, Clip, Song };
    View view() const { return viewMode; }
    int section() const { return clipSection; }
    // The linked-clip question: waiting for an answer (the edit shows, unpublished), and
    // the answer: 0 cancel (the edit is undone), 1 make unique, 2 edit the variant.
    bool linkedPending() const { return pendingLinked; }
    void resolveLinked(int choice);
    // The notes shown (pattern, clip or song view), for the tests.
    const pat::Long& notesShown() const { return *work; }
    // The song view's sections: where each starts in the view and in the song (beats).
    struct Segment {
        int section = 0;
        double viewStart = 0, songStart = 0, beats = 0;
        bool linked = true;
    };
    const std::vector<Segment>& songSegments() const { return segments; }
    // View beats (0 = the first step shown) to song beats and back (-1: not shown).
    double toSong(double viewBeat, bool end = false) const;
    double toView(double songBeat) const;
    // A region's edge (no wrapping; `end`: the end of a section counts as in it).
    double toViewEdge(double songBeat, bool end = false) const;
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
    // The transport (the hotfix after Milestone 14): Play, Stop, Loop and the bar ruler.
    juce::TextButton& playControl() { return playButton; }
    juce::TextButton& stopControl() { return stopButton; }
    juce::TextButton& loopControl() { return loopButton; }
    juce::Component& rulerView();
    // The grid's horizontal zoom (px per step) and first step shown, for the tests.
    double gridStepWidth() const;
    double gridFirstStep() const;

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
    class Ruler;
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
    // The transport's loop follows the pattern's length until the composer drags it; its
    // positions stay inside the pattern.
    void syncTransport();
public:
    // Zooms the grid around an x position (px in the grid).
    void zoomAround(float x, double factor);
    // Rolling: the grid scrolls to keep the playhead in view (the editor's timer calls it).
    void followPlayhead();
private:


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

    // The notes in a compact form (undo, drags): a long view is 160 kB.
    struct NotesSnap {
        uint8_t resolution = 0, bars = 0;
        int16_t lengthSteps = 0;
        bool followChords = true;
        int8_t base = 0;
        float swing = 0;
        std::vector<pat::Note> notes;
    };
    static NotesSnap captureNotes(const pat::Long& p);
    static void restoreNotes(pat::Long& p, const NotesSnap& s);
    struct Snapshot {
        pat::Element element;
        NotesSnap notes;
    };
    std::unique_ptr<pat::Long> work = std::make_unique<pat::Long>();
    pat::Long& notes() { return *work; }
    const pat::Long& notes() const { return *work; }
    View viewMode = View::Pattern;
    int clipSection = -1;
    bool clipOwnSwing = false;    // the clip shown swings by its own Swing
    std::vector<Segment> segments;
    std::vector<arr::ClipRef> songBase;   // each section's clip as the song view read it
    bool pendingLinked = false, storeUndoPending = true, uniqueConfirmed = false;
    uint32_t shownArrangement = 0;
    arr::ClipRef clipShown;                 // the clip view: the clip as read
    std::vector<arr::ClipRef> songShown;    // the song view: each section as shown (on its grid)
    arr::ClipRef segmentClip(int k) const;
    void setupClipControls();
    void open(int slot, int element, bool);
    NotesSnap beforeLinked;
    juce::TextButton uniqueButton { "MAKE UNIQUE" }, variantButton { "EDIT VARIANT" }, cancelLinked { "CANCEL" };
    juce::TextButton songButton { "SONG" }, clipUnique { "UNIQUE" }, clipRelink { "RE-LINK" };
    juce::Rectangle<int> linkedArea;
    void loadClip();
    void loadSong();
    void publishClip(bool undoable);
    void publishSong(bool undoable);
    bool clipIsLinked() const;
    void updateModeControls();
    juce::String barLabel(int viewBar) const;
    double viewOrigin() const;
    // The arrangement as the store has it (copied again only when it changes).
    const arr::Arrangement& arrangementNow() const;
    mutable arr::Arrangement cachedArrangement;
    mutable uint32_t cachedRevision = 0;
    mutable bool cachedValid = false;
    float placeSwing() const;
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
    // Scoop / Glide (in the instrument's strip, after LEVEL).
    juce::ComboBox slideType;
    juce::Slider slideAmount, slideTime;
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
    std::unique_ptr<Ruler> ruler;
    juce::TextButton playButton { "PLAY" }, stopButton { "STOP" }, loopButton { "LOOP" };
    std::unique_ptr<Lane> velocityLane, probabilityLane;
    juce::ScrollBar hScroll { false }, vScroll { true };
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> titleArea, toolbarArea, settingsArea, statusArea;
    bool updating = false;
    uint32_t lastGeneratedSerial = 0;
};

} // namespace wb::ui
