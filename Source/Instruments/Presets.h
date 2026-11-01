#pragma once
// Instrument presets (Instrument mode): an instrument, its Character, effects and
// controls. Separate from world presets (.world files and scenes): a preset never touches
// the world, and loading a world never touches the instrument.
//
// A preset stores the instrument and the inst_* parameters it sets; anything it does not
// set takes the instrument's own default. The composer's are .wbinst files (XML) in
// Documents/WORLDBUILDER/Instrument Presets; the factory set is below.
#include "Catalog.h"
#include <juce_core/juce_core.h>
#include <vector>

namespace wb::inst {

struct Preset {
    juce::String name;
    Id id {};
    int character = 0;
    bool factory = false;
    std::vector<std::pair<int, float>> values;   // parameter index (PInst...), value
    juce::File file;                             // the composer's preset file
};

namespace detail {
inline Preset preset(const char* name, int type, int element, int character, std::vector<std::pair<int, float>> values) {
    Preset p;
    p.name = name;
    p.id = { type, element };
    p.character = character;
    p.factory = true;
    p.values = std::move(values);
    return p;
}
} // namespace detail

// The factory set: the instruments that hold up best on their own (the developer's
// recordings first, then the most convincing models), each with its effects set for
// playing alone. Element indices are each Place's order (Places.h).
inline const std::vector<Preset>& factoryPresets() {
    using detail::preset;
    static const std::vector<Preset> list {
        // Recorded instruments (Milestone 15).
        preset("Gut-String Lute", Tavern, 0, 1, { { PInstReverb, 0.22f }, { PInstReverbType, RevRoom }, { PInstTone, 0.55f } }),
        preset("Tavern Fiddle", Tavern, 2, 0, { { PInstReverb, 0.28f }, { PInstReverbType, RevHall } }),
        preset("Desert Oud", Desert, 0, 0, { { PInstReverb, 0.3f }, { PInstReverbType, RevHall }, { PInstDelay, 0.14f }, { PInstDelayType, DlyDotted } }),
        preset("Forest Kalimba", Forest, 3, 0, { { PInstReverb, 0.3f }, { PInstReverbType, RevPlate }, { PInstDelay, 0.15f }, { PInstDelayType, DlyPingPong } }),
        preset("Ghost Violin", Ruins, 4, 1, { { PInstReverb, 0.45f }, { PInstReverbType, RevCathedral } }),
        preset("Forest Strings", Forest, 4, 0, { { PInstReverb, 0.35f }, { PInstReverbType, RevHall }, { PInstAttack, 0.45f } }),
        preset("Neon Pulse", Spaceport, 0, 0, { { PInstReverb, 0.2f }, { PInstReverbType, RevPlate }, { PInstDelay, 0.3f }, { PInstDelayType, DlyPingPong } }),
        preset("Soft Analog Pad", Nebula, 0, 0, { { PInstReverb, 0.4f }, { PInstReverbType, RevHall }, { PInstWidth, 1.4f } }),
        preset("Biodome Pad", Biodome, 0, 0, { { PInstReverb, 0.35f }, { PInstReverbType, RevPlate }, { PInstFx, ChChorus }, { PInstFxAmount, 0.3f } }),
        preset("Static Bass", Megacity, 0, 0, { { PInstReverb, 0.08f }, { PInstReverbType, RevRoom }, { PInstDrive, 0.2f } }),
        preset("Cyberpunk Stabs", Megacity, 4, 0, { { PInstReverb, 0.25f }, { PInstReverbType, RevPlate }, { PInstDelay, 0.25f }, { PInstDelayType, DlyDotted } }),
        preset("Reactor Bass", SpaceStation, 4, 0, { { PInstReverb, 0.1f }, { PInstReverbType, RevRoom } }),
        preset("Harsh Square Lead", Spaceport, 5, 0, { { PInstReverb, 0.2f }, { PInstReverbType, RevPlate }, { PInstDelay, 0.22f }, { PInstDelayType, DlyPingPong } }),
        preset("Warp Core Pulse", Bridge, 1, 0, { { PInstReverb, 0.3f }, { PInstReverbType, RevHall } }),
        // Models.
        preset("Cathedral Choir", Temple, 0, 0, { { PInstReverb, 0.5f }, { PInstReverbType, RevCathedral } }),
        preset("Castle Bells", Castle, 1, 0, { { PInstReverb, 0.4f }, { PInstReverbType, RevHall } }),
        preset("Singing Bowls", Temple, 2, 0, { { PInstReverb, 0.45f }, { PInstReverbType, RevCathedral } }),
        preset("Harpsichord", Palace, 0, 0, { { PInstReverb, 0.3f }, { PInstReverbType, RevRoom } }),
        preset("Palace Strings", Palace, 1, 0, { { PInstReverb, 0.38f }, { PInstReverbType, RevHall } }),
        preset("Velvet E-Piano", City, 2, 1, { { PInstReverb, 0.25f }, { PInstReverbType, RevPlate }, { PInstFx, ChChorus }, { PInstFxAmount, 0.3f } }),
        preset("Vibraphone", Mall, 1, 0, { { PInstReverb, 0.3f }, { PInstReverbType, RevPlate } }),
        preset("Crystal Cave", Cavern, 4, 0, { { PInstReverb, 0.5f }, { PInstReverbType, RevCave } }),
        preset("Star Bells", Nebula, 2, 0, { { PInstReverb, 0.5f }, { PInstReverbType, RevCathedral }, { PInstDelay, 0.25f }, { PInstDelayType, DlyPingPong } }),
        preset("Squeezebox", Tavern, 4, 0, { { PInstReverb, 0.2f }, { PInstReverbType, RevRoom } }),
        preset("Bugle Call", Military, 1, 0, { { PInstReverb, 0.3f }, { PInstReverbType, RevHall } }),
        preset("Steel Drum", Beach, 1, 0, { { PInstReverb, 0.22f }, { PInstReverbType, RevPlate } }),
        preset("War Timpani", Castle, 3, 0, { { PInstReverb, 0.35f }, { PInstReverbType, RevHall } }),
        preset("Drum Machine", Megacity, 3, 0, { { PInstReverb, 0.12f }, { PInstReverbType, RevRoom }, { PInstDrive, 0.15f } }),
    };
    return list;
}

} // namespace wb::inst
