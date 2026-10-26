#include "Scenes.h"
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

Scene inspire(uint32_t seed) {
    Dice d { seed };
    Scene s;
    s.seed = seed;
    // A world: fantasy, sci-fi, modern or nature-led.
    const float theme = d.unit();
    const int category = theme < 0.34f ? MedievalFantasy : theme < 0.64f ? SciFi : theme < 0.84f ? Modern : Nature;
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
        const int i = d.below(int(from.size()));
        chosen.push_back(from[std::size_t(i)]);
        from.erase(from.begin() + i);
    };
    take(own);
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
        slot.character = d.below(placeCharacters);
        slot.levelDb = i == 0 ? 0.0f : d.range(-6.0f, -2.5f);
        slot.distance = i == 0 ? d.range(0.25f, 0.4f) : d.range(0.4f, 0.62f);
        slot.pan = pans[std::size_t(i)] * d.range(0.7f, 1.1f);
        slot.activity = d.range(0.3f, 0.55f);
        slot.tension = d.range(0.2f, 0.45f);
    }
    // Key: the first Place's suggested Mood more often than not, or one that suits the world.
    s.root = d.below(12);
    const int suggested = placeInfo(chosen.front()).mood;
    if (suggested >= 0 && d.unit() < 0.6f) s.mood = suggested;
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
    // World Controls near their defaults, the Era and Magic of the world.
    const int era = category == MedievalFantasy ? int(d.range(12, 35)) : category == SciFi ? int(d.range(78, 96)) : category == Modern ? int(d.range(50, 66)) : int(d.range(25, 55));
    const int danger = int(d.range(8, 55));
    s.world = world({ { Era, era }, { Danger, danger }, { Temperature, int(d.range(25, 75)) }, { Weather, int(d.range(15, 55)) },
                      { Wetness, int(d.range(15, 60)) }, { Magic, category == MedievalFantasy ? int(d.range(35, 70)) : int(d.range(10, 45)) },
                      { Life, int(d.range(20, 70)) }, { Scale, int(d.range(40, 85)) }, { Activity, int(d.range(30, 55)) } });
    if (category == SciFi || category == Modern) s.world[Machinery] = int8_t(d.range(30, 70));
    s.intensity = d.range(0.45f, 0.55f);
    // A template for the mood of the world.
    if (danger > 42) s.templ = d.pick({ arr::Battle, arr::BossFight, arr::ClassicArc });
    else if (category == SciFi) s.templ = d.pick({ arr::CinematicReveal, arr::ExplorationLoop, arr::ClassicArc });
    else if (category == Modern) s.templ = d.pick({ arr::TownTheme, arr::ExplorationLoop, arr::ClassicArc });
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
    // Small changes to each instrument's effects.
    s.scaledFx = true;
    for (auto& f : s.fxScale) f = { d.range(0.8f, 1.25f), d.range(0.7f, 1.3f), d.range(0.8f, 1.2f) };
    // A name from the Places.
    s.name = std::string(placeInfo(chosen.front()).characters[std::size_t(s.slots[0].character)]) + " " + placeNames[std::size_t(chosen.front())];
    s.description = "Inspired by World Seed " + std::to_string(seed) + ":";
    for (std::size_t i = 0; i < chosen.size() && i < std::size_t(placeSlots); ++i) s.description += std::string(i == 0 ? " " : ", ") + placeNames[std::size_t(chosen[i])];
    return s;
}

WorldValues worldValues(const Scene& scene, const WorldValues& session) {
    WorldValues v;
    for (int p = 0; p < PCount; ++p) v[std::size_t(p)] = specs[std::size_t(p)].def;
    for (int p : { int(PPower), int(PMaster), int(PQuality), int(PPlayMode), int(PPlaceOuts) }) v[std::size_t(p)] = session[std::size_t(p)];
    v[PSeed] = float(std::clamp<uint32_t>(scene.seed, 1, 9999));
    v[PRoot] = float(std::clamp(scene.root, 0, 11));
    v[PMood] = float(std::clamp(scene.mood, 0, 8));
    v[PPlanet] = float(std::clamp(scene.planet, 0, 6));
    v[PPlanetInfluence] = dsp::clamp01(scene.influence);
    v[PIntensity] = dsp::clamp01(scene.intensity);
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
    return a;
}

} // namespace wb::scenes
