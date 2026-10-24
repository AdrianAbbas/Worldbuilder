#pragma once
// Key and tempo. Scales for each Mood, note frequencies and the shared harmonic
// clock. Everything tonal in the world (the Tonal Bed, the retuned climate layers
// and the Places) takes its notes from here, so the whole world sits in the song's
// key and can sit under a score.
#include "Core.h"
#include <array>
#include <cstdint>

namespace wb::music {

struct ScaleDef {
    int size;
    std::array<int, 8> steps;
};

inline const ScaleDef& scaleFor(int mood) {
    static constexpr std::array<ScaleDef, 9> table { {
        { 7, { 0, 2, 3, 5, 7, 8, 10 } },      // Minor (Aeolian)
        { 7, { 0, 2, 4, 5, 7, 9, 11 } },      // Major
        { 7, { 0, 2, 3, 5, 7, 9, 10 } },      // Dorian
        { 7, { 0, 1, 3, 5, 7, 8, 10 } },      // Phrygian
        { 7, { 0, 2, 4, 6, 7, 9, 11 } },      // Lydian
        { 7, { 0, 2, 4, 5, 7, 9, 10 } },      // Mixolydian
        { 6, { 0, 2, 4, 6, 8, 10 } },         // Whole-tone
        { 5, { 0, 3, 5, 7, 10 } },            // Pentatonic (minor, like the bed)
        { 8, { 0, 1, 3, 4, 6, 7, 9, 10 } },   // Chromatic: half-whole octatonic for melodies
    } };
    return table[std::size_t(std::clamp(mood, 0, 8))];
}

inline int floorDiv(int a, int b) { return (a >= 0 ? a : a - b + 1) / b; }
inline int64_t floorDiv(int64_t a, int64_t b) { return (a >= 0 ? a : a - b + 1) / b; }

// Semitones above the key root for scale degree `d` (any integer; octaves wrap).
inline int degree(int mood, int d) {
    const auto& s = scaleFor(mood);
    const int octave = floorDiv(d, s.size);
    return s.steps[std::size_t(d - octave * s.size)] + 12 * octave;
}

inline bool inScale(int mood, int semitones) {
    const auto& s = scaleFor(mood);
    const int pc = ((semitones % 12) + 12) % 12;
    for (int i = 0; i < s.size; ++i)
        if (s.steps[std::size_t(i)] == pc) return true;
    return false;
}

// The nearest in-scale note to `semitones` (ties go down).
inline int quantise(int mood, float semitones) {
    const int base = int(std::floor(semitones));
    for (int k = 0; k < 12; ++k) {
        const int down = base - k, up = base + 1 + k;
        const bool d = inScale(mood, down), u = inScale(mood, up);
        if (d && u) return (semitones - float(down) <= float(up) - semitones) ? down : up;
        if (d) return down;
        if (u) return up;
    }
    return base;
}

// The root in octave 3 (C3 = 130.8 Hz, D3 = 146.8 Hz).
inline float rootHz(int root) { return 130.8128f * std::exp2(float(std::clamp(root, 0, 11)) / 12.0f); }
inline float noteHz(int root, float semitones) { return rootHz(root) * std::exp2(semitones / 12.0f); }

// The shared harmonic clock: a slow progression, one chord per four bars, the same
// for every Place so they never clash. Each entry is the scale degree of the chord
// root. Danger turns the calm cycle into a darker one that keeps falling back to
// the tonic.
constexpr int beatsPerChord = 16;
inline int chordDegree(double beat, float danger) {
    static constexpr int calm[4] { 0, 5, 3, 4 }, dark[4] { 0, 1, 0, 5 };
    const auto k = int64_t(std::floor(beat / double(beatsPerChord)));
    const auto i = std::size_t(((k % 4) + 4) % 4);
    return danger > 0.55f ? dark[i] : calm[i];
}

// Chord tone `n` (0 root, 1 third, 2 fifth, 3 seventh...) of the chord built on scale
// degree `chord`, in semitones above the key root. Stacking every other scale note
// gives triads in the diatonic modes, quartal chords in the pentatonic, augmented
// chords in the whole-tone scale and diminished ones in the octatonic.
inline int chordTone(int mood, int chord, int n) { return degree(mood, chord + 2 * n); }

// The scale degree (0 .. size - 1) whose note is nearest to `semitones` (0..11) above
// the root: the fifth where the scale has one, else its closest neighbour.
inline int nearestDegree(int mood, int semitones) {
    const auto& s = scaleFor(mood);
    int best = 0;
    for (int i = 1; i < s.size; ++i)
        if (std::abs(s.steps[std::size_t(i)] - semitones) < std::abs(s.steps[std::size_t(best)] - semitones)) best = i;
    return best;
}

// Places a pitch class (semitones, any octave) into [low, low + 12).
inline int inRange(int semitones, int low) {
    int s = semitones;
    while (s < low) s += 12;
    while (s >= low + 12) s -= 12;
    return s;
}

} // namespace wb::music
