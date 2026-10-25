#include "Ensemble.h"

namespace wb {

void EnsemblePlace::prepare(double sampleRate) {
    rate = float(sampleRate);
    for (auto* p : parts)
        if (p != nullptr) p->prepare(rate);
    prepareFx();
    reset();
}

void EnsemblePlace::reset() {
    for (auto* p : parts)
        if (p != nullptr) p->reset();
    for (auto& b : busL) b.fill(0);
    for (auto& b : busR) b.fill(0);
    catchUp.fill(true);
    silent.fill(false);
    resetFx();
}

void EnsemblePlace::setSeed(uint32_t key) {
    noise.setKey(key, salt + 2);
    drift.setKey(key, salt + 3);
    for (int e = 0; e < placeElements; ++e)
        if (parts[std::size_t(e)] != nullptr) parts[std::size_t(e)]->setKey(key, salt + 10 + uint32_t(e));
}

NoteOn EnsemblePlace::noteFor(const PlaceContext& c, const Scheduled& x) const {
    const auto& ev = x.event;
    const auto& play = c.play[std::size_t(x.element)];
    NoteOn on;
    on.velocity = ev.velocity;
    on.beat = ev.beat;
    on.length = ev.length;
    on.seconds = float(ev.length / std::max(frame.beatsPerSecond, 0.01));
    on.extra = ev.extra;
    on.tag = x.tag;
    if (x.tag != 0 || play.pattern == nullptr) {
        // A Place's own note: the degree field carries semitones (or the kit row).
        on.semis = float(ev.degree);
        on.row = ev.degree;
    } else if (play.kit) {
        on.row = std::clamp(ev.degree, 0, 3);
    } else {
        const auto& p = *play.pattern;
        on.semis = float(pat::semitonesFor(p, frame.mood, x.chord, ev.degree, ev.alter));
        on.follow = p.followChords;
        on.base = p.base;
        on.degree = ev.degree;
        on.alter = ev.alter;
    }
    const uint64_t h = dsp::mix64(noise.key ^ (uint64_t(x.element + 1) << 56) ^ uint64_t(int64_t(std::floor(ev.beat * 96.0)) + (int64_t(1) << 40))
                                  ^ (uint64_t(ev.note + 2) << 44));
    on.random = dsp::unipolar(h);
    on.random2 = dsp::unipolar(dsp::mix64(h));
    return on;
}

void EnsemblePlace::render(const PlaceContext& c, float* left, float* right, int n) {
    const auto& w = *c.world;
    frame.rate = rate;
    frame.root = w.root;
    frame.mood = w.mood;
    frame.chord = c.chord;
    frame.quality = std::clamp(w.quality, 0, 2);
    frame.beat = c.beat;
    frame.beatsPerSecond = double(c.bpm) / 60.0;
    configure(c);
    for (auto* p : parts)
        if (p != nullptr) p->update(frame);

    std::array<Scheduled, maxScheduled> notes;
    int count = schedule(c, n, notes);
    const int before = count;
    count = addNotes(c, n, notes, count);
    if (count != before) sortScheduled(notes, count);

    for (int e = 0; e < placeElements; ++e) {
        const auto k = std::size_t(e);
        auto* part = parts[k];
        float* l = busL[k].data();
        float* r = busR[k].data();
        std::fill(l, l + n, 0.0f);
        std::fill(r, r + n, 0.0f);
        if (part == nullptr) continue;
        const float g = level[k] * c.s.element[k];
        // Switched off, muted or soloed away: silent and stopped. It picks up its
        // sustained notes again when it comes back.
        if (c.s.element[k] < 1.0e-4f) {
            if (!silent[k]) part->reset();
            silent[k] = true;
            catchUp[k] = true;
            continue;
        }
        silent[k] = false;
        const auto& play = c.play[k];
        if (catchUp[k]) {
            catchUp[k] = false;
            if (part->sustains() && play.pattern != nullptr)
                play.sounding(c.beat, [&](const pat::Event& ev) {
                    Scheduled x;
                    x.element = e;
                    x.event = ev;
                    x.chord = c.chord;
                    NoteOn on = noteFor(c, x);
                    if (shape(c, x, on)) part->start(frame, on);
                });
        }
        int done = 0;
        for (int i = 0; i < count; ++i) {
            const auto& x = notes[std::size_t(i)];
            if (x.element != e) continue;
            if (x.offset > done) {
                part->render(l + done, r + done, x.offset - done);
                done = x.offset;
            }
            NoteOn on = noteFor(c, x);
            if (!shape(c, x, on)) continue;
            // A player never hits two notes quite the same.
            NoteOn played = on;
            played.velocity = on.velocity * (0.92f + 0.16f * on.random2);
            part->start(frame, played);
            logEvent(c.clock + uint64_t(x.offset), e, play.kit && x.tag == 0 ? float(on.row) : on.semis, on.velocity);
        }
        if (done < n) part->render(l + done, r + done, n - done);
        for (int i = 0; i < n; ++i) {
            l[i] *= g;
            r[i] *= g;
        }
        // Population (the effect depth) deepens the sends, as it does the Place's own effects.
        if (c.fx != nullptr) c.fx->process(e, l, r, n, sendScale[k] * dsp::lerp(0.5f, 1.5f, c.s.fxDepth));
    }
    mix(c, left, right, n);
}

} // namespace wb
