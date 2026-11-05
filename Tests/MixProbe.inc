// The orchestration pass (2026-11-05): a development probe (--only mixprobe [--place keys]),
// written so it builds on the commit before the pass too (the "before" numbers): per Place,
// the Place's own instruments with Drama 0 and 100 % at the tiers' Score Intensities, the
// whole slot, the Drama percussion against the melodic parts and its reverb tail; and the
// developer's setup (a Palace, a Desert and a Beach, the arrangement as ARRANGE's "+" made it,
// at 180 BPM, Drama as in a Battle template) section by section.
// (Included after LoudnessTests.inc: it uses its helpers.)

namespace mixprobe {

using namespace loud;

inline uint32_t bit(int part) { return 1u << part; }
// The Drama layer's percussion: the drums, the hand drums, the metal, the impacts, the
// swells, the low thump under the ostinato.
inline uint32_t percMask() {
    using L = drama::Layer;
    return bit(L::PDrums) | bit(L::PPerc) | bit(L::PMetal) | bit(L::PHands) | bit(L::PMetalHit) | bit(L::PImpact) | bit(L::PSwell) | bit(L::PLow);
}
inline uint32_t drumsMask() {
    using L = drama::Layer;
    return bit(L::PDrums) | bit(L::PPerc) | bit(L::PHands);
}
inline uint32_t melodicMask() { return ~percMask() & ((1u << drama::Layer::PartCount) - 1u); }

inline Host place(int type, float intensity, float dramaAmount) {
    auto h = dramaTest::placeHost(type, intensity, dramaAmount);
    h.set("environment", -60.0f);
    return h;
}
inline Audio renderWith(int type, float intensity, float dramaAmount, uint32_t mask, double seconds = 12.0, const pat::Element* drums = nullptr) {
    const uint32_t keep = drama::Layer::partMask;
    drama::Layer::partMask = mask;
    auto h = place(type, intensity, dramaAmount);
    if (drums != nullptr) h.p->patterns.setElement(0, type, DramaDrums, *drums);
    auto a = h.render(seconds);
    drama::Layer::partMask = keep;
    return a;
}

// Sparse Drama drums (a low stroke and a snare a bar): their tail can be measured.
inline pat::Element sparseDrums(int type) {
    pat::Element el;
    el.controls = defaultControls(type, DramaDrums);
    el.controls.edited = true;
    el.controls.randomness = 0;
    el.pattern.setGrid(pat::Sixteenth, 1);
    el.pattern.followChords = false;
    el.pattern.swing = 0;
    el.pattern.add(makeNote(0, drama::RowLow, 1, 120));
    el.pattern.add(makeNote(8, drama::RowSnare, 1, 110));
    return el;
}

// The reverb against the direct sound of strokes every `period` seconds: the signal folded
// on the period, the energy from 250 ms to 900 ms after the loudest point against the
// first 120 ms (dB).
inline double tailRatio(const Audio& a, double from, double period = 1.0) {
    const auto n = std::size_t(period * a.rate);
    std::vector<double> e(n, 0.0);
    for (std::size_t i = std::size_t(from * a.rate); i < a.l.size(); ++i) e[i % n] += double(a.l[i]) * a.l[i] + double(a.r[i]) * a.r[i];
    std::size_t top = 0;
    for (std::size_t i = 0; i < n; ++i)
        if (e[i] > e[top]) top = i;
    const auto sum = [&](double t0, double t1) {
        double s = 0;
        for (auto i = std::size_t(t0 * a.rate); i < std::size_t(t1 * a.rate); ++i) s += e[(top + i) % n];
        return s;
    };
    return 10.0 * std::log10(std::max(sum(0.25, 0.9), 1.0e-20) / std::max(sum(0.0, 0.12), 1.0e-20));
}

// The share of energy below 90 Hz (dB against the whole).
inline double subShare(const Audio& a, double from) {
    double lo = 0, all = 0, y1 = 0, y2 = 0;
    const double c = 1.0 - std::exp(-2.0 * M_PI * 90.0 / a.rate);
    for (std::size_t i = 0; i < a.l.size(); ++i) {
        const double x = 0.5 * (double(a.l[i]) + a.r[i]);
        y1 += c * (x - y1);
        y2 += c * (y1 - y2);
        if (i < std::size_t(from * a.rate)) continue;
        lo += y2 * y2;
        all += x * x;
    }
    return 10.0 * std::log10(std::max(lo, 1.0e-20) / std::max(all, 1.0e-20));
}

// The developer's arrangement as ARRANGE's "+" made it before the pass (8-bar sections, every
// instrument on, the climax stretched to 18 bars), with the Battle template's arcs.
inline arr::Arrangement plusArrangement() {
    arr::Arrangement a;
    const std::array<std::pair<const char*, int>, 5> list { { { "Intro", 8 }, { "Build", 8 }, { "Climax", 18 }, { "Wind-down", 8 }, { "Outro", 8 } } };
    for (const auto& [name, bars] : list) {
        a.sections[std::size_t(a.count)] = arr::makeSection(name, bars, a.count);
        ++a.count;
    }
    arr::dramatize(a, 7);
    return a;
}

// What sounds at a song position (after the shaping): per slot and instrument. A Drama
// instrument counts while its section's tier is above Calm and it has notes for its slot's
// rank (the drums on the leading slot, the ostinato on the first two).
inline std::array<std::array<bool, slotElements>, placeSlots> sounding(const arr::Arrangement& a, double beat, const std::array<int, placeSlots>& types, float dramaAmount) {
    std::array<std::array<bool, slotElements>, placeSlots> on {};
    auto st = a.evaluate(beat);
    arr::Shaping sh;
    sh.drama = dramaAmount;
    sh.types = types;
    arr::shape(st, a, sh);
    const int tier = st.drama >= 0 ? st.drama : arr::sectionTier(a, st.at.section, sh);
    int rank = 0;
    for (int slot = 0; slot < placeSlots; ++slot) {
        const int type = types[std::size_t(slot)];
        if (type == NoPlace) continue;
        for (int e = 0; e < slotElements; ++e) {
            if (patternInfo(type, e).kind == NoPattern || st.gain[std::size_t(slot)][std::size_t(e)] <= 0.0f) continue;
            if (e >= placeElements) {
                if (dramaAmount <= 0.0f || tier <= drama::Calm) continue;
                if (e == DramaDrums && rank > 0) continue;
                if (e == DramaOstinato && rank > 1) continue;
            }
            on[std::size_t(slot)][std::size_t(e)] = true;
        }
        ++rank;
    }
    return on;
}
inline bool isKit(int type, int e) { return e == DramaDrums || (e < placeElements && patternInfo(type, e).kind == Kit && arr::roleOf(type, e) == arr::RoleRhythm); }

} // namespace mixprobe

void probeMix() {
    using namespace loud;
    using namespace mixprobe;
    WorldEngine::probeLinear = true;
    const bool quick = std::getenv("WB_QUICK") != nullptr;
    for (int type : builtPlaces()) {
        const std::string name = placeKeys[std::size_t(type)];
        for (float i : { 0.35f, 0.62f, 0.8f, 1.0f }) {
            if (quick && i < 0.9f && i > 0.4f) continue;
            const double d0 = integrated(renderWith(type, i, 0.0f, 0xffffffffu), 4.0);
            const double main = integrated(renderWith(type, i, 1.0f, 0u), 4.0);
            const double all = integrated(renderWith(type, i, 1.0f, 0xffffffffu), 4.0);
            std::cout << "MAIN " << name << " i " << fmt(i) << " d0 " << fmt(d0) << " main " << fmt(main) << " (" << fmt(main - d0) << ") all " << fmt(all) << " ("
                      << fmt(all - d0) << ")" << std::endl;
        }
        {
            const auto p = renderWith(type, 1.0f, 1.0f, 0u);
            const auto perc = minus(renderWith(type, 1.0f, 1.0f, percMask()), p);
            const auto mel = renderWith(type, 1.0f, 1.0f, melodicMask());
            const double lp = integrated(perc, 4.0), lm = integrated(mel, 4.0);
            const auto el = sparseDrums(type);
            const auto ps = renderWith(type, 1.0f, 1.0f, 0u, 12.0, &el);
            const auto drums = minus(renderWith(type, 1.0f, 1.0f, drumsMask(), 12.0, &el), ps);
            std::cout << "PERC " << name << " perc " << fmt(lp) << " melodic " << fmt(lm) << " diff " << fmt(lp - lm) << " tail " << fmt(tailRatio(drums, 4.0))
                      << " sub " << fmt(subShare(perc, 4.0)) << " tp " << fmt(truePeak(perc, 4.0)) << std::endl;
        }
    }
    WorldEngine::probeLinear = false;
    // The developer's setup.
    {
        const std::array<int, placeSlots> types { Palace, Desert, Beach, NoPlace };
        const auto a = plusArrangement();
        for (int k = 0; k < a.count; ++k) {
            const auto& sec = a.sections[std::size_t(k)];
            int most = 0, kits = 0;
            for (double f : { 0.25, 0.5, 0.75 }) {
                const auto on = sounding(a, (double(a.startOf(k)) + f * sec.bars) * 4.0, types, 1.0f);
                int n = 0, kit = 0;
                for (int slot = 0; slot < placeSlots; ++slot)
                    for (int e = 0; e < slotElements; ++e)
                        if (on[std::size_t(slot)][std::size_t(e)]) {
                            ++n;
                            kit += isKit(types[std::size_t(slot)], e) ? 1 : 0;
                        }
                most = std::max(most, n);
                kits = std::max(kits, kit);
            }
            std::cout << "ARRPARTS " << sec.nameText() << " bars " << int(sec.bars) << " parts " << most << " kits " << kits << std::endl;
        }
        std::cout << "ARRLEN bars " << a.totalBars() << " seconds at 180 " << fmt(a.totalBars() * 240.0 / 180.0) << std::endl;
        for (float d : { 0.0f, 1.0f })
            for (uint32_t mask : { 0u, 0xffffffffu }) {
                if (d == 0.0f && mask == 0u) continue;
                const uint32_t keep = drama::Layer::partMask;
                drama::Layer::partMask = mask;
                Host h;
                for (int slot = 0; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(types[std::size_t(slot)]));
                h.set("drama", d);
                h.set("intensity", 0.5f);
                h.playHead.bpm = 180;
                h.p->arrangement.set(a);
                arrangeTest::play(h);
                const double bar = 240.0 / 180.0, seconds = double(a.totalBars()) * bar;
                const auto audio = h.render(seconds);
                drama::Layer::partMask = keep;
                std::cout << "ARRLEVEL drama " << fmt(d) << (mask == 0u ? " place only" : " all") << ":";
                for (int k = 0; k < a.count; ++k) {
                    const double from = double(a.startOf(k)) * bar + bar, to = double(a.startOf(k) + a.sections[std::size_t(k)].span()) * bar - bar;
                    std::cout << " " << a.sections[std::size_t(k)].nameText() << " " << fmt(integrated(audio, from, to));
                }
                std::cout << " tp " << fmt(truePeak(audio, 0.5)) << std::endl;
            }
    }
}
