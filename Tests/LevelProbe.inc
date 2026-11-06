// The level and drums pass (2026-11-06): a development probe (--only levelprobe), written so it
// builds on the commit before the Drama tier (60fce95) too: the "before" numbers. It prints,
// per line, a tag and BS.1770 loudness (integrated, K-weighted, gated; momentary 400 ms):
// the Battle template (two and three Places) section by section at Drama 0 / 50 / 100 %, and
// the Drama dial moved (dropped to 0, raised, swept) on one Place at Score Intensity 100 %.
#if __has_include("../Source/Places/Drama.h")
#define WB_PROBE_DRAMA 1
#else
#define WB_PROBE_DRAMA 0
#endif

namespace lvp {

struct KFilter {
    double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
    double tick(double x) {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};
inline std::array<KFilter, 2> kFilters(double rate) {
    double k = std::tan(M_PI * 1681.974450955533 / rate), q = 0.7071752369554196, vh = std::pow(10.0, 3.999843853973347 / 20.0), vb = std::pow(vh, 0.4996667741545416);
    double a0 = 1 + k / q + k * k;
    KFilter shelf { (vh + vb * k / q + k * k) / a0, 2 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0 };
    k = std::tan(M_PI * 38.13547087602444 / rate);
    q = 0.5003270373238773;
    a0 = 1 + k / q + k * k;
    KFilter hp { 1, -2, 1, 2 * (k * k - 1) / a0, (1 - k / q + k * k) / a0 };
    return { shelf, hp };
}
// K-weighted power per sample (both channels summed).
inline std::vector<double> kPower(const Audio& a) {
    auto l = kFilters(a.rate), r = kFilters(a.rate);
    std::vector<double> p(a.l.size());
    for (std::size_t i = 0; i < a.l.size(); ++i) {
        const double x = l[1].tick(l[0].tick(a.l[i])), y = r[1].tick(r[0].tick(a.r[i]));
        p[i] = x * x + y * y;
    }
    return p;
}
inline double lufs(double power) { return -0.691 + 10.0 * std::log10(power + 1.0e-30); }
inline double integratedLufs(const Audio& a, double from, double to) {
    const auto p = kPower(a);
    const auto rate = a.rate;
    const auto b = std::size_t(from * rate), e = std::min(p.size(), std::size_t(to * rate));
    const auto blk = std::size_t(0.4 * rate), hop = std::size_t(0.1 * rate);
    std::vector<double> blocks;
    for (std::size_t i = b; i + blk <= e; i += hop) {
        double s = 0;
        for (std::size_t k = i; k < i + blk; ++k) s += p[k];
        blocks.push_back(s / double(blk));
    }
    double sum = 0;
    int n = 0;
    for (double x : blocks)
        if (lufs(x) > -70.0) sum += x, ++n;
    if (n == 0) return -120;
    const double rel = lufs(sum / n) - 10.0;
    sum = 0;
    n = 0;
    for (double x : blocks)
        if (lufs(x) > -70.0 && lufs(x) > rel) sum += x, ++n;
    return n > 0 ? lufs(sum / n) : -120;
}
// Momentary loudness (400 ms windows) every `step` seconds over [from, to).
inline std::vector<double> momentary(const Audio& a, double from, double to, double step = 0.1) {
    const auto p = kPower(a);
    std::vector<double> out;
    const auto blk = std::size_t(0.4 * a.rate);
    for (double t = from; t + 0.4 <= to; t += step) {
        const auto i = std::size_t(t * a.rate);
        if (i + blk > p.size()) break;
        double s = 0;
        for (std::size_t k = i; k < i + blk; ++k) s += p[k];
        out.push_back(lufs(s / double(blk)));
    }
    return out;
}

inline Host battleHost(const std::vector<int>& types, float dramaAmount, arr::Arrangement& a, int variant = 0) {
    Host h;
    for (int slot = 0; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(NoPlace));
    std::array<int, placeSlots> slots {};
    slots.fill(NoPlace);
    for (std::size_t k = 0; k < types.size(); ++k) {
        h.set(slotId(int(k), PlType), float(types[k]));
        slots[k] = types[k];
    }
#if WB_PROBE_DRAMA
    h.set("drama", dramaAmount);
#else
    (void) dramaAmount;
#endif
    a = arr::autoArrange(arr::makeTemplate(arr::Battle), slots, 7);
#if WB_PROBE_DRAMA
    // (1: not orchestrated, every instrument of every Place as before; 2: orchestrated, no
    // lead: the other Places keep their space.)
    if (variant == 1) a = arr::autoArrangeLegacy(arr::makeTemplate(arr::Battle), slots, 7);
    if (variant == 2)
        for (int i = 0; i < a.count; ++i) a.sections[std::size_t(i)].lead = -1;
    arr::dramatize(a, 7);
#else
    (void) variant;
#endif
    h.playHead.bpm = 120;
    h.p->arrangement.set(a);
    arrangeTest::play(h);
    return h;
}

} // namespace lvp

// Every Place at its defaults (Score Intensity 50 %) and at 80 % and 100 %: integrated over
// 4-20 s of 20 s (the level group's reference table, measured on the commit before the Drama tier).
void probePlaceLevels() {
    using namespace lvp;
    for (int type : builtPlaces()) {
        std::cout << "PRELEVEL " << placeKeys[std::size_t(type)];
        for (float at : { 0.5f, 0.8f, 1.0f }) {
            Host h;
            h.set("p1_type", float(type));
            h.set("intensity", at);
#if WB_PROBE_DRAMA
            h.set("drama", 0.0f);
#endif
            std::cout << " " << juce::String(integratedLufs(h.render(20.0), 4.0, 20.0), 2);
        }
        std::cout << std::endl;
    }
}

void probeLevels() {
    using namespace lvp;
    // The Battle template, section by section.
    for (const auto& types : std::vector<std::vector<int>> { { Castle, Village }, { Palace, Desert, Beach } }) {
        for (float d : { 0.0f, 0.5f, 1.0f }) {
#if !WB_PROBE_DRAMA
            if (d > 0.0f) continue;
#endif
            arr::Arrangement a;
            auto h = battleHost(types, d, a);
            const double bar = 2.0;
            const double total = std::min(240.0, double(a.totalBars()) * bar);
            const auto audio = h.render(total);
            std::cout << "BATTLE " << (types.size() == 2 ? "castle+village" : "palace+desert+beach") << " drama " << d << " all " << juce::String(integratedLufs(audio, 2.0, total), 2);
            for (int i = 0; i < a.count; ++i) {
                const double s0 = double(a.startOf(i)) * bar, s1 = std::min(total, double(a.startOf(i + 1 < a.count ? i + 1 : i) + (i + 1 < a.count ? 0 : a.sections[std::size_t(i)].bars)) * bar);
                std::cout << " | " << a.sections[std::size_t(i)].name.data() << " " << juce::String(integratedLufs(audio, s0 + bar, std::max(s0 + 2 * bar, s1 - bar)), 2);
            }
            std::cout << std::endl;
        }
    }
#if WB_PROBE_DRAMA
    for (int variant : { 1, 2 })
        for (float d : { 0.0f, 1.0f }) {
            arr::Arrangement a;
            auto h = battleHost({ Palace, Desert, Beach }, d, a, variant);
            const double bar = 2.0, total = std::min(240.0, double(a.totalBars()) * bar);
            const auto audio = h.render(total);
            std::cout << "BATTLEX " << (variant == 1 ? "legacy" : "nofocus") << " drama " << d << " all " << juce::String(integratedLufs(audio, 2.0, total), 2);
            for (int i = 0; i < a.count; ++i) {
                const double s0 = double(a.startOf(i)) * bar, s1 = double(a.startOf(i) + a.sections[std::size_t(i)].bars) * bar;
                std::cout << " | " << a.sections[std::size_t(i)].name.data() << " " << juce::String(integratedLufs(audio, s0 + bar, std::max(s0 + 2 * bar, std::min(total, s1) - bar)), 2);
            }
            std::cout << std::endl;
        }
    // The Drama dial dropped to 0 (and raised again) on one Place at Score Intensity 100 %.
    for (int type : { int(Castle), int(Palace), int(Megacity) }) {
        Host h;
        h.set("p1_type", float(type));
        h.set("intensity", 1.0f);
        h.set("drama", 1.0f);
        Audio all = h.render(12.0);
        h.set("drama", 0.0f);
        const auto b = h.render(6.0);
        all.l.insert(all.l.end(), b.l.begin(), b.l.end());
        all.r.insert(all.r.end(), b.r.begin(), b.r.end());
        h.set("drama", 1.0f);
        const auto c = h.render(8.0);
        all.l.insert(all.l.end(), c.l.begin(), c.l.end());
        all.r.insert(all.r.end(), c.r.begin(), c.r.end());
        const auto m = momentary(all, 10.0, 26.0, 0.1);
        std::cout << "DROP " << placeKeys[std::size_t(type)] << " (10 s..26 s, 0 at 12 s, back to 100 at 18 s):";
        for (double v : m) std::cout << " " << juce::String(v, 1);
        std::cout << std::endl;
        std::cout << "DROPINT " << placeKeys[std::size_t(type)] << " before " << juce::String(integratedLufs(all, 4.0, 12.0), 2) << " after " << juce::String(integratedLufs(all, 14.0, 18.0), 2)
                  << " again " << juce::String(integratedLufs(all, 21.0, 26.0), 2) << std::endl;
    }
#endif
}
