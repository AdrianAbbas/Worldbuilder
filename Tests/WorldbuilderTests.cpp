// Offline tests through the real processBlock. Exit code 0 only if every check passes.
//   WorldbuilderTests                 run everything
//   WorldbuilderTests --snapshot f.png also write an image of the editor
//   WorldbuilderTests --probe          print per-layer measurements (tuning aid)
//   WorldbuilderTests --probe-places   print each Place's level per character and element
//   WorldbuilderTests --only a,b       run only some groups (patterns, playmode, editor...)
//   WorldbuilderTests --render f.wav seconds [id=value ...]
//                                      render a world to a WAV file (listening aid)
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Patterns/PatternMidi.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <complex>
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

// In-place radix-2 FFT.
void fft(std::vector<std::complex<double>>& a) {
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1) {
        const std::complex<double> w = std::polar(1.0, -2.0 * M_PI / double(len));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<double> v = 1;
            for (std::size_t k = 0; k < len / 2; ++k) {
                const auto x = a[i + k], y = a[i + k + len / 2] * v;
                a[i + k] = x + y;
                a[i + k + len / 2] = x - y;
                v *= w;
            }
        }
    }
}

// Energy per pitch class (C = 0) between lo and hi Hz from `from` seconds on: each
// spectrum bin within 0.3 semitones of a note counts for that note's class.
std::array<double, 12> chroma(const Audio& a, double from, double lo, double hi, std::size_t size = 32768) {
    std::array<double, 12> pc {};
    std::vector<std::complex<double>> buf(size);
    for (std::size_t start = std::size_t(from * a.rate); start + size <= a.l.size(); start += size / 2) {
        for (std::size_t i = 0; i < size; ++i) {
            const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * double(i) / double(size));
            buf[i] = 0.5 * (a.l[start + i] + a.r[start + i]) * w;
        }
        fft(buf);
        for (std::size_t k = 1; k < size / 2; ++k) {
            const double hz = double(k) * a.rate / double(size);
            if (hz < lo || hz > hi) continue;
            const double midi = 69.0 + 12.0 * std::log2(hz / 440.0), nearest = std::round(midi);
            if (std::abs(midi - nearest) < 0.3) pc[std::size_t((int(nearest) % 12 + 12) % 12)] += std::norm(buf[k]);
        }
    }
    return pc;
}

// How well a chroma fits a key: mean and strongest energy of the in-scale classes
// against the out-of-scale ones (dB).
struct KeyFit {
    double meanGap = 0, maxGap = 0;
    std::string text;
};
KeyFit keyFit(const std::array<double, 12>& pc, int root, int mood) {
    double inSum = 0, outSum = 0, inMax = 1.0e-30, outMax = 1.0e-30;
    int ins = 0, outs = 0;
    for (int c = 0; c < 12; ++c) {
        const bool in = music::inScale(mood, c - root);
        (in ? inSum : outSum) += pc[std::size_t(c)];
        (in ? ins : outs) += 1;
        if (in) inMax = std::max(inMax, pc[std::size_t(c)]);
        else outMax = std::max(outMax, pc[std::size_t(c)]);
    }
    KeyFit fit;
    fit.meanGap = 10.0 * std::log10((inSum / ins + 1.0e-30) / (outSum / std::max(outs, 1) + 1.0e-30));
    fit.maxGap = 10.0 * std::log10(inMax / outMax);
    fit.text = "in-scale classes " + fmt(fit.meanGap) + " dB above the rest on average, strongest " + fmt(fit.maxGap) + " dB";
    return fit;
}

// Onset strength in 5 ms windows: how much the (pre-emphasised) energy rises.
std::vector<double> onsetFlux(const Audio& a, double from) {
    const auto w = std::size_t(0.005 * a.rate);
    std::vector<double> energy;
    double prev = 0;
    for (std::size_t start = std::size_t(from * a.rate); start + w <= a.l.size(); start += w) {
        double e = 0;
        for (std::size_t i = start; i < start + w; ++i) {
            const double m = 0.5 * (a.l[i] + a.r[i]);
            const double x = m - 0.9 * prev;
            prev = m;
            e += x * x;
        }
        energy.push_back(10.0 * std::log10(e / double(w) + 1.0e-14));
    }
    std::vector<double> flux(energy.size(), 0.0);
    for (std::size_t i = 4; i < energy.size(); ++i) flux[i] = std::max(0.0, energy[i] - std::max({ energy[i - 1], energy[i - 2], energy[i - 3], energy[i - 4] }));
    return flux;
}

// Number of onsets: windows where the energy rises by more than `rise` dB, at least
// 60 ms apart.
int countOnsets(const std::vector<double>& flux, double rise = 6.0) {
    int count = 0, last = -1000;
    for (int i = 0; i < int(flux.size()); ++i)
        if (flux[std::size_t(i)] > rise && i - last > 12) {
            ++count;
            last = i;
        }
    return count;
}

// How tightly onsets lock to a sixteenth-note grid at `bpm` (vector strength, 0..1).
double gridLock(const std::vector<double>& flux, double bpm) {
    std::complex<double> sum = 0;
    double total = 0;
    for (std::size_t i = 0; i < flux.size(); ++i) {
        const double phase = double(i) * 0.005 * bpm / 60.0 * 4.0;
        sum += flux[i] * std::polar(1.0, 2.0 * M_PI * phase);
        total += flux[i];
    }
    return total > 0 ? std::abs(sum) / total : 0.0;
}

double dcDb(const Audio& a, double from) {
    double mean = 0;
    const auto begin = std::size_t(from * a.rate);
    for (std::size_t i = begin; i < a.l.size(); ++i) mean += a.l[i] + a.r[i];
    mean /= 2.0 * double(a.l.size() - begin);
    return toDb(std::abs(mean));
}

struct Host;
int placeType(const Host& h, int slot);

// The parameter id of a Place slot field ("p1_type"...).
std::string slotId(int slot, int field) { return specs[std::size_t(placeParam(slot, field))].id; }

struct TestPlayHead final : juce::AudioPlayHead {
    bool playing = false;
    int64_t position = 0;
    double bpm = 120.0, rate = 48000;
    juce::Optional<PositionInfo> getPosition() const override {
        PositionInfo info;
        info.setIsPlaying(playing);
        info.setTimeInSamples(position);
        info.setBpm(bpm);
        info.setPpqPosition(double(position) / rate * bpm / 60.0);
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
        playHead.rate = rate;
        p->setPlayHead(&playHead);
        // The tests drive the world without a rolling transport: Play Mode Free (the
        // Transport mode has its own tests).
        set("playmode", float(PlayFree));
        p->prepareToPlay(rate, block);
        buffer.setSize(channels, block);
        midi.ensureSize(1024);
    }
    void set(const char* id, float value) {
        auto* parameter = p->parameters.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }
    void set(const std::string& id, float value) { set(id.c_str(), value); }
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

int placeType(const Host& h, int slot) { return juce::roundToInt(h.get(slotId(slot, PlType).c_str())); }

// Positions (seconds) of every note or hit slot 1's Place has played after `from`.
std::vector<double> placeEvents(const Host& h, double from = 0) {
    std::vector<double> at;
    if (const auto* place = h.p->engine.placeSlot(0).place()) {
        const uint32_t count = place->eventCount();
        for (uint32_t i = count > uint32_t(Place::logSize) ? count - uint32_t(Place::logSize) : 0; i < count; ++i) {
            const double t = double(place->eventClock(i)) / h.rate;
            if (t >= from) at.push_back(t);
        }
    }
    return at;
}

// How tightly events lock to a sixteenth-note grid at `bpm` (vector strength, 0..1).
double eventGridLock(const std::vector<double>& at, double bpm) {
    std::complex<double> sum = 0;
    for (double t : at) sum += std::polar(1.0, 2.0 * M_PI * t * bpm / 60.0 * 4.0);
    return at.empty() ? 0.0 : std::abs(sum) / double(at.size());
}

// Every note slot 1's Place played for one element (all elements: -1).
std::vector<Place::Logged> placeLog(const Host& h, int element = -1) {
    std::vector<Place::Logged> out;
    if (const auto* place = h.p->engine.placeSlot(0).place()) {
        const uint32_t count = place->eventCount();
        for (uint32_t i = count > uint32_t(Place::logSize) ? count - uint32_t(Place::logSize) : 0; i < count; ++i)
            if (element < 0 || place->logged(i).element == element) out.push_back(place->logged(i));
    }
    return out;
}

pat::Note makeNote(int step, int degree, int length = 1, int velocity = 100, bool locked = false, int probability = 100) {
    pat::Note n;
    n.step = int16_t(step);
    n.degree = int8_t(degree);
    n.length = int16_t(length);
    n.velocity = uint8_t(velocity);
    n.probability = uint8_t(probability);
    n.setLocked(locked);
    return n;
}

// An edited element: the given notes on a one-bar 1/16 grid, no swing, in the key.
pat::Element editedElement(std::initializer_list<pat::Note> notes, float randomness = 0, uint8_t vary = pat::VaryAll, int bars = 1) {
    pat::Element e;
    e.controls.edited = true;
    e.controls.randomness = randomness;
    e.controls.vary = vary;
    e.pattern.setGrid(pat::Sixteenth, bars);
    e.pattern.followChords = false;
    e.pattern.swing = 0;
    for (const auto& n : notes) e.pattern.add(n);
    return e;
}

// A Castle in slot 1 whose transport plays from `position`, with element `e` set.
Host patternHost(int e, const pat::Element& element, int64_t position = 144123, int block = 512,
                 const std::vector<std::pair<std::string, float>>& extra = {}) {
    Host h(48000, block);
    h.playHead.bpm = 120;
    h.set("p1_type", float(Castle));
    h.set("p1_solo", 1);
    for (const auto& [id, value] : extra) h.set(id, value);
    h.p->patterns.syncTypes(h.p->slotTypes());
    h.p->patterns.setElement(0, Castle, e, element);
    h.playHead.playing = true;
    h.playHead.position = position;
    return h;
}

bool sameEvents(const std::vector<Place::Logged>& a, const std::vector<Place::Logged>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].clock != b[i].clock || a[i].pitch != b[i].pitch || std::abs(a[i].velocity - b[i].velocity) > 1.0e-6f) return false;
    return true;
}

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
    s.section("every World Control at minimum and maximum (environment; the Places are tested below)");
    // The default Castle is muted here so the environment layers are measured as in
    // Milestone 2. Machinery, Life and Population only shape the Places now.
    Host reference;
    reference.set("p1_mute", 1);
    const auto base = measure(reference.render(7.0), 4.0);
    const std::set<int> later { Machinery, Life, Population };
    for (int w = 0; w < WorldControlCount; ++w) {
        double largest = 0;
        std::array<double, 2> motion {};
        for (float value : { 0.0f, 1.0f }) {
            Host h;
            h.set("p1_mute", 1);
            h.set(worldId(w), value);
            const auto st = measure(h.render(7.0), 4.0);
            const auto name = std::string(specs[std::size_t(PTemperature + w)].name) + (value == 0 ? " min" : " max");
            s.check(st.finite, name + ": finite");
            s.check(st.peak < 0.9, name + ": peak bounded", fmt(st.peak));
            s.check(st.rmsDb > -45.0, name + ": still audible", fmt(st.rmsDb));
            const double diff = difference(st, base);
            motion[value == 0 ? 0 : 1] = st.motionDb;
            if (later.count(w) != 0) {
                std::cout << "  [info] " << name << ": shapes the Places only (environment change " << fmt(diff) << " dB)\n";
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
    for (int slot = 0; slot < placeSlots; ++slot) muted.set(slotId(slot, PlMute), 1);
    s.check(measure(muted.render(3.0), 2.0).rmsDb < -100.0, "all layers and Places muted is silent");
    Host quiet;
    for (int l = 0; l < LayerCount; ++l) quiet.set(specs[std::size_t(layerLevel(l))].id, -60);
    for (int slot = 0; slot < placeSlots; ++slot) quiet.set(slotId(slot, PlLevel), -60);
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
                                  { "danger", 0.9f }, { "activity", 1.0f }, { "p2_type", float(Village) }, { "p3_type", float(Spaceport) },
                                  { "p3_character", 2 }, { "p4_type", float(Village) }, { "p4_sync", 0 }, { "population", 1.0f } })
            h->set(id, value);
        h->playHead.playing = true;
        h->playHead.position = 48000 * 75;
    }
    const auto ba = busyA.render(12.0), bb = busyB.render(12.0);
    maxDiff = 0;
    for (std::size_t i = 0; i < ba.l.size(); ++i) maxDiff = std::max(maxDiff, double(std::abs(ba.l[i] - bb.l[i]) + std::abs(ba.r[i] - bb.r[i])));
    s.check(maxDiff < 1.0e-4, "busy world with every event layer and four Places (one free-running): same output at any block size",
            fmt(toDb(maxDiff)) + " dB max difference");
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
    a.set("p2_type", float(Spaceport));
    a.set("p2_character", 2);
    a.set("p3_type", float(Village));
    a.set("p3_sync", 0);
    a.set("p1_e3_on", 0);
    a.set("p1_tension", 0.9f);
    a.set("p4_seed", 777);
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
        // Four Places, every character in turn.
        for (int slot = 0; slot < placeSlots; ++slot) {
            h.set(slotId(slot, PlType), float(Castle + (slot + quality) % 3));
            h.set(slotId(slot, PlCharacter), float((slot + quality) % 3));
            h.set(slotId(slot, PlActivity), 1.0f);
        }
        h.render(quality >= 3 ? 8.0 : 1.0);
        h.midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 10);
        allocationProbe::calls = 0;
        allocationProbe::enabled = true;
        for (int i = 0; i < 200; ++i) h.processOne(i % 3 == 0 ? 512 : 100 + i);
        allocationProbe::enabled = false;
            s.check(allocationProbe::calls == 0, "processBlock performs no heap allocations (quality " + std::to_string(quality % 3)
                + (quality >= 3 ? ", frozen" : ", scorching") + ", four Places)",
                std::to_string(allocationProbe::calls) + " allocations");
    }

    // Patterns edited while playing (published through the lock-free buffers), with
    // full randomness and Evolve, and Transport mode starting again at every locate.
    {
        Host h(48000, 256);
        h.set("playmode", float(PlayTransport));
        for (int slot = 0; slot < placeSlots; ++slot) h.set(slotId(slot, PlType), float(Castle + slot % 3));
        h.playHead.playing = true;
        h.p->patterns.syncTypes(h.p->slotTypes());
        auto e = editedElement({ makeNote(0, 0, 2), makeNote(3, 2, 1, 80), makeNote(8, 4, 4, 120, true), makeNote(12, 1, 1, 60, false, 50) }, 1.0f, pat::VaryAll);
        e.controls.evolve = 1;
        e.controls.returnBars = 2;
        h.render(1.0);
        allocationProbe::calls = 0;
        for (int i = 0; i < 300; ++i) {
            if (i % 25 == 0) {
                e.pattern.notes[0].degree = int8_t(i % 7);
                h.p->patterns.setElement(0, Castle, CastlePlace::Bells, e);
                h.p->patterns.setElement(1, Village, VillagePlace::HandDrums, e);
                auto el = h.p->patterns.element(2, Spaceport, SpaceportPlace::Pulses);
                el.controls.variant = uint8_t(i);
                h.p->patterns.setElement(2, Spaceport, SpaceportPlace::Pulses, el);
            }
            if (i % 60 == 30) h.playHead.position += 48000 * 3;   // a locate
            if (i == 150) h.playHead.playing = false;
            if (i == 200) h.playHead.playing = true;
            allocationProbe::enabled = true;
            h.processOne(i % 2 ? 256 : 77);
            allocationProbe::enabled = false;
        }
        s.check(allocationProbe::calls == 0, "no heap allocations with patterns edited while playing, Evolve, and Transport locates",
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

    // CPU: every World Control at maximum, four busy Places, High quality, 48 kHz / 256.
    Host h(48000, 256);
    h.set("quality", 2);
    for (int w = 0; w < WorldControlCount; ++w) h.set(worldId(w), 1.0f);
    for (int slot = 0; slot < placeSlots; ++slot) {
        h.set(slotId(slot, PlType), float(Castle + slot % 3));
        h.set(slotId(slot, PlActivity), 1.0f);
        h.set(slotId(slot, PlDensity), 1.0f);
    }
    const auto start = juce::Time::getHighResolutionTicks();
    h.render(10.0);
    const double seconds = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - start);
    s.metric("CPU, High quality, everything at maximum, four Places (one core of this machine)", 100.0 * seconds / 10.0, " %");
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
    const auto save = [&](const juce::Image& image, const juce::String& path) {
        if (path.isEmpty()) return;
        juce::File file(path);
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat().writeImageToStream(image, out);
        std::cout << "  editor snapshot written to " << path << "\n";
    };
    auto* world = dynamic_cast<WorldbuilderEditor*>(editor.get());
    s.check(world != nullptr, "editor is the WORLDBUILDER editor");
    if (world == nullptr) return;
    // Cards add Places to the first empty slots; a fifth is refused.
    const int village = world->addPlace(Village), spaceport = world->addPlace(Spaceport);
    s.check(village == 1 && spaceport == 2 && juce::roundToInt(h.get("p2_type")) == Village && juce::roundToInt(h.get("p3_type")) == Spaceport,
            "Place cards fill the next empty slots", std::to_string(village) + ", " + std::to_string(spaceport));
    s.check(world->addPlace(Factory) == -1, "a Place from a later milestone cannot be added yet");
    const int fourth = world->addPlace(Castle);
    s.check(fourth == 3 && world->addPlace(Village) == -1, "at most four Places");
    h.render(0.5);
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    s.check(image.isValid(), "editor paints");
    save(image, snapshotPath);
    world->openDetail(1);
    const auto detailImage = editor->createComponentSnapshot(editor->getLocalBounds());
    s.check(detailImage.isValid(), "detail panel paints");
    save(detailImage, snapshotPath.isEmpty() ? juce::String() : snapshotPath.upToLastOccurrenceOf(".", false, false) + "-detail.png");
    // The Pattern Editor, on each patterned element of the first three slots.
    world->openPatterns(0);
    auto& patterns = world->patternEditor();
    s.check(patterns.isVisible() && patterns.slot() == 0 && patterns.element() == CastlePlace::Bells, "the Pattern Editor opens on the Place's first pattern");
    h.render(0.5);
    for (int slot = 0; slot < 3; ++slot)
        for (int e = 0; e < placeElements; ++e) {
            if (patternInfo(placeType(h, slot), e).kind == NoPattern) continue;
            world->openPatterns(slot, e);
            h.render(0.1);
            const auto img = editor->createComponentSnapshot(editor->getLocalBounds());
            s.check(img.isValid(), "Pattern Editor paints (slot " + std::to_string(slot + 1) + ", element " + std::to_string(e + 1) + ")");
            if (slot == 0 && e == CastlePlace::Horns)
                save(img, snapshotPath.isEmpty() ? juce::String() : snapshotPath.upToLastOccurrenceOf(".", false, false) + "-patterns.png");
            if (slot == 1 && e == VillagePlace::HandDrums)
                save(img, snapshotPath.isEmpty() ? juce::String() : snapshotPath.upToLastOccurrenceOf(".", false, false) + "-patterns-kit.png");
        }
    world->openPatterns(0, CastlePlace::Pulse);
    s.check(world->patternEditor().shown().pattern.count > 0 && !world->patternEditor().shown().controls.edited,
            "the editor shows the generated pattern");
    // Editing: the first edit keeps the pattern; MIDI out and back in; Generate.
    {
        world->openPatterns(1, VillagePlace::Plucks);
        auto& pe = world->patternEditor();
        const int before = pe.shown().pattern.count;
        pe.addNote(3, 4, 2, 90);
        const auto stored = h.p->patterns.element(1, Village, VillagePlace::Plucks);
        s.check(stored.controls.edited && stored.pattern.count == before + 1, "drawing a note keeps the pattern as edited",
                std::to_string(before) + " -> " + std::to_string(stored.pattern.count));
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("wb-test-plucks.mid");
        const bool saved = pe.saveMidi(file, VillagePlace::Plucks);
        const bool imported = pe.importMidi(file, VillagePlace::Wood, false);
        const auto wood = h.p->patterns.element(1, Village, VillagePlace::Wood);
        s.check(saved && imported && wood.controls.edited && wood.pattern.count == stored.pattern.count,
                "a pattern saved as MIDI imports into another element", std::to_string(wood.pattern.count) + " notes");
        file.deleteFile();
        pe.generate();
        h.render(0.3);
        s.check(!h.p->patterns.element(1, Village, VillagePlace::Wood).controls.edited, "Generate returns the element to a generated pattern");
    }
    editor.reset();
}


//==============================================================================
// Places.

// A host playing one Place alone in slot 1, at `bpm`, with extra settings.
Host placeHost(int type, int character, const std::vector<std::pair<std::string, float>>& extra = {}, double bpm = 120.0) {
    Host h;
    h.playHead.bpm = bpm;
    h.set("p1_type", float(type));
    h.set("p1_character", float(character));
    h.set("p1_solo", 1);
    for (const auto& [id, value] : extra) h.set(id, value);
    return h;
}

//==============================================================================
// Patterns.

void testPatterns(Suite& s) {
    s.section("patterns: exact playback, sample-aligned to the host");
    const int64_t start = 144123;   // not on a beat or a chunk
    const double ppq0 = double(start) / 48000.0 * 2.0, samplesPerBeat = 24000.0;
    const auto pulse = editedElement({ makeNote(0, 0, 1, 120), makeNote(4, 2, 1, 70), makeNote(6, 1, 1, 90), makeNote(10, 0, 1, 50), makeNote(12, 2, 1, 100) });
    {
        auto a = patternHost(CastlePlace::Pulse, pulse);
        a.render(8.0);
        const auto events = placeLog(a, CastlePlace::Pulse);
        // Expected: every pass of the pattern from the start position on.
        std::vector<double> expected;
        for (int64_t pass = 0; pass < 8; ++pass)
            for (int step : { 0, 4, 6, 10, 12 }) {
                const double beat = double(pass * 16 + step) / 4.0;
                const double at = double(start) + (beat - ppq0) * samplesPerBeat;
                if (beat >= ppq0 && at < double(start) + 8.0 * 48000.0 - 32) expected.push_back(at);
            }
        double worst = 0;
        bool matched = events.size() == expected.size();
        for (std::size_t i = 0; matched && i < events.size(); ++i) worst = std::max(worst, std::abs(double(events[i].clock) - expected[i]));
        s.check(matched && worst <= 1.0, "0 % randomness: every note plays, on the exact sample of the host position",
                std::to_string(events.size()) + " of " + std::to_string(expected.size()) + " notes, worst " + fmt(worst) + " samples");
        // Every pass the same notes, velocities and drums.
        bool repeats = !events.empty();
        for (std::size_t i = 5; i < events.size(); ++i)
            repeats &= events[i].pitch == events[i - 5].pitch && events[i].velocity == events[i - 5].velocity;
        s.check(repeats, "0 % randomness: each pass plays the pattern exactly");
        // The same again, and with another block size.
        auto b = patternHost(CastlePlace::Pulse, pulse), c = patternHost(CastlePlace::Pulse, pulse, start, 333);
        const auto x = b.render(8.0);
        c.render(8.0);
        s.check(sameEvents(events, placeLog(b, CastlePlace::Pulse)) && sameEvents(events, placeLog(c, CastlePlace::Pulse)),
                "0 % randomness: identical every time, at any block size");
        (void) x;
    }

    s.section("patterns: randomness changes only what its switches allow");
    // Bells: a melodic pattern in the key (not on the chords), two bars.
    const auto bells = [](float randomness, uint8_t vary, bool locked = false) {
        return editedElement({ makeNote(0, 7, 2, 100, locked), makeNote(4, 9, 2, 80, locked), makeNote(8, 11, 2, 90, locked), makeNote(12, 8, 2, 70, locked),
                               makeNote(16, 13, 2, 110, locked), makeNote(22, 10, 2, 60, locked), makeNote(28, 12, 2, 95, locked) },
                             randomness, vary, 2);
    };
    // Events in the first 28 s of a 30 s render (a late note near the end still lands).
    const auto run = [&](const pat::Element& e) {
        auto h = patternHost(CastlePlace::Bells, e);
        h.render(30.0);
        auto v = placeLog(h, CastlePlace::Bells);
        v.erase(std::remove_if(v.begin(), v.end(), [&](const Place::Logged& x) { return double(x.clock) > double(start) + 28.0 * 48000.0; }), v.end());
        return v;
    };
    const auto exact = run(bells(0, pat::VaryAll));
    const double stepSamples = samplesPerBeat / 4.0;
    // Timing (clock), pitch and velocity of each event against the exact run, matched by order.
    struct Diff {
        int timing = 0, pitch = 0, velocity = 0;
        bool sameCount = false, laterOnly = true, inScale = true;
    };
    const auto compare = [&](const std::vector<Place::Logged>& v) {
        Diff d;
        d.sameCount = v.size() == exact.size();
        for (std::size_t i = 0; i < std::min(v.size(), exact.size()); ++i) {
            if (v[i].clock != exact[i].clock) ++d.timing;
            if (v[i].clock < exact[i].clock || double(v[i].clock - exact[i].clock) > 0.8 * stepSamples) d.laterOnly = false;
            if (v[i].pitch != exact[i].pitch) ++d.pitch;
            if (std::abs(v[i].velocity - exact[i].velocity) > 1.0e-6f) ++d.velocity;
            d.inScale &= music::inScale(0, int(v[i].pitch));
        }
        return d;
    };
    {
        const auto d = compare(run(bells(1, pat::VaryVelocity)));
        s.check(d.sameCount && d.timing == 0 && d.pitch == 0 && d.velocity > 3, "Velocity only: same notes and timing, velocities vary",
                std::to_string(d.velocity) + " velocities changed");
    }
    {
        const auto d = compare(run(bells(1, pat::VaryPitch)));
        s.check(d.sameCount && d.timing == 0 && d.velocity == 0 && d.pitch > 3 && d.inScale, "Pitch only: same timing and velocity, pitches move within the scale",
                std::to_string(d.pitch) + " pitches changed");
    }
    {
        const auto d = compare(run(bells(1, pat::VaryTiming)));
        s.check(d.sameCount && d.pitch == 0 && d.velocity == 0 && d.timing > 3 && d.laterOnly, "Timing only: same notes, some a little late (never early)",
                std::to_string(d.timing) + " moved");
    }
    {
        const auto v = run(bells(1, pat::VarySkip));
        bool subset = v.size() < exact.size() && !v.empty();
        std::size_t k = 0;
        for (const auto& e : v) {
            while (k < exact.size() && exact[k].clock < e.clock) ++k;
            subset &= k < exact.size() && exact[k].clock == e.clock && exact[k].pitch == e.pitch && exact[k].velocity == e.velocity;
        }
        s.check(subset, "Skip only: some notes drop out, the rest unchanged", std::to_string(exact.size()) + " -> " + std::to_string(v.size()));
    }
    {
        const auto v = run(bells(1, pat::VaryExtra));
        std::size_t found = 0;
        for (const auto& e : exact)
            for (const auto& x : v)
                if (x.clock == e.clock && x.pitch == e.pitch && x.velocity == e.velocity) {
                    ++found;
                    break;
                }
        s.check(found == exact.size() && v.size() > exact.size(), "Extra only: every note still plays, and extra ones join",
                std::to_string(exact.size()) + " -> " + std::to_string(v.size()));
    }
    {
        const auto d = compare(run(bells(1, 0)));
        s.check(d.sameCount && d.timing == 0 && d.pitch == 0 && d.velocity == 0, "nothing switched on: exact even at 100 % randomness");
    }

    s.section("patterns: locked notes never change, Evolve, probability");
    {
        auto e = bells(1, pat::VaryAll, true);
        e.controls.evolve = 1;
        s.check(sameEvents(run(e), exact), "every note locked: 100 % randomness and Evolve change nothing");
        // Two of seven locked: they always play exactly.
        auto mixed = bells(1, pat::VaryAll);
        mixed.controls.evolve = 1;
        mixed.pattern.notes[0].setLocked(true);
        mixed.pattern.notes[3].setLocked(true);
        const auto v = run(mixed);
        int lockedSeen = 0, lockedExpected = 0;
        // The locked notes: degrees 7 and 8 (12 and 14 semitones), which no other note plays.
        for (const auto& x : exact) {
            const bool isLocked = x.pitch == 12.0f || x.pitch == 14.0f;
            if (!isLocked) continue;
            ++lockedExpected;
            for (const auto& y : v)
                if (y.clock == x.clock && y.pitch == x.pitch && y.velocity == x.velocity) {
                    ++lockedSeen;
                    break;
                }
        }
        const auto d = compare(v);
        s.check(lockedSeen == lockedExpected && lockedExpected > 4 && (d.pitch > 0 || !d.sameCount), "locked notes play exactly while the others change",
                std::to_string(lockedSeen) + " of " + std::to_string(lockedExpected));
    }
    {
        // Evolve with Randomness at 0: mutations build up; returning every 4 bars, the
        // first bar of each return plays as written.
        auto e = bells(0, pat::VaryPitch | pat::VaryVelocity);
        e.controls.evolve = 1;
        e.controls.returnBars = 4;
        auto h = patternHost(CastlePlace::Bells, e, 0);
        h.render(64.0);
        const auto v = placeLog(h, CastlePlace::Bells);
        auto o = patternHost(CastlePlace::Bells, bells(0, pat::VaryAll), 0);
        o.render(64.0);
        const auto original = placeLog(o, CastlePlace::Bells);
        int changed = 0, returnBarsWrong = 0;
        for (std::size_t i = 0; i < std::min(v.size(), original.size()); ++i) {
            const bool differs = v[i].pitch != original[i].pitch || v[i].velocity != original[i].velocity;
            const int64_t bar = int64_t((double(v[i].clock) + 2.0) / (4.0 * samplesPerBeat));   // notes may land a sample early
            if (bar % 4 == 0 && differs) ++returnBarsWrong;
            if (differs) ++changed;
        }
        s.check(v.size() == original.size() && changed > 5 && returnBarsWrong == 0, "Evolve mutates the pattern and returns to it every 4 bars",
                std::to_string(changed) + " notes mutated, " + std::to_string(returnBarsWrong) + " in a return bar");
        auto never = e;
        never.controls.returnBars = 0;
        auto n = patternHost(CastlePlace::Bells, never, 0);
        n.render(64.0);
        const auto drift = placeLog(n, CastlePlace::Bells);
        int lateChanges = 0, earlyChanges = 0;
        for (std::size_t i = 0; i < std::min(drift.size(), original.size()); ++i) {
            const bool differs = drift[i].pitch != original[i].pitch || drift[i].velocity != original[i].velocity;
            (double(drift[i].clock) < 16.0 * 48000.0 ? earlyChanges : lateChanges) += differs ? 1 : 0;
        }
        s.check(lateChanges > earlyChanges, "Evolve without return drifts further over time",
                std::to_string(earlyChanges) + " changes in the first 16 s, " + std::to_string(lateChanges) + " after");
    }
    {
        auto e = editedElement({ makeNote(0, 7, 2, 100, false, 50), makeNote(8, 9, 2, 100, false, 100) });
        auto h = patternHost(CastlePlace::Bells, e);
        h.render(32.0);
        int half = 0, always = 0;
        for (const auto& x : placeLog(h, CastlePlace::Bells)) (x.pitch == 12.0f ? half : always) += 1;
        s.check(always >= 14 && half > 2 && half < always - 2, "a 50 % note plays on some passes, a 100 % note on every one",
                std::to_string(half) + " vs " + std::to_string(always));
        auto again = patternHost(CastlePlace::Bells, e);
        again.render(32.0);
        s.check(sameEvents(placeLog(h, CastlePlace::Bells), placeLog(again, CastlePlace::Bells)), "probability is repeatable (seeded)");
    }

    s.section("patterns: generators, Generate, edits are kept");
    {
        auto sparse = placeHost(Village, 0, { { "p1_activity", 0.05f } }), busy = placeHost(Village, 0, { { "p1_activity", 1.0f } });
        sparse.render(1.0);
        busy.render(1.0);
        const int few = sparse.p->patterns.generated(0).pattern[VillagePlace::Plucks].count, many = busy.p->patterns.generated(0).pattern[VillagePlace::Plucks].count;
        s.check(sparse.p->patterns.generated(0).type == Village && many > few, "the generated pattern follows Activity (and is reported to the editor)",
                std::to_string(few) + " -> " + std::to_string(many) + " notes");
        // Generate: a new variant gives a fresh pattern.
        auto h = placeHost(Village, 0);
        h.render(1.0);
        const auto before = h.p->patterns.generated(0).pattern[VillagePlace::Plucks];
        auto el = h.p->patterns.element(0, Village, VillagePlace::Plucks);
        el.controls.variant = 1;
        h.p->patterns.setElement(0, Village, VillagePlace::Plucks, el);
        h.render(1.0);
        const auto after = h.p->patterns.generated(0).pattern[VillagePlace::Plucks];
        s.check(!before.sameNotes(after) && after.count > 0, "Generate makes a fresh pattern");
        // An edited pattern is kept when the knobs move.
        auto edited = editedElement({ makeNote(0, 0), makeNote(8, 2) });
        h.p->patterns.setElement(0, Village, VillagePlace::Plucks, edited);
        h.render(1.0);
        h.p->engine.placeSlot(0).place();
        h.set("p1_activity", 1.0f);
        h.set("p1_character", 2.0f);
        h.render(2.0);
        const auto* playing = h.p->engine.placeSlot(0).playingPattern(VillagePlace::Plucks);
        s.check(playing != nullptr && playing->sameNotes(edited.pattern), "an edited pattern is kept when Activity and Character change");
        // Another Place in the slot starts from its own generated patterns.
        h.set("p1_type", float(Spaceport));
        h.render(1.0);
        h.p->patterns.syncTypes(h.p->slotTypes());
        s.check(!h.p->patterns.element(0, Spaceport, SpaceportPlace::Pulses).controls.edited, "a new Place in the slot starts with generated patterns");
    }

    s.section("patterns: MIDI export and import round trip");
    {
        pat::Pattern p;
        p.setGrid(pat::Sixteenth, 2);
        p.followChords = false;
        for (const auto& n : { makeNote(0, 0, 4, 100), makeNote(4, 2, 2, 64), makeNote(6, 4, 1, 30), makeNote(12, -3, 3, 127), makeNote(20, 9, 8, 90) }) p.add(n);
        auto sharp = makeNote(28, 3, 2, 77);
        sharp.alter = 1;
        p.add(sharp);
        pat::MidiTarget target;
        target.root = 2;
        target.mood = 0;
        const auto file = pat::toMidi(p, target);
        juce::MemoryOutputStream out;
        file.writeTo(out);
        juce::MemoryInputStream in(out.getData(), out.getDataSize(), false);
        juce::MidiFile read;
        const bool parsed = read.readFrom(in);
        pat::Pattern back;
        const int count = pat::fromMidi(read, back, target, false, pat::Sixteenth);
        s.check(parsed && count == p.count && back.sameNotes(p), "a melodic pattern survives MIDI export and import (exact pitches)",
                std::to_string(count) + " notes");
        pat::Pattern snapped;
        pat::fromMidi(read, snapped, target, true, pat::Sixteenth);
        bool inKey = snapped.count == p.count;
        for (int i = 0; i < snapped.count; ++i) inKey &= snapped.notes[std::size_t(i)].alter == 0;
        s.check(inKey, "import snaps to the scale when asked");
        // A kit.
        pat::Pattern kit;
        kit.setGrid(pat::Eighth, 1);
        kit.followChords = false;
        for (const auto& n : { makeNote(0, 0), makeNote(2, 1), makeNote(3, 2, 1, 50), makeNote(4, 0), makeNote(6, 1) }) kit.add(n);
        target.kit = true;
        juce::MemoryOutputStream kitOut;
        pat::toMidi(kit, target).writeTo(kitOut);
        juce::MemoryInputStream kitIn(kitOut.getData(), kitOut.getDataSize(), false);
        juce::MidiFile kitRead;
        kitRead.readFrom(kitIn);
        pat::Pattern kitBack;
        pat::fromMidi(kitRead, kitBack, target, true, pat::Eighth);
        s.check(kitBack.sameNotes(kit), "a drum pattern survives MIDI export and import (GM drum notes)");
    }

    s.section("patterns: state round trip");
    {
        Host a;
        a.set("p2_type", float(Village));
        a.p->patterns.syncTypes(a.p->slotTypes());
        auto e = editedElement({ makeNote(0, 0, 2, 100, true, 80), makeNote(5, 3, 1, 40), makeNote(9, -2, 6, 127, false, 25) }, 0.6f,
                               pat::VaryTiming | pat::VaryExtra, 2);
        e.pattern.swing = 0.3f;
        e.pattern.followChords = true;
        e.pattern.base = 3;
        e.controls.evolve = 0.4f;
        e.controls.returnBars = 8;
        a.p->patterns.setElement(1, Village, VillagePlace::Wood, e);
        auto generatedOnly = a.p->patterns.element(0, Castle, CastlePlace::Horns);
        generatedOnly.controls.randomness = 0.9f;
        generatedOnly.controls.variant = 3;
        a.p->patterns.setElement(0, Castle, CastlePlace::Horns, generatedOnly);
        juce::MemoryBlock state;
        a.p->getStateInformation(state);
        Host b;
        b.p->setStateInformation(state.getData(), int(state.getSize()));
        const auto got = b.p->patterns.element(1, Village, VillagePlace::Wood);
        s.check(got.controls == e.controls && got.pattern.sameNotes(e.pattern), "an edited pattern and its controls are restored");
        const auto horns = b.p->patterns.element(0, Castle, CastlePlace::Horns);
        s.check(horns.controls == generatedOnly.controls && !horns.controls.edited, "a generated element keeps its controls and variant");
        const auto xml = juce::String::fromUTF8(static_cast<const char*>(state.getData()), int(state.getSize()));
        s.metric("saved state with one edited pattern", double(state.getSize()), " bytes");
        b.render(2.0);
        const auto* playing = b.p->engine.placeSlot(1).playingPattern(VillagePlace::Wood);
        s.check(playing != nullptr && playing->sameNotes(e.pattern), "the restored pattern plays");
    }
}

void testPlayMode(Suite& s) {
    s.section("Play Mode: Free and Transport");
    {
        WorldbuilderProcessor fresh;
        s.check(juce::roundToInt(fresh.parameters.getRawParameterValue("playmode")->load()) == PlayTransport, "new instances start in Transport mode");
    }
    {
        Host h;
        h.set("playmode", float(PlayTransport));
        const auto st = measure(h.render(3.0), 1.0);
        s.check(st.rmsDb < -100.0, "Transport, host stopped: silent", fmt(st.rmsDb));
        h.playHead.playing = true;
        h.playHead.position = 48000 * 10;
        const auto playing = h.render(4.0);
        const auto p = measure(playing, 0.5);
        s.check(p.rmsDb > -45.0 && p.finite, "Transport, host playing: it plays", fmt(p.rmsDb));
        const auto first = measure(playing, 0.0, 0.003);
        s.check(first.peak < 0.05, "Transport: starts from silence", fmt(toDb(first.peak)) + " dBFS in the first 3 ms");
        h.playHead.playing = false;
        h.render(0.3);
        const auto stopped = measure(h.render(1.0), 0.0);
        s.check(stopped.rmsDb < -100.0, "Transport: stopping fades to silence", fmt(stopped.rmsDb));
    }
    {
        // A host with no transport at all (as the AU test harness, or the standalone app).
        Host h;
        h.set("playmode", float(PlayTransport));
        h.p->setPlayHead(nullptr);
        s.check(measure(h.render(3.0), 1.0).rmsDb > -45.0, "Transport, in a host without a transport: it plays freely");
    }
    {
        // A bounce from bar 9 matches playback from bar 9, whatever happened before.
        Host a, b;
        for (auto* h : { &a, &b }) {
            h->set("playmode", float(PlayTransport));
            h->set("p2_type", float(Spaceport));
            h->set("wetness", 0.7f);
        }
        a.playHead.playing = true;
        a.playHead.position = 48000 * 16;
        const auto x = a.render(6.0);
        b.playHead.playing = true;
        b.render(5.0);   // played from the start
        b.playHead.playing = false;
        b.render(1.0);
        b.playHead.playing = true;
        b.playHead.position = 48000 * 16;   // locate and play again
        const auto y = b.render(6.0);
        s.check(x.l == y.l && x.r == y.r, "Transport: the same song position always gives the same sound (bounces match playback)");
    }
    {
        Host h;   // Free (the tests' default)
        s.check(measure(h.render(4.0), 2.0).rmsDb > -45.0, "Free: plays with the host stopped");
        // A project saved before Play Mode existed loads in Free mode.
        juce::MemoryBlock state;
        h.p->getStateInformation(state);
        auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), int(state.getSize()));
        for (auto* child = xml->getFirstChildElement(); child != nullptr; child = child->getNextElement())
            if (child->getStringAttribute("id") == "playmode") {
                xml->removeChildElement(child, true);
                break;
            }
        juce::MemoryBlock old;
        juce::AudioProcessor::copyXmlToBinary(*xml, old);
        WorldbuilderProcessor loaded;
        loaded.setStateInformation(old.getData(), int(old.getSize()));
        s.check(juce::roundToInt(loaded.parameters.getRawParameterValue("playmode")->load()) == PlayFree, "an older project loads in Free mode, as it played");
    }
}


void testPlaces(Suite& s) {
    for (int type : { Castle, Village, Spaceport }) {
        const std::string place = placeNames[std::size_t(type)];
        s.section((place + ": characters, key, activity, tension, elements, seed").c_str());
        std::array<Stats, placeCharacters> character;
        for (int c = 0; c < placeCharacters; ++c) {
            const std::string name = place + " / " + placeInfo(type).characters[std::size_t(c)];
            auto h = placeHost(type, c);
            const auto a = h.render(32.0);
            const auto st = measure(a, 4.0);
            character[std::size_t(c)] = st;
            s.check(st.finite, name + ": finite");
            s.check(st.rmsDb > -42.0 && st.rmsDb < -18.0, name + ": a bed under a score (-42..-18 dBFS RMS)", fmt(st.rmsDb) + " dBFS");
            s.check(st.peak < 0.6, name + ": peaks stay below -4.4 dBFS", fmt(toDb(st.peak)) + " dBFS");
            s.check(st.sideDb > st.rmsDb - 20.0, name + ": stereo", "side " + fmt(st.sideDb));
            s.check(dcDb(a, 4.0) < st.rmsDb - 30.0, name + ": no DC offset", fmt(dcDb(a, 4.0)) + " dB");
            // Every note is in the key, but brass-like saw drones carry their natural
            // harmonics (the fifth harmonic of G is B), so the out-of-scale classes are not
            // empty: 5 dB on average is a clear margin for notes.
            const auto fit = keyFit(chroma(a, 4.0, 100.0, 5000.0), 2, 0);
            s.check(fit.meanGap > 5.0 && fit.maxGap > 3.0, name + ": in D minor", fit.text);
            s.metric(name + " RMS", st.rmsDb, " dBFS");
            s.metric(name + " key fit (mean gap)", fit.meanGap, " dB");
        }
        for (int a = 0; a < placeCharacters; ++a)
            for (int b = a + 1; b < placeCharacters; ++b) {
                const double diff = difference(character[std::size_t(a)], character[std::size_t(b)]);
                s.check(diff > 1.0, place + ": characters " + std::to_string(a + 1) + " and " + std::to_string(b + 1) + " sound different",
                        fmt(diff) + " dB");
            }

        // Key follows Root and Mood: G major.
        {
            auto h = placeHost(type, 0, { { "root", 7.0f }, { "mood", 1.0f } });
            const auto fit = keyFit(chroma(h.render(32.0), 4.0, 100.0, 5000.0), 7, 1);
            s.check(fit.meanGap > 5.0 && fit.maxGap > 3.0, place + ": follows Root and Mood (G major)", fit.text);
        }

        // Activity: sparse to busy.
        {
            auto sparse = placeHost(type, 0, { { "p1_activity", 0.05f } }), busy = placeHost(type, 0, { { "p1_activity", 1.0f } });
            const auto sa = sparse.render(32.0), ba = busy.render(32.0);
            const auto few = placeEvents(sparse).size(), many = placeEvents(busy).size();
            const double diff = difference(measure(sa, 4.0), measure(ba, 4.0));
            s.check(double(many) > 1.3 * double(few) && diff > 1.0, place + ": Activity makes it busier",
                    std::to_string(few) + " -> " + std::to_string(many) + " notes and hits, " + fmt(diff) + " dB");
        }
        // Tension: calm to menacing.
        {
            auto calm = placeHost(type, 2, { { "p1_tension", 0.0f } }), tense = placeHost(type, 2, { { "p1_tension", 1.0f } });
            const double diff = difference(measure(calm.render(32.0), 4.0), measure(tense.render(32.0), 4.0));
            s.check(diff > 1.5, place + ": Tension changes it clearly", fmt(diff) + " dB");
        }
        // Tone: dark to bright.
        {
            auto dark = placeHost(type, 0, { { "p1_tone", 0.0f } }), bright = placeHost(type, 0, { { "p1_tone", 1.0f } });
            const auto d = measure(dark.render(24.0), 4.0), b = measure(bright.render(24.0), 4.0);
            s.check(b.highDb - b.rmsDb > d.highDb - d.rmsDb + 3.0, place + ": Tone brightens it",
                    fmt(d.highDb - d.rmsDb) + " -> " + fmt(b.highDb - b.rmsDb) + " dB highs re RMS");
        }
        // Elements: each one changes the sound; all off is silent.
        {
            auto full = placeHost(type, 0, { { "p1_activity", 0.8f } });
            const auto all = measure(full.render(32.0), 4.0);
            for (int e = 0; e < placeElements; ++e) {
                auto without = placeHost(type, 0, { { "p1_activity", 0.8f }, { slotId(0, PlElementOn + e), 0.0f } });
                const double diff = difference(all, measure(without.render(32.0), 4.0));
                s.check(diff > 0.3, place + ": switching off " + placeInfo(type).elements[std::size_t(e)] + " changes it", fmt(diff) + " dB");
            }
            std::vector<std::pair<std::string, float>> off;
            for (int e = 0; e < placeElements; ++e) off.push_back({ slotId(0, PlElementOn + e), 0.0f });
            auto none = placeHost(type, 0, off);
            const auto st = measure(none.render(8.0), 4.0);
            s.check(st.rmsDb < -90.0, place + ": every element off is silent", fmt(st.rmsDb) + " dBFS");
        }
        // Seed: same seed, same Place; another seed, other patterns.
        {
            auto a = placeHost(type, 0), b = placeHost(type, 0), c = placeHost(type, 0, { { "p1_seed", 12.0f } });
            const auto x = a.render(10.0), y = b.render(10.0), z = c.render(10.0);
            double diff = 0;
            for (std::size_t i = 0; i < x.l.size(); ++i) diff += std::abs(x.l[i] - z.l[i]);
            s.check(x.l == y.l && x.r == y.r && diff / double(x.l.size()) > 1.0e-4, place + ": its Seed sets its patterns (same seed identical)",
                    fmt(diff / double(x.l.size())));
        }
        // Tempo: synced, onsets lock to the host's grid at two tempos; free, to its own.
        {
            // Every note lands on a sixteenth of the host's tempo (to within one 32-sample
            // control chunk); an off-tempo grid does not fit.
            for (double bpm : { 84.0, 132.0 }) {
                auto h = placeHost(type, 0, { { "p1_swing", 0.0f }, { "p1_activity", 0.8f } }, bpm);
                h.render(40.0);
                const auto at = placeEvents(h, 2.0);
                const double right = eventGridLock(at, bpm), wrong = eventGridLock(at, bpm * 1.17);
                s.check(at.size() > 20 && right > 0.9 && wrong < 0.5, place + ": synced, it plays on the host grid at " + fmt(bpm) + " BPM",
                        std::to_string(at.size()) + " notes, grid lock " + fmt(right) + " vs " + fmt(wrong) + " off-tempo");
            }
            auto h = placeHost(type, 0, { { "p1_swing", 0.0f }, { "p1_activity", 0.8f }, { "p1_sync", 0.0f } }, 132.0);
            h.render(40.0);
            const auto at = placeEvents(h, 2.0);
            // Free tempo: the Place's own, scaled by Activity (Population at its default leaves Activity as set).
            const double own = (type == Castle ? 66.0 : (type == Village ? 92.0 : 112.0)) * (0.85 + 0.3 * 0.8);
            const double ownLock = eventGridLock(at, own), hostLock = eventGridLock(at, 132.0);
            s.check(ownLock > 0.9 && hostLock < 0.5, place + ": free, it keeps its own tempo",
                    "grid lock " + fmt(ownLock) + " at " + fmt(own) + " BPM vs " + fmt(hostLock) + " at the host's 132");
        }
    }
}

// The World Controls fold into every Place.
void testPlaceWorld(Suite& s) {
    s.section("World Controls shape the Places");
    const auto compare = [&](int type, const char* id, float lo, float hi) {
        auto a = placeHost(type, 0, { { id, lo } }), b = placeHost(type, 0, { { id, hi } });
        const auto x = a.render(32.0), y = b.render(32.0);
        return std::pair<Stats, Stats> { measure(x, 4.0), measure(y, 4.0) };
    };
    for (int type : { Castle, Village, Spaceport }) {
        const std::string place = placeNames[std::size_t(type)];
        {
            auto a = placeHost(type, 0, { { "population", 0.0f } }), b = placeHost(type, 0, { { "population", 1.0f } });
            const auto x = a.render(32.0), y = b.render(32.0);
            const auto few = placeEvents(a).size(), many = placeEvents(b).size();
            const double diff = difference(measure(x, 4.0), measure(y, 4.0));
            s.check(double(many) > 1.2 * double(few) && diff > 1.0, place + ": Population makes it busier and deepens its effect",
                    std::to_string(few) + " -> " + std::to_string(many) + " notes and hits, " + fmt(diff) + " dB");
        }
        for (auto [id, threshold] : { std::pair<const char*, double> { "machinery", 1.0 }, { "life", 0.2 }, { "danger", 1.0 } }) {
            const auto [x, y] = compare(type, id, 0.0f, 1.0f);
            const double diff = difference(x, y);
            s.check(diff > threshold, place + ": " + id + " changes it", fmt(diff) + " dB");
            s.metric(place + " " + id + " change", diff, " dB");
        }
        {
            const auto [x, y] = compare(type, "era", 0.0f, 1.0f);
            s.check(y.highDb - y.rmsDb > x.highDb - x.rmsDb + 1.0, place + ": a futuristic Era is brighter",
                    fmt(x.highDb - x.rmsDb) + " -> " + fmt(y.highDb - y.rmsDb));
        }
        {
            const auto [x, y] = compare(type, "scale", 0.0f, 1.0f);
            s.check(y.rmsDb < x.rmsDb - 1.5, place + ": colossal Scale puts it further away (quieter, more of it in the reverb)",
                    fmt(x.rmsDb) + " -> " + fmt(y.rmsDb) + " dBFS");
        }
    }
}

void testPlaceSlots(Suite& s) {
    s.section("Place slots: distance, pan, mute, solo, switching, four at once");
    // Distance: quieter and darker.
    {
        auto nearHost = placeHost(Village, 0, { { "p1_distance", 0.0f } }), farHost = placeHost(Village, 0, { { "p1_distance", 1.0f } });
        const auto n = measure(nearHost.render(20.0), 4.0), f = measure(farHost.render(20.0), 4.0);
        s.check(f.rmsDb < n.rmsDb - 4.0 && f.highDb - f.rmsDb < n.highDb - n.rmsDb - 2.0, "far is quieter and darker",
                fmt(n.rmsDb) + " -> " + fmt(f.rmsDb) + " dBFS, highs " + fmt(n.highDb - n.rmsDb) + " -> " + fmt(f.highDb - f.rmsDb));
    }
    // Pan.
    for (float pan : { -1.0f, 1.0f }) {
        auto h = placeHost(Village, 0, { { "p1_pan", pan } });
        const auto a = h.render(16.0);
        double l = 0, r = 0;
        for (std::size_t i = std::size_t(4.0 * a.rate); i < a.l.size(); ++i) {
            l += a.l[i] * a.l[i];
            r += a.r[i] * a.r[i];
        }
        const double balance = 10.0 * std::log10((r + 1.0e-20) / (l + 1.0e-20));
        s.check(pan < 0 ? balance < -6.0 : balance > 6.0, std::string("pan ") + (pan < 0 ? "left" : "right") + " moves it", fmt(balance) + " dB right re left");
    }
    // A Place solo is the Place alone; a layer solo silences the Places.
    {
        Host solo, alone;
        solo.set("p1_solo", 1);
        for (int l = 0; l < LayerCount; ++l) alone.set(specs[std::size_t(layerMute(l))].id, 1);
        const auto x = solo.render(6.0), y = alone.render(6.0);
        s.check(x.l == y.l && x.r == y.r, "a Place solo plays the Place alone");
        Host air, airOnly;
        air.set("air_solo", 1);
        airOnly.set("air_solo", 1);
        airOnly.set("p1_type", 0);
        const auto p = air.render(6.0), q = airOnly.render(6.0);
        s.check(p.l == q.l && p.r == q.r, "a layer solo silences the Places");
        Host muted;
        muted.set("p1_mute", 1);
        muted.set("p1_solo", 1);
        s.check(measure(muted.render(5.0), 3.0).rmsDb < -100.0, "a muted Place is silent even when soloed");
    }
    // A Place from a later milestone is silent; so is an empty slot.
    {
        auto h = placeHost(Factory, 0);
        const auto st = measure(h.render(6.0), 2.0);
        s.check(st.finite && st.rmsDb < -100.0, "a Place from a later milestone is silent for now", fmt(st.rmsDb));
    }
    // Switching a slot's Place fades out and in: no clicks.
    {
        Host h;
        h.set("p1_solo", 1);
        Audio all;
        all.rate = h.rate;
        std::vector<double> switches;
        const auto append = [&](const Audio& a) {
            all.l.insert(all.l.end(), a.l.begin(), a.l.end());
            all.r.insert(all.r.end(), a.r.begin(), a.r.end());
        };
        append(h.render(6.0));
        for (int type : { int(Village), int(Spaceport), 0, int(Castle), int(Spaceport) }) {
            switches.push_back(double(all.l.size()) / all.rate);
            h.set("p1_type", float(type));
            append(h.render(5.0));
        }
        // A click is a burst of energy above 6 kHz: compare the worst 5 ms burst around
        // each switch with the worst during steady playing.
        double hp = 0, prev = 0, sum = 0;
        const auto w = std::size_t(0.005 * all.rate);
        std::vector<double> hf;
        for (std::size_t i = 0; i < all.l.size(); ++i) {
            const double x = 0.5 * (all.l[i] + all.r[i]);
            hp = 0.7 * (hp + x - prev);
            prev = x;
            sum += hp * hp;
            if ((i + 1) % w == 0) {
                hf.push_back(10.0 * std::log10(sum / double(w) + 1.0e-24));
                sum = 0;
            }
        }
        double steady = -240, around = -240;
        for (std::size_t k = 0; k < hf.size(); ++k) {
            const double t = double(k) * 0.005;
            if (t < 2.0) continue;
            bool near = false;
            for (double at : switches) near |= t > at - 0.05 && t < at + 0.7;
            (near ? around : steady) = std::max(near ? around : steady, hf[k]);
        }
        s.check(around < steady + 3.0, "switching Places has no clicks", "worst 5 ms burst " + fmt(around) + " dB vs " + fmt(steady) + " dB while playing");
        const auto last = measure(all, all.l.size() / all.rate - 3.0);
        s.check(last.rmsDb > -45.0, "the new Place plays after a switch", fmt(last.rmsDb));
    }
    // Four Places at once, in a busy world: finite, bounded, still a bed.
    {
        Host h;
        for (int slot = 0; slot < placeSlots; ++slot) {
            h.set(slotId(slot, PlType), float(Castle + slot % 3));
            h.set(slotId(slot, PlCharacter), float(slot % 3));
        }
        h.set("weather", 0.7f);
        h.set("wetness", 0.6f);
        const auto st = measure(h.render(24.0), 4.0);
        s.check(st.finite && st.peak < 0.9, "four Places in a stormy world: finite and below the ceiling", fmt(toDb(st.peak)) + " dBFS");
        s.check(st.rmsDb > -35.0 && st.rmsDb < -12.0, "four Places in a stormy world: a sensible level", fmt(st.rmsDb) + " dBFS");
        s.metric("four Places + storm RMS", st.rmsDb, " dBFS");
    }
}

// The environment retuned toward music: drips, rain, wind and thunder in the key.
void testMusic(Suite& s) {
    s.section("environment in the key: drips, rain, wind, thunder");
    for (auto [root, mood, name] : { std::tuple<int, int, const char*> { 2, 0, "D minor" }, { 7, 1, "G major" }, { 9, 3, "A Phrygian" } }) {
        Host h;
        h.set("water_solo", 1);
        h.set("wetness", 0.35f);
        h.set("weather", 0.0f);
        h.set("root", float(root));
        h.set("mood", float(mood));
        const auto a = h.render(40.0);
        const auto fit = keyFit(chroma(a, 4.0, 300.0, 5000.0), root, mood);
        s.check(fit.meanGap > 8.0 && fit.maxGap > 4.0, std::string("drips ring in ") + name, fit.text);
        if (root == 2) {
            // Natural timing: onset intervals vary widely.
            const auto flux = onsetFlux(a, 4.0);
            std::vector<double> at;
            int last = -1000;
            for (int i = 0; i < int(flux.size()); ++i)
                if (flux[std::size_t(i)] > 6.0 && i - last > 12) {
                    at.push_back(double(i) * 0.005);
                    last = i;
                }
            std::vector<double> gaps;
            for (std::size_t i = 1; i < at.size(); ++i) gaps.push_back(at[i] - at[i - 1]);
            double mean = 0, var = 0;
            for (double g : gaps) mean += g / double(std::max<std::size_t>(gaps.size(), 1));
            for (double g : gaps) var += (g - mean) * (g - mean) / double(std::max<std::size_t>(gaps.size(), 1));
            const double cv = mean > 0 ? std::sqrt(var) / mean : 0;
            s.check(gaps.size() >= 8 && cv > 0.5, "drips fall with natural, irregular timing",
                    std::to_string(at.size()) + " drips, interval variation " + fmt(cv));
            const auto st = measure(a, 4.0);
            s.check(st.sideDb > st.rmsDb - 12.0, "drips are spread across the stereo field", "side " + fmt(st.sideDb) + " vs " + fmt(st.rmsDb));
        }
    }
    {
        Host h;
        h.set("water_solo", 1);
        h.set("wetness", 0.8f);
        h.set("weather", 0.5f);
        const auto fit = keyFit(chroma(h.render(24.0), 4.0, 500.0, 5000.0), 2, 0);
        s.check(fit.meanGap > 3.0, "rain plucks and shimmer lean to the key", fit.text);
    }
    {
        Host h;
        h.set("air_solo", 1);
        h.set("weather", 0.7f);
        h.set("temperature", 0.2f);
        const auto fit = keyFit(chroma(h.render(24.0), 4.0, 400.0, 5000.0), 2, 0);
        s.check(fit.meanGap > 1.5, "wind whistles sing on scale notes", fit.text);
    }
    {
        Host h;
        h.set("thunder_solo", 1);
        h.set("weather", 1.0f);
        const auto pc = chroma(h.render(90.0), 4.0, 30.0, 160.0, 65536);
        const auto strongest = std::size_t(std::max_element(pc.begin(), pc.end()) - pc.begin());
        s.check(strongest == 2 || strongest == 9, "thunder lands on the root (or fifth) of the key", "strongest low pitch class " + std::to_string(strongest));
    }
}

// Tuning aid: per-layer measurements for each World Control at 0, default and 1.
int probe() {
    for (int w = 0; w < WorldControlCount; ++w) {
        std::cout << specs[std::size_t(PTemperature + w)].name << "\n";
        for (int layer = -1; layer < LayerCount; ++layer) {
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


// Tuning aid: each Place's level per character, and each element alone.
int probePlaces() {
    for (int type : { Castle, Village, Spaceport })
        for (int c = 0; c < placeCharacters; ++c) {
            std::printf("%-10s %-15s", placeNames[std::size_t(type)], placeInfo(type).characters[std::size_t(c)]);
            for (int e = -1; e < placeElements; ++e) {
                std::vector<std::pair<std::string, float>> settings;
                for (int k = 0; k < placeElements; ++k)
                    if (e >= 0 && k != e) settings.push_back({ slotId(0, PlElementOn + k), 0.0f });
                auto h = placeHost(type, c, settings);
                const auto a = h.render(32.0);
                const auto st = measure(a, 4.0);
                const auto fit = keyFit(chroma(a, 4.0, 100.0, 5000.0), 2, 0);
                std::printf("  %s %6.1f/%5.1f n%zu k%.1f", e < 0 ? "all" : placeInfo(type).elements[std::size_t(e)], st.rmsDb, toDb(st.peak),
                            placeEvents(h, 4.0).size(), fit.meanGap);
            }
            std::printf("\n");
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
        else if (juce::String(argv[i]) == "--probe-places") return probePlaces();
        else if (juce::String(argv[i]) == "--render" && i + 2 < argc) {
            juce::StringArray settings;
            for (int k = i + 3; k < argc; ++k) settings.add(argv[k]);
            return renderToFile(argv[i + 1], juce::String(argv[i + 2]).getDoubleValue(), settings);
        }

    // --only name[,name]: run only those groups (a development aid).
    juce::StringArray only;
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String(argv[i]) == "--only") only = juce::StringArray::fromTokens(argv[i + 1], ",", "");
    const auto run = [&](const char* name) { return only.isEmpty() || only.contains(name); };
    Suite s;
    if (run("parameters")) testParameters(s);
    if (run("rates")) testRatesAndBlocks(s);
    if (run("world")) testWorldControls(s);
    if (run("layers")) testLayers(s);
    if (run("climate")) testClimateLayers(s);
    if (run("music")) testMusic(s);
    if (run("places")) testPlaces(s);
    if (run("placeworld")) testPlaceWorld(s);
    if (run("slots")) testPlaceSlots(s);
    if (run("patterns")) testPatterns(s);
    if (run("playmode")) testPlayMode(s);
    if (run("determinism")) testDeterminism(s);
    if (run("power")) testPowerAndMaster(s);
    if (run("tonal")) testTonalChanges(s);
    if (run("state")) testState(s);
    if (run("realtime")) testRealtimeSafety(s);
    if (run("editor")) testEditor(s, snapshot);
    std::cout << "\n" << s.passed << " passed, " << s.failed << " failed" << std::endl;
    return s.failed == 0 ? 0 : 1;
}
