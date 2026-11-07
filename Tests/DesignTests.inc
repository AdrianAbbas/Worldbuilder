// The Drama design pass (2026-11-07): the orchestra scores arrangements live (a part budget, one
// lead, one bass, one kit at a time across every Place, whenever sections or Places are added or
// cells edited), the grid shows what plays, risers and hits mark arrivals only, the Drama
// percussion per world (orchestral, electronic, natural) in the Place's own space, the drums
// coming in last, the project's Drama setting. Group "design"; "familytrim" levels the new kits.
// Every "before" is the same build with drama::designPass false (as it played before the pass).
// (Included after OrchestraTests.inc: it uses its helpers.)

namespace designTest {

using namespace loud;
using namespace mixprobe;

std::string f1(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.1f", v);
    return b;
}
std::string f2(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.2f", v);
    return b;
}

const std::array<int, placeSlots> developerTypes { Palace, Temple, City, NoPlace };

// Switches the pass on or off for a scope (the "before" renders and checks).
struct Pass {
    bool keep = drama::designPass;
    explicit Pass(bool on) { drama::designPass = on; }
    ~Pass() { drama::designPass = keep; }
};

// The developer's hand-made arrangement: a Palace; "+" twice; a Temple added; "+" twice; a City
// added; "+" once (Intro, Build, Climax, Wind-down, Outro), through ARRANGE as the composer clicks
// it, the host at `bpm`, Drama `amount`, Score Intensity in the middle (every section follows its
// own: Drama as intensity). `steps` gets the played arrangement after each step.
struct HandMade {
    Host h;
    std::vector<std::pair<std::string, arr::Arrangement>> steps;
};
std::unique_ptr<HandMade> handMade(float amount, double bpm = 120.0) {
    auto m = std::make_unique<HandMade>();
    auto& h = m->h;
    for (int slot = 0; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(NoPlace));
    h.set(slotId(0, PlType), float(Palace));
    h.set("drama", amount);
    h.set("intensity", 0.5f);
    h.playHead.bpm = bpm;
    // (The host's tempo as the processor sees it: one block.)
    h.render(0.05);
    std::unique_ptr<juce::AudioProcessorEditor> editor(h.p->createEditor());
    auto* view = dynamic_cast<WorldbuilderEditor*>(editor.get());
    auto& arrange = view->arrangeView();
    arrange.refresh();
    const auto step = [&](const std::string& what) { m->steps.emplace_back(what, h.p->arrangement.played()); };
    arrange.addSection();
    step("+ Intro (Palace)");
    arrange.addSection();
    step("+ Build (Palace)");
    h.set(slotId(1, PlType), float(Temple));
    step("Temple added");
    arrange.addSection();
    step("+ Climax");
    arrange.addSection();
    step("+ Wind-down");
    h.set(slotId(2, PlType), float(City));
    step("City added");
    arrange.addSection();
    step("+ Outro");
    editor.reset();
    return m;
}

std::string partName(int type, int e) { return std::string(elementSpec(type, e).name); }

// What sounds in each section (after the Drama shaping), sampled every two bars.
struct SectionParts {
    std::string text;
    int maxKits = 0, maxBasses = 0, maxLeads = 0, maxParts = 0, overBudget = 0, everything = 0;
};
SectionParts sectionParts(const arr::Arrangement& a, const std::array<int, placeSlots>& types, float amount, float intensity = 0.5f, bool print = true) {
    SectionParts out;
    arr::Shaping sh;
    sh.drama = amount;
    sh.intensityOffset = intensity - 0.5f;
    sh.types = types;
    int all = 0;
    for (int slot = 0; slot < placeSlots; ++slot)
        for (int e = 0; e < slotElements; ++e) all += types[std::size_t(slot)] != NoPlace && patternInfo(types[std::size_t(slot)], e).kind != NoPattern && !isDramaElement(e) ? 1 : 0;
    for (int k = 0; k < a.count; ++k) {
        const auto& sec = a.sections[std::size_t(k)];
        arr::Sounding any {};
        int most = 0;
        for (int bar = 0; bar < sec.bars; bar += 2) {
            auto st = a.evaluate((double(a.startOf(k)) + double(bar) + 0.5) * 4.0);
            if (st.transition == arr::TransCrossfade && st.transitionT >= 0.0f) continue;
            arr::shape(st, a, sh);
            const auto on = arr::sounding(st, a, sh);
            for (int slot = 0; slot < placeSlots; ++slot)
                for (int e = 0; e < slotElements; ++e) any[std::size_t(slot)][std::size_t(e)] = any[std::size_t(slot)][std::size_t(e)] || on[std::size_t(slot)][std::size_t(e)];
            const int n = arr::countParts(on);
            most = std::max(most, n);
            out.maxKits = std::max(out.maxKits, arr::countVoice(on, types, arr::VoiceKit));
            out.maxBasses = std::max(out.maxBasses, arr::countVoice(on, types, arr::VoiceBass));
            out.maxLeads = std::max(out.maxLeads, arr::countVoice(on, types, arr::VoiceLead));
            int place = 0;
            for (int slot = 0; slot < placeSlots; ++slot)
                for (int e = 0; e < placeElements; ++e) place += on[std::size_t(slot)][std::size_t(e)] ? 1 : 0;
            if (place >= all && all > 6) ++out.everything;
        }
        out.maxParts = std::max(out.maxParts, most);
        const auto budget = arr::budgetOf(arr::kindOf(a, k));
        if (most > budget.high) ++out.overBudget;
        if (!print) continue;
        out.text += "    " + std::string(sec.nameText()) + " (bars " + std::to_string(a.startOf(k) + 1) + "-" + std::to_string(a.startOf(k) + sec.span()) + ", at most "
                    + std::to_string(most) + " parts, budget " + std::to_string(budget.low) + "-" + std::to_string(budget.high) + "):";
        for (int slot = 0; slot < placeSlots; ++slot) {
            const int type = types[std::size_t(slot)];
            if (type == NoPlace) continue;
            std::string list;
            for (int e = 0; e < slotElements; ++e)
                if (any[std::size_t(slot)][std::size_t(e)])
                    list += (list.empty() ? "" : ", ") + partName(type, e) + " (" + arr::voiceNames[std::size_t(arr::voiceOf(type, e))] + ")"
                            + (sec.cells[std::size_t(slot)][std::size_t(e)].manual ? " [yours]" : "");
            out.text += " " + std::string(placeNames[std::size_t(type)]) + ": " + (list.empty() ? "rests" : list) + ";";
        }
        out.text += "\n";
    }
    return out;
}

// The risers (a Riser or Reverse swell transition, and the Drama layer's swells), the impacts and
// the stingers, with their bar numbers.
struct Moments {
    std::vector<double> risers, impacts, stingers, swells;
    std::string text;
};
void addLayerMoments(Moments& m, Host& h, double bpm, uint32_t& seen0, double startClock = 0) {
    const double perBar = h.rate * 60.0 / bpm * 4.0;
    for (int slot = 0; slot < placeSlots; ++slot) {
        const auto& layer = h.p->engine.placeSlot(slot).drama();
        for (uint32_t i = 0; i < layer.momentCount() && i < uint32_t(drama::Layer::momentSize); ++i) {
            const auto& x = layer.momentAt(i);
            const double bar = (double(x.clock) - startClock) / perBar + 1.0;
            if (x.kind == drama::Layer::MomentImpact) m.impacts.push_back(bar);
            else if (x.kind == drama::Layer::MomentSwell) m.swells.push_back(bar);
            else m.stingers.push_back(bar);
        }
    }
    (void) seen0;
}
std::string listBars(const std::vector<double>& v) {
    std::string s;
    std::vector<double> x = v;
    std::sort(x.begin(), x.end());
    for (double b : x) s += (s.empty() ? "" : ", ") + f1(b);
    return s.empty() ? "none" : s;
}
void transitionRisers(Moments& m, const arr::Arrangement& a) {
    for (int k = 0; k < a.count; ++k) {
        const auto& sec = a.sections[std::size_t(k)];
        if (!arr::isRiser(sec.transition) || !a.transitionAfter(k)) continue;
        const int bars = a.autoOrchestra && drama::designPass ? std::min<int>(sec.transitionLength, arr::riserMaxBars(sec.bars)) : sec.transitionLength;
        m.risers.push_back(double(a.startOf(k) + sec.span() - bars) + 1.0);
    }
}

// Renders an arrangement through (in two-second pieces), collecting the layer's moments.
Audio renderThrough(Host& h, const arr::Arrangement* set, double seconds) {
    if (set != nullptr) h.p->arrangement.set(*set);
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

// The section starts that are arrivals (a bigger section, or the beat back after a Drop).
std::vector<double> arrivals(const arr::Arrangement& a) {
    std::vector<double> bars;
    for (int k = 1; k < a.count; ++k)
        if (arr::biggerSection(a, k - 1, k) || (a.sections[std::size_t(k - 1)].transition == arr::TransDrop && a.transitionAfter(k - 1)))
            bars.push_back(double(a.startOf(k)) + 1.0);
    return bars;
}
bool nearAny(double bar, const std::vector<double>& at, double within = 0.3) {
    for (double b : at)
        if (std::abs(bar - b) <= within) return true;
    return false;
}

// The Drama kit alone for a Place at a Climax (BS.1770, the Place subtracted, the master linear).
double percussionLoudness(int type, double seconds = 20.0) {
    WorldEngine::probeLinear = true;
    // (The kit: the drums, snare and toms, metal and hands. The impacts and swells are compared
    // apart: the pass keeps them for the arrivals, so there are fewer of them by design.)
    using L = drama::Layer;
    const uint32_t kit = bit(L::PDrums) | bit(L::PDrumsOrch) | bit(L::PPerc) | bit(L::PMetal) | bit(L::PHands);
    const double v = integrated(minus(renderWith(type, 1.0f, 1.0f, kit, seconds), renderWith(type, 1.0f, 1.0f, 0u, seconds)), 4.0);
    WorldEngine::probeLinear = false;
    return v;
}

} // namespace designTest

// The kits of the new families levelled to the orchestral kit they replaced: prints the table
// for Source/Places/FamilyTrim.inc (each line the trim to set: the current one plus the miss).
void probeFamilyTrim() {
    using namespace designTest;
    // (The kit's share at a Climax moves the slot's safety limiting too: a step of the trim moves
    // the kit by about 35 %, so each run over-relaxes.)
    const float relax = float(juce::SystemStats::getEnvironmentVariable("WB_TRIM_RELAX", "1").getDoubleValue());
    std::cout << "[familytrim] the percussion alone at a Climax, before (orchestral) and after (its family); FamilyTrim.inc:\n";
    std::string table;
    for (int type = 0; type < PlaceTypeCount; ++type) {
        float trim = 0.0f;
        if (type != NoPlace) {
            double before = 0, after = 0;
            {
                Pass off(false);
                before = percussionLoudness(type);
            }
            after = percussionLoudness(type);
            trim = drama::familyTrimDb(type) + relax * float(before - after);
            std::cout << "  " << placeKeys[std::size_t(type)] << " (" << drama::familyNames[std::size_t(drama::familyOf(type))] << "): before " << f2(before)
                      << " after " << f2(after) << " LUFS, trim " << f2(drama::familyTrimDb(type)) << " -> " << f2(trim) << "\n";
        }
        char b[48];
        std::snprintf(b, sizeof b, "%.2ff,%s", double(std::round(trim * 100.0f) / 100.0f), type + 1 < PlaceTypeCount ? "\n" : "\n");
        table += b;
    }
    std::cout << "----- FamilyTrim.inc -----\n" << table << "-----\n";
}

void testDesign(Suite& s) {
    using namespace designTest;
    s.section("design: the orchestra scores the hand-made arrangement live (Palace + Temple + City, \"+ Section\" and Places added)");
    {
        const auto m = handMade(1.0f);
        bool everyStep = true;
        std::string text;
        for (const auto& [what, a] : m->steps) {
            const auto types = m->h.p->orchestraContext().types;
            const auto parts = sectionParts(a, types, 0.0f, 0.5f, false);
            const auto partsUp = sectionParts(a, types, 1.0f, 0.5f, false);
            const bool ok = a.autoOrchestra && parts.overBudget == 0 && partsUp.overBudget == 0 && parts.everything == 0 && partsUp.maxKits <= 1;
            everyStep = everyStep && ok;
            text += what + ": " + std::to_string(a.count) + " sections, at most " + std::to_string(std::max(parts.maxParts, partsUp.maxParts)) + " parts; ";
        }
        s.check(everyStep, "after every \"+ Section\" and every Place added the played arrangement keeps to the budget (never every cell on)", text);
        const auto types = m->h.p->orchestraContext().types;
        const auto& played = m->steps.back().second;
        const auto raw = m->h.p->arrangement.get();
        s.check(raw.autoOrchestra && raw.count == 5, "a new hand-made arrangement is scored live (AUTO on) and has five sections");
        for (float amount : { 0.0f, 0.5f, 1.0f }) {
            const auto c = sectionParts(played, types, amount);
            std::cout << "  [info] Drama " << int(amount * 100.0f) << " %, the parts sounding per section:\n" << c.text;
            s.check(c.maxKits <= 1 && c.maxBasses <= 1 && c.maxLeads <= 1, "Drama " + std::to_string(int(amount * 100.0f)) + " %: no section has two kits, two basses or two leads",
                    "kits " + std::to_string(c.maxKits) + " basses " + std::to_string(c.maxBasses) + " leads " + std::to_string(c.maxLeads));
            s.check(c.overBudget == 0 && c.everything == 0, "Drama " + std::to_string(int(amount * 100.0f)) + " %: every section within its part budget");
        }
        {
            // The same arrangement as it played before the pass (every cell on).
            Pass off(false);
            auto before = raw;
            const auto c = sectionParts(before, types, 1.0f);
            std::cout << "  [info] before the pass (as written: every cell on), Drama 100 %:\n" << c.text;
            s.metric("before: the most kits at once", double(c.maxKits));
            s.metric("before: the most parts at once", double(c.maxParts));
        }
        // A click is an override: it plays as clicked, is marked, and the rest stays scored.
        Host& h = m->h;
        std::unique_ptr<juce::AudioProcessorEditor> editor(h.p->createEditor());
        auto& arrange = dynamic_cast<WorldbuilderEditor*>(editor.get())->arrangeView();
        arrange.refresh();
        int slot = -1, element = -1;
        for (int sl = 0; sl < 3 && slot < 0; ++sl)
            for (int e = 0; e < placeElements; ++e)
                if (h.p->arrangement.played().sections[2].cells[std::size_t(sl)][std::size_t(e)].entry == arr::EntryOff && patternInfo(types[std::size_t(sl)], e).kind != NoPattern) {
                    slot = sl;
                    element = e;
                    break;
                }
        if (slot >= 0) {
            arrange.clickCell(2, slot, element);
            const auto after = h.p->arrangement.played();
            const auto& cell = after.sections[2].cells[std::size_t(slot)][std::size_t(element)];
            s.check(cell.entry != arr::EntryOff && cell.manual && h.p->arrangement.get().sections[2].cells[std::size_t(slot)][std::size_t(element)].manual,
                    "a click on a resting cell turns it on as an override (kept, marked) in the scored arrangement");
            // Saved and loaded: the override and AUTO stay.
            juce::MemoryBlock state;
            h.p->getStateInformation(state);
            Host g;
            g.p->setStateInformation(state.getData(), int(state.getSize()));
            const auto loaded = g.p->arrangement.get();
            s.check(loaded.autoOrchestra && loaded.sections[2].cells[std::size_t(slot)][std::size_t(element)].manual, "AUTO and the overrides are saved with the project");
            h.p->arrangement.undo();
            s.check(!h.p->arrangement.get().sections[2].cells[std::size_t(slot)][std::size_t(element)].manual, "an override undoes in one step");
        }
        // Drama 0: only the composer's parts (the Drama instruments rest), the budget kept.
        const auto zero = sectionParts(played, types, 0.0f, 0.5f, false);
        s.check(zero.maxKits <= 1 && zero.overBudget == 0, "Drama 0: the hand-made arrangement is scored too (one kit, the budget)");
    }

    s.section("design: risers, impacts and stingers mark arrivals");
    {
        // Every template, Auto-arranged with its arcs, at 120 and 180 BPM: the rules.
        int bad = 0, checked = 0;
        std::string text;
        for (double bpm : { 120.0, 180.0 })
            for (int t = 0; t < arr::TemplateCount; ++t) {
                auto a = arr::autoArrange(arr::makeTemplate(t, arr::lengthSeconds(arr::lengthDefault), bpm), developerTypes, 7);
                arr::dramatize(a, 7);
                for (int k = 0; k < a.count; ++k) {
                    const auto& sec = a.sections[std::size_t(k)];
                    if (!arr::isRiser(sec.transition) || !a.transitionAfter(k)) continue;
                    ++checked;
                    const int next = a.nextOf(k);
                    const bool previousRiser = k > 0 && arr::isRiser(a.sections[std::size_t(k - 1)].transition) && a.transitionAfter(k - 1);
                    const bool ok = sec.bars >= 16 && arr::biggerSection(a, k, next) && !previousRiser && sec.transitionLength <= arr::riserMaxBars(sec.bars);
                    if (!ok) {
                        ++bad;
                        text += std::string(arr::templateNames[std::size_t(t)]) + " " + sec.nameText() + "; ";
                    }
                }
            }
        s.check(bad == 0, "every template's risers: only into a bigger section, never two in a row, none out of a section under 16 bars, at most 3 bars",
                std::to_string(checked) + " risers, " + std::to_string(bad) + " against the rules " + text);
        // The Drama hits' own patterns: no impacts or swells on the phrase lines.
        int phraseHits = 0;
        for (int type = 1; type < PlaceTypeCount; ++type)
            for (int tier = drama::Tension; tier <= drama::Climax; ++tier)
                for (int v = 0; v < pat::variantCount; ++v) {
                    GenInputs in;
                    in.tier = tier;
                    in.key = 11;
                    pat::Pattern p;
                    drama::generate(type, DramaHits, v, in, p);
                    for (int i = 0; i < p.count; ++i) phraseHits += p.notes[std::size_t(i)].degree == drama::RowImpact || p.notes[std::size_t(i)].degree == drama::RowSwell ? 1 : 0;
                }
        s.check(phraseHits == 0, "the Drama hits' patterns hold no impacts or swells (the arrivals bring them, once)", std::to_string(phraseHits) + " found");
    }

    s.section("design: the percussion per world, in the Place's own space");
    {
        int wrong = 0;
        std::string text;
        for (int type = 1; type < PlaceTypeCount; ++type) {
            auto h = place(type, 1.0f, 1.0f);
            h.render(3.0);
            const auto& layer = h.p->engine.placeSlot(0).drama();
            const int want = drama::familyOf(type);
            const bool fantasy = type == Castle || type == Village || type == Temple || type == Ruins || type == SeaShip || type == Palace || type == Tavern;
            const bool natural = type == Forest || type == Desert || type == Beach || type == Cavern;
            const int expect = fantasy ? drama::FamilyOrchestral : (natural ? drama::FamilyNatural : drama::FamilyElectronic);
            if (layer.percussionFamily() != want || want != expect) {
                ++wrong;
                text += std::string(placeKeys[std::size_t(type)]) + " ";
            }
            text += std::string(placeKeys[std::size_t(type)]) + " " + drama::familyNames[std::size_t(layer.percussionFamily())] + "; ";
        }
        s.check(wrong == 0, "fantasy and historical Places orchestral, sci-fi and modern electronic, natural Places their own", text);
        // The drums' room: within a range per Place (never dry, never washed out), shorter than the
        // layer's hall; measured: the room's share of the drums' energy.
        int out = 0;
        std::string rooms;
        for (int type = 1; type < PlaceTypeCount; ++type) {
            const auto r = drama::drumRoom(type);
            const double hall = 1.5 + 3.0 * double(drama::theme(type).hall);
            const bool ok = r.decay >= 0.4f && r.decay <= 1.2f && double(r.decay) < 0.5 * hall;
            if (!ok) ++out;
            rooms += std::string(placeKeys[std::size_t(type)]) + " " + f2(r.wet) + "/" + f2(r.decay) + "s; ";
        }
        s.check(out == 0, "every Place's drums room: 0.4-1.2 s, under half the hall the melodic parts play in", rooms);
        int measuredOut = 0;
        std::string measured;
        for (int type : { Castle, Temple, Spaceport, Nebula, Forest, Cavern, Megacity }) {
            const auto drums = sparseDrums(type);
            WorldEngine::probeLinear = true;
            const auto withRoom = renderWith(type, 1.0f, 1.0f, drumsMask(), 10.0, &drums);
            drama::Layer::roomOff = true;
            const auto dry = renderWith(type, 1.0f, 1.0f, drumsMask(), 10.0, &drums);
            drama::Layer::roomOff = false;
            const auto placeOnly = renderWith(type, 1.0f, 1.0f, 0u, 10.0, &drums);
            const double share = integrated(minus(withRoom, dry), 4.0) - integrated(minus(dry, placeOnly), 4.0);
            WorldEngine::probeLinear = false;
            if (!(share > -14.0 && share < -6.0)) ++measuredOut;
            measured += std::string(placeKeys[std::size_t(type)]) + " " + f1(share) + " LU; ";
        }
        s.check(measuredOut == 0, "measured (BS.1770): the drums' room 6-14 LU under the dry drums (in the Place's space, never washed out, never dry)", measured);
        // The new kits levelled to the kit they replaced (the calibrated balance holds).
        int levelOut = 0;
        std::string levels;
        for (int type : { Spaceport, Megacity, Factory, Military, Nebula, Forest, Desert, Beach, Cavern, Castle, Temple, Palace }) {
            double before = 0;
            {
                Pass off(false);
                before = percussionLoudness(type, 16.0);
            }
            const double after = percussionLoudness(type, 16.0);
            if (std::abs(after - before) > 1.0) ++levelOut;
            levels += std::string(placeKeys[std::size_t(type)]) + " " + f2(after - before) + " dB; ";
        }
        s.check(levelOut == 0, "each Place's Drama kit (its new sounds, its room) within 1 dB of what it was (the kit alone at a Climax)", levels);
    }

    s.section("design: Drama changes gear (the Place first, the drums last); Drama 0, old projects and Drama Off as before");
    {
        // At Tension the Drama drums rest; from Action on they play.
        int tensionDrums = 0, actionDrums = 0;
        for (int type : { Castle, Megacity, Forest }) {
            auto t = place(type, loud::tierIntensity[0], 1.0f);
            t.render(10.0);
            auto a = place(type, loud::tierIntensity[1], 1.0f);
            a.render(10.0);
            const auto count = [](Host& h) {
                int n = 0;
                const auto& l = h.p->engine.placeSlot(0).drama();
                for (uint32_t i = 0; i < l.loggedCount() && i < uint32_t(drama::Layer::logSize); ++i) n += l.loggedAt(i).element == DramaDrums ? 1 : 0;
                return n;
            };
            tensionDrums += count(t);
            actionDrums += count(a);
        }
        s.check(tensionDrums == 0 && actionDrums > 0, "the Drama drums come in last: none at Tension, playing from Action",
                std::to_string(tensionDrums) + " strokes at Tension, " + std::to_string(actionDrums) + " at Action");
        // Drama 0: every Place bit for bit as before the pass (free play).
        bool same = true;
        std::string differ;
        for (int type = 1; type < PlaceTypeCount; ++type) {
            Audio x, y;
            {
                Pass off(false);
                auto h = place(type, 0.9f, 0.0f);
                x = h.render(4.0);
            }
            auto h = place(type, 0.9f, 0.0f);
            y = h.render(4.0);
            bool eq = x.l.size() == y.l.size();
            for (std::size_t i = 0; eq && i < x.l.size(); ++i) eq = x.l[i] == y.l[i] && x.r[i] == y.r[i];
            if (!eq) differ += std::string(placeKeys[std::size_t(type)]) + " ";
            same = same && eq;
        }
        s.check(same, "Drama 0: every Place bit for bit as before the pass", differ);
        // An old project's arrangement (not scored live) at Drama 0: bit for bit as before.
        {
            auto a = mixprobe::plusArrangement();
            a.sections[0].transition = arr::TransRiser;
            a.sections[0].transitionLength = 4;
            Audio x, y;
            {
                Pass off(false);
                auto h = orchTest::developerWorld(0.0f, 120.0);
                x = renderThrough(h, &a, 30.0);
            }
            auto h = orchTest::developerWorld(0.0f, 120.0);
            y = renderThrough(h, &a, 30.0);
            bool eq = x.l.size() == y.l.size();
            for (std::size_t i = 0; eq && i < x.l.size(); ++i) eq = x.l[i] == y.l[i] && x.r[i] == y.r[i];
            s.check(eq && !h.p->arrangement.get().autoOrchestra, "an arrangement from an older project plays as saved at Drama 0, bit for bit (AUTO off)");
        }
        // Drama Off: the whole system off, the arrangement as written.
        {
            auto m = handMade(1.0f);
            auto& h = m->h;
            h.p->setDramaSystem(false);
            const auto played = h.p->arrangement.played();
            const auto raw = h.p->arrangement.get();
            bool asWritten = played.count == raw.count;
            for (int k = 0; k < raw.count && asWritten; ++k) asWritten = played.sections[std::size_t(k)].cells == raw.sections[std::size_t(k)].cells;
            h.set("intensity", 1.0f);
            renderThrough(h, nullptr, 20.0);
            bool silent = true;
            for (int slot = 0; slot < placeSlots; ++slot) silent = silent && !h.p->engine.placeSlot(slot).drama().active();
            s.check(asWritten && silent, "Drama Off: the arrangement plays as written (no orchestra) and no Drama layer plays at Drama 100 %");
            juce::MemoryBlock state;
            h.p->getStateInformation(state);
            Host g;
            g.p->setStateInformation(state.getData(), int(state.getSize()));
            s.check(!g.p->dramaSystem(), "the Drama setting is saved with the project");
            Host fresh;
            s.check(fresh.p->dramaSystem(), "a new project has Drama On");
        }
    }

    s.section("design: the listening renders and their moments (before / after)");
    {
        const bool renders = juce::SystemStats::getEnvironmentVariable("WORLDBUILDER_RENDERS", {}).isNotEmpty() || juce::SystemStats::getEnvironmentVariable("WB_DESIGN_RENDERS", {}).isNotEmpty();
        // 1. The hand-made Palace + Temple + City arrangement, Drama 100 % (as intensity).
        for (bool after : { false, true }) {
            Pass pass(after);
            auto m = handMade(1.0f);
            auto& h = m->h;
            const auto a = h.p->arrangement.played();
            const auto types = h.p->orchestraContext().types;
            const double seconds = arr::secondsOf(a, 120.0) + 4.0;
            const auto audio = renderThrough(h, nullptr, seconds);
            Moments mo;
            uint32_t seen = 0;
            addLayerMoments(mo, h, 120.0, seen);
            transitionRisers(mo, a);
            const auto parts = sectionParts(a, types, 1.0f);
            const std::string tag = after ? "after" : "before";
            std::cout << "  [info] hand-made Palace + Temple + City, " << tag << ": the parts per section (Drama 100 %):\n" << parts.text;
            int kitsEver = parts.maxKits;
            std::cout << "  [info] " << tag << ": at most " << kitsEver << " kit(s) at once; risers " << mo.risers.size() + mo.swells.size() << " (transition risers at bars "
                      << listBars(mo.risers) << "; layer swells at bars " << listBars(mo.swells) << "); impacts " << mo.impacts.size() << " (" << listBars(mo.impacts)
                      << "); stingers " << mo.stingers.size() << " (" << listBars(mo.stingers) << ")\n";
            if (after) {
                s.check(parts.maxKits <= 1 && parts.maxBasses <= 1 && parts.maxLeads <= 1, "hand-made, after: one kit, one bass, one lead at a time");
                const auto at = arrivals(a);
                bool marks = true;
                for (double b : mo.impacts) marks = marks && nearAny(b, at);
                for (double b : mo.stingers) marks = marks && nearAny(b, at);
                s.check(marks && mo.swells.empty(), "hand-made, after: impacts and stingers only on arrivals, no layer swells", "arrivals at bars " + listBars(at));
            }
            if (renders) dramaTest::writeFlac("design-handmade-palace-temple-city-" + juce::String(tag) + ".flac", audio.l, audio.r, audio.rate);
        }
        // 2. A sci-fi Place (the Megacity) and a fantasy Place (the Castle) at Drama 0, 50 and 100 %
        // (Score Intensity 100 %, 16 s each).
        for (int type : { Megacity, Castle })
            for (bool after : { false, true }) {
                Pass pass(after);
                std::vector<float> l, r;
                std::string text;
                for (float amount : { 0.0f, 0.5f, 1.0f }) {
                    auto h = place(type, 1.0f, amount);
                    h.set("environment", 0.0f);
                    const auto audio = h.render(16.0);
                    recTest::append(l, r, audio, 0.0, 1.0);
                    Moments mo;
                    uint32_t seen = 0;
                    addLayerMoments(mo, h, h.p->engine.frame().bpm, seen);
                    text += "Drama " + std::to_string(int(amount * 100.0f)) + " %: " + f1(integrated(audio, 4.0)) + " LUFS, impacts " + std::to_string(mo.impacts.size())
                            + ", swells " + std::to_string(mo.swells.size()) + ", stingers " + std::to_string(mo.stingers.size()) + "; ";
                    if (after && amount > 0.0f)
                        s.check(mo.swells.empty() && mo.impacts.size() <= 2 && mo.stingers.size() <= 1,
                                std::string(placeKeys[std::size_t(type)]) + " at Drama " + std::to_string(int(amount * 100.0f)) + " %: at most one arrival's impacts and stinger in 16 s, no swells");
                }
                std::cout << "  [info] " << placeKeys[std::size_t(type)] << " " << (after ? "after" : "before") << ": " << text << "\n";
                if (renders)
                    dramaTest::writeFlac("design-" + juce::String(placeKeys[std::size_t(type)]) + "-drama-0-50-100-" + juce::String(after ? "after" : "before") + ".flac", l, r, 48000);
            }
        // 3. A three-minute template cue (the Battle template, Palace + Temple + City, through ARRANGE
        // at a Length of 3:00): the risers, impacts and stingers with their bar numbers.
        for (bool after : { false, true }) {
            Pass pass(after);
            Host h;
            for (int slot = 0; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(developerTypes[std::size_t(slot)]));
            h.set("drama", 1.0f);
            h.playHead.bpm = 120.0;
            h.render(0.05);
            h.p->setArrangeLength(float(std::log(180.0 / 60.0) / std::log(210.0 / 60.0)));
            std::unique_ptr<juce::AudioProcessorEditor> editor(h.p->createEditor());
            auto& arrange = dynamic_cast<WorldbuilderEditor*>(editor.get())->arrangeView();
            arrange.refresh();
            arrange.applyTemplate(arr::Battle);
            editor.reset();
            const auto a = h.p->arrangement.played();
            const double seconds = arr::secondsOf(a, 120.0);
            const auto audio = renderThrough(h, nullptr, seconds + 4.0);
            Moments mo;
            uint32_t seen = 0;
            addLayerMoments(mo, h, 120.0, seen);
            transitionRisers(mo, a);
            const std::string tag = after ? "after" : "before";
            std::string sections;
            for (int k = 0; k < a.count; ++k)
                sections += std::string(a.sections[std::size_t(k)].nameText()) + " " + std::to_string(a.startOf(k) + 1) + "-" + std::to_string(a.startOf(k) + a.sections[std::size_t(k)].span())
                            + (a.sections[std::size_t(k)].transition != arr::TransCut ? std::string(" -> ") + arr::transitionNames[std::size_t(a.sections[std::size_t(k)].transition)] : "")
                            + "; ";
            std::cout << "  [info] Battle template, 3:00 (" << f1(seconds) << " s), " << tag << ": " << sections << "\n";
            std::cout << "  [info] " << tag << ": risers " << mo.risers.size() + mo.swells.size() << " (transition risers at bars " << listBars(mo.risers) << "; layer swells at bars "
                      << listBars(mo.swells) << "); impacts " << mo.impacts.size() << " (" << listBars(mo.impacts) << "); stingers " << mo.stingers.size() << " ("
                      << listBars(mo.stingers) << ")\n";
            const auto parts = sectionParts(a, developerTypes, 1.0f);
            std::cout << "  [info] " << tag << ": the parts per section:\n" << parts.text;
            if (after) {
                const auto at = arrivals(a);
                bool marks = true;
                for (double b : mo.impacts) marks = marks && nearAny(b, at);
                for (double b : mo.stingers) marks = marks && nearAny(b, at);
                s.check(marks && mo.swells.empty() && mo.risers.size() <= std::size_t(std::max(1, a.count / 2)),
                        "the template cue, after: impacts and stingers only on arrivals, no layer swells, risers per the rules", "arrivals at bars " + listBars(at));
                s.check(parts.maxKits <= 1 && parts.maxBasses <= 1 && parts.maxLeads <= 1, "the template cue, after: one kit, one bass, one lead at a time");
            }
            if (renders) dramaTest::writeFlac("design-template-battle-3min-" + juce::String(tag) + ".flac", audio.l, audio.r, audio.rate);
        }
    }
}

// Development probe: the Drama percussion's components before / after the pass (--only percparts).
void probePercParts() {
    using namespace designTest;
    using L = drama::Layer;
    const std::array<std::pair<const char*, uint32_t>, 6> parts { { { "drums", bit(L::PDrums) | bit(L::PDrumsOrch) }, { "perc", bit(L::PPerc) }, { "metal", bit(L::PMetal) },
                                                                    { "hands", bit(L::PHands) }, { "impact+crash", bit(L::PImpact) | bit(L::PMetalHit) }, { "swell+low", bit(L::PSwell) | bit(L::PLow) } } };
    for (int type : { Castle, Village, Nebula, Spaceport }) {
        std::cout << placeKeys[std::size_t(type)] << ":";
        for (const auto& [name, mask] : parts) {
            double v[2] {};
            for (int after = 0; after < 2; ++after) {
                Pass pass(after == 1);
                WorldEngine::probeLinear = true;
                v[after] = integrated(minus(renderWith(type, 1.0f, 1.0f, mask, 16.0), renderWith(type, 1.0f, 1.0f, 0u, 16.0)), 4.0);
                WorldEngine::probeLinear = false;
            }
            std::cout << " " << name << " " << f2(v[0]) << " -> " << f2(v[1]) << ";";
        }
        std::cout << "\n";
    }
}
