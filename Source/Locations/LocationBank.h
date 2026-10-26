#pragma once
// The eight Location slots as the composer keeps them (message thread, or any thread
// the host saves state from): a name, a colour, and a snapshot of the world with its
// patterns. Every change is published to the audio thread (Travel). Locations save
// with the plugin state: the located parameters by ID (so later versions can add
// parameters) and the patterns in the same compact form as the live ones.
#include "Travel.h"
#include <juce_data_structures/juce_data_structures.h>

namespace wb {

struct Location {
    bool used = false;
    juce::String name;
    int colour = 0;   // index into locationColours
    WorldValues values {};
    std::array<pat::SlotPatterns, locationSlots> patterns {};
};

class LocationBank {
public:
    explicit LocationBank(Travel& t);

    Location get(int index) const;
    bool used(int index) const;
    juce::String name(int index) const;
    uint32_t colour(int index) const;   // ARGB
    void save(int index, const WorldValues& values, const std::array<pat::SlotPatterns, locationSlots>& patterns);
    void clear(int index);
    void rename(int index, const juce::String& name);
    void setColour(int index, int colour);
    static juce::String defaultName(int index) { return "Location " + juce::String(index + 1); }
    // Bumped on every change (the editor repaints).
    uint32_t revision() const { return changes.load(std::memory_order_relaxed); }

    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& tree);

private:
    void publish(int index);   // with the lock held

    Travel& travel;
    juce::CriticalSection lock;
    std::array<Location, locationCount> slots;
    std::atomic<uint32_t> changes { 0 };
};

} // namespace wb
