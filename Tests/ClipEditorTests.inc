// Milestone 16: editing clips in the piano roll, the song view, and recording into a
// clip (included by WorldbuilderTests.cpp, after transportTest).
int clipCount(WorldbuilderProcessor& p, int section, int slot, int e) {
    const auto c = p.arrangement.clipOf(section, slot, e);
    return c != nullptr ? c->count : -1;
}
int clipLength(WorldbuilderProcessor& p, int section, int slot, int e) {
    const auto c = p.arrangement.clipOf(section, slot, e);
    return c != nullptr ? c->lengthSteps : -1;
}

void testClipEditor(Suite& s) {
    using namespace clipTest;
    s.section("clips: the piano roll on a section's clip");
    {
        auto h = clipHost();
        h->render(0.3);
        std::unique_ptr<juce::AudioProcessorEditor> editor(h->p->createEditor());
        auto* world = dynamic_cast<WorldbuilderEditor*>(editor.get());
        world->arrangeButton().onClick();
        world->settle(0.1f);
        auto& view = world->arrangeView();
        auto& pe = world->patternEditor();
        view.openClip(1, 0, element);
        s.check(pe.isVisible() && pe.view() == ui::PatternEditor::View::Clip && pe.section() == 1 && pe.element() == element,
                "clicking a section's instrument opens the piano roll on its clip");
        s.check(pe.notesShown().lengthSteps == 32 && pe.notesShown().count == 8 && std::abs(pe.toSong(0.0) - 8.0) < 1.0e-9,
                "the clip is its section's length, at the section's real bars (bar 3 of the song)",
                std::to_string(pe.notesShown().lengthSteps) + " steps, " + std::to_string(pe.notesShown().count) + " notes, starts at beat " + fmt(pe.toSong(0.0)));
        // A linked clip: the first edit asks; MAKE UNIQUE keeps the edit for this section only.
        pe.addNote(2, 5);
        s.check(pe.linkedPending() && h->p->arrangement.clipOf(1, 0, element) == nullptr, "editing a linked clip asks first (nothing is changed yet)");
        pe.resolveLinked(1);
        const auto unique = h->p->arrangement.clipOf(1, 0, element);
        s.check(!pe.linkedPending() && unique != nullptr && unique->count == 9 && h->p->arrangement.clipOf(0, 0, element) == nullptr,
                "MAKE UNIQUE: the edit lands in this section's own clip");
        pe.addNote(6, 3);
        s.check(!pe.linkedPending() && clipCount(*h->p, 1, 0, element) == 10, "a unique clip takes edits at once (asked only once)");
        pe.keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0));
        s.check(clipCount(*h->p, 1, 0, element) == 9, "undo in the piano roll undoes the clip edit");
        pe.keyPressed(juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0));
        s.check(clipCount(*h->p, 1, 0, element) == 10, "and redo");
        // Cancel takes the edit back; EDIT VARIANT opens the variant instead.
        view.openClip(0, 0, element);
        pe.addNote(3, 6);
        pe.resolveLinked(0);
        s.check(h->p->arrangement.clipOf(0, 0, element) == nullptr && pe.notesShown().count == 8 && pe.view() == ui::PatternEditor::View::Clip,
                "CANCEL: the clip stays linked, as it was");
        pe.addNote(3, 6);
        pe.resolveLinked(2);
        s.check(pe.view() == ui::PatternEditor::View::Pattern && pe.variant() == pat::VariantA && h->p->arrangement.clipOf(0, 0, element) == nullptr,
                "EDIT VARIANT: the variant it is linked to opens in the Pattern Editor");
        // MIDI export of a clip, and import into one (it keeps its section's length).
        view.openClip(1, 0, element);
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("wb-clip-test.mid");
        s.check(pe.saveMidi(file, element), "a clip saves as a MIDI file");
        juce::MidiFile midi;
        {
            juce::FileInputStream in(file);
            midi.readFrom(in);
        }
        int count = 0;
        double first = 1.0e9;
        for (int t = 0; t < midi.getNumTracks(); ++t)
            for (const auto* ev : *midi.getTrack(t))
                if (ev->message.isNoteOn()) {
                    ++count;
                    first = std::min(first, ev->message.getTimeStamp());
                }
        s.check(count == 10 && first == 0.0, "the MIDI clip holds the clip's notes from its first downbeat", std::to_string(count) + " notes");
        view.openClip(0, 0, element);
        s.check(pe.importMidi(file, element, true) && h->p->arrangement.clipOf(0, 0, element) == nullptr && pe.linkedPending(), "importing MIDI into a linked clip asks too");
        pe.resolveLinked(1);
        const auto imported = h->p->arrangement.clipOf(0, 0, element);
        s.check(imported != nullptr && imported->count == 10 && imported->lengthSteps == 32, "imported MIDI fills the clip, at the section's length",
                imported != nullptr ? std::to_string(imported->count) + " notes" : "none");
        file.deleteFile();
        // The transport works inside the clip.
        view.openClip(1, 0, element);
        auto& t = h->p->transport;
        s.check(t.arranged.load() && std::abs(t.loopStart.load() - 8.0) < 1.0e-9 && std::abs(t.loopEnd.load() - 16.0) < 1.0e-9 && t.playhead.load() >= 8.0,
                "the transport's loop and playhead are inside the clip (its bars in the song)",
                fmt(t.loopStart.load()) + ".." + fmt(t.loopEnd.load()));
        t.loop = true;
        h->playHead.playing = false;
        h->set("playmode", float(PlayTransport));
        h->p->transportPlay();
        h->render(1.0);
        s.check(h->p->engine.arrangeSection.load() == 1 && pe.toView(h->p->engine.slotBeat[0].load()) >= 0.0,
                "PLAY plays the arrangement in the clip's section", std::to_string(h->p->engine.arrangeSection.load()));
        h->render(4.5);
        const double beat = h->p->engine.slotBeat[0].load();
        s.check(beat >= 8.0 && beat < 16.0, "the loop goes round inside the clip", fmt(beat));
        h->p->transportStop();
        h->render(0.1);
        // The song view: every section end to end.
        world->arrangeButton().onClick();
        world->settle(0.05f);
        view.onOpenSong(0, element, 0);
        s.check(pe.view() == ui::PatternEditor::View::Song && pe.songSegments().size() == 2 && pe.notesShown().lengthSteps >= 64
                    && pe.notesShown().count == 20 && !pe.songSegments()[0].linked && !pe.songSegments()[1].linked,
                "the song view shows the instrument's clips across the whole arrangement", std::to_string(pe.notesShown().count) + " notes");
        const double perBeat = pat::stepsPerBeat(pe.notesShown().resolution);
        pe.addNote(int(std::lround(9.0 * perBeat)), 1);   // beat 9: bar 3, in the Build
        s.check(clipCount(*h->p, 1, 0, element) == 11 && clipCount(*h->p, 0, 0, element) == 10,
                "an edit in the song view lands in the section it is drawn in");
        h->p->relinkClip(0, 0, element);
        view.onOpenSong(0, element, 0);
        s.check(pe.songSegments()[0].linked, "linked sections are marked");
        pe.addNote(1, 2);
        s.check(pe.linkedPending(), "editing a linked section in the song view asks to make it unique");
        pe.resolveLinked(1);
        s.check(h->p->arrangement.clipOf(0, 0, element) != nullptr && clipCount(*h->p, 0, 0, element) == 9, "and makes it unique");
        // Back: BACK returns to ARRANGE.
        if (pe.onClose) pe.onClose();
        s.check(view.isVisible(), "BACK from a clip returns to ARRANGE");
    }
    s.section("clips: section tools in ARRANGE");
    {
        auto h = clipHost();
        h->render(0.3);
        std::unique_ptr<juce::AudioProcessorEditor> editor(h->p->createEditor());
        auto* world = dynamic_cast<WorldbuilderEditor*>(editor.get());
        world->arrangeButton().onClick();
        auto& view = world->arrangeView();
        h->p->arrangement.setClip(1, 0, element, clipOf(2, 6, 3));
        view.refresh();
        view.copyClip(1, 0, element);
        s.check(view.pasteClip(0, 0, 2) && h->p->arrangement.clipOf(0, 0, 2) != nullptr && clipCount(*h->p, 0, 0, 2) == 6,
                "copy a clip and paste it into another section and instrument");
        view.copySectionClips(1);
        auto a = h->p->arrangement.get();
        a.sections[0].bars = 4;
        h->p->arrangement.set(a);
        view.refresh();
        s.check(view.pasteSectionClips(0) && clipLength(*h->p, 0, 0, element) == 64 && clipCount(*h->p, 0, 0, element) == 12,
                "paste a whole section's clips (fitted to the section's length)");
        view.select(1);
        view.duplicateSection();
        s.check(h->p->arrangement.get().count == 3 && h->p->arrangement.clipOf(2, 0, element) != nullptr && clipCount(*h->p, 2, 0, element) == 6,
                "duplicate a section with its clips");
        bool opened = false;
        view.onOpenClip = [&](int, int, int) { opened = true; };
        view.openClip(0, 0, element);
        const bool before = h->p->arrangement.get().sections[0].cells[0][element].entry == arr::EntryOn;
        view.toggleCell(0, 0, element);
        s.check(opened && before && h->p->arrangement.get().sections[0].cells[0][element].entry == arr::EntryOff,
                "a click opens the clip; Cmd/Ctrl-click switches the instrument off");
    }
    s.section("clips: keyboard recording into a clip");
    {
        using namespace transportTest;
        auto h = stopped();
        constexpr int horns = CastlePlace::Horns;
        arr::Arrangement a;
        a.sections[0] = arr::makeSection("Intro", 2, 0);
        a.sections[1] = arr::makeSection("Build", 2, 1);
        a.count = 2;
        h.p->arrangement.set(a);
        h.render(0.2);
        // The editor shows the Build's clip: REC records into it, from its first downbeat.
        h.p->keys.clipSection = 1;
        h.p->transport.arranged = true;
        h.p->transport.loop = false;
        h.p->transport.playhead = 8.0;
        h.p->keys.mode = keys::Replace;
        h.p->setArmed(true);
        h.p->serviceKeys();
        s.check(h.p->arrangement.clipOf(1, 0, horns) != nullptr, "arming REC on a linked clip makes it unique (a take edits this section only)");
        const auto patternBefore = h.p->patterns.element(0, Castle, horns);
        h.p->transportPlay();
        // Beat 8 is the Build's first downbeat (4 s): notes on its beats 1 and 5 (+0.5 s, +2.5 s).
        playNotes(h, { 0.5, 2.5 }, 3.2);
        h.p->transportStop();
        h.render(0.1);
        h.p->serviceKeys();
        const auto clip = h.p->arrangement.clipOf(1, 0, horns);
        std::vector<int> stepsAt;
        if (clip != nullptr)
            for (int i = 0; i < clip->count; ++i) stepsAt.push_back(clip->notes[std::size_t(i)].step);
        // 1/32 steps: beat 1 is step 8, beat 5 (bar 2's beat 1) step 40.
        s.check(clip != nullptr && clip->lengthSteps == 64 && stepsAt == std::vector<int> { 8, 40 }, "recorded notes land in the clip, on the right bar and step",
                list(stepsAt));
        s.check(h.p->patterns.element(0, Castle, horns).pattern.sameNotes(patternBefore.pattern) && h.p->arrangement.clipOf(0, 0, horns) == nullptr,
                "the instrument's pattern and the other section are left as they were");
        h.p->recorder.undo();
        s.check(h.p->arrangement.clipOf(1, 0, horns) != nullptr && clipCount(*h.p, 1, 0, horns) != 2, "one undo takes the whole take back");
    }
}
