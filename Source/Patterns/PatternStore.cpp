#include "PatternStore.h"

namespace wb {

pat::SlotPatterns PatternStore::defaultsFor(int type) {
    pat::SlotPatterns p;
    p.type = type;
    for (int e = 0; e < placeElements; ++e) p.element[std::size_t(e)].controls = defaultControls(type, e);
    return p;
}

pat::SlotPatterns PatternStore::get(int slot) const {
    const juce::ScopedLock sl(lock);
    return model[std::size_t(std::clamp(slot, 0, placeSlots - 1))];
}

void PatternStore::set(int slot, const pat::SlotPatterns& patterns) {
    if (slot < 0 || slot >= placeSlots) return;
    const juce::ScopedLock sl(lock);
    const uint32_t revision = model[std::size_t(slot)].revision;
    model[std::size_t(slot)] = patterns;
    model[std::size_t(slot)].revision = revision + 1;
    publish(slot);
}

pat::Element PatternStore::element(int slot, int type, int e) const {
    const auto p = get(slot);
    if (p.type != type) return defaultsFor(type).element[std::size_t(std::clamp(e, 0, placeElements - 1))];
    return p.element[std::size_t(std::clamp(e, 0, placeElements - 1))];
}

void PatternStore::setElement(int slot, int type, int e, const pat::Element& element) {
    if (slot < 0 || slot >= placeSlots || e < 0 || e >= placeElements) return;
    const juce::ScopedLock sl(lock);
    auto& m = model[std::size_t(slot)];
    if (m.type != type) {
        const auto revision = m.revision;
        m = defaultsFor(type);
        m.revision = revision;
    }
    m.element[std::size_t(e)] = element;
    ++m.revision;
    publish(slot);
}

void PatternStore::publish(int slot) {
    auto& buffer = engine.patternsIn[std::size_t(slot)];
    buffer.write() = model[std::size_t(slot)];
    buffer.publish();
}

void PatternStore::syncTypes(const std::array<int, placeSlots>& types) {
    const juce::ScopedLock sl(lock);
    for (int slot = 0; slot < placeSlots; ++slot) {
        auto& m = model[std::size_t(slot)];
        if (m.type == types[std::size_t(slot)]) continue;
        const auto revision = m.revision;
        m = defaultsFor(types[std::size_t(slot)]);
        m.revision = revision + 1;
        publish(slot);
    }
}

const pat::Generated& PatternStore::generated(int slot) {
    const auto k = std::size_t(std::clamp(slot, 0, placeSlots - 1));
    if (engine.patternsOut[k].update()) latest[k] = engine.patternsOut[k].read();
    return latest[k];
}

pat::Pattern PatternStore::playing(int slot, int type, int e) {
    const auto el = element(slot, type, e);
    if (el.controls.edited) return el.pattern;
    const auto& g = generated(slot);
    if (g.type == type) return g.pattern[std::size_t(std::clamp(e, 0, placeElements - 1))];
    return {};
}

//==============================================================================
// State: <Patterns><Slot index type><Element index randomness ... notes="..."/></Slot></Patterns>
// Notes are "step:length:degree:alter:velocity:probability:flags" separated by spaces.

juce::String PatternStore::encodeNotes(const pat::Pattern& p) {
    juce::String text;
    text.preallocateBytes(size_t(p.count) * 18);
    for (int i = 0; i < p.count; ++i) {
        const auto& n = p.notes[std::size_t(i)];
        if (i > 0) text << " ";
        text << int(n.step) << ":" << int(n.length) << ":" << int(n.degree) << ":" << int(n.alter) << ":" << int(n.velocity) << ":"
             << int(n.probability) << ":" << int(n.flags);
    }
    return text;
}

void PatternStore::decodeNotes(const juce::String& text, pat::Pattern& p) {
    p.clear();
    for (const auto& token : juce::StringArray::fromTokens(text, " ", "")) {
        const auto f = juce::StringArray::fromTokens(token, ":", "");
        if (f.size() < 5) continue;
        pat::Note n;
        n.step = int16_t(juce::jlimit(0, pat::maxSteps - 1, f[0].getIntValue()));
        n.length = int16_t(juce::jlimit(1, pat::maxSteps, f[1].getIntValue()));
        n.degree = int8_t(juce::jlimit(-60, 60, f[2].getIntValue()));
        n.alter = int8_t(juce::jlimit(-6, 6, f[3].getIntValue()));
        n.velocity = uint8_t(juce::jlimit(1, 127, f[4].getIntValue()));
        n.probability = uint8_t(f.size() > 5 ? juce::jlimit(0, 100, f[5].getIntValue()) : 100);
        n.flags = uint8_t(f.size() > 6 ? juce::jlimit(0, 255, f[6].getIntValue()) : 0);
        p.add(n);
    }
}

juce::ValueTree PatternStore::toValueTree() const {
    juce::ValueTree tree("Patterns");
    const juce::ScopedLock sl(lock);
    for (int slot = 0; slot < placeSlots; ++slot) {
        const auto& m = model[std::size_t(slot)];
        if (m.type == NoPlace) continue;
        juce::ValueTree s("Slot");
        s.setProperty("index", slot, nullptr);
        s.setProperty("type", m.type, nullptr);
        for (int e = 0; e < placeElements; ++e) {
            const auto& el = m.element[std::size_t(e)];
            // Untouched elements are left out: the Place regenerates them from its seed.
            if (!el.controls.edited && el.controls == defaultControls(m.type, e)) continue;
            juce::ValueTree x("Element");
            x.setProperty("index", e, nullptr);
            x.setProperty("randomness", el.controls.randomness, nullptr);
            x.setProperty("vary", int(el.controls.vary), nullptr);
            x.setProperty("evolve", el.controls.evolve, nullptr);
            x.setProperty("return", int(el.controls.returnBars), nullptr);
            x.setProperty("variant", int(el.controls.variant), nullptr);
            if (el.controls.edited) {
                const auto& p = el.pattern;
                x.setProperty("resolution", int(p.resolution), nullptr);
                x.setProperty("bars", int(p.bars), nullptr);
                x.setProperty("steps", int(p.lengthSteps), nullptr);
                x.setProperty("follow", p.followChords, nullptr);
                x.setProperty("base", int(p.base), nullptr);
                x.setProperty("swing", p.swing, nullptr);
                x.setProperty("notes", encodeNotes(p), nullptr);
            }
            s.appendChild(x, nullptr);
        }
        tree.appendChild(s, nullptr);
    }
    return tree;
}

void PatternStore::fromValueTree(const juce::ValueTree& tree, const std::array<int, placeSlots>& types) {
    std::array<pat::SlotPatterns, placeSlots> loaded;
    for (int slot = 0; slot < placeSlots; ++slot) loaded[std::size_t(slot)] = defaultsFor(types[std::size_t(slot)]);
    if (tree.isValid())
        for (const auto& s : tree) {
            if (!s.hasType("Slot")) continue;
            const int slot = s.getProperty("index", -1);
            if (slot < 0 || slot >= placeSlots || int(s.getProperty("type", -1)) != types[std::size_t(slot)]) continue;
            for (const auto& x : s) {
                const int e = x.getProperty("index", -1);
                if (!x.hasType("Element") || e < 0 || e >= placeElements) continue;
                auto& el = loaded[std::size_t(slot)].element[std::size_t(e)];
                const auto number = [&](const char* id, float fallback, float lo, float hi) {
                    const float v = float(double(x.getProperty(id, fallback)));
                    return std::isfinite(v) ? juce::jlimit(lo, hi, v) : fallback;
                };
                el.controls.randomness = number("randomness", el.controls.randomness, 0.0f, 1.0f);
                el.controls.vary = uint8_t(int(number("vary", float(el.controls.vary), 0.0f, 31.0f)));
                el.controls.evolve = number("evolve", 0.0f, 0.0f, 1.0f);
                el.controls.returnBars = uint8_t(int(number("return", 0.0f, 0.0f, 64.0f)));
                el.controls.variant = uint8_t(int(number("variant", 0.0f, 0.0f, 255.0f)));
                if (x.hasProperty("notes")) {
                    auto& p = el.pattern;
                    const int bars = int(number("bars", 4.0f, 0.0f, float(pat::maxBars)));
                    p.setGrid(int(number("resolution", float(pat::Sixteenth), 0.0f, float(pat::ResolutionCount - 1))), bars,
                              int(number("steps", 16.0f, 1.0f, float(pat::maxSteps))));
                    p.followChords = bool(x.getProperty("follow", true));
                    p.base = int8_t(int(number("base", 0.0f, -36.0f, 36.0f)));
                    p.swing = number("swing", 0.0f, 0.0f, 1.0f);
                    decodeNotes(x.getProperty("notes").toString(), p);
                    p.normalise();
                    el.controls.edited = true;
                }
            }
        }
    const juce::ScopedLock sl(lock);
    for (int slot = 0; slot < placeSlots; ++slot) {
        const auto revision = model[std::size_t(slot)].revision;
        model[std::size_t(slot)] = loaded[std::size_t(slot)];
        model[std::size_t(slot)].revision = revision + 1;
        publish(slot);
    }
}

} // namespace wb
