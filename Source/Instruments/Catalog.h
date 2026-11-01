#pragma once
// Instrument mode: every WORLDBUILDER instrument as a normal software instrument.
//
// The catalog lists every instrument of every Place (each Place's six elements), in
// Place order, with a type group (Plucks, Pads, Bass...) for the browser. An instrument
// is named as the scene MIDI names its track: "Tavern - Lute Strum" (the browser shows
// "Tavern -> Lute Strum").
#include "../Parameters.h"
#include "../Places/Place.h"
#include <array>
#include <cstring>

namespace wb::inst {

// The browser's type groups. Keys (pianos, harpsichords, squeezeboxes) are an eleventh
// group beside the ten the brief named.
enum Group { Plucks, Pads, Bass, Leads, Keys, Percussion, BellsMallets, Strings, Voices, Textures, Fx, GroupCount };
inline constexpr std::array<const char*, GroupCount> groupNames { "Plucks", "Pads", "Bass", "Leads", "Keys", "Percussion", "Bells & Mallets",
                                                                  "Strings", "Choirs & Voices", "Textures", "FX" };

struct Id {
    int type = NoPlace, element = 0;
    bool valid() const { return type > NoPlace && type < PlaceTypeCount && element >= 0 && element < placeElements && patternInfo(type, element).kind != NoPattern; }
    bool operator==(const Id& o) const { return type == o.type && element == o.element; }
    bool operator!=(const Id& o) const { return !(*this == o); }
};

inline bool nameHas(const char* name, const char* part) { return name != nullptr && std::strstr(name, part) != nullptr; }

// An instrument's type group, from its kind of pattern, its icon and its name.
inline int groupOf(int type, int element) {
    if (type <= NoPlace || type >= PlaceTypeCount) return Textures;
    const auto& e = placeInfo(type).element[std::size_t(std::clamp(element, 0, placeElements - 1))];
    const char* n = e.name;
    // Names first: they say what the part is better than its glyph.
    if (nameHas(n, "Bass")) return Bass;
    if (nameHas(n, "Lead") || nameHas(n, "Beacon")) return Leads;
    if (nameHas(n, "Choir") || nameHas(n, "Chant") || nameHas(n, "Hymn") || nameHas(n, "Formants") || nameHas(n, "Crowd") || nameHas(n, "Chatter")) return Voices;
    if (nameHas(n, "Pad")) return Pads;
    if (nameHas(n, "Hurdy") || nameHas(n, "Fiddle") || nameHas(n, "Violin")) return Strings;
    if (e.kind == Kit) {
        switch (e.icon) {
            case IconRiser: case IconSteam: case IconSwarm: return Fx;
            default: return Percussion;
        }
    }
    switch (e.icon) {
        case IconLute: case IconHarp: case IconDrip: return Plucks;
        case IconPad: return Pads;
        case IconDrone: return e.sustained ? Pads : Strings;
        case IconBass: return Bass;
        case IconPulse: case IconFlute: case IconHorn: return Leads;
        case IconPiano: return Keys;
        case IconBell: case IconGong: case IconBowl: case IconGlass: case IconJingle: case IconMusicBox: case IconMarimba: case IconAnvil: case IconCrystal:
            return BellsMallets;
        case IconStrings: return Strings;
        case IconChoir: case IconCrowd: return Voices;
        case IconEngine: case IconWind: case IconWave: case IconSwarm: case IconSteam: case IconGear: return Textures;
        case IconTimpani: case IconHandDrum: case IconSnare: case IconShaker: case IconPiston: return Percussion;
        default: return Fx;   // risers, sirens, alarms, PA tones, blips, radio, birds
    }
}

// Whether an instrument has a built-in drone (the Drone control acts on it): the
// hurdy-gurdy's drone strings, the brass drone's and the squeezebox's pedal.
inline bool hasDrone(int type, int element) {
    return (type == Village && element == 1) || (type == Castle && element == 0) || (type == Tavern && element == 4);
}

// Every instrument, in Place order (each Place's six), built once.
struct Catalog {
    static constexpr int maxEntries = PlaceTypeCount * placeElements;
    std::array<Id, maxEntries> entry {};
    int count = 0;
    static const Catalog& get() {
        static const Catalog c = [] {
            Catalog x;
            for (int t = NoPlace + 1; t < PlaceTypeCount; ++t)
                for (int e = 0; e < placeElements; ++e)
                    if (Id { t, e }.valid()) x.entry[std::size_t(x.count++)] = { t, e };
            return x;
        }();
        return c;
    }
    int indexOf(Id id) const {
        for (int i = 0; i < count; ++i)
            if (entry[std::size_t(i)] == id) return i;
        return -1;
    }
    // The next (step +1) or previous (-1) instrument, wrapping.
    Id step(Id from, int by) const {
        if (count <= 0) return from;
        const int at = std::max(0, indexOf(from));
        return entry[std::size_t(((at + by) % count + count) % count)];
    }
};

// The default instrument of a new instance in Instrument mode.
inline constexpr Id defaultInstrument { Tavern, 0 };
static_assert(int(Tavern) == defaultInstrumentType, "the parameter defaults are the default instrument's");

} // namespace wb::inst
