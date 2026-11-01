// Instrument mode: any WORLDBUILDER instrument as a normal software instrument, played
// from the host's MIDI (no world, no patterns, no arrangement, the transport ignored).
// Included by WorldbuilderTests.cpp (after the keys tests, whose helpers it uses).

namespace instTest {
using namespace keysTest;

// A plugin in Instrument mode playing `id` (its own effect defaults), with a full scene
// behind it (the Castle and the Village with their patterns, the environment up) that
// must stay silent. Dry unless `wet`: no reverb or delay tails under what is measured.
Host ihost(inst::Id id, bool wet = false, bool scene = true) {
    Host h;
    if (scene) {
        h.set("p1_type", float(Castle));
        h.set("p2_type", float(Village));
        featureEnvironment(h);
        h.p->patterns.syncTypes(h.p->slotTypes());
    }
    h.p->setInstrument(id);
    h.p->setInstrumentMode(true);
    if (!wet) {
        h.set("inst_rev", 0.0f);
        h.set("inst_dly", 0.0f);
    }
    h.render(0.3);
    return h;
}
Host ihost(int type, int element, bool wet = false) { return ihost(inst::Id { type, element }, wet); }

// The note an instrument is tried on: A3, or A2 for the basses.
int noteFor(inst::Id id) { return inst::groupOf(id.type, id.element) == inst::Bass ? 45 : 57; }

// The largest sample-to-sample step (either channel) in [from, to) seconds.
double largestStep(const Audio& a, double from, double to) {
    double worst = 0;
    const auto begin = std::size_t(std::max(1.0, from * a.rate)), end = std::min(a.l.size(), std::size_t(to * a.rate));
    for (std::size_t i = begin; i < end; ++i) worst = std::max({ worst, double(std::abs(a.l[i] - a.l[i - 1])), double(std::abs(a.r[i] - a.r[i - 1])) });
    return worst;
}
} // namespace instTest

namespace instTest {
// The loudest 300 ms (dB RMS) of a held note: plucks peak at once, pads swell.
double loudest(const Audio& a) {
    double best = -240;
    for (double t = 0; t + 0.3 <= double(a.l.size()) / a.rate; t += 0.1) best = std::max(best, measure(a, t, t + 0.3).rmsDb);
    return best;
}
// One held note on `id` (dry, with or without the level trims): its loudest 300 ms.
double heldLevel(inst::Id id, bool trims) {
    auto h = ihost(id, false, false);
    h.p->levelTrims = trims;
    const bool kit = patternInfo(id.type, id.element).kind == Kit;
    note(h, kit ? 48 : noteFor(id), 100);
    return loudest(h.render(3.0));
}
} // namespace instTest

// --only instaudit: every instrument in Instrument mode, for sounds nobody played: with no
// note; after a release; a pitch class the key has but the note does not (Root F#, A played).
void auditInstruments() {
    using namespace instTest;
    const auto& catalog = inst::Catalog::get();
    for (int i = 0; i < catalog.count; ++i) {
        const auto id = catalog.entry[std::size_t(i)];
        const bool kit = patternInfo(id.type, id.element).kind == Kit;
        auto quiet = ihost(id, false, false);
        const double none = measure(quiet.render(2.0), 0.5).rmsDb;
        auto h = ihost(id, false, false);
        h.set("root", 6.0f);
        h.set("inst_drone", 0.0f);
        h.render(0.1);
        note(h, 57, 100);
        const auto held = h.render(1.5);
        off(h, 57);
        h.render(4.0);
        const double after = measure(h.render(1.0), 0.0).rmsDb;
        const auto pc = chroma(held, 0.05, 40.0, 5000.0, 32768);
        double best = 0;
        for (double e : pc) best = std::max(best, e);
        const double fs = 10.0 * std::log10(pc[6] / std::max(best, 1.0e-30) + 1.0e-30);
        const bool flag = none > -90.0 || after > -60.0 || (!kit && fs > -10.0);
        std::cout << (flag ? "  [audit] FLAG " : "  [audit] ok   ") << capture::trackName(id.type, id.element) << ": silent " << fmt(none) << ", after " << fmt(after)
                  << ", F# " << fmt(fs) << " dB" << (kit ? " (kit)" : "") << "\n";
    }
}

// --only insttrims: measures every instrument and prints Instruments/Trims.h's table.
void printInstrumentTrims() {
    using namespace instTest;
    std::cout << "inline constexpr std::array<std::array<float, 6>, 27> levelTrimDb { {\n    { 0, 0, 0, 0, 0, 0 },\n";
    for (int t = NoPlace + 1; t < PlaceTypeCount; ++t) {
        std::cout << "    { ";
        for (int e = 0; e < placeElements; ++e) {
            const inst::Id id { t, e };
            float trim = 0;
            if (id.valid()) trim = float(std::clamp(std::round((-28.0 - heldLevel(id, false)) * 2.0) / 2.0, -12.0, 12.0));
            std::cout << juce::String(trim, 1) << (e < placeElements - 1 ? "f, " : "f");
        }
        std::cout << " },   // " << placeNames[std::size_t(t)] << "\n";
    }
    std::cout << "} };\n";
}

void testInstrumentMode(Suite& s) {
    using namespace instTest;
    const auto& catalog = inst::Catalog::get();

    s.section("instrument mode: the switch, the catalog, the world silent");
    {
        Host h;
        s.check(!h.p->instrumentMode() && h.get("mode") == float(ModeWorld), "a new plugin starts in WORLD mode");
        s.check(specs[PMode].kind == Kind::Choice && juce::String(specs[PMode].choices) == "World|Instrument", "Mode is a host parameter: World or Instrument");
        int melodic = 0, kits = 0;
        std::array<int, inst::GroupCount> perGroup {};
        for (int i = 0; i < catalog.count; ++i) {
            const auto id = catalog.entry[std::size_t(i)];
            (patternInfo(id.type, id.element).kind == Kit ? kits : melodic)++;
            ++perGroup[std::size_t(inst::groupOf(id.type, id.element))];
        }
        s.check(catalog.count == (PlaceTypeCount - 1) * placeElements, "the catalog lists every instrument of every Place", std::to_string(catalog.count) + " instruments");
        bool everyGroup = true;
        std::string groups;
        for (int g = 0; g < inst::GroupCount; ++g) {
            everyGroup &= perGroup[std::size_t(g)] > 0;
            groups += std::string(inst::groupNames[std::size_t(g)]) + " " + std::to_string(perGroup[std::size_t(g)]) + ", ";
        }
        s.check(everyGroup, "every type group has instruments", groups);
        std::cout << "  [info] " << melodic << " melodic, " << kits << " kits; " << groups << "\n";
        s.check(catalog.step(catalog.entry[0], -1) == catalog.entry[std::size_t(catalog.count - 1)] && catalog.step(catalog.entry[0], 1) == catalog.entry[1],
                "previous and next step through the catalog (and wrap)");
        s.check(isInstrumentParam(PMode) && isInstrumentParam(PInstOctave) && moveOf(PInstTone) == Move::Live, "Instrument mode's parameters are the session's (no journey moves them)");
    }
    {
        // A busy world, then Instrument mode with no notes: nothing at all.
        Host h;
        h.set("p1_type", float(Castle));
        h.set("p2_type", float(Spaceport));
        featureEnvironment(h);
        h.p->patterns.syncTypes(h.p->slotTypes());
        h.render(3.0);
        const auto world = measure(h.render(1.0), 0.0);
        h.p->setInstrument({ Tavern, TavernPlace::LuteStrum });
        h.p->setInstrumentMode(true);
        h.render(0.3);
        const auto quiet = measure(h.render(2.0), 0.0);
        s.check(world.rmsDb > -50.0 && quiet.rmsDb < -100.0, "INSTRUMENT: no patterns, no environment, no world (no notes: silence)",
                fmt(world.rmsDb) + " -> " + fmt(quiet.rmsDb) + " dB");
        // Place Ambience on: the environment comes back under it.
        h.set("inst_ambience", 1.0f);
        h.render(1.0);
        const auto ambient = measure(h.render(1.5), 0.0);
        s.check(ambient.rmsDb > quiet.rmsDb + 40.0 && ambient.finite, "Place Ambience on: the world's environment plays under the instrument", fmt(ambient.rmsDb) + " dB");
        h.set("inst_ambience", 0.0f);
        h.render(0.5);
        const auto offAgain = measure(h.render(1.0), 0.0);
        s.check(offAgain.rmsDb < -90.0, "and off again: just the instrument", fmt(offAgain.rmsDb) + " dB");
        // The Arrangement and the transport do not apply.
        h.p->arrangement.set(arrangeTest::classicArc());
        h.set("playmode", float(PlayArrangement));
        h.playHead.position = 0;
        h.playHead.playing = true;
        const auto arranged = measure(h.render(2.0), 0.0);
        s.check(arranged.rmsDb < -100.0 && h.p->engine.arrangeSection.load() == -1, "with an arrangement and the host playing: still no patterns", fmt(arranged.rmsDb) + " dB");
        // Back to WORLD: everything again.
        h.set("playmode", float(PlayFree));
        h.playHead.playing = false;
        h.p->setInstrumentMode(false);
        h.render(3.0);
        s.check(measure(h.render(1.0), 0.0).rmsDb > world.rmsDb - 6.0, "WORLD again: the world as it was");
    }

    s.section("instrument mode: every instrument from host MIDI (audible, finite, in tune)");
    {
        int audible = 0, finite = 0, pitched = 0, inTune = 0;
        std::string quietList, offList, levels;
        double loudest = -240, softest = 240;
        for (int i = 0; i < catalog.count; ++i) {
            const auto id = catalog.entry[std::size_t(i)];
            auto h = ihost(id, false, false);
            const int key = noteFor(id);
            const bool kit = patternInfo(id.type, id.element).kind == Kit;
            note(h, kit ? 48 : key, 110);
            const auto a = h.render(1.6);
            off(h, kit ? 48 : key);
            const auto tail = h.render(0.6);
            const auto st = measure(a, 0.0);
            const bool fin = st.finite && measure(tail, 0.0).finite && st.peak <= 1.0;
            finite += fin ? 1 : 0;
            const bool heard = st.rmsDb > -60.0;
            audible += heard ? 1 : 0;
            const auto name = capture::trackName(id.type, id.element).toStdString();
            if (!heard) quietList += " " + name + " (" + fmt(st.rmsDb) + ")";
            loudest = std::max(loudest, st.rmsDb);
            softest = std::min(softest, st.rmsDb);
            levels += " " + name + " " + fmt(st.rmsDb) + ";";
            // In tune: the pitch class pressed carries the most energy (or within 3 dB of
            // it), 60 Hz - 5 kHz, from the note's start.
            if (!kit) {
                const auto pc = chroma(a, 0.05, 60.0, 5000.0, 32768);
                const int want = key % 12;
                double best = 0;
                for (double e : pc) best = std::max(best, e);
                const bool tuned = pc[std::size_t(want)] >= best * 0.5;
                const int group = inst::groupOf(id.type, id.element);
                // Textures and FX (wind, crowds, risers, sirens, birds) may be noise or glides:
                // reported, not judged.
                const bool judged = group != inst::Textures && group != inst::Fx;
                if (judged) {
                    ++pitched;
                    inTune += tuned ? 1 : 0;
                }
                if (!tuned) {
                    int strongest = 0;
                    for (int c = 1; c < 12; ++c)
                        if (pc[std::size_t(c)] > pc[std::size_t(strongest)]) strongest = c;
                    offList += " " + name + (judged ? "" : " [not judged]") + " (strongest class " + std::to_string(strongest) + ")";
                }
            }
        }
        s.check(audible == catalog.count, "every instrument is audible from a host note", std::to_string(audible) + " of " + std::to_string(catalog.count) + quietList);
        s.check(finite == catalog.count, "every instrument is finite and below full scale", std::to_string(finite) + " of " + std::to_string(catalog.count));
        s.check(inTune == pitched, "every pitched instrument sounds the key pressed (its pitch class strongest)",
                std::to_string(inTune) + " of " + std::to_string(pitched) + offList);
        if (!offList.empty()) std::cout << "  [info] off the pressed pitch class:" << offList << "\n";
        s.metric("instrument levels, loudest - softest (report only)", loudest - softest, " dB");
        std::cout << "  [info] levels (dB RMS, one note):" << levels << "\n";
    }

    s.section("instrument mode: stepping through the instruments keeps a similar loudness");
    {
        std::vector<double> raw, trimmed;
        for (int i = 0; i < catalog.count; ++i) {
            const auto id = catalog.entry[std::size_t(i)];
            raw.push_back(heldLevel(id, false));
            trimmed.push_back(heldLevel(id, true));
        }
        const auto spread = [](std::vector<double> v) {
            std::sort(v.begin(), v.end());
            // The middle 90 %: a few textures need more than the 12 dB a trim may give.
            return v[v.size() * 95 / 100] - v[v.size() * 5 / 100];
        };
        s.metric("loudness spread of the instruments, middle 90 %, without the trims (report only)", spread(raw), " dB");
        s.metric("loudness spread of the instruments, middle 90 %, with the trims (report only)", spread(trimmed), " dB");
        s.check(spread(trimmed) < 8.0 && spread(trimmed) < spread(raw) - 6.0, "the level trims keep 90 % of the instruments within 8 dB of each other",
                fmt(spread(raw)) + " -> " + fmt(spread(trimmed)) + " dB");
        double peak = -240;
        std::string loud;
        for (std::size_t i = 0; i < trimmed.size(); ++i) {
            peak = std::max(peak, trimmed[i]);
            if (trimmed[i] > -16.0) loud += " " + capture::trackName(catalog.entry[i].type, catalog.entry[i].element).toStdString() + " " + fmt(raw[i]) + "->" + fmt(trimmed[i]);
        }
        if (!loud.empty()) std::cout << "  [info] loudest:" << loud << "\n";
        s.check(peak < -12.0, "and no instrument's single note comes near full scale", fmt(peak) + " dB RMS at its loudest");
    }

    s.section("instrument mode: velocity, note length, sustain, pitch bend, mod wheel, octave");
    {
        for (const auto id : { inst::Id { Castle, CastlePlace::Horns }, inst::Id { Tavern, TavernPlace::LuteStrum }, inst::Id { Palace, PalacePlace::Harpsichord } }) {
            const auto name = capture::trackName(id.type, id.element).toStdString();
            auto soft = ihost(id), hard = ihost(id);
            note(soft, 57, 35);
            note(hard, 57, 127);
            const auto ss = measure(soft.render(1.0), 0.0), hs = measure(hard.render(1.0), 0.0);
            s.check(hs.rmsDb > ss.rmsDb + 3.0, name + ": velocity changes the level", fmt(hs.rmsDb - ss.rmsDb) + " dB");
        }
        // Note length and the pedal on the horns (held notes).
        auto released = ihost(Castle, CastlePlace::Horns), pedalled = ihost(Castle, CastlePlace::Horns);
        note(released, 57, 110);
        note(pedalled, 57, 110);
        cc(pedalled, 64, 127);
        const auto held = measure(released.render(0.6), 0.2);
        pedalled.render(0.6);
        off(released, 57);
        off(pedalled, 57);
        const auto ra = released.render(2.5), pa = pedalled.render(2.5);
        const auto rs = measure(ra, 1.5), ps = measure(pa, 1.5);
        s.check(rs.rmsDb < held.rmsDb - 20.0, "a released key ends its note", fmt(held.rmsDb) + " -> " + fmt(rs.rmsDb) + " dB");
        s.check(ps.rmsDb > rs.rmsDb + 15.0, "the sustain pedal holds it", fmt(ps.rmsDb - rs.rmsDb) + " dB");
        cc(pedalled, 64, 0);
        pedalled.render(2.5);
        s.check(measure(pedalled.render(0.5), 0.0).rmsDb < ps.rmsDb - 15.0, "lifting the pedal lets it go");
        // Pitch bend: full up is a tone on the horns.
        auto bent = ihost(Castle, CastlePlace::Horns);
        note(bent, 57, 110);
        bent.midi.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 0);
        const auto by = mono(bent.render(1.5), 0.6, 0.5);
        const double up = models::peakNear(by, 0, by.size(), 48000.0, hzOf(59));
        s.check(std::abs(cents(up, hzOf(59))) < 15.0, "pitch bend up bends a tone", fmt(up) + " Hz");
        auto straight = ihost(Castle, CastlePlace::Horns);
        note(straight, 57, 110);
        const auto sy = mono(straight.render(1.5), 0.6, 0.5);
        const double at = models::peakNear(sy, 0, sy.size(), 48000.0, hzOf(57));
        s.check(std::abs(cents(at, hzOf(57))) < 15.0, "unbent it plays at concert pitch (A3 = 220 Hz)", fmt(at) + " Hz");
        // The mod wheel: vibrato on the horns.
        auto still = ihost(Castle, CastlePlace::Horns), wobble = ihost(Castle, CastlePlace::Horns);
        note(still, 57, 110);
        note(wobble, 57, 110);
        cc(wobble, 1, 127);
        const auto sm = measure(still.render(2.0), 0.8), wm = measure(wobble.render(2.0), 0.8);
        s.check(difference(sm, wm) > 0.3, "the mod wheel changes the sound", fmt(difference(sm, wm)) + " dB");
        // Octave +1.
        auto shifted = ihost(Castle, CastlePlace::Horns);
        shifted.set("inst_octave", 1.0f);
        note(shifted, 57, 110);
        const auto oy = mono(shifted.render(1.5), 0.6, 0.5);
        const double octave = models::peakNear(oy, 0, oy.size(), 48000.0, hzOf(69));
        s.check(std::abs(cents(octave, hzOf(69))) < 15.0, "Octave +1 plays an octave up", fmt(octave) + " Hz");
        // Polyphony: a four-note chord on the harpsichord sounds every note.
        auto chord = ihost(Palace, PalacePlace::Harpsichord);
        for (int k : { 57, 61, 64, 69 }) note(chord, k, 100);
        const auto cy = mono(chord.render(1.0), 0.1, 0.5);
        int found = 0;
        for (int k : { 57, 61, 64, 69 }) found += std::abs(cents(models::peakNear(cy, 0, cy.size(), 48000.0, hzOf(k)), hzOf(k))) < 20.0 ? 1 : 0;
        s.check(found == 4, "polyphony: a four-note chord sounds all four", std::to_string(found) + " of 4");
    }

    s.section("instrument mode: the transport does not matter");
    {
        // Transport Play Mode, a note held while the host starts and locates: it rings on.
        auto h = ihost(Castle, CastlePlace::Horns);
        h.set("playmode", float(PlayTransport));
        note(h, 57, 110);
        const auto before = measure(h.render(0.8), 0.3);
        h.playHead.position = 48000 * 20;
        h.playHead.playing = true;
        const auto during = h.render(0.8);
        s.check(measure(during, 0.0, 0.1).rmsDb > before.rmsDb - 3.0 && largestStep(during, 0.0, 0.05) < 0.25,
                "the host starting playback does not cut or restart a held note", fmt(before.rmsDb) + " -> " + fmt(measure(during, 0.0, 0.1).rmsDb) + " dB");
        h.playHead.playing = false;
        const auto stopped = h.render(0.5);
        s.check(measure(stopped, 0.0, 0.2).rmsDb > before.rmsDb - 3.0, "nor does it stopping");
        off(h, 57);
        h.render(1.0);
    }

    s.section("instrument mode: switching instruments while notes are held");
    {
        auto h = ihost(Castle, CastlePlace::Horns);
        for (int k : { 57, 64 }) note(h, k, 110);
        const auto first = measure(h.render(1.0), 0.3);
        h.p->setInstrument({ Castle, CastlePlace::Choir });
        const auto swap = h.render(1.0);
        const auto now = measure(swap, 0.3);
        s.check(h.p->engine.instrumentElement() == CastlePlace::Choir, "the new instrument plays from the next block");
        s.check(largestStep(swap, 0.0, 0.1) < 0.2 && measure(swap, 0.0).finite, "no click: the old one fades out (12 ms) before the new one comes in",
                "largest step " + fmt(largestStep(swap, 0.0, 0.1)) + " (held " + fmt(first.rmsDb) + " dB)");
        s.check(now.rmsDb > -50.0, "the keys still held play on the new instrument", fmt(now.rmsDb) + " dB");
        // Another Place altogether, then let go: nothing hangs.
        h.p->setInstrument({ Spaceport, SpaceportPlace::Pads });
        h.render(1.0);
        // (The pads' own release is 4.5 s, with a shimmer: at its shortest here.)
        h.set("inst_release", 0.0f);
        h.render(0.1);
        for (int k : { 57, 64 }) off(h, k);
        // Every second quieter (the pads' shimmer rings on a while), then silent.
        std::vector<double> decay;
        for (int i = 0; i < 6; ++i) decay.push_back(measure(h.render(1.0), 0.0).rmsDb);
        bool falling = true;
        for (std::size_t i = 1; i < decay.size(); ++i) falling &= decay[i] < decay[i - 1];
        s.check(falling && decay.back() < -70.0, "after the keys are released nothing hangs (old or new instrument)",
                fmt(decay.front()) + " .. " + fmt(decay.back()) + " dB over 6 s");
        // Stepping through many instruments with a chord held, then releasing.
        auto step = ihost(Tavern, TavernPlace::LuteStrum);
        for (int k : { 50, 57, 62 }) note(step, k, 100);
        double worst = 0;
        bool fin = true;
        auto id = step.p->instrument();
        for (int i = 0; i < 24; ++i) {
            id = catalog.step(id, 1);
            step.p->setInstrument(id);
            const auto a = step.render(0.15);
            fin = fin && measure(a, 0.0).finite;
            worst = std::max(worst, largestStep(a, 0.0, 0.03));
        }
        for (int k : { 50, 57, 62 }) off(step, k);
        step.render(5.0);
        const auto left = measure(step.render(1.0), 0.0);
        s.check(fin && worst < 0.35, "24 instruments in a row with a chord held: finite, no step larger than a note's own", "largest step " + fmt(worst));
        s.check(left.rmsDb < -70.0, "and when the chord is released, silence", fmt(left.rmsDb) + " dB");
        // The mode itself switched (automated) with notes held: nothing hangs either way.
        auto mode = ihost(Castle, CastlePlace::Horns);
        note(mode, 57, 110);
        mode.render(0.5);
        mode.p->setInstrumentMode(false);
        const auto toWorld = mode.render(0.5);
        mode.p->setInstrumentMode(true);
        const auto back = mode.render(0.5);
        off(mode, 57);
        // The same note on an instance that never switched: the tail to compare with.
        auto same = ihost(Castle, CastlePlace::Horns);
        note(same, 57, 110);
        same.render(1.5);
        off(same, 57);
        mode.render(3.0);
        same.render(3.0);
        const auto modeAfter = measure(mode.render(1.0), 0.0), sameAfter = measure(same.render(1.0), 0.0);
        s.check(measure(toWorld, 0.0).finite && measure(back, 0.0).finite && modeAfter.rmsDb < std::max(sameAfter.rmsDb + 3.0, -80.0),
                "switching the Mode (automation) with a note held: finite, and nothing hangs (its tail as an unswitched note's)",
                fmt(modeAfter.rmsDb) + " vs " + fmt(sameAfter.rmsDb) + " dB, 3 s after the release");
    }

    s.section("instrument mode: recorded instruments play their articulations");
    {
        if (samples::Bank::get().zoneCount() == 0) {
            std::cout << "  [info] no recordings in this build: articulation checks skipped\n";
        } else {
            // The Megacity's glitch stabs (the cyberpunk synth: short and long hits).
            const inst::Id blips { Megacity, MegacityPlace::Stabs };
            auto quick = ihost(blips), longer = ihost(blips);
            note(quick, 57, 110);
            note(longer, 57, 110);
            const auto q0 = quick.render(0.06), l0 = longer.render(0.06);
            off(quick, 57);
            const auto q1 = quick.render(2.0), l1 = longer.render(2.0);
            s.check(proTest::worstDifference(q0, l0) < 1.0e-6, "a live note starts as the short hit (its length is not known yet)");
            const auto qt = measure(q1, 1.4, 2.0), lt = measure(l1, 1.4, 2.0);
            s.check(lt.rmsDb > qt.rmsDb + 12.0, "a short note stays a short hit; held, it becomes the long hit", "at 1.5 s: short " + fmt(qt.rmsDb) + " dB, held " + fmt(lt.rmsDb) + " dB");
            s.check(largestStep(l1, 0.0, 1.0) < 0.3, "the hand-over from the short to the long hit does not click", fmt(largestStep(l1, 0.0, 1.0)));
            off(longer, 57);
        }
    }

    s.section("instrument mode: the controls (Attack, Release, Tone, Drive, effects, Character)");
    {
        const inst::Id horns { Castle, CastlePlace::Horns };
        auto slow = ihost(horns), fast = ihost(horns);
        slow.set("inst_attack", 1.0f);
        fast.set("inst_attack", 0.0f);
        note(slow, 57, 110);
        note(fast, 57, 110);
        const auto sa = slow.render(1.5), fa = fast.render(1.5);
        s.check(measure(fa, 0.0, 0.1).rmsDb > measure(sa, 0.0, 0.1).rmsDb + 6.0, "Attack: slower at the top, quicker at the bottom",
                fmt(measure(fa, 0.0, 0.1).rmsDb) + " vs " + fmt(measure(sa, 0.0, 0.1).rmsDb) + " dB in the first 100 ms");
        auto longTail = ihost(horns), shortTail = ihost(horns);
        longTail.set("inst_release", 1.0f);
        shortTail.set("inst_release", 0.0f);
        note(longTail, 57, 110);
        note(shortTail, 57, 110);
        longTail.render(0.6);
        shortTail.render(0.6);
        off(longTail, 57);
        off(shortTail, 57);
        const auto lr = measure(longTail.render(1.0), 0.3, 0.8), sr = measure(shortTail.render(1.0), 0.3, 0.8);
        s.check(lr.rmsDb > sr.rmsDb + 10.0, "Release: a longer tail at the top", fmt(lr.rmsDb - sr.rmsDb) + " dB");
        auto dark = ihost(horns), bright = ihost(horns);
        dark.set("inst_tone", 0.0f);
        bright.set("inst_tone", 1.0f);
        note(dark, 57, 110);
        note(bright, 57, 110);
        const auto ds = measure(dark.render(1.0), 0.2), bs = measure(bright.render(1.0), 0.2);
        s.check(bs.highDb - bs.lowDb > ds.highDb - ds.lowDb + 6.0, "Tone: dark to bright", fmt((bs.highDb - bs.lowDb) - (ds.highDb - ds.lowDb)) + " dB");
        const inst::Id flute { Forest, ForestPlace::Flute };
        auto clean = ihost(flute), driven = ihost(flute);
        driven.set("inst_drive", 1.0f);
        note(clean, 57, 127);
        note(driven, 57, 127);
        const auto ca = clean.render(1.0), da = driven.render(1.0);
        const auto cs = measure(ca, 0.2), dv = measure(da, 0.2);
        // The third harmonic against the fundamental (a flute is nearly a sine).
        const auto cy = mono(ca, 0.3, 0.5), dy = mono(da, 0.3, 0.5);
        const auto third = [](const std::vector<float>& y) {
            return sampleTest::energyAt(y, 0, y.size(), 48000, 3.0 * hzOf(57)) - sampleTest::energyAt(y, 0, y.size(), 48000, hzOf(57));
        };
        s.check(dv.finite && dv.peak <= 1.0 && third(dy) > third(cy) + 6.0 && std::abs(dv.rmsDb - cs.rmsDb) < 9.0,
                "Drive: more harmonics, about the same loudness", fmt(third(dy) - third(cy)) + " dB more third harmonic, " + fmt(dv.rmsDb - cs.rmsDb) + " dB louder");
        const inst::Id harpsichord { Palace, PalacePlace::Harpsichord };
        auto dry = ihost(harpsichord), wet = ihost(harpsichord);
        wet.set("inst_rev", 0.8f);
        wet.set("inst_revtype", float(RevCathedral));
        for (auto* h : { &dry, &wet }) {
            note(*h, 57, 110);
            h->render(0.4);
            off(*h, 57);
        }
        const auto da2 = dry.render(2.0), wa2 = wet.render(2.0);
        const auto dt = measure(da2, 0.5, 1.5), wt = measure(wa2, 0.5, 1.5);
        s.check(wt.rmsDb > dt.rmsDb + 15.0, "the reverb send rings on after the note", fmt(wt.rmsDb) + " vs " + fmt(dt.rmsDb) + " dB");
        auto echo = ihost(Tavern, TavernPlace::LuteStrum);
        echo.set("inst_dly", 0.8f);
        note(echo, 57, 110);
        echo.render(0.1);
        off(echo, 57);
        auto plain = ihost(Tavern, TavernPlace::LuteStrum);
        note(plain, 57, 110);
        plain.render(0.1);
        off(plain, 57);
        s.check(measure(echo.render(2.0), 0.6, 2.0).rmsDb > measure(plain.render(2.0), 0.6, 2.0).rmsDb + 10.0, "the delay send repeats it");
        // The Characters change the instrument.
        std::array<Stats, 3> chars;
        for (int c = 0; c < 3; ++c) {
            auto h = ihost(Spaceport, SpaceportPlace::Pulses);
            h.set("inst_character", float(c));
            h.render(0.2);
            note(h, 57, 110);
            chars[std::size_t(c)] = measure(h.render(1.5), 0.1);
        }
        s.check(difference(chars[0], chars[1]) > 0.5 && difference(chars[0], chars[2]) > 0.5, "the Characters change the instrument",
                fmt(difference(chars[0], chars[1])) + " / " + fmt(difference(chars[0], chars[2])) + " dB");
        // Choosing an instrument sets its own effect defaults.
        Host h;
        h.p->setInstrument({ Temple, TemplePlace::Choir });
        s.check(juce::roundToInt(h.get("inst_revtype")) == RevCathedral && std::abs(h.get("inst_rev") - instrumentFx(Temple, TemplePlace::Choir).reverb) < 1.0e-4f,
                "choosing an instrument sets its own effects (the Temple choir: the Cathedral)");
    }

    s.section("instrument mode: state round trip");
    {
        auto h = ihost(Desert, DesertPlace::Oud);
        h.set("inst_character", 2.0f);
        h.set("inst_tone", 0.8f);
        h.set("inst_drive", 0.3f);
        h.set("inst_octave", -1.0f);
        h.set("inst_ambience", 1.0f);
        juce::MemoryBlock state;
        h.p->getStateInformation(state);
        Host fresh;
        fresh.p->setStateInformation(state.getData(), int(state.getSize()));
        s.check(fresh.p->instrumentMode() && fresh.p->instrument() == inst::Id { Desert, DesertPlace::Oud }, "Mode and the instrument come back with the project");
        bool same = true;
        for (int p = 0; p < PCount; ++p)
            if (isInstrumentParam(p)) same &= std::abs(fresh.get(specs[std::size_t(p)].id) - h.get(specs[std::size_t(p)].id)) < 1.0e-4f;
        s.check(same, "and every one of its controls");
        note(fresh, 57, 110);
        note(h, 57, 110);
        const auto a = h.render(1.0), b = fresh.render(1.0);
        s.check(measure(b, 0.1).rmsDb > -50.0 && std::abs(measure(a, 0.1).rmsDb - measure(b, 0.1).rmsDb) < 3.0, "and it plays the same",
                fmt(measure(a, 0.1).rmsDb) + " / " + fmt(measure(b, 0.1).rmsDb) + " dB");
        // A .world file is a world: loading one keeps the session's instrument.
        Host other;
        other.set("p1_type", float(Forest));
        const auto world = other.p->exportWorld();
        h.p->importWorld(world);
        s.check(h.p->instrumentMode() && h.p->instrument() == inst::Id { Desert, DesertPlace::Oud } && std::abs(h.get("inst_tone") - 0.8f) < 1.0e-4f
                    && placeType(h, 0) == Forest,
                "loading a .world keeps Instrument mode, its instrument and controls (and loads the world)");
    }

    s.section("instrument mode: no heap allocation on the audio thread");
    {
        auto h = ihost(Tavern, TavernPlace::LuteStrum, true);
        std::size_t calls = 0;
        auto id = h.p->instrument();
        for (int i = 0; i < 60; ++i) {
            if (i % 5 == 0) {
                id = catalog.step(id, 7);
                h.p->setInstrument(id);   // the browser (message thread), between blocks
            }
            if (i % 3 == 0) note(h, 48 + i % 24, 100);
            if (i % 4 == 0) off(h, 48 + (i + 12) % 24);
            if (i % 7 == 0) h.midi.addEvent(juce::MidiMessage::pitchWheel(1, 4096 + 300 * i), 3);
            if (i % 9 == 0) cc(h, 64, i % 2 ? 127 : 0);
            allocationProbe::calls = 0;
            allocationProbe::enabled = true;
            h.processOne(i % 2 == 0 ? 512 : 129);
            allocationProbe::enabled = false;
            calls += allocationProbe::calls;
        }
        s.check(calls == 0, "Instrument mode's processBlock allocates nothing (notes, bend, pedal, instruments changing)", std::to_string(calls) + " allocations");
    }

    s.section("instrument mode: several instances at once (CPU)");
    {
        // One WORLDBUILDER per track, each its own instrument, four-note chords held, the
        // effects on: like a song of four tracks in Logic.
        std::vector<std::unique_ptr<Host>> tracks;
        const std::array<inst::Id, 4> ids { { { Castle, CastlePlace::Choir }, { Tavern, TavernPlace::LuteStrum }, { Spaceport, SpaceportPlace::Pads }, { Palace, PalacePlace::Strings } } };
        for (const auto& id : ids) {
            auto h = std::make_unique<Host>(48000, 256);
            h->p->setInstrument(id);
            h->p->setInstrumentMode(true);
            h->render(0.3);
            for (int k : { 50, 57, 62, 65 }) note(*h, k, 100);
            tracks.push_back(std::move(h));
        }
        for (auto& h : tracks) h->render(0.5);
        const double seconds = 6.0;
        const auto begin = juce::Time::getHighResolutionTicks();
        bool fin = true;
        for (int block = 0; block < int(seconds * 48000.0 / 256.0); ++block)
            for (auto& h : tracks) {
                h->processOne(256);
                fin = fin && std::isfinite(h->buffer.getSample(0, 0));
            }
        const double used = 100.0 * juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin) / seconds;
        s.metric("CPU, four instances in Instrument mode, four-note chords (report only; the gate is generous)", used, " %");
        s.check(fin && used < 60.0, "four instances in Instrument mode stay within the CPU budget (15 % of a core each)", fmt(used) + " % of one core in all");
    }

    s.section("instrument mode: the interface (switch, browser, keyboard, presets)");
    {
        Host h;
        h.set("p1_type", float(Castle));
        h.render(0.3);
        std::unique_ptr<juce::AudioProcessorEditor> editor(h.p->createEditor());
        auto* view = dynamic_cast<WorldbuilderEditor*>(editor.get());
        view->settle(0.2f);
        s.check(!view->showingInstrument() && view->worldModeButton().getToggleState() && view->knob(PTemperature) != nullptr && view->knob(PTemperature)->isVisible(),
                "WORLD: the world's interface, WORLD lit in the top bar");
        view->instrumentModeButton().onClick();
        view->settle(0.2f);
        auto& iv = view->instrumentView();
        s.check(h.p->instrumentMode() && view->showingInstrument() && iv.isVisible() && !view->knob(PTemperature)->isVisible() && view->instrumentModeButton().getToggleState(),
                "INSTRUMENT in the top bar: the Mode parameter, and the instrument's screen in place of the world's panels");
        // Host automation of Mode: the editor follows.
        h.set("mode", float(ModeWorld));
        view->settle(0.2f);
        s.check(!view->showingInstrument() && !iv.isVisible() && view->knob(PTemperature)->isVisible(), "automating Mode back to World: the world's interface returns");
        h.set("mode", float(ModeInstrument));
        view->settle(0.2f);
        s.check(view->showingInstrument() && iv.isVisible(), "and to Instrument again");

        // The browser.
        iv.setSearch({});
        iv.setOrder(ui::InstrumentView::ByPlace);
        s.check(iv.listedCount() == catalog.count && iv.listedAt(0) == inst::Id { Castle, 0 }, "BY PLACE lists every instrument, the Castle's first",
                std::to_string(iv.listedCount()));
        iv.setOrder(ui::InstrumentView::ByType);
        bool grouped = true;
        for (int i = 1; i < iv.listedCount(); ++i) grouped &= inst::groupOf(iv.listedAt(i - 1).type, iv.listedAt(i - 1).element) <= inst::groupOf(iv.listedAt(i).type, iv.listedAt(i).element);
        s.check(iv.listedCount() == catalog.count && grouped && inst::groupOf(iv.listedAt(0).type, iv.listedAt(0).element) == inst::Plucks,
                "BY TYPE lists every instrument grouped by type (Plucks first)");
        iv.setSearch("bells");
        bool bells = iv.listedCount() > 2;
        for (int i = 0; i < iv.listedCount(); ++i) {
            const auto id = iv.listedAt(i);
            bells &= juce::String(placeInfo(id.type).element[std::size_t(id.element)].name).containsIgnoreCase("bell")
                     || juce::String(inst::groupNames[std::size_t(inst::groupOf(id.type, id.element))]).containsIgnoreCase("bell");
        }
        s.check(bells, "search: \"bells\" finds the bells (by name or type)", std::to_string(iv.listedCount()) + " found");
        iv.setSearch("tavern lute");
        s.check(iv.listedCount() == 1 && iv.listedAt(0) == inst::Id { Tavern, TavernPlace::LuteStrum }, "search words narrow together: \"tavern lute\"");
        iv.setSearch("pad");
        const int pads = iv.listedCount();
        iv.choose(iv.listedAt(0));
        const auto first = h.p->instrument();
        iv.nextButton().onClick();
        const auto second = h.p->instrument();
        iv.previousButton().onClick();
        s.check(pads > 3 && second == iv.listedAt(1) && h.p->instrument() == first, "NEXT and PREVIOUS step through the list as shown (here: the pads)");
        iv.previousButton().onClick();
        s.check(h.p->instrument() == iv.listedAt(pads - 1), "and wrap around");
        iv.setSearch({});
        // Choosing sets the instrument's own effects; the macros read ATTACK or STRIKE.
        iv.choose({ Temple, TemplePlace::Choir });
        s.check(h.p->instrument() == inst::Id { Temple, TemplePlace::Choir } && juce::roundToInt(h.get("inst_revtype")) == RevCathedral && iv.macroLabel(1) == "ATTACK",
                "a click in the browser plays that instrument, with its own effects");
        iv.choose({ Castle, CastlePlace::Bells });
        s.check(iv.macroLabel(1) == "STRIKE" && iv.macroLabel(2) == "RING", "a struck instrument's macros say STRIKE and RING");
        // Character buttons show the Place's Characters and set the parameter.
        iv.characterButton(2).onClick();
        s.check(juce::roundToInt(h.get("inst_character")) == 2 && iv.characterButton(2).getButtonText() == "BESIEGED", "the Character buttons (the Castle's: BESIEGED...)");
        iv.ambienceButton().setToggleState(true, juce::dontSendNotification);
        iv.ambienceButton().onClick();
        s.check(h.get("inst_ambience") > 0.5f, "PLACE AMBIENCE sets its parameter");
        iv.ambienceButton().setToggleState(false, juce::dontSendNotification);
        iv.ambienceButton().onClick();

        // The keyboard lights up with incoming notes; a click plays.
        h.midi.addEvent(juce::MidiMessage::noteOn(1, 64, uint8_t(100)), 0);
        h.render(0.1);
        s.check(iv.keyLit(64) && !iv.keyLit(65), "the on-screen keyboard lights up the notes arriving from the host");
        h.midi.addEvent(juce::MidiMessage::noteOff(1, 64), 0);
        h.render(0.1);
        s.check(!iv.keyLit(64), "and goes dark when they end");
        auto& kb = iv.keyboardComponent();
        const juce::MouseEvent press(juce::Desktop::getInstance().getMainMouseSource(), juce::Point<float>(float(kb.getWidth()) * 0.5f, float(kb.getHeight()) * 0.8f),
                                     juce::ModifierKeys(), 1.0f, 0, 0, 0, 0, &kb, &kb, juce::Time(), {}, juce::Time(), 1, false);
        kb.mouseDown(press);
        const auto clicked = measure(h.render(0.4), 0.05);
        kb.mouseUp(press);
        h.render(0.1);
        bool any = false;
        for (int n = 24; n <= 108; ++n) any |= iv.keyLit(n);
        s.check(clicked.rmsDb > -60.0, "a click on the on-screen keyboard plays the instrument", fmt(clicked.rmsDb) + " dB");

        // Presets: the factory set, previous / next, a tile, a file round trip.
        s.check(iv.presetTileCount() == int(inst::factoryPresets().size()) && iv.presetTileCount() >= 20, "a factory set of instrument presets, each a tile",
                std::to_string(iv.presetTileCount()));
        iv.presetTile(0).onClick();
        const auto& p0 = inst::factoryPresets()[0];
        bool applied = h.p->instrument() == p0.id && h.p->instrumentPresetName() == p0.name;
        for (const auto& [param, value] : p0.values) applied &= std::abs(h.get(specs[std::size_t(param)].id) - value) < 1.0e-3f;
        s.check(applied, "a factory preset sets its instrument, Character, effects and controls", h.p->instrumentPresetName().toStdString());
        iv.stepPreset(1);
        s.check(h.p->instrumentPresetName() == inst::factoryPresets()[1].name && iv.presetTile(1).getToggleState(), "the next preset, its tile lit");
        h.set("inst_tone", 0.83f);
        h.set("inst_drive", 0.41f);
        const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("wb-inst-presets");
        folder.deleteRecursively();
        const auto file = folder.getChildFile("My Fiddle.wbinst");
        s.check(iv.savePresetTo(file, "My Fiddle") && file.existsAsFile(), "SAVE writes a preset file");
        const auto savedId = h.p->instrument();
        iv.choose({ Nebula, NebulaPlace::Pads });
        s.check(iv.loadPresetFrom(file) && h.p->instrument() == savedId && std::abs(h.get("inst_tone") - 0.83f) < 1.0e-3f && std::abs(h.get("inst_drive") - 0.41f) < 1.0e-3f
                    && h.p->instrumentPresetName() == "My Fiddle",
                "LOAD brings back the instrument and every control");
        // A preset leaves the world alone, and a world leaves the instrument alone.
        s.check(placeType(h, 0) == Castle, "loading an instrument preset leaves the world as it was");
        folder.deleteRecursively();
        s.check(!juce::String(h.p->exportInstrumentPreset("x")).contains("p1_type"), "an instrument preset holds no world");
        juce::ignoreUnused(any);
    }

    s.section("instrument mode: only what is played sounds (drones, pedals, strokes, open strings, tails)");
    {
        // Every instrument: nothing without a note; nothing tied to the key (Root F# against
        // Root C, an A played: the share of F# must not grow, Drone Off); a held note let go
        // is gone within six seconds (struck bells, gongs and drums may ring on).
        std::string sounding, keyed, tails;
        int checked = 0;
        const auto shareOf = [](const std::array<double, 12>& pc, int c) {
            double total = 0;
            for (double e : pc) total += e;
            return total > 0.0 ? pc[std::size_t(c)] / total : 0.0;
        };
        for (int i = 0; i < catalog.count; ++i) {
            const auto id = catalog.entry[std::size_t(i)];
            const auto name = capture::trackName(id.type, id.element).toStdString();
            const bool kit = patternInfo(id.type, id.element).kind == Kit;
            const int group = inst::groupOf(id.type, id.element);
            auto quiet = ihost(id, false, false);
            if (measure(quiet.render(2.0), 0.0).rmsDb > -100.0) sounding += " " + name;
            std::array<double, 2> share {};
            double after = -240;
            for (int k = 0; k < 2; ++k) {
                auto h = ihost(id, false, false);
                h.set("root", k == 0 ? 6.0f : 0.0f);
                h.set("inst_drone", float(DroneOff));
                h.render(0.1);
                note(h, 57, 100);
                share[std::size_t(k)] = shareOf(chroma(h.render(1.5), 0.05, 40.0, 5000.0, 32768), 6);
                off(h, 57);
                h.render(5.0);
                after = std::max(after, measure(h.render(1.0), 0.0).rmsDb);
            }
            if (!kit && share[0] > 2.0 * share[1] + 0.03) keyed += " " + name + " (" + fmt(share[1]) + " -> " + fmt(share[0]) + ")";
            const bool rings = kit || group == inst::BellsMallets || group == inst::Percussion;
            if (!rings && after > -60.0) tails += " " + name + " (" + fmt(after) + " dB)";
            ++checked;
        }
        s.check(checked == catalog.count && sounding.empty(), "no instrument sounds without a note", sounding);
        s.check(keyed.empty(), "no instrument plays a pitch tied to the key instead of the note (drones, pedals, open strings)", keyed);
        s.check(tails.empty(), "every held instrument has let go within six seconds of the release (below -60 dB)", tails);
    }
    {
        // The hurdy-gurdy: its drone strings follow the note held, as Drone says.
        const inst::Id hurdy { Village, VillagePlace::HurdyGurdy };
        const auto play = [&](int drone, double& root, double& fifth, double& tail) {
            auto h = ihost(hurdy);
            h.set("root", 0.0f);
            h.set("inst_drone", float(drone));
            h.render(0.1);
            note(h, 57, 100);
            const auto y = mono(h.render(1.5), 0.6, 0.8);
            root = sampleTest::energyAt(y, 0, y.size(), 48000, hzOf(45)) - sampleTest::energyAt(y, 0, y.size(), 48000, hzOf(57));
            fifth = sampleTest::energyAt(y, 0, y.size(), 48000, hzOf(52)) - sampleTest::energyAt(y, 0, y.size(), 48000, hzOf(57));
            off(h, 57);
            h.render(1.0);
            tail = measure(h.render(0.5), 0.0).rmsDb;
        };
        double offRoot = 0, offFifth = 0, offTail = 0, rootRoot = 0, rootFifth = 0, rootTail = 0, bothRoot = 0, bothFifth = 0, bothTail = 0;
        play(DroneOff, offRoot, offFifth, offTail);
        play(DroneRoot, rootRoot, rootFifth, rootTail);
        play(DroneRootFifth, bothRoot, bothFifth, bothTail);
        s.check(specs[PInstDrone].def == float(DroneRoot) && juce::String(specs[PInstDrone].choices) == "Off|Root|Root + Fifth", "a Drone control: Off, Root, Root + Fifth (Root by default)");
        s.check(rootRoot > offRoot + 12.0, "Drone Root: the hurdy-gurdy's drone sounds the played note's root (A2 under A3)", fmt(offRoot) + " -> " + fmt(rootRoot) + " dB");
        s.check(bothFifth > rootFifth + 12.0, "Drone Root + Fifth: and its fifth (E3)", fmt(rootFifth) + " -> " + fmt(bothFifth) + " dB");
        s.check(std::max({ offTail, rootTail, bothTail }) < -70.0, "the drone stops with the key (one second after: silence)",
                fmt(offTail) + " / " + fmt(rootTail) + " / " + fmt(bothTail) + " dB");
        // In World mode the drones stay on the key, as before.
        Host world;
        world.set("p1_type", float(Village));
        world.set("p1_solo", 1.0f);
        world.render(0.2);
        s.check(!world.p->instrumentMode(), "(World mode keeps its drones: the hurdy group tests them)");
    }
}

