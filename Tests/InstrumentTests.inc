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
        s.check(measure(toWorld, 0.0).finite && measure(back, 0.0).finite && modeAfter.rmsDb < sameAfter.rmsDb + 3.0,
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
}
