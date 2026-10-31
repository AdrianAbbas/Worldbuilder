// Milestone 16: section clips (included by WorldbuilderTests.cpp, after arrangeTest).
// Every section x instrument can have its own notes (unique) or play its variant (linked).
namespace clipTest {
using namespace arrangeTest;
constexpr int element = 4;   // the Castle's harp: a melodic, patterned instrument
// Castle in slot 1, its harp playing an edited one-bar pattern (four notes on `degree`),
// Play Mode Arrangement, 120 bpm (a bar is 2 s), two sections of two bars.
std::unique_ptr<Host> clipHost(int degree = 0) {
    auto h = std::make_unique<Host>();
    h->playHead.bpm = 120;
    h->set("p1_type", float(Castle));
    h->set("p1_solo", 1);
    h->p->patterns.syncTypes(h->p->slotTypes());
    h->p->patterns.setElement(0, Castle, element, notesPerBar(4, degree));
    arr::Arrangement a;
    a.sections[0] = arr::makeSection("Intro", 2, 0);
    a.sections[1] = arr::makeSection("Build", 2, 1);
    a.count = 2;
    h->p->arrangement.set(a);
    return h;
}
// The harp's notes from song position `from` (samples) for `seconds`.
std::vector<Place::Logged> harp(Host& h, int64_t from, double seconds) {
    if (const auto* place = h.p->engine.placeSlot(0).place()) const_cast<Place*>(place)->clearLog();
    h.playHead.playing = false;
    h.render(0.05);   // stopped: the next start is a locate (Transport restarts the world)
    if (const auto* place = h.p->engine.placeSlot(0).place()) const_cast<Place*>(place)->clearLog();
    play(h, from);
    h.render(seconds);
    std::vector<Place::Logged> out;
    for (const auto& ev : events(h))
        if (ev.element == element) out.push_back(ev);
    return out;
}
// A clip of `count` notes on `degree` over `bars` bars of sixteenths.
arr::ClipRef clipOf(int bars, int count, int degree) {
    auto c = std::make_shared<pat::Clip>();
    c->setLength(pat::Sixteenth, bars);
    c->followChords = false;
    c->swing = 0;
    c->ownSwing = true;
    for (int i = 0; i < count; ++i) c->add(makeNote(i * c->lengthSteps / count, degree));
    return c;
}
// Notes in [from, to) samples (their count in `n`), all at `pitch` (any pitch: NaN).
bool allPitch(const std::vector<Place::Logged>& ev, int64_t from, int64_t to, float pitch, int* n = nullptr) {
    int k = 0;
    bool ok = true;
    for (const auto& e : ev)
        if (int64_t(e.clock) >= from && int64_t(e.clock) < to) {
            ++k;
            ok &= std::isnan(pitch) || std::abs(e.pitch - pitch) < 0.01f;
        }
    if (n != nullptr) *n = k;
    return ok && k > 0;
}
std::vector<Place::Logged> after(const std::vector<Place::Logged>& ev, int64_t from) {
    std::vector<Place::Logged> out;
    for (const auto& e : ev)
        if (int64_t(e.clock) >= from) out.push_back(e);
    return out;
}
} // namespace clipTest

void testClips(Suite& s) {
    using namespace clipTest;
    const int64_t bar = 2 * 48000;
    const float any = std::numeric_limits<float>::quiet_NaN();
    s.section("clips: model");
    {
        arr::Arrangement a;
        a.sections[0] = arr::makeSection("A", 4, 0);
        a.sections[1] = arr::makeSection("B", 2, 1);
        a.sections[1].repeats = 2;
        a.count = 2;
        a.end = arr::EndLoop;
        a.sections[1].cells[0][element].clip = clipOf(2, 4, 3);
        a.sections[1].cells[0][element].fill = true;
        const auto in = a.evaluate(4.5 * 4), second = a.evaluate(6.5 * 4), looped = a.evaluate(12.5 * 4);
        s.check(in.clip[0][element] == a.sections[1].cells[0][element].clip.get() && in.clip[0][0] == nullptr && a.evaluate(1.0).clip[0][element] == nullptr,
                "a unique cell plays its clip; linked cells play their variant");
        s.check(in.clipOrigin == 16.0 && second.clipOrigin == 16.0 && in.clipSpan == 16.0 && looped.clipOrigin == 48.0,
                "a clip starts on its section's first downbeat (passes repeat it; after End: loop, where the loop brought it)",
                fmt(in.clipOrigin) + " " + fmt(second.clipOrigin) + " " + fmt(looped.clipOrigin));
        const auto fillBar = a.evaluate(7.5 * 4);
        s.check(fillBar.clip[0][element] == nullptr && fillBar.variant[0][element] == pat::VariantFill && second.clip[0][element] != nullptr
                    && a.evaluate(5.5 * 4).clip[0][element] != nullptr,
                "the Fill switch plays the Fill over a unique clip's last bar (last pass only)");
        const auto c = clipOf(2, 4, 1);
        const auto longer = ArrangementStore::conform(c, 5), shorter = ArrangementStore::conform(c, 1);
        s.check(longer->lengthSteps == 80 && longer->count == 10 && longer->notes[4].step == 32 && shorter->lengthSteps == 16 && shorter->count == 2,
                "a clip fits a new section length (repeating, or cut)");
        pat::Pattern p;
        p.setGrid(pat::Sixteenth, 3);
        p.add(makeNote(0, 0));
        p.add(makeNote(16, 1));
        p.add(makeNote(32, 2));
        const auto m = ArrangementStore::fromPattern(p, 4, 4, true);
        s.check(m->lengthSteps == 64 && m->count == 4 && m->notes[0].degree == 1 && m->notes[0].step == 0 && m->notes[1].degree == 2 && m->notes[2].degree == 0
                    && m->ownSwing,
                "a new clip is the variant repeated over the section, from its place in the song");
    }
    s.section("clips: linked and unique");
    {
        auto linked = clipHost();
        const auto reference = harp(*linked, 0, 8.5);
        auto unique = clipHost();
        s.check(unique->p->makeClipUnique(1, 0, element) && unique->p->arrangement.clipOf(1, 0, element) != nullptr, "make unique");
        const auto same = harp(*unique, 0, 8.5);
        s.check(reference.size() >= 12 && sameEvents(reference, same), "a new unique clip plays exactly as the variant it came from (nothing changes until it is edited)",
                std::to_string(reference.size()) + " / " + std::to_string(same.size()) + " notes");
        auto edited = clipHost();
        edited->p->makeClipUnique(1, 0, element);
        edited->p->arrangement.setClip(1, 0, element, clipOf(2, 8, 2));
        const auto ev = harp(*edited, 0, 8.5);
        const float intro = reference.empty() ? 0 : reference.front().pitch;
        int inBuild = 0;
        float buildPitch = 0;
        for (const auto& e : ev)
            if (int64_t(e.clock) >= 2 * bar && int64_t(e.clock) < 4 * bar) {
                ++inBuild;
                buildPitch = e.pitch;
            }
        s.check(allPitch(ev, 0, 2 * bar, intro) && inBuild == 8 && allPitch(ev, 2 * bar, 4 * bar, buildPitch) && std::abs(buildPitch - intro) > 0.5f,
                "an edited unique clip changes its own section only", std::to_string(inBuild) + " notes in the Build at " + fmt(buildPitch) + " (Intro " + fmt(intro) + ")");
        edited->p->patterns.setElement(0, Castle, element, notesPerBar(2, 4));
        const auto changed = harp(*edited, 0, 8.5);
        int introCount = 0, buildCount = 0;
        allPitch(changed, 0, 2 * bar, any, &introCount);
        const bool kept = allPitch(changed, 2 * bar, 4 * bar, buildPitch, &buildCount);
        s.check(introCount == 4 && buildCount == 8 && kept, "edits to the variant show up where it is linked, not in a unique clip",
                std::to_string(introCount) + " / " + std::to_string(buildCount));
        edited->p->relinkClip(1, 0, element);
        int relinkedBuild = 0;
        allPitch(harp(*edited, 0, 8.5), 2 * bar, 4 * bar, any, &relinkedBuild);
        s.check(edited->p->arrangement.clipOf(1, 0, element) == nullptr && relinkedBuild == 4, "re-link: the section plays its variant again",
                std::to_string(relinkedBuild) + " notes");
        edited->p->clearClip(1, 0, element);
        int cleared = 0;
        allPitch(harp(*edited, 0, 8.5), 2 * bar, 4 * bar, any, &cleared);
        s.check(cleared == 0 && edited->p->arrangement.clipOf(1, 0, element) != nullptr, "clear: the clip is unique and empty");
        int undone = 0;
        while (edited->p->arrangement.undo()) ++undone;
        s.check(undone >= 5 && edited->p->arrangement.clipOf(1, 0, element) == nullptr, "clip edits undo", std::to_string(undone) + " undos");
        for (int k = 0; k < 3; ++k) edited->p->arrangement.redo();
        s.check(edited->p->arrangement.clipOf(1, 0, element) != nullptr && edited->p->arrangement.clipOf(1, 0, element)->count == 8, "and redo");
    }
    s.section("clips: from any start bar");
    {
        auto h = clipHost();
        h->p->arrangement.setClip(0, 0, element, clipOf(2, 6, 1));
        h->p->arrangement.setClip(1, 0, element, clipOf(2, 5, 3));
        auto a = h->p->arrangement.get();
        a.sections[1].repeats = 2;
        h->p->arrangement.set(a);
        const auto whole = harp(*h, 0, 13.0);
        bool all = true;
        std::string detail;
        for (int startBar : { 1, 2, 3, 5 }) {
            auto g = clipHost();
            g->p->arrangement.set(h->p->arrangement.get());
            const int64_t from = int64_t(startBar) * bar;
            const auto got = after(harp(*g, from, 13.0 - 2.0 * startBar), from + 64), expect = after(whole, from + 64);
            const bool ok = sameEvents(expect, got) && !got.empty();
            all &= ok;
            detail += "bar " + std::to_string(startBar + 1) + ": " + std::to_string(got.size()) + "/" + std::to_string(expect.size()) + " ";
        }
        s.check(all, "clip playback from any start bar is what the whole song plays there", detail);
        int repeat = 0;
        const bool second = allPitch(whole, 4 * bar, 6 * bar, whole.empty() ? 0 : whole.back().pitch, &repeat);
        s.check(second && repeat == 5, "a repeating section plays its clip again on each pass", std::to_string(repeat) + " notes in the second pass");
    }
    s.section("clips: duplicate, copy and paste, resize");
    {
        auto h = clipHost();
        h->p->arrangement.setClip(1, 0, element, clipOf(2, 8, 2));
        const int d = h->p->arrangement.duplicate(1);
        const auto dup = h->p->arrangement.clipOf(d, 0, element);
        s.check(d == 2 && dup != nullptr && dup->count == 8, "duplicating a section copies its clips");
        h->p->arrangement.setClip(d, 0, element, clipOf(2, 2, 5));
        s.check(h->p->arrangement.clipOf(1, 0, element)->count == 8 && h->p->arrangement.clipOf(d, 0, element)->count == 2,
                "editing the copy leaves the original as it was");
        h->p->arrangement.setClip(0, 0, 3, h->p->arrangement.clipOf(1, 0, element));
        s.check(h->p->arrangement.clipOf(0, 0, 3) != nullptr && h->p->arrangement.clipOf(0, 0, 3)->count == 8, "a clip pastes into another section and instrument");
        h->p->arrangement.resize(1, 4, true);
        const auto grown = h->p->arrangement.clipOf(1, 0, element);
        h->p->arrangement.resize(1, 1, true);
        h->p->arrangement.resize(1, 3, false);
        const auto back = h->p->arrangement.clipOf(1, 0, element);
        s.check(grown->lengthSteps == 64 && grown->count == 16 && back->lengthSteps == 48 && back->count == 12,
                "resizing a section fits its clips (repeating into new bars; a drag gives notes back)", std::to_string(back->count) + " notes");
    }
    s.section("clips: state");
    {
        auto h = clipHost();
        h->set("p2_type", float(Tavern));
        h->p->patterns.syncTypes(h->p->slotTypes());
        h->p->arrangement.setClip(0, 0, element, clipOf(2, 7, 1));
        auto swung = std::make_shared<pat::Clip>(*clipOf(2, 64, 2));
        swung->swing = 0.3f;
        swung->ownSwing = false;
        swung->followChords = true;
        swung->base = -5;
        h->p->arrangement.setClip(1, 1, 2, swung);
        auto a = h->p->arrangement.get();
        a.sections[1].cells[0][element].fill = true;
        h->p->arrangement.set(a);
        juce::MemoryBlock state;
        h->p->getStateInformation(state);
        Host b;
        b.p->setStateInformation(state.getData(), int(state.getSize()));
        const auto back = b.p->arrangement.get();
        s.check(back == h->p->arrangement.get() && back.sections[1].cells[1][2].clip != nullptr && back.sections[1].cells[1][2].clip->base == -5
                    && !back.sections[1].cells[1][2].clip->ownSwing && back.sections[0].cells[0][element].clip->count == 7
                    && back.sections[1].cells[0][element].clip == nullptr,
                "an arrangement with unique clips survives a state round trip (linked cells stay linked)");
        Host c;
        s.check(c.p->importWorld(h->p->exportWorld()) && c.p->arrangement.get() == h->p->arrangement.get(), "and a .world file");
    }
    s.section("clips: real-time safety");
    {
        auto h = clipHost();
        for (int slot = 1; slot < placeSlots; ++slot) h->set(slotId(slot, PlType), float(Village + slot));
        h->set("p1_solo", 0);
        h->p->patterns.syncTypes(h->p->slotTypes());
        for (int sec = 0; sec < 2; ++sec)
            for (int slot = 0; slot < placeSlots; ++slot)
                for (int e = 0; e < placeElements; ++e) h->p->arrangement.setClip(sec, slot, e, clipOf(2, 32, e % 5));
        play(*h);
        h->render(1.0);
        allocationProbe::calls = 0;
        allocationProbe::enabled = true;
        for (int i = 0; i < 400; ++i) {
            if (i % 50 == 25) {
                allocationProbe::enabled = false;
                h->p->arrangement.setClip(i % 2, 0, element, clipOf(2, 16 + i % 7, 2));   // an edit while it plays (message thread)
                allocationProbe::enabled = true;
            }
            h->processOne(i % 3 == 0 ? 512 : 100 + i % 200);
        }
        allocationProbe::enabled = false;
        s.check(allocationProbe::calls == 0, "no heap allocations on the audio thread with four Places of clips, edited while playing",
                std::to_string(allocationProbe::calls) + " allocations");
    }
}
