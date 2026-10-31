#pragma once
// The arrangement as the composer edits it (message thread), with undo and redo.
// Every change is published to the audio thread through the engine's lock-free
// buffer. It saves with the plugin state (and .world files) as compact text.
#include "Arrangement.h"
#include "../Patterns/Pattern.h"
#include <juce_data_structures/juce_data_structures.h>
#include <vector>

namespace wb {

class ArrangementStore {
public:
    explicit ArrangementStore(pat::TripleBuffer<arr::Arrangement>& audio) : out(audio) { publish(); }

    arr::Arrangement get() const;
    // Replaces the arrangement and publishes it; `undoable` keeps the previous one for
    // undo (a drag passes false after its first step, so it undoes in one go).
    void set(const arr::Arrangement& a, bool undoable = true);
    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;
    // Bumped by every change (the interface repaints when it moves).
    uint32_t revision() const;

    // Edits, each one undoable. Indices are clamped; the return value is the section
    // the edit leaves selected.
    int add(const arr::Section& s, int at = -1);
    int remove(int index);
    int duplicate(int index);
    int move(int from, int to);
    void resize(int index, int bars, bool undoable = true);
    // Milestone 16: a section's own clip for an instrument (nullptr: linked to its
    // variant again). Undoable unless `undoable` is false (a drag or a take after its
    // first step).
    void setClip(int section, int slot, int element, arr::ClipRef clip, bool undoable = true);
    arr::ClipRef clipOf(int section, int slot, int element) const;
    // A clip fitted to `bars` bars: shorter, the notes after the end go; longer, the
    // clip repeats into the new bars (as its section would have repeated it).
    static arr::ClipRef conform(const arr::ClipRef& clip, int bars);
    // What a linked cell plays, as a clip: the pattern laid on the song from its bar 1
    // (as a linked instrument plays it), the part under the section's first pass
    // (`startBar`, `bars` bars). `ownSwing`: the pattern was edited (its own Swing).
    static arr::ClipRef fromPattern(const pat::Pattern& p, int startBar, int bars, bool ownSwing);

    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& tree);
    static juce::ValueTree toTree(const arr::Arrangement& a);
    static arr::Arrangement fromTree(const juce::ValueTree& tree);
    static juce::String encodeCells(const arr::Grid& cells);
    static void decodeCells(const juce::String& text, arr::Grid& cells);
    static void writeClip(juce::ValueTree& x, const pat::Clip& c);
    static arr::ClipRef readClip(const juce::ValueTree& x, int bars);

private:
    void publish();   // with the lock held
    void apply(const arr::Arrangement& a, bool undoable);

    pat::TripleBuffer<arr::Arrangement>& out;
    juce::CriticalSection lock;
    arr::Arrangement model {};
    std::vector<arr::Arrangement> undoStack, redoStack;
    uint32_t changes = 0;
    static constexpr std::size_t undoLimit = 100;
};

} // namespace wb
