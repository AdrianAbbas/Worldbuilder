// The orchestration pass (2026-11-05): the Place's own instruments level as Drama rises, the
// drama parts under them; arrangements of several Places orchestrated (a part budget per
// section, one kit, one bass, one lead); the Drama percussion tight and dry; long cues by
// time and the Length control. Measured, not eyeballed. Group "orchestra".
// (Included after MixProbe.inc: it uses its helpers.)

namespace orchTest {

using namespace loud;
using namespace mixprobe;

const std::array<int, placeSlots> developerTypes { Palace, Desert, Beach, NoPlace };

// The developer's world: a Palace, a Desert and a Beach, the host at `bpm`.
Host developerWorld(float dramaAmount, double bpm) {
    Host h;
    for (int slot = 0; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(developerTypes[std::size_t(slot)]));
    h.set("drama", dramaAmount);
    h.set("intensity", 0.5f);
    h.playHead.bpm = bpm;
    return h;
}

// A template as ARRANGE builds it now: fitted to the Length at the tempo, Auto-arranged for the
// Places, with the Battle template's arcs (as Auto-arrange gives them while Drama plays).
arr::Arrangement arranged(int which, const std::array<int, placeSlots>& types, double bpm, float length = arr::lengthDefault) {
    auto a = arr::autoArrange(arr::makeTemplate(which, arr::lengthSeconds(length), bpm), types, 7);
    arr::dramatize(a, 7);
    return a;
}

// The default new arrangement: ARRANGE's "+" five times (Intro, Build, Climax, Wind-down, Outro)
// at the Length and tempo, Auto-arranged with its arcs.
arr::Arrangement plusDefault(const std::array<int, placeSlots>& types, double bpm, float length = arr::lengthDefault) {
    arr::Arrangement a;
    static constexpr std::array<int, 5> kinds { arr::KindIntro, arr::KindBuild, arr::KindPeak, arr::KindCalm, arr::KindOutro };
    for (int k = 0; k < 5; ++k) {
        a.sections[std::size_t(k)] = arr::makeSection(arr::presetNames[std::size_t(k)], arr::newSectionBars(kinds[std::size_t(k)], arr::lengthSeconds(length), bpm), k);
        a.count = k + 1;
    }
    a = arr::autoArrange(a, types, 7);
    arr::dramatize(a, 7);
    return a;
}

// Every two bars of every section: what sounds (after the shaping) at a Drama amount.
struct Census {
    int over = 0, under = 0, kits = 0, basses = 0, leads = 0, ostinatos = 0, points = 0;
    std::string text;
};
Census census(const arr::Arrangement& a, const std::array<int, placeSlots>& types, float dramaAmount) {
    Census c;
    arr::Shaping sh;
    sh.drama = dramaAmount;
    sh.types = types;
    for (int k = 0; k < a.count; ++k) {
        const auto& sec = a.sections[std::size_t(k)];
        const auto budget = arr::budgetOf(arr::kindOf(a, k));
        int most = 0, fewest = 99;
        for (int bar = 0; bar < sec.bars; bar += 2) {
            auto st = a.evaluate((double(a.startOf(k)) + double(bar) + 0.5) * 4.0);
            // (A Crossfade overlaps the parts leaving with those entering, by design.)
            if (st.transition == arr::TransCrossfade && st.transitionT >= 0.0f) continue;
            arr::shape(st, a, sh);
            const auto on = arr::sounding(st, a, sh);
            const int n = arr::countParts(on);
            most = std::max(most, n);
            // (The build-up's and the intro's first bars bring the parts in one by one; a fade.)
            if (double(bar) >= 0.85 * sec.bars) fewest = std::min(fewest, n);
            c.kits = std::max(c.kits, arr::countVoice(on, types, arr::VoiceKit));
            c.basses = std::max(c.basses, arr::countVoice(on, types, arr::VoiceBass));
            c.leads = std::max(c.leads, arr::countVoice(on, types, arr::VoiceLead));
            c.ostinatos = std::max(c.ostinatos, arr::countVoice(on, types, arr::VoiceOstinato));
            ++c.points;
        }
        if (most > budget.high) ++c.over;
        if (fewest < 99 && fewest < budget.low && arr::kindOf(a, k) != arr::KindOutro) ++c.under;
        c.text += std::string(sec.nameText()) + " " + std::to_string(fewest == 99 ? most : fewest) + "-" + std::to_string(most) + " (" + std::to_string(budget.low)
                  + "-" + std::to_string(budget.high) + "); ";
    }
    return c;
}

Audio renderArrangement(Host& h, const arr::Arrangement& a, double seconds) {
    h.p->arrangement.set(a);
    arrangeTest::play(h);
    Audio out;
    for (double t = 0; t < seconds; t += 2.0) {
        const auto part = h.render(std::min(2.0, seconds - t));
        out.rate = part.rate;
        out.l.insert(out.l.end(), part.l.begin(), part.l.end());
        out.r.insert(out.r.end(), part.r.begin(), part.r.end());
    }
    return out;
}

} // namespace orchTest

void testOrchestra(Suite& s) {
    using namespace loud;
    using namespace mixprobe;
    using namespace orchTest;

    s.section("orchestra: the Place's own instruments stay level as Drama rises; the drama parts under them");
    {
        WorldEngine::probeLinear = true;
        double worstMain = 99, worstUp = -99, worstDown = 99;
        std::string text;
        int okMain = 0, okAll = 0, total = 0;
        for (int type : builtPlaces()) {
            const std::string name = placeKeys[std::size_t(type)];
            bool mainOk = true, allOk = true;
            std::string line = name + ":";
            for (int t = 0; t < 3; ++t) {
                const float i = loud::tierIntensity[std::size_t(t)];
                // (16 s: the Drama tier's own chord progression averaged over.)
                const double d0 = integrated(renderWith(type, i, 0.0f, 0xffffffffu, 20.0), 4.0);
                const double main = integrated(renderWith(type, i, 1.0f, 0u, 20.0), 4.0);
                const double all = integrated(renderWith(type, i, 1.0f, 0xffffffffu, 20.0), 4.0);
                const double dm = main - d0, da = all - d0;
                worstMain = std::min(worstMain, dm);
                worstUp = std::max(worstUp, da);
                worstDown = std::min(worstDown, da);
                mainOk = mainOk && dm >= -1.0;
                allOk = allOk && da >= -1.5 && da <= (t == 2 ? 2.0 : 1.5);
                line += " t" + std::to_string(t + 1) + " main " + fmt(dm) + " all " + fmt(da);
            }
            ++total;
            okMain += mainOk ? 1 : 0;
            okAll += allOk ? 1 : 0;
            s.check(mainOk, name + ": its own instruments at Drama 100 % within -1 dB of Drama 0 (Tension, Action, Climax)", line);
            s.check(allOk, name + ": the whole slot at Drama 100 % within the window of Drama 0 (Tension and Action +-1.5 dB, Climax at most +2 dB)", line);
            text += line + "; ";
        }
        WorldEngine::probeLinear = false;
        s.metric("orchestra: the Places' own instruments, Drama 100 % against 0, the lowest", worstMain, " dB");
        s.metric("orchestra: the whole slot, Drama 100 % against 0, the highest", worstUp, " dB");
        s.metric("orchestra: the whole slot, Drama 100 % against 0, the lowest", worstDown, " dB");
        s.metric("orchestra: Places with their own instruments level", double(okMain), (" of " + std::to_string(total)).c_str());
    }
    {
        // The developer's arrangement at 180 BPM, section by section: the three Places' own
        // instruments (the drama parts silenced) at Drama 100 % against the same at Drama 0.
        const auto a = plusDefault(developerTypes, 180.0);
        const double bar = 240.0 / 180.0, seconds = double(a.totalBars()) * bar;
        WorldEngine::probeLinear = true;
        const uint32_t keep = drama::Layer::partMask;
        auto h0 = developerWorld(0.0f, 180.0);
        const auto d0 = renderArrangement(h0, a, seconds);
        drama::Layer::partMask = 0u;
        auto hm = developerWorld(1.0f, 180.0);
        const auto main = renderArrangement(hm, a, seconds);
        drama::Layer::partMask = keep;
        auto ha = developerWorld(1.0f, 180.0);
        const auto all = renderArrangement(ha, a, seconds);
        WorldEngine::probeLinear = false;
        bool mainOk = true, allOk = true;
        std::string text;
        arr::Shaping sh;
        sh.drama = 1.0f;
        sh.types = developerTypes;
        for (int k = 0; k < a.count; ++k) {
            const auto& sec = a.sections[std::size_t(k)];
            const double from = double(a.startOf(k)) * bar + 2.0 * bar, to = double(a.startOf(k) + sec.span()) * bar - 2.0 * bar;
            if (to - from < 4.0 * bar) continue;
            const double l0 = integrated(d0, from, to), lm = integrated(main, from, to), la = integrated(all, from, to);
            const int tier = arr::sectionTier(a, k, sh);
            // (A calm section thins out as Drama rises, by design: its parts leave, none gets quieter.)
            if (tier > drama::Calm) {
                mainOk = mainOk && lm - l0 >= -1.0;
                allOk = allOk && la - l0 <= (tier == drama::Climax ? 2.0 : 1.5);
            }
            text += std::string(sec.nameText()) + " (tier " + std::to_string(tier) + ") main " + fmt(lm - l0) + " all " + fmt(la - l0) + "; ";
        }
        s.check(mainOk, "the developer's arrangement (Palace, Desert, Beach, 180 BPM): every dramatic section's own instruments within -1 dB of Drama 0", text);
        s.check(allOk, "the same: no dramatic section more than the window above Drama 0 (+1.5 dB; a Climax +2 dB)", text);
    }

    s.section("orchestra: three Places in an arrangement, within the budget, one kit, one bass, one lead");
    {
        bool ok = true;
        int checked = 0;
        for (double bpm : { 120.0, 180.0 })
            for (int which = -1; which < arr::TemplateCount; ++which) {
                const auto a = which < 0 ? plusDefault(developerTypes, bpm) : arranged(which, developerTypes, bpm);
                for (float d : { 0.0f, 0.5f, 1.0f }) {
                    const auto c = census(a, developerTypes, d);
                    const bool good = c.over == 0 && c.under == 0 && c.kits <= 1 && c.basses <= 1 && c.leads <= 1 && c.ostinatos <= 1;
                    ok = ok && good;
                    ++checked;
                    if (!good || (bpm == 180.0 && d == 1.0f && which <= 0))
                        s.check(good, std::string(which < 0 ? "the default arrangement" : arr::templateNames[std::size_t(which)]) + " at " + fmt(bpm) + " BPM, Drama "
                                          + fmt(100.0 * d) + " %: parts within the budget, at most one kit, bass, lead and ostinato",
                                c.text + " kits " + std::to_string(c.kits) + ", basses " + std::to_string(c.basses) + ", leads " + std::to_string(c.leads) + ", ostinatos "
                                    + std::to_string(c.ostinatos));
                }
            }
        s.check(ok, "every template and the default arrangement, at 120 and 180 BPM and Drama 0 / 50 / 100 %: within the budget, one kit, bass, lead, ostinato",
                std::to_string(checked) + " arrangements and Drama amounts");
        // The lead passes between the Places; the others' space is thinned.
        const auto a = plusDefault(developerTypes, 180.0);
        std::set<int> leads;
        bool focus = true;
        for (int k = 0; k < a.count; ++k) {
            leads.insert(a.sections[std::size_t(k)].lead);
            auto st = a.evaluate((double(a.startOf(k)) + 4.5) * 4.0);
            arr::Shaping sh;
            sh.drama = 1.0f;
            sh.types = developerTypes;
            arr::shape(st, a, sh);
            for (int slot = 0; slot < 3; ++slot)
                focus = focus && std::abs(st.focus[std::size_t(slot)] - (slot == a.sections[std::size_t(k)].lead ? 1.0f : 0.5f)) < 1.0e-6f;
        }
        s.check(leads.size() == 3, "the lead passes between the three Places across the sections", std::to_string(leads.size()) + " Places lead");
        s.check(focus, "the Places that do not lead keep half of their space (World Space and reverb sends)");
        // The space thinned: the slot's World Space send.
        {
            auto h = developerWorld(0.0f, 120.0);
            h.set("environment", -60.0f);
            auto plain = a;
            for (int k = 0; k < plain.count; ++k) plain.sections[std::size_t(k)].orchestrated = false;
            h.p->arrangement.set(plain);
            arrangeTest::play(h);
            h.render(6.0);
            const float before = h.p->engine.placeSlot(1).spaceSend();
            auto g = developerWorld(0.0f, 120.0);
            g.set("environment", -60.0f);
            g.p->arrangement.set(a);
            arrangeTest::play(g);
            g.render(6.0);
            const int lead = a.sections[0].lead, other = lead == 1 ? 2 : 1;
            const float leadSend = g.p->engine.placeSlot(lead).spaceSend(), otherSend = g.p->engine.placeSlot(other).spaceSend();
            s.check(otherSend < 0.6f * before && leadSend > 0.0f, "a Place that does not lead sends about half as much to the World Space",
                    fmt(otherSend) + " against " + fmt(before) + " unorchestrated (the lead " + fmt(leadSend) + ")");
        }
        // Before: the arrangement as ARRANGE's "+" made it (every instrument of every Place).
        const auto c = census(plusArrangement(), developerTypes, 1.0f);
        s.metric("before: the developer's arrangement, the most kits at once", double(c.kits), "");
        std::cout << "  [info] before: " << c.text << "\n";
    }
    {
        // Orchestrate in ARRANGE: an arrangement of the composer's own (every instrument on),
        // one click, one undo step; an arrangement left alone keeps its cells.
        Host h;
        for (int slot = 0; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(developerTypes[std::size_t(slot)]));
        const auto mine = plusArrangement();
        h.p->arrangement.set(mine);
        std::unique_ptr<juce::AudioProcessorEditor> editor(h.p->createEditor());
        auto* view = dynamic_cast<WorldbuilderEditor*>(editor.get());
        auto& arrange = view->arrangeView();
        arrange.refresh();
        const auto kept = h.p->arrangement.get();
        s.check(kept == mine, "an existing arrangement keeps its cells as set (nothing orchestrates it on its own)");
        arrange.orchestrate();
        const auto done = h.p->arrangement.get();
        const auto c = census(done, developerTypes, 1.0f);
        bool lengths = done.count == mine.count;
        for (int k = 0; k < done.count && lengths; ++k) lengths = done.sections[std::size_t(k)].bars == mine.sections[std::size_t(k)].bars;
        s.check(c.kits <= 1 && c.basses <= 1 && c.leads <= 1 && c.over == 0 && lengths, "ORCHESTRATE: the composer's arrangement within the budget by voice, its lengths kept",
                c.text);
        h.p->arrangement.undo();
        s.check(h.p->arrangement.get() == mine, "ORCHESTRATE undoes in one step");
        // Saved and reloaded: orchestrated and its lead.
        h.p->arrangement.set(done);
        const auto back = ArrangementStore::fromTree(ArrangementStore::toTree(done));
        bool same = back == done;
        s.check(same, "an orchestrated arrangement saves and loads (its lead and orchestration)");
    }

    s.section("orchestra: the Drama percussion tight, dry and under the melodic parts");
    {
        WorldEngine::probeLinear = true;
        bool tailOk = true, levelOk = true, dropOk = true;
        std::string text;
        for (int type : { Palace, Desert, Beach, Castle, Spaceport, Factory }) {
            std::array<double, 2> tail {}, diff {}, sub {};
            for (int after = 0; after < 2; ++after) {
                drama::Layer::mixPass = after == 1;
                const auto p = renderWith(type, 1.0f, 1.0f, 0u);
                const auto perc = minus(renderWith(type, 1.0f, 1.0f, percMask()), p);
                const auto mel = renderWith(type, 1.0f, 1.0f, melodicMask());
                diff[std::size_t(after)] = integrated(perc, 4.0) - integrated(mel, 4.0);
                sub[std::size_t(after)] = subShare(perc, 4.0);
                const auto el = sparseDrums(type);
                const auto ps = renderWith(type, 1.0f, 1.0f, 0u, 12.0, &el);
                tail[std::size_t(after)] = tailRatio(minus(renderWith(type, 1.0f, 1.0f, drumsMask(), 12.0, &el), ps), 4.0);
            }
            drama::Layer::mixPass = true;
            // (What rings after 250 ms is mostly the big drums' own decay, not a room.)
            tailOk = tailOk && tail[1] <= -10.0 && tail[1] <= tail[0] - 6.0;
            levelOk = levelOk && diff[1] <= -1.0 && diff[1] >= -7.0;
            dropOk = dropOk && diff[0] - diff[1] >= 3.0;
            text += std::string(placeKeys[std::size_t(type)]) + " tail " + fmt(tail[0]) + " -> " + fmt(tail[1]) + " dB, against the melodic " + fmt(diff[0]) + " -> " + fmt(diff[1])
                    + " dB, below 90 Hz " + fmt(sub[0]) + " -> " + fmt(sub[1]) + " dB; ";
            s.metric(std::string("percussion ") + placeKeys[std::size_t(type)] + ": lower against the melodic parts than before by", diff[0] - diff[1], " dB");
        }
        WorldEngine::probeLinear = false;
        s.check(tailOk, "the Drama drums dry: their tail (250-900 ms against the first 120 ms) at most -10 dB and 6 dB under before", text);
        s.check(levelOk, "the Drama percussion under the melodic parts at Climax (1 to 7 dB under them)", text);
        s.check(dropOk, "the Drama percussion at least 3 dB lower against the melodic parts than before", text);
    }

    s.section("orchestra: long cues by time (templates, Auto-arrange, the default arrangement)");
    {
        bool ok = true, ramps = true;
        std::string text;
        for (double bpm : { 120.0, 180.0 }) {
            for (int which = -1; which < arr::TemplateCount; ++which) {
                const auto a = which < 0 ? plusDefault(developerTypes, bpm) : arranged(which, developerTypes, bpm);
                const double sec = arr::secondsOf(a, bpm);
                ok = ok && sec >= 180.0;
                text += std::string(which < 0 ? "default" : arr::templateNames[std::size_t(which)]) + " " + fmt(sec) + " s; ";
                // A build-up layers across its whole length: the parts come in one by one, the
                // last after half of it, Score Intensity rising throughout.
                arr::Shaping sh;
                sh.drama = 1.0f;
                sh.types = developerTypes;
                for (int k = 0; k < a.count; ++k) {
                    if (arr::kindOf(a, k) != arr::KindBuild) continue;
                    const auto& s2 = a.sections[std::size_t(k)];
                    int last = -1, previous = -1;
                    float lastIntensity = -1;
                    bool rising = true;
                    for (int step = 0; step < 10; ++step) {
                        const double bar = (double(step) + 0.5) / 10.0 * s2.bars;
                        auto st = a.evaluate((double(a.startOf(k)) + bar) * 4.0);
                        arr::shape(st, a, sh);
                        const int n = arr::countParts(arr::sounding(st, a, sh));
                        if (n > previous && previous >= 0) last = step;
                        rising = rising && n >= previous && st.intensity >= lastIntensity - 1.0e-4f;
                        previous = n;
                        lastIntensity = st.intensity;
                    }
                    ramps = ramps && rising && last >= 5;
                }
            }
        }
        s.check(ok, "every template, Auto-arrange and the default arrangement last at least 3 minutes at 120 and at 180 BPM", text);
        s.check(ramps, "every build-up layers across its whole length (the parts one by one, the last after its middle; Score Intensity rising)");
        s.check(arr::maxBars >= 240, "sections may be up to 240 bars");
    }

    s.section("orchestra: the Length control (Short, Medium, Long, Epic and between)");
    {
        bool ok = true, proportions = true, kept = true;
        std::string text;
        for (double bpm : { 120.0, 180.0 })
            for (float p : { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f })
                for (int which = 0; which < arr::TemplateCount; ++which) {
                    const auto base = arr::makeTemplate(which, false);
                    const auto a = arr::fitLength(base, arr::lengthSeconds(p), bpm);
                    const double sec = arr::secondsOf(a, bpm), want = arr::lengthSeconds(p);
                    const bool good = std::abs(sec / want - 1.0) <= 0.10;
                    ok = ok && good;
                    if (!good || which == 0) text += std::string(arr::templateNames[std::size_t(which)]) + " " + fmt(bpm) + " BPM p" + fmt(p) + " " + fmt(sec) + " / " + fmt(want) + " s; ";
                    // The proportions: each section's share within a phrase (and 4 %) of the template's.
                    for (int k = 0; k < a.count; ++k) {
                        const double share = double(a.sections[std::size_t(k)].bars) / double(a.totalBars()), was = double(base.sections[std::size_t(k)].bars) / double(base.totalBars());
                        const int q = a.sections[std::size_t(k)].bars < 12 ? 4 : (a.sections[std::size_t(k)].bars < 64 ? 8 : 16);
                        proportions = proportions && std::abs(share - was) <= double(q) / double(a.totalBars()) + 0.04;
                        const auto& x = a.sections[std::size_t(k)];
                        const auto& y = base.sections[std::size_t(k)];
                        kept = kept && x.intensityStart == y.intensityStart && x.intensityEnd == y.intensityEnd && x.curve == y.curve && x.evolve == y.evolve
                               && x.transition == y.transition && x.bars % 4 == 0;
                    }
                }
        s.check(ok, "every Length setting gives its duration within 10 % (every template, 120 and 180 BPM)", text);
        s.check(proportions, "the sections keep their proportions (within a phrase)");
        s.check(kept, "their ramps, curves, evolution and transitions kept; every length a whole phrase (4, 8, 16 bars)");
        // In ARRANGE: one undoable step, the readout, saved with the session.
        Host h;
        h.playHead.bpm = 180;
        for (int slot = 0; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(developerTypes[std::size_t(slot)]));
        h.render(0.2);
        std::unique_ptr<juce::AudioProcessorEditor> editor(h.p->createEditor());
        auto* view = dynamic_cast<WorldbuilderEditor*>(editor.get());
        auto& arrange = view->arrangeView();
        arrange.refresh();
        arrange.applyTemplate(arr::ClassicArc);
        const auto medium = h.p->arrangement.get();
        const double mediumSeconds = arr::secondsOf(medium, 180.0);
        arrange.setCueLength(3.0f, true);
        const auto epic = h.p->arrangement.get();
        const double epicSeconds = arr::secondsOf(epic, 180.0);
        s.check(std::abs(epicSeconds / 480.0 - 1.0) <= 0.1 && std::abs(mediumSeconds / 210.0 - 1.0) <= 0.1,
                "ARRANGE's LENGTH: the Classic Arc at Medium and at Epic at 180 BPM", fmt(mediumSeconds) + " s, " + fmt(epicSeconds) + " s; " + arrange.cueLengthLabel().toStdString());
        h.p->arrangement.undo();
        s.check(h.p->arrangement.get() == medium, "the Length change undoes in one step");
        bool cells = true;
        for (int k = 0; k < epic.count; ++k)
            for (int slot = 0; slot < placeSlots; ++slot)
                for (int e = 0; e < slotElements; ++e)
                    cells = cells && epic.sections[std::size_t(k)].cells[std::size_t(slot)][std::size_t(e)].entry == medium.sections[std::size_t(k)].cells[std::size_t(slot)][std::size_t(e)].entry;
        s.check(cells, "scaling keeps every section's parts and transitions (the cells as they were)");
        h.p->setArrangeLength(2.4f);
        juce::MemoryBlock block;
        h.p->getStateInformation(block);
        Host g;
        g.p->setStateInformation(block.getData(), int(block.getSize()));
        s.check(std::abs(g.p->arrangeLength() - 2.4f) < 1.0e-4f, "the Length is saved with the session", fmt(g.p->arrangeLength()));
        Host fresh;
        s.check(fresh.p->arrangeLength() == arr::lengthDefault, "a new session starts at Medium");
    }

    s.section("orchestra: listening renders (before / after, Short / Long)");
    {
        // The developer's setup: a Palace, a Desert and a Beach, the default arrangement at
        // 180 BPM, Drama as in a Battle template. Before: the arrangement as ARRANGE's "+"
        // made it (8-bar sections, every instrument of every Place) with the mix of the Drama
        // loudness pass; after: the default arrangement now, orchestrated, with this pass's mix.
        const auto write = [&](const char* name, const Audio& a) { dramaTest::writeFlac(name, a.l, a.r, a.rate); };
        {
            drama::Layer::mixPass = false;
            const auto a = plusArrangement();
            auto h = developerWorld(1.0f, 180.0);
            const auto audio = renderArrangement(h, a, double(a.totalBars()) * 240.0 / 180.0 + 2.0);
            drama::Layer::mixPass = true;
            write("orchestration-before.flac", audio);
            s.metric("before: the developer's arrangement at 180 BPM", double(a.totalBars()) * 240.0 / 180.0, " s");
        }
        {
            const auto a = plusDefault(developerTypes, 180.0);
            auto h = developerWorld(1.0f, 180.0);
            const auto audio = renderArrangement(h, a, double(a.totalBars()) * 240.0 / 180.0 + 2.0);
            write("orchestration-after.flac", audio);
            s.metric("after: the default arrangement at 180 BPM", double(a.totalBars()) * 240.0 / 180.0, " s");
            s.check(truePeak(audio, 0.5) < -1.0 && measure(audio, 0).finite, "the after render: true peak below -1 dBFS", fmt(truePeak(audio, 0.5)) + " dBTP");
        }
        for (float length : { 0.0f, 2.0f }) {
            const auto a = arranged(arr::ClassicArc, developerTypes, 180.0, length);
            auto h = developerWorld(1.0f, 180.0);
            const auto audio = renderArrangement(h, a, double(a.totalBars()) * 240.0 / 180.0 + 2.0);
            write(length == 0.0f ? "length-short.flac" : "length-long.flac", audio);
        }
    }
}
