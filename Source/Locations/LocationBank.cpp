#include "LocationBank.h"
#include "../FxDefaults.h"
#include "../Patterns/PatternStore.h"

namespace wb {

static_assert(locationSlots == placeSlots, "a Location holds every Place slot");

LocationBank::LocationBank(Travel& t) : travel(t) {
    for (int i = 0; i < locationCount; ++i) {
        auto& slot = slots[std::size_t(i)];
        slot.name = defaultName(i);
        slot.colour = i;
        publish(i);
    }
}

Location LocationBank::get(int index) const {
    const juce::ScopedLock sl(lock);
    return slots[std::size_t(std::clamp(index, 0, locationCount - 1))];
}

bool LocationBank::used(int index) const {
    const juce::ScopedLock sl(lock);
    return index >= 0 && index < locationCount && slots[std::size_t(index)].used;
}

juce::String LocationBank::name(int index) const {
    const juce::ScopedLock sl(lock);
    return slots[std::size_t(std::clamp(index, 0, locationCount - 1))].name;
}

uint32_t LocationBank::colour(int index) const {
    const juce::ScopedLock sl(lock);
    return locationColours[std::size_t(std::clamp(slots[std::size_t(std::clamp(index, 0, locationCount - 1))].colour, 0, int(locationColours.size()) - 1))];
}

void LocationBank::save(int index, const WorldValues& values, const std::array<pat::SlotPatterns, locationSlots>& patterns) {
    if (index < 0 || index >= locationCount) return;
    const juce::ScopedLock sl(lock);
    auto& slot = slots[std::size_t(index)];
    slot.used = true;
    slot.values = values;
    slot.patterns = patterns;
    publish(index);
}

void LocationBank::clear(int index) {
    if (index < 0 || index >= locationCount) return;
    const juce::ScopedLock sl(lock);
    auto& slot = slots[std::size_t(index)];
    slot = Location {};
    slot.name = defaultName(index);
    slot.colour = index;
    publish(index);
}

void LocationBank::rename(int index, const juce::String& name) {
    if (index < 0 || index >= locationCount) return;
    const juce::ScopedLock sl(lock);
    const auto trimmed = name.trim().substring(0, 24);
    slots[std::size_t(index)].name = trimmed.isEmpty() ? defaultName(index) : trimmed;
    changes.fetch_add(1, std::memory_order_relaxed);
}

void LocationBank::setColour(int index, int colour) {
    if (index < 0 || index >= locationCount) return;
    const juce::ScopedLock sl(lock);
    slots[std::size_t(index)].colour = std::clamp(colour, 0, int(locationColours.size()) - 1);
    changes.fetch_add(1, std::memory_order_relaxed);
}

void LocationBank::publish(int index) {
    const auto& slot = slots[std::size_t(index)];
    auto& data = travel.locationsIn[std::size_t(index)].write();
    data.used = slot.used;
    data.values = slot.values;
    data.patterns = slot.patterns;
    travel.locationsIn[std::size_t(index)].publish();
    changes.fetch_add(1, std::memory_order_relaxed);
}

//==============================================================================
// State: <Locations><Location index name colour><Values temperature="0.5" .../>
// <Patterns>...</Patterns></Location></Locations>. Only used slots carry a world.

juce::ValueTree LocationBank::toValueTree() const {
    juce::ValueTree tree("Locations");
    const juce::ScopedLock sl(lock);
    for (int i = 0; i < locationCount; ++i) {
        const auto& slot = slots[std::size_t(i)];
        if (!slot.used && slot.name == defaultName(i) && slot.colour == i) continue;
        juce::ValueTree t("Location");
        t.setProperty("index", i, nullptr);
        t.setProperty("name", slot.name, nullptr);
        t.setProperty("colour", slot.colour, nullptr);
        t.setProperty("used", slot.used, nullptr);
        if (slot.used) {
            juce::ValueTree values("Values");
            for (int p = 0; p < PCount; ++p)
                if (moveOf(p) != Move::Live) values.setProperty(juce::Identifier(specs[std::size_t(p)].id), double(slot.values[std::size_t(p)]), nullptr);
            t.appendChild(values, nullptr);
            t.appendChild(PatternStore::toTree(slot.patterns), nullptr);
        }
        tree.appendChild(t, nullptr);
    }
    return tree;
}

void LocationBank::fromValueTree(const juce::ValueTree& tree) {
    std::array<Location, locationCount> loaded;
    for (int i = 0; i < locationCount; ++i) {
        loaded[std::size_t(i)].name = defaultName(i);
        loaded[std::size_t(i)].colour = i;
    }
    if (tree.isValid())
        for (const auto& t : tree) {
            const int i = t.getProperty("index", -1);
            if (!t.hasType("Location") || i < 0 || i >= locationCount) continue;
            auto& slot = loaded[std::size_t(i)];
            const auto name = t.getProperty("name").toString().trim().substring(0, 24);
            slot.name = name.isEmpty() ? defaultName(i) : name;
            slot.colour = std::clamp(int(t.getProperty("colour", i)), 0, int(locationColours.size()) - 1);
            const auto values = t.getChildWithName("Values");
            slot.used = bool(t.getProperty("used", false)) && values.isValid();
            if (!slot.used) continue;
            // Every parameter at its default first; the stored ones over it, clamped.
            for (int p = 0; p < PCount; ++p) {
                const auto& spec = specs[std::size_t(p)];
                float v = spec.def;
                if (const auto* stored = values.getPropertyPointer(juce::Identifier(spec.id)))
                    if (const float x = float(double(*stored)); std::isfinite(x)) v = std::clamp(x, spec.min, spec.max);
                slot.values[std::size_t(p)] = v;
            }
            // An instrument effect stored by no version yet takes its Place's default.
            for (int s = 0; s < locationSlots; ++s) {
                const int type = slotTypeIn(slot.values, s);
                for (int e = 0; e < placeElements; ++e)
                    for (int f = 0; f < FxFieldCount; ++f) {
                        const int p = placeParam(s, fxField(e, f));
                        if (!values.hasProperty(juce::Identifier(specs[std::size_t(p)].id))) slot.values[std::size_t(p)] = instrumentFxDefault(type, e, f);
                    }
            }
            std::array<int, placeSlots> types {};
            for (int s = 0; s < locationSlots; ++s) types[std::size_t(s)] = slotTypeIn(slot.values, s);
            slot.patterns = PatternStore::fromTree(t.getChildWithName("Patterns"), types);
        }
    const juce::ScopedLock sl(lock);
    slots = loaded;
    for (int i = 0; i < locationCount; ++i) publish(i);
}

} // namespace wb
