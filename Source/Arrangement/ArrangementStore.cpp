#include "ArrangementStore.h"

namespace wb {

using namespace arr;

arr::Arrangement ArrangementStore::get() const {
    const juce::ScopedLock sl(lock);
    return model;
}

uint32_t ArrangementStore::revision() const {
    const juce::ScopedLock sl(lock);
    return changes;
}

void ArrangementStore::publish() {
    model.revision = ++changes;
    out.write() = model;
    out.publish();
}

void ArrangementStore::apply(const Arrangement& a, bool undoable) {
    if (undoable) {
        undoStack.push_back(model);
        if (undoStack.size() > undoLimit) undoStack.erase(undoStack.begin());
        redoStack.clear();
    }
    model = a;
    model.count = std::clamp(model.count, 0, maxSections);
    model.end = uint8_t(std::min<int>(model.end, EndCount - 1));
    model.loopTo = uint8_t(std::clamp<int>(model.loopTo, 0, std::max(model.count - 1, 0)));
    publish();
}

void ArrangementStore::set(const Arrangement& a, bool undoable) {
    const juce::ScopedLock sl(lock);
    apply(a, undoable);
}

bool ArrangementStore::undo() {
    const juce::ScopedLock sl(lock);
    if (undoStack.empty()) return false;
    redoStack.push_back(model);
    model = undoStack.back();
    undoStack.pop_back();
    publish();
    return true;
}

bool ArrangementStore::redo() {
    const juce::ScopedLock sl(lock);
    if (redoStack.empty()) return false;
    undoStack.push_back(model);
    model = redoStack.back();
    redoStack.pop_back();
    publish();
    return true;
}

bool ArrangementStore::canUndo() const {
    const juce::ScopedLock sl(lock);
    return !undoStack.empty();
}

bool ArrangementStore::canRedo() const {
    const juce::ScopedLock sl(lock);
    return !redoStack.empty();
}

int ArrangementStore::add(const Section& s, int at) {
    const juce::ScopedLock sl(lock);
    if (model.count >= maxSections) return std::min(at < 0 ? model.count - 1 : at, model.count - 1);
    auto a = model;
    const int index = at < 0 || at > a.count ? a.count : at;
    for (int i = a.count; i > index; --i) a.sections[std::size_t(i)] = a.sections[std::size_t(i - 1)];
    a.sections[std::size_t(index)] = s;
    ++a.count;
    if (a.loopTo >= index && a.count > 1 && index < a.count - 1) ++a.loopTo;
    apply(a, true);
    return index;
}

int ArrangementStore::remove(int index) {
    const juce::ScopedLock sl(lock);
    if (index < 0 || index >= model.count) return -1;
    auto a = model;
    for (int i = index; i + 1 < a.count; ++i) a.sections[std::size_t(i)] = a.sections[std::size_t(i + 1)];
    --a.count;
    a.sections[std::size_t(a.count)] = Section {};
    if (a.loopTo > index) --a.loopTo;
    apply(a, true);
    return std::min(index, a.count - 1);
}

int ArrangementStore::duplicate(int index) {
    Section copy;
    {
        const juce::ScopedLock sl(lock);
        if (index < 0 || index >= model.count) return -1;
        copy = model.sections[std::size_t(index)];
    }
    return add(copy, index + 1);
}

int ArrangementStore::move(int from, int to) {
    const juce::ScopedLock sl(lock);
    if (from < 0 || from >= model.count) return -1;
    to = std::clamp(to, 0, model.count - 1);
    if (to == from) return from;
    auto a = model;
    const Section s = a.sections[std::size_t(from)];
    if (to > from)
        for (int i = from; i < to; ++i) a.sections[std::size_t(i)] = a.sections[std::size_t(i + 1)];
    else
        for (int i = from; i > to; --i) a.sections[std::size_t(i)] = a.sections[std::size_t(i - 1)];
    a.sections[std::size_t(to)] = s;
    // The loop target follows its section.
    const int target = a.loopTo;
    if (target == from) a.loopTo = uint8_t(to);
    else if (from < target && to >= target) a.loopTo = uint8_t(target - 1);
    else if (from > target && to <= target) a.loopTo = uint8_t(target + 1);
    apply(a, true);
    return to;
}

void ArrangementStore::resize(int index, int bars, bool undoable) {
    const juce::ScopedLock sl(lock);
    if (index < 0 || index >= model.count) return;
    auto a = model;
    a.sections[std::size_t(index)].bars = uint8_t(std::clamp(bars, minBars, maxBars));
    if (a == model && !undoable) return;
    apply(a, undoable);
}

//==============================================================================
// State: <Arrangement end loopTo><Section name colour bars repeats curve start end
// cells="..."/></Arrangement>. Each cell is three characters (entry digit, variant
// letter A-C, f for Fill or -), separated by spaces, slot by slot.

juce::String ArrangementStore::encodeCells(const Grid& cells) {
    juce::String text;
    text.preallocateBytes(std::size_t(placeSlots * placeElements * 4));
    for (int slot = 0; slot < placeSlots; ++slot)
        for (int e = 0; e < placeElements; ++e) {
            const auto& c = cells[std::size_t(slot)][std::size_t(e)];
            if (slot + e > 0) text << " ";
            text << juce::String::charToString(juce::juce_wchar('0' + std::min<int>(c.entry, EntryCount - 1)))
                 << juce::String::charToString(juce::juce_wchar('A' + std::min<int>(c.variant, VariantC))) << (c.fill ? "f" : "-");
        }
    return text;
}

void ArrangementStore::decodeCells(const juce::String& text, Grid& cells) {
    const auto tokens = juce::StringArray::fromTokens(text, " ", "");
    for (int i = 0; i < tokens.size() && i < placeSlots * placeElements; ++i) {
        const auto& t = tokens[i];
        if (t.length() < 3) continue;
        auto& c = cells[std::size_t(i / placeElements)][std::size_t(i % placeElements)];
        c.entry = uint8_t(juce::jlimit(0, EntryCount - 1, int(t[0] - '0')));
        c.variant = uint8_t(juce::jlimit(0, int(VariantC), int(t[1] - 'A')));
        c.fill = t[2] == 'f';
    }
}

juce::ValueTree ArrangementStore::toValueTree() const { return toTree(get()); }

juce::ValueTree ArrangementStore::toTree(const Arrangement& a) {
    juce::ValueTree tree("Arrangement");
    tree.setProperty("end", int(a.end), nullptr);
    tree.setProperty("loopTo", int(a.loopTo), nullptr);
    for (int i = 0; i < a.count; ++i) {
        const auto& s = a.sections[std::size_t(i)];
        juce::ValueTree x("Section");
        x.setProperty("name", juce::String::fromUTF8(s.nameText()), nullptr);
        x.setProperty("colour", int(s.colour), nullptr);
        x.setProperty("bars", int(s.bars), nullptr);
        x.setProperty("repeats", int(s.repeats), nullptr);
        x.setProperty("curve", int(s.curve), nullptr);
        x.setProperty("start", s.intensityStart, nullptr);
        x.setProperty("end", s.intensityEnd, nullptr);
        x.setProperty("cells", encodeCells(s.cells), nullptr);
        tree.appendChild(x, nullptr);
    }
    return tree;
}

Arrangement ArrangementStore::fromTree(const juce::ValueTree& tree) {
    Arrangement a;
    if (!tree.isValid()) return a;
    const auto number = [](const juce::ValueTree& x, const char* id, float fallback, float lo, float hi) {
        const float v = float(double(x.getProperty(id, fallback)));
        return std::isfinite(v) ? juce::jlimit(lo, hi, v) : fallback;
    };
    for (const auto& x : tree) {
        if (!x.hasType("Section") || a.count >= maxSections) continue;
        Section s;
        s.setName(x.getProperty("name", "Section").toString().toRawUTF8());
        s.colour = uint8_t(int(number(x, "colour", 0, 0, float(colourCount - 1))));
        s.bars = uint8_t(int(number(x, "bars", 8, float(minBars), float(maxBars))));
        s.repeats = uint8_t(int(number(x, "repeats", 1, 0, 64)));
        s.curve = uint8_t(int(number(x, "curve", 0, 0, float(CurveCount - 1))));
        s.intensityStart = number(x, "start", 0.5f, 0, 1);
        s.intensityEnd = number(x, "end", 0.5f, 0, 1);
        decodeCells(x.getProperty("cells").toString(), s.cells);
        a.sections[std::size_t(a.count++)] = s;
    }
    a.end = uint8_t(int(number(tree, "end", 0, 0, float(EndCount - 1))));
    a.loopTo = uint8_t(int(number(tree, "loopTo", 0, 0, float(std::max(a.count - 1, 0)))));
    return a;
}

void ArrangementStore::fromValueTree(const juce::ValueTree& tree) {
    const auto loaded = fromTree(tree);
    const juce::ScopedLock sl(lock);
    undoStack.clear();
    redoStack.clear();
    apply(loaded, false);
}

} // namespace wb
