#pragma once
// Patterns to and from Standard MIDI Files: export (drag out or "Save as MIDI") and
// import (drop a .mid file on an element).
#include "Pattern.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace wb::pat {

// The MIDI note of a kit row (General MIDI drums: kick, snare, closed hat, open hat).
inline constexpr std::array<int, 4> kitNotes { 36, 38, 42, 46 };

struct MidiTarget {
    int root = 2, mood = 0;
    bool kit = false;
    int kitRows = 3;
    float bpm = 120;
    float danger = 0;   // the progression a Follow Chords pattern is written out against
};

constexpr int ticksPerBeat = 960;

// Adds the pattern's notes to a track (960 ticks per beat from beat 0): repeated over
// `beats` beats (notes running past the end are cut there), or one pass as written when
// `beats` <= 0. Follow Chords patterns follow the shared chord progression throughout.
void appendNotes(juce::MidiMessageSequence& track, const Pattern& pattern, const MidiTarget& target, double beats, int channel);

// The pattern's notes over one pass, as a one-track MIDI file (960 ticks per beat).
// Follow Chords patterns are written out on the shared chord progression from bar 1.
juce::MidiFile toMidi(const Pattern& pattern, const MidiTarget& target);

// Reads every note of every track. Melodic: notes snap to the scale (or keep their
// exact pitch as an alteration when `snap` is false). Kit: GM drum notes map to rows,
// other pitches by order. The grid keeps `resolution`; the length is the notes' bars.
// Returns the number of notes read (0: nothing usable, `out` unchanged).
int fromMidi(const juce::MidiFile& file, Pattern& out, const MidiTarget& target, bool snap, int resolution);

// Scale degree and alteration for semitones above the root.
void degreeFor(int mood, int semitones, bool snap, int& degree, int& alter);

} // namespace wb::pat
