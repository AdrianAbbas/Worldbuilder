#pragma once
// Places: self-contained musical engines, each scoring one setting (a castle, a
// village, a spaceport...). A Place is musical rather than literal: tonal and
// rhythmic parts in the song's key and tempo, with its own effect character.
//
// Up to four Places play at once, one per slot. The slot (WorldEngine) handles
// level, distance, pan, crossfades and the beat clock; the Place only makes music.
#include "../DSP/Generator.h"
#include "../DSP/Music.h"
#include "../DSP/PlaceFx.h"
#include "../Patterns/Pattern.h"

namespace wb {

// The Place types, in selector order. The first three are built in this
// milestone; the rest arrive in Milestone 4 and are silent until then.
enum PlaceType { NoPlace, Castle, Village, Spaceport, Factory, City, Temple, Forest, Ruins, Cavern, Hive, PlaceTypeCount };

inline constexpr std::array<const char*, PlaceTypeCount> placeNames { "Empty", "Castle", "Village", "Spaceport", "Factory", "City",
                                                                      "Temple", "Forest", "Ruins", "Cavern", "Alien Hive" };
inline constexpr std::array<bool, PlaceTypeCount> placeImplemented { false, true, true, true, false, false,
                                                                     false, false, false, false, false };

constexpr int placeElements = 5, placeCharacters = 3;

struct PlaceInfo {
    std::array<const char*, placeCharacters> characters;
    std::array<const char*, placeElements> elements;
    const char* description;
};

inline const PlaceInfo& placeInfo(int type) {
    static const std::array<PlaceInfo, PlaceTypeCount> table { {
        { { "-", "-", "-" }, { "-", "-", "-", "-", "-" }, "" },
        { { "Fortress", "Royal Court", "Besieged" }, { "Drones", "Bells", "Horns", "Pulse", "Hall" },
          "Low brass drones, distant tuned bells, horn calls and a slow processional pulse in a stone hall." },
        { { "Market Day", "Hearth", "Festival" }, { "Plucks", "Drone", "Hand Drums", "Wood", "Tape" },
          "Warm plucked ostinatos, a folk drone, hand percussion and wooden tones through tape warmth." },
        { { "Busy Hub", "Abandoned Dock", "Launch" }, { "Pulses", "Pads", "Risers", "Announce", "Engines" },
          "Arpeggiated pulses, wide pads, thruster risers, PA chimes and engine hum through a ping-pong delay." },
        { { "-", "-", "-" }, { "-", "-", "-", "-", "-" }, "Metallic tuned anvil hits and a pulsing ostinato. Milestone 4." },
        { { "-", "-", "-" }, { "-", "-", "-", "-", "-" }, "Layered rhythmic motion and muted bell tones. Milestone 4." },
        { { "-", "-", "-" }, { "-", "-", "-", "-", "-" }, "Choir-like pads, gongs and singing bowls. Milestone 4." },
        { { "-", "-", "-" }, { "-", "-", "-", "-", "-" }, "Breathy pads and bird-like melodies in scale. Milestone 4." },
        { { "-", "-", "-" }, { "-", "-", "-", "-", "-" }, "Decayed, reversed fragments and haunted tones. Milestone 4." },
        { { "-", "-", "-" }, { "-", "-", "-", "-", "-" }, "Resonant tuned drips and deep drones. Milestone 4." },
        { { "-", "-", "-" }, { "-", "-", "-", "-", "-" }, "Organic, pulsing, alien calls. Milestone 4." },
    } };
    return table[std::size_t(std::clamp(type, 0, PlaceTypeCount - 1))];
}

// A slot's controls as the Place sees them: smoothed, with the World Controls
// already folded in (Population into Activity and effect depth, Danger into
// Tension, Scale into Size, Era and Temperature into Tone...).
struct PlaceSettings {
    int character = 0;
    float activity = 0.5f, tension = 0.3f, density = 0.5f, swing = 0.15f, tone = 0.5f, size = 0.5f, variation = 0.35f;
    float fxDepth = 0.5f, life = 0.3f, machinery = 0, magic = 0.25f, era = 0.3f;
    std::array<float, placeElements> element {};
    bool sync = true;
};

struct PlaceContext {
    const WorldFrame* world = nullptr;
    uint64_t clock = 0;   // absolute sample of the chunk start
    double beat = 0;      // the Place's beat position at the chunk start
    double beatsPerSample = 0;
    float bpm = 96;
    int chord = 0;        // scale degree of the current chord (shared harmonic clock)
    PlaceSettings s;
    // How each element plays its pattern (pattern == nullptr: the element has none).
    std::array<pat::Playback, placeElements> play {};
};

// A deterministic random value per (salt, position), for the pattern generators.
struct Chance {
    dsp::Stream stream;
    float at(int64_t pos, uint32_t salt) const { return stream.unit((uint64_t(salt) << 48) ^ uint64_t(pos)); }
};

// Calls f(step) for each step of a grid with `perBeat` steps per beat that starts in
// [beat, beat + span). Odd steps are delayed by swing (up to 45 % of a step).
template <typename F>
void forEachStep(double beat, double span, int perBeat, float swing, F&& f) {
    const auto first = int64_t(std::floor(beat * perBeat)) - 1, last = int64_t(std::floor((beat + span) * perBeat));
    for (int64_t s = first; s <= last; ++s) {
        const double at = (double(s) + ((s & 1) != 0 ? 0.45 * double(swing) : 0.0)) / double(perBeat);
        if (at >= beat && at < beat + span) f(s);
    }
}

// Per-chunk glide coefficient for SynthVoice::glide: about `seconds` to arrive.
inline float glideFor(float seconds, float rate) { return std::exp(-float(dsp::chunk) / (std::max(seconds, 0.001f) * rate)); }
// Position of a sixteenth-note step within its 4/4 bar (0..15).
inline int stepInBar(int64_t step) { return int(((step % 16) + 16) % 16); }

// Which elements of a Place play from a pattern, and how: melodic (scale degrees) or
// a kit (each row a different drum or sound).
enum ElementKind { NoPattern, Melodic, Kit };
struct ElementPatternInfo {
    int kind = NoPattern;
    int rows = 0;                              // kit rows
    std::array<const char*, 4> kit {};         // kit row names
    uint8_t vary = pat::VaryVelocity | pat::VarySkip;   // default randomness switches
};

inline const ElementPatternInfo& patternInfo(int type, int element) {
    using namespace pat;
    static const ElementPatternInfo none {};
    using Row = std::array<ElementPatternInfo, placeElements>;
    const auto melodic = [](uint8_t vary) { ElementPatternInfo i; i.kind = Melodic; i.vary = vary; return i; };
    const auto kit = [](const char* a, const char* b, const char* c, uint8_t vary) {
        ElementPatternInfo i;
        i.kind = Kit;
        i.rows = 3;
        i.kit = { a, b, c, nullptr };
        i.vary = vary;
        return i;
    };
    static const std::array<Row, 4> table {
        Row { none, none, none, none, none },
        // Castle: Drones, Bells, Horns, Pulse, Hall
        Row { none, melodic(VaryVelocity | VarySkip | VaryPitch), melodic(VaryVelocity | VarySkip),
              kit("Timpani", "Timpani 5th", "Side Drum", VaryVelocity | VarySkip | VaryExtra), none },
        // Village: Plucks, Drone, Hand Drums, Wood, Tape
        Row { melodic(VaryVelocity | VarySkip | VaryPitch | VaryExtra), none, kit("Frame Drum", "Hand Drum", "Shaker", VaryVelocity | VarySkip | VaryExtra),
              melodic(VaryVelocity | VarySkip | VaryPitch), none },
        // Spaceport: Pulses, Pads, Risers, Announce, Engines
        Row { melodic(VaryVelocity | VarySkip | VaryPitch), none, kit("Pass-by", "Launch", "Falling", VarySkip), melodic(VaryVelocity | VarySkip), none },
    };
    if (type < 0 || type >= int(table.size()) || element < 0 || element >= placeElements) return none;
    return table[std::size_t(type)][std::size_t(element)];
}
inline bool hasPatterns(int type) {
    for (int e = 0; e < placeElements; ++e)
        if (patternInfo(type, e).kind != NoPattern) return true;
    return false;
}
inline pat::Controls defaultControls(int type, int element) {
    pat::Controls c;
    c.vary = patternInfo(type, element).vary;
    return c;
}

// What a pattern generator reads: the Place's settings that shape its patterns.
struct GenInputs {
    int character = 0, mood = 0;
    float activity = 0.5f, density = 0.5f, tension = 0.3f, life = 0.3f;
    uint32_t key = 0;
    uint8_t variant = 0;
    bool operator==(const GenInputs& o) const {
        return character == o.character && mood == o.mood && activity == o.activity && density == o.density && tension == o.tension
               && life == o.life && key == o.key && variant == o.variant;
    }
};

// A note scheduled within the chunk being rendered, at a sample offset.
struct Scheduled {
    int offset = 0, element = 0;
    pat::Event event;
    int chord = 0;
};

class Place {
public:
    virtual ~Place() = default;
    virtual void prepare(double sampleRate) = 0;
    virtual void reset() = 0;
    virtual void setSeed(uint32_t key) = 0;
    // Writes n <= dsp::chunk samples per channel.
    virtual void render(const PlaceContext& context, float* left, float* right, int n) = 0;
    // The Place's own tempo when its Rhythm is free (not synced to the host).
    virtual float freeTempo() const = 0;
    // How much of the Place feeds the World Space (its own space is built in).
    virtual float spaceSend() const { return 0.35f; }
    // Writes the starting pattern of a patterned element. A pure function of the
    // inputs (no audio state), so the same inputs always give the same pattern.
    virtual void generate(int element, const GenInputs& in, pat::Pattern& out) const = 0;

    // Every note or hit the Place plays is logged, for the tests.
    struct Logged {
        uint64_t clock = 0;   // sample position
        int element = -1;
        float pitch = 0;      // semitones above the key root (kit: the row)
        float velocity = 0;
    };
    static constexpr int logSize = 512;
    uint32_t eventCount() const { return events; }
    uint64_t eventClock(uint32_t i) const { return eventLog[std::size_t(i % logSize)].clock; }
    const Logged& logged(uint32_t i) const { return eventLog[std::size_t(i % logSize)]; }
    void clearLog() { events = 0; }

protected:
    void logEvent(uint64_t clock, int element = -1, float pitch = 0, float velocity = 0) {
        eventLog[std::size_t(events++ % logSize)] = { clock, element, pitch, velocity };
    }

    // Collects the pattern notes of every sounding element that start in this chunk,
    // at their sample offsets, into `out` (sorted by offset). Returns the count.
    static constexpr int maxScheduled = 48;
    int schedule(const PlaceContext& c, int n, std::array<Scheduled, maxScheduled>& out, int count = 0) const {
        const double span = c.beatsPerSample * double(n);
        const float danger = (*c.world)[Danger];
        for (int e = 0; e < placeElements; ++e) {
            const auto& play = c.play[std::size_t(e)];
            if (play.pattern == nullptr || c.s.element[std::size_t(e)] <= 0.001f) continue;
            play.play(c.beat, span, [&](const pat::Event& ev) {
                if (count >= maxScheduled) return;
                Scheduled x;
                x.offset = std::clamp(int(std::ceil((ev.beat - c.beat) / c.beatsPerSample - 1.0e-9)), 0, n - 1);
                x.element = e;
                x.event = ev;
                // Synced Places take the chord at the note itself; free ones the current chord.
                x.chord = c.s.sync ? music::chordDegree(ev.beat, danger) : c.chord;
                out[std::size_t(count++)] = x;
            });
        }
        sortScheduled(out, count);
        return count;
    }
    static void sortScheduled(std::array<Scheduled, maxScheduled>& out, int count) {
        for (int i = 1; i < count; ++i) {
            const Scheduled x = out[std::size_t(i)];
            int j = i;
            while (j > 0 && out[std::size_t(j - 1)].offset > x.offset) {
                out[std::size_t(j)] = out[std::size_t(j - 1)];
                --j;
            }
            out[std::size_t(j)] = x;
        }
    }

private:
    uint32_t events = 0;
    std::array<Logged, logSize> eventLog {};
};

} // namespace wb
