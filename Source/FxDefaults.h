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
inline constexpr const char* slideChoices = "Off|On";

// The effect fields of one instrument, in parameter order. Scoop / Glide (after
// Milestone 15a) is kept with them: whether the instrument's notes slide in pitch (a
// scoop or a bend into each note, glides between held notes), how far and how long.
enum FxField { FxReverb, FxReverbType, FxDelay, FxDelayType, FxCharacter, FxAmount, FxTone, FxWidth, FxSlide, FxSlideAmount, FxSlideTime, FxFieldCount };

struct InstrumentFxDefaults {
    uint8_t reverbType = RevRoom;
    float reverb = 0.15f;
    uint8_t delayType = DlyTape;
    float delay = 0;
    uint8_t character = ChOff;
    float amount = 0.35f;
    float tone = 0.5f, width = 1.0f;
    // Scoop / Glide: off unless the slide is part of the instrument (see slideTable).
    // The amount (semitones) and time (seconds) are the instrument's own slide.
    uint8_t slide = 0;
    float slideSemis = 0.5f, slideSeconds = 0.08f;
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
            case FxSlide: return float(slide);
            case FxSlideAmount: return slideSemis;
            case FxSlideTime: return slideSeconds;
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

// Scoop / Glide per instrument, indexed as instrumentFxTable. `semis` and `seconds`
// are the instrument's own slide as its Place plays it in its first Character (a scoop
// or a bend into each note: how far, how quickly; for instruments that only glide, the
// glide's time); the Place keeps its Characters' differences in proportion. Instruments
// without a slide of their own get a plain scoop from below (0.5 semitone, 80 ms) when
// switched on. On by default only where the slide is the instrument: bird calls and
// gulls swooping, a drum skin's pitch falling after the strike, a drip's rising bloop,
// the Temple gongs' glide, the City sirens. Generated from the Places' own patches
// (Tests: --only slidetable prints it; the "slide" group checks it still matches).
struct SlideDefault {
    uint8_t on = 0;
    float semis = 0.5f, seconds = 0.08f;
};
// clang-format off
inline constexpr std::array<std::array<SlideDefault, 6>, 27> slideTable = [] {
    std::array<std::array<SlideDefault, 6>, 27> t {};
    for (auto& place : t)
        for (auto& e : place) e = SlideDefault {};
    const auto s = [](float semis, float seconds) { return SlideDefault { 0, semis, seconds }; };
    const auto on = [](float semis, float seconds) { return SlideDefault { 1, semis, seconds }; };
    // Castle: Brass Drone, Bells, Horns, Timpani, Harp, Choir
    t[1] = { s(0.350f, 1.400f), s(0.500f, 0.080f), s(0.350f, 0.028f), on(1.650f, 0.035f), s(0.500f, 0.080f), s(0.000f, 0.600f) };
    // Village: Lute, Hurdy-Gurdy, Hand Drums, Marimba, Recorder, Bells
    t[2] = { s(0.500f, 0.080f), s(0.500f, 0.080f), on(1.332f, 0.035f), s(0.500f, 0.080f), s(0.500f, 0.060f), s(0.500f, 0.080f) };
    // Spaceport: Pulses, Pads, Risers, PA Tones, Engine, Data Blips
    t[3] = { s(0.500f, 0.080f), s(0.000f, 2.000f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.000f, 0.500f), s(0.500f, 0.080f) };
    // Factory: Anvil, Pistons, Steam, Bowed Metal, Gears, Furnace Bass
    t[4] = { s(0.500f, 0.080f), on(5.196f, 0.035f), s(0.500f, 0.080f), s(0.000f, 0.800f), on(0.845f, 0.035f), s(0.500f, 0.080f) };
    // City: Chimes, Synth Bass, E-Piano, Crowd, Sirens, Traffic Hum
    t[5] = { s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.000f, 0.300f), on(0.000f, 0.350f), s(0.000f, 1.000f) };
    // Forest: Flute, Birds, Wood, Kalimba, Strings, Rustle
    t[6] = { s(0.500f, 0.060f), on(3.000f, 0.025f), on(0.845f, 0.035f), s(0.500f, 0.080f), s(0.000f, 1.200f), s(0.500f, 0.080f) };
    // Alien Hive: Pulses, Glass Calls, Chitter, Throb Bass, Formants, Swarm
    t[7] = { s(0.500f, 0.080f), s(0.500f, 0.080f), on(8.137f, 0.035f), s(0.000f, 0.300f), s(0.000f, 0.800f), s(0.000f, 0.500f) };
    // Temple: Choir, Gongs, Bowls, Chant Drone, Temple Bells, Zither
    t[8] = { s(0.000f, 1.000f), on(0.200f, 0.700f), s(0.500f, 0.080f), s(0.000f, 1.000f), s(0.500f, 0.080f), s(0.500f, 0.080f) };
    // Cavern: Drips, Sub Drone, Metal Taps, Bowed Bass, Crystals, Rumble
    t[9] = { on(5.000f, 0.044f), s(0.000f, 1.000f), s(0.500f, 0.080f), s(0.090f, 0.200f), s(0.500f, 0.080f), on(5.825f, 0.035f) };
    // Ruins: Piano Shards, Music Box, Broken Bells, Wind, Violin Textures, Low Toms
    t[10] = { s(0.500f, 0.080f), s(0.150f, 1.500f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.000f, 1.000f), on(2.420f, 0.035f) };
    // Spaceship: Engine, Life Support, Computer, Airlock, Hull, Alarm
    t[11] = { s(0.000f, 1.000f), on(3.863f, 0.035f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.000f, 0.400f), s(0.500f, 0.080f) };
    // Biodome: Pads, Mallets, Birdsong, Water Plucks, Vents, Shaker
    t[12] = { s(0.000f, 1.000f), s(0.500f, 0.080f), on(4.000f, 0.020f), s(0.600f, 0.010f), s(0.500f, 0.080f), on(0.845f, 0.035f) };
    // Sea Ship: Creaks, Concertina, Rope Swells, Ship Bell, Waves, Gulls
    t[13] = { on(1.071f, 0.035f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.500f, 0.080f), on(5.000f, 0.050f) };
    // Suburbs: E-Piano, Pop Pulse, Wind Chimes, Sprinkler, Music Box, Bass
    t[14] = { s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.500f, 0.080f), on(0.845f, 0.035f), s(0.500f, 0.080f), s(0.500f, 0.080f) };
    // Mall: E-Piano, Vibraphone, PA Chimes, Escalator, Fountain, Drums
    t[15] = { s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.000f, 1.000f), s(0.500f, 0.080f), on(8.137f, 0.035f) };
    // Military Camp: Snare March, Bugle, Low Drums, Radio Blips, Clanks, Strings
    t[16] = { on(0.845f, 0.035f), s(0.350f, 0.028f), on(4.542f, 0.035f), s(0.500f, 0.080f), on(0.845f, 0.035f), s(0.090f, 0.060f) };
    // Beach: Guitar, Steel Drum, Waves, Shaker, Breeze Pad, Gulls
    t[17] = { s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.500f, 0.080f), on(0.845f, 0.035f), s(0.000f, 0.800f), on(5.000f, 0.050f) };
    // Desert: Oud, Frame Drums, Ney Flute, Tanpura, Wind, Chimes
    t[18] = { s(0.180f, 0.030f), on(1.650f, 0.035f), s(1.200f, 0.120f), s(0.000f, 1.000f), s(0.500f, 0.080f), s(0.500f, 0.080f) };
    // Palace: Harpsichord, Strings, Fanfare, Glockenspiel, Timpani, Choir
    t[19] = { s(0.500f, 0.080f), s(0.090f, 0.600f), s(0.350f, 0.028f), s(0.500f, 0.080f), on(0.845f, 0.035f), s(0.000f, 0.600f) };
    // Tavern: Lute Strum, Frame Drum, Fiddle, Glass Clinks, Squeezebox, Stomps
    t[20] = { s(0.500f, 0.080f), on(1.332f, 0.035f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.500f, 0.080f), on(4.542f, 0.035f) };
    // Space Station: Hull Drone, Docking Clamps, Comms Beeps, Airflow, Reactor Pulse, Zero-G Lead
    t[21] = { s(0.000f, 1.500f), on(4.542f, 0.035f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.000f, 0.350f) };
    // Cyberpunk Megacity: Grit Bass, Neon Arp, Rain Pad, Drum Machine, Glitch Stabs, Megastructure
    t[22] = { s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.000f, 1.200f), on(13.650f, 0.035f), s(0.500f, 0.080f), s(0.000f, 0.600f) };
    // Starship Bridge: Consoles, Warp Core, Tactical Pulse, Alert Brass, Crew Chatter, Sensor Pings
    t[23] = { s(0.500f, 0.080f), s(0.000f, 1.000f), on(10.176f, 0.035f), s(0.350f, 0.028f), s(0.000f, 0.600f), s(0.500f, 0.080f) };
    // Asteroid Mining Colony: Drill, Grind Bass, Metal Clanks, Dust Hiss, Worker Hymn, Beacon
    t[24] = { s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.000f, 0.800f), s(0.300f, 0.060f) };
    // Alien Ruins: Crystals, Machine Pulses, Alien Choir, Sub Drone, Ritual Drums, Whispers
    t[25] = { s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.000f, 1.000f), s(0.000f, 2.000f), on(2.420f, 0.035f), s(0.500f, 0.080f) };
    // Nebula: Nebula Pads, Cosmic Wind, Star Bells, Deep Hum, Stardust, Solar Flares
    t[26] = { s(0.000f, 2.000f), s(0.500f, 0.080f), s(0.500f, 0.080f), s(0.000f, 1.000f), s(0.500f, 0.080f), s(0.500f, 0.080f) };
    return t;
}();
// clang-format on

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
    // Ruins: Piano Shards, Music Box, Broken Bells, Wind, Violin Textures, Low Toms.
    t[10] = { hall(0.35f).echo(DlyReverse, 0.25f).with(ChLoFi, 0.15f), hall(0.3f).echo(DlyTape, 0.2f).with(ChLoFi, 0.3f),
              cathedral(0.35f).echo(DlyTape, 0.15f), hall(0.2f).wide(1.6f), cathedral(0.4f).echo(DlyReverse, 0.15f).wide(1.4f), hall(0.3f).toned(0.45f) };
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
    // Asteroid Mining Colony: Drill, Grind Bass, Metal Clanks, Dust Hiss, Worker Hymn, Beacon.
    t[24] = { room(0.15f).with(ChSaturation, 0.25f), room(0.05f).with(ChSaturation, 0.3f).toned(0.4f).wide(0.6f), cave(0.3f).echo(DlyTape, 0.15f),
              room(0.15f).wide(1.6f), hall(0.35f).with(ChChorus, 0.25f).wide(1.4f), cave(0.35f).echo(DlyDotted, 0.2f) };
    // Alien Ruins: Crystals, Machine Pulses, Alien Choir, Sub Drone, Ritual Drums, Whispers.
    t[25] = { cathedral(0.35f).echo(DlyReverse, 0.2f).wide(1.5f), cave(0.25f).with(ChPhaser, 0.25f), cathedral(0.4f).with(ChChorus, 0.3f).wide(1.6f),
              cave(0.1f).toned(0.4f).wide(0.7f), cave(0.3f).echo(DlyTape, 0.1f), cave(0.25f).echo(DlyReverse, 0.15f).wide(1.7f) };
    // Nebula: Nebula Pads, Cosmic Wind, Star Bells, Deep Hum, Stardust, Solar Flares.
    t[26] = { cathedral(0.35f).with(ChChorus, 0.35f).wide(1.7f), hall(0.25f).wide(1.8f), cathedral(0.4f).echo(DlyPingPong, 0.2f).wide(1.5f),
              hall(0.15f).toned(0.4f).wide(0.8f), plate(0.35f).echo(DlyDotted, 0.25f).wide(1.8f), cathedral(0.3f).with(ChPhaser, 0.3f).wide(1.6f) };
    for (std::size_t p = 0; p < t.size(); ++p)
        for (std::size_t e = 0; e < 6; ++e) {
            t[p][e].slide = slideTable[p][e].on;
            t[p][e].slideSemis = slideTable[p][e].semis;
            t[p][e].slideSeconds = slideTable[p][e].seconds;
        }
    return t;
}();

inline constexpr const InstrumentFxDefaults& instrumentFx(int type, int element) {
    return instrumentFxTable[std::size_t(type < 0 || type > 26 ? 0 : type)][std::size_t(element < 0 || element > 5 ? 0 : element)];
}
inline constexpr float instrumentFxDefault(int type, int element, int field) { return instrumentFx(type, element).field(field); }

} // namespace wb
