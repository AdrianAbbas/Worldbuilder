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

// Milestone 16 part 5: Auto-arrange writes clips shaped for each section.
void testClipArrange(Suite& s) {
    using namespace clipTest;
    s.section("clips: auto-arrange shapes each section");
    {
        // A busy part: sixteenths over 8 bars, one scale step apart in fours.
        pat::Clip base;
        base.setLength(pat::Sixteenth, 8);
        base.followChords = false;
        for (int st = 0; st < base.lengthSteps; st += 2) base.add(makeNote(st, (st / 2) % 4, 2, 90));
        struct Stats {
            int count = 0, firstHalf = 0, secondHalf = 0;
            double degreeEarly = 0, degreeLate = 0, velEarly = 0, velLate = 0;
            int early = 0, late = 0;
        };
        const auto stats = [](const pat::Clip& c) {
            Stats st;
            st.count = c.count;
            for (int i = 0; i < c.count; ++i) {
                const auto& n = c.notes[std::size_t(i)];
                const double t = double(n.step) / double(c.lengthSteps);
                (t < 0.5 ? st.firstHalf : st.secondHalf)++;
                if (t < 0.25) {
                    st.degreeEarly += n.degree;
                    st.velEarly += n.velocity;
                    ++st.early;
                } else if (t >= 0.625 && t < 0.875) {
                    st.degreeLate += n.degree;
                    st.velLate += n.velocity;
                    ++st.late;
                }
            }
            if (st.early > 0) { st.degreeEarly /= st.early; st.velEarly /= st.early; }
            if (st.late > 0) { st.degreeLate /= st.late; st.velLate /= st.late; }
            return st;
        };
        const auto shaped = [&](int kind, bool runOut = false) {
            auto c = base;
            arr::shapeClip(c, kind, arr::RoleMelody, false, 7, 1234, runOut);
            return c;
        };
        const auto in = stats(base);
        const auto intro = shaped(arr::KindIntro);
        double introDegree = 0, introVel = 0;
        for (int i = 0; i < intro.count; ++i) {
            introDegree += intro.notes[std::size_t(i)].degree;
            introVel += intro.notes[std::size_t(i)].velocity;
        }
        introDegree /= std::max<int>(intro.count, 1);
        introVel /= std::max<int>(intro.count, 1);
        s.check(intro.count > 0 && intro.count < in.count / 2 && introDegree < -4.0 && introVel < 80.0, "an intro is sparse, lower and soft",
                std::to_string(intro.count) + " of " + std::to_string(in.count) + " notes, degree " + fmt(introDegree) + ", velocity " + fmt(introVel));
        const auto build = shaped(arr::KindBuild, true);
        const auto b = stats(build);
        bool runs = true;
        int run = 0, firstRun = 0, lastRun = 0;
        for (int i = 0, last = -100; i < build.count; ++i) {
            const auto& n = build.notes[std::size_t(i)];
            if (n.step < build.lengthSteps - 16) continue;
            runs &= n.degree >= last;
            if (run == 0) firstRun = n.degree;
            last = lastRun = n.degree;
            ++run;
        }
        runs &= lastRun >= firstRun + 4;
        s.check(b.secondHalf > b.firstHalf && b.degreeLate > b.degreeEarly + 1.5 && b.velLate > b.velEarly + 15.0,
                "a build-up rises: more notes, climbing pitch, louder",
                std::to_string(b.firstHalf) + " -> " + std::to_string(b.secondHalf) + " notes, degree " + fmt(b.degreeEarly) + " -> " + fmt(b.degreeLate)
                    + ", velocity " + fmt(b.velEarly) + " -> " + fmt(b.velLate));
        s.check(run >= 8 && runs, "and runs up into the next section in its last bar", std::to_string(run) + " notes in the last bar");
        int tight = 0;
        for (int i = 0; i < build.count; ++i) tight += build.notes[std::size_t(i)].step >= 40 && build.notes[std::size_t(i)].step % 2 == 1 ? 1 : 0;
        const auto peak = stats(shaped(arr::KindPeak));
        double peakVel = 0;
        const auto peakClip = shaped(arr::KindPeak);
        for (int i = 0; i < peakClip.count; ++i) peakVel += peakClip.notes[std::size_t(i)].velocity;
        peakVel /= std::max<int>(peakClip.count, 1);
        s.check(peak.count > in.count && peakVel > 95.0, "a climax is full (every note, louder, the melody doubled)", std::to_string(peak.count) + " notes, velocity " + fmt(peakVel));
        const auto calm = stats(shaped(arr::KindCalm));
        s.check(calm.secondHalf < calm.firstHalf && calm.velLate < calm.velEarly - 10.0, "a wind-down thins out and softens",
                std::to_string(calm.firstHalf) + " -> " + std::to_string(calm.secondHalf) + " notes");
        auto locked = base;
        for (int i = 0; i < locked.count; ++i) locked.notes[std::size_t(i)].setLocked(true);
        arr::shapeClip(locked, arr::KindIntro, arr::RoleMelody, false, 7, 99, false);
        s.check(locked.count == base.count, "locked notes are always kept");
        (void)tight;
    }
    {
        auto h = clipHost();
        h->set("p1_activity", 0.8f);
        h->set("p1_density", 0.8f);
        h->p->engine.arrangeVariants.store(true);
        h->render(0.5);   // every variant generated
        const auto a = h->p->autoArrangeWithClips(arr::makeTemplate(arr::ClassicArc));
        const auto again = h->p->autoArrangeWithClips(arr::makeTemplate(arr::ClassicArc));
        int clips = 0, introNotes = 0, climaxNotes = 0, introBars = 0, climaxBars = 0;
        bool linkedOff = true;
        for (int i = 0; i < a.count; ++i)
            for (int e = 0; e < placeElements; ++e) {
                const auto& cell = a.sections[std::size_t(i)].cells[0][std::size_t(e)];
                if (cell.clip == nullptr) {
                    continue;
                }
                ++clips;
                linkedOff &= cell.entry != arr::EntryOff;
                const int kind = arr::kindOf(a, i);
                if (kind == arr::KindIntro) introNotes += cell.clip->count;
                if (kind == arr::KindPeak) climaxNotes += cell.clip->count;
            }
        for (int i = 0; i < a.count; ++i) {
            const int kind = arr::kindOf(a, i);
            if (kind == arr::KindIntro) introBars += a.sections[std::size_t(i)].bars;
            if (kind == arr::KindPeak) climaxBars += a.sections[std::size_t(i)].bars;
        }
        s.check(clips >= 10 && linkedOff, "Auto-arrange gives the instruments that play their own section clips", std::to_string(clips) + " clips");
        s.check(introBars > 0 && climaxBars > 0 && double(introNotes) / introBars < 0.5 * double(climaxNotes) / climaxBars,
                "the intro's clips are much sparser than the climax's (notes per bar)",
                fmt(double(introNotes) / std::max(introBars, 1)) + " vs " + fmt(double(climaxNotes) / std::max(climaxBars, 1)));
        s.check(a == again, "the same World Seed gives the same clips");
        // Applied from the ARRANGE view (templates and AUTO-ARRANGE): editable and undoable.
        std::unique_ptr<juce::AudioProcessorEditor> editor(h->p->createEditor());
        auto* world = dynamic_cast<WorldbuilderEditor*>(editor.get());
        world->arrangeButton().onClick();
        world->settle(0.05f);
        world->arrangeView().applyTemplate(arr::ClassicArc);
        int fromView = 0;
        const auto shownA = h->p->arrangement.get();
        for (int i = 0; i < shownA.count; ++i)
            for (int e = 0; e < placeElements; ++e) fromView += shownA.sections[std::size_t(i)].cells[0][std::size_t(e)].clip != nullptr ? 1 : 0;
        h->p->arrangement.undo();
        s.check(fromView >= 10 && h->p->arrangement.get().count == 2, "TEMPLATE writes the clips too, and one undo takes it back", std::to_string(fromView) + " clips");
        // It plays: Arrangement mode, from the start, no allocation needed and finite.
        h->p->arrangement.set(a);
        play(*h);
        const auto audio = h->render(6.0);
        const auto st = measure(audio, 0.5);
        s.check(st.finite && st.peak < 1.0 && st.rmsDb > -80.0, "an auto-arranged song with clips plays (finite, below the ceiling)");
    }
}

// Milestone 16 part 6: renders, game files, stems and scene MIDI follow the clips.
void testClipCapture(Suite& s) {
    using namespace clipTest;
    s.section("clips: renders, game audio and scene MIDI follow the clips");
    // Castle and Tavern at 120 bpm; three sections (2 bars, 2 bars x 2, 2 bars); clips in
    // several cells (with the instruments' own Randomness: playback stays deterministic).
    const auto makeHost = [](int block) {
        auto h = std::make_unique<Host>(48000, block);
        h->playHead.bpm = 120;
        h->set("p1_type", float(Castle));
        h->set("p2_type", float(Tavern));
        h->p->patterns.syncTypes(h->p->slotTypes());
        arr::Arrangement a;
        a.sections[0] = arr::makeSection("Intro", 2, 0, 0.3f);
        a.sections[1] = arr::makeSection("Build", 2, 1, 0.6f);
        a.sections[1].repeats = 2;
        a.sections[2] = arr::makeSection("Climax", 2, 2, 0.9f);
        a.count = 3;
        a.sections[0].cells[0][element].clip = clipOf(2, 6, 1);
        a.sections[1].cells[0][element].clip = clipOf(2, 10, 4);
        a.sections[1].cells[1][2].clip = clipOf(2, 12, 2);
        a.sections[2].cells[0][2].clip = clipOf(2, 16, 0);
        a.sections[2].cells[0][element].fill = false;
        h->p->arrangement.set(a);
        h->set("playmode", float(PlayArrangement));
        h->playHead.playing = true;
        h->render(0.1);
        h->playHead.playing = false;
        h->render(0.3);
        return h;
    };
    auto host = makeHost(512);
    const auto scene = host->p->captureScene();
    s.check(scene.arrangement == host->p->arrangement.get() && scene.arrangement.sections[1].cells[1][2].clip != nullptr, "the scene carries the arrangement's clips");
    const auto mixPass = capture::passes(scene, false).front();
    bool all = true;
    std::string detail;
    for (int startBar : { 0, 3, 5 }) {
        const double seconds = 12.0 - 2.0 * startBar;
        Audio offline;
        capture::renderArranged(scene, scene.arrangement, mixPass, startBar * 4.0, capture::samplesFor(scene, seconds / 2.0), [&](const float* l, const float* r, int n) {
            offline.l.insert(offline.l.end(), l, l + n);
            offline.r.insert(offline.r.end(), r, r + n);
            return true;
        });
        auto live = makeHost(333);
        live->playHead.playing = true;
        live->playHead.position = int64_t(startBar) * 2 * 48000;
        const auto played = live->render(seconds);
        double worst = 0, energy = 0;
        for (std::size_t i = 0; i < played.l.size() && i < offline.l.size(); ++i) {
            worst = std::max(worst, double(std::abs(played.l[i] - offline.l[i])) + std::abs(played.r[i] - offline.r[i]));
            energy += double(played.l[i]) * played.l[i];
        }
        const bool ok = played.l.size() == offline.l.size() && worst < 1.0e-5 && energy > 1.0e-3;
        all &= ok;
        detail += "bar " + std::to_string(startBar + 1) + ": " + (ok ? "same" : "max difference " + std::to_string(worst)) + "; ";
    }
    s.check(all, "renders of the arrangement match Arrangement playback from any start bar, sample for sample", detail);
    // The clips are what is heard: the same scene with every clip re-linked renders differently.
    auto linkedScene = scene;
    for (int i = 0; i < linkedScene.arrangement.count; ++i)
        for (auto& row : linkedScene.arrangement.sections[std::size_t(i)].cells)
            for (auto& c : row) c.clip = nullptr;
    const auto renderSection = [&](const capture::Scene& sc, const capture::GameFile& file, const capture::Pass& pass) {
        Audio a;
        capture::renderGameFile(sc, file, pass, [&](const float* l, const float* r, int n) {
            a.l.insert(a.l.end(), l, l + n);
            a.r.insert(a.r.end(), r, r + n);
            return true;
        });
        return a;
    };
    const auto files = capture::gameFiles(scene, false);
    bool differ = true, audible = true;
    for (const auto& f : files) {
        const auto withClips = renderSection(scene, f, mixPass), linked = renderSection(linkedScene, f, mixPass);
        audible &= measure(withClips, 0.0).finite && measure(withClips, 0.0).rmsDb > -70.0;
        differ &= withClips.l != linked.l;
    }
    s.check(files.size() == 3 && audible && differ, "each section's game file (and the seamless loop) plays its clips", std::to_string(files.size()) + " files");
    // Stems: the Tavern's stem carries its clip.
    const auto stems = capture::passes(scene, true);
    const auto stemOf = [&](const capture::Scene& sc, const capture::Pass& pass) {
        capture::GameFile whole;
        whole.kind = capture::GameFile::Whole;
        return renderSection(sc, whole, pass);
    };
    bool stemDiffers = false;
    for (const auto& pass : stems)
        if (pass.name.containsIgnoreCase("Tavern")) stemDiffers = stemOf(scene, pass).l != stemOf(linkedScene, pass).l;
    s.check(stems.size() >= 2 && stemDiffers, "the stems follow the clips too");
    // Scene MIDI: inside a section with a clip, the instrument's track holds exactly the clip.
    const auto tracks = capture::sceneTracks(scene);
    const auto midi = capture::arrangementMidi(scene, tracks);
    int trackIndex = -1;
    for (std::size_t k = 0; k < tracks.size(); ++k)
        if (tracks[k].slot == 1 && tracks[k].element == 2) trackIndex = int(k) + 1;
    s.check(trackIndex > 0, "an instrument with clips has its track");
    if (trackIndex > 0) {
        std::vector<double> got;
        for (const auto* ev : *midi.getTrack(trackIndex))
            if (ev->message.isNoteOn()) {
                const double beat = ev->message.getTimeStamp() / double(pat::ticksPerBeat);
                if (beat >= 8.0 - 1.0e-6 && beat < 24.0 - 1.0e-6) got.push_back(beat);
            }
        std::vector<double> want;
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < 12; ++i) want.push_back(8.0 + 8.0 * pass + double(i * 32 / 12) / 4.0);
        bool same = got.size() == want.size();
        for (std::size_t i = 0; same && i < got.size(); ++i) same = std::abs(got[i] - want[i]) < 1.0e-6;
        s.check(same, "scene MIDI writes the clip's notes where its section plays them (both passes), and nothing else there",
                std::to_string(got.size()) + " notes, " + std::to_string(want.size()) + " wanted");
    }
}
