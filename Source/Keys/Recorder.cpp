#include "Recorder.h"

namespace wb::keys {

void Recorder::toDegree(const pat::Pattern& p, int mood, int chord, int pitch, int& degree, int& alter) {
    // The highest degree at or below the pitch; the rest is the alteration.
    int best = -40;
    for (int d = -40; d <= 40; ++d) {
        if (pat::semitonesFor(p.followChords, p.base, mood, chord, d, 0) > pitch) break;
        best = d;
    }
    degree = best;
    alter = pitch - pat::semitonesFor(p.followChords, p.base, mood, chord, best, 0);
}

void Recorder::pushUndo(int s, int t, int e, const pat::Element& el) {
    undoStack.push_back({ s, t, e, el });
    if (undoStack.size() > undoLimit) undoStack.erase(undoStack.begin());
}

bool Recorder::undo() {
    if (open || undoStack.empty()) return false;
    const auto u = undoStack.back();
    undoStack.pop_back();
    if (u.section >= 0) {
        if (arrangement == nullptr) return false;
        arrangement->setClip(u.section, u.slot, u.element, u.clip, true);
        ++changes;
        return true;
    }
    if (store.get(u.slot).type != u.type) return false;   // the slot holds another Place now
    store.setElement(u.slot, u.type, u.element, u.element_);
    ++changes;
    return true;
}

void Recorder::begin(const Captured& c, const RecordSettings& s) {
    open = true;
    isFree = c.free;
    pushedUndo = false;
    slot = c.slot;
    type = c.type;
    element = c.element;
    mode = s.mode;
    variant = s.mode == IntoVariant ? pat::Element::clampVariant(s.variant) : 0;
    grid = s.quantise;
    strength = s.strength;
    punch = s.punch && s.punchOut > s.punchIn && s.mode != Overdub;
    punchIn = s.punchIn;
    punchOut = s.punchOut;
    autoLength = s.autoLength && s.mode != Overdub && !punch;
    kit = c.kit;
    startBeat = lastBeat = c.beat;
    before = store.element(slot, type, element);
    base = store.playing(slot, type, element, variant);
    notes.clear();
    clipSection = arrangement != nullptr ? s.clipSection : -1;
    clipOrigin = s.clipOrigin;
    clipBeats = s.clipBeats;
    clipSpan = s.clipSpan;
    clipUndoPushed = false;
    clipBefore = clipSection >= 0 ? arrangement->clipOf(clipSection, slot, element) : nullptr;
    if (clipSection >= 0 && clipBefore == nullptr) clipSection = -1;   // not unique (the processor makes it so): the pattern
}

void Recorder::rebuildClip(int mood, double until) {
    if (notes.empty() || clipBefore == nullptr) return;
    auto c = std::make_shared<pat::Clip>(*clipBefore);
    const int bars = std::max(1, int(std::lround(clipBeats / 4.0)));
    if (mode == Overdub) {
        // as it was
    } else if (punch) {
        const double perBeat = pat::stepsPerBeat(c->resolution);
        for (int i = c->count - 1; i >= 0; --i) {
            const double at = clipOrigin + double(c->notes[std::size_t(i)].step) / perBeat;
            if (at >= punchIn - 1.0e-9 && at < punchOut - 1.0e-9) c->remove(i);
        }
    } else {
        // A new clip on the finest grid, its section's length, fixed in the key.
        c->setLength(tripletGrid(grid) ? pat::ThirtySecondT : pat::ThirtySecond, bars);
        c->followChords = false;
        c->base = 0;
        c->swing = 0;
        c->ownSwing = true;
        c->clear();
    }
    const double perBeat = pat::stepsPerBeat(c->resolution);
    const double length = c->lengthBeats();
    for (const auto& t : notes) {
        const double at = quantiseBeat(t.on, grid, strength) - (isFree ? 0.0 : clipOrigin);
        // Inside the section only; its repeats fold onto the clip.
        if (at < -1.0e-6 || (clipSpan > 0 && at >= clipSpan - 1.0e-6)) continue;
        const double pos = std::max(0.0, at) - std::floor(std::max(0.0, at) / length) * length;
        auto step = int(std::lround(pos * perBeat));
        if (step >= c->lengthSteps) step -= c->lengthSteps;
        pat::Note n;
        n.step = int16_t(std::clamp(step, 0, int(c->lengthSteps) - 1));
        const double end = t.off >= 0 ? t.off : std::max(t.on, until);
        n.length = int16_t(std::clamp(int(std::lround((end - t.on) * perBeat)), 1, int(c->lengthSteps)));
        n.velocity = t.velocity;
        n.probability = 100;
        n.flags = open ? pat::Note::takeFlag : 0;
        if (kit) {
            n.degree = int8_t(t.pitch);
        } else {
            int degree = 0, alter = 0;
            pat::Pattern p;
            p.followChords = c->followChords;
            p.base = c->base;
            toDegree(p, mood, t.chord, t.pitch, degree, alter);
            n.degree = int8_t(std::clamp(degree, -60, 60));
            n.alter = int8_t(std::clamp(alter, -6, 6));
        }
        c->add(n);
    }
    // One undo step for the whole take (in ARRANGE's undo and in REC's).
    arrangement->setClip(clipSection, slot, element, c, !clipUndoPushed);
    clipUndoPushed = true;
    ++changes;
}

void Recorder::rebuild(int mood, double until) {
    if (notes.empty()) return;
    if (clipSection >= 0) {
        rebuildClip(mood, until);
        return;
    }
    pat::Pattern p;
    if (mode == Overdub) {
        p = base;
    } else if (punch) {
        // Punched in: the pattern stays, less what it had inside the region.
        p = base;
        const double perBeat = pat::stepsPerBeat(p.resolution);
        for (int i = p.count - 1; i >= 0; --i) {
            const double at = double(p.notes[std::size_t(i)].step) / perBeat;
            if (at >= punchIn - 1.0e-9 && at < punchOut - 1.0e-9) p.remove(i);
        }
    } else {
        // A new pattern on the finest grid, as long as the pattern was (or Auto).
        int bars = base.bars > 0 ? int(base.bars) : std::clamp(int(std::ceil(base.lengthBeats() / 4.0 - 1.0e-6)), 1, pat::maxBars);
        if (autoLength) {
            const double from = isFree ? 0.0 : std::floor(startBeat / 4.0 + 1.0e-9) * 4.0;
            double last = from;
            for (const auto& n : notes) last = std::max(last, n.off >= 0 ? n.off : std::max(n.on, until));
            bars = std::clamp(int(std::ceil((last - from) / 4.0 - 1.0e-6)), 1, pat::maxBars);
        }
        p.setGrid(tripletGrid(grid) ? pat::ThirtySecondT : pat::ThirtySecond, bars);
        p.followChords = false;
        p.base = 0;
        p.swing = 0;
        p.clear();
    }
    const double perBeat = pat::stepsPerBeat(p.resolution);
    const double length = p.lengthBeats();
    for (const auto& t : notes) {
        const double at = quantiseBeat(t.on, grid, strength);
        double pos = at - std::floor(at / length) * length;
        auto step = int(std::lround(pos * perBeat));
        if (step >= p.lengthSteps) step -= p.lengthSteps;
        pat::Note n;
        n.step = int16_t(std::clamp(step, 0, int(p.lengthSteps) - 1));
        const double end = t.off >= 0 ? t.off : std::max(t.on, until);
        n.length = int16_t(std::clamp(int(std::lround((end - t.on) * perBeat)), 1, int(p.lengthSteps)));
        n.velocity = t.velocity;
        n.probability = 100;
        n.flags = open ? pat::Note::takeFlag : 0;
        if (kit) {
            n.degree = int8_t(t.pitch);
        } else {
            // Following the chords: the chord where the note was played (a free take: where
            // it lands in the song's first pass of the pattern).
            const int chord = isFree ? music::chordDegree(double(n.step) / perBeat, 0.0f) : t.chord;
            int degree = 0, alter = 0;
            toDegree(p, mood, chord, t.pitch, degree, alter);
            n.degree = int8_t(std::clamp(degree, -60, 60));
            n.alter = int8_t(std::clamp(alter, -6, 6));
        }
        p.add(n);
    }
    auto el = store.element(slot, type, element);
    el.patternOf(variant) = p;
    el.setEdited(variant, true);
    store.setElement(slot, type, element, el);
    ++changes;
}

void Recorder::finish(int mood, double at) {
    for (auto& n : notes)
        if (n.off < 0) n.off = std::max(n.on, at);
    open = false;
    if (notes.empty()) return;   // nothing played: the pattern is as it was
    rebuild(mood, at);           // without the take's marks
    if (!isFree) transportTake = true;
}

bool Recorder::service(const RecordSettings& settings, int mood) {
    bool changed = false;
    Captured c;
    while (capture.out.pop(c)) {
        switch (c.kind) {
            case Captured::TakeStart:
                if (open) finish(mood, lastBeat);
                begin(c, settings);
                break;
            case Captured::TakeEnd:
                if (open) {
                    finish(mood, c.beat);
                    changed = true;
                }
                break;
            case Captured::NoteOn:
                if (!open || c.slot != slot || c.element != element) break;
                if (!pushedUndo) {
                    pushUndo(slot, type, element, before);
                    if (clipSection >= 0) {
                        undoStack.back().section = clipSection;
                        undoStack.back().clip = clipBefore;
                    }
                    pushedUndo = true;
                }
                notes.push_back({ c.note, c.pitch, c.chord, c.velocity, c.beat, -1 });
                lastBeat = std::max(lastBeat, c.beat);
                changed = true;
                break;
            case Captured::NoteOff:
                if (!open) break;
                for (auto it = notes.rbegin(); it != notes.rend(); ++it)
                    if (it->note == c.note && it->off < 0) {
                        it->off = std::max(c.beat, it->on);
                        changed = true;
                        break;
                    }
                lastBeat = std::max(lastBeat, c.beat);
                break;
            default: break;
        }
    }
    if (changed && open) rebuild(mood, lastBeat);
    return changed;
}

bool Recorder::quantise(int s, int t, int e, int v, int q, float amount) {
    if (open || quantiseBeats[std::size_t(std::clamp(q, 0, QuantiseCount - 1))] <= 0.0) return false;
    auto el = store.element(s, t, e);
    auto p = store.playing(s, t, e, v);
    if (p.count <= 0) return false;
    pushUndo(s, t, e, el);
    const double perBeat = pat::stepsPerBeat(p.resolution);
    for (int i = 0; i < p.count; ++i) {
        auto& n = p.notes[std::size_t(i)];
        const double at = quantiseBeat(double(n.step) / perBeat, q, amount);
        auto step = int(std::lround(at * perBeat));
        n.step = int16_t(((step % p.lengthSteps) + p.lengthSteps) % p.lengthSteps);
    }
    p.normalise();
    el.patternOf(v) = p;
    el.setEdited(v, true);
    store.setElement(s, t, e, el);
    ++changes;
    return true;
}

} // namespace wb::keys
