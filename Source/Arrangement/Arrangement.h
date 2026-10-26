#pragma once
// The Arrangement (Milestone 11): a timeline of sections, locked to the host's bars,
// that turns WORLDBUILDER into a game-soundtrack maker. A track evolves through the
// sections the composer designs (e.g. 8 bars Intro, 16 Build, 16 Climax, 16 Wind-down,
// 8 Outro), in any order, any lengths, any number.
//
// Each section has a name and colour, a length in bars, how many times it plays (or
// endlessly, for game-style loops), a Score Intensity ramp (start, end and a curve
// shape), and an instrument grid: for every instrument of every Place slot, whether it
// plays (on, off, entering halfway, fading in or out), which pattern variant (A, B or
// C) it plays, and whether it plays its Fill in the last bar before the section changes.
//
// What plays at a song position is a pure function of the arrangement and that
// position (evaluate()), so playback from any bar is repeatable and matches renders.
// Everything is in 4/4 bars of four beats, as the patterns and chords count.
//
// Plain data with fixed storage: the editor publishes it to the audio thread whole
// (pat::TripleBuffer) and nothing here allocates.
#include "../Places/PlaceSlot.h"
#include <array>
#include <cstring>

namespace wb::arr {

constexpr int maxSections = 32, minBars = 1, maxBars = 64, beatsPerBar = 4, nameLength = 24;

// How an instrument plays in a section.
enum Entry : uint8_t { EntryOn, EntryOff, EntryHalfway, EntryFadeIn, EntryFadeOut, EntryCount };
inline constexpr std::array<const char*, EntryCount> entryNames { "On", "Off", "Enter halfway", "Fade in", "Fade out" };

// The shape of a section's Score Intensity ramp from its start value to its end value.
enum Curve : uint8_t { CurveLinear, CurveEaseIn, CurveEaseOut, CurveSwell, CurveCount };
inline constexpr std::array<const char*, CurveCount> curveNames { "Linear", "Ease in", "Ease out", "Swell" };

// After the last section: stop (the instruments stop and the world fades out over two
// bars), or loop back to a section.
enum End : uint8_t { EndStop, EndLoop, EndCount };

// Pattern variants: each instrument has A, B and C, and a Fill.
using pat::VariantA, pat::VariantB, pat::VariantC, pat::VariantFill;
constexpr int VariantCount = pat::variantCount;
inline constexpr std::array<const char*, VariantCount> variantNames { "A", "B", "C", "Fill" };

// How many times a section plays (0: endlessly, a game-style loop).
inline constexpr std::array<int, 6> repeatChoices { 1, 2, 3, 4, 8, 0 };

inline constexpr std::array<const char*, 8> presetNames { "Intro", "Build", "Climax", "Wind-down", "Outro", "Loop", "Bridge", "Break" };
constexpr int colourCount = 10;
inline constexpr std::array<uint32_t, colourCount> colours { 0xffc9a45c, 0xff4fc3d9, 0xffd9604f, 0xff7fb069, 0xff9a7fd1,
                                                             0xffe08a3c, 0xff3fa7a0, 0xffd1c7a8, 0xffd96fa8, 0xff6f8fd9 };

struct Cell {
    uint8_t entry = EntryOn;
    uint8_t variant = VariantA;   // A, B or C
    bool fill = false;            // plays its Fill in the last bar before a section change
    bool operator==(const Cell& o) const { return entry == o.entry && variant == o.variant && fill == o.fill; }
};

using Grid = std::array<std::array<Cell, placeElements>, placeSlots>;

struct Section {
    std::array<char, nameLength> name {};
    uint8_t colour = 0;
    uint8_t bars = 8;        // one pass
    uint8_t repeats = 1;     // passes (0: endless)
    uint8_t curve = CurveLinear;
    float intensityStart = 0.5f, intensityEnd = 0.5f;
    Grid cells {};

    void setName(const char* text) {
        name.fill(0);
        if (text != nullptr) std::strncpy(name.data(), text, std::size_t(nameLength - 1));
    }
    const char* nameText() const { return name.data(); }
    bool endless() const { return repeats == 0; }
    // Bars the section takes on the timeline (one pass when endless).
    int span() const { return int(bars) * std::max<int>(repeats, 1); }
    bool operator==(const Section& o) const {
        return std::strncmp(name.data(), o.name.data(), std::size_t(nameLength)) == 0 && colour == o.colour && bars == o.bars
               && repeats == o.repeats && curve == o.curve && intensityStart == o.intensityStart && intensityEnd == o.intensityEnd
               && cells == o.cells;
    }
};

inline Section makeSection(const char* name, int bars, int colour, float intensity = 0.5f) {
    Section s;
    s.setName(name);
    s.bars = uint8_t(std::clamp(bars, minBars, maxBars));
    s.colour = uint8_t(((colour % colourCount) + colourCount) % colourCount);
    s.intensityStart = s.intensityEnd = dsp::clamp01(intensity);
    return s;
}

// The intensity curve: 0..1 across a pass to 0..1 between the start and end values.
inline float shape(int curve, float t) {
    t = dsp::clamp01(t);
    switch (curve) {
        case CurveEaseIn: return t * t;
        case CurveEaseOut: return 1.0f - (1.0f - t) * (1.0f - t);
        case CurveSwell: return dsp::smoothstep(0.0f, 1.0f, t);
        default: return t;
    }
}
inline float intensityAt(const Section& s, float t) {
    float v = dsp::lerp(s.intensityStart, s.intensityEnd, shape(s.curve, t));
    // A swell rises above both ends in the middle of the section and settles again.
    if (s.curve == CurveSwell) v += 0.25f * std::sin(dsp::pi * dsp::clamp01(t));
    return dsp::clamp01(v);
}

// Where the arrangement stands at a song position.
struct Position {
    int section = -1;          // -1: no sections
    int pass = 0;              // which repeat
    double bar = 0;            // bars into the pass
    bool lastPass = true;
    bool ended = false;        // after the last section (End: stop)
    double beyond = 0;         // bars after the end
    int startBar = 0;          // where the section starts on the timeline (first pass)
};

// What the arrangement plays at a song position.
struct State {
    bool active = false;       // there are sections
    Position at;
    float intensity = 0.5f;    // the section's Score Intensity
    float endGain = 1;         // the fade after the end (End: stop)
    bool fillBar = false;      // the last bar before a section change
    std::array<std::array<float, placeElements>, placeSlots> gain {};
    std::array<std::array<uint8_t, placeElements>, placeSlots> variant {};
};

struct Arrangement {
    std::array<Section, maxSections> sections {};
    int count = 0;
    uint8_t end = EndStop;
    uint8_t loopTo = 0;        // End: loop, back to this section
    uint32_t revision = 0;

    bool operator==(const Arrangement& o) const {
        if (count != o.count || end != o.end || loopTo != o.loopTo) return false;
        for (int i = 0; i < count; ++i)
            if (!(sections[std::size_t(i)] == o.sections[std::size_t(i)])) return false;
        return true;
    }
    // Where section i starts (bars from the song start).
    int startOf(int index) const {
        int bar = 0;
        for (int i = 0; i < std::min(index, count); ++i) bar += sections[std::size_t(i)].span();
        return bar;
    }
    // Bars on the timeline (an endless section counts one pass, and ends the reachable part).
    int totalBars() const { return startOf(count); }
    // The first endless section (count: none). Sections after it are never reached.
    int firstEndless() const {
        for (int i = 0; i < count; ++i)
            if (sections[std::size_t(i)].endless()) return i;
        return count;
    }
    int loopTarget() const { return std::clamp(int(loopTo), 0, std::max(count - 1, 0)); }

    Position locate(double beat) const {
        Position p;
        if (count <= 0) return p;
        double bar = std::max(0.0, beat / double(beatsPerBar));
        for (int pass = 0; pass < 2; ++pass) {
            double start = 0;
            for (int i = 0; i < count; ++i) {
                const auto& s = sections[std::size_t(i)];
                const double length = double(std::max<int>(s.bars, 1));
                const bool endlessHere = s.endless();
                const double span = endlessHere ? 0.0 : length * double(s.repeats);
                if (endlessHere || bar < start + span) {
                    const double into = bar - start;
                    p.section = i;
                    p.pass = int(std::floor(into / length));
                    p.bar = into - double(p.pass) * length;
                    p.lastPass = !endlessHere && p.pass >= int(s.repeats) - 1;
                    p.startBar = int(start);
                    return p;
                }
                start += span;
            }
            // After the last section.
            const double total = start;
            if (end == EndStop || pass > 0) {
                const auto& last = sections[std::size_t(count - 1)];
                p.section = count - 1;
                p.pass = std::max<int>(last.repeats, 1) - 1;
                p.bar = double(std::max<int>(last.bars, 1));
                p.lastPass = true;
                p.ended = true;
                p.beyond = bar - total;
                p.startBar = int(total) - last.span();
                return p;
            }
            const double from = double(startOf(loopTarget())), length = total - from;
            if (length <= 0) {
                bar = from;
                continue;
            }
            bar = from + std::fmod(bar - total, length);
        }
        return p;
    }

    // Whether the section after `index` (at its end) is a different one: a section
    // change follows, so its last bar plays the Fills.
    bool changeAfter(int index) const {
        if (index + 1 < count) return true;
        if (end == EndStop) return true;
        return loopTarget() != index;
    }

    State evaluate(double beat) const {
        State st;
        for (auto& row : st.gain) row.fill(1.0f);
        st.active = count > 0;
        if (!st.active) return st;
        st.at = locate(beat);
        const auto& s = sections[std::size_t(st.at.section)];
        const double bars = double(std::max<int>(s.bars, 1));
        const float t = float(std::clamp(st.at.bar / bars, 0.0, 1.0));
        st.intensity = intensityAt(s, st.at.ended ? 1.0f : t);
        if (st.at.ended) {
            // The instruments stop; the world (reverbs, environment) fades out over two bars.
            for (auto& row : st.gain) row.fill(0.0f);
            const float fade = dsp::clamp01(float(st.at.beyond / 2.0));
            st.endGain = 0.5f + 0.5f * std::cos(dsp::pi * fade);
            return st;
        }
        st.fillBar = st.at.lastPass && st.at.bar >= bars - 1.0 && changeAfter(st.at.section);
        // Enter halfway: on from the middle bar (the middle beat of a one-bar section).
        const double half = s.bars >= 2 ? double(s.bars / 2) : 0.5;
        for (int slot = 0; slot < placeSlots; ++slot)
            for (int e = 0; e < placeElements; ++e) {
                const auto& cell = s.cells[std::size_t(slot)][std::size_t(e)];
                float g = 1;
                switch (cell.entry) {
                    case EntryOff: g = 0; break;
                    case EntryHalfway: g = st.at.bar >= half ? 1.0f : 0.0f; break;
                    case EntryFadeIn: g = 0.5f - 0.5f * std::cos(dsp::pi * t); break;
                    case EntryFadeOut: g = 0.5f + 0.5f * std::cos(dsp::pi * t); break;
                    default: break;
                }
                st.gain[std::size_t(slot)][std::size_t(e)] = g;
                st.variant[std::size_t(slot)][std::size_t(e)] =
                    st.fillBar && cell.fill ? uint8_t(VariantFill) : uint8_t(std::min<int>(cell.variant, VariantC));
            }
        return st;
    }
};

} // namespace wb::arr
