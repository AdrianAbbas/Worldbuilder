#pragma once
// Patterns: the editable notes each rhythmic or melodic element of a Place plays.
//
// A Pattern is a list of notes on a step grid (1/4 to 1/32, straight or triplet), 1-8
// bars long or a free number of steps. Pitch is a scale degree, so a pattern follows
// Root and Mood; with Follow Chords the degree counts from the root of the current
// chord of the shared progression. Percussion elements use "kit" patterns, where the
// pitch picks a drum (a row of the kit) instead.
//
// Controlled randomness changes what plays from pass to pass, only in the ways its
// switches allow, never for a locked note, and always as a pure function of
// (seed, pass, note): the same song position always plays the same thing. Evolve
// slowly accumulates mutations over the bars (within the same switches) and can
// return to the original every N bars.
//
// Plain data with fixed storage: patterns are copied between threads whole (see
// TripleBuffer) and nothing here allocates.
#include "../DSP/Core.h"
#include "../DSP/Music.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace wb::pat {

constexpr int maxNotes = 256;
constexpr int maxBars = 8;

enum Resolution { Quarter, Eighth, Sixteenth, ThirtySecond, QuarterT, EighthT, SixteenthT, ThirtySecondT, ResolutionCount };
inline constexpr std::array<const char*, ResolutionCount> resolutionNames { "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/32T" };
inline constexpr std::array<int, ResolutionCount> stepsPerBarTable { 4, 8, 16, 32, 6, 12, 24, 48 };
inline int stepsPerBar(int resolution) { return stepsPerBarTable[std::size_t(std::clamp(resolution, 0, ResolutionCount - 1))]; }
inline double stepsPerBeat(int resolution) { return double(stepsPerBar(resolution)) / 4.0; }
constexpr int maxSteps = maxBars * 48;

struct Note {
    int16_t step = 0;        // start, in steps from the pattern start
    int16_t length = 1;      // in steps
    int8_t degree = 0;       // scale degree (melodic) or kit row
    int8_t alter = 0;        // chromatic offset in semitones (the 12-semitone view)
    uint8_t velocity = 100;  // 1..127
    uint8_t probability = 100;   // 0..100 %
    uint8_t flags = 0;
    static constexpr uint8_t lockFlag = 1;
    // Recorded in the take under way (KEYS): heard live as it is played, so playback
    // leaves it out until the take ends.
    static constexpr uint8_t takeFlag = 2;
    bool inTake() const { return (flags & takeFlag) != 0; }
    bool locked() const { return (flags & lockFlag) != 0; }
    void setLocked(bool on) { flags = uint8_t(on ? (flags | lockFlag) : (flags & ~lockFlag)); }
    bool operator==(const Note& o) const {
        return step == o.step && length == o.length && degree == o.degree && alter == o.alter && velocity == o.velocity
               && probability == o.probability && flags == o.flags;
    }
};

// What randomness and Evolve may change.
enum Vary : uint8_t { VaryTiming = 1, VaryVelocity = 2, VaryPitch = 4, VarySkip = 8, VaryExtra = 16, VaryAll = 31 };

struct Pattern {
    std::array<Note, maxNotes> notes {};
    int16_t count = 0;
    int16_t lengthSteps = 64;
    uint8_t resolution = Sixteenth;
    uint8_t bars = 4;            // 0: free length (lengthSteps)
    bool followChords = true;    // degrees count from the current chord's root
    int8_t base = 0;             // Follow Chords: semitones of the lowest place for the chord root
    float swing = 0.15f;         // delays the odd steps (up to 45 % of a step)

    void clear() { count = 0; }
    void setGrid(int res, int barCount, int freeSteps = 16) {
        resolution = uint8_t(std::clamp(res, 0, ResolutionCount - 1));
        bars = uint8_t(std::clamp(barCount, 0, maxBars));
        lengthSteps = int16_t(bars > 0 ? bars * stepsPerBar(resolution) : std::clamp(freeSteps, 1, maxSteps));
    }
    double lengthBeats() const { return double(lengthSteps) / stepsPerBeat(resolution); }
    bool add(Note n) {
        if (count >= maxNotes || n.step < 0 || n.step >= lengthSteps) return false;
        n.length = int16_t(std::clamp<int>(n.length, 1, maxSteps));
        n.velocity = uint8_t(std::clamp<int>(n.velocity, 1, 127));
        n.probability = uint8_t(std::min<int>(n.probability, 100));
        // Keep the notes sorted by step (insertion; patterns are small).
        int i = count++;
        while (i > 0 && notes[std::size_t(i - 1)].step > n.step) {
            notes[std::size_t(i)] = notes[std::size_t(i - 1)];
            --i;
        }
        notes[std::size_t(i)] = n;
        return true;
    }
    void remove(int index) {
        if (index < 0 || index >= count) return;
        for (int i = index; i + 1 < count; ++i) notes[std::size_t(i)] = notes[std::size_t(i + 1)];
        --count;
    }
    // Re-sorts after edits that move notes, and drops notes outside the length.
    void normalise() {
        int kept = 0;
        for (int i = 0; i < count; ++i)
            if (notes[std::size_t(i)].step >= 0 && notes[std::size_t(i)].step < lengthSteps) notes[std::size_t(kept++)] = notes[std::size_t(i)];
        count = int16_t(kept);
        for (int i = 1; i < count; ++i) {
            const Note n = notes[std::size_t(i)];
            int j = i;
            while (j > 0 && notes[std::size_t(j - 1)].step > n.step) {
                notes[std::size_t(j)] = notes[std::size_t(j - 1)];
                --j;
            }
            notes[std::size_t(j)] = n;
        }
    }
    bool sameNotes(const Pattern& o) const {
        if (count != o.count || lengthSteps != o.lengthSteps || resolution != o.resolution || bars != o.bars || followChords != o.followChords
            || base != o.base || std::abs(swing - o.swing) > 1.0e-6f)
            return false;
        for (int i = 0; i < count; ++i)
            if (!(notes[std::size_t(i)] == o.notes[std::size_t(i)])) return false;
        return true;
    }
};

// The per-element controls the composer sets in the Pattern Editor.
struct Controls {
    float randomness = 0.35f;   // 0 plays the pattern exactly
    uint8_t vary = VaryVelocity | VarySkip;
    float evolve = 0;           // 0 off .. 1 fast
    uint8_t returnBars = 0;     // Evolve returns to the original every N bars (0: never)
    bool edited = false;        // false: the Place's generator writes the pattern
    uint8_t variant = 0;        // bumped by Generate for a fresh pattern
    bool mute = false, solo = false;   // the element's mute and solo (within its Place)
    bool operator==(const Controls& o) const {
        return std::abs(randomness - o.randomness) < 1.0e-6f && vary == o.vary && std::abs(evolve - o.evolve) < 1.0e-6f
               && returnBars == o.returnBars && edited == o.edited && variant == o.variant && mute == o.mute && solo == o.solo;
    }
};

inline constexpr std::array<int, 6> returnChoices { 0, 1, 2, 4, 8, 16 };

// Pattern variants (Milestone 11): every element has patterns A, B and C, and a Fill
// that plays in the last bar before an arrangement section changes.
enum Variant : uint8_t { VariantA, VariantB, VariantC, VariantFill };
constexpr int variantCount = 4;

// An element: its controls, and its patterns once edited. Variant A is `pattern` (its
// edited flag and Generate count are in the controls); B, C and the Fill are `alt`.
// Randomness, Evolve, mute and solo are shared by the variants.
struct Element {
    Controls controls;
    Pattern pattern;
    std::array<Pattern, variantCount - 1> alt {};
    uint8_t altEdited = 0;                             // bit v - 1: variant v is edited
    std::array<uint8_t, variantCount - 1> altGenerate {};   // Generate counts of B, C, Fill

    static int clampVariant(int v) { return std::clamp(v, 0, variantCount - 1); }
    bool edited(int v) const { return v <= 0 ? controls.edited : ((altEdited >> (clampVariant(v) - 1)) & 1) != 0; }
    void setEdited(int v, bool on) {
        if (v <= 0) controls.edited = on;
        else altEdited = uint8_t(on ? (altEdited | (1 << (clampVariant(v) - 1))) : (altEdited & ~(1 << (clampVariant(v) - 1))));
    }
    Pattern& patternOf(int v) { return v <= 0 ? pattern : alt[std::size_t(clampVariant(v) - 1)]; }
    const Pattern& patternOf(int v) const { return v <= 0 ? pattern : alt[std::size_t(clampVariant(v) - 1)]; }
    uint8_t generation(int v) const { return v <= 0 ? controls.variant : altGenerate[std::size_t(clampVariant(v) - 1)]; }
    void setGeneration(int v, uint8_t g) {
        if (v <= 0) controls.variant = g;
        else altGenerate[std::size_t(clampVariant(v) - 1)] = g;
    }
    // Only A has been touched (B, C and the Fill as generated).
    bool variantsUntouched() const { return altEdited == 0 && altGenerate == std::array<uint8_t, variantCount - 1> {}; }
};

// Every Place has up to six instruments, each an element with its own pattern.
constexpr int elementsPerPlace = 6;

// One slot's patterns, as the editor publishes them to the audio thread.
struct SlotPatterns {
    int type = 0;           // the Place they belong to (others are ignored)
    uint32_t revision = 0;
    std::array<Element, elementsPerPlace> element {};
};

// Generated patterns, as the audio thread reports them back to the editor.
struct Generated {
    int type = 0;
    uint32_t serial = 0;
    std::array<Pattern, elementsPerPlace> pattern {};
    std::array<std::array<Pattern, elementsPerPlace>, variantCount - 1> alt {};   // B, C, Fill
    const Pattern& of(int variant, int e) const {
        const auto k = std::size_t(std::clamp(e, 0, elementsPerPlace - 1));
        return variant <= 0 ? pattern[k] : alt[std::size_t(std::min(variant, variantCount - 1) - 1)][k];
    }
};

//==============================================================================
// The latest value from one writer to one reader, lock-free: the writer fills
// write() and publishes it; the reader picks up the newest one with update().
template <typename T>
class TripleBuffer {
public:
    T& write() { return buffers[std::size_t(back)]; }
    void publish() { back = middle.exchange(back | dirty, std::memory_order_acq_rel) & 3; }
    // Reader side: true when a newer value was taken.
    bool update() {
        if ((middle.load(std::memory_order_acquire) & dirty) == 0) return false;
        front = middle.exchange(front, std::memory_order_acq_rel) & 3;
        return true;
    }
    const T& read() const { return buffers[std::size_t(front)]; }

private:
    static constexpr int dirty = 4;
    std::array<T, 3> buffers {};
    std::atomic<int> middle { 1 };
    int front = 0, back = 2;
};

//==============================================================================
// Playback.

// One note as it plays: when (beats), pitch (degree or kit row plus alteration),
// velocity (0..1) and length (beats).
struct Event {
    double beat = 0, length = 0.25;
    int degree = 0, alter = 0;
    float velocity = 1;
    int note = -1;       // index in the pattern (-1: an extra note)
    bool extra = false;
};

inline uint64_t hashOf(uint64_t key, int64_t a, int64_t b, uint64_t salt) {
    return dsp::mix64(key ^ dsp::mix64(uint64_t(a) * 0x9E3779B97F4A7C15ull ^ (uint64_t(b) << 20) ^ (salt << 52)));
}
inline float unitOf(uint64_t key, int64_t a, int64_t b, uint64_t salt) { return dsp::unipolar(hashOf(key, a, b, salt)); }

// How a note is changed on one pass: by randomness (fresh every pass) and by Evolve
// (mutations that build up over the bars and stay).
struct Change {
    bool skip = false, extra = false;
    int pitch = 0;          // scale steps (kit: no change)
    float velocity = 1;     // factor
    float delay = 0;        // steps
};

struct Playback {
    const Pattern* pattern = nullptr;
    Controls controls;
    float randomness = 0;   // effective (the element's Randomness scaled by the Place's Variation)
    float swing = 0;
    bool kit = false;
    uint64_t key = 0;

    Change changeFor(int index, int64_t pass, int64_t bar) const {
        Change c;
        const Note& n = pattern->notes[std::size_t(index)];
        if (n.locked()) return c;
        const uint8_t vary = controls.vary;
        const float r = randomness;
        if (r > 0.0f) {
            const auto u = [&](uint64_t salt) { return unitOf(key, pass, index, salt); };
            if ((vary & VarySkip) && u(1) < 0.4f * r) c.skip = true;
            if ((vary & VaryVelocity)) c.velocity *= 1.0f + 0.7f * r * (u(2) - 0.5f);
            if ((vary & VaryPitch) && !kit && u(3) < 0.5f * r) c.pitch += (u(4) < 0.5f ? -1 : 1) * (pattern->followChords ? 2 : 1);
            if ((vary & VaryTiming)) c.delay += 0.4f * r * u(5);
            if ((vary & VaryExtra) && u(6) < 0.35f * r) c.extra = true;
        }
        if (controls.evolve > 0.0f && vary != 0) {
            // Evolve: each note mutates at random moments (exponential waits, faster with
            // Evolve) counted in bars since the pattern last returned to the original.
            const int64_t every = controls.returnBars;
            const int64_t epoch = every > 0 ? music::floorDiv(bar, every) : 0;
            const double age = double(every > 0 ? bar - epoch * every : std::max<int64_t>(bar, 0));
            const double rate = 0.02 + 0.3 * double(controls.evolve);   // mutations per bar per note
            double at = 0;
            for (int k = 0; k < 4; ++k) {
                const auto u = [&](uint64_t salt) { return unitOf(key ^ 0x5EEDull, epoch, index * 8 + k, salt); };
                at += -std::log(1.0 - 0.999 * double(u(1))) / rate;
                if (at >= age) break;   // a pattern that has just returned plays as written
                // Pick one of the allowed changes.
                int allowed[5], m = 0;
                for (int bit = 0; bit < 5; ++bit)
                    if ((vary & (1 << bit)) && !(bit == 2 && kit)) allowed[m++] = bit;
                if (m == 0) break;
                switch (allowed[std::min(m - 1, int(u(2) * float(m)))]) {
                    case 0: c.delay += 0.25f; break;
                    case 1: c.velocity *= 0.65f + 0.7f * u(3); break;
                    case 2: c.pitch += (u(4) < 0.5f ? -1 : 1) * (pattern->followChords ? 2 : 1); break;
                    case 3: c.skip = !c.skip; break;
                    default: c.extra = true; break;
                }
            }
        }
        c.delay = std::min(c.delay, 0.75f);
        return c;
    }

    // Calls f(Event) for each note that is sounding at `beat` (started before it and not
    // yet over), exactly as play() would have played it. Held instruments use it to pick
    // up notes they missed: a Place just added, an element switched back on, a locate.
    template <typename F>
    void sounding(double beat, F&& f) const {
        if (pattern == nullptr || pattern->count <= 0 || pattern->lengthSteps <= 0) return;
        const Pattern& p = *pattern;
        const double perBeat = stepsPerBeat(p.resolution);
        const int64_t length = p.lengthSteps;
        const int perBar = stepsPerBar(p.resolution);
        int longest = 1;
        for (int i = 0; i < p.count; ++i) longest = std::max<int>(longest, p.notes[std::size_t(i)].length);
        const auto now = int64_t(std::floor(beat * perBeat));
        const int64_t lastPass = music::floorDiv(now, length), firstPass = music::floorDiv(now - longest - 3, length);
        for (int64_t pass = firstPass; pass <= lastPass; ++pass)
            for (int i = 0; i < p.count; ++i) {
                const Note& n = p.notes[std::size_t(i)];
                const int64_t s = pass * length + n.step;
                if (s > now) break;
                if (n.inTake()) continue;
                if (n.probability < 100 && unitOf(key, pass, i, 9) * 100.0f >= float(n.probability)) continue;
                const Change c = changeFor(i, pass, music::floorDiv(s, int64_t(perBar)));
                if (c.skip) continue;
                const double swingSteps = (s & 1) != 0 ? 0.45 * double(swing) : 0.0;
                const double at = (double(s) + swingSteps + double(c.delay)) / perBeat, len = double(n.length) / perBeat;
                if (at <= beat && at + len > beat)
                    f(Event { at, len, int(n.degree) + c.pitch, int(n.alter), dsp::clamp01(float(n.velocity) / 127.0f * c.velocity), i, false });
            }
    }

    // Calls f(Event) for each note that starts in [beat, beat + span).
    template <typename F>
    void play(double beat, double span, F&& f) const {
        if (pattern == nullptr || pattern->count <= 0 || pattern->lengthSteps <= 0) return;
        const Pattern& p = *pattern;
        const double perBeat = stepsPerBeat(p.resolution);
        const int64_t length = p.lengthSteps;
        const int perBar = stepsPerBar(p.resolution);
        // A note can be delayed by up to three steps (swing, timing, an extra note).
        const auto first = int64_t(std::floor(beat * perBeat)) - 3, last = int64_t(std::floor((beat + span) * perBeat));
        for (int64_t s = first; s <= last; ++s) {
            const int64_t pass = music::floorDiv(s, length), pos = s - pass * length;
            // Notes are sorted by step: find the first at pos.
            int lo = 0, hi = p.count;
            while (lo < hi) {
                const int mid = (lo + hi) / 2;
                if (p.notes[std::size_t(mid)].step < pos) lo = mid + 1;
                else hi = mid;
            }
            const int64_t bar = music::floorDiv(s, int64_t(perBar));
            for (int i = lo; i < p.count && p.notes[std::size_t(i)].step == pos; ++i) {
                const Note& n = p.notes[std::size_t(i)];
                if (n.inTake()) continue;
                if (n.probability < 100 && unitOf(key, pass, i, 9) * 100.0f >= float(n.probability)) continue;
                const Change c = changeFor(i, pass, bar);
                const double swingSteps = (s & 1) != 0 ? 0.45 * double(swing) : 0.0;
                const double at = (double(s) + swingSteps + double(c.delay)) / perBeat;
                const float v = float(n.velocity) / 127.0f;
                if (!c.skip && at >= beat && at < beat + span)
                    f(Event { at, double(n.length) / perBeat, int(n.degree) + c.pitch, int(n.alter), dsp::clamp01(v * c.velocity), i, false });
                if (c.extra) {
                    // An extra note one step later: a ghost hit, or a neighbouring note.
                    const double extraAt = at + 1.0 / perBeat;
                    const int shift = kit ? 0 : (unitOf(key, pass, i, 7) < 0.5f ? -1 : 1) * (p.followChords ? 2 : 1);
                    if (extraAt >= beat && extraAt < beat + span)
                        f(Event { extraAt, 1.0 / perBeat, int(n.degree) + shift, int(n.alter), 0.55f * v, -1, true });
                }
            }
        }
    }
};

// Semitones above the key root for a melodic event. Follow Chords: the chord root is
// placed in [base, base + 12) and the degree counts from it.
inline int semitonesFor(bool followChords, int base, int mood, int chord, int degree, int alter) {
    if (!followChords) return music::degree(mood, degree) + alter;
    const int rootSemis = music::degree(mood, chord);
    return music::inRange(rootSemis, base) + (music::degree(mood, chord + degree) - rootSemis) + alter;
}
inline int semitonesFor(const Pattern& p, int mood, int chord, int degree, int alter) {
    return semitonesFor(p.followChords, p.base, mood, chord, degree, alter);
}

} // namespace wb::pat
