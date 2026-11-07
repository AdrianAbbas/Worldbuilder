// The woodwinds pass: the woodwind parts on VSCO 2 Community Edition's woodwinds (CC0).
// Included by WorldbuilderTests.cpp after the level tests (it uses the recordings tests' helpers).
//
//   --only woodwinds   the checks: every woodwind set loads and plays in tune (a cents
//                      report), its dynamics rise smoothly (velocity and the mod wheel),
//                      nothing clicks (loops, note-offs, legato), each changed part sits at
//                      its synthesised stand-in's level, the Places keep their identity, the
//                      CPU stays within the budget; listening renders before and after
//   --only wwrange     the notes each woodwind part plays (MIDI), per Character
//   --only wwaudit     each changed part's level with the recordings and without

namespace wwTest {
using namespace recTest;

// A Place's woodwind part, and the woodwind it plays.
struct Woodwind {
    int type, element;
    const char* name;
};
const std::vector<Woodwind>& woodwinds() {
    static const std::vector<Woodwind> list {
        { Village, VillagePlace::Recorder, "Village Recorder" },
        { Forest, ForestPlace::Flute, "Forest Flute" },
        { Desert, DesertPlace::Ney, "Desert Ney Flute" },
        { Mining, MiningPlace::Beacon, "Asteroid Mining Beacon" },
    };
    return list;
}

// The lowest and highest MIDI note a part plays alone in a Character over a range of
// settings (activity, tension, the key).
std::pair<int, int> noteRange(int type, int character, int element, float transpose) {
    int lo = 127, hi = 0;
    for (float act : { 0.2f, 0.6f, 0.95f })
        for (float ten : { 0.1f, 0.9f })
            for (int root : { 0, 5, 11 }) {
                auto h = instrumentHost(type, character, element, { { "p1_activity", act }, { "p1_tension", ten }, { "root", float(root) } });
                h.render(30.0);
                for (const auto& e : placeLog(h, element)) {
                    const int midi = int(std::lround(48.0f + float(root) + e.pitch + transpose));
                    lo = std::min(lo, midi);
                    hi = std::max(hi, midi);
                }
            }
    return { lo, hi };
}
} // namespace wwTest

void probeWoodwindRanges() {
    using namespace wwTest;
    for (const auto& w : woodwinds()) {
        std::string line = std::string(w.name) + ":";
        for (int c = 0; c < placeCharacters; ++c) {
            auto h = instrumentHost(w.type, c, w.element);
            h.render(0.1);
            const float transpose = h.p->engine.placeSlot(0).place()->partTranspose(w.element);
            const auto [lo, hi] = noteRange(w.type, c, w.element, transpose);
            line += "  " + std::string(placeInfo(w.type).characters[std::size_t(c)]) + " " + recTest::noteName(lo) + "-" + recTest::noteName(hi) + " (" + std::to_string(lo) + "-" +
                    std::to_string(hi) + ", transpose " + fmt(transpose) + ")";
        }
        std::cout << line << std::endl;
    }
}

// --only wwaudit: each woodwind part's loudness (LUFS) with the recordings and without (its
// synthesised stand-in), per Character, and the gain that would match them.
void auditWoodwinds() {
    using namespace wwTest;
    samples::Bank::get().load();
    samples::Bank::get().waitUntilLoaded(60000);
    for (const auto& w : woodwinds()) {
        std::string line = std::string(w.name) + ":";
        for (int c = 0; c < placeCharacters; ++c) {
            const double with = loudness(elementAudio(w.type, c, w.element, true, 26.0), 2.0);
            const double without = loudness(elementAudio(w.type, c, w.element, false, 26.0), 2.0);
            line += "  " + std::string(placeInfo(w.type).characters[std::size_t(c)]) + " " + fmt(with) + " vs " + fmt(without) + " (gain x" +
                    juce::String(std::pow(10.0, (without - with) / 20.0), 4).toStdString() + ")";
        }
        std::cout << line << std::endl;
    }
}

namespace wwTest {
// The held (looped) and staccato sets.
const std::vector<const char*>& heldSets() {
    static const std::vector<const char*> s { "ww-flute", "ww-flute-vib", "ww-piccolo", "ww-oboe", "ww-oboe-vib", "ww-clarinet", "ww-bassoon", "ww-bassoon-vib" };
    return s;
}
const std::vector<const char*>& shortSets() {
    static const std::vector<const char*> s { "ww-flute-stac", "ww-piccolo-stac", "ww-oboe-stac", "ww-clarinet-stac", "ww-bassoon-stac" };
    return s;
}

// A woodwind patch alone, neutral (full tone, nothing evened, no breath or key noise), for
// measuring the recordings.
SamplePatch neutral(const char* id, bool held) {
    auto p = woodwindPatch(held ? id : nullptr, nullptr);
    p.instrument = id;
    p.held = held;
    p.even = 0;
    p.spread = 0;
    p.toneHz = 20000;
    p.velocityTone = 0;
    p.breath = 0;
    p.keyNoise = 0;
    p.mono = false;
    return p;
}

// The round-robin pick of the notes playNotes plays.
float notePick = 0.25f;

// A sampled part (mono sum, 48 kHz, 2 beats per second, root C) playing pattern notes
// {MIDI key, start beat, beats, velocity}, the mod wheel following `wheel(seconds)`.
std::vector<float> playNotes(const SamplePatch& patch, const std::vector<std::array<double, 4>>& notes, double seconds,
                             const std::function<float(double)>& wheel = {}, SamplePart** keep = nullptr) {
    static PluckPart unused;
    static std::unique_ptr<SamplePart> part;
    part = std::make_unique<SamplePart>(unused);
    part->patch = patch;
    const float rate = 48000;
    part->prepare(rate);
    PartFrame f;
    f.rate = rate;
    f.root = 0;
    f.beatsPerSecond = 2;
    std::vector<float> l(std::size_t(seconds * rate), 0.0f), r(l.size(), 0.0f);
    std::size_t next = 0;
    for (std::size_t at = 0; at < l.size(); at += std::size_t(dsp::chunk)) {
        f.beat = double(at) / rate * f.beatsPerSecond;
        if (wheel) part->mod = wheel(double(at) / rate);
        part->update(f);
        while (next < notes.size() && notes[next][1] < f.beat + double(dsp::chunk) / rate * f.beatsPerSecond - 1.0e-9) {
            NoteOn n;
            n.semis = float(notes[next][0] - 48.0);
            n.beat = notes[next][1];
            n.length = notes[next][2];
            n.seconds = float(n.length / f.beatsPerSecond);
            n.velocity = float(notes[next][3]);
            n.random = 0.3f;
            n.random2 = notePick;
            part->start(f, n);
            ++next;
        }
        part->render(l.data() + at, r.data() + at, int(std::min<std::size_t>(std::size_t(dsp::chunk), l.size() - at)));
    }
    for (std::size_t i = 0; i < l.size(); ++i) l[i] += r[i];
    if (keep != nullptr) *keep = part.get();
    return l;
}

// One take alone: its layer's velocity, its round-robin pick, no crossfade.
std::vector<float> playTake(const char* id, bool held, const samples::Instrument& inst, const samples::Zone& z, double beats, double seconds) {
    auto p = neutral(id, held);
    p.crossfade = false;
    p.expression = false;
    int takes = 0;
    for (const auto& q : inst.zones) takes += (q.layer == z.layer && std::abs(q.root - z.root) < 0.5f) ? 1 : 0;
    notePick = (float(z.rr) + 0.5f) / float(std::max(takes, 1));
    const auto y = playNotes(p, { { { double(std::lround(z.root)), 0.0, beats, 0.5 * double(z.loVel + z.hiVel) } } }, seconds);
    notePick = 0.25f;
    return y;
}

// A Place at its defaults (every instrument), with the woodwinds or without (their stand-ins).
Audio placeAudio(int type, int character, bool woodwinds, double seconds) {
    samples::Bank::hideForTests(woodwinds ? nullptr : "ww-");
    auto h = placeHost(type, character);
    auto a = h.render(seconds);
    samples::Bank::hideForTests(nullptr);
    return a;
}
// One woodwind part alone (activity 0.9), with the woodwinds or without.
Audio partAudio(int type, int character, int element, bool woodwinds, double seconds) {
    samples::Bank::hideForTests(woodwinds ? nullptr : "ww-");
    auto h = instrumentHost(type, character, element, { { "p1_activity", 0.9f } });
    auto a = h.render(seconds);
    samples::Bank::hideForTests(nullptr);
    return a;
}

// Where y, from `from` on, has fallen `below` dB under its loudest 10 ms for good (clicks are
// judged while the note is still well above the floor: in silence any step stands out).
std::size_t audibleUntil(const std::vector<float>& y, std::size_t from, double below = 50.0) {
    const std::size_t w = 480;
    double top = -300;
    for (std::size_t i = 0; i + w <= y.size(); i += w) top = std::max(top, rmsDb(y, i, w));
    std::size_t last = from;
    for (std::size_t i = from; i + w <= y.size(); i += w)
        if (rmsDb(y, i, w) > top - below) last = i + w;
    return last;
}

double maxAbsDiff(const Audio& a, const Audio& b) {
    double d = 0;
    for (std::size_t i = 0; i < std::min(a.l.size(), b.l.size()); ++i) d = std::max({ d, std::abs(double(a.l[i]) - double(b.l[i])), std::abs(double(a.r[i]) - double(b.r[i])) });
    return d;
}
} // namespace wwTest

void testWoodwinds(Suite& s) {
    using namespace wwTest;
    auto& bank = samples::Bank::get();
    bank.load();
    const bool have = bank.waitUntilLoaded(60000);
    s.section("woodwinds: VSCO 2 Community Edition's woodwinds in the bank");
    if (!have || !expectSamples || bank.instrument("ww-flute") == nullptr) {
        std::cout << "  [report] no woodwind samples: the synthesised stand-ins play (checked by the Place groups)\n";
        // Without the samples every woodwind part is its stand-in, bit for bit.
        bool same = true;
        for (const auto& w : woodwinds()) same = same && maxAbsDiff(partAudio(w.type, 0, w.element, true, 6.0), partAudio(w.type, 0, w.element, false, 6.0)) == 0.0;
        s.check(same, "without the woodwinds every woodwind part plays its synthesised stand-in");
        return;
    }
    {
        int sets = 0, loops = 0, held = 0;
        std::size_t bytes = 0;
        for (const auto* id : heldSets()) {
            const auto* i = bank.instrument(id);
            if (i == nullptr) continue;
            ++sets;
            for (const auto& z : i->zones) {
                ++held;
                loops += (z.loops() && z.release > 0) ? 1 : 0;
                bytes += std::size_t(z.length) * 2;
            }
        }
        for (const auto* id : shortSets())
            if (const auto* i = bank.instrument(id)) {
                ++sets;
                for (const auto& z : i->zones) bytes += std::size_t(z.length) * 2;
            }
        s.check(sets == 13, "all thirteen woodwind sets load (flute, piccolo, oboe, clarinet, bassoon: sustains, vibrato, staccatos)", std::to_string(sets) + " of 13");
        s.check(held > 0 && loops == held, "every sustained woodwind note has a loop and its recorded end", std::to_string(loops) + " of " + std::to_string(held));
        int layered = 0;
        for (const char* id : { "ww-flute", "ww-flute-vib", "ww-flute-stac", "ww-oboe", "ww-oboe-vib", "ww-oboe-stac", "ww-clarinet", "ww-clarinet-stac", "ww-bassoon", "ww-bassoon-vib",
                                "ww-bassoon-stac", "ww-piccolo" })
            if (const auto* i = bank.instrument(id)) layered += (i->layers >= 2 && i->crossfade) ? 1 : 0;
        s.check(layered == 12, "the dynamics are crossfaded velocity layers (two or three per set; the piccolo's staccatos have one)", std::to_string(layered) + " of 12");
        s.metric("woodwinds: memory (shared by every instance)", double(bytes) / 1048576.0, " MB");
        s.metric("bank memory with the woodwinds", double(bank.memoryBytes()) / 1048576.0, " MB");
    }

    s.section("woodwinds: every note plays in tune (cents from equal temperament)");
    {
        int tuned = 0, total = 0;
        std::string detail;
        for (const auto* id : heldSets()) {
            const auto* inst = bank.instrument(id);
            if (inst == nullptr) continue;
            std::string line;
            std::vector<std::pair<float, int>> seen;
            for (const auto& z : inst->zones) {
                const std::pair<float, int> take { z.root, z.length };
                if (std::find(seen.begin(), seen.end(), take) != seen.end()) continue;   // a derived layer: the same take
                seen.push_back(take);
                const int key = int(std::lround(z.root));
                const auto y = playTake(id, true, *inst, z, 8.0, 3.5);
                const double inc = double(inst->rate) / 48000.0 * std::exp2((double(key) - double(z.root)) / 12.0);
                const auto from = std::size_t(double(z.loopStart) / inc), length = std::size_t(std::min(48000.0, double(z.loopEnd - z.loopStart) / inc));
                const double c = harmonicCents(y, from, length, key);
                ++total;
                tuned += std::abs(c) <= 10.0 ? 1 : 0;
                line += " " + noteName(key) + (z.layer == 0 ? "p" : "f") + " " + juce::String(c, 0).toStdString();
                if (std::abs(c) > 10.0) detail += " " + std::string(id) + " " + noteName(key) + " " + fmt(c);
            }
            std::cout << "  [report] " << id << " (cents, on its loop):" << line << "\n";
        }
        s.check(total > 0 && tuned == total, "every sustained note in tune on its loop (within 10 cents)", std::to_string(tuned) + " of " + std::to_string(total) + detail);
        int stTuned = 0, stTotal = 0;
        std::string stDetail;
        for (const auto* id : shortSets()) {
            const auto* inst = bank.instrument(id);
            if (inst == nullptr) continue;
            std::string line;
            for (const auto& z : inst->zones) {
                const int key = int(std::lround(z.root));
                const auto y = playTake(id, false, *inst, z, 0.5, 0.6);
                const double c = harmonicCents(y, std::size_t(0.03 * 48000), std::size_t(0.2 * 48000), key);
                ++stTotal;
                stTuned += std::abs(c) <= 15.0 ? 1 : 0;
                line += " " + noteName(key) + " " + juce::String(c, 0).toStdString();
                if (std::abs(c) > 15.0) stDetail += " " + std::string(id) + " " + noteName(key) + " " + fmt(c);
            }
            std::cout << "  [report] " << id << " (cents, 30-230 ms):" << line << "\n";
        }
        s.check(stTotal > 0 && stTuned >= stTotal - 2, "every staccato in tune (within 15 cents over its first 200 ms; at most two short attacks off)",
                std::to_string(stTuned) + " of " + std::to_string(stTotal) + stDetail);
    }

    s.section("woodwinds: the dynamics rise smoothly (velocity layers, the mod wheel as expression)");
    {
        int rising = 0, sweeps = 0;
        std::string rd;
        for (const char* id : { "ww-flute", "ww-flute-vib", "ww-oboe", "ww-clarinet", "ww-bassoon", "ww-flute-stac", "ww-clarinet-stac" }) {
            const auto* inst = bank.instrument(id);
            if (inst == nullptr) continue;
            const bool held = std::string(id).find("stac") == std::string::npos;
            for (float at : { 0.2f, 0.5f, 0.8f }) {
                const int key = int(std::lround(inst->lowRoot + at * (inst->highRoot - inst->lowRoot)));
                double last = -200, dip = 0, jump = 0, first = 0;
                for (int k = 0; k <= 10; ++k) {
                    const double v = 0.05 + 0.095 * k;
                    const auto y = playNotes(neutral(id, held), { { { double(key), 0.0, held ? 4.0 : 0.5, v } } }, held ? 1.6 : 0.6);
                    Audio a;
                    a.rate = 48000;
                    a.l = y;
                    a.r = y;
                    const double l = loudness(a, 0.0, true);
                    if (k == 0) first = l;
                    else dip = std::max(dip, last - l), jump = std::max(jump, l - last);
                    last = l;
                }
                ++sweeps;
                // (Staccatos: two different short takes crossfaded peak a little apart, so their
                // loudest 400 ms may sit up to 2 dB under either's.)
                const bool ok = dip < (held ? 0.75 : 2.0) && jump < 4.0 && last - first > 4.0;
                rising += ok ? 1 : 0;
                if (!ok) rd += " " + std::string(id) + " " + noteName(key) + " (dip " + fmt(dip) + ", jump " + fmt(jump) + ", range " + fmt(last - first) + " dB)";
            }
        }
        s.check(sweeps > 0 && rising == sweeps, "a velocity sweep (soft to full) rises evenly: no dip over 0.75 dB (staccatos 2 dB), no step over 4 dB, at least 4 dB from soft to full",
                std::to_string(rising) + " of " + std::to_string(sweeps) + rd);
        // The mod wheel raised over 4 s on a soft held note: the level and tone rise
        // smoothly through the layers (no jump, no dip, no click).
        int smoothN = 0, total = 0;
        std::string md;
        for (const char* id : { "ww-flute", "ww-oboe", "ww-clarinet", "ww-bassoon", "ww-flute-vib" }) {
            const auto* inst = bank.instrument(id);
            if (inst == nullptr) continue;
            for (float at : { 0.3f, 0.7f }) {
                const int key = int(std::lround(inst->lowRoot + at * (inst->highRoot - inst->lowRoot)));
                const auto y = playNotes(neutral(id, true), { { { double(key), 0.0, 14.0, 0.15 } } }, 6.0, [](double t) { return float(std::clamp((t - 1.0) / 4.0, 0.0, 1.0)); });
                // The level over 250 ms windows every 50 ms, from 0.8 s (the attack over) to 5.2 s
                // (a recorded vibrato's own pulse averages out).
                // (A recorded vibrato's level swells: 500 ms windows.)
                const bool vibrato = std::string(id).find("vib") != std::string::npos;
                const double window = vibrato ? 0.5 : 0.25;
                std::vector<double> lv;
                for (double t = 0.8; t + window <= 5.2; t += 0.05) lv.push_back(rmsDb(y, std::size_t(t * 48000), std::size_t(window * 48000)));
                double rise = 0, fall = 0;
                for (std::size_t i = 1; i < lv.size(); ++i) rise = std::max(rise, lv[i] - lv[i - 1]), fall = std::max(fall, lv[i - 1] - lv[i]);
                const double edge = worstStep(y, std::size_t(0.3 * 48000), std::size_t(0.95 * 48000));
                const double step = worstStep(y, std::size_t(1.0 * 48000), std::size_t(5.5 * 48000));
                const bool ok = lv.back() - lv.front() > 5.0 && rise < 1.5 && fall < (vibrato ? 1.5 : 1.0) && step < std::max(30.0, edge + 6.0);
                ++total;
                smoothN += ok ? 1 : 0;
                if (!ok) md += " " + std::string(id) + " " + noteName(key) + " (+" + fmt(lv.back() - lv.front()) + " dB, rise " + fmt(rise) + ", fall " + fmt(fall) + ", step " + fmt(step) + " vs " + fmt(edge) + ")";
            }
        }
        s.check(total > 0 && smoothN == total, "the mod wheel (expression) moves a sounding note through its layers smoothly: louder by 5 dB or more, no rise over 1.5 dB per 50 ms, no dip, no click",
                std::to_string(smoothN) + " of " + std::to_string(total) + md);
    }

    s.section("woodwinds: no clicks (loops, note-offs, legato, staccato ends)");
    {
        // Every sustained take held 6 s through its loop, judged against its own first pass.
        int clean = 0, total = 0;
        std::string detail;
        for (const auto* id : heldSets()) {
            const auto* inst = bank.instrument(id);
            if (inst == nullptr) continue;
            std::vector<std::pair<float, int>> seen;
            for (const auto& z : inst->zones) {
                const std::pair<float, int> take { z.root, z.length };
                if (std::find(seen.begin(), seen.end(), take) != seen.end()) continue;
                seen.push_back(take);
                const int key = int(std::lround(z.root));
                const auto y = playTake(id, true, *inst, z, 12.0, 6.5);
                const double firstPass = double(z.loopEnd) / double(inst->rate) * 0.98;
                const double edge = worstStep(y, std::size_t(0.3 * 48000), std::size_t(firstPass * 48000));
                const double step = worstStep(y, std::size_t(firstPass * 48000), std::size_t(6.0 * 48000));
                const bool ok = step < std::max(30.0, edge + 6.0);
                ++total;
                clean += ok ? 1 : 0;
                if (!ok) detail += " " + std::string(id) + " " + noteName(key) + " (" + fmt(step) + " vs its own " + fmt(edge) + " dB)";
            }
        }
        s.check(total > 0 && clean == total, "held 6 s through the loops, no click (no step 6 dB sharper than the take's own)", std::to_string(clean) + " of " + std::to_string(total) + detail);
        // A note let go after 1.3 s crosses into its recorded end: no click, quiet within 1.5 s.
        int ends = 0, endTotal = 0;
        std::string ed;
        for (const auto* id : heldSets()) {
            const auto* inst = bank.instrument(id);
            if (inst == nullptr) continue;
            for (float at : { 0.15f, 0.5f, 0.85f }) {
                const int key = int(std::lround(inst->lowRoot + at * (inst->highRoot - inst->lowRoot)));
                const auto y = playNotes(neutral(id, true), { { { double(key), 0.0, 2.6, 0.7 } } }, 3.2);
                const double edge = worstStep(y, std::size_t(0.3 * 48000), std::size_t(1.2 * 48000));
                const double step = worstStep(y, std::size_t(1.25 * 48000), std::max(audibleUntil(y, std::size_t(1.25 * 48000)), std::size_t(1.3 * 48000)));
                const double heldDb = rmsDb(y, std::size_t(0.9 * 48000), 9600), after = rmsDb(y, std::size_t(2.8 * 48000), 9600);
                const bool ok = step < std::max(30.0, edge + 6.0) && after < heldDb - 40.0;
                ++endTotal;
                ends += ok ? 1 : 0;
                if (!ok) ed += " " + std::string(id) + " " + noteName(key) + " (step " + fmt(step) + " vs " + fmt(edge) + ", " + fmt(after - heldDb) + " dB 1.5 s on)";
            }
        }
        s.check(endTotal > 0 && ends == endTotal, "a released note ends through its recorded release: no click, 40 dB down within 1.5 s", std::to_string(ends) + " of " + std::to_string(endTotal) + ed);
        // Legato: one line, each note slurring into the next (up and down, a step and a leap).
        int slurs = 0, slurTotal = 0;
        std::string sd;
        for (const char* id : { "ww-flute", "ww-flute-vib", "ww-oboe", "ww-clarinet", "ww-bassoon" }) {
            const auto* inst = bank.instrument(id);
            if (inst == nullptr) continue;
            auto p = neutral(id, true);
            p.mono = true;
            const int base = int(std::lround(inst->lowRoot + 0.4f * (inst->highRoot - inst->lowRoot)));
            const auto y = playNotes(p, { { { double(base), 0.0, 2.0, 0.7 } }, { { double(base + 2), 2.0, 2.0, 0.7 } }, { { double(base + 7), 4.0, 2.0, 0.7 } },
                                          { { double(base + 3), 6.0, 2.0, 0.7 } }, { { double(base), 8.0, 2.0, 0.7 } } }, 5.6);
            const double edge = worstStep(y, std::size_t(0.3 * 48000), std::size_t(0.95 * 48000));
            double worst = -200, dip = 0;
            for (double t : { 1.0, 2.0, 3.0, 4.0 }) {
                worst = std::max(worst, worstStep(y, std::size_t((t - 0.05) * 48000), std::size_t((t + 0.25) * 48000)));
                // No hole between the notes: through the hand-over (20 ms windows) never more than
                // 3 dB under the quieter of the two notes (the notes' own levels differ by register).
                const double quieter = std::min(rmsDb(y, std::size_t((t - 0.3) * 48000), 4800), rmsDb(y, std::size_t((t + 0.2) * 48000), 4800));
                for (double u = t - 0.04; u < t + 0.14; u += 0.01) dip = std::max(dip, quieter - rmsDb(y, std::size_t(u * 48000), 960));
            }
            const bool ok = worst < std::max(30.0, edge + 6.0) && dip < 3.0;
            ++slurTotal;
            slurs += ok ? 1 : 0;
            if (!ok) sd += " " + std::string(id) + " (step " + fmt(worst) + " vs " + fmt(edge) + ", dip " + fmt(dip) + " dB)";
        }
        s.check(slurTotal > 0 && slurs == slurTotal, "legato: each note slurs into the next with no click and no hole (never 3 dB under the quieter note)", std::to_string(slurs) + " of " + std::to_string(slurTotal) + sd);
        // Staccatos end cleanly (faded, no click in their last part).
        int stClean = 0, stTotal = 0;
        std::string st;
        for (const auto* id : shortSets()) {
            const auto* inst = bank.instrument(id);
            if (inst == nullptr) continue;
            for (const auto& z : inst->zones) {
                const int key = int(std::lround(z.root));
                const auto y = playTake(id, false, *inst, z, 4.0, double(z.length) / double(inst->rate) + 0.2);
                const double from = 0.5 * double(z.length) / double(inst->rate);
                const double edge = worstStep(y, std::size_t(0.01 * 48000), std::size_t(from * 48000));
                // (Judged while it is within 50 dB of its peak: past that, its last fade into silence.)
                const std::size_t until = audibleUntil(y, std::size_t(from * 48000));
                const double step = until > std::size_t(from * 48000) + 1440 ? worstStep(y, std::size_t(from * 48000), until) : -200.0;
                ++stTotal;
                const bool ok = step < std::max(30.0, edge + 6.0);
                stClean += ok ? 1 : 0;
                if (!ok) st += " " + std::string(id) + " " + noteName(key) + " (" + fmt(step) + " vs " + fmt(edge) + ")";
            }
        }
        s.check(stTotal > 0 && stClean == stTotal, "every staccato rings out and ends with no click", std::to_string(stClean) + " of " + std::to_string(stTotal) + st);
        // One line: a new note takes over (the old one fades within 0.1 s).
        SamplePart* part = nullptr;
        auto p = neutral("ww-flute", true);
        p.mono = true;
        playNotes(p, { { { 72.0, 0.0, 4.0, 0.7 } }, { { 76.0, 0.5, 4.0, 0.7 } } }, 0.5, {}, &part);
        s.check(part != nullptr && part->sounding() == 1, "one line, as the model plays it: a note starting over another takes over from it",
                std::to_string(part != nullptr ? part->sounding() : -1) + " sounding 0.25 s after");
    }

    s.section("woodwinds: the Places' woodwind parts play the recordings, level-matched to their stand-ins");
    for (const auto& w : woodwinds()) {
        std::string levels;
        bool near = true, audible = true, differs = true;
        for (int c = 0; c < placeCharacters; ++c) {
            const auto a = partAudio(w.type, c, w.element, true, 24.0);
            const auto b = partAudio(w.type, c, w.element, false, 24.0);
            const double la = loudness(a, 2.0), lb = loudness(b, 2.0);
            const auto st = measure(a, 2.0);
            audible = audible && la > -60.0 && st.finite && st.peak < 0.95;
            differs = differs && maxAbsDiff(a, b) > 1.0e-3;
            near = near && std::abs(la - lb) < 1.5;
            levels += (c > 0 ? ", " : "") + std::string(placeInfo(w.type).characters[std::size_t(c)]) + " " + fmt(la) + " (stand-in " + fmt(lb) + ")";
        }
        s.check(audible, std::string(w.name) + ": audible, finite, under full scale in every Character", levels);
        s.check(differs, std::string(w.name) + ": plays the recordings in every Character");
        s.check(near, std::string(w.name) + ": level-matched to its synthesised stand-in (within 1.5 LU, LUFS) in every Character", levels);
    }

    s.section("woodwinds: each part plays in the key (its notes' harmonics included)");
    for (const auto& w : woodwinds()) {
        std::string line;
        bool ok = true;
        for (int c = 0; c < placeCharacters; ++c) {
            const auto with = keyFit(chroma(partAudio(w.type, c, w.element, true, 24.0), 2.0, 100.0, 5000.0), 2, 0);
            const auto without = keyFit(chroma(partAudio(w.type, c, w.element, false, 24.0), 2.0, 100.0, 5000.0), 2, 0);
            // As clearly in D minor as the Places are asked to be (5 dB), or as its stand-in.
            ok = ok && (with.meanGap > 5.0 || with.meanGap > without.meanGap - 1.0) && with.maxGap > 3.0;
            line += (c > 0 ? ", " : "") + std::string(placeInfo(w.type).characters[std::size_t(c)]) + " " + fmt(with.meanGap) + " / " + fmt(with.maxGap) + " dB (stand-in " +
                    fmt(without.meanGap) + ")";
        }
        s.check(ok, std::string(w.name) + ": in D minor in every Character (the in-scale classes 5 dB or more over the rest on average)", line);
    }

    s.section("woodwinds: each Place at its level with the other parts, and still itself");
    {
        // The whole Place at its defaults (every instrument): as loud as with the stand-in,
        // and the woodwind's share of it the same (realistic loudness: BS.1770, gated).
        for (const auto& w : woodwinds()) {
            std::string line;
            bool same = true;
            for (int c = 0; c < placeCharacters; ++c) {
                const double with = loudness(placeAudio(w.type, c, true, 24.0), 4.0), without = loudness(placeAudio(w.type, c, false, 24.0), 4.0);
                same = same && std::abs(with - without) < 0.75;
                line += (c > 0 ? ", " : "") + std::string(placeInfo(w.type).characters[std::size_t(c)]) + " " + fmt(with) + " (" + fmt(without) + ")";
            }
            s.check(same, std::string(placeNames[std::size_t(w.type)]) + ": the whole Place as loud as with the stand-in (within 0.75 LU) in every Character", line);
        }
        // Identity: each changed Place (all its instruments, dry) is nearer to itself with the
        // stand-in than to any other Place.
        std::vector<int> types = builtPlaces();
        const auto print = [&](int type, bool woodwinds) {
            samples::Bank::hideForTests(woodwinds ? nullptr : "ww-");
            auto h = placeHost(type, 0, { { "p1_activity", 0.7f }, { "spacemix", 0.0f } });
            auto f = fingerprint(h.render(14.0), 2.0);
            samples::Bank::hideForTests(nullptr);
            return f;
        };
        std::vector<Fingerprint> others;
        for (int t : types) others.push_back(print(t, true));
        for (const auto& w : woodwinds()) {
            const auto with = print(w.type, true), without = print(w.type, false);
            const double self = distance(with, without);
            double nearest = 1.0e9;
            int who = -1;
            for (std::size_t i = 0; i < types.size(); ++i) {
                if (types[i] == w.type) continue;
                const double d = distance(with, others[i]);
                if (d < nearest) nearest = d, who = types[i];
            }
            s.check(self < nearest, std::string(placeNames[std::size_t(w.type)]) + " keeps its identity: nearer to itself with the stand-in than to any other Place",
                    fmt(self) + " vs " + fmt(nearest) + " (" + (who >= 0 ? placeNames[std::size_t(who)] : "?") + ")");
        }
    }

    s.section("woodwinds: KEYS and Instrument mode (in tune across each part's range, the mod wheel as expression)");
    {
        using namespace instTest;
        struct Range { int type, element; std::array<int, 3> keys; };
        const std::vector<Range> ranges { { Village, VillagePlace::Recorder, { 57, 74, 93 } }, { Forest, ForestPlace::Flute, { 50, 69, 91 } },
                                          { Desert, DesertPlace::Ney, { 62, 72, 84 } }, { Mining, MiningPlace::Beacon, { 50, 57, 64 } } };
        int tuned = 0, total = 0, expressive = 0;
        std::string detail, report, ex;
        for (const auto& r : ranges) {
            for (int key : r.keys) {
                auto h = ihost(r.type, r.element);
                note(h, key, 90);
                const auto a = h.render(2.0);
                // The part may sound an octave from the key (its own transposition, as its model).
                const auto m = mono(a, 0.6, 1.2);
                double best = 1.0e9;
                for (int oct : { 0, 12, -12 }) {
                    const double c = harmonicCents(m, 0, m.size(), key + oct);
                    if (std::abs(c) < std::abs(best)) best = c;
                }
                ++total;
                tuned += std::abs(best) <= 15.0 ? 1 : 0;
                report += " " + std::string(placeNames[std::size_t(r.type)]) + "/" + noteName(key) + " " + juce::String(best, 0).toStdString();
                if (std::abs(best) > 15.0) detail += " " + std::string(placeNames[std::size_t(r.type)]) + " " + noteName(key) + " " + fmt(best);
            }
            // The mod wheel up on a held soft note: louder (the layers crossfading), smoothly.
            auto h = ihost(r.type, r.element);
            note(h, r.keys[1], 40);
            h.render(1.0);
            const double soft = rmsDb(mono(h.render(0.6), 0.0, 0.6), 0, std::size_t(0.6 * h.rate));
            std::vector<float> l;
            for (int k = 0; k <= 32; ++k) {
                cc(h, 1, k * 4 > 127 ? 127 : k * 4);
                const auto a = h.render(0.05);
                for (std::size_t i = 0; i < a.l.size(); ++i) l.push_back(a.l[i] + a.r[i]);
            }
            const double loud = rmsDb(mono(h.render(0.6), 0.0, 0.6), 0, std::size_t(0.6 * h.rate));
            const double step = worstStep(l, 480, l.size() - 1);
            const bool ok = loud > soft + 4.0 && step < 30.0;
            expressive += ok ? 1 : 0;
            ex += " " + std::string(placeNames[std::size_t(r.type)]) + " +" + fmt(loud - soft) + " dB (step " + fmt(step) + ")";
        }
        std::cout << "  [report] woodwind parts played live (cents):" << report << "\n";
        s.check(tuned == total, "every woodwind part plays in tune from a keyboard across its range (within 15 cents)", std::to_string(tuned) + " of " + std::to_string(total) + detail);
        s.check(expressive == int(ranges.size()), "the mod wheel is expression on every woodwind part: a held note grows louder (4 dB or more) with no click", ex);
    }

    s.section("woodwinds: a second sample set (the developer's recorder and ocarina) takes over, levelled");
    {
        // The oboe stands in for a recording the developer adds later: the flute's patch with
        // it as its second set plays it, at the flute's level.
        bool plays = true;
        double la = 0, lo = 0;
        for (double key : { 65.0, 74.0, 84.0 }) {
            SamplePart* part = nullptr;
            auto p = neutral("ww-flute", true);
            p.alternate = "ww-oboe";
            p.even = 0.5f;
            const auto alt = playNotes(p, { { { key, 0.0, 4.0, 0.6 } } }, 1.8, {}, &part);
            plays = plays && part != nullptr && part->alternatePlays();
            auto q = neutral("ww-flute", true);
            q.even = 0.5f;
            const auto own = playNotes(q, { { { key, 0.0, 4.0, 0.6 } } }, 1.8);
            la += rmsDb(alt, std::size_t(0.4 * 48000), std::size_t(1.0 * 48000)) / 3.0;
            lo += rmsDb(own, std::size_t(0.4 * 48000), std::size_t(1.0 * 48000)) / 3.0;
        }
        s.check(plays && std::abs(la - lo) < 3.0, "a second set, where the bank has it, plays instead of the first, levelled to it (within 3 dB over three notes)",
                fmt(la) + " vs " + fmt(lo) + " dB");
        s.check(std::string(woodwinds()[0].name) == "Village Recorder" && std::string(woodwinds()[1].name) == "Forest Flute",
                "the Village's recorder and the Forest's flute name their second sets (recorder-alto, ocarina; tools/sample-prep/README.md)");
    }

    s.section("woodwinds: performance");
    {
        // Four busy Places that play the woodwinds, in Standard quality: with the recordings and
        // with the stand-ins.
        const auto cpu = [&](bool woodwinds) {
            samples::Bank::hideForTests(woodwinds ? nullptr : "ww-");
            Host st(48000, 256);
            st.set("quality", 1);
            for (int wc = 0; wc < WorldControlCount; ++wc) st.set(worldId(wc), 0.7f);
            const std::array<int, 4> types { Village, Forest, Desert, Mining };
            for (int slot = 0; slot < placeSlots; ++slot) {
                st.set(slotId(slot, PlType), float(types[std::size_t(slot % 4)]));
                st.set(slotId(slot, PlCharacter), float(slot % 3));
                st.set(slotId(slot, PlActivity), 1.0f);
            }
            st.render(1.0);
            double best = 1.0e9;
            for (int run = 0; run < 3; ++run) {
                const auto begin = juce::Time::getHighResolutionTicks();
                st.render(4.0);
                best = std::min(best, 100.0 * juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin) / 4.0);
            }
            samples::Bank::hideForTests(nullptr);
            return best;
        };
        const double with = cpu(true), without = cpu(false);
        s.metric("CPU, four busy woodwind Places (Village, Forest, Desert, Mining), Standard quality, with the woodwinds", with, " %");
        s.metric("CPU, the same with the stand-ins", without, " %");
        s.check(with < 15.0 && with - without < 2.0, "four busy woodwind Places within the CPU budget (15 % of one core), the woodwinds adding under 2 %",
                fmt(with) + " % (stand-ins " + fmt(without) + " %)");
    }

    s.section("woodwinds: listening renders (test logs, renders/)");
    {
        int written = 0;
        for (const auto& w : woodwinds()) {
            const juce::String base = juce::String("woodwinds-") + placeKeys[std::size_t(w.type)] + "-" +
                                      juce::String(placeInfo(w.type).element[std::size_t(w.element)].name).toLowerCase().replace(" ", "-");
            // The part alone in each Character (10 s each): before (the stand-in), after.
            for (bool after : { false, true }) {
                std::vector<float> l, r;
                for (int c = 0; c < placeCharacters; ++c) append(l, r, partAudio(w.type, c, w.element, after, 12.0), 2.0, 1.0);
                written += writeWav(base + (after ? "-after.wav" : "-before.wav"), l, r, 48000) ? 1 : 0;
            }
            // The whole Place, first Character (16 s): before, after.
            std::vector<float> l, r;
            append(l, r, placeAudio(w.type, 0, false, 18.0), 2.0, 1.5);
            append(l, r, placeAudio(w.type, 0, true, 18.0), 2.0, 0.5);
            written += writeWav(juce::String("woodwinds-") + placeKeys[std::size_t(w.type)] + "-place-before-after.wav", l, r, 48000) ? 1 : 0;
        }
        s.check(written == 3 * int(woodwinds().size()), "listening renders written (each woodwind part before and after in its three Characters; each Place before then after)",
                std::to_string(written) + " files in " + rendersFolder().getFullPathName().toStdString());
    }
}
