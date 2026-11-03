#include "Scenes.h"
#include <algorithm>
#include <initializer_list>

namespace wb::scenes {

namespace {

using WorldList = std::initializer_list<std::pair<int, int>>;

std::array<int8_t, WorldControlCount> world(WorldList list) {
    std::array<int8_t, WorldControlCount> w {};
    w.fill(-1);
    for (const auto& [control, percent] : list) w[std::size_t(control)] = int8_t(std::clamp(percent, 0, 100));
    return w;
}

SlotSetup place(int type, int character, float levelDb = 0, float distance = 0.35f, float pan = 0, float activity = 0.4f, float tension = 0.3f) {
    SlotSetup s;
    s.type = type;
    s.character = character;
    s.levelDb = levelDb;
    s.distance = distance;
    s.pan = pan;
    s.activity = activity;
    s.tension = tension;
    return s;
}

// A Location: its name, colour, each slot's Character (-1: as set), levels (keepLevel:
// as set) and World Controls.
LocationSetup location(const char* name, int colour, std::array<int8_t, placeSlots> character = { -1, -1, -1, -1 },
                       std::array<float, placeSlots> levels = { keepLevel, keepLevel, keepLevel, keepLevel }, WorldList controls = {}) {
    LocationSetup l;
    l.name = name;
    l.colour = colour;
    l.character = character;
    l.levelDb = levels;
    l.world = world(controls);
    return l;
}

Scene scene(const char* name, const char* description, std::array<SlotSetup, placeSlots> slots, int planet, float influence, int root, int mood,
            WorldList controls, int templ, uint32_t seed, std::initializer_list<LocationSetup> locations, WorldList peak = {}) {
    Scene s;
    s.name = name;
    s.description = description;
    s.slots = slots;
    s.planet = planet;
    s.influence = influence;
    s.root = root;
    s.mood = mood;
    s.world = world(controls);
    s.templ = templ;
    s.seed = seed;
    for (const auto& l : locations)
        if (s.locationCount < maxLocations) s.locations[std::size_t(s.locationCount++)] = l;
    s.peakWorld = world(peak);
    return s;
}

// A dramatic scene (the Drama tier, after the performance pass): the full dramatic range,
// its style, and the arrangement's arcs.
Scene dramatic(Scene s, int style) {
    s.drama = 1.0f;
    s.dramaStyle = style;
    s.arcs = true;
    return s;
}

// Shorthands: Moods, Planets, roots.
enum { Minor, Major, Dorian, Phrygian, Lydian, Mixolydian, WholeTone, Pentatonic };
enum { Rocky, Ocean, IceGiant, GasGiant, DesertWorld, LavaWorld, ForestWorld };
enum { C, Cs, D, Ds, E, F, Fs, G, Gs, A, As, B };
constexpr SlotSetup none {};
constexpr float keep = keepLevel;

const std::vector<Scene>& table() {
    using namespace arr;
    static const std::vector<Scene> scenes {
        scene("Ambush at the Tavern", "A lively tavern night that turns into a brawl as soldiers burst in from the dark woods.",
              { place(Tavern, 0, 0, 0.25f, -0.1f, 0.5f), place(Forest, 1, -5, 0.6f, 0.55f), place(Military, 1, -6, 0.55f, -0.6f, 0.5f, 0.5f), none },
              Rocky, 0.3f, D, Dorian, { { Era, 25 }, { Danger, 25 }, { Population, 70 }, { Weather, 40 } }, Battle, 101,
              { location("Lively Night", 0, { 0, 1, 2, -1 }, { 0, -8, -60, keep }),
                location("Soldiers Close In", 2, { 1, 1, 1, -1 }, { -3, -4, -6, keep }, { { Danger, 50 } }),
                location("Brawl", 2, { 2, 1, 1, -1 }, { 0, -6, -3, keep }, { { Danger, 75 }, { Activity, 75 } }) },
              { { Danger, 80 } }),
        scene("Docking at Orbital Station", "Drifting in from deep space, through the spaceport's traffic to the station's docking clamps.",
              { place(SpaceStation, 0, 0, 0.3f, 0.0f), place(Spaceport, 0, -4, 0.5f, 0.5f), place(Nebula, 0, -3, 0.65f, -0.5f), none },
              GasGiant, 0.4f, A, Lydian, { { Era, 90 }, { Machinery, 55 }, { Scale, 75 }, { Danger, 15 } }, CinematicReveal, 102,
              { location("Deep Space", 1, { -1, -1, 0, -1 }, { -60, -60, 0, keep }),
                location("Approach", 1, { -1, 0, -1, -1 }, { -8, -2, -4, keep }),
                location("Docking", 3, { 0, 2, 0, -1 }, { 0, -3, -8, keep }, { { Machinery, 70 } }) }),
        scene("Siege of the Palace", "The court holds its breath as the castle walls fall and the army reaches the throne room.",
              { place(Palace, 1, 0, 0.3f, 0.0f), place(Castle, 2, -3, 0.45f, -0.5f, 0.5f, 0.6f), place(Military, 1, -5, 0.5f, 0.55f, 0.5f, 0.55f), none },
              Rocky, 0.3f, C, Minor, { { Era, 25 }, { Danger, 45 }, { Scale, 70 }, { Magic, 20 } }, BossFight, 103,
              { location("Throne Room", 0, { 1, 0, 2, -1 }, { 0, -8, -60, keep }),
                location("Walls Breached", 2, { 1, 2, 1, -1 }, { -2, -2, -4, keep }, { { Danger, 65 } }),
                location("Fallen Palace", 2, { 2, 2, 1, -1 }, { 0, -4, -6, keep }, { { Danger, 70 }, { Weather, 55 } }) },
              { { Danger, 85 } }),
        scene("Neon Rain Chase", "A chase through a rain-soaked megacity: the night market, the corporate towers, the riot.",
              { place(Megacity, 0, 0, 0.3f, 0.1f, 0.55f), place(City, 2, -4, 0.5f, -0.5f), none, none },
              Ocean, 0.3f, E, Phrygian, { { Era, 92 }, { Wetness, 80 }, { Weather, 55 }, { Machinery, 60 }, { Population, 70 } }, Battle, 104,
              { location("Night Market", 1, { 0, 2, -1, -1 }),
                location("Corporate Tower", 4, { 1, 1, -1, -1 }, { 0, -6, keep, keep }, { { Danger, 45 } }),
                location("Riot", 2, { 2, 2, -1, -1 }, { 0, -3, keep, keep }, { { Danger, 75 }, { Activity, 75 } }) },
              { { Weather, 75 } }),
        scene("Abandoned Mine", "Down the old shafts, past haunted ruins, to the flooded depths.",
              { place(Cavern, 1, 0, 0.35f, 0.0f), place(Ruins, 1, -4, 0.5f, -0.5f), place(Mining, 2, -7, 0.6f, 0.55f), none },
              Rocky, 0.35f, A, Phrygian, { { Era, 35 }, { Danger, 40 }, { Life, 10 }, { Population, 0 }, { Scale, 65 } }, ExplorationLoop, 105,
              { location("Mine Entrance", 0, { 1, 2, 2, -1 }, { 0, -6, -60, keep }),
                location("Haunted Shafts", 3, { 1, 1, 2, -1 }, { -2, -2, -6, keep }, { { Danger, 55 } }),
                location("Flooded Depths", 1, { 2, 1, 1, -1 }, { 0, -6, -8, keep }, { { Wetness, 75 } }) }),
        scene("First Contact", "The bridge crew finds alien ruins awakening in a nebula; the guardian answers.",
              { place(Bridge, 0, 0, 0.3f, -0.2f), place(AlienRuins, 0, -3, 0.5f, 0.5f), place(Nebula, 1, -5, 0.65f, -0.6f), none },
              GasGiant, 0.4f, F, WholeTone, { { Era, 90 }, { Magic, 55 }, { Scale, 80 }, { Danger, 30 } }, CinematicReveal, 106,
              { location("Uncharted", 1, { 0, 0, 1, -1 }, { -2, -60, 0, keep }),
                location("Signal", 4, { 0, 1, 1, -1 }, { 0, -2, -6, keep }, { { Magic, 70 } }),
                location("The Guardian", 2, { 1, 2, 2, -1 }, { -2, 0, -6, keep }, { { Danger, 60 }, { Magic, 80 } }) },
              { { Danger, 70 } }),
        scene("Market at Dawn", "A village wakes in the morning glade, the market fills, and the tavern opens its doors.",
              { place(Village, 2, 0, 0.3f, 0.0f), place(Forest, 2, -5, 0.55f, -0.55f), place(Tavern, 1, -6, 0.5f, 0.55f), none },
              ForestWorld, 0.3f, G, Major, { { Era, 20 }, { Life, 65 }, { Population, 55 }, { Danger, 5 }, { Temperature, 55 } }, TownTheme, 107,
              { location("Quiet Dawn", 0, { 2, 2, 1, -1 }, { 0, -3, -60, keep }),
                location("Market Day", 3, { 0, 2, 0, -1 }, { 0, -8, -4, keep }, { { Population, 75 } }),
                location("Tavern Corner", 0, { 1, 0, 1, -1 }, { -3, -8, 0, keep }) }),
        scene("Storm at Sea", "Calm seas turn to a storm off a wild shore; the ship rides it out.",
              { place(SeaShip, 0, 0, 0.3f, 0.0f), place(Beach, 2, -6, 0.6f, 0.55f), none, none },
              Ocean, 0.45f, D, Minor, { { Wetness, 70 }, { Weather, 30 }, { Era, 30 }, { Scale, 70 } }, ClassicArc, 108,
              { location("Calm Seas", 1, { 0, 1, -1, -1 }, { 0, -60, keep, keep }),
                location("Gathering Clouds", 4, { 0, 2, -1, -1 }, { 0, -8, keep, keep }, { { Weather, 60 } }),
                location("The Storm", 2, { 1, 2, -1, -1 }, { 0, -5, keep, keep }, { { Weather, 90 }, { Danger, 55 } }) },
              { { Weather, 95 } }),
        scene("Desert Caravan", "Across endless dunes from an oasis to an ancient temple, through a sandstorm.",
              { place(Desert, 0, 0, 0.3f, 0.0f), place(Temple, 1, -5, 0.55f, 0.5f), none, none },
              DesertWorld, 0.4f, E, Phrygian, { { Temperature, 85 }, { Wetness, 5 }, { Era, 20 }, { Magic, 35 } }, ExplorationLoop, 109,
              { location("Oasis", 0, { 0, 1, -1, -1 }, { 0, -60, keep, keep }),
                location("Endless Dunes", 3, { 1, 1, -1, -1 }, { 0, -8, keep, keep }),
                location("Sandstorm", 2, { 2, 1, -1, -1 }, { 0, -4, keep, keep }, { { Weather, 80 }, { Danger, 45 } }) }),
        scene("Into the Nebula", "A cruising ship jumps to warp and drifts into a stellar nursery, then the void.",
              { place(Nebula, 1, 0, 0.35f, 0.0f), place(Spaceship, 0, -4, 0.4f, -0.5f), place(Bridge, 2, -6, 0.5f, 0.5f), none },
              GasGiant, 0.45f, B, Lydian, { { Era, 95 }, { Scale, 90 }, { Magic, 40 }, { Danger, 15 } }, CinematicReveal, 110,
              { location("Cruising", 1, { 0, 0, 0, -1 }, { -6, 0, -60, keep }),
                location("Warp Jump", 4, { 1, 0, 2, -1 }, { -3, -3, -2, keep }),
                location("Stellar Nursery", 3, { 1, 0, 0, -1 }, { 0, -6, -60, keep }),
                location("The Void", 2, { 2, 2, 0, -1 }, { 0, -8, -60, keep }, { { Danger, 45 } }) }),
        scene("Frozen Citadel", "An ice-bound fortress above a forsaken temple and crystal caves.",
              { place(Castle, 0, 0, 0.35f, 0.0f), place(Temple, 2, -4, 0.5f, -0.5f), place(Cavern, 0, -5, 0.55f, 0.55f), none },
              IceGiant, 0.45f, C, Dorian, { { Temperature, 8 }, { Weather, 50 }, { Era, 20 }, { Magic, 45 }, { Scale, 80 } }, ClassicArc, 111,
              { location("Crystal Caves", 1, { 0, 2, 0, -1 }, { -60, -6, 0, keep }),
                location("Frozen Gate", 4, { 0, 2, 0, -1 }, { 0, -4, -8, keep }, { { Weather, 65 } }),
                location("Citadel Besieged", 2, { 2, 1, 0, -1 }, { 0, -4, -10, keep }, { { Danger, 60 }, { Weather, 75 } }) }),
        scene("Clockwork Metropolis", "Steam mills, assembly lines and the rush of a mechanical city.",
              { place(Factory, 0, 0, 0.35f, 0.3f), place(City, 0, -3, 0.45f, -0.4f), place(Mall, 0, -8, 0.6f, 0.6f), none },
              Rocky, 0.3f, G, Mixolydian, { { Machinery, 80 }, { Era, 55 }, { Population, 75 }, { Danger, 20 } }, ExplorationLoop, 112,
              { location("Steam Mill", 0, { 0, 0, 0, -1 }, { 0, -6, -60, keep }),
                location("Rush Hour", 3, { 2, 0, 0, -1 }, { -2, 0, -6, keep }, { { Activity, 65 } }),
                location("Night Shift", 4, { 1, 1, 1, -1 }, { 0, -4, -10, keep }, { { Population, 40 } }) }),
        scene("Dragon's Caldera", "A besieged keep on the rim of a lava world's caldera; ruins smoulder below.",
              { place(Castle, 2, 0, 0.35f, -0.2f, 0.5f, 0.6f), place(Ruins, 2, -4, 0.55f, 0.5f), place(Temple, 1, -6, 0.6f, -0.6f), none },
              LavaWorld, 0.45f, D, Phrygian, { { Temperature, 92 }, { Danger, 55 }, { Era, 20 }, { Magic, 50 }, { Scale, 85 } }, BossFight, 113,
              { location("The Rim", 0, { 0, 2, 1, -1 }, { -2, 0, -60, keep }),
                location("Ancient Rite", 4, { 0, 2, 1, -1 }, { -4, -4, 0, keep }, { { Magic, 70 } }),
                location("The Dragon", 2, { 2, 2, 1, -1 }, { 0, -4, -6, keep }, { { Danger, 80 } }) },
              { { Danger, 90 } }),
        scene("Drowned Temple", "A ghost ship over a sunken temple; flooded halls echo below the waves.",
              { place(Temple, 2, 0, 0.35f, 0.0f), place(Cavern, 2, -3, 0.5f, -0.5f), place(SeaShip, 2, -6, 0.6f, 0.55f), none },
              Ocean, 0.5f, F, Minor, { { Wetness, 90 }, { Magic, 50 }, { Era, 25 }, { Danger, 35 }, { Life, 20 } }, CinematicReveal, 114,
              { location("Ghost Ship", 1, { 2, 2, 2, -1 }, { -60, -8, 0, keep }),
                location("Descent", 3, { 2, 2, 2, -1 }, { -4, 0, -8, keep }),
                location("Sunken Sanctum", 4, { 0, 2, 2, -1 }, { 0, -3, -60, keep }, { { Magic, 70 } }) }),
        scene("Haunted Moor", "A harvest night in a lonely village at the edge of dark woods and haunted ruins.",
              { place(Ruins, 1, 0, 0.4f, 0.4f), place(Forest, 1, -3, 0.5f, -0.5f), place(Village, 1, -5, 0.6f, 0.0f), none },
              Rocky, 0.3f, A, Minor, { { Weather, 45 }, { Danger, 40 }, { Era, 20 }, { Life, 25 }, { Population, 20 } }, ClassicArc, 115,
              { location("Harvest Night", 0, { 1, 1, 1, -1 }, { -60, -6, 0, keep }),
                location("The Moor", 3, { 1, 1, 1, -1 }, { -4, 0, -8, keep }, { { Weather, 55 } }),
                location("The Haunting", 2, { 1, 1, 1, -1 }, { 0, -4, -60, keep }, { { Danger, 65 } }) }),
        scene("Elven Canopy", "An enchanted forest, a temple at sunrise and a grand ball in the trees.",
              { place(Forest, 0, 0, 0.3f, 0.0f), place(Temple, 0, -4, 0.5f, -0.5f), place(Palace, 0, -6, 0.55f, 0.55f), none },
              ForestWorld, 0.45f, E, Lydian, { { Life, 75 }, { Magic, 65 }, { Era, 15 }, { Danger, 5 }, { Temperature, 55 } }, TownTheme, 116,
              { location("Enchanted Glade", 3, { 0, 0, 0, -1 }, { 0, -6, -60, keep }),
                location("Sacred Dawn", 4, { 2, 0, 0, -1 }, { -3, 0, -60, keep }),
                location("The Grand Ball", 0, { 0, 0, 0, -1 }, { -4, -8, 0, keep }) }),
        scene("Abandoned Reactor", "A derelict ship, an abandoned station, a dead factory still humming.",
              { place(Spaceship, 2, 0, 0.35f, 0.0f), place(SpaceStation, 2, -3, 0.5f, -0.5f), place(Factory, 2, -6, 0.6f, 0.55f), none },
              Rocky, 0.35f, Cs, Phrygian, { { Era, 85 }, { Machinery, 60 }, { Danger, 45 }, { Population, 0 }, { Life, 0 } }, ExplorationLoop, 117,
              { location("Derelict", 1, { 2, 2, 2, -1 }, { 0, -6, -60, keep }),
                location("Reactor Core", 2, { 1, 1, 2, -1 }, { -2, 0, -4, keep }, { { Danger, 65 }, { Machinery, 75 } }) }),
        scene("Alien Jungle", "A thriving biodome gone wild; the hive below is waking up.",
              { place(Biodome, 0, 0, 0.35f, 0.0f), place(Hive, 1, -3, 0.5f, 0.5f), place(Forest, 1, -6, 0.55f, -0.55f), none },
              ForestWorld, 0.4f, Fs, Dorian, { { Life, 85 }, { Wetness, 65 }, { Temperature, 70 }, { Era, 80 }, { Danger, 30 } }, Battle, 118,
              { location("Thriving", 3, { 0, 0, 1, -1 }, { 0, -60, -6, keep }),
                location("Awakening", 4, { 1, 1, 1, -1 }, { -2, -2, -6, keep }, { { Danger, 50 } }),
                location("Swarm", 2, { 2, 2, 1, -1 }, { -3, 0, -8, keep }, { { Danger, 75 }, { Activity, 75 } }) },
              { { Danger, 80 } }),
        scene("Dead Mall Nights", "An empty mall after closing and the uncanny suburbs around it, all soft neon and reverb.",
              { place(Mall, 1, 0, 0.35f, 0.0f), place(Suburbs, 1, -4, 0.5f, -0.5f), none, none },
              Rocky, 0.25f, A, Major, { { Era, 65 }, { Population, 15 }, { Danger, 15 }, { Scale, 70 } }, ExplorationLoop, 119,
              { location("Empty Night", 4, { 1, 1, -1, -1 }),
                location("Dead Mall", 1, { 2, 2, -1, -1 }, { 0, -6, keep, keep }, { { Danger, 35 } }) }),
        scene("Summer by the Sea", "A sunny suburban afternoon that drifts down to the beach at sunset.",
              { place(Beach, 0, 0, 0.3f, 0.2f), place(Suburbs, 0, -3, 0.45f, -0.4f), none, none },
              Ocean, 0.35f, F, Major, { { Temperature, 70 }, { Wetness, 40 }, { Era, 60 }, { Life, 55 }, { Danger, 0 } }, TownTheme, 120,
              { location("Sunny Afternoon", 0, { 0, 0, -1, -1 }, { -6, 0, keep, keep }),
                location("Beach Day", 3, { 0, 0, -1, -1 }, { 0, -6, keep, keep }),
                location("Sunset", 2, { 1, 1, -1, -1 }, { 0, -6, keep, keep }) }),
        scene("Asteroid Shift Change", "Drills and beacons on an asteroid colony; then the cave-in alarm sounds across the station.",
              { place(Mining, 0, 0, 0.35f, 0.0f), place(SpaceStation, 0, -4, 0.5f, 0.5f), none, none },
              Rocky, 0.4f, G, Minor, { { Era, 85 }, { Machinery, 70 }, { Population, 40 }, { Danger, 25 } }, Battle, 121,
              { location("Shift Change", 0, { 0, 0, -1, -1 }),
                location("Night Shift", 1, { 2, 0, -1, -1 }, { 0, -6, keep, keep }),
                location("Cave-in", 2, { 1, 1, -1, -1 }, { 0, -2, keep, keep }, { { Danger, 75 } }) },
              { { Danger, 80 } }),
        scene("Battle Stations", "A starship at red alert launching from a spaceport above the megacity.",
              { place(Bridge, 1, 0, 0.3f, 0.0f), place(Spaceport, 2, -3, 0.5f, -0.5f), place(Megacity, 1, -7, 0.6f, 0.6f), none },
              GasGiant, 0.35f, C, Minor, { { Era, 95 }, { Machinery, 65 }, { Danger, 50 }, { Scale, 75 } }, BossFight, 122,
              { location("Launch", 4, { 0, 2, 1, -1 }, { -2, 0, -6, keep }),
                location("Red Alert", 2, { 1, 2, 2, -1 }, { 0, -4, -6, keep }, { { Danger, 70 } }),
                location("Warp Out", 1, { 2, 0, 1, -1 }, { 0, -8, -60, keep }) },
              { { Danger, 85 } }),
        scene("Royal Coronation", "A sacred dawn in the temple, the royal court assembles, and the palace's grand ball.",
              { place(Palace, 0, 0, 0.3f, 0.0f), place(Castle, 1, -3, 0.45f, -0.5f), place(Temple, 0, -5, 0.55f, 0.55f), none },
              Rocky, 0.3f, D, Major, { { Era, 25 }, { Magic, 30 }, { Population, 65 }, { Danger, 5 }, { Scale, 75 } }, CinematicReveal, 123,
              { location("Sacred Dawn", 3, { 0, 1, 0, -1 }, { -60, -8, 0, keep }),
                location("Royal Court", 0, { 1, 1, 0, -1 }, { -3, 0, -6, keep }),
                location("Grand Ball", 4, { 0, 1, 0, -1 }, { 0, -6, -10, keep }) }),
        scene("Void Between Stars", "Nothing but the void, and the slow pulse of a guardian older than the stars.",
              { place(Nebula, 2, 0, 0.35f, 0.0f), place(AlienRuins, 2, -4, 0.55f, 0.4f), none, none },
              GasGiant, 0.5f, Gs, Phrygian, { { Era, 95 }, { Scale, 95 }, { Magic, 45 }, { Danger, 35 }, { Life, 0 }, { Population, 0 } },
              CinematicReveal, 124,
              { location("Drift", 1, { 0, 0, -1, -1 }, { 0, -60, keep, keep }),
                location("The Void", 4, { 2, 0, -1, -1 }, { 0, -6, keep, keep }),
                location("Guardian", 2, { 2, 2, -1, -1 }, { -2, 0, keep, keep }, { { Danger, 60 } }) }),
        // The Drama tier (after the performance pass): dramatic scenes, arranged in arcs
        // (build, climax, drop, build) on the Drama tiers.
        dramatic(scene("Battle at the Castle Gate", "The army reaches the gate: war drums, spiccato strings and brass as the walls are stormed.",
              { place(Castle, 2, 0, 0.28f, 0.0f, 0.55f, 0.65f), place(Military, 1, -4, 0.45f, 0.5f, 0.55f, 0.6f), place(Forest, 1, -7, 0.6f, -0.55f), none },
              Rocky, 0.3f, D, Minor, { { Era, 22 }, { Danger, 70 }, { Activity, 70 }, { Scale, 70 }, { Weather, 40 } }, Siege, 201,
              { location("The Approach", 1, { 0, 2, 1, -1 }, { 0, -6, -4, keep }, { { Danger, 45 } }),
                location("Walls Stormed", 2, { 2, 1, 1, -1 }, { 0, -3, -8, keep }, { { Danger, 80 } }) }, { { Danger, 85 } }), drama::StyleDriving),
        dramatic(scene("Chase Through the Neon City", "A pursuit through rain-soaked streets: grinding bass, hybrid drums and cyberpunk stabs.",
              { place(Megacity, 2, 0, 0.28f, 0.1f, 0.6f, 0.65f), place(City, 1, -4, 0.5f, -0.5f, 0.55f, 0.55f), none, none },
              Ocean, 0.3f, E, Phrygian, { { Era, 92 }, { Wetness, 75 }, { Machinery, 65 }, { Danger, 65 }, { Activity, 75 }, { Population, 70 } }, Chase, 202,
              { location("Night Market", 1, { 0, 1, -1, -1 }), location("Riot", 2, { 2, 2, -1, -1 }, { 0, -3, keep, keep }, { { Danger, 80 } }) }),
              drama::StyleDriving),
        dramatic(scene("The Dragon in the Deep", "A boss in the dark under the temple: deep drums, choir and brass, a chromatic shift into each phase.",
              { place(Cavern, 1, 0, 0.3f, 0.0f, 0.5f, 0.6f), place(Castle, 2, -3, 0.45f, -0.5f, 0.55f, 0.65f), place(Temple, 2, -6, 0.55f, 0.55f), none },
              LavaWorld, 0.4f, C, Minor, { { Era, 25 }, { Temperature, 80 }, { Danger, 75 }, { Magic, 55 }, { Scale, 85 } }, BossFight, 203,
              { location("The Descent", 1, { 1, 2, 2, -1 }, { 0, -8, -6, keep }, { { Danger, 50 } }),
                location("The Dragon", 2, { 0, 2, 2, -1 }, { 0, -2, -6, keep }, { { Danger, 90 } }) }, { { Danger, 90 } }), drama::StyleEpic),
        dramatic(scene("Siege of the Desert Fortress", "Sandstorm and siege: racing frame drums, plucked strings, horns over the fallen palace.",
              { place(Desert, 2, 0, 0.3f, 0.0f, 0.55f, 0.6f), place(Military, 1, -4, 0.45f, 0.5f, 0.55f, 0.6f), place(Palace, 2, -6, 0.55f, -0.5f), none },
              DesertWorld, 0.45f, A, Phrygian, { { Era, 25 }, { Temperature, 85 }, { Weather, 60 }, { Danger, 70 }, { Activity, 70 } }, Siege, 204,
              { location("Dunes", 1, { 1, 0, 1, -1 }, { 0, -8, -6, keep }, { { Danger, 40 } }),
                location("Sandstorm Assault", 2, { 2, 1, 2, -1 }, { 0, -3, -6, keep }, { { Danger, 85 }, { Weather, 80 } }) }), drama::StyleDriving),
        dramatic(scene("Space Battle over the Station", "Battle stations: hybrid pulses and percussion, braams and brass as the fleet engages.",
              { place(Bridge, 1, 0, 0.28f, -0.1f, 0.6f, 0.7f), place(SpaceStation, 1, -4, 0.45f, 0.5f, 0.5f, 0.6f), place(Spaceport, 2, -6, 0.55f, -0.55f), none },
              GasGiant, 0.4f, Fs, Minor, { { Era, 95 }, { Machinery, 70 }, { Danger, 70 }, { Activity, 70 }, { Scale, 80 } }, Battle, 205,
              { location("Contact", 1, { 0, 0, 0, -1 }, { 0, -6, -6, keep }, { { Danger, 45 } }),
                location("Engaged", 2, { 1, 1, 2, -1 }, { 0, -3, -6, keep }, { { Danger, 85 } }) }, { { Danger, 85 } }), drama::StyleDriving),
        dramatic(scene("Escape from the Hive", "Run: the swarm wakes, the ship's alarms sound, the guardian closes in.",
              { place(Hive, 2, 0, 0.3f, 0.0f, 0.6f, 0.7f), place(Spaceship, 1, -4, 0.45f, 0.5f, 0.55f, 0.65f), place(AlienRuins, 2, -7, 0.6f, -0.55f), none },
              LavaWorld, 0.4f, B, Phrygian, { { Era, 90 }, { Life, 80 }, { Danger, 75 }, { Activity, 75 }, { Machinery, 40 } }, Chase, 206,
              { location("Dormant", 1, { 0, 0, 0, -1 }, { 0, -6, -60, keep }, { { Danger, 40 } }),
                location("Swarm", 2, { 2, 1, 2, -1 }, { 0, -3, -6, keep }, { { Danger, 85 } }) }), drama::StyleDriving),
        dramatic(scene("The Final Stand", "The last defenders hold the keep: war drums, a great choir and brass, and the climax that turns the battle.",
              { place(Castle, 0, 0, 0.28f, 0.0f, 0.55f, 0.55f), place(Palace, 1, -4, 0.45f, 0.5f), place(Village, 1, -8, 0.6f, -0.55f), none },
              Rocky, 0.35f, D, Minor, { { Era, 22 }, { Danger, 60 }, { Scale, 80 }, { Magic, 35 }, { Activity, 60 } }, FinalStand, 207,
              { location("Before Dawn", 1, { 0, 1, 2, -1 }, { 0, -6, -6, keep }, { { Danger, 40 } }),
                location("The Last Charge", 2, { 2, 1, 1, -1 }, { 0, -2, -8, keep }, { { Danger, 80 } }) }, { { Danger, 75 } }), drama::StyleEpic),
        dramatic(scene("Tragic Loss", "After the battle: low piano strikes, slow strings and a distant choir over the ruins.",
              { place(Ruins, 2, 0, 0.3f, 0.0f, 0.35f, 0.5f), place(Temple, 2, -4, 0.5f, 0.5f), place(Forest, 1, -8, 0.6f, -0.55f), none },
              Rocky, 0.35f, A, Minor, { { Era, 25 }, { Danger, 35 }, { Weather, 45 }, { Wetness, 55 }, { Life, 15 }, { Activity, 30 } }, Lament, 208,
              { location("Ashes", 1, { 2, 2, 1, -1 }, { 0, -6, -8, keep }), location("Mourning", 4, { 1, 2, 1, -1 }, { 0, -3, -6, keep }, { { Weather, 60 } }) }),
              drama::StyleTragic),
        dramatic(scene("Triumphant Return", "The heroes come home: a march, fanfare brass and choir through the market to the palace.",
              { place(Palace, 0, 0, 0.3f, 0.0f, 0.55f, 0.25f), place(Village, 0, -4, 0.45f, -0.5f, 0.55f), place(Castle, 1, -6, 0.55f, 0.5f), none },
              ForestWorld, 0.3f, G, Major, { { Era, 25 }, { Danger, 10 }, { Population, 80 }, { Activity, 65 }, { Temperature, 60 }, { Scale, 75 } },
              FinalStand, 209,
              { location("The Road Home", 0, { 1, 2, 1, -1 }, { -3, -2, -8, keep }), location("The Palace", 3, { 0, 0, 1, -1 }, { 0, -6, -3, keep }) }),
              drama::StyleTriumphant),
        dramatic(scene("Pirate Battle in the Storm", "Broadsides in a storm: battle drums at sea, strings and brass, a brawl on deck.",
              { place(SeaShip, 1, 0, 0.28f, 0.0f, 0.6f, 0.65f), place(Tavern, 2, -5, 0.5f, -0.5f, 0.6f, 0.6f), place(Beach, 2, -8, 0.6f, 0.55f), none },
              Ocean, 0.45f, E, Minor, { { Era, 30 }, { Wetness, 80 }, { Weather, 75 }, { Danger, 70 }, { Activity, 75 }, { Scale, 75 } }, Battle, 210,
              { location("Sails Ahead", 1, { 0, 0, 0, -1 }, { 0, -6, -6, keep }, { { Weather, 45 } }),
                location("Boarding", 2, { 1, 2, 2, -1 }, { 0, -2, -6, keep }, { { Weather, 85 }, { Danger, 85 } }) }), drama::StyleDriving),
        dramatic(scene("Mining Colony Uprising", "The workers rise: drills and pistons as rhythm, metal hits, grinding bass and braams.",
              { place(Mining, 1, 0, 0.3f, 0.0f, 0.6f, 0.65f), place(Factory, 1, -4, 0.45f, 0.5f, 0.6f, 0.6f), place(Megacity, 2, -7, 0.6f, -0.55f), none },
              Rocky, 0.35f, Cs, Phrygian, { { Era, 85 }, { Machinery, 85 }, { Danger, 70 }, { Activity, 70 }, { Population, 70 } }, Battle, 211,
              { location("Shift Change", 1, { 0, 2, 0, -1 }, { 0, -6, -60, keep }, { { Danger, 40 } }),
                location("Uprising", 2, { 1, 1, 2, -1 }, { 0, -3, -6, keep }, { { Danger, 85 } }) }), drama::StyleDriving),
        dramatic(scene("Ambush in the Dark Woods", "Something hunts in the trees: a heartbeat, tribal drums and strings, then the ambush.",
              { place(Forest, 1, 0, 0.3f, 0.0f, 0.5f, 0.65f), place(Military, 2, -5, 0.5f, 0.5f, 0.5f, 0.6f), place(Village, 2, -8, 0.6f, -0.55f), none },
              ForestWorld, 0.35f, G, Minor, { { Era, 25 }, { Danger, 60 }, { Life, 55 }, { Weather, 35 }, { Activity, 55 } }, Chase, 212,
              { location("Watched", 1, { 1, 2, 2, -1 }, { 0, -8, -6, keep }, { { Danger, 45 } }),
                location("Ambush", 2, { 1, 1, 2, -1 }, { 0, -3, -8, keep }, { { Danger, 80 } }) }), drama::StyleSuspense),
        dramatic(scene("Awakening of the Guardian", "The ancient guardian rises in the nebula: vast hybrid swells, ritual drums pitched down, a great choir.",
              { place(AlienRuins, 2, 0, 0.3f, 0.0f, 0.5f, 0.6f), place(Nebula, 2, -4, 0.5f, -0.5f), place(Bridge, 1, -6, 0.5f, 0.5f, 0.55f, 0.6f), none },
              GasGiant, 0.5f, F, Minor, { { Era, 95 }, { Magic, 75 }, { Scale, 95 }, { Danger, 65 } }, BossFight, 213,
              { location("Signal", 1, { 0, 0, 0, -1 }, { 0, -6, -8, keep }, { { Danger, 40 } }),
                location("Guardian", 2, { 2, 2, 1, -1 }, { 0, -3, -4, keep }, { { Danger, 85 } }) }, { { Danger, 85 } }), drama::StyleEpic),
        dramatic(scene("Factory Breakdown", "The line runs out of control: pistons, metal percussion and sirens through the rain-soaked city.",
              { place(Factory, 2, 0, 0.3f, 0.0f, 0.6f, 0.65f), place(City, 2, -5, 0.5f, -0.5f), place(Mining, 0, -8, 0.6f, 0.55f), none },
              Rocky, 0.3f, D, Phrygian, { { Era, 70 }, { Machinery, 90 }, { Danger, 65 }, { Activity, 70 }, { Wetness, 60 } }, Chase, 214,
              { location("Assembly Line", 1, { 2, 1, 0, -1 }, { 0, -6, -8, keep }), location("Breakdown", 2, { 1, 2, 0, -1 }, { 0, -3, -6, keep }, { { Danger, 85 } }) }),
              drama::StyleDriving),
        dramatic(scene("Last Light of the Temple", "The rite fails and the temple floods: slow strings, piano strikes and a mourning choir.",
              { place(Temple, 1, 0, 0.3f, 0.0f, 0.35f, 0.5f), place(Cavern, 2, -4, 0.5f, 0.5f), place(Ruins, 1, -7, 0.55f, -0.5f), none },
              Ocean, 0.4f, B, Minor, { { Era, 20 }, { Magic, 60 }, { Wetness, 70 }, { Danger, 40 }, { Scale, 80 } }, Lament, 215,
              { location("The Rite", 1, { 1, 1, 1, -1 }, { 0, -6, -8, keep }), location("The Flood", 4, { 2, 2, 1, -1 }, { 0, -3, -6, keep }, { { Wetness, 85 } }) }),
              drama::StyleTragic),
        dramatic(scene("Victory Parade", "Drums and brass down the avenue: the army's march, the crowd, the palace balcony.",
              { place(Military, 0, 0, 0.3f, 0.0f, 0.6f, 0.25f), place(City, 0, -4, 0.45f, 0.5f), place(Palace, 1, -6, 0.55f, -0.5f), none },
              Rocky, 0.3f, C, Major, { { Era, 55 }, { Danger, 10 }, { Population, 90 }, { Activity, 70 }, { Scale, 70 } }, ClassicArc, 216,
              { location("Avenue", 0, { 0, 0, 1, -1 }), location("Balcony", 3, { 0, 0, 1, -1 }, { -3, -6, 0, keep }) }), drama::StyleTriumphant),
        dramatic(scene("Warp Jump Pursuit", "Hunted between the stars: the bridge's pulses, hybrid drums and braams, one jump ahead.",
              { place(Bridge, 2, 0, 0.28f, 0.0f, 0.6f, 0.7f), place(Spaceship, 1, -4, 0.45f, -0.5f, 0.55f, 0.65f), place(Nebula, 1, -7, 0.6f, 0.55f), none },
              IceGiant, 0.4f, A, Minor, { { Era, 95 }, { Machinery, 70 }, { Danger, 70 }, { Activity, 75 }, { Scale, 85 } }, Chase, 217,
              { location("Pursued", 1, { 0, 1, 1, -1 }, { 0, -6, -6, keep }, { { Danger, 50 } }),
                location("Warp Jump", 2, { 2, 1, 1, -1 }, { 0, -3, -6, keep }, { { Danger, 85 } }) }), drama::StyleDriving),
    };
    return scenes;
}

// Deterministic randomness for Inspire: a hash of (seed, draw).
struct Dice {
    uint32_t seed, n = 0;
    uint32_t next() {
        uint32_t x = seed * 0x9E3779B1u + (++n) * 0x85EBCA77u;
        x ^= x >> 15;
        x *= 0x2C1B3C6Du;
        x ^= x >> 12;
        x *= 0x297A2D39u;
        x ^= x >> 15;
        return x;
    }
    float unit() { return float(next() >> 8) / 16777216.0f; }
    float range(float a, float b) { return a + (b - a) * unit(); }
    int below(int count) { return count <= 1 ? 0 : int(next() % uint32_t(count)); }
    template <typename T>
    T pick(std::initializer_list<T> list) { return *(list.begin() + below(int(list.size()))); }
};

// Names for an inspired scene's Locations, by what they do.
constexpr std::array<const char*, 4> calmNames { "Arrival", "Stillness", "First Light", "Quiet" };
constexpr std::array<const char*, 4> risingNames { "Rising", "Gathering", "The Way In", "Unrest" };
constexpr std::array<const char*, 4> peakNames { "Climax", "The Storm", "Showdown", "Breaking Point" };

} // namespace

int factoryCount() { return int(table().size()); }

const Scene& factory(int index) { return table()[std::size_t(std::clamp(index, 0, factoryCount() - 1))]; }

uint32_t nextSeed(uint32_t seed) {
    Dice d { seed + 7919u };
    const uint32_t next = 1u + d.next() % 9999u;
    return next == seed ? (seed % 9999u) + 1u : next;
}

int moodFor(uint32_t seed) {
    // Any: the dramatic moods about three times in five.
    Dice d { seed ^ 0x5EEDu };
    const float r = d.unit();
    static constexpr std::array<std::pair<float, int>, 8> weights { { { 0.14f, MoodCalm }, { 0.27f, MoodAdventure }, { 0.37f, MoodMystery },
                                                                      { 0.49f, MoodTension }, { 0.66f, MoodBattle }, { 0.79f, MoodEpic },
                                                                      { 0.88f, MoodTragic }, { 1.01f, MoodTriumphant } } };
    for (const auto& [edge, mood] : weights)
        if (r < edge) return mood;
    return MoodBattle;
}

namespace {
// What each mood asks of a scene.
struct MoodRecipe {
    std::vector<int> places;                // the Places that suit it (chosen first within the world)
    int character;                          // -1: any; else preferred Character (0..2)
    std::vector<int> keys;                  // Moods (scales)
    float intensityLo, intensityHi;
    int dangerLo, dangerHi, activityLo, activityHi;
    float tensionLo, tensionHi;
    int style;                              // drama::Style
    std::vector<int> templates;
    bool arcs;
};
const MoodRecipe& recipe(int mood) {
    using namespace arr;
    static const std::array<MoodRecipe, InspireMoodCount> table { {
        { {}, -1, {}, 0.45f, 0.55f, 8, 55, 30, 55, 0.2f, 0.45f, drama::StyleAuto, {}, false },   // any (unused)
        { { Village, Forest, Beach, Biodome, Suburbs, Mall, Temple, Nebula, Desert }, 0, { Major, Pentatonic, Lydian }, 0.3f, 0.42f, 0, 12, 18, 35,
          0.1f, 0.2f, drama::StyleAuto, { TownTheme, ExplorationLoop }, false },
        { { Village, Forest, Desert, Beach, SeaShip, Tavern, Spaceport, Biodome, Bridge }, -1, { Dorian, Mixolydian, Major }, 0.58f, 0.66f, 15, 35,
          45, 65, 0.3f, 0.45f, drama::StyleDriving, { ExplorationLoop, ClassicArc, Chase }, true },
        { { Temple, Ruins, AlienRuins, Nebula, Cavern, Forest, Hive, SpaceStation }, 0, { Lydian, WholeTone, Dorian, Phrygian }, 0.45f, 0.55f, 25, 40, 20, 40,
          0.35f, 0.5f, drama::StyleSuspense, { CinematicReveal, ExplorationLoop }, false },
        { { Cavern, Hive, Ruins, Spaceship, SpaceStation, Military, Megacity, Mining, Bridge }, 1, { Phrygian, Minor }, 0.64f, 0.72f, 45, 65, 40, 60,
          0.6f, 0.75f, drama::StyleSuspense, { Chase, BossFight }, true },
        { { Castle, Military, Tavern, Megacity, Bridge, SeaShip, Palace, Forest, Spaceport, Mining }, 2, { Minor, Phrygian, Dorian }, 0.88f, 0.96f, 60, 82,
          65, 88, 0.6f, 0.8f, drama::StyleDriving, { Battle, Chase, FinalStand, Siege }, true },
        { { Castle, Temple, Palace, AlienRuins, Nebula, Desert, Bridge, SeaShip }, 1, { Minor, Dorian }, 0.86f, 0.94f, 35, 55, 40, 60, 0.45f, 0.6f,
          drama::StyleEpic, { CinematicReveal, FinalStand, Siege }, true },
        { { Ruins, Temple, Cavern, Forest, SeaShip, Palace, SpaceStation, Suburbs }, 2, { Minor }, 0.7f, 0.8f, 30, 50, 15, 35, 0.45f, 0.6f,
          drama::StyleTragic, { Lament }, true },
        { { Palace, Castle, Village, Military, Bridge, SeaShip, Desert, Spaceport }, 1, { Major, Mixolydian, Lydian }, 0.82f, 0.9f, 10, 25, 55, 75,
          0.2f, 0.35f, drama::StyleTriumphant, { FinalStand, ClassicArc }, true },
    } };
    return table[std::size_t(std::clamp(mood, 0, InspireMoodCount - 1))];
}
bool listed(const std::vector<int>& list, int value) { return std::find(list.begin(), list.end(), value) != list.end(); }
} // namespace

Scene inspire(uint32_t seed, int moodIn) {
    Dice d { seed };
    Scene s;
    s.seed = seed;
    const int mood = moodIn > MoodAny && moodIn < InspireMoodCount ? moodIn : moodFor(seed);
    const auto& want = recipe(mood);
    // A world: fantasy, sci-fi, modern or nature-led; the mood's own Places lean it.
    const float theme = d.unit();
    int category = theme < 0.34f ? MedievalFantasy : theme < 0.64f ? SciFi : theme < 0.84f ? Modern : Nature;
    {
        // A world with none of the mood's Places takes the world of one of them.
        bool any = false;
        for (int t : want.places) any |= placeInfo(t).category == category;
        if (!any && want.places.size() > 0) category = placeInfo(*(want.places.begin() + d.below(int(want.places.size())))).category;
    }
    std::vector<int> own, nature, other;
    for (int t = 1; t < PlaceTypeCount; ++t) {
        if (!placeImplemented[std::size_t(t)]) continue;
        const int c = placeInfo(t).category;
        (c == category ? own : c == Nature ? nature : other).push_back(t);
    }
    const float countRoll = d.unit();
    const int count = countRoll < 0.3f ? 2 : countRoll < 0.78f ? 3 : 4;
    std::vector<int> chosen;
    const auto take = [&](std::vector<int>& from) {
        if (from.empty()) return;
        // The mood's Places first (most of the time).
        std::vector<int> suited;
        for (std::size_t k = 0; k < from.size(); ++k)
            if (listed(want.places, from[k])) suited.push_back(int(k));
        const int i = !suited.empty() && d.unit() < 0.8f ? suited[std::size_t(d.below(int(suited.size())))] : d.below(int(from.size()));
        chosen.push_back(from[std::size_t(i)]);
        from.erase(from.begin() + i);
    };
    // The leading Place: one of the mood's from this world where there is one.
    {
        std::vector<int> suited;
        for (std::size_t k = 0; k < own.size(); ++k)
            if (listed(want.places, own[k])) suited.push_back(int(k));
        if (!suited.empty()) {
            const int i = suited[std::size_t(d.below(int(suited.size())))];
            chosen.push_back(own[std::size_t(i)]);
            own.erase(own.begin() + i);
        } else take(own);
    }
    bool blended = false;
    for (int i = 1; i < count; ++i) {
        const float r = d.unit();
        // Mostly the same world or nature; now and then one from another world (a blend).
        // A nature-led world takes more nature or a Place from any world.
        if (category == Nature) take(r < 0.45f && !own.empty() ? own : other);
        else if (r < 0.1f && !other.empty() && !blended) {
            take(other);
            blended = true;
        }
        else if (r < 0.62f || nature.empty()) take(own.empty() ? nature : own);
        else take(nature);
    }
    std::array<float, placeSlots> pans { -0.15f, 0.5f, -0.55f, 0.65f };
    for (int i = 0; i < int(chosen.size()) && i < placeSlots; ++i) {
        auto& slot = s.slots[std::size_t(i)];
        slot.type = chosen[std::size_t(i)];
        slot.character = want.character >= 0 && d.unit() < 0.7f ? want.character : d.below(placeCharacters);
        slot.levelDb = i == 0 ? 0.0f : d.range(-6.0f, -2.5f);
        slot.distance = i == 0 ? d.range(0.25f, 0.4f) : d.range(0.4f, 0.62f);
        slot.pan = pans[std::size_t(i)] * d.range(0.7f, 1.1f);
        slot.activity = d.range(0.3f, 0.55f);
        slot.tension = d.range(want.tensionLo, want.tensionHi);
    }
    // Key: the mood's, else the first Place's suggested Mood more often than not, or one
    // that suits the world.
    s.root = d.below(12);
    const int suggested = placeInfo(chosen.front()).mood;
    if (want.keys.size() > 0) s.mood = *(want.keys.begin() + d.below(int(want.keys.size())));
    else if (suggested >= 0 && d.unit() < 0.6f) s.mood = suggested;
    else if (category == MedievalFantasy) s.mood = d.pick({ Minor, Dorian, Phrygian, Lydian, Mixolydian, Pentatonic });
    else if (category == SciFi) s.mood = d.pick({ Minor, Lydian, WholeTone, Phrygian, Dorian });
    else if (category == Modern) s.mood = d.pick({ Major, Dorian, Mixolydian, Pentatonic, Minor });
    else s.mood = d.pick({ Pentatonic, Lydian, Dorian, Major });
    // A Planet that fits the Places.
    s.planet = category == MedievalFantasy ? d.pick({ Rocky, ForestWorld, Ocean, IceGiant })
             : category == SciFi           ? d.pick({ GasGiant, IceGiant, Rocky, LavaWorld })
             : category == Modern          ? d.pick({ Rocky, Ocean, Rocky })
                                           : d.pick({ ForestWorld, Ocean });
    for (int t : chosen) {
        if (t == Desert) s.planet = DesertWorld;
        if (t == Beach || t == SeaShip) s.planet = Ocean;
        if (t == Nebula) s.planet = GasGiant;
    }
    s.influence = d.range(0.2f, 0.5f);
    // World Controls near their defaults, the Era and Magic of the world; Danger and
    // Activity as the mood asks.
    const int era = category == MedievalFantasy ? int(d.range(12, 35)) : category == SciFi ? int(d.range(78, 96)) : category == Modern ? int(d.range(50, 66)) : int(d.range(25, 55));
    const int danger = int(d.range(float(want.dangerLo), float(want.dangerHi)));
    const int activity = int(d.range(float(want.activityLo), float(want.activityHi)));
    s.world = world({ { Era, era }, { Danger, danger }, { Temperature, int(d.range(25, 75)) }, { Weather, int(d.range(15, 55)) },
                      { Wetness, int(d.range(15, 60)) }, { Magic, category == MedievalFantasy ? int(d.range(35, 70)) : int(d.range(10, 45)) },
                      { Life, int(d.range(20, 70)) }, { Scale, int(d.range(40, 85)) }, { Activity, activity } });
    if (category == SciFi || category == Modern) s.world[Machinery] = int8_t(d.range(30, 70));
    if (mood == MoodEpic) s.world[Scale] = int8_t(d.range(75, 95));
    if (mood == MoodCalm) s.world[Weather] = int8_t(d.range(5, 25));
    s.intensity = d.range(want.intensityLo, want.intensityHi);
    // The Drama tier: every inspired scene can change gear; the mood sets its feel.
    s.drama = 1.0f;
    s.dramaStyle = want.style;
    s.arcs = want.arcs;
    s.inspired = mood;
    // A template for the mood.
    if (want.templates.size() > 0) s.templ = *(want.templates.begin() + d.below(int(want.templates.size())));
    else if (danger > 42) s.templ = d.pick({ arr::Battle, arr::BossFight, arr::ClassicArc });
    else s.templ = d.pick({ arr::TownTheme, arr::ClassicArc, arr::CinematicReveal, arr::ExplorationLoop });
    // Locations: calm, (rising,) climax. The first Place changes Character along the way.
    const int first = s.slots[0].character;
    s.locationCount = d.unit() < 0.5f ? 2 : 3;
    auto& calm = s.locations[0];
    calm = location(calmNames[std::size_t(d.below(4))], 1);
    calm.world[Danger] = int8_t(std::max(0, danger - 15));
    if (chosen.size() > 2) calm.levelDb[chosen.size() - 1] = -60.0f;   // the last Place arrives later
    if (s.locationCount == 3) {
        auto& rising = s.locations[1];
        rising = location(risingNames[std::size_t(d.below(4))], 4);
        rising.character[0] = int8_t((first + 1) % placeCharacters);
        rising.world[Danger] = int8_t(std::min(100, danger + 10));
    }
    auto& peak = s.locations[std::size_t(s.locationCount - 1)];
    peak = location(peakNames[std::size_t(d.below(4))], 2);
    peak.character[0] = int8_t((first + 2) % placeCharacters);
    peak.world[Danger] = int8_t(std::min(100, danger + 25));
    peak.world[Weather] = int8_t(std::min(100, int(s.world[Weather]) + 20));
    peak.world[Activity] = int8_t(std::min(100, int(s.world[Activity]) + 20));
    // A dramatic mood starts where it is: the first Location keeps the mood's Characters
    // and Danger (the climb is the arrangement's).
    if (want.arcs) {
        calm.world[Danger] = int8_t(danger);
        calm.character = { -1, -1, -1, -1 };
    }
    // Small changes to each instrument's effects.
    s.scaledFx = true;
    for (auto& f : s.fxScale) f = { d.range(0.8f, 1.25f), d.range(0.7f, 1.3f), d.range(0.8f, 1.2f) };
    // A name from the Places.
    s.name = std::string(placeInfo(chosen.front()).characters[std::size_t(s.slots[0].character)]) + " " + placeNames[std::size_t(chosen.front())];
    s.description = "Inspired by World Seed " + std::to_string(seed) + " (" + inspireMoodNames[std::size_t(mood)] + "):";
    for (std::size_t i = 0; i < chosen.size() && i < std::size_t(placeSlots); ++i) s.description += std::string(i == 0 ? " " : ", ") + placeNames[std::size_t(chosen[i])];
    return s;
}

WorldValues worldValues(const Scene& scene, const WorldValues& session) {
    WorldValues v;
    for (int p = 0; p < PCount; ++p) v[std::size_t(p)] = specs[std::size_t(p)].def;
    for (int p : { int(PPower), int(PMaster), int(PQuality), int(PPlayMode), int(PPlaceOuts) }) v[std::size_t(p)] = session[std::size_t(p)];
    // The environment mix stays the composer's (levels, mutes, solos, Environment master).
    for (int p = 0; p < PCount; ++p)
        if (isEnvironmentMix(p) || isInstrumentParam(p)) v[std::size_t(p)] = session[std::size_t(p)];
    v[PSeed] = float(std::clamp<uint32_t>(scene.seed, 1, 9999));
    v[PRoot] = float(std::clamp(scene.root, 0, 11));
    v[PMood] = float(std::clamp(scene.mood, 0, 8));
    v[PPlanet] = float(std::clamp(scene.planet, 0, 6));
    v[PPlanetInfluence] = dsp::clamp01(scene.influence);
    v[PIntensity] = dsp::clamp01(scene.intensity);
    // The Drama tier: the scenes from before it play the Classic score, as they always did.
    v[PDrama] = dsp::clamp01(scene.drama);
    v[PDramaStyle] = float(std::clamp(scene.dramaStyle, 0, 5));
    for (int w = 0; w < WorldControlCount; ++w)
        if (scene.world[std::size_t(w)] >= 0) v[std::size_t(PTemperature + w)] = float(scene.world[std::size_t(w)]) / 100.0f;
    for (int slot = 0; slot < placeSlots; ++slot) {
        const auto& setup = scene.slots[std::size_t(slot)];
        const auto at = [slot](int field) { return std::size_t(placeParam(slot, field)); };
        const int type = std::clamp(setup.type, 0, PlaceTypeCount - 1);
        v[at(PlType)] = float(type);
        v[at(PlSeed)] = float(1 + (scene.seed * 37u + uint32_t(slot) * 1013u) % 9999u);
        if (type == NoPlace) continue;
        v[at(PlCharacter)] = float(std::clamp(setup.character, 0, placeCharacters - 1));
        v[at(PlLevel)] = std::clamp(setup.levelDb, -60.0f, 6.0f);
        v[at(PlDistance)] = dsp::clamp01(setup.distance);
        v[at(PlPan)] = std::clamp(setup.pan, -1.0f, 1.0f);
        v[at(PlActivity)] = dsp::clamp01(setup.activity);
        v[at(PlTension)] = dsp::clamp01(setup.tension);
        // Each instrument's effects at that instrument's own defaults (scaled by Inspire).
        for (int e = 0; e < placeElements; ++e) {
            const auto& scale = scene.fxScale[std::size_t(slot * placeElements + e)];
            for (int f = 0; f < FxFieldCount; ++f) {
                float value = instrumentFxDefault(type, e, f);
                if (scene.scaledFx) {
                    if (f == FxReverb) value *= scale[0];
                    if (f == FxDelay) value *= scale[1];
                    if (f == FxAmount) value *= scale[2];
                }
                const auto& spec = specs[std::size_t(placeParam(slot, fxField(e, f)))];
                v[at(fxField(e, f))] = std::clamp(value, spec.min, spec.max);
            }
        }
    }
    return v;
}

WorldValues locationValues(const Scene& scene, int index, const WorldValues& base) {
    WorldValues v = base;
    if (index < 0 || index >= scene.locationCount) return v;
    const auto& l = scene.locations[std::size_t(index)];
    for (int slot = 0; slot < placeSlots; ++slot) {
        if (scene.slots[std::size_t(slot)].type == NoPlace) continue;
        if (l.character[std::size_t(slot)] >= 0) v[std::size_t(placeParam(slot, PlCharacter))] = float(std::clamp<int>(l.character[std::size_t(slot)], 0, 2));
        if (l.levelDb[std::size_t(slot)] < keepLevel) v[std::size_t(placeParam(slot, PlLevel))] = std::clamp(l.levelDb[std::size_t(slot)], -60.0f, 6.0f);
    }
    for (int w = 0; w < WorldControlCount; ++w)
        if (l.world[std::size_t(w)] >= 0) v[std::size_t(PTemperature + w)] = float(l.world[std::size_t(w)]) / 100.0f;
    return v;
}

int locationFor(int kind, int locations) {
    if (locations <= 1) return 0;
    const int last = locations - 1, middle = std::min(1, last);
    switch (kind) {
        case arr::KindPeak: return last;
        case arr::KindBuild: return locations >= 3 ? middle : 0;
        case arr::KindLoop: return locations >= 3 ? middle : last;
        case arr::KindBridge: return locations >= 4 ? 2 : last;
        case arr::KindBreak: return locations >= 4 ? 2 : 0;
        default: return 0;   // intro, wind-down, outro
    }
}

arr::Arrangement arrangement(const Scene& scene) {
    std::array<int, placeSlots> types {};
    for (int slot = 0; slot < placeSlots; ++slot) types[std::size_t(slot)] = scene.slots[std::size_t(slot)].type;
    auto a = arr::autoArrange(arr::makeTemplate(scene.templ), types, scene.seed);
    for (int i = 0; i < a.count; ++i) {
        auto& s = a.sections[std::size_t(i)];
        const int kind = arr::kindOf(a, i);
        if (scene.locationCount > 0) s.location = int8_t(locationFor(kind, scene.locationCount));
        if (kind == arr::KindPeak) s.world = scene.peakWorld;
    }
    // A dramatic scene's arcs (the scenes from before the Drama tier keep theirs).
    if (scene.drama > 0.0f && scene.arcs) arr::dramatize(a, scene.seed);
    return a;
}

} // namespace wb::scenes
