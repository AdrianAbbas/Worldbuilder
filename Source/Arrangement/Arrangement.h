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
#include <memory>

namespace wb::arr {

constexpr int maxSections = 32, minBars = 1, maxBars = 64, beatsPerBar = 4, nameLength = 24;

// How an instrument plays in a section.
enum Entry : uint8_t { EntryOn, EntryOff, EntryHalfway, EntryFadeIn, EntryFadeOut, EntryCount };
inline constexpr std::array<const char*, EntryCount> entryNames { "On", "Off", "Enter halfway", "Fade in", "Fade out" };

// The shape of a section's Score Intensity ramp from its start value to its end value.
enum Curve : uint8_t { CurveLinear, CurveEaseIn, CurveEaseOut, CurveSwell, CurveCount };
inline constexpr std::array<const char*, CurveCount> curveNames { "Linear", "Ease in", "Ease out", "Swell" };

// After the last section: stop (the instruments stop and the world fades out over two
// bars), or loop back to a section. EndRingOut is for renders only (stingers, never
// saved): no new notes after the end, but what rings keeps ringing (its last transition
// lands too), and the world fades out from one to three bars after it.
enum End : uint8_t { EndStop, EndLoop, EndCount, EndRingOut = EndCount };

// Transitions (Milestone 11b): how a section hands over to the next one, at its end.
// Crossfade: over its last 1-4 bars the instruments leaving fade out, those entering
// fade in (playing their next-section variant) and Score Intensity glides to the next
// section's start. Riser: a rising swell of noise and the chord over its last 1-4 bars.
// Drum fill: the rhythm instruments play their Fill over its last 1-4 bars. Drop: the
// world falls silent for the last 1-4 beats, then a sub hit lands on the downbeat.
// Reverse swell: the chord swells up into the downbeat over 1-4 bars. Filter sweep: the
// whole mix closes down over 1-4 bars and opens again over the first beat after.
enum Transition : uint8_t { TransCut, TransCrossfade, TransRiser, TransDrumFill, TransDrop, TransReverse, TransFilter, TransitionCount };
inline constexpr std::array<const char*, TransitionCount> transitionNames { "Cut", "Crossfade", "Riser", "Drum fill", "Drop", "Reverse swell", "Filter sweep" };
constexpr int maxTransitionLength = 4;
// The transition's length in beats: bars, except a Drop (beats of silence).
inline double transitionBeats(int kind, int length) {
    const double n = double(std::clamp(length, 1, maxTransitionLength));
    return kind == TransDrop ? n : n * 4.0;
}

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

// Milestone 16: a section's own notes for an instrument. Immutable once made (an edit
// makes a new one), so sections and undo steps share them, and the audio thread reads
// them without locks; the last reference is only ever dropped off the audio thread.
using ClipRef = std::shared_ptr<const pat::Clip>;
inline bool sameClip(const ClipRef& a, const ClipRef& b) {
    if (a == b) return true;
    if (a == nullptr || b == nullptr) return false;
    return a->ownSwing == b->ownSwing && a->sameNotes(*b);
}

struct Cell {
    uint8_t entry = EntryOn;
    uint8_t variant = VariantA;   // A, B or C
    bool fill = false;            // plays its Fill in the last bar before a section change
    // The section's own clip (unique), or nullptr: linked to the variant, which plays
    // as it does everywhere it is linked (edits to the variant show up here).
    ClipRef clip;
    bool unique() const { return clip != nullptr; }
    bool operator==(const Cell& o) const { return entry == o.entry && variant == o.variant && fill == o.fill && sameClip(clip, o.clip); }
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
    // Milestone 11b: the transition into the next section and its length (bars; beats
    // for a Drop); the Location to be at (-1: stay where the world is; the journey
    // arrives on the section's first downbeat); the Planet (-1: as set) and World Control
    // overrides (-1: as set, else 0-100 %) while the section plays.
    uint8_t transition = TransCut, transitionLength = 1;
    int8_t location = -1, planet = -1;
    std::array<int8_t, WorldControlCount> world { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

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
               && cells == o.cells && transition == o.transition && transitionLength == o.transitionLength && location == o.location
               && planet == o.planet && world == o.world;
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
    bool looped = false;       // reached through End: loop (the section before it is the last one)
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
    // Milestone 16: each instrument's own clip in this section (nullptr: linked, or a
    // Fill plays instead), laid from `clipOrigin` (song beats: where this pass of the
    // section's clip sequence starts, its first downbeat) for `clipSpan` beats (0:
    // endlessly).
    std::array<std::array<const pat::Clip*, placeElements>, placeSlots> clip {};
    double clipOrigin = 0, clipSpan = 0;
    // Transitions: the one under way into the next section (transitionT 0..1 across its
    // length; -1: none) and the one that led into this section (`arrived`, with
    // `sinceStart` beats into the section; Cut when it was a repeat or the first).
    uint8_t transition = TransCut, arrived = TransCut;
    float transitionT = -1;
    double transitionLength = 0;   // beats
    double sinceStart = 0;
    bool drumFill = false;     // the rhythm instruments play their Fills (the engine picks them)
    bool ringOut = false;      // End: ring out (renders): no new notes
    float duck = 1;            // a Drop's silence (the whole world)
    // Overrides while the section plays.
    int planet = -1;
    std::array<float, WorldControlCount> world {};   // < 0: as set
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
                    p.looped = pass > 0;
                    return p;
                }
                start += span;
            }
            // After the last section.
            const double total = start;
            if (end != EndLoop || pass > 0) {
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
        if (end != EndLoop) return true;
        return loopTarget() != index;
    }
    // The section that follows `index` (-1: the end, End: stop).
    int nextOf(int index) const {
        if (index + 1 < count) return index + 1;
        return end == EndLoop ? loopTarget() : -1;
    }
    // Whether a transition plays at the end of `index`: a different section follows.
    bool transitionAfter(int index) const {
        if (end == EndRingOut && index == count - 1) return !sections[std::size_t(index)].endless();
        const int next = nextOf(index);
        return next >= 0 && next != index && !sections[std::size_t(index)].endless();
    }
    // The Location the world is at in section `index`: its own, or the last one set
    // before it (-1: none, the world as set).
    int locationOf(int index) const {
        for (int i = std::min(index, count - 1); i >= 0; --i)
            if (sections[std::size_t(i)].location >= 0) return sections[std::size_t(i)].location;
        return -1;
    }
    bool usesLocations() const {
        for (int i = 0; i < count; ++i)
            if (sections[std::size_t(i)].location >= 0) return true;
        return false;
    }
    // The journey between Locations at a song position: from `from` to `to` (-1: the
    // world as set) at `position` (0..1, not eased). A journey to the next section's
    // Location takes travelBeats (at most the section's last pass) and arrives on its
    // first downbeat.
    void locationAt(double beat, double travelBeats, int& from, int& to, float& position) const {
        from = to = -1;
        position = 0;
        if (count <= 0) return;
        const auto at = locate(beat);
        from = to = locationOf(at.section);
        if (at.ended || !at.lastPass || !transitionAfter(at.section)) return;
        const int next = nextOf(at.section);
        // Looping back: the loop target's Location is its own, or the one it inherits.
        const int there = locationOf(next);
        if (there == from) return;
        const auto& s = sections[std::size_t(at.section)];
        const double passBeats = double(std::max<int>(s.bars, 1)) * beatsPerBar;
        const double length = std::clamp(travelBeats, 1.0e-3, passBeats);
        const double remaining = passBeats - at.bar * beatsPerBar;
        if (remaining > length) return;
        to = there;
        position = float(std::clamp(1.0 - remaining / length, 0.0, 1.0));
    }

    State evaluate(double beat) const {
        State st;
        for (auto& row : st.gain) row.fill(1.0f);
        st.world.fill(-1.0f);
        st.active = count > 0;
        if (!st.active) return st;
        st.at = locate(beat);
        const auto& s = sections[std::size_t(st.at.section)];
        const double bars = double(std::max<int>(s.bars, 1));
        const float t = float(std::clamp(st.at.bar / bars, 0.0, 1.0));
        st.intensity = intensityAt(s, st.at.ended ? 1.0f : t);
        st.planet = s.planet;
        for (int w = 0; w < WorldControlCount; ++w)
            if (s.world[std::size_t(w)] >= 0) st.world[std::size_t(w)] = float(std::min<int>(s.world[std::size_t(w)], 100)) / 100.0f;
        // The transition that led here: on the first pass, from the section before it.
        st.sinceStart = st.at.bar * beatsPerBar;
        if (!st.at.ended && st.at.pass == 0) {
            const int previous = st.at.section > 0 && !(st.at.looped && st.at.section == loopTarget()) ? st.at.section - 1
                                 : st.at.looped && st.at.section == loopTarget()                     ? count - 1
                                                                                                    : -1;
            if (previous >= 0 && previous != st.at.section && transitionAfter(previous)) st.arrived = sections[std::size_t(previous)].transition;
        }
        if (st.at.ended && end == EndRingOut) {
            // A stinger's ring-out: as the end left it, without new notes; the last
            // transition lands; the world fades out from one to three bars after the end.
            State last = evaluate(double(totalBars()) * beatsPerBar - 1.0e-6);
            last.at = st.at;
            last.ringOut = true;
            last.transition = TransCut;
            last.transitionT = -1;
            last.drumFill = false;
            last.duck = 1;
            last.arrived = s.transition;
            last.sinceStart = st.at.beyond * beatsPerBar;
            if (last.arrived == TransDrop) last.duck = float(std::clamp(last.sinceStart / 0.05, 0.0, 1.0)) * 0.85f + 0.15f;
            const float fade = dsp::clamp01(float((st.at.beyond - 1.0) / 2.0));
            last.endGain = 0.5f + 0.5f * std::cos(dsp::pi * fade);
            return last;
        }
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
                const bool fill = st.fillBar && cell.fill;
                st.variant[std::size_t(slot)][std::size_t(e)] = fill ? uint8_t(VariantFill) : uint8_t(std::min<int>(cell.variant, VariantC));
                // The Fill switch plays the instrument's Fill over a unique clip's last bar too.
                st.clip[std::size_t(slot)][std::size_t(e)] = fill ? nullptr : cell.clip.get();
            }
        // The clips start on the section's first downbeat (on the song timeline as it
        // plays now: after End: loop, that of the pass under way) and repeat with its passes.
        st.clipSpan = s.endless() ? 0.0 : double(s.span()) * beatsPerBar;
        st.clipOrigin = std::round(beat - (double(st.at.pass) * bars + st.at.bar) * beatsPerBar);
        // The transition into the next section, over its last beats.
        if (st.at.lastPass && s.transition != TransCut && transitionAfter(st.at.section)) {
            const double length = std::min(transitionBeats(s.transition, s.transitionLength), bars * beatsPerBar);
            const double remaining = (bars - st.at.bar) * beatsPerBar;
            if (remaining <= length) {
                st.transition = s.transition;
                st.transitionT = float(std::clamp(1.0 - remaining / length, 0.0, 1.0));
                st.transitionLength = length;
                const int next = nextOf(st.at.section);
                transitionInto(st, sections[std::size_t(next >= 0 ? next : st.at.section)]);
            }
        }
        // The first beat after a Drop: the world comes back over a few milliseconds.
        if (st.arrived == TransDrop) st.duck = std::min(st.duck, float(std::clamp(st.sinceStart / 0.05, 0.0, 1.0)) * 0.85f + 0.15f);
        return st;
    }

private:
    // What a transition under way does to the instruments and the intensity.
    static void transitionInto(State& st, const Section& next) {
        const float t = st.transitionT;
        switch (st.transition) {
            case TransCrossfade: {
                const float u = 0.5f - 0.5f * std::cos(dsp::pi * t);
                for (int slot = 0; slot < placeSlots; ++slot)
                    for (int e = 0; e < placeElements; ++e) {
                        const auto& cell = next.cells[std::size_t(slot)][std::size_t(e)];
                        // How the instrument starts the next section.
                        const float there = cell.entry == EntryOn || cell.entry == EntryFadeOut ? 1.0f : 0.0f;
                        float& g = st.gain[std::size_t(slot)][std::size_t(e)];
                        if (g <= 0.0f && there > 0.0f) {
                            st.variant[std::size_t(slot)][std::size_t(e)] = uint8_t(std::min<int>(cell.variant, VariantC));
                            st.clip[std::size_t(slot)][std::size_t(e)] = nullptr;
                        }
                        g = dsp::lerp(g, there, u);
                    }
                st.intensity = dsp::lerp(st.intensity, next.intensityStart, u);
                break;
            }
            case TransRiser: st.intensity = dsp::clamp01(st.intensity + 0.12f * t * t); break;
            case TransDrumFill: st.drumFill = true; break;
            case TransDrop:
                // Silence: the instruments stop and the world ducks away over the first 15 %.
                for (auto& row : st.gain) row.fill(0.0f);
                st.duck = 1.0f - 0.85f * dsp::smoothstep(0.0f, 0.15f, t);
                break;
            default: break;
        }
    }
public:
};

} // namespace wb::arr
