#pragma once
// Places: self-contained musical engines, each scoring one setting (a castle, a
// village, a spaceport...). A Place is musical rather than literal: tonal and
// rhythmic parts in the song's key and tempo, with its own effect character.
//
// Every Place is an ensemble of up to six instruments (drone or bed, bass, rhythm,
// melody, accents, texture). Each instrument is an element: it plays its own
// pattern, with its own level, switch, mute and solo (see Ensemble.h).
//
// Up to four Places play at once, one per slot. The slot (PlaceSlot) handles
// level, distance, pan, crossfades and the beat clock; the Place only makes music.
#include "../DSP/Generator.h"
#include "../DSP/Music.h"
#include "../DSP/PlaceFx.h"
#include "../Patterns/Pattern.h"

namespace wb {

// The Place types, in selector order. The numbering is stable (saved in projects).
enum PlaceType {
    NoPlace, Castle, Village, Spaceport, Factory, City, Forest, Hive, Temple, Cavern, Ruins, Spaceship, Biodome, SeaShip, Suburbs, Mall,
    Military, Beach, Desert, Palace, Tavern, PlaceTypeCount
};
static_assert(int(PlaceTypeCount) == placeChoiceCount, "the Place choice parameter lists every Place");

inline constexpr std::array<const char*, PlaceTypeCount> placeNames {
    "Empty", "Castle", "Village", "Spaceport", "Factory", "City", "Forest", "Alien Hive", "Temple", "Cavern", "Ruins", "Spaceship",
    "Biodome", "Sea Ship", "Suburbs", "Mall", "Military Camp", "Beach", "Desert", "Palace", "Tavern"
};
// Lower-case keys: artwork folders (assets/places/<key>/) and file names.
inline constexpr std::array<const char*, PlaceTypeCount> placeKeys {
    "empty", "castle", "village", "spaceport", "factory", "city", "forest", "hive", "temple", "cavern", "ruins", "spaceship",
    "biodome", "seaship", "suburbs", "mall", "military", "beach", "desert", "palace", "tavern"
};
// Which Places sound yet. The others are listed (cards, names, instruments) and arrive
// in Milestones 5b-5d; until then they are silent and their cards cannot be added.
inline constexpr std::array<bool, PlaceTypeCount> placeImplemented { false, true, true, true, false, false, false, false, false, false, false,
                                                                     false, false, false, false, false, false, false, false, false, false };
// The sub-milestone each Place arrives in (shown on its card).
inline constexpr std::array<const char*, PlaceTypeCount> placeArrives { "", "", "", "", "5B", "5B", "5B", "5B", "5B", "5B", "5C",
                                                                        "5C", "5C", "5C", "5C", "5D", "5D", "5D", "5D", "5D", "5D" };

constexpr int placeElements = pat::elementsPerPlace, placeCharacters = 3;

enum PlaceCategory { MedievalFantasy, Modern, SciFi, Nature, PlaceCategoryCount };
inline constexpr std::array<const char*, PlaceCategoryCount> categoryNames { "MEDIEVAL & FANTASY", "MODERN", "SCI-FI", "NATURE" };

// A small glyph for each kind of instrument (the Pattern Editor's tabs, the detail panel).
enum InstrumentIcon {
    IconDrone, IconBell, IconHorn, IconTimpani, IconHarp, IconChoir, IconLute, IconMarimba, IconFlute, IconJingle, IconPulse, IconPad,
    IconRiser, IconPA, IconEngine, IconBlip, IconHandDrum, IconAnvil, IconPiston, IconSteam, IconGear, IconPiano, IconCrowd, IconSiren,
    IconBird, IconWave, IconGong, IconBowl, IconDrip, IconCrystal, IconWind, IconMusicBox, IconAlarm, IconStrings, IconGlass, IconShaker,
    IconSnare, IconRadio, IconBass, IconSwarm, InstrumentIconCount
};

// How an element plays its pattern: melodic (scale degrees) or a kit (each row a
// different drum or sound).
enum ElementKind { NoPattern, Melodic, Kit };

struct ElementSpec {
    const char* name = "-";
    int icon = IconDrone;
    int kind = NoPattern;
    std::array<const char*, 4> kit {};   // kit row names
    int rows = 0;
    uint8_t vary = pat::VaryVelocity | pat::VarySkip;   // default randomness switches
};

struct PlaceInfo {
    std::array<const char*, placeCharacters> characters { "-", "-", "-" };
    std::array<ElementSpec, placeElements> element {};
    const char* description = "";
    const char* effect = "";      // the Place's effect character
    int category = MedievalFantasy;
    int mood = -1;                // a suggested Mood (the user's choice always wins), -1: none
};

namespace detail {
using namespace pat;
inline ElementSpec mel(const char* name, int icon, uint8_t vary = VaryVelocity | VarySkip | VaryPitch) {
    ElementSpec e;
    e.name = name;
    e.icon = icon;
    e.kind = Melodic;
    e.vary = vary;
    return e;
}
inline ElementSpec kit(const char* name, int icon, std::array<const char*, 4> rows, uint8_t vary = VaryVelocity | VarySkip | VaryExtra) {
    ElementSpec e;
    e.name = name;
    e.icon = icon;
    e.kind = Kit;
    e.kit = rows;
    e.rows = 0;
    for (const char* r : rows) e.rows += r != nullptr ? 1 : 0;
    e.vary = vary;
    return e;
}
// A sustained instrument (drones, pads): long notes that follow the chords.
inline ElementSpec held(const char* name, int icon) { return mel(name, icon, VaryVelocity); }
inline PlaceInfo info(std::array<const char*, 3> characters, std::array<ElementSpec, placeElements> elements, const char* description,
                      const char* effect, int category, int mood = -1) {
    PlaceInfo i;
    i.characters = characters;
    i.element = elements;
    i.description = description;
    i.effect = effect;
    i.category = category;
    i.mood = mood;
    return i;
}
} // namespace detail

inline const PlaceInfo& placeInfo(int type) {
    using namespace detail;
    using pat::VaryVelocity, pat::VarySkip, pat::VaryPitch, pat::VaryExtra, pat::VaryTiming;
    static const std::array<PlaceInfo, PlaceTypeCount> table { {
        {},
        info({ "Fortress", "Royal Court", "Besieged" },
             { held("Brass Drone", IconDrone), mel("Bells", IconBell), mel("Horns", IconHorn, VaryVelocity | VarySkip),
               kit("Timpani", IconTimpani, { "Timpani", "Timpani 5th", "Side Drum", nullptr }), mel("Harp", IconHarp), held("Choir", IconChoir) },
             "Brass-like drones, tuned bells, horn calls, a processional timpani pulse, harp arpeggios and a choir.",
             "Stone-hall reverb and a slow tape echo", MedievalFantasy),
        info({ "Market Day", "Harvest Night", "Quiet Dawn" },
             { mel("Lute", IconLute, VaryVelocity | VarySkip | VaryPitch | VaryExtra), held("Hurdy-Gurdy", IconDrone),
               kit("Hand Drums", IconHandDrum, { "Frame Drum", "Hand Drum", "Shaker", nullptr }), mel("Marimba", IconMarimba),
               mel("Recorder", IconFlute), mel("Bells", IconJingle, VaryVelocity | VarySkip | VaryTiming) },
             "Lute-like plucks, hand drums, wooden marimba tones, a hurdy-gurdy drone, a recorder and small bell jingles.",
             "Warm tape saturation with wow and flutter", MedievalFantasy, 2),
        info({ "Busy Hub", "Abandoned Dock", "Launch" },
             { mel("Pulses", IconPulse), held("Pads", IconPad), kit("Risers", IconRiser, { "Pass-by", "Launch", "Falling", nullptr }, VarySkip),
               mel("PA Tones", IconPA, VaryVelocity | VarySkip), held("Engine", IconEngine), mel("Data Blips", IconBlip, VaryVelocity | VarySkip | VaryPitch | VaryExtra) },
             "Arpeggiated pulses, wide pads, thruster risers, PA tones, an engine sub drone and data blips.",
             "Wide ping-pong delay and a sci-fi shimmer", SciFi),
        info({ "Steam Mill", "Iron Forge", "Assembly Line" },
             { mel("Anvil", IconAnvil), kit("Pistons", IconPiston, { "Piston", "Valve", nullptr, nullptr }), kit("Steam", IconSteam, { "Swell", "Hiss", nullptr, nullptr }),
               held("Bowed Metal", IconStrings), kit("Gears", IconGear, { "Click", "Ratchet", nullptr, nullptr }), mel("Furnace Bass", IconBass) },
             "Tuned anvil hits, a piston ostinato, steam swells, a bowed-metal drone, gear clicks and a furnace bass.",
             "Gritty saturation and a metallic comb resonance", Modern),
        info({ "Rush Hour", "Night City", "Rain-soaked" },
             { mel("Chimes", IconBell), mel("Synth Bass", IconBass), mel("E-Piano", IconPiano), held("Crowd", IconCrowd),
               mel("Sirens", IconSiren), held("Traffic Hum", IconEngine) },
             "Muted bell chimes, a pulsing synth bass, electric-piano chords, a crowd texture in key, siren glides and a traffic hum.",
             "Dense short reverb and a subtle chorus", Modern),
        info({ "Enchanted", "Dark Woods", "Morning Glade" },
             { mel("Flute", IconFlute), mel("Birds", IconBird), kit("Wood", IconHandDrum, { "Log", "Stick", "Shaker", nullptr }),
               mel("Kalimba", IconMarimba), held("Strings", IconStrings), held("Rustle", IconWind) },
             "A breathy flute, melodic bird calls, wood percussion, kalimba plucks, a string pad and rustling leaves.",
             "Soft diffusion in a damped space", Nature, 4),
        info({ "Dormant", "Awakening", "Swarm" },
             { mel("Pulses", IconPulse), mel("Glass Calls", IconGlass), kit("Chitter", IconSwarm, { "Click", "Chirp", nullptr, nullptr }),
               mel("Throb Bass", IconBass), held("Formants", IconChoir), held("Swarm", IconSwarm) },
             "Modulated organic pulses, glassy FM calls, a chittering rhythm, a throbbing bass, formant drones and a swarm.",
             "Ring-modulated shimmer and a warped delay", SciFi, 6),
        info({ "Sacred Dawn", "Ancient Rite", "Forsaken" },
             { held("Choir", IconChoir), mel("Gongs", IconGong), mel("Bowls", IconBowl), held("Chant Drone", IconDrone),
               mel("Temple Bells", IconBell), mel("Zither", IconHarp) },
             "A formant choir pad, gongs, singing bowls, a low chant drone, temple bells and zither plucks.",
             "A long shimmer reverb", MedievalFantasy),
        info({ "Crystal Cave", "Deep Mine", "Flooded" },
             { mel("Drips", IconDrip), held("Sub Drone", IconDrone), mel("Metal Taps", IconAnvil), mel("Bowed Bass", IconStrings),
               mel("Crystals", IconCrystal), kit("Rumble", IconTimpani, { "Rumble", "Thud", nullptr, nullptr }) },
             "Tuned drips, a sub drone, echoing metal taps, a bowed low bass, crystal resonances and a distant rumble.",
             "A long dark reverb and slapback echo", Nature),
        info({ "Overgrown", "Haunted", "Desolate" },
             { mel("Piano Shards", IconPiano), mel("Music Box", IconMusicBox), mel("Broken Bells", IconBell), held("Wind", IconWind),
               held("Ghost Pad", IconPad), kit("Low Toms", IconTimpani, { "Low Tom", "Floor Tom", nullptr, nullptr }) },
             "Reversed piano fragments, a lo-fi music box, broken bells, cold wind harmonics, a ghostly pad and sparse low toms.",
             "Lo-fi decay and reverse echoes", MedievalFantasy),
        info({ "Cruising", "Red Alert", "Derelict" },
             { held("Engine", IconEngine), kit("Life Support", IconPulse, { "Pump", "Hiss", nullptr, nullptr }), mel("Computer", IconBlip),
               kit("Airlock", IconRiser, { "Cycle", "Seal", nullptr, nullptr }), mel("Hull", IconGong), mel("Alarm", IconAlarm) },
             "An engine drone, a life-support pulse, computer tone arpeggios, airlock swells, hull resonances and a soft alarm motif.",
             "A confined metallic room and a filtered delay", SciFi),
        info({ "Thriving", "Night Cycle", "Failing" },
             { held("Pads", IconPad), mel("Mallets", IconMarimba), mel("Birdsong", IconBird), mel("Water Plucks", IconDrip),
               held("Vents", IconWind), kit("Shaker", IconShaker, { "Shaker", "Tick", nullptr, nullptr }) },
             "Lush pads, bright mallet arpeggios, synthetic birdsong, water-like plucks, a ventilation hum and a gentle shaker.",
             "A bright airy reverb and a gentle chorus", SciFi, 4),
        info({ "Calm Seas", "Storm", "Ghost Ship" },
             { kit("Creaks", IconHandDrum, { "Creak", "Knock", nullptr, nullptr }), mel("Concertina", IconPiano), kit("Rope Swells", IconRiser, { "Pull", "Strain", nullptr, nullptr }),
               mel("Ship Bell", IconBell), held("Waves", IconWave), mel("Gulls", IconBird) },
             "Creaking wood percussion, concertina chords, rope-pull swells, a ship's bell, a wave drone and gull-like calls.",
             "Rolling modulation and a wide, wet reverb", Modern, 2),
        info({ "Sunny Afternoon", "Evening", "Uncanny" },
             { mel("E-Piano", IconPiano), mel("Pop Pulse", IconPulse), mel("Wind Chimes", IconJingle),
               kit("Sprinkler", IconShaker, { "Hat", "Tick", nullptr, nullptr }), mel("Music Box", IconMusicBox), mel("Bass", IconBass) },
             "Warm electric-piano chords, a soft pop pulse, wind chimes, a sprinkler-like hi-hat, a music-box motif and a mellow bass.",
             "Warm tape in a short room", Modern, 1),
        info({ "Busy Weekend", "Empty Night", "Dead Mall" },
             { mel("E-Piano", IconPiano), mel("Vibraphone", IconMarimba), mel("PA Chimes", IconPA), held("Escalator", IconEngine),
               held("Fountain", IconWave), kit("Drums", IconSnare, { "Kick", "Snare", "Hat", nullptr }) },
             "Smooth electric piano, vibraphone melodies, PA chime dings, an escalator hum, fountain shimmer and a soft drum groove.",
             "Glossy reverb with a slight detune", Modern, 4),
        info({ "Morning Drill", "On Alert", "Night Watch" },
             { kit("Snare March", IconSnare, { "Snare", "Rim", nullptr, nullptr }), mel("Bugle", IconHorn), kit("Low Drums", IconTimpani, { "Bass Drum", "Tom", nullptr, nullptr }),
               mel("Radio Blips", IconRadio), kit("Clanks", IconAnvil, { "Clank", "Rattle", nullptr, nullptr }), mel("Strings", IconStrings) },
             "Snare march patterns, bugle calls, a low drum pulse, tuned radio blips, metallic clanks and a tense string ostinato.",
             "A tight room and a gritty radio filter", Modern),
        info({ "Tropical Day", "Sunset", "Stormy Shore" },
             { mel("Guitar", IconLute), mel("Steel Drum", IconBowl), held("Waves", IconWave), kit("Shaker", IconShaker, { "Shaker", "Clave", nullptr, nullptr }),
               held("Breeze Pad", IconPad), mel("Gulls", IconBird) },
             "Warm guitar-like plucks, steel-drum tones, wave swells, a shaker, a breezy pad and gull calls.",
             "Open airy space and a gentle chorus", Nature, 5),
        info({ "Oasis", "Endless Dunes", "Sandstorm" },
             { mel("Oud", IconLute), kit("Frame Drums", IconHandDrum, { "Doum", "Tek", "Riq", nullptr }), mel("Ney Flute", IconFlute),
               held("Tanpura", IconDrone), held("Wind", IconWind), mel("Chimes", IconJingle) },
             "Oud-like plucks, frame drums, a ney-like flute, a tanpura-like drone, wind whistles and sparse metal chimes.",
             "A wide dry space and a long sparse echo", Nature, 3),
        info({ "Grand Ball", "Throne Room", "Fallen Palace" },
             { mel("Harpsichord", IconHarp), held("Strings", IconStrings), mel("Fanfare", IconHorn), mel("Glockenspiel", IconBell),
               kit("Timpani", IconTimpani, { "Timpani", "Timpani 5th", nullptr, nullptr }), held("Choir", IconChoir) },
             "Harpsichord arpeggios, string-ensemble pads, fanfare brass, a glockenspiel, courtly timpani and choir swells.",
             "A grand hall reverb", MedievalFantasy, 1),
        info({ "Lively Night", "Quiet Corner", "Brawl" },
             { mel("Lute Strum", IconLute), kit("Frame Drum", IconHandDrum, { "Frame Drum", "Rim", nullptr, nullptr }), mel("Fiddle", IconStrings),
               mel("Glass Clinks", IconGlass), mel("Squeezebox", IconPiano), kit("Stomps", IconTimpani, { "Stomp", "Clap", nullptr, nullptr }) },
             "Lute-like strumming, a frame-drum pulse, fiddle drones and melodies, tuned glass clinks, squeezebox chords and stomps.",
             "A warm wooden room with tape warmth", MedievalFantasy, 2),
    } };
    return table[std::size_t(std::clamp(type, 0, PlaceTypeCount - 1))];
}

// A slot's controls as the Place sees them: smoothed, with the World Controls
// already folded in (Population into Activity and effect depth, Danger into
// Tension, Scale into Size, Era and Temperature into Tone...). element[e] is the
// instrument's level with its switch, mute and solo applied.
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

// The element's pattern spec (NoPattern for Places that do not sound yet).
struct ElementPatternInfo {
    int kind = NoPattern;
    int rows = 0;
    std::array<const char*, 4> kit {};
    uint8_t vary = pat::VaryVelocity | pat::VarySkip;
};
inline ElementPatternInfo patternInfo(int type, int element) {
    ElementPatternInfo i;
    if (type <= NoPlace || type >= PlaceTypeCount || element < 0 || element >= placeElements || !placeImplemented[std::size_t(type)]) return i;
    const auto& e = placeInfo(type).element[std::size_t(element)];
    i.kind = e.kind;
    i.rows = e.rows;
    i.kit = e.kit;
    i.vary = e.vary;
    return i;
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
    int tag = 0;   // a Place's own extra notes (not from the pattern) carry a tag
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
    static constexpr int logSize = 1024;
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
    static constexpr int maxScheduled = 64;
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
