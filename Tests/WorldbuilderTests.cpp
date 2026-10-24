// Offline tests through the real processBlock. Exit code 0 only if every check passes.
//   WorldbuilderTests                 run everything
//   WorldbuilderTests --snapshot f.png also write an image of the editor
//   WorldbuilderTests --probe          print per-layer measurements (tuning aid)
//   WorldbuilderTests --render f.wav seconds [id=value ...]
//                                      render a world to a WAV file (listening aid)
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <set>
#include <algorithm>
#include <string>
#include <vector>

// Count C++ allocations on the calling thread while enabled.
namespace allocationProbe {
thread_local bool enabled = false;
thread_local std::size_t calls = 0;
void mark() noexcept { if (enabled) ++calls; }
}
void* operator new(std::size_t n) {
    allocationProbe::mark();
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t alignment) {
    allocationProbe::mark();
    void* p = nullptr;
    if (posix_memalign(&p, static_cast<std::size_t>(alignment), n ? n : 1) == 0) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n, std::align_val_t a) { return ::operator new(n, a); }
void operator delete(void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { std::free(p); }

using namespace wb;

namespace {

struct Suite {
    int passed = 0, failed = 0;
    void check(bool okay, const std::string& name, const std::string& detail = {}) {
        okay ? ++passed : ++failed;
        if (!okay) std::cout << "FAIL: " << name << (detail.empty() ? "" : " :: " + detail) << "\n";
    }
    void section(const char* title) { std::cout << "[test] " << title << std::endl; }
    void metric(const std::string& name, double value, const char* unit = "") { std::cout << "  [metric] " << name << " = " << value << unit << "\n"; }
};

double toDb(double x) { return 20.0 * std::log10(std::max(x, 1.0e-12)); }
std::string fmt(double v) { return juce::String(v, 2).toStdString(); }

struct Audio {
    std::vector<float> l, r;
    double rate = 48000;
};

// Measurements over [from, to) seconds.
struct Stats {
    double rmsDb = -240, peak = 0, lowDb = -240, midDb = -240, highDb = -240, sideDb = -240;
    double motionDb = 0;   // standard deviation of the 50 ms level: how much the world moves
    bool finite = true;
};

Stats measure(const Audio& a, double from, double to = 1.0e9) {
    Stats s;
    const auto begin = std::size_t(from * a.rate), end = std::min(a.l.size(), std::size_t(to * a.rate));
    if (end <= begin) return s;
    // Band split with one-pole filters: < 250 Hz, 250 Hz - 3 kHz, > 3 kHz.
    const double aLow = 1.0 - std::exp(-2.0 * M_PI * 250.0 / a.rate), aHigh = 1.0 - std::exp(-2.0 * M_PI * 3000.0 / a.rate);
    double zl = 0, zh = 0, sum = 0, low = 0, mid = 0, high = 0, side = 0;
    for (std::size_t i = 0; i < end; ++i) {
        const double l = a.l[i], r = a.r[i];
        if (!std::isfinite(l) || !std::isfinite(r)) { s.finite = false; continue; }
        const double m = 0.5 * (l + r);
        zl += aLow * (m - zl);
        zh += aHigh * (m - zh);
        if (i < begin) continue;
        s.peak = std::max({ s.peak, std::abs(l), std::abs(r) });
        sum += 0.5 * (l * l + r * r);
        low += zl * zl;
        mid += (zh - zl) * (zh - zl);
        high += (m - zh) * (m - zh);
        side += 0.25 * (l - r) * (l - r);
    }
    const double n = double(end - begin);
    s.rmsDb = 10.0 * std::log10(sum / n + 1.0e-24);
    s.lowDb = 10.0 * std::log10(low / n + 1.0e-24);
    s.midDb = 10.0 * std::log10(mid / n + 1.0e-24);
    s.highDb = 10.0 * std::log10(high / n + 1.0e-24);
    s.sideDb = 10.0 * std::log10(side / n + 1.0e-24);
    const auto w = std::size_t(0.05 * a.rate);
    std::vector<double> env;
    for (std::size_t start = begin; start + w <= end; start += w) {
        double e = 0;
        for (std::size_t i = start; i < start + w; ++i) e += 0.5 * (a.l[i] * a.l[i] + a.r[i] * a.r[i]);
        env.push_back(10.0 * std::log10(e / double(w) + 1.0e-24));
    }
    double mean = 0, var = 0;
    for (double e : env) mean += e / double(env.size());
    for (double e : env) var += (e - mean) * (e - mean) / double(env.size());
    s.motionDb = std::sqrt(var);
    return s;
}

// Largest change in any band (dB) between two measurements.
double difference(const Stats& a, const Stats& b) {
    return std::max({ std::abs(a.rmsDb - b.rmsDb), std::abs(a.lowDb - b.lowDb), std::abs(a.midDb - b.midDb),
                      std::abs(a.highDb - b.highDb), std::abs(a.sideDb - b.sideDb), std::abs(a.motionDb - b.motionDb) });
}

// Short-window RMS envelope, for click and jump checks.
std::vector<double> envelopeDb(const Audio& a, double windowSeconds, double from = 0) {
    std::vector<double> env;
    const auto w = std::size_t(windowSeconds * a.rate);
    for (std::size_t start = std::size_t(from * a.rate); start + w <= a.l.size(); start += w) {
        double sum = 0;
        for (std::size_t i = start; i < start + w; ++i) sum += 0.5 * (a.l[i] * a.l[i] + a.r[i] * a.r[i]);
        env.push_back(10.0 * std::log10(sum / double(w) + 1.0e-24));
    }
    return env;
}

struct TestPlayHead final : juce::AudioPlayHead {
    bool playing = false;
    int64_t position = 0;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo info;
        info.setIsPlaying(playing);
        info.setTimeInSamples(position);
        info.setBpm(120.0);
        return info;
    }
};

struct Host {
    std::unique_ptr<WorldbuilderProcessor> p = std::make_unique<WorldbuilderProcessor>();
    juce::AudioBuffer<float> buffer;
    juce::MidiBuffer midi;
    TestPlayHead playHead;
    double rate;
    int block;

    explicit Host(double sampleRate = 48000, int blockSize = 512, int channels = 2) : rate(sampleRate), block(blockSize) {
        p->setPlayConfigDetails(0, channels, rate, block);
        p->setPlayHead(&playHead);
        p->prepareToPlay(rate, block);
        buffer.setSize(channels, block);
        midi.ensureSize(1024);
    }
    void set(const char* id, float value) {
        auto* parameter = p->parameters.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }
    float get(const char* id) const { return p->parameters.getRawParameterValue(id)->load(); }
    void processOne(int n) {
        buffer.setSize(buffer.getNumChannels(), n, false, false, true);
        buffer.clear();
        p->processBlock(buffer, midi);
        midi.clear();
        if (playHead.playing) playHead.position += n;
    }
    Audio render(double seconds) {
        Audio a;
        a.rate = rate;
        const auto total = std::size_t(seconds * rate);
        a.l.reserve(total);
        a.r.reserve(total);
        while (a.l.size() < total) {
            const int n = int(std::min<std::size_t>(std::size_t(block), total - a.l.size()));
            processOne(n);
            const float* l = buffer.getReadPointer(0);
            const float* r = buffer.getNumChannels() > 1 ? buffer.getReadPointer(1) : l;
            a.l.insert(a.l.end(), l, l + n);
            a.r.insert(a.r.end(), r, r + n);
        }
        return a;
    }
};

const char* worldId(int w) { return specs[std::size_t(PTemperature + w)].id; }

//==============================================================================
void testParameters(Suite& s) {
    s.section("parameter table");
    std::set<std::string> ids;
    for (const auto& spec : specs) {
        s.check(ids.insert(spec.id).second, std::string("parameter id is unique: ") + spec.id);
        s.check(spec.def >= spec.min && spec.def <= spec.max, std::string("default inside range: ") + spec.id);
    }
    WorldbuilderProcessor p;
    s.check(int(p.getParameters().size()) == PCount, "every spec is a host parameter", std::to_string(p.getParameters().size()));
    s.check(p.getTotalNumInputChannels() == 0 && p.getTotalNumOutputChannels() == 2, "instrument: no input, stereo output");
    s.check(p.acceptsMidi(), "accepts MIDI");
}

void testRatesAndBlocks(Suite& s) {
    s.section("default world at several sample rates and block sizes");
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (int block : { 32, 333, 1024, 4096 }) {
            Host h(rate, block);
            const auto a = h.render(4.0);
            const auto st = measure(a, 2.5);
            const auto name = "@" + std::to_string(int(rate)) + "/" + std::to_string(block);
            s.check(st.finite, "finite " + name);
            s.check(st.rmsDb > -40.0 && st.rmsDb < -10.0, "audible and sensible level " + name, fmt(st.rmsDb) + " dBFS");
            s.check(st.peak < 0.9, "peak below the -1 dBFS ceiling " + name, fmt(st.peak));
            s.check(st.sideDb > st.rmsDb - 20.0, "stereo, not mono " + name, "side " + fmt(st.sideDb));
            if (rate == 48000.0 && block == 1024) {
                s.metric("default RMS", st.rmsDb, " dBFS");
                s.metric("default peak", toDb(st.peak), " dBFS");
                s.metric("default low / mid / high", st.lowDb);
                s.metric("  mid", st.midDb);
                s.metric("  high", st.highDb);
            }
        }
    Host mono(48000, 512, 1);
    const auto st = measure(mono.render(4.0), 2.5);
    s.check(st.finite && st.rmsDb > -40.0, "mono output is audible", fmt(st.rmsDb));
}

void testWorldControls(Suite& s) {
    s.section("every World Control at minimum and maximum");
    Host reference;
    const auto base = measure(reference.render(7.0), 4.0);
    // Controls whose layers arrive later. They must be safe now and audible later.
    const std::set<int> later { Machinery, Life, Population };
    for (int w = 0; w < WorldControlCount; ++w) {
        double largest = 0;
        std::array<double, 2> motion {};
        for (float value : { 0.0f, 1.0f }) {
            Host h;
            h.set(worldId(w), value);
            const auto st = measure(h.render(7.0), 4.0);
            const auto name = std::string(specs[std::size_t(PTemperature + w)].name) + (value == 0 ? " min" : " max");
            s.check(st.finite, name + ": finite");
            s.check(st.peak < 0.9, name + ": peak bounded", fmt(st.peak));
            s.check(st.rmsDb > -45.0, name + ": still audible", fmt(st.rmsDb));
            const double diff = difference(st, base);
            motion[value == 0 ? 0 : 1] = st.motionDb;
            if (later.count(w) != 0) {
                std::cout << "  [info] " << name << ": layer arrives in a later milestone (change " << fmt(diff) << " dB)\n";
            } else {
                // Defaults sit near one end for some controls (Danger 20 %), so each extreme
                // must be measurable and at least one must be clearly audible.
                s.check(diff > 0.2, name + ": measurable change from the default world", fmt(diff) + " dB");
                s.metric(name + " change", diff, " dB");
                largest = std::max(largest, diff);
            }
        }
        if (w == Activity) {
            // Activity sets how much and how fast the world moves rather than its tone: the
            // 50 ms level movement must be far larger at chaotic than at calm.
            s.check(motion[1] > 2.0 * motion[0], "Activity: the world moves far more when chaotic than when calm",
                    fmt(motion[0]) + " -> " + fmt(motion[1]) + " dB of movement");
        } else if (later.count(w) == 0) {
            s.check(largest > 1.5, std::string(specs[std::size_t(PTemperature + w)].name) + ": clearly audible at one extreme", fmt(largest) + " dB");
        }
    }
}

// A world that features each layer, so its solo is clearly heard. Thunder needs a
// storm; the others are present in the default world but louder here.
struct Setting { const char* id; float value; };
std::vector<Setting> featuredWorld(int layer) {
    switch (layer) {
        case Water: return { { "wetness", 0.8f }, { "weather", 0.6f } };
        case Fire: return { { "temperature", 0.85f } };
        case Earth: return { { "temperature", 0.05f }, { "danger", 0.6f } };
        case Thunder: return { { "weather", 1.0f } };
        default: return {};
    }
}

void testLayers(Suite& s) {
    s.section("layer solo, mute and level");
    Host all;
    const auto mix = measure(all.render(5.0), 3.0);
    for (int l = 0; l < LayerCount; ++l) {
        const std::string name = layerNames[std::size_t(l)];
        if (!layerImplemented[std::size_t(l)]) {
            Host h;
            h.set(specs[std::size_t(layerSolo(l))].id, 1);
            const auto st = measure(h.render(5.0), 3.0);
            s.check(st.finite && st.rmsDb < -100.0, name + " solo: silent until implemented", fmt(st.rmsDb));
            continue;
        }
        // Thunder is event based: listen long enough for a few strikes.
        const double seconds = l == Thunder ? 30.0 : 8.0;
        Host featured, solo;
        for (const auto& setting : featuredWorld(l)) {
            featured.set(setting.id, setting.value);
            solo.set(setting.id, setting.value);
        }
        solo.set(specs[std::size_t(layerSolo(l))].id, 1);
        const auto soloAudio = solo.render(seconds);
        const auto st = measure(soloAudio, 3.0), whole = measure(featured.render(seconds), 3.0);
        s.check(st.finite, name + " solo: finite");
        s.check(st.rmsDb > -50.0, name + " solo: audible in a world that features it", fmt(st.rmsDb));
        s.check(st.rmsDb < whole.rmsDb + 0.5, name + " solo: part of the mix, not more", fmt(st.rmsDb) + " vs " + fmt(whole.rmsDb));
        s.check(st.sideDb > st.rmsDb - 25.0, name + " solo: stereo", "side " + fmt(st.sideDb) + " vs " + fmt(st.rmsDb));
        double mean = 0;
        for (std::size_t i = std::size_t(3.0 * soloAudio.rate); i < soloAudio.l.size(); ++i) mean += soloAudio.l[i] + soloAudio.r[i];
        mean /= 2.0 * double(soloAudio.l.size() - std::size_t(3.0 * soloAudio.rate));
        s.check(toDb(std::abs(mean)) < st.rmsDb - 30.0, name + " solo: no DC offset", fmt(toDb(std::abs(mean))) + " dB");
        s.metric(name + " solo (featured world)", st.rmsDb, " dBFS");
    }
    Host muted;
    for (int l = 0; l < LayerCount; ++l) muted.set(specs[std::size_t(layerMute(l))].id, 1);
    s.check(measure(muted.render(3.0), 2.0).rmsDb < -100.0, "all layers muted is silent");
    Host quiet;
    for (int l = 0; l < LayerCount; ++l) quiet.set(specs[std::size_t(layerLevel(l))].id, -60);
    s.check(measure(quiet.render(5.0), 3.0).rmsDb < mix.rmsDb - 40.0, "layer levels at -60 dB are near silent");
}

// Renders one layer alone in a world given by `settings`.
Stats soloStats(int layer, std::vector<Setting> settings, double seconds = 10.0, uint32_t seed = 1) {
    Host h;
    h.set(specs[std::size_t(layerSolo(layer))].id, 1);
    h.set("seed", float(seed));
    for (const auto& setting : settings) h.set(setting.id, setting.value);
    return measure(h.render(seconds), 3.0);
}

void testClimateLayers(Suite& s) {
    s.section("climate layers follow the World Controls");
    // Water: Wetness brings it in, Weather turns drizzle into a downpour, frost stills it.
    const auto dryWater = soloStats(Water, { { "wetness", 0.0f } });
    const auto someWater = soloStats(Water, { { "wetness", 0.3f } });
    const auto drenched = soloStats(Water, { { "wetness", 1.0f } });
    s.check(dryWater.rmsDb < -90.0, "Water: an arid world has no water", fmt(dryWater.rmsDb));
    s.check(drenched.rmsDb > someWater.rmsDb + 10.0, "Water: drenched is much louder than damp", fmt(someWater.rmsDb) + " -> " + fmt(drenched.rmsDb));
    const auto drizzle = soloStats(Water, { { "wetness", 0.7f }, { "weather", 0.0f } });
    const auto downpour = soloStats(Water, { { "wetness", 0.7f }, { "weather", 1.0f } });
    s.check(downpour.highDb > drizzle.highDb + 3.0, "Water: Weather turns drizzle into a downpour (more rain highs)",
            fmt(drizzle.highDb) + " -> " + fmt(downpour.highDb));
    const auto snow = soloStats(Water, { { "wetness", 0.8f }, { "temperature", 0.0f } });
    const auto rain = soloStats(Water, { { "wetness", 0.8f }, { "temperature", 0.5f } });
    s.check(snow.rmsDb < rain.rmsDb - 6.0 && snow.rmsDb > -70.0, "Water: frozen worlds get a soft snow hush instead of rain",
            fmt(rain.rmsDb) + " -> " + fmt(snow.rmsDb));
    const auto nearWater = soloStats(Water, { { "wetness", 0.8f }, { "scale", 0.0f } });
    const auto farWater = soloStats(Water, { { "wetness", 0.8f }, { "scale", 1.0f } });
    s.check(farWater.highDb - farWater.rmsDb < nearWater.highDb - nearWater.rmsDb - 1.5, "Water: colossal Scale sounds further away (darker)",
            fmt(nearWater.highDb - nearWater.rmsDb) + " -> " + fmt(farWater.highDb - farWater.rmsDb) + " dB highs re RMS");

    // Fire: Temperature brings it in, scorching adds lava, Wetness douses it.
    const auto frozenFire = soloStats(Fire, { { "temperature", 0.0f } });
    const auto temperateFire = soloStats(Fire, { { "temperature", 0.5f } });
    const auto warmFire = soloStats(Fire, { { "temperature", 0.65f } });
    const auto scorching = soloStats(Fire, { { "temperature", 1.0f } });
    s.check(frozenFire.rmsDb < -100.0, "Fire: nothing burns in a frozen world", fmt(frozenFire.rmsDb));
    s.check(temperateFire.rmsDb > -65.0, "Fire: a temperate world has a faint campfire", fmt(temperateFire.rmsDb));
    s.check(scorching.rmsDb > temperateFire.rmsDb + 12.0, "Fire: scorching is far louder", fmt(temperateFire.rmsDb) + " -> " + fmt(scorching.rmsDb));
    s.check(scorching.lowDb > warmFire.lowDb + 10.0, "Fire: scorching adds lava and roar in the low end", fmt(warmFire.lowDb) + " -> " + fmt(scorching.lowDb));
    const auto dryFire = soloStats(Fire, { { "temperature", 0.85f }, { "wetness", 0.0f } });
    const auto wetFire = soloStats(Fire, { { "temperature", 0.85f }, { "wetness", 1.0f } });
    s.check(wetFire.rmsDb < dryFire.rmsDb - 3.0, "Fire: Wetness douses it", fmt(dryFire.rmsDb) + " -> " + fmt(wetFire.rmsDb));

    // Ice & Earth: frozen worlds creak and crack (bright), hostile ones rumble (low).
    const auto ice = soloStats(Earth, { { "temperature", 0.0f } });
    const auto hot = soloStats(Earth, { { "temperature", 1.0f } });
    s.check(ice.highDb - ice.rmsDb > hot.highDb - hot.rmsDb + 6.0, "Ice & Earth: frozen worlds creak and crack (bright)",
            fmt(hot.highDb - hot.rmsDb) + " -> " + fmt(ice.highDb - ice.rmsDb) + " dB highs re RMS");
    s.check(ice.motionDb > hot.motionDb + 1.0, "Ice & Earth: ice events make the frozen ground move", fmt(hot.motionDb) + " -> " + fmt(ice.motionDb));
    const auto calmGround = soloStats(Earth, { { "danger", 0.0f } });
    const auto hostileGround = soloStats(Earth, { { "danger", 1.0f } });
    s.check(hostileGround.lowDb > calmGround.lowDb + 4.0, "Ice & Earth: Danger adds rumble and groan", fmt(calmGround.lowDb) + " -> " + fmt(hostileGround.lowDb));

    // Thunder: Weather sets how often, Scale how far.
    Host still;
    still.set("thunder_solo", 1);
    still.set("weather", 0.0f);
    const auto stillStats = measure(still.render(40.0), 1.0);
    s.check(stillStats.rmsDb < -100.0 && still.p->engine.thunderLayer().strikeCount() == 0, "Thunder: no strikes in still weather",
            std::to_string(still.p->engine.thunderLayer().strikeCount()) + " strikes");
    Host storm;
    storm.set("thunder_solo", 1);
    storm.set("weather", 1.0f);
    const auto stormAudio = storm.render(150.0);
    const auto& thunder = storm.p->engine.thunderLayer();
    const auto strikes = thunder.strikeCount();
    s.check(strikes >= 10 && strikes <= 60, "Thunder: a storm strikes every few seconds", std::to_string(strikes) + " strikes in 150 s");
    s.metric("Thunder strikes in 150 s of storm", strikes);
    std::vector<double> gaps;
    for (uint32_t i = 1; i < strikes && i < uint32_t(ThunderLayer::logSize); ++i)
        gaps.push_back(double(thunder.strikeClock(i) - thunder.strikeClock(i - 1)) / storm.rate);
    double mean = 0, var = 0;
    for (double g : gaps) mean += g / double(gaps.size());
    for (double g : gaps) var += (g - mean) * (g - mean) / double(gaps.size());
    const double cv = gaps.empty() ? 0.0 : std::sqrt(var) / mean;
    s.check(cv > 0.4, "Thunder: irregular timing, never metronomic", "interval variation " + fmt(cv));
    const auto stormStats = measure(stormAudio, 3.0);
    s.check(stormStats.finite && stormStats.peak < 0.9, "Thunder: finite and below the ceiling", fmt(stormStats.peak));
    s.check(stormStats.motionDb > 6.0, "Thunder: strikes stand out from the quiet between them", fmt(stormStats.motionDb) + " dB");
    const auto nearStorm = soloStats(Thunder, { { "weather", 1.0f }, { "scale", 0.0f } }, 90.0);
    const auto farStorm = soloStats(Thunder, { { "weather", 1.0f }, { "scale", 1.0f } }, 90.0);
    s.check(farStorm.highDb - farStorm.rmsDb < nearStorm.highDb - nearStorm.rmsDb - 3.0, "Thunder: colossal Scale puts the storm further away",
            fmt(nearStorm.highDb - nearStorm.rmsDb) + " -> " + fmt(farStorm.highDb - farStorm.rmsDb) + " dB highs re RMS");
}

void testDeterminism(Suite& s) {
    s.section("seed determinism");
    Host a, b, c;
    c.set("seed", 2);
    const auto x = a.render(3.0), y = b.render(3.0), z = c.render(3.0);
    s.check(x.l == y.l && x.r == y.r, "same seed and settings give identical output");
    double diff = 0;
    for (std::size_t i = 0; i < x.l.size(); ++i) diff += std::abs(x.l[i] - z.l[i]);
    s.check(diff / double(x.l.size()) > 1.0e-4, "a different seed gives different output", fmt(diff / double(x.l.size())));

    // Following the transport: two instances started at the same song position agree,
    // whatever block size the host uses.
    Host p(48000, 256), q(48000, 1000);
    for (auto* h : { &p, &q }) {
        h->playHead.playing = true;
        h->playHead.position = 48000 * 30;
    }
    const auto pa = p.render(2.0), qa = q.render(2.0);
    double maxDiff = 0;
    for (std::size_t i = 0; i < pa.l.size(); ++i) maxDiff = std::max(maxDiff, double(std::abs(pa.l[i] - qa.l[i])));
    s.check(maxDiff < 1.0e-4, "same start position gives the same output at any block size", fmt(toDb(maxDiff)) + " dB max difference");

    // The same in a busy world where every event layer fires (rain, crackle, lava,
    // rockfalls, thunder), at two odd block sizes.
    Host busyA(48000, 64), busyB(48000, 999);
    for (auto* h : { &busyA, &busyB }) {
        for (auto [id, value] : { std::pair<const char*, float> { "weather", 1.0f }, { "wetness", 0.8f }, { "temperature", 0.85f },
                                  { "danger", 0.9f }, { "activity", 1.0f } })
            h->set(id, value);
        h->playHead.playing = true;
        h->playHead.position = 48000 * 75;
    }
    const auto ba = busyA.render(12.0), bb = busyB.render(12.0);
    maxDiff = 0;
    for (std::size_t i = 0; i < ba.l.size(); ++i) maxDiff = std::max(maxDiff, double(std::abs(ba.l[i] - bb.l[i]) + std::abs(ba.r[i] - bb.r[i])));
    s.check(maxDiff < 1.0e-4, "busy world with every event layer: same output at any block size", fmt(toDb(maxDiff)) + " dB max difference");
    s.check(busyA.p->engine.thunderLayer().strikeCount() > 0, "busy world: thunder struck during the comparison",
            std::to_string(busyA.p->engine.thunderLayer().strikeCount()));
}

void testPowerAndMaster(Suite& s) {
    s.section("power fade and master level");
    Host h;
    h.render(3.0);
    h.set("power", 0);
    const auto off = h.render(3.0);
    const auto offStats = measure(off, 2.5);
    s.check(offStats.rmsDb < -120.0, "Power off is silent after the 2 s fade", fmt(offStats.rmsDb));
    const auto during = measure(off, 0.5, 1.5);
    s.check(during.rmsDb > -80.0, "Power off fades rather than cutting", fmt(during.rmsDb));
    // Over the audible part of the fade (its first 40 dB), 20 ms level drops may be at
    // most 1.5 dB worse than the world's own movement when left alone.
    const auto worstDrop = [](const std::vector<double>& env, double floor) {
        double worst = 0;
        for (std::size_t i = 1; i < env.size(); ++i)
            if (env[i] > floor) worst = std::max(worst, env[i - 1] - env[i]);
        return worst;
    };
    const auto env = envelopeDb(off, 0.02);
    Host steadyHost;
    steadyHost.render(3.0);
    const double steady = worstDrop(envelopeDb(steadyHost.render(3.0), 0.02), -200.0);
    const double worst = worstDrop(env, (env.empty() ? 0 : env.front()) - 40.0);
    s.check(worst < steady + 1.5, "fade-out has no level jumps (20 ms windows)", fmt(worst) + " dB vs steady " + fmt(steady) + " dB");
    h.set("power", 1);
    const auto on = h.render(3.0);
    s.check(measure(on, 2.5).rmsDb > -40.0, "Power on brings the world back", fmt(measure(on, 2.5).rmsDb));

    Host m;
    const auto before = measure(m.render(4.0), 3.0);
    m.set("master", -60);
    const auto after = measure(m.render(2.0), 1.0);
    s.check(before.rmsDb - after.rmsDb > 40.0, "Master Level at minimum drops the level", fmt(before.rmsDb) + " -> " + fmt(after.rmsDb));
}

void testTonalChanges(Suite& s) {
    s.section("root and mood changes glide without clicks");
    Host h;
    h.set("bed_solo", 1);
    h.render(3.0);
    Audio all;
    all.rate = h.rate;
    for (int step = 0; step < 12; ++step) {
        h.set("root", float((step * 5) % 12));
        h.set("mood", float(step % 9));
        const auto a = h.render(1.0);
        all.l.insert(all.l.end(), a.l.begin(), a.l.end());
        all.r.insert(all.r.end(), a.r.begin(), a.r.end());
    }
    // Level steps in 40 ms windows, compared with the same drone left alone.
    const auto steadyAudio = h.render(12.0);
    const auto worstStep = [](const std::vector<double>& env) {
        double worst = 0;
        for (std::size_t i = 1; i < env.size(); ++i) worst = std::max(worst, std::abs(env[i] - env[i - 1]));
        return worst;
    };
    const double worst = worstStep(envelopeDb(all, 0.04)), steady = worstStep(envelopeDb(steadyAudio, 0.04));
    // Gliding voices beat against each other as they pass (a few dB of wobble over
    // 40 ms is the glide itself); a jump or dropout would be far larger.
    s.check(worst < 6.0, "no level jumps across 12 changes (40 ms windows)", fmt(worst) + " dB vs steady " + fmt(steady) + " dB");
    // A click shows up as a burst of energy above 6 kHz.
    double hp = 0, prev = 0, worstHf = 0;
    const auto w = std::size_t(0.005 * all.rate);
    double sum = 0;
    std::vector<double> hf;
    for (std::size_t i = 0; i < all.l.size(); ++i) {
        const double x = all.l[i];
        hp = 0.7 * (hp + x - prev);
        prev = x;
        sum += hp * hp;
        if ((i + 1) % w == 0) { hf.push_back(10.0 * std::log10(sum / double(w) + 1.0e-24)); sum = 0; }
    }
    double hfMedian = 0;
    {
        auto sorted = hf;
        std::sort(sorted.begin(), sorted.end());
        hfMedian = sorted[sorted.size() / 2];
    }
    for (double v : hf) worstHf = std::max(worstHf, v - hfMedian);
    s.check(worstHf < 12.0, "no high-frequency clicks", fmt(worstHf) + " dB above the median");
}

void testState(Suite& s) {
    s.section("state round trip");
    Host a;
    a.set("temperature", 0.1f);
    a.set("weather", 0.9f);
    a.set("seed", 4242);
    a.set("root", 9);
    a.set("mood", 4);
    a.set("quality", 2);
    a.set("air_level", -7.5f);
    a.set("bed_mute", 1);
    a.set("power", 0);
    juce::MemoryBlock state;
    a.p->getStateInformation(state);
    Host b;
    b.p->setStateInformation(state.getData(), int(state.getSize()));
    int mismatches = 0;
    std::string first;
    for (const auto& spec : specs)
        if (std::abs(a.get(spec.id) - b.get(spec.id)) > 1.0e-4f) {
            if (mismatches++ == 0) first = spec.id;
        }
    s.check(mismatches == 0, "every parameter restored", first);
    Host c;
    const char junk[] = "not a world";
    c.p->setStateInformation(junk, int(sizeof(junk)));
    s.check(c.get("temperature") == specs[PTemperature].def, "garbage state is ignored");
    s.check(measure(b.render(1.0), 0).finite, "restored instance renders");
}

void testRealtimeSafety(Suite& s) {
    s.section("real-time safety, extremes and CPU");
    // Everything at maximum (scorching: fire, lava, storm, waves) and the same frozen
    // (ice creaks and cracks, snow), so every voice type runs.
    for (int quality = 0; quality < 6; ++quality) {
        Host h(48000, 512);
        h.set("quality", float(quality % 3));
        for (int w = 0; w < WorldControlCount; ++w) h.set(worldId(w), 1.0f);
        if (quality >= 3) h.set("temperature", 0.0f);
        h.render(1.0);
        h.midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 10);
        allocationProbe::calls = 0;
        allocationProbe::enabled = true;
        for (int i = 0; i < 200; ++i) h.processOne(i % 3 == 0 ? 512 : 100 + i);
        allocationProbe::enabled = false;
        s.check(allocationProbe::calls == 0, "processBlock performs no heap allocations (quality " + std::to_string(quality % 3)
                + (quality >= 3 ? ", frozen" : ", scorching") + ")",
                std::to_string(allocationProbe::calls) + " allocations");
    }

    // Seeded random settings, including every layer and global control.
    juce::Random random(12345);
    int bad = 0;
    for (int trial = 0; trial < 24; ++trial) {
        Host h(trial % 2 ? 44100 : 96000, trial % 3 ? 512 : 64);
        for (const auto& spec : specs) {
            if (juce::String(spec.id) == "power" || juce::String(spec.id).endsWith("_mute") || juce::String(spec.id).endsWith("_solo")) continue;
            h.set(spec.id, spec.min + random.nextFloat() * (spec.max - spec.min));
        }
        const auto st = measure(h.render(3.0), 1.0);
        if (!st.finite || st.peak >= 0.9) {
            ++bad;
            std::cout << "  random world " << trial << ": finite=" << st.finite << " peak=" << st.peak << "\n";
        }
    }
    s.check(bad == 0, "24 random worlds: finite and below the ceiling", std::to_string(bad) + " bad");

    // CPU: every World Control at maximum, High quality, 48 kHz / 256.
    Host h(48000, 256);
    h.set("quality", 2);
    for (int w = 0; w < WorldControlCount; ++w) h.set(worldId(w), 1.0f);
    const auto start = juce::Time::getHighResolutionTicks();
    h.render(10.0);
    const double seconds = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - start);
    s.metric("CPU, High quality, everything at maximum (one core of this machine)", 100.0 * seconds / 10.0, " %");
}

void testEditor(Suite& s, const juce::String& snapshotPath) {
    s.section("editor");
    Host h;
    h.render(1.0);
    std::unique_ptr<juce::AudioProcessorEditor> editor(h.p->createEditor());
    s.check(editor != nullptr, "editor is created");
    if (editor == nullptr) return;
    s.check(editor->getWidth() >= 800 && editor->getHeight() >= 500, "editor has its full size",
            std::to_string(editor->getWidth()) + " x " + std::to_string(editor->getHeight()));
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    s.check(image.isValid(), "editor paints");
    if (snapshotPath.isNotEmpty()) {
        juce::File file(snapshotPath);
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat().writeImageToStream(image, out);
        std::cout << "  editor snapshot written to " << snapshotPath << "\n";
    }
    editor.reset();
}

// Tuning aid: per-layer measurements for each World Control at 0, default and 1.
int probe() {
    for (int w = 0; w < WorldControlCount; ++w) {
        std::cout << specs[std::size_t(PTemperature + w)].name << "\n";
        for (int layer = -1; layer < LayerCount; ++layer) {
            if (layer >= 0 && !layerImplemented[std::size_t(layer)]) continue;
            const char* solo = layer < 0 ? "" : specs[std::size_t(layerSolo(layer))].id;
            for (float value : { 0.0f, specs[std::size_t(PTemperature + w)].def, 1.0f }) {
                Host h;
                if (*solo != 0) h.set(solo, 1);
                h.set(worldId(w), value);
                const auto st = measure(h.render(12.0), 4.0);
                std::printf("  %-13s %.2f  rms %6.1f  low %6.1f  mid %6.1f  high %6.1f  side %6.1f  motion %5.2f  peak %6.1f\n",
                            *solo ? solo : "mix", double(value), st.rmsDb, st.lowDb, st.midDb, st.highDb, st.sideDb, st.motionDb,
                            toDb(st.peak));
            }
        }
    }
    return 0;
}

// Listening aid: renders a world with the given parameter values to a 24-bit WAV.
int renderToFile(const juce::String& path, double seconds, const juce::StringArray& settings) {
    Host h(48000, 512);
    for (const auto& setting : settings) {
        const auto id = setting.upToFirstOccurrenceOf("=", false, false), value = setting.fromFirstOccurrenceOf("=", false, false);
        if (h.p->parameters.getParameter(id) == nullptr) {
            std::cout << "unknown parameter " << id << "\n";
            return 1;
        }
        h.set(id.toRawUTF8(), value.getFloatValue());
    }
    const auto a = h.render(seconds);
    juce::AudioBuffer<float> buffer(2, int(a.l.size()));
    buffer.copyFrom(0, 0, a.l.data(), int(a.l.size()));
    buffer.copyFrom(1, 0, a.r.data(), int(a.r.size()));
    juce::File file(path);
    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
    const auto options = juce::AudioFormatWriterOptions().withSampleRate(48000).withNumChannels(2).withBitsPerSample(24);
    const auto writer = juce::WavAudioFormat().createWriterFor(stream, options);
    if (writer == nullptr) return 1;
    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    const auto st = measure(a, 2.0);
    std::cout << path << ": rms " << fmt(st.rmsDb) << " dBFS, peak " << fmt(toDb(st.peak)) << " dBFS\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    juce::String snapshot;
    for (int i = 1; i < argc; ++i)
        if (juce::String(argv[i]) == "--snapshot" && i + 1 < argc) snapshot = argv[++i];
        else if (juce::String(argv[i]) == "--probe") return probe();
        else if (juce::String(argv[i]) == "--render" && i + 2 < argc) {
            juce::StringArray settings;
            for (int k = i + 3; k < argc; ++k) settings.add(argv[k]);
            return renderToFile(argv[i + 1], juce::String(argv[i + 2]).getDoubleValue(), settings);
        }

    Suite s;
    testParameters(s);
    testRatesAndBlocks(s);
    testWorldControls(s);
    testLayers(s);
    testClimateLayers(s);
    testDeterminism(s);
    testPowerAndMaster(s);
    testTonalChanges(s);
    testState(s);
    testRealtimeSafety(s);
    testEditor(s, snapshot);
    std::cout << "\n" << s.passed << " passed, " << s.failed << " failed" << std::endl;
    return s.failed == 0 ? 0 : 1;
}
