#pragma once
// One table for every automatable parameter: ID, display name, range, default.
// IDs are stable once released (hosts store automation by them). JUCE derives the
// AU parameter ID from a hash of the string ID, so new parameters can be added in
// later milestones without disturbing existing ones.
#include "FxDefaults.h"
#include <array>
#include <cstddef>

namespace wb {

enum class Kind { Float, Decibels, Bool, Choice, Int, Seconds, Semitones, ShortSeconds };

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
// The instrument a new instance plays in Instrument mode: the Tavern's Lute Strum
// (PlaceType Tavern, which Parameters.h cannot name; Catalog.h checks it).
inline constexpr int defaultInstrumentType = 20;
inline constexpr const char* droneChoices = "Off|Root|Root + Fifth";
enum Drone { DroneOff, DroneRoot, DroneRootFifth };
inline constexpr const char* modeChoices = "World|Instrument";
enum Mode { ModeWorld, ModeInstrument };
inline constexpr const char* dramaStyleChoices = "Auto|Driving|Epic|Tragic|Triumphant|Suspense";
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
    // The environment layers' master level (the GarageBand fix round).
    PEnvironment,
    // Instrument mode: one instrument played from host MIDI like a normal software
    // instrument (the world, its patterns and the arrangement silent). The instrument
    // itself (Place and element) is saved in the state; these are its sound.
    PMode, PInstCharacter, PInstLevel, PInstTone, PInstAttack, PInstRelease, PInstDrive,
    PInstReverb, PInstReverbType, PInstDelay, PInstDelayType, PInstFx, PInstFxAmount, PInstWidth,
    PInstAmbience, PInstOctave, PInstDrone,
    // The Drama tier (after the performance pass).
    PDrama, PDramaStyle,
    // The Drama tier rework: each slot's three Drama instruments (drums, ostinato, hits:
    // elements 7-9 of the slot), DramaParamCount per slot, as the six instruments have
    // them. Appended after everything older, so existing automation keeps its parameters.
    PDramaFirst,
    PCount = PDramaFirst + 4 * (6 + 3 * FxFieldCount)
};

// Each slot's Drama instrument parameters, in this order (from PDramaFirst + slot *
// DramaParamCount): three levels, three switches, then each one's effects.
enum DramaParam { DrLevel, DrOn = DrLevel + 3, DrFx = DrOn + 3, DramaParamCount = DrFx + 3 * FxFieldCount };
static_assert(int(PCount) == int(PDramaFirst) + 4 * int(DramaParamCount), "four slots of Drama instrument parameters");
inline constexpr int dramaParam(int slot, int field) { return PDramaFirst + slot * DramaParamCount + field; }

inline constexpr int layerLevel(int layer) { return PLayerFirst + 3 * layer; }
inline constexpr int layerMute(int layer) { return PLayerFirst + 3 * layer + 1; }
inline constexpr int layerSolo(int layer) { return PLayerFirst + 3 * layer + 2; }
inline constexpr int placeParam(int slot, int field) { return PPlaceFirst + slot * PlaceParamCount + field; }
// Any instrument of a slot (0-5: the Place's, 6-8: the Drama instruments): its level,
// switch and effect field.
inline constexpr int elementLevelParam(int slot, int e) { return e < 6 ? placeParam(slot, PlElement + e) : dramaParam(slot, DrLevel + e - 6); }
inline constexpr int elementOnParam(int slot, int e) { return e < 6 ? placeParam(slot, PlElementOn + e) : dramaParam(slot, DrOn + e - 6); }
inline constexpr int elementFxParam(int slot, int e, int field) {
    return e < 6 ? placeParam(slot, fxField(e, field)) : dramaParam(slot, DrFx + (e - 6) * FxFieldCount + field);
}
inline constexpr bool isDramaInstrumentParam(int p) { return p >= PDramaFirst && p < PCount; }

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
    { id e "_width", name label " Width", Kind::Float, 0, 2, instrumentFxDefault(type, k, FxWidth) }, \
    { id e "_slide", name label " Scoop/Glide", Kind::Choice, 0, 1, instrumentFxDefault(type, k, FxSlide), slideChoices }, \
    { id e "_slideamt", name label " Scoop Amount", Kind::Semitones, 0, 12, instrumentFxDefault(type, k, FxSlideAmount) }, \
    { id e "_slidetime", name label " Scoop/Glide Time", Kind::ShortSeconds, 0.005f, 3, instrumentFxDefault(type, k, FxSlideTime) }
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
    // Environment: one level for all six environment layers (Air, Water, Fire & Heat,
    // Ice & Earth, Thunder, Tonal Bed), on top of each layer's own.
    { "environment", "Environment Level", Kind::Decibels, -60, 6, 0 },
    // Instrument mode. Mode: WORLD (everything) or INSTRUMENT (one instrument from the
    // host's MIDI). Attack and Release scale the instrument's own times (the middle is as
    // designed); Drive saturates it; Place Ambience brings the world's environment and
    // space back under it (off: just the instrument).
    { "mode", "Mode", Kind::Choice, 0, 1, ModeWorld, modeChoices },
    { "inst_character", "Instrument Character", Kind::Choice, 0, 2, 0, characterChoices },
    { "inst_level", "Instrument Level", Kind::Decibels, -60, 6, 0 },
    { "inst_tone", "Instrument Tone", Kind::Float, 0, 1, 0.5f },
    { "inst_attack", "Instrument Attack", Kind::Float, 0, 1, 0.5f },
    { "inst_release", "Instrument Release", Kind::Float, 0, 1, 0.5f },
    { "inst_drive", "Instrument Drive", Kind::Float, 0, 1, 0 },
    { "inst_rev", "Instrument Reverb", Kind::Float, 0, 1, instrumentFxDefault(defaultInstrumentType, 0, FxReverb) },
    { "inst_revtype", "Instrument Reverb Type", Kind::Choice, 0, ReverbTypeCount - 1, instrumentFxDefault(defaultInstrumentType, 0, FxReverbType), reverbChoices },
    { "inst_dly", "Instrument Delay", Kind::Float, 0, 1, instrumentFxDefault(defaultInstrumentType, 0, FxDelay) },
    { "inst_dlytype", "Instrument Delay Type", Kind::Choice, 0, DelayTypeCount - 1, instrumentFxDefault(defaultInstrumentType, 0, FxDelayType), delayChoices },
    { "inst_char", "Instrument Character Effect", Kind::Choice, 0, CharacterTypeCount - 1, instrumentFxDefault(defaultInstrumentType, 0, FxCharacter), characterFxChoices },
    { "inst_charamt", "Instrument Character Amount", Kind::Float, 0, 1, instrumentFxDefault(defaultInstrumentType, 0, FxAmount) },
    { "inst_width", "Instrument Width", Kind::Float, 0, 2, instrumentFxDefault(defaultInstrumentType, 0, FxWidth) },
    { "inst_ambience", "Place Ambience", Kind::Bool, 0, 1, 0 },
    { "inst_octave", "Instrument Octave", Kind::Int, -2, 2, 0 },
    // Drone (the hotfix after Instrument mode): an instrument's built-in drone (the
    // hurdy-gurdy's drone strings, a brass or reed pedal) under the notes played: none,
    // the played note's root an octave down, or the root and its fifth. It follows the
    // note held and stops with it.
    { "inst_drone", "Instrument Drone", Kind::Choice, 0, 2, 1, droneChoices },
    // Drama (after the performance pass): how far the top of Score Intensity (and Tension)
    // changes gear, as a game score's layers do: Calm, Tension, Action, Climax. 0 is the
    // Classic score (the world exactly as before: projects and scenes from before it open
    // at 0); at 100 % a Drama layer of driving rhythm, war drums, stabs and swells, themed
    // by each Place, joins from the middle of Score Intensity up. Drama Style picks its
    // feel (Auto: each Place's own).
    { "drama", "Drama", Kind::Float, 0, 1, 1 },
    { "dramastyle", "Drama Style", Kind::Choice, 0, 5, 0, dramaStyleChoices },
    // The Drama instruments (the Drama tier rework): per slot, the Drama Drums, Ostinato
    // and Hits (elements 7, 8 and 9) with a level, a switch and the effects of any
    // instrument. Projects from before them open with them off (PluginProcessor).
#define WB_DRAMA(id, name) \
    { id "_e7", name " Drama Drums", Kind::Float, 0, 1, 0.8f }, { id "_e8", name " Drama Ostinato", Kind::Float, 0, 1, 0.8f }, \
    { id "_e9", name " Drama Hits", Kind::Float, 0, 1, 0.8f }, \
    { id "_e7_on", name " Drama Drums On", Kind::Bool, 0, 1, 1 }, { id "_e8_on", name " Drama Ostinato On", Kind::Bool, 0, 1, 1 }, \
    { id "_e9_on", name " Drama Hits On", Kind::Bool, 0, 1, 1 }, \
    WB_FX(id, name, 0, "_e7", " Drama Drums", 6), WB_FX(id, name, 0, "_e8", " Drama Ostinato", 7), WB_FX(id, name, 0, "_e9", " Drama Hits", 8)
    WB_DRAMA("p1", "Place 1"), WB_DRAMA("p2", "Place 2"), WB_DRAMA("p3", "Place 3"), WB_DRAMA("p4", "Place 4"),
#undef WB_DRAMA
#undef WB_FX
} };
// clang-format on

// Instrument mode's parameters: the session's, never part of a world, a Location or a
// scene (loading a world or a scene keeps them; journeys leave them alone).
inline constexpr bool isInstrumentParam(int p) { return p >= PMode && p <= PInstDrone; }

static_assert(specs.back().id != nullptr, "the table fills every parameter slot");
static_assert(int(PTemperature) == int(Temperature) && int(PActivity) == int(Activity), "World Controls come first, in enum order");

} // namespace wb
