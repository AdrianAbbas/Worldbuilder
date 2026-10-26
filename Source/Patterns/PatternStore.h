#pragma once
// The patterns as the composer edits them (message thread): one SlotPatterns per
// Place slot. Every change is published to the audio thread through the engine's
// lock-free buffers; patterns the Places generate come back the same way. Patterns
// save with the plugin state as compact text (a few bytes per note).
#include "../WorldEngine.h"
#include <juce_data_structures/juce_data_structures.h>

namespace wb {

class PatternStore {
public:
    explicit PatternStore(WorldEngine& e) : engine(e) {}

    pat::SlotPatterns get(int slot) const;
    // Replaces a slot's patterns and publishes them.
    void set(int slot, const pat::SlotPatterns& patterns);
    // One element's controls and pattern (defaults when the slot holds another Place).
    pat::Element element(int slot, int type, int e) const;
    void setElement(int slot, int type, int e, const pat::Element& element);

    // A slot that now holds another Place starts from its defaults.
    void syncTypes(const std::array<int, placeSlots>& types);
    // The newest patterns the audio thread generated (message thread only).
    const pat::Generated& generated(int slot);
    // The pattern element e of a slot plays in a variant (A, B, C, Fill): the edited one,
    // or the generated one.
    pat::Pattern playing(int slot, int type, int e, int variant = 0);

    juce::ValueTree toValueTree() const;
    void fromValueTree(const juce::ValueTree& tree, const std::array<int, placeSlots>& types);
    // The same for any four slots' patterns (Locations keep their own).
    static juce::ValueTree toTree(const std::array<pat::SlotPatterns, placeSlots>& slots);
    static std::array<pat::SlotPatterns, placeSlots> fromTree(const juce::ValueTree& tree, const std::array<int, placeSlots>& types);
    static pat::SlotPatterns defaultsFor(int type);

    static juce::String encodeNotes(const pat::Pattern& p);
    static void writePattern(juce::ValueTree& x, const pat::Pattern& p);
    static void readPattern(const juce::ValueTree& x, pat::Pattern& p);
    static void decodeNotes(const juce::String& text, pat::Pattern& p);

private:
    void publish(int slot);   // with the lock held

    WorldEngine& engine;
    juce::CriticalSection lock;
    std::array<pat::SlotPatterns, placeSlots> model {};
    std::array<pat::Generated, placeSlots> latest {};
};

} // namespace wb
