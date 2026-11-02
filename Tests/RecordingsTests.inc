// Milestone 17: the piano, voice, djembe and banjo recordings.
// Included by WorldbuilderTests.cpp after the instrument-mode tests (it uses their helpers).
//
//   --only recordings   the checks: every new set loads, plays in tune, its velocity layers
//                       rise smoothly, nothing clicks (loops, note-offs, short-to-long
//                       hand-overs), each changed part sits at its fallback's level, the
//                       Places stay themselves; listening renders into the test logs
//   --only recaudit     every changed part's level with the recordings and without, per
//                       Character (how the parts' gains were set)

namespace recTest {
using namespace sampleTest;
using namespace keysTest;

std::string noteName(int midi) {
    static const char* names[12] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return std::string(names[((midi % 12) + 12) % 12]) + std::to_string(midi / 12 - 1);
}

// A Place's part that plays the new recordings, and in which Characters it does.
struct Changed {
    int type, element;
    const char* name;
    std::array<bool, 3> uses;
    bool layer = false;   // a layer added under the part (the fallback is the part alone)
};
const std::vector<Changed>& changed() {
    static const std::vector<Changed> list {
        { Suburbs, SuburbsPlace::EPiano, "Suburbs E-Piano (piano in the Evening)", { false, true, false } },
        { Mall, MallPlace::EPiano, "Mall E-Piano (piano on an empty night and in the dead mall)", { false, true, true } },
        { Ruins, RuinsPlace::PianoShards, "Ruins Piano Shards", { true, true, true } },
        { Ruins, RuinsPlace::Wind, "Ruins Wind (ghostly falsetto and whispers under it)", { true, true, true }, true },
        { Ruins, RuinsPlace::LowToms, "Ruins Low Toms", { true, true, true } },
        { Castle, CastlePlace::Choir, "Castle Choir", { true, true, true } },
        { Palace, PalacePlace::Choir, "Palace Choir", { true, true, true } },
        { Temple, TemplePlace::Choir, "Temple Choir", { true, true, true } },
        { Temple, TemplePlace::Chant, "Temple Chant Drone", { true, true, true } },
        { AlienRuins, AlienRuinsPlace::Choir, "Alien Ruins Alien Choir", { true, true, true } },
        { AlienRuins, AlienRuinsPlace::Whispers, "Alien Ruins Whispers", { true, true, true } },
        { AlienRuins, AlienRuinsPlace::Ritual, "Alien Ruins Ritual Drums", { true, true, true } },
        { Mining, MiningPlace::Hymn, "Asteroid Mining Worker Hymn", { true, true, true } },
        { Cavern, CavernPlace::SubDrone, "Cavern Sub Drone (a hum layer under it)", { true, true, true }, true },
        { Nebula, NebulaPlace::SubDrone, "Nebula Deep Hum", { true, true, true } },
        { Hive, HivePlace::Formants, "Alien Hive Formants", { true, true, true } },
        { Bridge, BridgePlace::Chatter, "Starship Bridge Crew Chatter", { true, true, true } },
        { Village, VillagePlace::HandDrums, "Village Hand Drums", { true, true, true } },
        { Tavern, TavernPlace::FrameDrum, "Tavern Frame Drum", { true, true, true } },
        { Tavern, TavernPlace::Stomps, "Tavern Stomps", { true, true, true } },
        { Desert, DesertPlace::FrameDrums, "Desert Frame Drums", { true, true, true } },
        { Temple, TemplePlace::Zither, "Temple Zither (the banjo)", { true, true, true } },
#ifdef WB_BANJO_PARTS
        WB_BANJO_PARTS
#endif
    };
    return list;
}

// One element alone in a Character (activity 0.9), with the recordings or without.
Audio elementAudio(int type, int character, int element, bool recordings, double seconds) {
    samples::Bank::bypassForTests(!recordings);
    auto h = instrumentHost(type, character, element, { { "p1_activity", 0.9f } });
    auto a = h.render(seconds);
    samples::Bank::bypassForTests(false);
    return a;
}

// A WAV for listening (16-bit stereo) in the renders folder (WORLDBUILDER_RENDERS, else
// build/test-logs/renders).
juce::File rendersFolder() {
    const auto env = juce::SystemStats::getEnvironmentVariable("WORLDBUILDER_RENDERS", {});
    auto dir = env.isNotEmpty() ? juce::File(env) : juce::File::getCurrentWorkingDirectory().getChildFile("build/test-logs/renders");
    dir.createDirectory();
    return dir;
}
bool writeWav(const juce::String& name, const std::vector<float>& l, const std::vector<float>& r, double rate) {
    auto file = rendersFolder().getChildFile(name);
    file.deleteFile();
    std::unique_ptr<juce::FileOutputStream> out(file.createOutputStream());
    if (out == nullptr) return false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(out.get(), rate, 2, 16, {}, 0));
    if (w == nullptr) return false;
    out.release();
    // Normalised to -1 dBFS peak where louder (a render to listen to, not to measure).
    float peak = 1.0e-9f;
    for (std::size_t i = 0; i < l.size(); ++i) peak = std::max({ peak, std::abs(l[i]), std::abs(r[i]) });
    const float g = std::min(1.0f, 0.89f / peak);
    juce::AudioBuffer<float> b(2, int(l.size()));
    for (std::size_t i = 0; i < l.size(); ++i) {
        b.setSample(0, int(i), l[i] * g);
        b.setSample(1, int(i), r[i] * g);
    }
    return w->writeFromAudioSampleBuffer(b, 0, b.getNumSamples());
}
void append(std::vector<float>& l, std::vector<float>& r, const Audio& a, double from = 0.0, double gap = 0.75) {
    for (std::size_t i = std::size_t(from * a.rate); i < a.l.size(); ++i) l.push_back(a.l[i]), r.push_back(a.r[i]);
    for (int i = 0; i < int(gap * a.rate); ++i) l.push_back(0.0f), r.push_back(0.0f);
}

// A sampled part's note alone (mono), the part built and set up by `make`.
template <typename PartT>
std::vector<float> partNote(PartT& part, float semis, float velocity, double hold, double seconds, bool live = true, float random = 0.3f, float random2 = 0.5f) {
    const float rate = 48000;
    part.prepare(rate);
    PartFrame f;
    f.rate = rate;
    f.root = 0;
    f.beatsPerSecond = 2;
    NoteOn n;
    n.semis = semis;
    n.velocity = velocity;
    n.random = random;
    n.random2 = random2;
    if (live) {
        n.live = 99;
        n.length = liveLength;
        n.seconds = 3600;
    } else {
        n.length = hold * 2.0;
        n.seconds = float(hold);
    }
    part.update(f);
    part.start(f, n);
    std::vector<float> l(std::size_t(seconds * rate), 0.0f), r(l.size(), 0.0f);
    bool released = false;
    for (std::size_t at = 0; at < l.size(); at += std::size_t(dsp::chunk)) {
        if (live && !released && double(at) >= hold * rate) {
            part.stopLive(99);
            released = true;
        }
        f.beat = double(at) / rate * f.beatsPerSecond;
        part.update(f);
        part.render(l.data() + at, r.data() + at, int(std::min<std::size_t>(std::size_t(dsp::chunk), l.size() - at)));
    }
    for (std::size_t i = 0; i < l.size(); ++i) l[i] += r[i];
    return l;
}

// The piano as Places play it (pianoPatch), neutral: full tone, nothing evened.
void neutralPiano(SamplePatch& p) {
    p = pianoPatch();
    p.toneHz = 20000;
    p.velocityTone = 0;
    p.even = 0;
    p.spread = 0;
    p.voices = 4;
}

// The pitch (cents from equal temperament) a piano key sounds at: its fundamental (the
// strongest frequency within 60 cents of the key, 1-cent steps) from 0.2 to 1.2 s.
double pianoCents(const std::vector<float>& y, int key) {
    const double hz = hzOf(key);
    const auto from = std::size_t(0.2 * 48000), length = std::size_t(1.0 * 48000);
    double best = -1.0e9, at = hz;
    for (double c = -60; c <= 60; c += 1.0) {
        const double f = hz * std::exp2(c / 1200.0);
        const double e = energyAt(y, from, length, 48000, f);
        if (e > best) best = e, at = f;
    }
    // Refine around the best to 0.2 cents.
    for (double c = -1.0; c <= 1.0; c += 0.2) {
        const double f = at * std::exp2(c / 1200.0);
        const double e = energyAt(y, from, length, 48000, f);
        if (e > best) best = e, at = f;
    }
    return cents(at, hz);
}

// The pitch (cents from `key`) a sung note's first four harmonics agree on over
// [from, from + length): the frequency within 60 cents whose harmonics carry the most
// energy (2-cent steps, refined to 0.4).
double harmonicCents(const std::vector<float>& y, std::size_t from, std::size_t length, int key) {
    const double hz = hzOf(key);
    const auto score = [&](double f) {
        double e = 0;
        for (int k = 1; k <= 4; ++k)
            if (f * k < 20000.0) e += std::pow(10.0, energyAt(y, from, length, 48000, f * k) / 10.0);
        return e;
    };
    double best = -1, at = hz;
    for (double c = -60; c <= 60; c += 2.0) {
        const double f = hz * std::exp2(c / 1200.0), e = score(f);
        if (e > best) best = e, at = f;
    }
    const double coarse = at;
    for (double c = -2.0; c <= 2.0; c += 0.4) {
        const double f = coarse * std::exp2(c / 1200.0), e = score(f);
        if (e > best) best = e, at = f;
    }
    return cents(at, hz);
}

// Brightness: the energy above 1.5 kHz against the whole (dB) in [from, from + length)
// (a Hann-windowed FFT of up to 32768 samples).
double brightness(const std::vector<float>& y, std::size_t from, std::size_t length) {
    std::size_t n = 1;
    while (n * 2 <= std::min<std::size_t>(length, 32768)) n *= 2;
    std::vector<std::complex<double>> a(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * double(i) / double(n));
        a[i] = from + i < y.size() ? double(y[from + i]) * w : 0.0;
    }
    fft(a);
    double all = 0, high = 0;
    for (std::size_t k = 1; k < n / 2; ++k) {
        const double hz = double(k) * 48000.0 / double(n), p = std::norm(a[k]);
        if (hz < 30.0) continue;
        all += p;
        if (hz >= 1500.0) high += p;
    }
    return 10.0 * std::log10(std::max(high, 1.0e-30) / std::max(all, 1.0e-30));
}
} // namespace recTest

// --only recaudit: each changed part with and without the recordings, per Character.
void auditRecordings() {
    using namespace recTest;
    samples::Bank::get().load();
    samples::Bank::get().waitUntilLoaded(60000);
    for (const auto& x : changed()) {
        std::string line = std::string(x.name) + ":";
        for (int c = 0; c < placeCharacters; ++c) {
            const double with = loudness(elementAudio(x.type, c, x.element, true, 26.0), 2.0);
            const double without = loudness(elementAudio(x.type, c, x.element, false, 26.0), 2.0);
            line += "  " + std::string(placeInfo(x.type).characters[std::size_t(c)]) + (x.uses[std::size_t(c)] ? "" : " (model)") + " " + fmt(with) + " vs " + fmt(without) + " (" +
                    fmt(with - without) + ")";
        }
        std::cout << line << std::endl;
    }
}

void testRecordings(Suite& s) {
    using namespace recTest;
    auto& bank = samples::Bank::get();
    bank.load();
    const bool have = bank.waitUntilLoaded(60000);
    s.section("recordings (Milestone 17): the new sets in the bank");
    if (!have || !expectSamples) {
        std::cout << "  [report] no samples: the fallbacks play (checked by the Place groups)\n";
        return;
    }
    {
        const auto count = [&](const char* id) {
            const auto* i = bank.instrument(id);
            return i != nullptr ? int(i->zones.size()) : 0;
        };
        const auto* shortPiano = bank.instrument("piano-short");
        int keys = 0;
        if (shortPiano != nullptr)
            for (int k = 21; k <= 108; ++k) {
                int layers = 0;
                for (const auto& z : shortPiano->zones) layers += (k >= z.loKey && k <= z.hiKey) ? 1 : 0;
                keys += layers == 3 ? 1 : 0;
            }
        s.check(keys == 88, "piano: every key A0-C8 has its three velocity layers (soft, medium, full)", std::to_string(keys) + " keys");
        s.check(count("piano-long") == 88 * 3, "piano: every key has a held note (medium, with soft and full derived from it)", std::to_string(count("piano-long")) + " zones");
        s.check(count("djembe") == 90, "djembe: ten strokes, nine hits each (three per layer)", std::to_string(count("djembe")) + " hits");
        s.check(count("banjo") >= 33 + 9, "banjo: three layers of eleven notes, and the open fifth string's nine takes", std::to_string(count("banjo")) + " notes");
        int voices = 0;
        for (const char* id : { "voice-ooh", "voice-oh", "voice-aah", "voice-ee", "voice-mm", "voice-hum", "voice-choir-ooh", "voice-choir-aah", "voice-choir-mm",
                                "voice-drone-hum", "voice-drone-ooh", "voice-drone-aah", "voice-chant", "voice-grit", "voice-ghost", "voice-whisper", "voice-breath" })
            voices += count(id) > 0 ? 1 : 0;
        s.check(voices == 17, "voice: all seventeen sets load (vowels, stacks, drones, chant, grit, hum, ghost, whispers, breaths)", std::to_string(voices) + " of 17");
        s.metric("bank memory with the new recordings", double(bank.memoryBytes()) / 1048576.0, " MB");
        s.metric("bank load time", bank.loadSeconds(), " s");
    }

    // ---------------------------------------------------------------- the piano
    s.section("recordings: the piano plays in tune (cents from equal temperament, its stretch kept)");
    {
        std::string report;
        std::vector<double> dev;
        int checked = 0, close = 0;
        for (int key = 21; key <= 108; key += 1) {
            PluckPart unused;
            SamplePart part(unused);
            neutralPiano(part.patch);
            part.patch.shortInstrument = nullptr;
            const auto y = partNote(part, float(key - 48), 0.6f, 1.5, 1.5);
            const double c = pianoCents(y, key);
            dev.push_back(c);
            if (key >= 36) {
                ++checked;
                // A piano's stretch: flat in the bass, sharp in the treble (about -15 to +25
                // cents at the ends); within 35 cents of equal temperament everywhere it is
                // measured on its fundamental.
                close += std::abs(c) <= 35.0 ? 1 : 0;
            }
            if ((key - 21) % 3 == 0) report += " " + noteName(key) + " " + juce::String(c, 0).toStdString();
        }
        std::cout << "  [report] piano (cents, every third key):" << report << "\n";
        s.check(close == checked, "piano: every key from C2 up within 35 cents of equal temperament (the stretch tuning)", std::to_string(close) + " of " + std::to_string(checked));
        // The stretch is smooth: neighbouring keys (C2 up) never more than 8 cents apart.
        int rough = 0;
        std::string where;
        // (12 in the top octave, where a key's three strings beat more and the measurement
        // is shorter.)
        for (std::size_t i = 16; i + 1 < dev.size(); ++i)
            if (std::abs(dev[i + 1] - dev[i]) > (i + 22 >= 96 ? 12.0 : 8.0)) ++rough, where += " " + noteName(int(i) + 22);
        s.check(rough == 0, "piano: neighbouring keys within 8 cents of each other (12 above C7: no key off the curve)", std::to_string(rough) + where);
        // The middle in tune with A440: F3-F5 within 6 cents on average.
        double mid = 0;
        int n = 0;
        for (int key = 53; key <= 77; ++key) mid += dev[std::size_t(key - 21)], ++n;
        s.metric("piano: mean offset F3-F5 from A440", mid / n, " cents");
        s.check(std::abs(mid / n) < 6.0, "piano: the middle (F3-F5) sits on A440 (the overall offset taken off)", fmt(mid / n) + " cents");
    }

    s.section("recordings: the piano's velocity layers rise smoothly");
    {
        int smooth = 0, total = 0, brighter = 0;
        std::string detail;
        for (int key : { 33, 45, 52, 60, 67, 74, 81, 88, 96 }) {
            double last = -200, lastB = -200, worstDip = 0, worstJump = 0;
            double softB = 0, fullB = 0;
            std::string curve;
            for (int k = 0; k <= 18; ++k) {
                const float v = 0.05f + 0.05f * float(k);
                PluckPart unused;
                SamplePart part(unused);
                neutralPiano(part.patch);
                part.patch.instrument = "piano-short";
                part.patch.shortInstrument = nullptr;
                part.patch.held = false;
                const auto y = partNote(part, float(key - 48), v, 1.0, 0.5, false);
                const double level = rmsDb(y, 0, std::size_t(0.3 * 48000));
                const double b = brightness(y, 0, std::size_t(0.3 * 48000));
                curve += " " + juce::String(level, 1).toStdString();
                if (k == 0) softB = b;
                if (k == 18) fullB = b;
                if (last > -199) {
                    worstDip = std::max(worstDip, last - level);
                    worstJump = std::max(worstJump, level - last);
                }
                last = level;
                lastB = b;
            }
            ++total;
            const bool ok = worstDip < 1.0 && worstJump < 4.0;
            smooth += ok ? 1 : 0;
            brighter += fullB > softB + 1.0 ? 1 : 0;
            if (!ok) detail += " " + noteName(key) + " (dip " + fmt(worstDip) + ", jump " + fmt(worstJump) + " dB:" + curve + ")";
            (void) lastB;
        }
        s.check(smooth == total, "piano: from velocity 0.05 to 0.95 each step louder, no dip over 1 dB, no jump over 4 dB (layers crossfaded)",
                std::to_string(smooth) + " of " + std::to_string(total) + detail);
        s.check(brighter >= total - 1, "piano: full hits brighter than soft ones (by 1 dB above 2 kHz)", std::to_string(brighter) + " of " + std::to_string(total));
    }

    s.section("recordings: the piano's note-offs, pedal and short-to-long hand-over");
    {
        // A held key released after 1 s: the damper stops it (60 dB down within 2 s) with no click.
        int clean = 0, stopped = 0, total = 0;
        std::string detail;
        for (int key : { 28, 40, 52, 64, 76, 86 }) {
            PluckPart unused;
            SamplePart part(unused);
            neutralPiano(part.patch);
            const auto y = partNote(part, float(key - 48), 0.7f, 1.0, 3.5);
            const double before = rmsDb(y, std::size_t(0.85 * 48000), 4800), after = rmsDb(y, std::size_t(3.0 * 48000), 9600);
            const double step = worstStep(y, std::size_t(0.9 * 48000), std::size_t(1.6 * 48000));
            const double hand = worstStep(y, std::size_t(0.1 * 48000), std::size_t(0.8 * 48000));
            ++total;
            clean += step < 30.0 && hand < 30.0 ? 1 : 0;
            stopped += after < before - 40.0 ? 1 : 0;
            if (step >= 30.0 || hand >= 30.0 || after >= before - 40.0)
                detail += " " + noteName(key) + " (release step " + fmt(step) + " dB, hand-over step " + fmt(hand) + " dB, " + fmt(after - before) + " dB)";
        }
        s.check(clean == total, "piano: no click at the short-to-long hand-over (live) or at the damper", std::to_string(clean) + " of " + std::to_string(total) + detail);
        s.check(stopped == total, "piano: a released key is damped (40 dB down 2 s later)", std::to_string(stopped) + " of " + std::to_string(total) + detail);
        // Undamped top keys ring on after the key; a held key decays naturally (no loop: it falls).
        {
            PluckPart unused;
            SamplePart part(unused);
            neutralPiano(part.patch);
            const auto y = partNote(part, float(96 - 48), 0.7f, 0.3, 2.0);
            const double at = rmsDb(y, std::size_t(0.25 * 48000), 4800), later = rmsDb(y, std::size_t(0.8 * 48000), 4800);
            s.check(later > at - 30.0, "piano: a top key (C7, no damper) rings on after it is released", fmt(later - at) + " dB 0.5 s after the key");
        }
        {
            PluckPart unused;
            SamplePart part(unused);
            neutralPiano(part.patch);
            const auto y = partNote(part, float(40 - 48), 0.7f, 8.0, 8.0);
            const auto lv = frames(y, 48000, 500.0);
            int rises = 0;
            for (std::size_t i = 2; i + 1 < lv.size(); ++i) rises += lv[i + 1] > lv[i] + 1.0 ? 1 : 0;
            // (Clicks judged while the note is still well above the floor: in the last fade a
            // single bit is a "step".)
            const double step = worstStep(y, 48000, std::size_t(5.0 * 48000));
            s.check(rises == 0 && lv.back() < lv[1] - 10.0 && step < 30.0, "piano: a key held 8 s (E2) dies away naturally, never rising, no click (no loop)",
                    fmt(lv[1]) + " dB at 0.5 s, " + fmt(lv.back()) + " dB at 7.5 s, worst step " + fmt(step) + " dB");
        }
        // The pedal (Instrument mode, the Mall's piano on an empty night): a key released
        // with the pedal down rings on; the pedal lifted damps it.
        {
            using namespace instTest;
            auto h = ihost(Mall, MallPlace::EPiano);
            h.set("inst_character", 1.0f);
            h.render(0.2);
            cc(h, 64, 127);
            note(h, 48, 90);
            h.render(0.3);
            off(h, 48);
            const auto held = h.render(1.5);
            cc(h, 64, 0);
            const auto damped = h.render(2.0);
            const double ring = measure(held, 1.2, 1.5).rmsDb, after = measure(damped, 1.5, 2.0).rmsDb;
            s.check(ring > -60.0 && after < ring - 25.0, "piano: the sustain pedal holds a released key's natural decay; lifting it damps the note",
                    "pedalled " + fmt(ring) + " dB, after the pedal " + fmt(after) + " dB");
        }
    }

    // ---------------------------------------------------------------- the voice
    s.section("recordings: the voice (in tune, loops, vowels, ensemble, extension)");
    {
        // Every recorded vowel note, held 6 s: in tune (measured on its loop) and no click.
        int tuned = 0, smoothN = 0, total = 0;
        std::string detail, tuning;
        for (const char* id : { "voice-ooh", "voice-oh", "voice-aah", "voice-ee", "voice-mm" }) {
            const auto* inst = bank.instrument(id);
            if (inst == nullptr) continue;
            // A take used in both layers (the falsetto: the same recording, filed twice) counts once.
            std::vector<std::pair<float, int>> seen;
            for (const auto& z : inst->zones) {
                const std::pair<float, int> take { z.root, z.length };
                if (!z.loops() || std::find(seen.begin(), seen.end(), take) != seen.end()) continue;
                seen.push_back(take);
                const int key = int(std::lround(z.root));
                const auto y = play(id, key, z.layer == 0 ? 0.3f : 0.9f, (float(z.rr) + 0.5f) / 3.0f, 6.0, 6.5, true);
                // Measured on the loop (what a held note sustains on), up to a second of it.
                const double inc = double(inst->rate) / 48000.0 * std::exp2((double(key) - double(z.root)) / 12.0);
                const auto loopFrom = std::size_t(double(z.loopStart) / inc), loopLength = std::size_t(std::min(48000.0, double(z.loopEnd - z.loopStart) / inc));
                const double c = harmonicCents(y, loopFrom, loopLength, key);
                // A voice's glottal pulses are sharp edges of their own: each take is judged
                // against its own first pass (the recording played straight through, up to
                // its loop's end), as the synth patches are.
                const double firstPass = double(z.loopEnd) / double(inst->rate) * 0.98;
                const double edge = worstStep(y, std::size_t(0.3 * 48000), std::size_t(firstPass * 48000));
                const double step = worstStep(y, std::size_t(firstPass * 48000), std::size_t(5.9 * 48000));
                const bool clean = step < std::max(30.0, edge + 6.0);
                ++total;
                tuned += std::abs(c) <= 15.0 ? 1 : 0;
                smoothN += clean ? 1 : 0;
                tuning += " " + std::string(id).substr(6) + "/" + noteName(key) + (z.layer == 0 ? "s" : "f") + " " + juce::String(c, 0).toStdString();
                if (std::abs(c) > 15.0 || !clean)
                    detail += " " + std::string(id) + " " + noteName(key) + " (" + fmt(c) + " cents, step " + fmt(step) + " vs its own " + fmt(edge) + " dB)";
            }
        }
        std::cout << "  [report] voice (cents, its harmonics over its loop, up to 1 s):" << tuning << "\n";
        s.check(total > 0 && tuned >= total - 2, "voice: every vowel note plays in tune (within 15 cents on its loop; at most two drifting takes)",
                std::to_string(tuned) + " of " + std::to_string(total) + detail);
        s.check(total > 0 && smoothN == total, "voice: held 6 s through the loops, no click (no step 6 dB sharper than the take's own)", std::to_string(smoothN) + " of " + std::to_string(total));
        // The soft and full takes crossfade: a velocity sweep rises with no dip over 1.5 dB.
        int rising = 0, sweeps = 0;
        std::string rd;
        for (int key : { 52, 57, 64, 70 }) {
            double last = -200, dip = 0;
            for (int k = 0; k <= 10; ++k) {
                NullPart unused;
                VoicePart part(unused, nullptr);
                part.patch.vowel = 2;
                part.patch.vowelDrift = 0;
                part.patch.attack = 0.01f;
                part.patch.even = 0;
                part.patch.velocityGain = 0.0f;
                part.patch.velocityTone = 0;
                const auto y = partNote(part, float(key - 48), 0.05f + 0.09f * float(k), 2.0, 1.5);
                const double level = rmsDb(y, std::size_t(0.6 * 48000), std::size_t(0.6 * 48000));
                if (last > -199) dip = std::max(dip, last - level);
                last = level;
            }
            ++sweeps;
            rising += dip < 1.5 ? 1 : 0;
            if (dip >= 1.5) rd += " " + noteName(key) + " (dip " + fmt(dip) + " dB)";
        }
        s.check(rising == sweeps, "voice: soft to full rises smoothly with velocity (layers crossfaded, no dip over 1.5 dB)", std::to_string(rising) + " of " + std::to_string(sweeps) + rd);
        // A released note crosses into its recorded tail: no click, gone within 3 s.
        {
            NullPart unused;
            VoicePart part(unused, nullptr);
            part.patch.vowel = 0;
            part.patch.vowelDrift = 0;
            part.patch.release = 0.5f;
            const auto y = partNote(part, float(57 - 48), 0.7f, 2.0, 5.5);
            const double held = rmsDb(y, std::size_t(1.5 * 48000), 9600), after = rmsDb(y, std::size_t(5.0 * 48000), 9600);
            const double step = worstStep(y, std::size_t(1.8 * 48000), std::size_t(3.0 * 48000));
            s.check(after < held - 40.0 && step < 30.0, "voice: a released note fades through its recorded tail, no click",
                    "held " + fmt(held) + " dB, 3 s later " + fmt(after) + " dB, worst step " + fmt(step) + " dB");
        }
        // The vowel morph: "ooh" and "ee" differ (brightness); halfway lies between.
        {
            const auto vowelAt = [&](float vw) {
                NullPart unused;
                VoicePart part(unused, nullptr);
                part.patch.vowel = vw;
                part.patch.vowelDrift = 0;
                return brightness(partNote(part, float(57 - 48), 0.7f, 2.0, 2.0), std::size_t(0.8 * 48000), std::size_t(0.8 * 48000));
            };
            const double ooh = vowelAt(0.0f), mid = vowelAt(1.5f), ee = vowelAt(3.0f);
            s.check(ee > ooh + 3.0 && mid > ooh && mid < ee, "voice: the vowel morphs (\"ooh\" dark, \"ee\" bright, between them in between)",
                    "ooh " + fmt(ooh) + ", halfway " + fmt(mid) + ", ee " + fmt(ee) + " dB above 2 kHz");
        }
        // The ensemble: one voice to a full choir, wider and louder, no click.
        {
            std::vector<double> width, level;
            for (int singers : { 1, 4, 8 }) {
                NullPart unused;
                VoicePart part(unused, nullptr);
                part.patch.vowel = 2;
                part.patch.singers = singers;
                part.patch.spread = 0.8f;
                const float rate = 48000;
                part.prepare(rate);
                PartFrame f;
                f.rate = rate;
                f.beatsPerSecond = 2;
                NoteOn n;
                n.semis = 9;
                n.velocity = 0.7f;
                n.live = 5;
                n.length = liveLength;
                part.update(f);
                part.start(f, n);
                std::vector<float> l(std::size_t(3.0 * rate)), r(l.size());
                for (std::size_t at = 0; at < l.size(); at += std::size_t(dsp::chunk)) {
                    part.update(f);
                    part.render(l.data() + at, r.data() + at, dsp::chunk);
                }
                double mid = 0, side = 0;
                for (std::size_t i = std::size_t(rate); i < l.size(); ++i) {
                    mid += (l[i] + r[i]) * (l[i] + r[i]);
                    side += (l[i] - r[i]) * (l[i] - r[i]);
                }
                width.push_back(toDb(std::sqrt(side / std::max(mid, 1.0e-20))));
                level.push_back(toDb(std::sqrt(mid / double(l.size()))));
            }
            s.check(width[2] > width[0] + 6.0 && level[2] > level[0], "voice: the ensemble (1, 4, 8 singers) grows wider and fuller",
                    "side/mid " + fmt(width[0]) + ", " + fmt(width[1]) + ", " + fmt(width[2]) + " dB; level " + fmt(level[0]) + ", " + fmt(level[1]) + ", " + fmt(level[2]) + " dB");
        }
        // Above the top recording: still the right pitch (shifted, the synthesised choir
        // taking over), audible and finite.
        {
            ChoirPart synth;
            VoicePart part(synth, &synth);
            part.patch.vowel = 2;
            part.prepare(48000);
            const float top = part.top();
            const int key = int(top) + 5;
            const auto y = partNote(part, float(key - 48), 0.7f, 2.0, 2.0);
            const double hz = pitchAt(y, std::size_t(0.8 * 48000), std::size_t(0.8 * 48000), 48000, hzOf(key) * 0.95, hzOf(key) * 1.05);
            s.check(std::abs(cents(hz, hzOf(key))) < 20.0 && rmsDb(y, 48000, 48000) > -50.0 && finite(y),
                    "voice: above the highest recorded note (" + noteName(int(top)) + ") it is extended in tune, blended with the synthesised choir",
                    noteName(key) + " " + fmt(cents(hz, hzOf(key))) + " cents");
        }
    }

    // ---------------------------------------------------------------- the hits
    s.section("recordings: the djembe and banjo layers");
    {
        // Each stroke: velocity steps rise (no dip over 1.5 dB), the three takes differ.
        int ok = 0, total = 0, rr = 0;
        std::string detail;
        for (int stroke : { djembe::BigBass, djembe::BigTone, djembe::BigSlap, djembe::SmallBass, djembe::SmallTone, djembe::SmallSlap }) {
            double last = -200, dip = 0;
            for (int k = 0; k <= 10; ++k) {
                DrumPart unused;
                HitPart part(unused);
                part.patch.rows[0].stroke = stroke;
                part.patch.velocityGain = 0;
                part.patch.even = 0;
                const auto y = partNote(part, 0, 0.05f + 0.09f * float(k), 0.5, 0.4, false);
                const double level = rmsDb(y, 0, std::size_t(0.1 * 48000));
                if (last > -199) dip = std::max(dip, last - level);
                last = level;
            }
            ++total;
            ok += dip < 1.5 ? 1 : 0;
            if (dip >= 1.5) detail += " " + std::to_string(stroke) + " (dip " + fmt(dip) + " dB)";
            std::vector<std::vector<float>> takes;
            for (float pick : { 0.1f, 0.5f, 0.9f }) {
                DrumPart unused;
                HitPart part(unused);
                part.patch.rows[0].stroke = stroke;
                takes.push_back(partNote(part, 0, 0.6f, 0.5, 0.3, false, 0.3f, pick));
            }
            double d = 0;
            for (std::size_t i = 0; i < takes[0].size(); ++i) d += std::abs(takes[0][i] - takes[1][i]) + std::abs(takes[1][i] - takes[2][i]);
            rr += d > 1.0 ? 1 : 0;
        }
        s.check(ok == total, "djembe: every stroke's velocity layers rise smoothly (no dip over 1.5 dB)", std::to_string(ok) + " of " + std::to_string(total) + detail);
        s.check(rr == total, "djembe: three different takes per layer (round-robins)", std::to_string(rr) + " of " + std::to_string(total));
        // The banjo: in tune across its range, layers rising.
        int tuned = 0, notes = 0;
        std::string bd;
        for (int key = 50; key <= 80; key += 3) {
            const auto y = play("banjo", key, 0.6f, 0.5f, 1.0, 1.0, false);
            const double hz = pitchAt(y, std::size_t(0.15 * 48000), std::size_t(0.4 * 48000), 48000, hzOf(key) * 0.97, hzOf(key) * 1.03);
            const double c = cents(hz, hzOf(key));
            ++notes;
            tuned += std::abs(c) <= 15.0 ? 1 : 0;
            bd += " " + noteName(key) + " " + juce::String(c, 0).toStdString();
        }
        std::cout << "  [report] banjo (cents):" << bd << "\n";
        s.check(tuned == notes, "banjo: in tune from D3 to G#5 (within 15 cents)", std::to_string(tuned) + " of " + std::to_string(notes));
    }

    // ---------------------------------------------------------------- the Places
    s.section("recordings: each changed part plays the recordings at its fallback's level");
    for (const auto& x : changed()) {
        if (!placeFilter.empty() && std::find(placeFilter.begin(), placeFilter.end(), x.type) == placeFilter.end()) continue;
        std::string levels;
        bool near = true, audible = true, differs = true, modelKept = true;
        for (int c = 0; c < placeCharacters; ++c) {
            const auto a = elementAudio(x.type, c, x.element, true, 24.0);
            const auto b = elementAudio(x.type, c, x.element, false, 24.0);
            const double la = loudness(a, 2.0), lb = loudness(b, 2.0);
            const auto st = measure(a, 2.0);
            audible = audible && la > -60.0 && st.finite && st.peak < 0.95;
            double diff = 0;
            for (std::size_t i = 0; i < a.l.size(); ++i) diff += std::abs(double(a.l[i]) - double(b.l[i]));
            if (x.uses[std::size_t(c)]) {
                differs = differs && diff > 1.0;
                // A layer adds to the part: at most 3 LU louder; a replacement within 2.5 LU.
                near = near && (x.layer ? (la - lb > -0.5 && la - lb < 3.0) : std::abs(la - lb) < 2.5);
            } else {
                modelKept = modelKept && diff < 1.0e-3;
            }
            levels += (c > 0 ? ", " : "") + std::string(placeInfo(x.type).characters[std::size_t(c)]) + " " + fmt(la) + " (fallback " + fmt(lb) + ")";
        }
        s.check(audible, std::string(x.name) + ": audible, finite, under full scale in every Character", levels);
        s.check(differs, std::string(x.name) + ": plays the recordings where it should");
        s.check(modelKept, std::string(x.name) + ": the Characters that keep the model are unchanged");
        s.check(near, std::string(x.name) + (x.layer ? ": the layer adds at most 3 LU" : ": level-matched to its fallback (within 2.5 LU, LUFS) in every Character that plays the recordings"), levels);
    }

    s.section("recordings: every use of the voice sounds different (per-Place processing)");
    {
        struct Use { int type, element; const char* name; };
        const std::vector<Use> uses { { Castle, CastlePlace::Choir, "Castle Choir" }, { Palace, PalacePlace::Choir, "Palace Choir" },
                                      { Temple, TemplePlace::Choir, "Temple Choir" }, { Temple, TemplePlace::Chant, "Temple Chant Drone" },
                                      { AlienRuins, AlienRuinsPlace::Choir, "Alien Choir" }, { AlienRuins, AlienRuinsPlace::Whispers, "Alien Ruins Whispers" },
                                      { Mining, MiningPlace::Hymn, "Worker Hymn" }, { Nebula, NebulaPlace::SubDrone, "Nebula Deep Hum" },
                                      { Hive, HivePlace::Formants, "Hive Formants" }, { Bridge, BridgePlace::Chatter, "Crew Chatter" } };
        std::vector<Fingerprint> prints;
        for (const auto& u : uses) {
            auto h = instrumentHost(u.type, 0, u.element, { { "p1_activity", 0.7f }, { "spacemix", 0.0f }, { fxId(0, u.element, FxReverb), 0.0f }, { fxId(0, u.element, FxDelay), 0.0f } });
            prints.push_back(fingerprint(h.render(14.0), 2.0));
        }
        double closest = 1.0e9;
        std::string pair;
        for (std::size_t i = 0; i < uses.size(); ++i)
            for (std::size_t j = i + 1; j < uses.size(); ++j) {
                const double d = distance(prints[i], prints[j]);
                if (d < closest) closest = d, pair = std::string(uses[i].name) + " / " + uses[j].name;
            }
        s.metric("closest pair of voice uses (" + pair + ")", closest, "");
        s.check(closest >= 3.0, "voice: no two Places' voices alike (fingerprint distance 3 or more, as the distinctness report counts)", pair + " " + fmt(closest));
    }

    s.section("recordings: KEYS and Instrument mode");
    {
        using namespace instTest;
        // The playable piano: the Mall's E-Piano on an empty night (the factory preset
        // "Piano") plays the recordings, in tune.
        bool presetFound = false;
        for (const auto& p : inst::factoryPresets())
            if (p.name == "Piano" && p.id == inst::Id { Mall, MallPlace::EPiano }) presetFound = true;
        s.check(presetFound, "Instrument mode: a factory preset \"Piano\" (the developer's piano)");
        auto h = ihost(Mall, MallPlace::EPiano);
        h.set("inst_character", 1.0f);
        h.render(0.2);
        note(h, 60, 90);
        const auto a = h.render(1.5);
        samples::Bank::bypassForTests(true);
        auto hb = ihost(Mall, MallPlace::EPiano);
        hb.set("inst_character", 1.0f);
        hb.render(0.2);
        note(hb, 60, 90);
        const auto b = hb.render(1.5);
        samples::Bank::bypassForTests(false);
        double diff = 0;
        for (std::size_t i = 0; i < a.l.size(); ++i) diff += std::abs(double(a.l[i]) - double(b.l[i]));
        const auto m = mono(a, 0.2, 1.0);
        const double hz = pitchAt(m, 0, m.size(), a.rate, hzOf(60) * 0.97, hzOf(60) * 1.03);
        s.check(diff > 1.0 && std::abs(cents(hz, hzOf(60))) < 10.0, "Instrument mode: the piano plays the recordings, in tune (C4)", fmt(cents(hz, hzOf(60))) + " cents");
        // The choirs played from a keyboard sing with the voice.
        for (const auto& id : { inst::Id { Temple, TemplePlace::Choir }, inst::Id { Castle, CastlePlace::Choir } }) {
            auto k = ihost(id);
            note(k, 57, 90);
            const auto x = k.render(2.5);
            samples::Bank::bypassForTests(true);
            auto kb = ihost(id);
            note(kb, 57, 90);
            const auto y = kb.render(2.5);
            samples::Bank::bypassForTests(false);
            double d = 0;
            for (std::size_t i = 0; i < x.l.size(); ++i) d += std::abs(double(x.l[i]) - double(y.l[i]));
            const auto mx = mono(x, 1.0, 1.0);
            const double hx = pitchAt(mx, 0, mx.size(), x.rate, hzOf(57) * 0.97, hzOf(57) * 1.03);
            s.check(d > 1.0 && std::abs(cents(hx, hzOf(57))) < 15.0, std::string("KEYS / Instrument mode: ") + placeNames[std::size_t(id.type)] + " Choir sings with the recorded voice, in tune",
                    fmt(cents(hx, hzOf(57))) + " cents");
        }
    }

    // ---------------------------------------------------------------- listening renders
    s.section("recordings: listening renders (test logs, renders/)");
    {
        using namespace instTest;
        int written = 0;
        // The piano: a chromatic run soft to full, chords on the pedal, a melody.
        {
            auto h = ihost(Mall, MallPlace::EPiano, true);
            h.set("inst_character", 1.0f);
            h.render(0.2);
            std::vector<float> l, r;
            for (int key = 21; key <= 108; ++key) {
                const int v = 12 + (key - 21) * 112 / 87;
                note(h, key, v);
                const auto a = h.render(0.06);
                off(h, key);
                append(l, r, a, 0.0, 0.0);
                append(l, r, h.render(0.04), 0.0, 0.0);
            }
            append(l, r, h.render(2.0), 0.0, 0.5);
            written += writeWav("piano-chromatic-soft-to-full.wav", l, r, h.rate) ? 1 : 0;
        }
        {
            auto h = ihost(Mall, MallPlace::EPiano, true);
            h.set("inst_character", 1.0f);
            h.render(0.2);
            std::vector<float> l, r;
            const std::vector<std::vector<int>> chords { { 36, 48, 52, 55, 60 }, { 41, 53, 57, 60, 65 }, { 43, 55, 59, 62, 67 }, { 36, 48, 52, 55, 64 } };
            for (const auto& c : chords) {
                cc(h, 64, 0);
                append(l, r, h.render(0.02), 0.0, 0.0);
                for (int k : c) note(h, k, 70);
                append(l, r, h.render(0.05), 0.0, 0.0);
                cc(h, 64, 127);
                append(l, r, h.render(0.3), 0.0, 0.0);
                for (int k : c) off(h, k);
                append(l, r, h.render(1.8), 0.0, 0.0);
            }
            cc(h, 64, 0);
            append(l, r, h.render(2.5), 0.0, 0.5);
            written += writeWav("piano-pedal-chords.wav", l, r, h.rate) ? 1 : 0;
        }
        {
            auto h = ihost(Mall, MallPlace::EPiano, true);
            h.set("inst_character", 1.0f);
            h.render(0.2);
            std::vector<float> l, r;
            // An original little tune over a left hand (quarter notes at 90 bpm).
            const std::vector<std::pair<int, double>> tune { { 64, 1 }, { 67, 0.5 }, { 69, 0.5 }, { 71, 1 }, { 69, 1 }, { 67, 1.5 }, { 64, 0.5 },
                                                             { 62, 1 }, { 64, 1 }, { 60, 2 }, { 67, 1 }, { 72, 1 }, { 71, 0.5 }, { 69, 0.5 }, { 67, 2 } };
            const double beat = 60.0 / 90.0;
            int bar = 0;
            double t = 0;
            for (const auto& [k, len] : tune) {
                if (t >= double(bar) * 4.0 - 1.0e-9) {
                    const int bass = (bar % 2 == 0) ? 36 : 43;
                    note(h, bass, 55);
                    note(h, bass + 12, 50);
                    ++bar;
                }
                note(h, k, 64 + int(20.0 * std::sin(t)));
                append(l, r, h.render(len * beat * 0.92), 0.0, 0.0);
                off(h, k);
                if (std::fmod(t + len, 4.0) < 1.0e-6) off(h, (bar % 2 == 1) ? 36 : 43), off(h, (bar % 2 == 1) ? 48 : 55);
                append(l, r, h.render(len * beat * 0.08), 0.0, 0.0);
                t += len;
            }
            for (int k = 0; k < 128; ++k) off(h, k);
            append(l, r, h.render(2.5), 0.0, 0.5);
            written += writeWav("piano-melody.wav", l, r, h.rate) ? 1 : 0;
        }
        // The voice choir, one singer to the full ensemble on a held chord.
        {
            std::vector<float> l, r;
            for (int singers : { 1, 2, 4, 6, 8 }) {
                std::array<std::unique_ptr<NullPart>, 3> nulls;
                std::array<std::unique_ptr<VoicePart>, 3> parts;
                const float rate = 48000;
                std::vector<float> pl(std::size_t(3.5 * rate)), pr(pl.size());
                for (int k = 0; k < 3; ++k) {
                    nulls[std::size_t(k)] = std::make_unique<NullPart>();
                    parts[std::size_t(k)] = std::make_unique<VoicePart>(*nulls[std::size_t(k)], nullptr);
                    auto& p = *parts[std::size_t(k)];
                    p.patch.vowel = 2.0f;
                    p.patch.singers = singers;
                    p.patch.attack = 0.4f;
                    p.patch.spread = 0.8f;
                    p.prepare(rate);
                    PartFrame f;
                    f.rate = rate;
                    f.beatsPerSecond = 2;
                    NoteOn n;
                    n.semis = float(std::array<int, 3> { 0, 4, 7 }[std::size_t(k)]) + 2.0f;
                    n.velocity = 0.7f;
                    n.random = 0.2f + 0.3f * float(k);
                    n.length = 5.0;
                    n.seconds = 2.5f;
                    p.update(f);
                    p.start(f, n);
                    for (std::size_t at = 0; at < pl.size(); at += std::size_t(dsp::chunk)) {
                        f.beat = double(at) / rate * 2.0;
                        p.update(f);
                        p.render(pl.data() + at, pr.data() + at, dsp::chunk);
                    }
                }
                for (std::size_t i = 0; i < pl.size(); ++i) l.push_back(pl[i] * 0.5f), r.push_back(pr[i] * 0.5f);
            }
            written += writeWav("voice-choir-1-2-4-6-8-singers.wav", l, r, 48000) ? 1 : 0;
        }
        // Each changed part: before (the synthesised fallback), then after (the recordings),
        // in a Character that plays them.
        for (const auto& x : changed()) {
            int c = 0;
            while (c < 2 && !x.uses[std::size_t(c)]) ++c;
            std::vector<float> l, r;
            append(l, r, elementAudio(x.type, c, x.element, false, 10.0), 2.0, 1.0);
            append(l, r, elementAudio(x.type, c, x.element, true, 10.0), 2.0, 0.5);
            juce::String name = juce::String(placeKeys[std::size_t(x.type)]) + "-" + juce::String(placeInfo(x.type).element[std::size_t(x.element)].name).toLowerCase().replace(" ", "-") +
                                "-before-after.wav";
            written += writeWav(name, l, r, 48000) ? 1 : 0;
        }
        s.check(written == int(changed().size()) + 4, "listening renders written (piano run, pedal chords, melody; the choir 1 to 8 singers; every changed part before and after)",
                std::to_string(written) + " files in " + rendersFolder().getFullPathName().toStdString());
    }
}
