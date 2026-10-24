#pragma once
// One table for every automatable parameter: ID, display name, range, default.
// IDs are stable once released (hosts store automation by them). JUCE derives the
// AU parameter ID from a hash of the string ID, so new parameters can be added in
// later milestones without disturbing existing ones.
#include <array>
#include <cstddef>

namespace wb {

enum class Kind { Float, Decibels, Bool, Choice, Int };

struct ParamSpec {
    const char* id;
    const char* name;
    Kind kind;
    float min, max, def;
    const char* choices = "";   // "|" separated, for Kind::Choice
};

// The World Controls. 0..1, shown as percentages.
enum WorldControl {
    Temperature, Weather, Wetness, Machinery, Magic, Life, Population, Scale, Era, Danger, Activity,
    WorldControlCount
};

// The ten sound layers, in mixer order.
enum Layer { Air, Water, Fire, Earth, Thunder, Machine, Arcane, Creatures, Crowd, Bed, LayerCount };

inline constexpr std::array<const char*, LayerCount> layerIds { "air", "water", "fire", "earth", "thunder",
                                                                "machine", "magic", "life", "crowd", "bed" };
inline constexpr std::array<const char*, LayerCount> layerNames { "Air", "Water", "Fire & Heat", "Ice & Earth", "Thunder",
                                                                  "Machinery", "Magic", "Life", "Population", "Tonal Bed" };
// Layers that produce sound in this build. The rest arrive in Milestone 3.
inline constexpr std::array<bool, LayerCount> layerImplemented { true, true, true, true, true,
                                                                 false, false, false, false, true };

inline constexpr const char* rootChoices = "C|C#|D|D#|E|F|F#|G|G#|A|A#|B";
inline constexpr const char* moodChoices = "Minor|Major|Dorian|Phrygian|Lydian|Mixolydian|Whole-tone|Pentatonic|Chromatic";
inline constexpr const char* qualityChoices = "Eco|Standard|High";

// Indices into `specs`.
enum P {
    PTemperature, PWeather, PWetness, PMachinery, PMagic, PLife, PPopulation, PScale, PEra, PDanger, PActivity,
    PPower, PMaster, PWidth, PSeed, PQuality, PSpaceMix, PRoot, PMood,
    PLayerFirst,   // then level, mute, solo for each layer
    PCount = PLayerFirst + 3 * LayerCount
};

inline constexpr int layerLevel(int layer) { return PLayerFirst + 3 * layer; }
inline constexpr int layerMute(int layer) { return PLayerFirst + 3 * layer + 1; }
inline constexpr int layerSolo(int layer) { return PLayerFirst + 3 * layer + 2; }

// clang-format off
inline constexpr std::array<ParamSpec, PCount> specs { {
    { "temperature", "Temperature", Kind::Float, 0, 1, 0.50f },
    { "weather",     "Weather",     Kind::Float, 0, 1, 0.35f },
    { "wetness",     "Wetness",     Kind::Float, 0, 1, 0.30f },
    { "machinery",   "Machinery",   Kind::Float, 0, 1, 0.00f },
    { "magic",       "Magic",       Kind::Float, 0, 1, 0.25f },
    { "life",        "Life",        Kind::Float, 0, 1, 0.30f },
    { "population",  "Population",  Kind::Float, 0, 1, 0.00f },
    { "scale",       "Scale",       Kind::Float, 0, 1, 0.50f },
    { "era",         "Era",         Kind::Float, 0, 1, 0.30f },
    { "danger",      "Danger",      Kind::Float, 0, 1, 0.20f },
    { "activity",    "Activity",    Kind::Float, 0, 1, 0.40f },
    { "power",       "Power",       Kind::Bool,  0, 1, 1 },
    { "master",      "Master Level", Kind::Decibels, -60, 6, 0 },
    { "width",       "Width",       Kind::Float, 0, 2, 1 },
    { "seed",        "Seed",        Kind::Int,   1, 9999, 1 },
    { "quality",     "Quality",     Kind::Choice, 0, 2, 1, qualityChoices },
    { "spacemix",    "Space Mix",   Kind::Float, 0, 1, 0.40f },
    { "root",        "Root",        Kind::Choice, 0, 11, 2, rootChoices },
    { "mood",        "Mood",        Kind::Choice, 0, 8, 0, moodChoices },
#define WB_LAYER(id, name) \
    { id "_level", name " Level", Kind::Decibels, -60, 6, 0 }, \
    { id "_mute",  name " Mute",  Kind::Bool, 0, 1, 0 }, \
    { id "_solo",  name " Solo",  Kind::Bool, 0, 1, 0 }
    WB_LAYER("air", "Air"), WB_LAYER("water", "Water"), WB_LAYER("fire", "Fire"), WB_LAYER("earth", "Earth"),
    WB_LAYER("thunder", "Thunder"), WB_LAYER("machine", "Machinery"), WB_LAYER("magic", "Magic"),
    WB_LAYER("life", "Life"), WB_LAYER("crowd", "Population"), WB_LAYER("bed", "Tonal Bed"),
#undef WB_LAYER
} };
// clang-format on

static_assert(int(PTemperature) == int(Temperature) && int(PActivity) == int(Activity), "World Controls come first, in enum order");

} // namespace wb
