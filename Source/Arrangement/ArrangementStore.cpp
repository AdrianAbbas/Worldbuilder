#include "ArrangementStore.h"
#include "../DSP/Planet.h"
#include "../Patterns/PatternStore.h"

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
    auto& s = a.sections[std::size_t(index)];
    s.bars = uint8_t(std::clamp(bars, minBars, maxBars));
    // The section's own clips follow its length. During a drag (after its first, undoable
    // step) they are fitted from the clips as the drag found them, so shrinking and
    // growing again gives the notes back.
    const Arrangement& from = !undoable && !undoStack.empty() && undoStack.back().count == model.count ? undoStack.back() : model;
    for (int slot = 0; slot < placeSlots; ++slot)
        for (int e = 0; e < placeElements; ++e) {
            auto& cell = s.cells[std::size_t(slot)][std::size_t(e)];
            if (cell.clip == nullptr) continue;
            const auto& original = from.sections[std::size_t(index)].cells[std::size_t(slot)][std::size_t(e)].clip;
            cell.clip = conform(original != nullptr ? original : cell.clip, s.bars);
        }
    if (a == model && !undoable) return;
    apply(a, undoable);
}

void ArrangementStore::setClip(int section, int slot, int element, ClipRef clip, bool undoable) {
    const juce::ScopedLock sl(lock);
    if (section < 0 || section >= model.count || slot < 0 || slot >= placeSlots || element < 0 || element >= placeElements) return;
    auto a = model;
    auto& cell = a.sections[std::size_t(section)].cells[std::size_t(slot)][std::size_t(element)];
    if (clip != nullptr) clip = conform(clip, a.sections[std::size_t(section)].bars);
    cell.clip = std::move(clip);
    apply(a, undoable);
}

ClipRef ArrangementStore::clipOf(int section, int slot, int element) const {
    const juce::ScopedLock sl(lock);
    if (section < 0 || section >= model.count || slot < 0 || slot >= placeSlots || element < 0 || element >= placeElements) return {};
    return model.sections[std::size_t(section)].cells[std::size_t(slot)][std::size_t(element)].clip;
}

ClipRef ArrangementStore::fromPattern(const pat::Pattern& p, int startBar, int bars, bool ownSwing) {
    auto out = std::make_shared<pat::Clip>();
    out->followChords = p.followChords;
    out->base = p.base;
    out->swing = p.swing;
    out->ownSwing = ownSwing;
    out->setLength(p.resolution, std::clamp(bars, 1, pat::maxClipBars));
    out->clear();
    const int64_t length = p.lengthSteps;
    if (length <= 0 || p.count <= 0) return out;
    const int64_t from = int64_t(std::max(startBar, 0)) * pat::stepsPerBar(p.resolution), to = from + out->lengthSteps;
    for (int64_t pass = from / length; pass * length < to; ++pass)
        for (int i = 0; i < p.count; ++i) {
            auto n = p.notes[std::size_t(i)];
            const int64_t at = pass * length + n.step;
            if (at < from || n.inTake()) continue;
            if (at >= to) break;
            n.step = int16_t(at - from);
            out->add(n);
        }
    return out;
}

ClipRef ArrangementStore::conform(const ClipRef& clip, int bars) {
    if (clip == nullptr) return clip;
    bars = std::clamp(bars, 1, pat::maxClipBars);
    const int perBar = pat::stepsPerBar(clip->resolution);
    const int want = bars * perBar;
    if (clip->lengthSteps == want && clip->bars == bars) return clip;
    auto out = std::make_shared<pat::Clip>(*clip);
    out->setLength(clip->resolution, bars);
    out->clear();
    const int length = std::max<int>(clip->lengthSteps, 1);
    for (int pass = 0; pass * length < want; ++pass)
        for (int i = 0; i < clip->count; ++i) {
            auto n = clip->notes[std::size_t(i)];
            const int step = pass * length + n.step;
            if (step >= want) break;
            n.step = int16_t(step);
            out->add(n);
        }
    return out;
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

void ArrangementStore::writeClip(juce::ValueTree& x, const pat::Clip& c) {
    PatternStore::writePattern(x, c);
    if (c.ownSwing) x.setProperty("ownSwing", true, nullptr);
}

ClipRef ArrangementStore::readClip(const juce::ValueTree& x, int bars) {
    auto c = std::make_shared<pat::Clip>();
    PatternStore::readPattern(x, *c);
    c->ownSwing = bool(x.getProperty("ownSwing", false));
    // Always a whole number of bars, its section's.
    return conform(c, bars);
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
        // Milestone 11b: written only when set, so older readers and files stay as they were.
        if (s.transition != TransCut) {
            x.setProperty("transition", int(s.transition), nullptr);
            x.setProperty("transitionLength", int(s.transitionLength), nullptr);
        }
        if (s.location >= 0) x.setProperty("location", int(s.location), nullptr);
        if (s.planet >= 0) x.setProperty("planet", int(s.planet), nullptr);
        juce::StringArray world;
        bool overrides = false;
        for (auto w : s.world) {
            world.add(juce::String(int(w)));
            overrides |= w >= 0;
        }
        if (overrides) x.setProperty("world", world.joinIntoString(","), nullptr);
        // Milestone 16: the section's own clips (linked cells have none).
        for (int slot = 0; slot < placeSlots; ++slot)
            for (int e = 0; e < placeElements; ++e)
                if (const auto& clip = s.cells[std::size_t(slot)][std::size_t(e)].clip) {
                    juce::ValueTree cx("Clip");
                    cx.setProperty("slot", slot, nullptr);
                    cx.setProperty("element", e, nullptr);
                    writeClip(cx, *clip);
                    x.appendChild(cx, nullptr);
                }
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
        s.transition = uint8_t(int(number(x, "transition", 0, 0, float(TransitionCount - 1))));
        s.transitionLength = uint8_t(int(number(x, "transitionLength", 1, 1, float(maxTransitionLength))));
        s.location = int8_t(int(number(x, "location", -1, -1, 7)));
        s.planet = int8_t(int(number(x, "planet", -1, -1, float(dsp::PlanetTypeCount - 1))));
        const auto world = juce::StringArray::fromTokens(x.getProperty("world").toString(), ",", "");
        for (int w = 0; w < world.size() && w < WorldControlCount; ++w) s.world[std::size_t(w)] = int8_t(juce::jlimit(-1, 100, world[w].getIntValue()));
        for (const auto& cx : x) {
            if (!cx.hasType("Clip")) continue;
            const int slot = cx.getProperty("slot", -1), e = cx.getProperty("element", -1);
            if (slot < 0 || slot >= placeSlots || e < 0 || e >= placeElements) continue;
            s.cells[std::size_t(slot)][std::size_t(e)].clip = readClip(cx, s.bars);
        }
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
