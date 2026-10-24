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

// The environment layers, in mixer order. Milestone 3 removed the Machinery, Magic,
// Life and Population layers: their roles moved into the Places (and the Tonal Bed
// for Magic), and the World Controls of the same names now shape the Places.
enum Layer { Air, Water, Fire, Earth, Thunder, Bed, LayerCount };

inline constexpr std::array<const char*, LayerCount> layerIds { "air", "water", "fire", "earth", "thunder", "bed" };
inline constexpr std::array<const char*, LayerCount> layerNames { "Air", "Water", "Fire & Heat", "Ice & Earth", "Thunder", "Tonal Bed" };

inline constexpr const char* rootChoices = "C|C#|D|D#|E|F|F#|G|G#|A|A#|B";
inline constexpr const char* moodChoices = "Minor|Major|Dorian|Phrygian|Lydian|Mixolydian|Whole-tone|Pentatonic|Chromatic";
inline constexpr const char* qualityChoices = "Eco|Standard|High";
inline constexpr const char* placeChoices = "Empty|Castle|Village|Spaceport|Factory|City|Forest|Alien Hive|Temple|Cavern|Ruins|Spaceship|Biodome|"
                                            "Sea Ship|Suburbs|Mall|Military Camp|Beach|Desert|Palace|Tavern";
inline constexpr int placeChoiceCount = 21;
inline constexpr const char* characterChoices = "1|2|3";
inline constexpr const char* playModeChoices = "Free|Transport";
enum PlayMode { PlayFree, PlayTransport };

// Each of the four Place slots has these parameters, in this order.
enum PlaceParam {
    PlType, PlLevel, PlDistance, PlPan, PlMute, PlSolo, PlCharacter, PlActivity, PlTension, PlSync, PlDensity, PlSwing,
    PlTone, PlSize, PlVariation, PlSeed,
    PlElement,                     // six element (instrument) levels
    PlElementOn = PlElement + 6,   // six element switches
    PlaceParamCount = PlElementOn + 6
};
inline constexpr int placeSlotCount = 4;

// Indices into `specs`.
enum P {
    PTemperature, PWeather, PWetness, PMachinery, PMagic, PLife, PPopulation, PScale, PEra, PDanger, PActivity,
    PPower, PMaster, PWidth, PSeed, PQuality, PSpaceMix, PRoot, PMood,
    PLayerFirst,   // then level, mute, solo for each layer
    PPlaceFirst = PLayerFirst + 3 * LayerCount,   // then PlaceParamCount per slot
    PPlayMode = PPlaceFirst + placeSlotCount * PlaceParamCount,
    PCount
};

inline constexpr int layerLevel(int layer) { return PLayerFirst + 3 * layer; }
inline constexpr int layerMute(int layer) { return PLayerFirst + 3 * layer + 1; }
inline constexpr int layerSolo(int layer) { return PLayerFirst + 3 * layer + 2; }
inline constexpr int placeParam(int slot, int field) { return PPlaceFirst + slot * PlaceParamCount + field; }

// clang-format off
inline constexpr std::array<ParamSpec, PCount> specs { {
    { "temperature", "Temperature", Kind::Float, 0, 1, 0.50f },
    { "weather",     "Weather",     Kind::Float, 0, 1, 0.35f },
    { "wetness",     "Wetness",     Kind::Float, 0, 1, 0.30f },
    { "machinery",   "Machinery",   Kind::Float, 0, 1, 0.00f },
    { "magic",       "Magic",       Kind::Float, 0, 1, 0.25f },
    { "life",        "Life",        Kind::Float, 0, 1, 0.30f },
    { "population",  "Population",  Kind::Float, 0, 1, 0.50f },
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
    WB_LAYER("thunder", "Thunder"), WB_LAYER("bed", "Tonal Bed"),
#undef WB_LAYER
#define WB_ELEMENTS(id, name, kind, def, suffix, label) \
    { id "_e1" suffix, name " Element 1" label, kind, 0, 1, def }, { id "_e2" suffix, name " Element 2" label, kind, 0, 1, def }, \
    { id "_e3" suffix, name " Element 3" label, kind, 0, 1, def }, { id "_e4" suffix, name " Element 4" label, kind, 0, 1, def }, \
    { id "_e5" suffix, name " Element 5" label, kind, 0, 1, def }, { id "_e6" suffix, name " Element 6" label, kind, 0, 1, def }
#define WB_PLACE(id, name, type, pan, seed) \
    { id "_type", name " Place", Kind::Choice, 0, 20, type, placeChoices }, \
    { id "_level", name " Level", Kind::Decibels, -60, 6, 0 }, \
    { id "_distance", name " Distance", Kind::Float, 0, 1, 0.35f }, \
    { id "_pan", name " Pan", Kind::Float, -1, 1, pan }, \
    { id "_mute", name " Mute", Kind::Bool, 0, 1, 0 }, \
    { id "_solo", name " Solo", Kind::Bool, 0, 1, 0 }, \
    { id "_character", name " Character", Kind::Choice, 0, 2, 0, characterChoices }, \
    { id "_activity", name " Activity", Kind::Float, 0, 1, 0.5f }, \
    { id "_tension", name " Tension", Kind::Float, 0, 1, 0.3f }, \
    { id "_sync", name " Sync", Kind::Bool, 0, 1, 1 }, \
    { id "_density", name " Density", Kind::Float, 0, 1, 0.5f }, \
    { id "_swing", name " Swing", Kind::Float, 0, 1, 0.15f }, \
    { id "_tone", name " Tone", Kind::Float, 0, 1, 0.5f }, \
    { id "_size", name " Size", Kind::Float, 0, 1, 0.5f }, \
    { id "_variation", name " Variation", Kind::Float, 0, 1, 0.35f }, \
    { id "_seed", name " Seed", Kind::Int, 1, 9999, seed }, \
    WB_ELEMENTS(id, name, Kind::Float, 0.8f, "", ""), \
    WB_ELEMENTS(id, name, Kind::Bool, 1, "_on", " On")
    WB_PLACE("p1", "Place 1", 1, -0.25f, 11), WB_PLACE("p2", "Place 2", 0, 0.35f, 22),
    WB_PLACE("p3", "Place 3", 0, -0.55f, 33), WB_PLACE("p4", "Place 4", 0, 0.55f, 44),
#undef WB_PLACE
#undef WB_ELEMENTS
    // Free: the world plays all the time. Transport: only while the host plays, from
    // silence and locked to the song position (so bounces always contain it).
    { "playmode", "Play Mode", Kind::Choice, 0, 1, PlayTransport, playModeChoices },
} };
// clang-format on

static_assert(specs.back().id != nullptr, "the table fills every parameter slot");
static_assert(int(PTemperature) == int(Temperature) && int(PActivity) == int(Activity), "World Controls come first, in enum order");

} // namespace wb
