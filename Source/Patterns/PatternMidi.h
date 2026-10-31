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
// `offset` (beats) moves every note later and reads the chords there: a clip laid at its
// section's place in the song (Milestone 16). Patterns, clips and long views alike.
template <typename P>
void appendNotes(juce::MidiMessageSequence& track, const P& pattern, const MidiTarget& target, double beats, int channel, double offset = 0);

// The pattern's notes over one pass, as a one-track MIDI file (960 ticks per beat).
// Follow Chords patterns are written out on the shared chord progression from bar 1.
// `chordsFrom`: the song beat the notes start on (a clip: its section's first downbeat),
// for the chords a Follow Chords part is written out on.
template <typename P>
juce::MidiFile toMidi(const P& pattern, const MidiTarget& target, double chordsFrom = 0);

// Reads every note of every track. Melodic: notes snap to the scale (or keep their
// exact pitch as an alteration when `snap` is false). Kit: GM drum notes map to rows,
// other pitches by order. The grid keeps `resolution`; the length is the notes' bars.
// Returns the number of notes read (0: nothing usable, `out` unchanged).
// `bars` > 0: the length is that many bars (a clip keeps its section's length).
template <typename P>
int fromMidi(const juce::MidiFile& file, P& out, const MidiTarget& target, bool snap, int resolution, int bars = 0);

// Scale degree and alteration for semitones above the root.
void degreeFor(int mood, int semitones, bool snap, int& degree, int& alter);

} // namespace wb::pat
