// The Drama loudness pass (2026-11-04): drama makes the music more intense, not louder.
// BS.1770 loudness (K-weighted, gated integrated and 3 s short-term), true peak (4x
// oversampled), and the checks: every Place and Drama Style at Calm / Tension / Action /
// Climax in one loudness window, no true peak above -1 dBFS with four busy Places and the
// hits stacking, the limiters rarely engaging, the transitions, Inspire's moods and the
// dramatic factory scenes in the same window; Drama 0 untouched.
// (Included from WorldbuilderTests.cpp after the Drama tier's tests: it uses their helpers.)

namespace loud {

// BS.1770-4 K-weighting at 48 kHz (the tests' rate): the head's shelf, then the RLB high-pass.
struct KFilter {
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0, u1 = 0, u2 = 0, v1 = 0, v2 = 0;
    double tick(double x) {
        const double y = 1.53512485958697 * x - 2.69169618940638 * x1 + 1.19839281085285 * x2 + 1.69065929318241 * y1 - 0.73248077421585 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        const double v = y - 2.0 * u1 + u2 + 1.99004745483398 * v1 - 0.99007225036621 * v2;
        u2 = u1;
        u1 = y;
        v2 = v1;
        v1 = v;
        return v;
    }
};

// The K-weighted mean square of both channels in 100 ms steps from `from` (seconds).
std::vector<double> steps100(const Audio& a, double from, double to = 1.0e9) {
    KFilter fl, fr;
    const auto begin = std::size_t(std::max(0.0, from) * a.rate), end = std::min(a.l.size(), std::size_t(std::max(0.0, to) * a.rate));
    const auto step = std::size_t(0.1 * a.rate);
    std::vector<double> out;
    double sum = 0;
    std::size_t in = 0;
    for (std::size_t i = 0; i < end; ++i) {
        const double l = fl.tick(double(a.l[i])), r = fr.tick(double(a.r[i]));
        if (i < begin) continue;
        sum += l * l + r * r;
        if (++in == step) {
            out.push_back(sum / double(step));
            sum = 0;
            in = 0;
        }
    }
    return out;
}
double lufs(double meanSquare) { return -0.691 + 10.0 * std::log10(std::max(meanSquare, 1.0e-20)); }

// Blocks of `count` 100 ms steps, every step (momentary: 4, short-term: 30).
std::vector<double> blocks(const std::vector<double>& s, int count) {
    std::vector<double> out;
    for (std::size_t i = 0; i + std::size_t(count) <= s.size(); ++i) {
        double m = 0;
        for (int k = 0; k < count; ++k) m += s[i + std::size_t(k)];
        out.push_back(m / double(count));
    }
    return out;
}

// Integrated loudness (LUFS): 400 ms blocks, the absolute (-70) and relative (-10 LU) gates.
double integrated(const Audio& a, double from, double to = 1.0e9) {
    const auto b = blocks(steps100(a, from, to), 4);
    double sum = 0;
    int n = 0;
    for (double m : b)
        if (lufs(m) > -70.0) {
            sum += m;
            ++n;
        }
    if (n == 0) return -120.0;
    const double gate = lufs(sum / double(n)) - 10.0;
    sum = 0;
    n = 0;
    for (double m : b)
        if (lufs(m) > -70.0 && lufs(m) > gate) {
            sum += m;
            ++n;
        }
    return n == 0 ? -120.0 : lufs(sum / double(n));
}

// Short-term loudness (3 s windows, every 100 ms), LUFS.
std::vector<double> shortTerm(const Audio& a, double from, double to = 1.0e9) {
    std::vector<double> out;
    for (double m : blocks(steps100(a, from, to), 30)) out.push_back(lufs(m));
    return out;
}
double maxOf(const std::vector<double>& v) {
    double m = -120.0;
    for (double x : v) m = std::max(m, x);
    return m;
}

// True peak (dBTP): 4x oversampled with a windowed-sinc interpolator (48 taps), as BS.1770 Annex 2.
double truePeak(const Audio& a, double from = 0, double to = 1.0e9) {
    static const auto taps = [] {
        std::array<std::array<double, 12>, 4> t {};
        for (int phase = 0; phase < 4; ++phase)
            for (int k = 0; k < 12; ++k) {
                const double x = double(5 - k) + double(phase) / 4.0;   // from sample i + k - 5 to the point i + phase / 4
                const double sinc = std::abs(x) < 1.0e-9 ? 1.0 : std::sin(M_PI * x) / (M_PI * x);
                const double w = 0.5 + 0.5 * std::cos(M_PI * x / 6.5);       // a Hann window over the taps
                t[std::size_t(phase)][std::size_t(k)] = sinc * w;
            }
        return t;
    }();
    const auto begin = std::size_t(std::max(0.0, from) * a.rate), end = std::min(a.l.size(), std::size_t(std::max(0.0, to) * a.rate));
    double peak = 0;
    for (const auto* ch : { &a.l, &a.r })
        for (std::size_t i = begin + 6; i + 6 < end; ++i) {
            const auto& x = *ch;
            peak = std::max(peak, std::abs(double(x[i])));
            for (int phase = 1; phase < 4; ++phase) {
                double v = 0;
                for (int k = 0; k < 12; ++k) v += taps[std::size_t(phase)][std::size_t(k)] * double(x[i + std::size_t(k) - 5]);
                peak = std::max(peak, std::abs(v));
            }
        }
    return toDb(peak);
}

} // namespace loud

namespace loud {

// The world's master limiter and the slots' (their gain reduction, when the engine keeps it).
struct Reduction {
    double averageDb = 0, maxDb = 0, engagedShare = 0;
};

struct Level {
    double integrated = -120, shortMax = -120, truePeak = -120, peak = 0;
    double events = 0, onsets = 0, low = -240;
    int tier = 0;
    bool finite = true;
    Audio audio;
};

Level level(Host& h, double seconds = 12.0, double from = 4.0) {
    Level v;
    const uint32_t before = allEvents(h) + dramaTest::dramaNotes(h);
    v.audio = h.render(seconds);
    const auto st = measure(v.audio, from);
    v.integrated = integrated(v.audio, from);
    v.shortMax = maxOf(shortTerm(v.audio, from));
    v.truePeak = truePeak(v.audio, from);
    v.peak = st.peak;
    v.finite = st.finite;
    v.low = st.lowDb - st.rmsDb;   // the low end against the level (weight, not loudness)
    v.onsets = dramaTest::onsetRate(v.audio, from);
    v.events = double(allEvents(h) + dramaTest::dramaNotes(h) - before) / seconds;
    v.tier = h.p->engine.placeSlot(0).drama().tier();
    return v;
}

} // namespace loud

// A development aid (--only loudprobe [--place keys]): every Place at Calm (35 % and 50 %)
// and, for each Drama Style, Tension / Action / Climax: integrated and short-term loudness,
// true peak, notes per second.
void probeLoudness() {
    using namespace loud;
    for (int type : builtPlaces()) {
        const std::string name = placeKeys[std::size_t(type)];
        const auto line = [&](const std::string& what, Level& v) {
            std::cout << "LOUD " << name << " " << what << " int " << fmt(v.integrated) << " st " << fmt(v.shortMax) << " tp " << fmt(v.truePeak)
                      << " notes " << fmt(v.events) << " onsets " << fmt(v.onsets) << " low " << fmt(v.low) << " tier " << v.tier << std::endl;
        };
        for (float i : { 0.35f, 0.5f }) {
            auto h = dramaTest::placeHost(type, i, 1.0f);
            auto v = level(h);
            line("calm" + std::to_string(int(i * 100)), v);
        }
        {
            auto h = dramaTest::placeHost(type, 1.0f, 0.0f);
            auto v = level(h);
            line("classic100", v);
        }
        for (int style = drama::StyleDriving; style < drama::StyleCount; ++style)
            for (float i : { 0.62f, 0.8f, 1.0f }) {
                auto h = dramaTest::placeHost(type, i, 1.0f, style);
                auto v = level(h);
                line("s" + std::to_string(style) + "-" + std::to_string(int(i * 100 + 0.5f)), v);
            }
    }
}

namespace loud {

Audio minus(const Audio& a, const Audio& b) {
    Audio d;
    d.rate = a.rate;
    d.l.resize(a.l.size());
    d.r.resize(a.r.size());
    for (std::size_t i = 0; i < a.l.size() && i < b.l.size(); ++i) {
        d.l[i] = a.l[i] - b.l[i];
        d.r[i] = a.r[i] - b.r[i];
    }
    return d;
}
double powerMinus(double a, double b) { return 10.0 * std::log10(std::max(std::pow(10.0, a / 10.0) - std::pow(10.0, b / 10.0), 1.0e-12)); }

// One Place at an intensity and style, rendered with only the Drama parts in `mask` sounding.
Audio renderMasked(int type, float intensity, int style, uint32_t mask, double seconds = 12.0) {
    const uint32_t keep = drama::Layer::partMask;
    drama::Layer::partMask = mask;
    auto h = dramaTest::placeHost(type, intensity, 1.0f, style);
    auto a = h.render(seconds);
    drama::Layer::partMask = keep;
    return a;
}

uint32_t elementMask(int element) {
    uint32_t m = 0;
    for (int k = 0; k < drama::Layer::PartCount; ++k)
        if (drama::Layer::elementOf(k) == element) m |= 1u << k;
    return m;
}

} // namespace loud

// A development aid (--only hitprobe [--place keys]): at Climax, each Drama instrument's and
// each hit part's own loudness and peaks (the layer's sound by subtraction, linear master).
void probeHits() {
    using namespace loud;
    WorldEngine::probeLinear = true;
    for (int type : builtPlaces()) {
        const std::string name = placeKeys[std::size_t(type)];
        for (int style = drama::StyleDriving; style < drama::StyleCount; ++style) {
            const auto place = renderMasked(type, 1.0f, style, 0u);
            const auto all = minus(renderMasked(type, 1.0f, style, 0xffffffffu), place);
            const double li = integrated(all, 4.0);
            std::cout << "HIT " << name << " s" << style << " layer int " << fmt(li) << " mom " << fmt(maxOf([&] { std::vector<double> m; for (double x : blocks(steps100(all, 4.0), 4)) m.push_back(lufs(x)); return m; }()))
                      << " tp " << fmt(truePeak(all, 4.0)) << " place " << fmt(integrated(place, 4.0));
            for (int e = 0; e < 3; ++e) {
                const auto part = minus(renderMasked(type, 1.0f, style, elementMask(e)), place);
                std::vector<double> mom;
                for (double x : blocks(steps100(part, 4.0), 4)) mom.push_back(lufs(x));
                std::cout << " | e" << e << " int " << fmt(integrated(part, 4.0)) << " mom " << fmt(maxOf(mom)) << " tp " << fmt(truePeak(part, 4.0));
            }
            std::cout << std::endl;
            if (style == drama::StyleDriving)
                for (int k = 0; k < drama::Layer::PartCount; ++k) {
                    if (drama::Layer::elementOf(k) != 2 && k != drama::Layer::PMetal) continue;
                    const auto part = minus(renderMasked(type, 1.0f, style, 1u << k), place);
                    const double tp = truePeak(part, 4.0);
                    if (tp < -60.0) continue;
                    std::vector<double> mom;
                    for (double x : blocks(steps100(part, 4.0), 4)) mom.push_back(lufs(x));
                    std::cout << "PART " << name << " part " << k << " int " << fmt(integrated(part, 4.0)) << " mom " << fmt(maxOf(mom)) << " tp " << fmt(tp) << std::endl;
                }
        }
    }
    WorldEngine::probeLinear = false;
}

namespace loud {

// Score Intensity at each tier's probe point (Tension, Action, Climax) and Calm's.
constexpr std::array<float, 3> tierIntensity { 0.62f, 0.8f, 1.0f };
constexpr float calmIntensity = 0.35f, asSetIntensity = 0.5f;
using drama::layerShare;

// The level the tiers are balanced to: Calm (35 %), a little towards the Place as set (50 %).
double balanceTarget(double calm, double asSet) { return calm + std::clamp(0.5 * (asSet - calm), 0.5, 1.0); }

} // namespace loud

// A development aid (--only balancecal [--place keys]): writes the balance table's rows
// ("BAL place style tier placeDb layerDb"). Where a Place's row is still zero it is solved
// from the Place and the Drama layer measured apart (the master linear); otherwise the
// row as it is gets the measured error added (a second pass after the first).
void calibrateBalance() {
    using namespace loud;
    for (int type : builtPlaces()) {
        const std::string name = placeKeys[std::size_t(type)];
        WorldEngine::probeLinear = true;
        drama::balanceOff = true;
        double calm = 0, asSet = 0;
        {
            auto h = dramaTest::placeHost(type, calmIntensity, 1.0f);
            calm = integrated(h.render(12.0), 4.0);
            auto g = dramaTest::placeHost(type, asSetIntensity, 1.0f);
            asSet = integrated(g.render(12.0), 4.0);
        }
        const double target = balanceTarget(calm, asSet);
        {
            std::array<double, 3> rise {};
            for (int k = 0; k < 3; ++k) {
                auto h = dramaTest::placeHost(type, tierIntensity[std::size_t(k)], 0.0f);
                rise[std::size_t(k)] = integrated(h.render(12.0), 4.0) - asSet;
            }
            std::cout << "CLS " << name << " " << juce::String(rise[0], 2) << " " << juce::String(rise[1], 2) << " " << juce::String(rise[2], 2) << std::endl;
        }
        {
            // Each of the Place's six instruments alone (Classic, 50 %): its weight in the Place.
            std::cout << "ELW " << name;
            for (int e = 0; e < placeElements; ++e) {
                double level = -60.0;
                if (patternInfo(type, e).kind != NoPattern) {
                    auto h = dramaTest::placeHost(type, asSetIntensity, 0.0f);
                    for (int o = 0; o < placeElements; ++o) h.set(specs[std::size_t(elementOnParam(0, o))].id, o == e ? 1.0f : 0.0f);
                    if (h.get(specs[std::size_t(elementLevelParam(0, e))].id) < 0.05f) h.set(specs[std::size_t(elementLevelParam(0, e))].id, 0.8f);
                    level = std::max(-60.0, integrated(h.render(12.0), 4.0) - asSet);
                }
                std::cout << " " << juce::String(level, 1);
            }
            std::cout << std::endl;
        }
        std::cout << "CAL " << name << " calm " << fmt(calm) << " asset " << fmt(asSet) << " target " << fmt(target) << std::endl;
        drama::balanceOff = false;
        bool zero = true;
        for (int style = 1; style < drama::StyleCount; ++style)
            for (int t = 1; t <= 3; ++t) {
                const auto b = drama::balance(type, style, t, 0);
                zero = zero && b.placeDb == 0.0f && b.layerDb == 0.0f;
            }
        for (int style = 1; style < drama::StyleCount; ++style)
            for (int t = 1; t <= 3; ++t) {
                const float intensity = tierIntensity[std::size_t(t - 1)];
                double placeDb = 0, layerDb = 0;
                if (zero) {
                    drama::balanceOff = true;
                    const double all = integrated(renderMasked(type, intensity, style, 0xffffffffu), 4.0);
                    const double place = integrated(renderMasked(type, intensity, style, 0u), 4.0);
                    drama::balanceOff = false;
                    const double layer = all - place < 0.05 ? place - 20.0 : powerMinus(all, place);
                    const double share = double(layerShare[std::size_t(t - 1)]);
                    placeDb = std::min(0.0, target + 10.0 * std::log10(1.0 - share) - place);
                    const double placed = place + placeDb;
                    const double rest = placed > target - 0.2 ? target - 12.0 : powerMinus(target, placed);
                    layerDb = std::clamp(rest - layer, -30.0, 6.0);
                    std::cout << "  " << name << " s" << style << " t" << t << " all " << fmt(all) << " place " << fmt(place) << " layer " << fmt(layer) << std::endl;
                } else {
                    const auto b = drama::balance(type, style, t, 0);
                    const double now = integrated(renderMasked(type, intensity, style, 0xffffffffu), 4.0);
                    const double error = target - now;
                    placeDb = std::min(0.0, double(b.placeDb) + error);
                    layerDb = std::clamp(double(b.layerDb) + error, -30.0, 6.0);
                    std::cout << "  " << name << " s" << style << " t" << t << " now " << fmt(now) << " error " << fmt(error) << std::endl;
                }
                std::cout << "BAL " << name << " " << style << " " << t << " " << juce::String(placeDb, 1) << " " << juce::String(layerDb, 1) << std::endl;
            }
        WorldEngine::probeLinear = false;
    }
}

namespace loud {

struct SectionLevel {
    std::string name;
    int tier = -1, transition = 0;
    double from = 0, to = 0, level = -120, landing = -120, estimate = -120, balance = 0;
};
struct SceneLevel {
    std::vector<SectionLevel> sections;
    double integrated = -120, truePeak = -120, calm = -120, seconds = 0;
    Reduction master;
    Audio audio;
};

// A scene played through its arrangement (at most `cap` seconds): each section's
// integrated loudness (its first and last bar left out: the transitions) and the momentary
// (400 ms) peak loudness over its landing (the first two beats).
SceneLevel playScene(const scenes::Scene& scene, double cap = 240.0, float drama = -1.0f) {
    SceneLevel out;
    Host h;
    h.p->loadScene(scene);
    if (drama >= 0.0f) h.set("drama", drama);
    if (std::getenv("WB_NOENV") != nullptr) h.set("environment", -60.0f);
    if (std::getenv("WB_NOLOC") != nullptr) {
        auto b = h.p->arrangement.get();
        for (int k = 0; k < b.count; ++k) b.sections[std::size_t(k)].location = -1;
        h.p->arrangement.set(b);
    }
    const auto a = h.p->arrangement.get();
    arrangeTest::play(h);
    const double bpm = h.playHead.bpm, bar = 240.0 / bpm;
    out.seconds = std::min(cap, double(a.totalBars()) * bar);
    h.p->engine.clearReductions();
    std::vector<std::pair<double, float>> balances;
    for (double t = 0; t < out.seconds;) {
        const double piece = std::min(1.0, out.seconds - t);
        const auto part = h.render(piece);
        out.audio.l.insert(out.audio.l.end(), part.l.begin(), part.l.end());
        out.audio.r.insert(out.audio.r.end(), part.r.begin(), part.r.end());
        t += piece;
        balances.emplace_back(t, h.p->engine.sectionBalanceNow());
    }
    const auto& m = h.p->engine.masterReduction();
    out.master = { m.averageDb(), double(m.maxDb), m.engagedShare() };
    out.integrated = integrated(out.audio, 0.5);
    out.truePeak = truePeak(out.audio, 0.5);
    for (int k = 0; k < a.count; ++k) {
        const auto& sec = a.sections[std::size_t(k)];
        SectionLevel s;
        s.name = sec.name.data();
        s.tier = sec.drama;
        s.transition = sec.transition;
        s.from = double(a.startOf(k)) * bar;
        s.to = double(a.startOf(k) + sec.span()) * bar;
        if (s.from >= out.seconds) break;
        if (s.to - s.from > 2.5 * bar) s.level = integrated(out.audio, s.from + bar, std::min(out.seconds, s.to - bar));
        for (const auto& [at, b] : balances)
            if (at > 0.5 * (s.from + s.to)) {
                s.balance = b;
                break;
            }
        s.estimate = 0.5 * double(sec.intensityStart + sec.intensityEnd) + double(h.get("intensity")) - 0.5;
        std::vector<double> mom;
        for (double x : blocks(steps100(out.audio, std::max(0.0, s.from - 0.2), std::min(out.seconds, s.from + 0.5 * bar + 0.4)), 4)) mom.push_back(lufs(x));
        s.landing = maxOf(mom);
        out.sections.push_back(s);
    }
    double calmSum = 0;
    int calmCount = 0;
    for (const auto& s : out.sections)
        if (s.tier == drama::Calm && s.level > -100) {
            calmSum += std::pow(10.0, s.level / 10.0);
            ++calmCount;
        }
    out.calm = calmCount > 0 ? 10.0 * std::log10(calmSum / calmCount) : -120.0;
    return out;
}

} // namespace loud

// A development aid (--only sceneprobe): the dramatic factory scenes, section by section.
void probeScenes() {
    using namespace loud;
    const char* only = std::getenv("WB_SCENE");
    for (int i = 0; i < scenes::factoryCount(); ++i) {
        const auto& sc = scenes::factory(i);
        if (sc.drama <= 0.0f) continue;
        if (only != nullptr && std::to_string(i) != only) continue;
        WorldEngine::probeNoSectionBalance = std::getenv("WB_NOSECTION") != nullptr;
        const char* d = std::getenv("WB_DRAMA");
        const auto v = playScene(sc, 240.0, d != nullptr ? float(std::atof(d)) : -1.0f);
        std::cout << "SCENE " << i << " " << sc.name << " int " << fmt(v.integrated) << " calm " << fmt(v.calm) << " tp " << fmt(v.truePeak) << " master GR avg "
                  << fmt3(v.master.averageDb) << " max " << fmt(v.master.maxDb) << " s " << fmt(v.seconds) << std::endl;
        for (const auto& s : v.sections)
            std::cout << "   " << s.name << " tier " << s.tier << " trans " << s.transition << " at " << fmt(s.from) << " level " << fmt(s.level) << " (" << fmt(s.level - v.calm)
                      << ") landing " << fmt(s.landing) << " balance " << fmt(s.balance) << std::endl;
    }
}

// A development aid (--only arrcal [--place keys]): each Place alone through the Battle and
// Chase templates (Auto-arranged with their arcs, Drama 100 %, no environment), each section
// against the calm sections; prints "ARR place tension action climax", the arrangement
// correction updated by what was measured (Tension and Action aimed at the calm level,
// a Climax 0.5 dB above it).
void calibrateArranged() {
    using namespace loud;
    WorldEngine::probeNoSectionBalance = std::getenv("WB_NOSECTION") != nullptr;
    for (int type : builtPlaces()) {
        const std::string name = placeKeys[std::size_t(type)];
        std::array<double, 4> sum {};
        std::array<int, 4> count {};
        double calmLevel = 0;
        const bool second = std::getenv("WB_SECOND") != nullptr;
        {
            const int lead = type == Castle ? Village : Castle;
            auto c = second ? intensityWorld({ lead, type }, calmIntensity) : dramaTest::placeHost(type, calmIntensity, 1.0f);
            c.set("drama", 1.0f);
            if (second) c.set(slotId(1, PlLevel), -4.0f);
            c.set("environment", -60.0f);
            calmLevel = integrated(c.render(12.0), 4.0);
        }
        for (int which : { int(arr::Battle), int(arr::Chase) }) {
            arr::Arrangement a;
            const bool second = std::getenv("WB_SECOND") != nullptr;
            const int lead = type == Castle ? Village : Castle;
            auto h = second ? reworkTest::battleWorld({ lead, type }, 1.0f, a, which) : reworkTest::battleWorld({ type }, 1.0f, a, which);
            if (second) h.set(slotId(1, PlLevel), -4.0f);
            h.set("environment", -60.0f);
            h.playHead.bpm = 120;
            h.p->arrangement.set(a);
            arrangeTest::play(h);
            const double bar = 2.0, seconds = std::min(300.0, double(a.totalBars()) * bar);
            Audio audio;
            std::vector<float> balanceAt;
            for (double t = 0; t < seconds; t += 1.0) {
                const auto part = h.render(std::min(1.0, seconds - t));
                audio.l.insert(audio.l.end(), part.l.begin(), part.l.end());
                audio.r.insert(audio.r.end(), part.r.begin(), part.r.end());
                balanceAt.push_back(h.p->engine.sectionBalanceNow());
            }
            std::vector<std::pair<int, double>> levels;
            double calm = 0;
            int calms = 0;
            arr::Shaping sh;
            sh.drama = 1.0f;
            sh.types[0] = type;
            for (int k = 0; k < a.count; ++k) {
                const double from = double(a.startOf(k)) * bar + bar, to = std::min(seconds, double(a.startOf(k) + a.sections[std::size_t(k)].span()) * bar - bar);
                if (to - from < 2.0 * bar || a.sections[std::size_t(k)].transition == arr::TransFilter) continue;
                const double l = integrated(audio, from, to);
                const int tier = arr::sectionTier(a, k, sh);
                if (std::getenv("WB_VERBOSE") != nullptr)
                    std::cout << "  " << a.sections[std::size_t(k)].nameText() << " tier " << tier << " level " << fmt(l) << " balance "
                              << fmt(balanceAt[std::min(balanceAt.size() - 1, std::size_t(0.5 * (from + to)))]) << std::endl;
                levels.push_back({ tier, l });
                if (tier == drama::Calm) {
                    calm += std::pow(10.0, l / 10.0);
                    ++calms;
                }
            }
            if (calms == 0) continue;
            const double ref = std::max(10.0 * std::log10(calm / double(calms)), calmLevel - 4.0);
            for (const auto& [tier, l] : levels)
                if (tier > drama::Calm) {
                    sum[std::size_t(tier)] += l - ref;
                    ++count[std::size_t(tier)];
                }
        }
        std::array<double, 4> residual {};
        for (int t = 1; t <= 3; ++t) residual[std::size_t(t)] = count[std::size_t(t)] > 0 ? sum[std::size_t(t)] / count[std::size_t(t)] : 1.0e9;
        if (residual[2] > 1.0e8) residual[2] = residual[1] < 1.0e8 && residual[3] < 1.0e8 ? 0.5 * (residual[1] + residual[3]) : 0.0;
        for (int t : { 1, 3 })
            if (residual[std::size_t(t)] > 1.0e8) residual[std::size_t(t)] = 0.0;
        std::cout << "ARRM " << name << " tension " << fmt(residual[1]) << " action " << fmt(residual[2]) << " climax " << fmt(residual[3]) << std::endl;
        std::cout << (second ? "ARR2 " : "ARR ") << name;
        for (int t = 1; t <= 3; ++t) {
            const double target = t == 3 ? 0.5 : 0.0;
            // (In a pair the second Place carries about half of what is measured.)
            const double step = second ? 1.6 : 1.0;
            const double corr = std::clamp(double(drama::arrangedCorrectionDb(type, t, second ? 1 : 0)) + step * (residual[std::size_t(t)] - target), -12.0, 6.0);
            std::cout << " " << juce::String(corr, 2);
        }
        std::cout << std::endl;
    }
}
