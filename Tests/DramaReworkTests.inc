// The Drama tier rework: drama changes the arrangement itself (per section, the Drama meter
// as an offset), the Drama instruments are real instruments (edited patterns play as
// edited, saved and reloaded, arranged, played from KEYS and in Instrument mode, with their
// levels, switches, mutes and effects), old sessions and scenes as before, the long
// templates and their evolving sections; listening renders.
// (Included from WorldbuilderTests.cpp after the Drama tier's tests: it uses their helpers.)

namespace reworkTest {

// What a section played: the Place's instruments playing (per chunk, summed over the
// slots), their variants as gears (C 0, A 1, B 2, Fill 2), doubled parts, the Drama
// instruments' tier, notes per second (the Places' and the Drama instruments') and level.
struct PerSection {
    double playing = 0, gear = 0, doubled = 0, fills = 0, chunks = 0, cells = 0, tier = 0;
    double notes = 0, seconds = 0, rmsDb = -240;
};

// Plays the arrangement from its start at `bpm` and measures each section (its first and
// last bar left out of the level, where transitions sound).
std::vector<PerSection> playSections(Host& h, const arr::Arrangement& a, double bpm, double untilBar = 1.0e9) {
    h.playHead.bpm = bpm;
    h.p->arrangement.set(a);
    arrangeTest::play(h);
    const double barSeconds = 240.0 / bpm;
    const double total = std::min(double(a.totalBars()), untilBar) * barSeconds;
    std::vector<PerSection> out(std::size_t(a.count));
    Audio audio;
    audio.rate = h.rate;
    const auto types = h.p->slotTypes();
    double at = 0;
    while (at < total) {
        const uint32_t before = allEvents(h) + dramaTest::dramaNotes(h);
        const auto piece = h.render(barSeconds / 4.0);
        audio.l.insert(audio.l.end(), piece.l.begin(), piece.l.end());
        audio.r.insert(audio.r.end(), piece.r.begin(), piece.r.end());
        at += barSeconds / 4.0;
        const auto& st = h.p->engine.arrangeState();
        if (st.at.section < 0 || st.at.ended) continue;
        auto& x = out[std::size_t(st.at.section)];
        x.chunks += 1;
        x.notes += double(allEvents(h) + dramaTest::dramaNotes(h) - before);
        x.seconds += barSeconds / 4.0;
        x.tier += st.drama < 0 ? 0.0 : double(st.drama);
        for (int slot = 0; slot < placeSlots; ++slot) {
            if (types[std::size_t(slot)] == NoPlace) continue;
            for (int e = 0; e < placeElements; ++e) {
                if (patternInfo(types[std::size_t(slot)], e).kind == NoPattern) continue;
                if (st.gain[std::size_t(slot)][std::size_t(e)] <= 0.0f) continue;
                const int v = st.variant[std::size_t(slot)][std::size_t(e)];
                x.playing += 1;
                x.cells += 1;
                x.gear += v == pat::VariantFill ? 2.0 : double(arr::variantGear(v));
                x.fills += v == pat::VariantFill ? 1.0 : 0.0;
                x.doubled += st.doubling[std::size_t(slot)][std::size_t(e)] != 0 ? 1.0 : 0.0;
            }
        }
    }
    for (int i = 0; i < a.count; ++i) {
        auto& x = out[std::size_t(i)];
        if (x.chunks > 0) {
            x.gear /= std::max(1.0, x.cells);
            x.playing /= x.chunks;
            x.doubled /= x.chunks;
            x.tier /= x.chunks;
        }
        x.notes = x.seconds > 0 ? x.notes / x.seconds : 0.0;
        const double from = double(a.startOf(i) + 1) * barSeconds, to = double(a.startOf(i) + a.sections[std::size_t(i)].span() - 1) * barSeconds;
        if (to > from + 1.0) x.rmsDb = measure(audio, from, to).rmsDb;
    }
    return out;
}

// A world of Places (each at its card's defaults), the arrangement's template Auto-arranged
// for them with its arcs, at a Drama amount.
Host battleWorld(const std::vector<int>& types, float drama, arr::Arrangement& a, int which = arr::Battle) {
    Host h;
    for (int slot = 0; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(NoPlace));
    std::array<int, placeSlots> slots {};
    slots.fill(NoPlace);
    for (std::size_t k = 0; k < types.size(); ++k) {
        h.set(slotId(int(k), PlType), float(types[k]));
        slots[k] = types[k];
    }
    h.set("drama", drama);
    h.set("intensity", 0.5f);
    a = arr::autoArrange(arr::makeTemplate(which), slots, 7);
    arr::dramatize(a, 7);
    return h;
}

std::string describe(const PerSection& x) {
    return fmt(x.playing) + " playing, gear " + fmt(x.gear) + ", " + fmt(x.notes) + " notes/s, " + fmt(x.rmsDb) + " dB, doubled " + fmt(x.doubled) + ", tier "
           + fmt(x.tier);
}

// The Drama drums' notes the layer played (sample, row) in slot 1, from log entry `from`.
std::vector<std::pair<uint64_t, int>> dramaLog(const Host& h, int element, uint32_t from = 0) {
    std::vector<std::pair<uint64_t, int>> out;
    const auto& layer = h.p->engine.placeSlot(0).drama();
    const uint32_t count = layer.loggedCount();
    for (uint32_t i = std::max(from, count > uint32_t(drama::Layer::logSize) ? count - uint32_t(drama::Layer::logSize) : 0u); i < count; ++i)
        if (layer.loggedAt(i).element == element) out.push_back({ layer.loggedAt(i).clock, layer.loggedAt(i).row });
    return out;
}

// A Castle alone in Transport at 120 bpm, at Climax, its Drama drums edited.
pat::Element editedDrums() {
    pat::Element el;
    el.controls = defaultControls(Castle, DramaDrums);
    el.controls.edited = true;
    el.controls.randomness = 0;
    el.pattern.setGrid(pat::Sixteenth, 1);
    el.pattern.followChords = false;
    el.pattern.swing = 0;
    el.pattern.add(makeNote(0, drama::RowLow, 1, 120));
    el.pattern.add(makeNote(6, drama::RowSnare, 1, 110));
    el.pattern.add(makeNote(10, drama::RowMid, 1, 100));
    el.pattern.add(makeNote(14, drama::RowTomLow, 1, 90));
    return el;
}
Host climaxCastle() {
    Host h;
    h.playHead.bpm = 120;
    for (int slot = 1; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(NoPlace));
    h.set("p1_type", float(Castle));
    h.set("drama", 1.0f);
    h.set("intensity", 1.0f);
    h.set("environment", -60.0f);   // the Place alone (no environment bed under it)
    h.set("playmode", float(PlayTransport));
    h.p->patterns.syncTypes(h.p->slotTypes());
    h.playHead.playing = true;
    h.playHead.position = 0;
    return h;
}
// The steps (sixteenths in the bar) and rows of the logged notes after `fromSeconds`.
std::set<std::pair<int, int>> stepsAndRows(const std::vector<std::pair<uint64_t, int>>& log, double bpm, double fromSeconds) {
    std::set<std::pair<int, int>> out;
    for (const auto& [clock, row] : log) {
        const double seconds = double(clock) / 48000.0;
        if (seconds < fromSeconds) continue;
        const double step = seconds * bpm / 60.0 * 4.0;
        out.insert({ int(std::llround(step)) % 16, row });
    }
    return out;
}

} // namespace reworkTest

void testDramaRework(Suite& s) {
    using namespace reworkTest;
    using dramaTest::writeFlac;
    using dramaTest::appendPart;

    s.section("drama rework: nine instruments per slot, parameters appended");
    {
        // The parameters before the rework keep their places; the new ones come after them.
        s.check(std::string(specs[std::size_t(PDramaStyle)].id) == "dramastyle" && PDramaFirst == PDramaStyle + 1 && PCount == PDramaFirst + 4 * DramaParamCount,
                "the Drama instruments' host parameters are appended after every older one");
        bool ids = true;
        for (int slot = 0; slot < placeSlotCount; ++slot)
            for (int e = 0; e < slotElements; ++e) {
                const std::string prefix = "p" + std::to_string(slot + 1) + "_e" + std::to_string(e + 1);
                ids = ids && specs[std::size_t(elementLevelParam(slot, e))].id == prefix && specs[std::size_t(elementOnParam(slot, e))].id == prefix + "_on"
                      && specs[std::size_t(elementFxParam(slot, e, FxReverb))].id == prefix + "_rev"
                      && specs[std::size_t(elementFxParam(slot, e, FxSlideTime))].id == prefix + "_slidetime";
            }
        s.check(ids, "each slot's nine instruments have a level, a switch and the effects (p1_e7..p1_e9 for the Drama drums, ostinato and hits)");
        bool named = true;
        for (int type : builtPlaces()) {
            named = named && patternInfo(type, DramaDrums).kind == Kit && patternInfo(type, DramaDrums).rows >= 6 && patternInfo(type, DramaOstinato).kind == Melodic
                    && patternInfo(type, DramaHits).kind == Kit && patternInfo(type, DramaHits).rows == drama::HitRowCount
                    && std::string(elementSpec(type, DramaHits).name).find('&') != std::string::npos;
        }
        s.check(named, "every Place has its three Drama instruments (themed drums with their kit rows, an ostinato, its two hits)");
    }

    s.section("drama rework: the Drama instruments' patterns");
    {
        // Generated by tier and style: busier with the tier, B busier and C sparser than A.
        GenInputs in;
        in.key = 99;
        in.style = drama::StyleAuto;
        int lastCount = -1;
        bool rising = true, variants = true;
        for (int tier = drama::Tension; tier <= drama::Climax; ++tier) {
            in.tier = tier;
            pat::Pattern a, b, c;
            drama::generate(Castle, DramaDrums, pat::VariantA, in, a);
            drama::generate(Castle, DramaDrums, pat::VariantB, in, b);
            drama::generate(Castle, DramaDrums, pat::VariantC, in, c);
            rising = rising && a.count > lastCount;
            variants = variants && b.count > a.count && c.count < a.count;
            lastCount = a.count;
        }
        s.check(rising && variants, "the Drama drums' starting patterns get busier with the tier; B is busier and C sparser than A");
        in.tier = drama::Action;
        pat::Pattern second, third;
        in.rank = 1;
        drama::generate(Castle, DramaDrums, pat::VariantA, in, second);
        pat::Pattern ost;
        drama::generate(Castle, DramaOstinato, pat::VariantA, in, ost);
        in.rank = 2;
        drama::generate(Castle, DramaOstinato, pat::VariantA, in, third);
        s.check(second.count == 0 && ost.count > 0 && third.count == 0, "the leading Place plays the drums, the second the ostinato too, the rest the hits");
        // The editor sees them: the audio thread reports them back with the six.
        auto h = climaxCastle();
        h.render(3.0);
        const auto shown = h.p->patterns.playing(0, Castle, DramaDrums);
        pat::Pattern expected;
        GenInputs want;
        s.check(shown.count > 0 && h.p->engine.placeSlot(0).generatedPattern(DramaDrums).sameNotes(shown),
                "the Pattern Editor shows the Drama drums' generated pattern (the tier playing)", std::to_string(shown.count) + " notes");
        (void) expected;
        (void) want;
    }

    s.section("drama rework: an edited Drama pattern plays as edited, saved and reloaded, arranged");
    {
        const double bpm = 120;
        auto h = climaxCastle();
        h.p->patterns.setElement(0, Castle, DramaDrums, editedDrums());
        h.render(8.0);   // four bars at 120 bpm
        const auto log = dramaLog(h, DramaDrums);
        const auto heard = stepsAndRows(log, bpm, 1.0);
        const std::set<std::pair<int, int>> written { { 0, drama::RowLow }, { 6, drama::RowSnare }, { 10, drama::RowMid }, { 14, drama::RowTomLow } };
        s.check(heard == written && log.size() == 16, "the edited Drama drums play exactly their notes (steps and rows), bar after bar",
                std::to_string(log.size()) + " notes, " + std::to_string(heard.size()) + " different");
        // Saved and reloaded.
        juce::MemoryBlock block;
        h.p->getStateInformation(block);
        auto g = climaxCastle();
        g.p->setStateInformation(block.getData(), int(block.getSize()));
        g.p->patterns.syncTypes(g.p->slotTypes());
        g.render(8.0);
        const auto again = stepsAndRows(dramaLog(g, DramaDrums), bpm, 1.0);
        s.check(g.p->patterns.element(0, Castle, DramaDrums).pattern.sameNotes(editedDrums().pattern) && g.p->patterns.element(0, Castle, DramaDrums).controls.edited
                    && again == written,
                "saved and reloaded, the edited Drama pattern is there and plays the same");
        // Its switch, mute and level.
        auto off = climaxCastle();
        off.p->patterns.setElement(0, Castle, DramaDrums, editedDrums());
        off.set("p1_e7_on", 0.0f);
        off.render(3.0);
        auto muted = climaxCastle();
        auto el = editedDrums();
        el.controls.mute = true;
        muted.p->patterns.setElement(0, Castle, DramaDrums, el);
        muted.render(3.0);
        s.check(dramaLog(off, DramaDrums).empty() && dramaLog(muted, DramaDrums).empty(), "switched off or muted, the Drama drums play nothing");
        const auto soloLevel = [&](float level) {
            auto x = climaxCastle();
            auto drums = editedDrums();
            drums.controls.solo = true;
            x.p->patterns.setElement(0, Castle, DramaDrums, drums);
            x.set("p1_e7", level);
            x.set("p1_e7_rev", 0.0f);
            x.set("p1_level", -18.0f);   // under the master's glue and limiter
            return measure(x.render(6.0), 2.0).rmsDb;
        };
        const double full = soloLevel(0.8f), half = soloLevel(0.4f), none = soloLevel(0.0f);
        // (What stays with the drums at 0, the Place's own space, taken out by power.)
        const auto without = [&](double db) { return 10.0 * std::log10(std::max(1.0e-30, std::pow(10.0, db / 10.0) - std::pow(10.0, none / 10.0))); };
        s.check(std::abs(without(full) - without(half) - 6.02) < 1.0 && none < full - 12.0, "its level scales it (soloed: 0.8 against 0.4 is 6 dB)",
                fmt(full) + " / " + fmt(half) + " / at 0 " + fmt(none) + " dB");
        // Its effects: more reverb, a different sound.
        const auto withReverb = [&](float send) {
            auto x = climaxCastle();
            auto drums = editedDrums();
            drums.controls.solo = true;
            x.p->patterns.setElement(0, Castle, DramaDrums, drums);
            x.set("p1_e7_rev", send);
            return x.render(5.0);
        };
        const auto dry = withReverb(0.0f), wet = withReverb(1.0f);
        Audio diff;
        for (std::size_t i = 0; i < dry.l.size(); ++i) {
            diff.l.push_back(wet.l[i] - dry.l[i]);
            diff.r.push_back(wet.r[i] - dry.r[i]);
        }
        s.check(measure(diff, 2.0).rmsDb > measure(dry, 2.0).rmsDb - 20.0, "its reverb send reaches the shared reverb",
                "difference " + fmt(measure(diff, 2.0).rmsDb) + " dB");
        // A listening render of the edited pattern.
        std::vector<float> pl, pr;
        const auto edited = [&] {
            auto x = climaxCastle();
            x.p->patterns.setElement(0, Castle, DramaDrums, editedDrums());
            return x.render(14.0);
        }();
        appendPart(pl, pr, edited, 2.0, 12.0);
        writeFlac("drama-edited-pattern-castle.flac", pl, pr, 48000);
    }
    {
        // In ARRANGE: a section where the Drama drums are off, one where they play their
        // edited variant B; the arrangement decides.
        Host h;
        h.playHead.bpm = 120;
        for (int slot = 1; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(NoPlace));
        h.set("p1_type", float(Castle));
        h.set("drama", 1.0f);
        h.p->patterns.syncTypes(h.p->slotTypes());
        auto el = editedDrums();
        auto& b = el.patternOf(pat::VariantB);
        b.setGrid(pat::Sixteenth, 1);
        b.followChords = false;
        b.swing = 0;
        for (int step : { 0, 4, 8, 12 }) b.add(makeNote(step, drama::RowSnare, 1, 100));
        el.setEdited(pat::VariantB, true);
        h.p->patterns.setElement(0, Castle, DramaDrums, el);
        arr::Arrangement a;
        a.sections[0] = arr::makeSection("Battle", 4, 0, 0.9f);
        a.sections[0].drama = drama::Climax;
        a.sections[0].cells[0][DramaDrums].entry = arr::EntryOff;
        a.sections[1] = a.sections[0];
        a.sections[1].cells[0][DramaDrums].entry = arr::EntryOn;
        a.sections[1].cells[0][DramaDrums].variant = pat::VariantB;
        a.count = 2;
        h.p->arrangement.set(a);
        arrangeTest::play(h);
        h.render(8.0);   // section 1 (4 bars at 120 bpm)
        const uint32_t mark = h.p->engine.placeSlot(0).drama().loggedCount();
        const auto first = dramaLog(h, DramaDrums);
        h.render(6.0);
        // (Its first three bars: a fight's fourth bar ends on the drums' Fill.)
        const auto second = stepsAndRows(dramaLog(h, DramaDrums, mark), 120, 8.0);
        const std::set<std::pair<int, int>> wantB { { 0, drama::RowSnare }, { 4, drama::RowSnare }, { 8, drama::RowSnare }, { 12, drama::RowSnare } };
        s.check(first.empty() && second == wantB, "in ARRANGE the Drama drums' row decides: off in one section, their edited B in the next",
                std::to_string(first.size()) + " notes while off, " + std::to_string(second.size()) + " kinds after");
    }

    s.section("drama rework: KEYS, Instrument mode and the recorder on the Drama instruments");
    {
        auto h = climaxCastle();
        h.set("drama", 0.0f);
        // The Drama ostinato soloed: the Castle's own patterns rest, the keyboard plays it.
        auto ost = h.p->patterns.element(0, Castle, DramaOstinato);
        ost.controls.solo = true;
        h.p->patterns.setElement(0, Castle, DramaOstinato, ost);
        h.p->keys.slot = 0;
        h.p->keys.element = DramaOstinato;
        h.render(2.2);
        const auto quiet = measure(h.render(0.5), 0.0).rmsDb;
        keysTest::note(h, 57, 110);
        const auto played = h.render(0.6);
        keysTest::off(h, 57);
        h.render(0.2);
        s.check(measure(played, 0.0).rmsDb > quiet + 6.0 && h.p->engine.placeSlot(0).drama().started() > 0,
                "KEYS plays the Drama ostinato (even at Drama 0)", fmt(quiet) + " -> " + fmt(measure(played, 0.0).rmsDb) + " dB");
        Host i;
        i.p->setInstrument({ Castle, DramaHits }, true);
        i.p->setInstrumentMode(true);
        i.render(0.3);
        keysTest::note(i, 60, 110);
        const auto hit = i.render(0.8);
        s.check(inst::Id { Castle, DramaHits }.valid() && measure(hit, 0.0).rmsDb > -50.0, "Instrument mode plays a Drama instrument (the Castle's hits)",
                fmt(measure(hit, 0.0).rmsDb) + " dB");
        // The recorder takes a Drama instrument's notes into its pattern.
        auto r = climaxCastle();
        r.p->keys.slot = 0;
        r.p->keys.element = DramaDrums;
        r.render(0.5);
        keysTest::recordTake(r, { 0.0, 1.0, 2.5 }, 120, 3.0, 60);
        const auto took = r.p->patterns.element(0, Castle, DramaDrums);
        s.check(took.controls.edited && took.pattern.count >= 3, "REC records into the Drama drums' pattern", std::to_string(took.pattern.count) + " notes");
    }

    s.section("drama rework: drama changes the arrangement itself");
    {
        const std::vector<int> types { Castle, Village };
        arr::Arrangement a;
        auto classic = battleWorld(types, 0.0f, a);
        const auto c0 = playSections(classic, a, 120.0);
        auto low = battleWorld(types, 0.5f, a);
        const auto c50 = playSections(low, a, 120.0);
        auto high = battleWorld(types, 1.0f, a);
        const auto c100 = playSections(high, a, 120.0);
        std::string text;
        bool fights = true, calm = true;
        for (int i = 0; i < a.count; ++i) {
            const auto kind = arr::kindOf(a, i);
            const auto& x0 = c0[std::size_t(i)];
            const auto& x5 = c50[std::size_t(i)];
            const auto& x1 = c100[std::size_t(i)];
            text += std::string(a.sections[std::size_t(i)].nameText()) + ": classic " + describe(x0) + " | 50 % " + describe(x5) + " | 100 % " + describe(x1) + "; ";
            if (kind == arr::KindPeak) {
                fights = fights && x1.playing >= x0.playing && x1.gear > x0.gear + 0.25 && x1.notes > 1.3 * x0.notes && x1.doubled > 0.0;
                fights = fights && x1.playing >= x5.playing && x1.gear > x5.gear + 0.1 && x1.notes > 1.1 * x5.notes;
            }
            if (kind == arr::KindIntro || kind == arr::KindBreak) calm = calm && x1.playing < x0.playing - 0.5;
        }
        s.check(fights, "with an arrangement playing, Drama raises its fights: more of the instruments, busier variants, denser, doubled (per section, 0 / 50 / 100 %)",
                text);
        s.check(calm, "and thins its calm sections (intro, break): fewer of the instruments play", text);
        const auto contrast = [&](const std::vector<PerSection>& c) {
            double fight = 240, rest = -240;
            for (int i = 0; i < a.count; ++i) {
                const int kind = arr::kindOf(a, i);
                if (kind == arr::KindPeak) fight = std::min(fight, c[std::size_t(i)].rmsDb);
                if (kind == arr::KindBreak || kind == arr::KindIntro) rest = std::max(rest, c[std::size_t(i)].rmsDb);
            }
            return fight - rest;
        };
        s.check(contrast(c100) > contrast(c0) + 3.0 && contrast(c50) > contrast(c0), "the arcs are heard in the arrangement's own parts: the fights stand further above the calm",
                "fight over calm: classic " + fmt(contrast(c0)) + " dB, 50 % " + fmt(contrast(c50)) + " dB, 100 % " + fmt(contrast(c100)) + " dB");
        for (int i = 0; i < a.count; ++i) {
            const std::string name = std::string("Battle ") + a.sections[std::size_t(i)].nameText();
            for (const auto& [label, c] : { std::pair<const char*, const std::vector<PerSection>*> { " at Drama 0 %", &c0 }, { " at Drama 50 %", &c50 },
                                            { " at Drama 100 %", &c100 } }) {
                s.metric(name + label + ": instruments playing", (*c)[std::size_t(i)].playing);
                s.metric(name + label + ": variant gear (C 0, A 1, B 2)", (*c)[std::size_t(i)].gear);
                s.metric(name + label + ": notes", (*c)[std::size_t(i)].notes, " /s");
                s.metric(name + label + ": level", (*c)[std::size_t(i)].rmsDb, " dB");
            }
        }
        // The harmony follows the section's gear, the Drama instruments their tier.
        double fightTier = 0, calmTier = 9;
        for (int i = 0; i < a.count; ++i) {
            if (arr::kindOf(a, i) == arr::KindPeak) fightTier = std::max(fightTier, c100[std::size_t(i)].tier);
            if (arr::kindOf(a, i) == arr::KindBreak) calmTier = std::min(calmTier, c100[std::size_t(i)].tier);
        }
        s.check(fightTier > 2.4 && calmTier < 0.5, "at 100 % the fights play at Climax and the break at Calm", fmt(fightTier) + " / " + fmt(calmTier));
        double tier50 = 0;
        for (int i = 0; i < a.count; ++i)
            if (arr::kindOf(a, i) == arr::KindPeak) tier50 = std::max(tier50, c50[std::size_t(i)].tier);
        s.check(tier50 > 1.4 && tier50 < 2.6, "the Drama meter is an offset: at 50 % the fights play one tier lower (Action)", fmt(tier50));
    }
    {
        // Listening renders: the Battle template at Drama 0, 50 and 100 % (from its Tension
        // through its first fight into the break).
        const std::vector<int> types { Castle, Village };
        for (float amount : { 0.0f, 0.5f, 1.0f }) {
            arr::Arrangement a;
            auto h = battleWorld(types, amount, a);
            h.playHead.bpm = 120;
            h.p->arrangement.set(a);
            arrangeTest::play(h);
            const auto audio = h.render(double(a.startOf(3) + 4) * 2.0);
            std::vector<float> pl, pr;
            appendPart(pl, pr, audio, double(a.startOf(1) - 2) * 2.0, double(a.startOf(3) + 4 - a.startOf(1) + 2) * 2.0);
            writeFlac("drama-battle-template-drama-" + juce::String(juce::roundToInt(amount * 100.0f)) + ".flac", pl, pr, 48000);
        }
    }

    s.section("drama rework: long sections that evolve");
    {
        bool lengths = true, evolving = true;
        std::string wrong;
        for (int t = 0; t < arr::TemplateCount; ++t) {
            const auto a = arr::makeTemplate(t);
            for (int i = 0; i < a.count; ++i) {
                const auto& sec = a.sections[std::size_t(i)];
                std::string name = sec.nameText();
                for (auto& ch : name) ch = char(std::tolower(static_cast<unsigned char>(ch)));
                const int kind = arr::kindOf(a, i);
                const bool main = name.find("combat") != std::string::npos || name.find("battle") != std::string::npos || name.find("chase") != std::string::npos
                                  || name.find("loop") != std::string::npos || name.find("theme") != std::string::npos || name.find("phase") != std::string::npos;
                const bool edge = !main && (kind == arr::KindIntro || kind == arr::KindBreak || kind == arr::KindOutro);
                const int lo = main ? 32 : (edge ? 8 : 16), hi = main ? 64 : (edge ? 16 : 32);
                if (sec.bars < lo || sec.bars > hi) {
                    lengths = false;
                    wrong += std::string(arr::templateNames[std::size_t(t)]) + "/" + sec.nameText() + " " + std::to_string(sec.bars) + " ";
                }
                evolving = evolving && sec.evolve;
            }
        }
        s.check(lengths, "every template's sections are scoring lengths (main 32-64 bars, intros and transitions 8-16, the rest 16-32)", wrong);
        s.check(evolving && arr::maxBars >= 128, "every template section evolves; sections may be up to 128 bars");
        // Auto-arrange lengthens short sections; an arrangement as saved keeps its lengths.
        arr::Arrangement mine;
        mine.sections[0] = arr::makeSection("Intro", 4, 0, 0.3f);
        mine.sections[1] = arr::makeSection("Combat", 8, 2, 0.9f);
        mine.sections[2] = arr::makeSection("Battle", 96, 2, 0.9f);
        mine.count = 3;
        std::array<int, placeSlots> types { Castle, NoPlace, NoPlace, NoPlace };
        const auto auto_ = arr::autoArrange(mine, types, 3);
        const auto kept = ArrangementStore::fromTree(ArrangementStore::toTree(mine));
        s.check(auto_.sections[0].bars == 8 && auto_.sections[1].bars == 32 && auto_.sections[2].bars == 96 && kept.sections[1].bars == 8 && !kept.sections[1].evolve,
                "Auto-arrange gives short sections their scoring length (longer ones stay); a saved arrangement keeps its lengths");
        bool inspired = true;
        for (uint32_t seed : { 5u, 50u, 500u }) {
            const auto sc = scenes::inspire(seed, scenes::MoodBattle);
            const auto a = scenes::arrangement(sc);
            int longest = 0;
            for (int i = 0; i < a.count; ++i) longest = std::max<int>(longest, a.sections[std::size_t(i)].bars);
            inspired = inspired && longest >= 32;
        }
        s.check(inspired, "Inspire's arrangements are long too");
    }
    {
        // A 32-bar section rises: its last eight bars denser and louder than its first eight
        // (Classic: the section's own evolution alone; and with Drama).
        for (float amount : { 0.0f, 1.0f }) {
            const std::vector<int> types { Castle, Village };
            arr::Arrangement a;
            auto h = battleWorld(types, amount, a);
            const int combat = 2;
            s.check(a.sections[combat].bars == 32 && a.sections[combat].evolve, "the Battle template's Combat is 32 bars and evolves");
            h.playHead.bpm = 120;
            h.p->arrangement.set(a);
            arrangeTest::play(h);
            h.render(double(a.startOf(combat)) * 2.0);
            std::vector<double> notes, levels;
            std::set<int> variants;
            for (int bar = 0; bar < 32; ++bar) {
                const uint32_t before = allEvents(h) + dramaTest::dramaNotes(h);
                const auto piece = h.render(2.0);
                notes.push_back(double(allEvents(h) + dramaTest::dramaNotes(h) - before) / 2.0);
                levels.push_back(measure(piece, 0.0).rmsDb);
                const auto& st = h.p->engine.arrangeState();
                variants.insert(st.variant[0][0] * 10 + st.variant[0][2]);
            }
            const auto mean = [](const std::vector<double>& v, int from, int to) {
                double sum = 0;
                for (int i = from; i < to; ++i) sum += v[std::size_t(i)];
                return sum / double(to - from);
            };
            const auto powerMean = [](const std::vector<double>& v, int from, int to) {
                double sum = 0;
                for (int i = from; i < to; ++i) sum += std::pow(10.0, v[std::size_t(i)] / 10.0);
                return 10.0 * std::log10(sum / double(to - from));
            };
            // (Its first bar left out: the transition into it lands there.)
            const auto median = [](std::vector<double> v, int from, int to) {
                std::vector<double> part(v.begin() + from, v.begin() + to);
                std::sort(part.begin(), part.end());
                return part[part.size() / 2];
            };
            const double firstNotes = mean(notes, 1, 9), lastNotes = mean(notes, 24, 32);
            const double firstLevel = median(levels, 1, 9), lastLevel = median(levels, 24, 32);
            (void) powerMean;
            const std::string tag = amount > 0.0f ? " (Drama 100 %)" : " (Classic)";
            s.check(lastNotes > 1.15 * firstNotes && lastLevel > firstLevel + 1.0,
                    "a 32-bar section grows rather than loops: its last 8 bars denser and louder than its first" + tag,
                    fmt(firstNotes) + " -> " + fmt(lastNotes) + " notes/s, " + fmt(firstLevel) + " -> " + fmt(lastLevel) + " dB");
            s.metric("Combat's first 8 bars, notes" + tag, firstNotes, " /s");
            s.metric("Combat's last 8 bars, notes" + tag, lastNotes, " /s");
            s.metric("Combat's last 8 bars over its first, level" + tag, lastLevel - firstLevel, " dB");
            s.check(variants.size() >= 2, "its variants change on the phrase lines" + tag, std::to_string(variants.size()) + " combinations");
        }
    }

    s.section("drama rework: old sessions and scenes as before");
    {
        // A project saved before the rework: version 4, no Drama instrument parameters, no
        // evolving sections or Drama instrument cells. Drama 0 (from before the Drama tier)
        // and Drama 100 % (a Drama tier session).
        for (float amount : { 0.0f, 1.0f }) {
            Host a;
            a.set("p1_type", float(Castle));
            a.set("drama", amount);
            arr::Arrangement arrangement;
            arrangement.sections[0] = arr::makeSection("Intro", 4, 0, 0.3f);
            arrangement.sections[1] = arr::makeSection("Climax", 4, 2, 0.95f);
            arrangement.count = 2;
            a.p->arrangement.set(arrangement);
            juce::MemoryBlock block;
            a.p->getStateInformation(block);
            auto xml = juce::AudioProcessor::getXmlFromBinary(block.getData(), int(block.getSize()));
            xml->setAttribute("version", 4);
            for (auto* child = xml->getFirstChildElement(); child != nullptr;) {
                auto* next = child->getNextElement();
                const auto id = child->getStringAttribute("id");
                bool drop = false;
                for (int p = PDramaFirst; p < PCount; ++p) drop |= id == specs[std::size_t(p)].id;
                if (drop) xml->removeChildElement(child, true);
                child = next;
            }
            if (auto* tree = xml->getChildByName("Arrangement"))
                for (auto* section : tree->getChildIterator()) {
                    section->removeAttribute("dramaCells");
                    section->removeAttribute("evolve");
                }
            juce::MemoryBlock old;
            juce::AudioProcessor::copyXmlToBinary(*xml, old);
            Host b;
            b.p->setStateInformation(old.getData(), int(old.getSize()));
            const bool off = b.get("p1_e7_on") < 0.5f && b.get("p2_e9_on") < 0.5f;
            const auto loaded = b.p->arrangement.get();
            s.check(loaded.count == 2 && !loaded.sections[0].evolve && loaded.sections[0].bars == 4 && loaded.sections[1].cells[0][DramaDrums] == arr::Cell {}
                        && (amount > 0.0f ? !off && b.get("p1_e7_on") > 0.5f : off),
                    amount > 0.0f ? "a Drama tier project (Drama above 0) opens with its Drama instruments on (they are its drums, ostinato and hits)"
                                  : "a project from before opens with the Drama instruments off and its arrangement as it was (lengths, no evolving)");
            if (amount <= 0.0f) {
                // Nothing reshapes it (at every position, as the arrangement says), and played,
                // the Drama instruments never wake.
                arr::Shaping shaping;
                shaping.types = b.p->slotTypes();
                bool same = true;
                for (double beat = 0; beat < 40.0; beat += 0.37) {
                    const auto raw = loaded.evaluate(beat);
                    auto shaped = raw;
                    arr::shape(shaped, loaded, shaping);
                    same = same && shaped.gain == raw.gain && shaped.variant == raw.variant && shaped.intensity == raw.intensity && shaped.doubling == raw.doubling
                           && shaped.drama == raw.drama && shaped.gear < 0.0f;
                }
                arrangeTest::play(b);
                b.render(12.0);
                s.check(same && !b.p->engine.placeSlot(0).drama().active() && dramaTest::dramaNotes(b) == 0,
                        "played, nothing reshapes it (at every position as the arrangement says) and the Drama instruments never wake");
            }
        }
        // The factory scenes from before the rework keep their arrangements and play no Drama instrument.
        int legacy = 0, same = 0, off = 0;
        for (int i = 0; i < scenes::factoryCount(); ++i) {
            const auto& sc = scenes::factory(i);
            if (sc.drama > 0.0f) continue;
            ++legacy;
            std::array<int, placeSlots> types {};
            for (int slot = 0; slot < placeSlots; ++slot) types[std::size_t(slot)] = sc.slots[std::size_t(slot)].type;
            const auto before = arr::autoArrangeLegacy(arr::makeTemplate(sc.templ, true), types, sc.seed);
            const auto now = scenes::arrangement(sc);
            bool match = now.count == before.count;
            for (int k = 0; match && k < now.count; ++k)
                match = now.sections[std::size_t(k)].bars == before.sections[std::size_t(k)].bars && !now.sections[std::size_t(k)].evolve;
            same += match ? 1 : 0;
            const auto values = scenes::worldValues(sc, WorldValues {});
            off += values[std::size_t(dramaParam(0, DrOn))] < 0.5f ? 1 : 0;
        }
        s.check(legacy == 24 && same == 24 && off == 24, "the 24 factory scenes from before keep their short, unchanged arrangements and play no Drama instrument",
                std::to_string(same) + " / " + std::to_string(off) + " of " + std::to_string(legacy));
    }
}

// Development aid (--only dramaprobe): Instrument mode on the Drama instruments.
void probeDramaInstruments() {
    for (int e : { int(DramaDrums), int(DramaOstinato), int(DramaHits) }) {
        auto quiet = instTest::ihost(inst::Id { Castle, e }, false, false);
        const auto q = quiet.render(2.0);
        std::cout << "element " << e << " silent " << measure(q, 0.5).rmsDb << " dB, active " << quiet.p->engine.placeSlot(0).drama().active()
                  << " started " << quiet.p->engine.placeSlot(0).drama().started() << " slot0 type " << quiet.p->engine.placeSlot(0).playingType() << "\n";
        std::cout << "  from 0: " << measure(q, 0.0).rmsDb << "\n";
        for (float root : { 0.0f, 6.0f }) {
            auto r = instTest::ihost(inst::Id { Castle, e }, false, false);
            r.set("root", root);
            r.set("inst_drone", 0.0f);
            r.render(0.1);
            keysTest::note(r, 57, 100);
            const auto heldR = r.render(1.5);
            const auto pcR = chroma(heldR, 0.05, 40.0, 5000.0, 32768);
            int bestR = 0;
            for (int c = 1; c < 12; ++c)
                if (pcR[std::size_t(c)] > pcR[std::size_t(bestR)]) bestR = c;
            keysTest::off(r, 57);
            r.render(5.0);
            std::cout << "  root " << root << ": strongest " << bestR << ", after " << measure(r.render(1.0), 0.0).rmsDb << "\n";
        }
        auto h = instTest::ihost(inst::Id { Castle, e }, false, false);
        h.render(0.1);
        keysTest::note(h, 57, 100);
        const auto held = h.render(1.5);
        keysTest::off(h, 57);
        h.render(4.0);
        const auto after = h.render(1.0);
        const auto pc = chroma(held, 0.05, 40.0, 5000.0, 32768);
        int best = 0;
        for (int c = 1; c < 12; ++c)
            if (pc[std::size_t(c)] > pc[std::size_t(best)]) best = c;
        std::cout << "  held " << measure(held, 0.0).rmsDb << " dB, after " << measure(after, 0.0).rmsDb << " dB, strongest class " << best << " started "
                  << h.p->engine.placeSlot(0).drama().started() << "\n";
    }
}
// Development aid (--only dramatrims): the Drama instruments' held levels (untrimmed).
void probeDramaTrims() {
    for (int e : { int(DramaDrums), int(DramaOstinato), int(DramaHits) }) {
        std::vector<double> levels;
        for (int t = NoPlace + 1; t < PlaceTypeCount; ++t) levels.push_back(instTest::heldLevel({ t, e }, false));
        auto sorted = levels;
        std::sort(sorted.begin(), sorted.end());
        std::cout << "element " << e << " median " << sorted[sorted.size() / 2] << " min " << sorted.front() << " max " << sorted.back() << " trim "
                  << std::round((-28.0 - sorted[sorted.size() / 2]) * 2.0) / 2.0 << "\n";
    }
}
