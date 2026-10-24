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
};

// A random value per sequencer step for a pattern that repeats every `length`
// steps. Variation is the chance that a step departs from the pattern on a given
// pass, so 0 repeats exactly and 1 never repeats.
struct Pattern {
    dsp::Stream stream;
    float at(int64_t step, int64_t length, float variation, uint32_t salt) const {
        const int64_t pass = music::floorDiv(step, length), pos = step - pass * length;
        const uint64_t s = uint64_t(salt) << 48;
        const bool varied = stream.unit(s ^ (uint64_t(pass) << 20) ^ uint64_t(pos) ^ (1ull << 47)) < variation;
        return varied ? stream.unit(s ^ (uint64_t(step) + (1ull << 46))) : stream.unit(s ^ uint64_t(pos));
    }
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

// A short melodic phrase (horn call, carillon run, PA chime): notes at beat
// offsets from its start, in semitones above the key root.
struct Phrase {
    struct Note {
        float beat = 0, length = 1, velocity = 1;
        int semitones = 0;
    };
    std::array<Note, 12> notes {};
    int count = 0, next = 0;
    double start = 0;
    bool active() const { return next < count; }
    void begin(double at) {
        start = at;
        next = 0;
    }
    void clear() { count = next = 0; }
    void add(float beat, int semitones, float length, float velocity = 1) {
        if (count < int(notes.size())) notes[std::size_t(count++)] = { beat, length, velocity, semitones };
    }
    // Calls f(note) for each note that starts before beat + span.
    template <typename F>
    void play(double beat, double span, F&& f) {
        while (next < count && start + double(notes[std::size_t(next)].beat) < beat + span) f(notes[std::size_t(next++)]);
    }
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

    // Every note or hit the Place plays is logged (sample position), for the tests.
    static constexpr int logSize = 512;
    uint32_t eventCount() const { return events; }
    uint64_t eventClock(uint32_t i) const { return eventLog[std::size_t(i % logSize)]; }
    void clearLog() { events = 0; }

protected:
    void logEvent(uint64_t clock) { eventLog[std::size_t(events++ % logSize)] = clock; }

private:
    uint32_t events = 0;
    std::array<uint64_t, logSize> eventLog {};
};

} // namespace wb
