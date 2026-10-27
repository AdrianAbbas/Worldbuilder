#pragma once
// REC (Milestone 14): turns what the audio thread captured (Capture, in Keys.h) into
// pattern notes, on the message thread.
//
// A take writes into one element's pattern: Replace (a new pattern, from the take's
// first note: a take with no notes changes nothing), Overdub (notes added to the
// pattern as it is, generated or edited) or Into variant (a new pattern in variant A,
// B, C or the Fill). Notes land on the pattern's steps, quantised on the way in (the
// grid and a strength that keeps some of the player's timing); a new pattern uses the
// finest grid (1/32, or 1/32T for a triplet quantise), is fixed in the key (it does not
// follow the chords) and has no swing. Its length is the pattern's, or Auto: whole bars
// covering the take, at most eight. Notes from the song keep their place in it: a note
// played at bar 9 of the song plays at bar 9 again (the pattern repeats from bar 1).
// With a punch region, Replace keeps the pattern (its grid and the notes outside the
// region) and replaces only the notes inside it.
//
// While the take is under way its notes carry pat::Note::takeFlag, so the pattern does
// not play them again on top of what is being played; they appear in the Pattern Editor
// at once. One undo removes a whole take; quantising afterwards is undone the same way.
#include "Keys.h"
#include "../Patterns/PatternStore.h"
#include <vector>

namespace wb::keys {

struct RecordSettings {
    int mode = Replace, variant = 0, quantise = QuantiseOff;
    float strength = 1.0f;
    bool autoLength = false;
    // Punch (the Pattern Editor's transport): Replace and Into variant replace only the
    // notes starting inside [punchIn, punchOut) beats and keep the rest of the pattern.
    bool punch = false;
    double punchIn = 0, punchOut = 0;
};

class Recorder {
public:
    Recorder(PatternStore& s, Capture& c) : store(s), capture(c) {}

    // Takes what the audio thread captured into the patterns (the Root's Mood for the
    // pitches). Returns true when a pattern changed.
    bool service(const RecordSettings& settings, int mood);
    bool taking() const { return open; }
    // Bumped by every change it makes to the patterns.
    uint32_t revision() const { return changes; }
    int takeNotes() const { return int(notes.size()); }

    // A take recorded with the host transport has ended with notes in it (read once):
    // the host has very likely recorded the same notes as a MIDI region.
    bool takeWithTransportEnded() {
        const bool was = transportTake;
        transportTake = false;
        return was;
    }

    // One step of undo: the last take or quantise, as it was before.
    bool canUndo() const { return !undoStack.empty(); }
    bool undo();
    // Quantises an element's pattern (as it plays, generated or edited) afterwards: the
    // grid and strength as on input. Undoable.
    bool quantise(int slot, int type, int element, int variant, int grid, float strength);

    // The pitch (semitones above the key root) as a pattern note's degree and alteration,
    // for a pattern that is fixed in the key or follows the chords (chord: the chord's
    // degree there). Public for the tests.
    static void toDegree(const pat::Pattern& p, int mood, int chord, int pitch, int& degree, int& alter);

private:
    struct TakeNote {
        int note = 0, pitch = 0, chord = 0;
        uint8_t velocity = 100;
        double on = 0, off = -1;
    };
    void begin(const Captured& c, const RecordSettings& s);
    void rebuild(int mood, double until);
    void finish(int mood, double at);

    PatternStore& store;
    Capture& capture;
    bool open = false, isFree = false, transportTake = false, pushedUndo = false;
    int slot = 0, type = 0, element = 0, variant = 0, mode = Replace, grid = QuantiseOff;
    float strength = 1;
    bool autoLength = false, kit = false, punch = false;
    double punchIn = 0, punchOut = 0;
    uint32_t changes = 0;
    double startBeat = 0, lastBeat = 0;
    pat::Element before;      // the element when the take started
    pat::Pattern base;        // what Overdub adds to
    std::vector<TakeNote> notes;
    struct Undo {
        int slot = 0, type = 0, element = 0;
        pat::Element element_;
    };
    std::vector<Undo> undoStack;
    static constexpr std::size_t undoLimit = 32;
    void pushUndo(int s, int t, int e, const pat::Element& el);
};

} // namespace wb::keys
