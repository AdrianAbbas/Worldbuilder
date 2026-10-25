#pragma once
// Each instrument's own effects (Milestone 6): a send to one of the shared reverbs,
// a send to one of the shared tempo delays, a character effect with its amount, a
// dark-to-bright tone tilt and a stereo width. This table holds the starting values
// for every instrument of every Place; a Place added to a slot starts from them.
// Kept free of other headers so the parameter table can use it.
#include <array>
#include <cstdint>

namespace wb {

enum ReverbType : uint8_t { RevRoom, RevHall, RevCathedral, RevPlate, RevCave, ReverbTypeCount };
enum DelayType : uint8_t { DlyTape, DlyPingPong, DlyDotted, DlyReverse, DelayTypeCount };
enum CharacterType : uint8_t { ChOff, ChSaturation, ChChorus, ChPhaser, ChFilter, ChLoFi, CharacterTypeCount };

inline constexpr const char* reverbChoices = "Room|Hall|Cathedral|Plate|Cave";
inline constexpr const char* delayChoices = "Tape|Ping-Pong|Dotted|Reverse";
inline constexpr const char* characterFxChoices = "Off|Saturation|Chorus|Phaser|Filter|Lo-Fi";

// The effect fields of one instrument, in parameter order.
enum FxField { FxReverb, FxReverbType, FxDelay, FxDelayType, FxCharacter, FxAmount, FxTone, FxWidth, FxFieldCount };

struct InstrumentFxDefaults {
    uint8_t reverbType = RevRoom;
    float reverb = 0.15f;
    uint8_t delayType = DlyTape;
    float delay = 0;
    uint8_t character = ChOff;
    float amount = 0.35f;
    float tone = 0.5f, width = 1.0f;
    constexpr InstrumentFxDefaults echo(uint8_t type, float send) const {
        auto d = *this;
        d.delayType = type;
        d.delay = send;
        return d;
    }
    constexpr InstrumentFxDefaults with(uint8_t type, float a) const {
        auto d = *this;
        d.character = type;
        d.amount = a;
        return d;
    }
    constexpr InstrumentFxDefaults toned(float t) const {
        auto d = *this;
        d.tone = t;
        return d;
    }
    constexpr InstrumentFxDefaults wide(float w) const {
        auto d = *this;
        d.width = w;
        return d;
    }
    constexpr float field(int f) const {
        switch (f) {
            case FxReverb: return reverb;
            case FxReverbType: return float(reverbType);
            case FxDelay: return delay;
            case FxDelayType: return float(delayType);
            case FxCharacter: return float(character);
            case FxAmount: return amount;
            case FxTone: return tone;
            default: return width;
        }
    }
};

namespace fxdetail {
constexpr InstrumentFxDefaults verb(uint8_t type, float send) {
    InstrumentFxDefaults d;
    d.reverbType = type;
    d.reverb = send;
    return d;
}
constexpr auto room(float s) { return verb(RevRoom, s); }
constexpr auto hall(float s) { return verb(RevHall, s); }
constexpr auto cathedral(float s) { return verb(RevCathedral, s); }
constexpr auto plate(float s) { return verb(RevPlate, s); }
constexpr auto cave(float s) { return verb(RevCave, s); }
} // namespace fxdetail

// Indexed by Place type (the PlaceType order in Places/Place.h) and instrument.
// Every Place keeps its own effect character on top of these: the sends add each
// instrument's own space, so they stay moderate.
inline constexpr std::array<std::array<InstrumentFxDefaults, 6>, 27> instrumentFxTable = [] {
    using namespace fxdetail;
    std::array<std::array<InstrumentFxDefaults, 6>, 27> t {};
    for (auto& place : t)
        for (auto& e : place) e = InstrumentFxDefaults {};
    // Castle: Brass Drone, Bells, Horns, Timpani, Harp, Choir.
    t[1] = { hall(0.25f).with(ChSaturation, 0.2f).toned(0.45f).wide(1.2f), cathedral(0.35f).echo(DlyTape, 0.15f).toned(0.55f).wide(1.2f),
             hall(0.3f).echo(DlyTape, 0.12f).with(ChSaturation, 0.15f), hall(0.25f), hall(0.25f).echo(DlyDotted, 0.12f).wide(1.3f),
             cathedral(0.4f).wide(1.4f) };
    // Village: Lute, Hurdy-Gurdy, Hand Drums, Marimba, Recorder, Bells.
    t[2] = { room(0.2f), room(0.15f).with(ChSaturation, 0.2f).toned(0.45f), room(0.2f), room(0.2f).echo(DlyDotted, 0.1f),
             hall(0.2f).echo(DlyTape, 0.1f), room(0.25f).wide(1.3f) };
    // Spaceport: Pulses, Pads, Risers, PA Tones, Engine, Data Blips.
    t[3] = { plate(0.2f).echo(DlyPingPong, 0.3f).with(ChFilter, 0.3f), hall(0.35f).with(ChChorus, 0.4f).wide(1.6f), hall(0.3f).echo(DlyPingPong, 0.15f),
             hall(0.3f).echo(DlyDotted, 0.2f), room(0.1f).with(ChSaturation, 0.3f).toned(0.4f).wide(0.8f),
             plate(0.25f).echo(DlyPingPong, 0.35f).with(ChLoFi, 0.2f).wide(1.5f) };
    // Factory: Anvil, Pistons, Steam, Bowed Metal, Gears, Furnace Bass.
    t[4] = { room(0.25f).echo(DlyTape, 0.15f).with(ChSaturation, 0.1f), room(0.15f).with(ChSaturation, 0.3f), room(0.2f).wide(1.4f),
             hall(0.3f).with(ChPhaser, 0.25f), room(0.15f).echo(DlyDotted, 0.1f).with(ChLoFi, 0.15f),
             room(0.08f).with(ChSaturation, 0.35f).toned(0.4f).wide(0.6f) };
    // City: Chimes, Synth Bass, E-Piano, Crowd, Sirens, Traffic Hum.
    t[5] = { hall(0.25f).echo(DlyDotted, 0.15f), room(0.05f).with(ChSaturation, 0.2f).toned(0.45f).wide(0.6f), room(0.2f).with(ChPhaser, 0.35f).wide(1.3f),
             room(0.2f).wide(1.5f), hall(0.2f).echo(DlyTape, 0.15f), room(0.1f).with(ChFilter, 0.3f).wide(1.2f) };
    // Forest: Flute, Birds, Wood, Kalimba, Strings, Rustle.
    t[6] = { hall(0.3f).echo(DlyTape, 0.12f), hall(0.25f).echo(DlyDotted, 0.1f).wide(1.5f), room(0.25f), room(0.2f).echo(DlyDotted, 0.15f),
             hall(0.35f).wide(1.4f), room(0.15f).wide(1.6f) };
    // Alien Hive: Pulses, Glass Calls, Chitter, Throb Bass, Formants, Swarm.
    t[7] = { cave(0.25f).echo(DlyPingPong, 0.2f).with(ChPhaser, 0.3f), cave(0.35f).echo(DlyReverse, 0.2f).wide(1.4f), cave(0.2f).with(ChLoFi, 0.2f),
             room(0.05f).with(ChSaturation, 0.35f).toned(0.4f).wide(0.6f), cave(0.35f).with(ChChorus, 0.3f).wide(1.5f),
             cave(0.25f).with(ChPhaser, 0.3f).wide(1.6f) };
    // Temple: Choir, Gongs, Bowls, Chant Drone, Temple Bells, Zither.
    t[8] = { cathedral(0.45f).wide(1.5f), cathedral(0.4f), cathedral(0.35f).wide(1.3f), cathedral(0.3f).wide(1.2f),
             cathedral(0.4f).echo(DlyTape, 0.1f), hall(0.25f).echo(DlyDotted, 0.12f) };
    // Cavern: Drips, Sub Drone, Metal Taps, Bowed Bass, Crystals, Rumble.
    t[9] = { cave(0.4f).echo(DlyDotted, 0.15f), cave(0.15f).toned(0.4f).wide(0.8f), cave(0.35f).echo(DlyTape, 0.2f), cave(0.25f).toned(0.45f),
             cave(0.4f).echo(DlyReverse, 0.15f).wide(1.4f), cave(0.3f).toned(0.4f) };
    // Ruins: Piano Shards, Music Box, Broken Bells, Wind, Ghost Pad, Low Toms.
    t[10] = { hall(0.35f).echo(DlyReverse, 0.25f).with(ChLoFi, 0.15f), hall(0.3f).echo(DlyTape, 0.2f).with(ChLoFi, 0.3f),
              cathedral(0.35f).echo(DlyTape, 0.15f), hall(0.2f).wide(1.6f), cathedral(0.4f).with(ChChorus, 0.3f).wide(1.5f), hall(0.3f).toned(0.45f) };
    // Spaceship: Engine, Life Support, Computer, Airlock, Hull, Alarm.
    t[11] = { room(0.1f).with(ChFilter, 0.25f).wide(0.9f), room(0.15f).with(ChLoFi, 0.1f), plate(0.2f).echo(DlyPingPong, 0.25f),
              room(0.2f).with(ChPhaser, 0.2f), plate(0.3f).echo(DlyTape, 0.1f), room(0.2f).echo(DlyDotted, 0.15f).with(ChSaturation, 0.15f) };
    // Biodome: Pads, Mallets, Birdsong, Water Plucks, Vents, Shaker.
    t[12] = { hall(0.35f).with(ChChorus, 0.35f).wide(1.6f), plate(0.2f).echo(DlyDotted, 0.2f), hall(0.25f).echo(DlyPingPong, 0.15f).wide(1.5f),
              plate(0.25f).echo(DlyDotted, 0.15f), room(0.15f).with(ChFilter, 0.2f).wide(1.3f), room(0.15f).wide(1.2f) };
    // Sea Ship: Creaks, Concertina, Rope Swells, Ship Bell, Waves, Gulls.
    t[13] = { room(0.25f), room(0.2f).with(ChSaturation, 0.1f), room(0.2f), hall(0.35f).echo(DlyTape, 0.15f), hall(0.2f).wide(1.6f),
              hall(0.3f).echo(DlyDotted, 0.1f).wide(1.4f) };
    // Suburbs: E-Piano, Pop Pulse, Wind Chimes, Sprinkler, Music Box, Bass.
    t[14] = { room(0.2f).with(ChChorus, 0.3f), room(0.15f).echo(DlyDotted, 0.2f), hall(0.25f).wide(1.4f), room(0.15f).wide(1.3f),
              room(0.2f).echo(DlyTape, 0.15f).with(ChLoFi, 0.2f), room(0.05f).with(ChSaturation, 0.15f).wide(0.6f) };
    // Mall: E-Piano, Vibraphone, PA Chimes, Escalator, Fountain, Drums.
    t[15] = { plate(0.25f).with(ChChorus, 0.35f), plate(0.25f).echo(DlyDotted, 0.15f).with(ChChorus, 0.2f), hall(0.35f).echo(DlyDotted, 0.15f),
              room(0.15f).with(ChFilter, 0.2f), plate(0.2f).wide(1.6f), room(0.2f) };
    // Military Camp: Snare March, Bugle, Low Drums, Radio Blips, Clanks, Strings.
    t[16] = { room(0.2f), hall(0.3f).echo(DlyTape, 0.1f), hall(0.25f).toned(0.45f), room(0.1f).echo(DlyPingPong, 0.2f).with(ChLoFi, 0.3f),
              room(0.25f).echo(DlyTape, 0.1f), hall(0.3f) };
    // Beach: Guitar, Steel Drum, Waves, Shaker, Breeze Pad, Gulls.
    t[17] = { room(0.2f).with(ChChorus, 0.2f), plate(0.2f).echo(DlyDotted, 0.15f), hall(0.2f).wide(1.6f), room(0.15f).wide(1.3f),
              hall(0.35f).with(ChChorus, 0.3f).wide(1.6f), hall(0.3f).echo(DlyDotted, 0.1f).wide(1.4f) };
    // Desert: Oud, Frame Drums, Ney Flute, Tanpura, Wind, Chimes.
    t[18] = { room(0.2f).echo(DlyTape, 0.15f), room(0.25f), hall(0.3f).echo(DlyTape, 0.2f), hall(0.25f).wide(1.3f), hall(0.2f).wide(1.6f),
              hall(0.3f).echo(DlyDotted, 0.15f).wide(1.3f) };
    // Palace: Harpsichord, Strings, Fanfare, Glockenspiel, Timpani, Choir.
    t[19] = { hall(0.25f), hall(0.35f).wide(1.4f), hall(0.35f).echo(DlyTape, 0.08f), hall(0.3f).echo(DlyDotted, 0.1f), hall(0.3f),
              cathedral(0.4f).wide(1.4f) };
    // Tavern: Lute Strum, Frame Drum, Fiddle, Glass Clinks, Squeezebox, Stomps.
    t[20] = { room(0.25f), room(0.25f), room(0.25f).echo(DlyTape, 0.08f), room(0.3f).wide(1.3f), room(0.2f).with(ChSaturation, 0.1f), room(0.3f) };
    // Space Station: Hull Drone, Docking Clamps, Comms Beeps, Airflow, Reactor Pulse, Zero-G Lead.
    t[21] = { room(0.15f).with(ChFilter, 0.2f).wide(1.3f), plate(0.2f).echo(DlyTape, 0.1f), plate(0.2f).echo(DlyPingPong, 0.25f),
              room(0.15f).wide(1.6f), room(0.05f).with(ChSaturation, 0.2f).toned(0.45f).wide(0.6f), hall(0.35f).echo(DlyDotted, 0.2f).with(ChChorus, 0.3f).wide(1.5f) };
    // Cyberpunk Megacity: Grit Bass, Neon Arp, Rain Pad, Drum Machine, Glitch Stabs, Megastructure.
    t[22] = { room(0.05f).with(ChSaturation, 0.35f).toned(0.45f).wide(0.6f), plate(0.25f).echo(DlyPingPong, 0.3f).wide(1.4f),
              hall(0.35f).with(ChChorus, 0.4f).wide(1.7f), room(0.15f).with(ChSaturation, 0.25f), plate(0.2f).echo(DlyDotted, 0.15f).with(ChLoFi, 0.35f).wide(1.3f),
              hall(0.25f).with(ChFilter, 0.2f).wide(1.2f) };
    // Starship Bridge: Consoles, Warp Core, Tactical Pulse, Alert Brass, Crew Chatter, Sensor Pings.
    t[23] = { plate(0.2f).echo(DlyPingPong, 0.2f).wide(1.4f), hall(0.2f).with(ChFilter, 0.2f).wide(1.2f), room(0.15f).with(ChSaturation, 0.15f),
              hall(0.35f).wide(1.5f), room(0.2f).with(ChLoFi, 0.15f).wide(1.3f), hall(0.35f).echo(DlyDotted, 0.25f).wide(1.6f) };
    return t;
}();

inline constexpr const InstrumentFxDefaults& instrumentFx(int type, int element) {
    return instrumentFxTable[std::size_t(type < 0 || type > 26 ? 0 : type)][std::size_t(element < 0 || element > 5 ? 0 : element)];
}
inline constexpr float instrumentFxDefault(int type, int element, int field) { return instrumentFx(type, element).field(field); }

} // namespace wb
