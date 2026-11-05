#pragma once
// Scenes (Milestone 12): a complete world in one recipe, used by the factory scene
// presets and by Inspire.
//
// A scene names up to four Places (with their Character, level, distance and pan), the
// Planet, Root and Mood, the World Controls, an arrangement template, and two to four
// Locations that change Characters, levels and World Controls. Building a scene gives
// the parameter values of its world (the first Location), the values of each Location,
// and an arrangement: the template, Auto-arranged for its Places with the scene's seed,
// with each section at the Location that suits it (calm sections at the first, peaks
// at the last), so travelling can be heard from the first play.
//
// Inspire builds a random but musical scene from the World Seed: Places that belong
// together (one world's category plus nature, now and then one from another world),
// Characters, a Mood that suits them, a Planet, World Controls, small changes to the
// instruments' effects, a template and its Locations. The same seed gives the same scene.
#include "../Arrangement/Templates.h"
#include "../Locations/Locations.h"

namespace wb::scenes {

inline constexpr int maxLocations = 4;
inline constexpr float keepLevel = 99.0f;   // a Location leaves the slot's level as it is

struct SlotSetup {
    int type = NoPlace, character = 0;
    float levelDb = 0, distance = 0.35f, pan = 0;
    float activity = 0.4f, tension = 0.3f;
};

struct LocationSetup {
    const char* name = "";
    int colour = 0;
    std::array<int8_t, placeSlots> character { -1, -1, -1, -1 };      // -1: as the scene sets it
    std::array<float, placeSlots> levelDb { keepLevel, keepLevel, keepLevel, keepLevel };
    std::array<int8_t, WorldControlCount> world { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };   // percent, -1: as the scene
};

struct Scene {
    std::string name, description;
    std::array<SlotSetup, placeSlots> slots {};
    int planet = 0;
    float influence = 0.35f;
    int root = 2, mood = 0;
    std::array<int8_t, WorldControlCount> world { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };   // percent, -1: default
    float intensity = 0.5f;
    // The Drama tier (after the performance pass): 0 the Classic score (every scene from
    // before it), 1 the full dramatic range; its style (drama::Style).
    float drama = 0;
    int dramaStyle = 0;
    bool arcs = false;     // the arrangement's arcs on the Drama tiers (arr::dramatize)
    int inspired = -1;     // Inspire's mood (InspireMood), -1: not inspired
    int templ = arr::ClassicArc;
    uint32_t seed = 1;
    std::array<LocationSetup, maxLocations> locations {};
    int locationCount = 0;
    // World Control overrides on the arrangement's peak sections (percent, -1: none).
    std::array<int8_t, WorldControlCount> peakWorld { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    // Changes to each instrument's effects (Inspire): send and amount scales, 1: as the
    // instrument's defaults.
    std::array<std::array<float, 3>, placeSlots * placeElements> fxScale {};
    bool scaledFx = false;
};

// The factory scenes.
int factoryCount();
const Scene& factory(int index);
// Inspire's moods (after the performance pass): the scene it builds is made to sound like
// it (Places, Characters, key, World Controls, Score Intensity and the Drama tier, a
// template with arcs). Any picks one from the seed, the dramatic ones (Tension, Battle,
// Epic, Tragic, Triumphant) about three times in five.
enum InspireMood { MoodAny, MoodCalm, MoodAdventure, MoodMystery, MoodTension, MoodBattle, MoodEpic, MoodTragic, MoodTriumphant, InspireMoodCount };
inline constexpr std::array<const char*, InspireMoodCount> inspireMoodNames { "Any", "Calm", "Adventure", "Mystery", "Tension",
                                                                             "Battle", "Epic", "Tragic", "Triumphant" };
// The mood Any picks for a seed.
int moodFor(uint32_t seed);
// A random, musical scene from a seed (1-9999), in a mood.
Scene inspire(uint32_t seed, int mood = MoodAny);
// The next World Seed Inspire moves to from `seed` (1-9999, never the same).
uint32_t nextSeed(uint32_t seed);

// The parameter values of a scene's world (its first Location). `session` gives the
// values that belong to the session, not the world (Power, Master Level, Quality, Play
// Mode, Place Outputs, and the environment mix: the layers' levels, mutes and solos and
// the Environment master); everything else starts from its default.
WorldValues worldValues(const Scene& scene, const WorldValues& session);
// Location i's values (the world with the Location's changes).
WorldValues locationValues(const Scene& scene, int index, const WorldValues& world);
// The scene's arrangement: the template, Auto-arranged, sections at their Locations. The
// orchestration pass: a dramatic or inspired scene's cue is fitted to `seconds` at `bpm` (the
// Length control at the host's tempo; 0: Medium at 120 BPM); the older scenes keep theirs.
arr::Arrangement arrangement(const Scene& scene, double seconds = 0.0, double bpm = 0.0);
// Which Location a section of the arrangement is at (by what the section does).
int locationFor(int kind, int locations);

} // namespace wb::scenes
