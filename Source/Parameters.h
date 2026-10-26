#pragma once
// One table for every automatable parameter: ID, display name, range, default.
// IDs are stable once released (hosts store automation by them). JUCE derives the
// AU parameter ID from a hash of the string ID, so new parameters can be added in
// later milestones without disturbing existing ones.
#include "FxDefaults.h"
#include <array>
#include <cstddef>

namespace wb {

enum class Kind { Float, Decibels, Bool, Choice, Int, Seconds };

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
                                            "Sea Ship|Suburbs|Mall|Military Camp|Beach|Desert|Palace|Tavern|"
                                            "Space Station|Cyberpunk Megacity|Starship Bridge|Asteroid Mining Colony|Alien Ruins|Nebula";
inline constexpr int placeChoiceCount = 27;
inline constexpr const char* characterChoices = "1|2|3";
// Arrangement (Milestone 11): as Transport, following the arrangement's sections.
inline constexpr const char* playModeChoices = "Free|Transport|Arrangement";
// Locations and Journeys (Milestone 9): eight Location slots.
inline constexpr const char* destinationChoices = "-|1|2|3|4|5|6|7|8";
inline constexpr const char* locationChoices = "1|2|3|4|5|6|7|8";
inline constexpr const char* travelSyncChoices = "Seconds|1 bar|2 bars|4 bars|8 bars|16 bars";
inline constexpr std::array<int, 6> travelSyncBars { 0, 1, 2, 4, 8, 16 };
inline constexpr const char* pathLegChoices = "1 bar|2 bars|4 bars|8 bars|16 bars|32 bars";
inline constexpr std::array<int, 6> pathLegBars { 1, 2, 4, 8, 16, 32 };
inline constexpr const char* pathEndChoices = "One-way|Loop";
// Planet types (Milestone 10), in dsp::PlanetType order.
// Separate outputs (Milestone 12): Copy keeps every Place in the main mix as well;
// Split takes a Place with its own output out of the main mix.
inline constexpr const char* placeOutChoices = "Copy|Split";
inline constexpr const char* planetChoices = "Rocky|Ocean|Ice Giant|Gas Giant|Desert World|Lava World|Forest World";
enum PlayMode { PlayFree, PlayTransport, PlayArrangement };

// Each of the four Place slots has these parameters, in this order.
enum PlaceParam {
    PlType, PlLevel, PlDistance, PlPan, PlMute, PlSolo, PlCharacter, PlActivity, PlTension, PlSync, PlDensity, PlSwing,
    PlTone, PlSize, PlVariation, PlSeed,
    PlElement,                     // six element (instrument) levels
    PlElementOn = PlElement + 6,   // six element switches
    PlFx = PlElementOn + 6,        // each element's effects (FxFieldCount each, see FxDefaults.h)
    PlaceParamCount = PlFx + 6 * FxFieldCount
};
inline constexpr int fxField(int element, int field) { return PlFx + element * FxFieldCount + field; }
inline constexpr int placeSlotCount = 4;

// Indices into `specs`.
enum P {
    PTemperature, PWeather, PWetness, PMachinery, PMagic, PLife, PPopulation, PScale, PEra, PDanger, PActivity,
    PPower, PMaster, PWidth, PSeed, PQuality, PSpaceMix, PRoot, PMood,
    PLayerFirst,   // then level, mute, solo for each layer
    PPlaceFirst = PLayerFirst + 3 * LayerCount,   // then PlaceParamCount per slot
    PPlayMode = PPlaceFirst + placeSlotCount * PlaceParamCount,
    PIntensity,   // Score Intensity (Milestone 7)
    // Locations and Journeys (Milestone 9).
    PDestination, PTravelTime, PTravelSync, PJourneyOn, PJourneyFrom, PJourneyTo, PJourney,
    PPathOn, PPathLeg, PPathEnd,
    // The Planet (Milestone 10).
    PPlanet, PPlanetInfluence,
    // Separate outputs (Milestone 12).
    PPlaceOuts,
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
// An element's effects; the defaults are those of the slot's default Place.
#define WB_FX(id, name, type, e, label, k) \
    { id e "_rev", name label " Reverb", Kind::Float, 0, 1, instrumentFxDefault(type, k, FxReverb) }, \
    { id e "_revtype", name label " Reverb Type", Kind::Choice, 0, ReverbTypeCount - 1, instrumentFxDefault(type, k, FxReverbType), reverbChoices }, \
    { id e "_dly", name label " Delay", Kind::Float, 0, 1, instrumentFxDefault(type, k, FxDelay) }, \
    { id e "_dlytype", name label " Delay Type", Kind::Choice, 0, DelayTypeCount - 1, instrumentFxDefault(type, k, FxDelayType), delayChoices }, \
    { id e "_char", name label " Character", Kind::Choice, 0, CharacterTypeCount - 1, instrumentFxDefault(type, k, FxCharacter), characterFxChoices }, \
    { id e "_charamt", name label " Character Amount", Kind::Float, 0, 1, instrumentFxDefault(type, k, FxAmount) }, \
    { id e "_tone", name label " Tone", Kind::Float, 0, 1, instrumentFxDefault(type, k, FxTone) }, \
    { id e "_width", name label " Width", Kind::Float, 0, 2, instrumentFxDefault(type, k, FxWidth) }
#define WB_PLACE(id, name, type, pan, seed) \
    { id "_type", name " Place", Kind::Choice, 0, placeChoiceCount - 1, type, placeChoices }, \
    { id "_level", name " Level", Kind::Decibels, -60, 6, 0 }, \
    { id "_distance", name " Distance", Kind::Float, 0, 1, 0.35f }, \
    { id "_pan", name " Pan", Kind::Float, -1, 1, pan }, \
    { id "_mute", name " Mute", Kind::Bool, 0, 1, 0 }, \
    { id "_solo", name " Solo", Kind::Bool, 0, 1, 0 }, \
    { id "_character", name " Character", Kind::Choice, 0, 2, 0, characterChoices }, \
    { id "_activity", name " Activity", Kind::Float, 0, 1, 0.4f }, \
    { id "_tension", name " Tension", Kind::Float, 0, 1, 0.3f }, \
    { id "_sync", name " Sync", Kind::Bool, 0, 1, 1 }, \
    { id "_density", name " Density", Kind::Float, 0, 1, 0.45f }, \
    { id "_swing", name " Swing", Kind::Float, 0, 1, 0.08f }, \
    { id "_tone", name " Tone", Kind::Float, 0, 1, 0.5f }, \
    { id "_size", name " Size", Kind::Float, 0, 1, 0.5f }, \
    { id "_variation", name " Variation", Kind::Float, 0, 1, 0.35f }, \
    { id "_seed", name " Seed", Kind::Int, 1, 9999, seed }, \
    WB_ELEMENTS(id, name, Kind::Float, 0.8f, "", ""), \
    WB_ELEMENTS(id, name, Kind::Bool, 1, "_on", " On"), \
    WB_FX(id, name, type, "_e1", " Element 1", 0), WB_FX(id, name, type, "_e2", " Element 2", 1), \
    WB_FX(id, name, type, "_e3", " Element 3", 2), WB_FX(id, name, type, "_e4", " Element 4", 3), \
    WB_FX(id, name, type, "_e5", " Element 5", 4), WB_FX(id, name, type, "_e6", " Element 6", 5)
    WB_PLACE("p1", "Place 1", 1, -0.25f, 11), WB_PLACE("p2", "Place 2", 0, 0.35f, 22),
    WB_PLACE("p3", "Place 3", 0, -0.55f, 33), WB_PLACE("p4", "Place 4", 0, 0.55f, 44),
#undef WB_PLACE
#undef WB_FX
#undef WB_ELEMENTS
    // Free: the world plays all the time. Transport: only while the host plays, from
    // silence and locked to the song position (so bounces always contain it).
    // Arrangement: as Transport, playing the arrangement's section at the song position.
    { "playmode", "Play Mode", Kind::Choice, 0, 2, PlayTransport, playModeChoices },
    // Score Intensity: a calm bed at 0, every Place as set at 0.5, a climax at 1.
    { "intensity", "Score Intensity", Kind::Float, 0, 1, 0.5f },
    // Locations: choosing a Destination travels there over the Travel Time (seconds, or
    // bars of the host tempo). Journey Mode morphs between From and To at the Journey
    // position instead, for drawing a walk through the world as automation.
    { "destination", "Destination", Kind::Choice, 0, 8, 0, destinationChoices },
    { "traveltime", "Travel Time", Kind::Seconds, 0.5f, 60, 4 },
    { "travelsync", "Travel Sync", Kind::Choice, 0, 5, 0, travelSyncChoices },
    { "journeymode", "Journey Mode", Kind::Bool, 0, 1, 0 },
    { "journeyfrom", "Journey From", Kind::Choice, 0, 7, 0, locationChoices },
    { "journeyto", "Journey To", Kind::Choice, 0, 7, 1, locationChoices },
    { "journey", "Journey", Kind::Float, 0, 1, 0 },
    // Journey Path: the saved Locations in order, one leg every so many bars of the
    // song, arriving on the downbeat; then stop at the last, or loop.
    { "pathmode", "Journey Path", Kind::Bool, 0, 1, 0 },
    { "pathleg", "Path Leg", Kind::Choice, 0, 5, 2, pathLegChoices },
    { "pathend", "Path End", Kind::Choice, 0, 1, 1, pathEndChoices },
    // The Planet: the globe's look and a sound colour on the whole world, as strong as
    // Planet Influence.
    { "planet", "Planet", Kind::Choice, 0, 6, 0, planetChoices },
    { "planetinfluence", "Planet Influence", Kind::Float, 0, 1, 0.35f },
    // Separate outputs: each Place slot also plays on its own stereo output (Logic's
    // multi-output instrument). Copy: the main output keeps the full mix. Split: a Place
    // that has its own output leaves the main mix (its reverb and delay stay there).
    { "placeouts", "Place Outputs", Kind::Choice, 0, 1, 0, placeOutChoices },
} };
// clang-format on

static_assert(specs.back().id != nullptr, "the table fills every parameter slot");
static_assert(int(PTemperature) == int(Temperature) && int(PActivity) == int(Activity), "World Controls come first, in enum order");

} // namespace wb
