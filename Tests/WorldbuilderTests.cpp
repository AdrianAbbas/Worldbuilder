// Offline tests through the real processBlock. Exit code 0 only if every check passes.
//   WorldbuilderTests                 run everything
//   WorldbuilderTests --snapshot f.png also write an image of the editor
//   WorldbuilderTests --snapshots dir [prefix]   images of each part of the interface for review
//   WorldbuilderTests --probe          print per-layer measurements (tuning aid)
//   WorldbuilderTests --probe-places   print each Place's level per character and element
//   WorldbuilderTests --only a,b       run only some groups (patterns, playmode, editor...)
//   WorldbuilderTests --render f.wav seconds [id=value ...]
//                                      render a world to a WAV file (listening aid)
#include "PluginEditor.h"
#include "DSP/Planet.h"
#include "PluginProcessor.h"
#include "Patterns/PatternMidi.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <complex>
#include <functional>
#include <cstdlib>
#include <iostream>
#include <new>
#include <set>
#include <algorithm>
#include <string>
#include <vector>

// Count C++ allocations on the calling thread while enabled (and the bytes asked for,
// for the memory metric).
namespace allocationProbe {
thread_local bool enabled = false;
thread_local std::size_t calls = 0, bytes = 0;
void mark(std::size_t n = 0) noexcept {
    if (enabled) ++calls;
    bytes += n;
}
}
void* operator new(std::size_t n) {
    allocationProbe::mark(n);
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t alignment) {
    allocationProbe::mark(n);
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

// The built Places the Place tests cover (--place key[,key] narrows them, a development aid).
std::vector<int> placeFilter;
std::vector<int> builtPlaces() {
    std::vector<int> out;
    for (int type = NoPlace + 1; type < PlaceTypeCount; ++type)
        if (placeImplemented[std::size_t(type)] && (placeFilter.empty() || std::find(placeFilter.begin(), placeFilter.end(), type) != placeFilter.end()))
            out.push_back(type);
    return out;
}

// The parameter id of a Place slot field ("p1_type"...).
std::string slotId(int slot, int field) { return specs[std::size_t(placeParam(slot, field))].id; }
// The id of an instrument's effect field ("p1_e2_rev"...).
std::string fxId(int slot, int element, int field) { return specs[std::size_t(placeParam(slot, fxField(element, field)))].id; }

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

    // Four active Places with edited patterns, mute and solo: everything comes back, and
    // the restored world plays the same (the first Places, then four each from 5b, 5c and 5d).
    for (const std::array<int, 4> types : { std::array<int, 4> { Castle, Village, Spaceport, Village }, std::array<int, 4> { Factory, City, Hive, Cavern },
                                            std::array<int, 4> { Ruins, Spaceship, SeaShip, Suburbs }, std::array<int, 4> { Mall, Military, Desert, Tavern } }) {
        if (!placeImplemented[std::size_t(types[3])]) continue;
        Host x;
        for (int slot = 0; slot < placeSlots; ++slot) {
            x.set(slotId(slot, PlType), float(types[std::size_t(slot)]));
            x.set(slotId(slot, PlCharacter), float(slot % 3));
        }
        x.p->patterns.syncTypes(x.p->slotTypes());
        for (int slot = 0; slot < placeSlots; ++slot)
            for (int e = 0; e < placeElements; e += 2) {
                auto el = editedElement({ makeNote(0, e, 4), makeNote(4 + slot, 2, 2, 90, slot == 1), makeNote(10, 4, 1, 70, false, 60) }, 0.2f, pat::VaryVelocity, 2);
                el.controls.evolve = 0.25f * float(slot);
                el.controls.mute = slot == 2 && e == 2;
                el.controls.solo = slot == 3 && e == 4;
                x.p->patterns.setElement(slot, types[std::size_t(slot)], e, el);
            }
        juce::MemoryBlock saved;
        x.p->getStateInformation(saved);
        Host y;
        y.p->setStateInformation(saved.getData(), int(saved.getSize()));
        int different = 0;
        for (int slot = 0; slot < placeSlots; ++slot)
            for (int e = 0; e < placeElements; ++e) {
                const auto a = x.p->patterns.element(slot, types[std::size_t(slot)], e), b = y.p->patterns.element(slot, types[std::size_t(slot)], e);
                if (!(a.controls == b.controls) || (a.controls.edited && !a.pattern.sameNotes(b.pattern))) ++different;
            }
        const std::string names = std::string(placeNames[std::size_t(types[0])]) + ", " + placeNames[std::size_t(types[1])] + ", "
                                  + placeNames[std::size_t(types[2])] + ", " + placeNames[std::size_t(types[3])];
        s.check(different == 0, "four Places with edited patterns, mute and solo survive a state round trip (" + names + ")", std::to_string(different) + " elements differ");
        const auto ax = x.render(6.0), by = y.render(6.0);
        s.check(ax.l == by.l && ax.r == by.r && measure(ax, 1.0).rmsDb > -60.0, "the restored four-Place world plays exactly the same (" + names + ")");
    }
}

Host placeHost(int type, int character, const std::vector<std::pair<std::string, float>>& extra = {}, double bpm = 120.0);

// Every instrument of slot 1 with the same effects.
void setAllFx(Host& h, int field, float value) {
    for (int e = 0; e < placeElements; ++e) h.set(fxId(0, e, field), value);
}

void testEffects(Suite& s) {
    s.section("instrument effects: shared reverbs and delays, character, tone, width");
    s.check(PlaceParamCount == PlFx + placeElements * FxFieldCount && FxFieldCount == 8, "every instrument of every slot has its eight effect parameters",
            std::to_string(placeSlotCount * placeElements * FxFieldCount) + " parameters");
    // Defaults: the slot's default Place (Castle) and the documented examples.
    s.check(instrumentFx(Temple, TemplePlace::Choir).reverbType == RevCathedral && instrumentFx(Tavern, TavernPlace::LuteStrum).reverbType == RevRoom,
            "defaults per instrument: the Temple choir in the Cathedral, the Tavern lute in the Room");
    {
        Host h;
        bool castle = true;
        for (int e = 0; e < placeElements; ++e)
            for (int f = 0; f < FxFieldCount; ++f) castle &= std::abs(h.get(fxId(0, e, f).c_str()) - instrumentFxDefault(Castle, e, f)) < 1.0e-4f;
        s.check(castle, "a new plugin's Castle (slot 1) starts with the Castle instruments' effects");
    }
    // A dry reference: Tavern (busy, every instrument) with no sends and no character.
    const auto dryHost = [](int type) {
        auto h = placeHost(type, 0, { { "p1_activity", 0.8f } });
        setAllFx(h, FxReverb, 0);
        setAllFx(h, FxDelay, 0);
        setAllFx(h, FxCharacter, ChOff);
        return h;
    };
    const auto dry = [&] {
        auto h = dryHost(Tavern);
        return measure(h.render(14.0), 4.0);
    }();
    // The dry tail: what is left a second after every instrument is switched off.
    const auto tailAfterOff = [](Host& h) {
        h.render(10.0);
        for (int e = 0; e < placeElements; ++e) h.set(slotId(0, PlElementOn + e), 0);
        const auto a = h.render(3.0);
        return measure(a, 0.15, 0.6);
    };
    const auto dryTail = [&] {
        auto h = dryHost(Tavern);
        return tailAfterOff(h);
    }();
    for (int t = 0; t < ReverbTypeCount; ++t) {
        const auto name = juce::StringArray::fromTokens(reverbChoices, "|", "")[t].toStdString();
        auto h = dryHost(Tavern);
        setAllFx(h, FxReverbType, float(t));
        setAllFx(h, FxReverb, 0.6f);
        const auto st = measure(h.render(14.0), 4.0);
        auto g = dryHost(Tavern);
        setAllFx(g, FxReverbType, float(t));
        setAllFx(g, FxReverb, 0.6f);
        const auto tail = tailAfterOff(g);
        s.check(st.finite && st.peak < 0.95 && difference(st, dry) > 0.5 && tail.rmsDb > dryTail.rmsDb + 3.0,
                "reverb " + name + ": audible (a tail after the instruments stop) and finite",
                "change " + fmt(difference(st, dry)) + " dB, tail " + fmt(tail.rmsDb) + " vs dry " + fmt(dryTail.rmsDb) + " dBFS, peak " + fmt(st.peak));
    }
    for (int t = 0; t < DelayTypeCount; ++t) {
        const auto name = juce::StringArray::fromTokens(delayChoices, "|", "")[t].toStdString();
        auto h = dryHost(Tavern);
        setAllFx(h, FxDelayType, float(t));
        setAllFx(h, FxDelay, 0.6f);
        const auto st = measure(h.render(14.0), 4.0);
        auto g = dryHost(Tavern);
        setAllFx(g, FxDelayType, float(t));
        setAllFx(g, FxDelay, 0.6f);
        const auto tail = tailAfterOff(g);
        s.check(st.finite && st.peak < 0.95 && difference(st, dry) > 0.5 && tail.rmsDb > dryTail.rmsDb + 3.0,
                "delay " + name + ": audible (repeats after the instruments stop) and finite",
                "change " + fmt(difference(st, dry)) + " dB, tail " + fmt(tail.rmsDb) + " vs dry " + fmt(dryTail.rmsDb) + " dBFS, peak " + fmt(st.peak));
    }
    s.metric("Ping-Pong delay seconds at 96 BPM (an eighth)", dsp::SendDelay::secondsFor(DlyPingPong, 96.0f), " s");
    s.check(std::abs(dsp::SendDelay::secondsFor(DlyDotted, 120.0f) - 0.375f) < 1.0e-4f && std::abs(dsp::SendDelay::secondsFor(DlyTape, 60.0f) - 1.0f) < 1.0e-4f,
            "delay times follow the tempo (a dotted eighth at 120 BPM is 375 ms, a quarter at 60 BPM 1 s)");
    for (int t = ChSaturation; t < CharacterTypeCount; ++t) {
        const auto name = juce::StringArray::fromTokens(characterFxChoices, "|", "")[t].toStdString();
        auto h = dryHost(Tavern);
        setAllFx(h, FxCharacter, float(t));
        setAllFx(h, FxAmount, 0.8f);
        const auto st = measure(h.render(14.0), 4.0);
        s.check(st.finite && st.peak < 0.95 && st.rmsDb > -60.0 && difference(st, dry) > 0.7, "character " + name + " at 80 %: audible and finite",
                "change " + fmt(difference(st, dry)) + " dB, " + fmt(st.rmsDb) + " dBFS");
    }
    {
        auto dark = dryHost(Tavern), bright = dryHost(Tavern);
        setAllFx(dark, FxTone, 0.0f);
        setAllFx(bright, FxTone, 1.0f);
        const auto a = measure(dark.render(14.0), 4.0), b = measure(bright.render(14.0), 4.0);
        const double tilt = (b.highDb - b.lowDb) - (a.highDb - a.lowDb);
        s.check(tilt > 8.0, "tone: dark to bright tilts the instruments",
                "highs against lows " + fmt(tilt) + " dB (highs +" + fmt(b.highDb - a.highDb) + ", lows -" + fmt(a.lowDb - b.lowDb) + ")");
        // The Place's own room spreads every instrument too; the World Space is left out.
        auto mono = dryHost(Tavern), wide = dryHost(Tavern);
        for (auto* h : { &mono, &wide }) {
            h->set("spacemix", 0.0f);
            h->set("p1_size", 0.0f);
        }
        setAllFx(mono, FxWidth, 0.0f);
        setAllFx(wide, FxWidth, 2.0f);
        const auto m = measure(mono.render(14.0), 4.0), w = measure(wide.render(14.0), 4.0);
        s.check(w.sideDb - m.sideDb > 4.0, "width: mono to wide (through the Place's own stereo effects)", "side +" + fmt(w.sideDb - m.sideDb) + " dB");
    }
    {
        // Switching types while playing: no click (no 5 ms step far above the music's own).
        auto h = dryHost(Tavern);
        setAllFx(h, FxReverb, 0.5f);
        setAllFx(h, FxDelay, 0.4f);
        setAllFx(h, FxAmount, 0.7f);
        h.render(4.0);
        Audio all;
        all.rate = h.rate;
        double worst = 0;
        for (int k = 0; k < 12; ++k) {
            setAllFx(h, FxReverbType, float(k % ReverbTypeCount));
            setAllFx(h, FxDelayType, float((k + 1) % DelayTypeCount));
            setAllFx(h, FxCharacter, float(1 + k % (CharacterTypeCount - 1)));
            const auto a = h.render(0.5);
            // The largest sample-to-sample jump, against the signal's own peak.
            double peak = 1.0e-9, jump = 0;
            for (std::size_t i = 1; i < a.l.size(); ++i) {
                peak = std::max(peak, double(std::abs(a.l[i])));
                jump = std::max(jump, double(std::abs(a.l[i] - a.l[i - 1])));
            }
            worst = std::max(worst, jump / peak);
            all.l.insert(all.l.end(), a.l.begin(), a.l.end());
            all.r.insert(all.r.end(), a.r.begin(), a.r.end());
        }
        s.check(measure(all, 0).finite && worst < 0.9, "changing reverb, delay and character types while playing does not click",
                "largest step " + fmt(100.0 * worst) + " % of the peak");
    }
    {
        // Idle buses sleep: no sends, and after their tails nothing is awake.
        auto h = dryHost(Tavern);
        h.render(12.0);
        s.check(h.p->engine.shared().awake() == 0, "the shared buses sleep when nothing sends to them", std::to_string(h.p->engine.shared().awake()) + " awake");
        auto busy = placeHost(Temple, 0);
        busy.render(2.0);
        s.check(busy.p->engine.shared().awake() > 0, "and wake when an instrument sends", std::to_string(busy.p->engine.shared().awake()) + " awake");
    }
    {
        // Four Places of six instruments, every instrument sending to a reverb and a delay
        // (all nine buses busy) and with a character effect: within the CPU budget.
        Host st(48000, 256);
        st.set("quality", 1);
        for (int w = 0; w < WorldControlCount; ++w) st.set(worldId(w), 0.7f);
        const std::array<int, 4> types { Castle, Temple, Spaceport, Tavern };
        for (int slot = 0; slot < placeSlots; ++slot) {
            st.set(slotId(slot, PlType), float(types[std::size_t(slot)]));
            st.set(slotId(slot, PlActivity), 1.0f);
            for (int e = 0; e < placeElements; ++e) {
                const int k = slot * placeElements + e;
                st.set(fxId(slot, e, FxReverb), 0.5f);
                st.set(fxId(slot, e, FxReverbType), float(k % ReverbTypeCount));
                st.set(fxId(slot, e, FxDelay), 0.4f);
                st.set(fxId(slot, e, FxDelayType), float(k % DelayTypeCount));
                st.set(fxId(slot, e, FxCharacter), float(1 + k % (CharacterTypeCount - 1)));
                st.set(fxId(slot, e, FxAmount), 0.6f);
                st.set(fxId(slot, e, FxTone), 0.3f + 0.05f * float(e));
                st.set(fxId(slot, e, FxWidth), 1.4f);
            }
        }
        Host plain(48000, 256);
        plain.set("quality", 1);
        for (int w = 0; w < WorldControlCount; ++w) plain.set(worldId(w), 0.7f);
        for (int slot = 0; slot < placeSlots; ++slot) {
            plain.set(slotId(slot, PlType), float(types[std::size_t(slot)]));
            plain.set(slotId(slot, PlActivity), 1.0f);
            for (int e = 0; e < placeElements; ++e) {
                plain.set(fxId(slot, e, FxReverb), 0.0f);
                plain.set(fxId(slot, e, FxDelay), 0.0f);
                plain.set(fxId(slot, e, FxCharacter), float(ChOff));
                plain.set(fxId(slot, e, FxTone), 0.5f);
                plain.set(fxId(slot, e, FxWidth), 1.0f);
            }
        }
        plain.render(1.0);
        const auto plainBegin = juce::Time::getHighResolutionTicks();
        plain.render(10.0);
        const double plainUsed = 100.0 * juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - plainBegin) / 10.0;
        s.metric("CPU, the same four Places with no instrument effects", plainUsed, " %");
        st.render(1.0);
        const auto begin = juce::Time::getHighResolutionTicks();
        const auto a = st.render(10.0);
        const double used = 100.0 * juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin) / 10.0;
        s.metric("CPU, four busy Places, 24 instruments all with sends and character effects", used, " %");
        s.check(measure(a, 0).finite && st.p->engine.shared().awake() == ReverbTypeCount + DelayTypeCount,
                "four Places with every effect in use render finite, all nine buses awake", std::to_string(st.p->engine.shared().awake()) + " awake");
        // Report only: the difference of two whole-plugin timings swings by several points
        // on a shared machine. The effects' own cost is checked below; the four Places
        // against the 15 % budget in the real-time group (with their default effects).
        s.metric("CPU, the instrument effects in a full four-Place render (report only)", used - plainUsed, " %");
    }
    {
        // The effects alone: four slots of six instruments, every one with a character
        // effect, tone, width and sends to a reverb and a delay; all nine buses busy.
        const float rate = 48000;
        std::array<dsp::InstrumentFx, placeSlots> slotFx;
        dsp::SharedFx bank;
        bank.prepare(rate);
        std::array<dsp::ElementFxControls, placeElements> controls {};
        for (auto& f : slotFx) f.prepare(rate);
        dsp::Stream noise;
        noise.setKey(3, 4);
        std::array<float, dsp::chunk> l {}, r {}, outL {}, outR {}, gain {};
        gain.fill(1.0f);
        const int chunks = int(10.0f * rate / float(dsp::chunk));
        // The input, made before the clock starts.
        std::vector<float> table(8192);
        for (std::size_t i = 0; i < table.size(); ++i) table[i] = 0.1f * noise.white(i);
        std::size_t at = 0;
        const auto begin = juce::Time::getHighResolutionTicks();
        for (int c = 0; c < chunks; ++c) {
            bank.begin(dsp::chunk);
            for (int slot = 0; slot < placeSlots; ++slot) {
                for (int e = 0; e < placeElements; ++e) {
                    const int k = slot * placeElements + e;
                    auto& x = controls[std::size_t(e)];
                    x.reverb = 0.5f;
                    x.reverbType = k % ReverbTypeCount;
                    x.delay = 0.4f;
                    x.delayType = k % DelayTypeCount;
                    x.character = 1 + k % (CharacterTypeCount - 1);
                    x.amount = 0.6f;
                    x.tone = 0.35f;
                    x.width = 1.4f;
                }
                auto& fx = slotFx[std::size_t(slot)];
                fx.begin(controls, dsp::chunk);
                for (int e = 0; e < placeElements; ++e) {
                    for (int i = 0; i < dsp::chunk; ++i) {
                        l[std::size_t(i)] = table[at++ & 8191];
                        r[std::size_t(i)] = table[(at + 4001) & 8191];
                    }
                    fx.process(e, l.data(), r.data(), dsp::chunk);
                }
                bank.add(fx.sends, gain.data(), gain.data(), dsp::chunk);
            }
            outL.fill(0);
            outR.fill(0);
            bank.process(outL.data(), outR.data(), dsp::chunk, 96.0f);
        }
        const double effects = 100.0 * juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin) / 10.0;
        // Report only (Milestone 10): cloud and CI machines are not representative; the
        // developer measures CPU on the Mac.
        s.metric("CPU, the effects alone: 24 instruments (character, tone, width, two sends) and nine busy buses (report only, budget 8 %)", effects, " %");
        s.check(std::isfinite(outL[0]), "every instrument effect in use on 24 instruments renders finite");
    }
    {
        // State: every effect parameter round trips, and an old project's instruments take
        // their Place's defaults.
        Host a;
        a.set("p2_type", float(Temple));
        a.set(fxId(1, 0, FxReverbType), float(RevPlate));
        a.set(fxId(1, 2, FxDelay), 0.77f);
        a.set(fxId(3, 5, FxCharacter), float(ChLoFi));
        a.set(fxId(0, 4, FxWidth), 1.9f);
        a.set(fxId(2, 1, FxTone), 0.12f);
        juce::MemoryBlock state;
        a.p->getStateInformation(state);
        Host b;
        b.p->setStateInformation(state.getData(), int(state.getSize()));
        int mismatches = 0;
        for (int slot = 0; slot < placeSlots; ++slot)
            for (int e = 0; e < placeElements; ++e)
                for (int f = 0; f < FxFieldCount; ++f) mismatches += std::abs(a.get(fxId(slot, e, f).c_str()) - b.get(fxId(slot, e, f).c_str())) > 1.0e-4f ? 1 : 0;
        s.check(mismatches == 0, "every instrument effect survives a state round trip", std::to_string(mismatches) + " differ");
        // The same project as Milestone 5 saved it (no effect parameters).
        auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), int(state.getSize()));
        for (int i = xml->getNumChildElements() - 1; i >= 0; --i)
            if (xml->getChildElement(i)->getStringAttribute("id").matchesWildcard("p?_e?_*", false)
                && !xml->getChildElement(i)->getStringAttribute("id").endsWith("_on"))
                xml->removeChildElement(xml->getChildElement(i), true);
        juce::MemoryBlock old;
        juce::AudioProcessor::copyXmlToBinary(*xml, old);
        Host c;
        c.p->setStateInformation(old.getData(), int(old.getSize()));
        s.check(std::abs(c.get(fxId(1, TemplePlace::Choir, FxReverbType).c_str()) - float(RevCathedral)) < 1.0e-4f
                    && std::abs(c.get(fxId(0, 0, FxReverb).c_str()) - instrumentFxDefault(Castle, 0, FxReverb)) < 1.0e-4f,
                "a project from before the effects: each instrument starts from its Place's defaults");
    }
}

//==============================================================================
// The rebuilt instrument models, each alone (no Place): in tune, partials where the
// acoustics tables put them, finite, no click at the start, and louder and brighter
// when played harder.
namespace models {
// The strongest frequency within 3 % of hz (a direct DFT, steps of 0.1 cent).
double peakNear(const std::vector<float>& y, std::size_t from, std::size_t length, double rate, double hz) {
    double best = -1, at = hz;
    for (double cents = -50.0; cents <= 50.0; cents += 0.25) {
        const double f = hz * std::exp2(cents / 1200.0);
        std::complex<double> sum = 0;
        const std::complex<double> step = std::polar(1.0, -2.0 * M_PI * f / rate);
        std::complex<double> w = 1;
        for (std::size_t i = 0; i < length; ++i) {
            const double window = 0.5 - 0.5 * std::cos(2.0 * M_PI * double(i) / double(length));
            sum += double(y[from + i]) * window * w;
            w *= step;
        }
        if (std::abs(sum) > best) {
            best = std::abs(sum);
            at = f;
        }
    }
    return at;
}
double rmsDb(const std::vector<float>& y, std::size_t from, std::size_t length) {
    double e = 0;
    for (std::size_t i = from; i < from + length; ++i) e += double(y[i]) * double(y[i]);
    return 10.0 * std::log10(e / double(length) + 1.0e-24);
}
// Spectral centroid (Hz) over 8192 samples.
double centroid(const std::vector<float>& y, std::size_t from, double rate) {
    std::vector<std::complex<double>> buf(8192);
    for (std::size_t i = 0; i < buf.size(); ++i) buf[i] = double(y[from + i]) * (0.5 - 0.5 * std::cos(2.0 * M_PI * double(i) / double(buf.size())));
    fft(buf);
    double num = 0, den = 0;
    for (std::size_t k = 1; k < buf.size() / 2; ++k) {
        const double m = std::abs(buf[k]);
        num += m * double(k) * rate / double(buf.size());
        den += m;
    }
    return num / std::max(den, 1.0e-12);
}
bool finite(const std::vector<float>& y) {
    for (float v : y)
        if (!std::isfinite(v)) return false;
    return true;
}
// No click: it starts from silence (the first sample small against the attack's peak)
// and the first millisecond is no burst far louder than the attack right after it (a
// pick's or bow's noise is part of the attack; a click stands out of it).
bool clickFree(const std::vector<float>& y, double rate) {
    double peak = 1.0e-9;
    for (std::size_t i = 0; i < std::size_t(0.05 * rate); ++i) peak = std::max(peak, double(std::abs(y[i])));
    const auto ms = std::size_t(0.001 * rate);
    const auto rms = [&](std::size_t from, std::size_t to) {
        double e = 0;
        for (std::size_t i = from; i < to; ++i) e += double(y[i]) * double(y[i]);
        return std::sqrt(e / double(to - from));
    };
    return std::abs(y[0]) < 0.2 * peak && rms(0, ms) < 2.0 * rms(ms, 20 * ms);
}
} // namespace models

void testModels(Suite& s) {
    using namespace models;
    s.section("instrument models: in tune, partials, finite, no clicks, velocity brightens");
    const double rate = 48000;
    const std::size_t seconds = std::size_t(rate);
    dsp::Stream noise;
    noise.setKey(7, 8);
    // Renders `length` samples of a voice started at velocity v (mono).
    using Render = std::function<std::vector<float>(double hz, float v)>;
    struct Model {
        const char* name;
        Render render;
        std::vector<double> ratios;   // partials expected (to the note)
        double hz;
        std::size_t from;              // analysis start (samples)
        double tolerance;              // cents
    };
    std::vector<Model> list;
    list.push_back({ "plucked string (guitar)", [&](double hz, float v) {
                        dsp::Pluck p;
                        p.prepare(float(rate), 40.0f);
                        p.start(float(hz), 2.0f, 0.5f, v, 0.0f, float(rate), 1ull << 30, dsp::Pluck::Shape {});
                        std::vector<float> y(seconds);
                        for (auto& x : y) x = p.tick(noise);
                        return y;
                    }, { 1, 2, 3 }, 196.0, 2400, 2.0 });
    list.push_back({ "plucked course (oud, two strings)", [&](double hz, float v) {
                        dsp::Pluck p;
                        p.prepare(float(rate), 40.0f);
                        p.start(float(hz), 1.5f, 0.5f, v, 0.0f, float(rate), 1ull << 31, dsp::Pluck::Shape { 0.14f, 0.4f, 0.08f, 1.0f, 3.5f, 0.45f, 0.0f });
                        std::vector<float> y(seconds);
                        for (auto& x : y) x = p.tick(noise);
                        return y;
                    }, { 1, 2 }, 146.8, 2400, 4.0 });
    list.push_back({ "bowed string (violin)", [&](double hz, float v) {
                        dsp::StringVoice b;
                        b.prepare(float(rate), 30.0f);
                        b.players = 1;
                        b.vibratoDepth = 0;
                        b.brightness = 0.6f;
                        b.start(float(hz), 0.08f, v, 0.5f, 99, false);
                        b.gain = v;
                        std::vector<float> y(seconds);
                        for (std::size_t i = 0; i < y.size(); ++i) {
                            if (i % dsp::chunk == 0) b.update();
                            y[i] = b.tick(noise);
                        }
                        return y;
                    }, { 1, 2, 3 }, 440.0, 14400, 3.0 });
    const auto synth = [&](int model, float cutoff, float formant) {
        return [&, model, cutoff, formant](double hz, float v) {
            dsp::SynthVoice b;
            b.model = model;
            b.velocity = v;
            b.cutoff = cutoff;
            b.formantHz = formant;
            b.detuneCents = 0;
            b.envOctaves = 0.3f;
            b.start(float(hz), 0.04f, 0.3f, 0.9f, float(rate));
            b.gain = v;
            std::vector<float> y(seconds);
            for (std::size_t i = 0; i < y.size(); ++i) {
                if (i % dsp::chunk == 0) b.update(float(rate));
                float l = 0, r = 0;
                b.tick(l, r);
                y[i] = l + r;
            }
            return y;
        };
    };
    list.push_back({ "brass (horn)", synth(dsp::SynthBrass, 1000.0f, 420.0f), { 1, 2, 3 }, 174.6, 9600, 3.0 });
    list.push_back({ "free reed (accordion)", synth(dsp::SynthReed, 2500.0f, 1800.0f), { 1, 2 }, 261.6, 9600, 3.0 });
    list.push_back({ "woodwind (flute)", [&](double hz, float v) {
                        dsp::WindVoice w;
                        w.h2 = 0.15f;
                        w.h3 = 0.08f;
                        w.vibratoDepth = 0;
                        w.wander = 0;
                        w.velocity = v;
                        w.start(float(hz), 0.05f, 0.6f, float(rate), 1ull << 32);
                        w.gain = v;
                        std::vector<float> y(seconds);
                        for (std::size_t i = 0; i < y.size(); ++i) {
                            if (i % dsp::chunk == 0) w.update(float(rate));
                            y[i] = w.tick(noise);
                        }
                        return y;
                    }, { 1, 2 }, 587.3, 9600, 5.0 });
    const auto modal = [&](std::vector<float> ratios, std::vector<float> decays, std::vector<float> gains, float hardness, float beat) {
        return [&, ratios, decays, gains, hardness, beat](double hz, float v) {
            dsp::Bell b;
            b.extras.beatHz = beat;
            b.start(float(hz), 3.0f, dsp::clamp01(hardness + 0.35f * (v - 0.6f)), v, 0.0f, float(rate), ratios.data(), decays.data(), gains.data(),
                    int(ratios.size()));
            std::vector<float> y(seconds);
            for (std::size_t i = 0; i < y.size(); ++i) {
                if (i % dsp::chunk == 0) b.update();
                y[i] = b.tick();
            }
            return y;
        };
    };
    list.push_back({ "church bell", modal({ 0.5f, 1.0f, 1.2f, 1.5f, 2.0f, 2.5f, 2.67f, 3.0f }, { 1.4f, 1.0f, 0.8f, 0.5f, 0.6f, 0.35f, 0.3f, 0.25f },
                                          { 0.5f, 0.8f, 0.6f, 0.25f, 1.0f, 0.4f, 0.3f, 0.3f }, 0.5f, 0.6f),
                     { 0.5, 1, 1.2, 1.5, 2 }, 392.0, 2400, 3.0 });
    list.push_back({ "marimba bar", modal({ 1.0f, 3.99f, 9.2f }, { 1.0f, 0.25f, 0.1f }, { 1.0f, 0.3f, 0.1f }, 0.35f, 0.0f), { 1, 3.99 }, 220.0, 1200, 3.0 });
    list.push_back({ "singing bowl", modal({ 1.0f, 2.778f, 5.181f, 8.163f }, { 1.0f, 0.7f, 0.5f, 0.3f }, { 1.0f, 0.5f, 0.3f, 0.15f }, 0.3f, 0.0f),
                     { 1, 2.778, 5.181 }, 300.0, 2400, 3.0 });
    const auto drum = [&](int kind) {
        return [&, kind](double hz, float v) {
            dsp::Drum d;
            d.kind = kind;
            d.position = 0.4f;
            d.hardness = dsp::clamp01(0.5f + 0.3f * (v - 0.6f));
            d.partial = 0.4f;
            d.start(float(hz), 1.0f, 1.5f, v, 0.0f, 800.0f, 0.7f, 0.1f, 0.0f, float(rate), 1ull << 33);
            std::vector<float> y(seconds);
            for (std::size_t i = 0; i < y.size(); ++i) {
                if (i % dsp::chunk == 0) d.update(float(rate));
                y[i] = d.tick(noise);
            }
            return y;
        };
    };
    list.push_back({ "membrane drum (tom)", drum(dsp::DrumMembrane), { 1, 1.594, 2.136 }, 110.0, 480, 6.0 });
    list.push_back({ "timpani", drum(dsp::DrumTimpani), { 1, 1.5, 1.98 }, 98.0, 480, 6.0 });
    list.push_back({ "choir (tenor ah)", [&](double hz, float v) {
                        dsp::ChoirVoice c;
                        c.vowel = 2;
                        c.vibratoDepth = 0;
                        c.detuneCents = 4;
                        c.velocity = v;
                        c.start(float(hz), 0.1f, float(rate));
                        c.gain = v;
                        std::vector<float> y(seconds);
                        for (std::size_t i = 0; i < y.size(); ++i) {
                            if (i % dsp::chunk == 0) c.update(float(rate));
                            y[i] = c.tick();
                        }
                        return y;
                    }, { 1, 2, 3 }, 220.0, 14400, 8.0 });
    list.push_back({ "tine electric piano", [&](double hz, float v) {
                        dsp::TineVoice t;
                        t.start(float(hz), 3.0f, 0.5f, v, float(rate));
                        std::vector<float> y(seconds);
                        for (std::size_t i = 0; i < y.size(); ++i) {
                            if (i % dsp::chunk == 0) t.update();
                            y[i] = t.tick();
                        }
                        return y;
                    }, { 1, 2 }, 261.6, 2400, 3.0 });

    for (const auto& m : list) {
        const std::string name = m.name;
        const auto soft = m.render(m.hz, 0.3f), hard = m.render(m.hz, 0.9f);
        s.check(finite(soft) && finite(hard), name + ": finite");
        s.check(clickFree(soft, rate) && clickFree(hard, rate), name + ": starts without a click");
        // Partials where expected, the note itself in tune.
        std::string where;
        bool inTune = true;
        for (std::size_t k = 0; k < m.ratios.size(); ++k) {
            const double expected = m.hz * m.ratios[k];
            const double found = peakNear(hard, m.from, 8192, rate, expected);
            const double cents = 1200.0 * std::log2(found / expected);
            inTune = inTune && std::abs(cents) <= (k == 0 ? m.tolerance : 2.5 * m.tolerance + 2.0);
            where += (k > 0 ? ", " : "") + fmt(m.ratios[k]) + "x " + fmt(cents) + " c";
        }
        s.check(inTune, name + ": in tune, partials where expected", where);
        const double level = rmsDb(hard, m.from, 8192) - rmsDb(soft, m.from, 8192);
        const double bright = centroid(hard, m.from, rate) / centroid(soft, m.from, rate);
        s.check(level > 3.0 && bright > 1.04, name + ": played harder, louder and brighter",
                "+" + fmt(level) + " dB, centroid x" + fmt(bright));
    }
}

//==============================================================================
// Distinctness (report only): every instrument of every Place alone, described by its
// spectral shape (24 bands, level removed) and its envelope (attack, how fast it falls
// away, how much its level moves); the closest pairs are listed for listening.
struct Fingerprint {
    std::array<double, 24> band {};
    double attack = 0, decay = 0, motion = 0;
};
Fingerprint fingerprint(const Audio& a, double from) {
    Fingerprint f;
    const std::size_t size = 4096;
    std::vector<std::complex<double>> buf(size);
    int frames = 0;
    for (std::size_t start = std::size_t(from * a.rate); start + size <= a.l.size(); start += size) {
        for (std::size_t i = 0; i < size; ++i) buf[i] = 0.5 * (a.l[start + i] + a.r[start + i]) * (0.5 - 0.5 * std::cos(2.0 * M_PI * double(i) / double(size)));
        fft(buf);
        for (std::size_t k = 1; k < size / 2; ++k) {
            const double hz = double(k) * a.rate / double(size);
            // 24 bands, a third of an octave from 50 Hz.
            const int b = int(std::floor(3.0 * std::log2(std::max(hz, 1.0) / 50.0)));
            if (b >= 0 && b < 24) f.band[std::size_t(b)] += std::norm(buf[k]);
        }
        ++frames;
    }
    double total = 0;
    for (double v : f.band) total += v;
    for (auto& v : f.band) v = 10.0 * std::log10(v / std::max(total, 1.0e-30) + 1.0e-9);
    // Envelope: 10 ms frames; attack = mean rise of the rises, decay = mean fall of the falls.
    const std::size_t w = std::size_t(0.01 * a.rate);
    std::vector<double> env;
    for (std::size_t start = std::size_t(from * a.rate); start + w <= a.l.size(); start += w) {
        double e = 0;
        for (std::size_t i = start; i < start + w; ++i) e += 0.5 * (a.l[i] * a.l[i] + a.r[i] * a.r[i]);
        env.push_back(10.0 * std::log10(e / double(w) + 1.0e-14));
    }
    double rise = 0, fall = 0, mean = 0;
    int rises = 0, falls = 0;
    for (std::size_t i = 1; i < env.size(); ++i) {
        const double d = env[i] - env[i - 1];
        if (d > 0.5) {
            rise += d;
            ++rises;
        } else if (d < -0.1) {
            fall -= d;
            ++falls;
        }
        mean += env[i];
    }
    mean /= double(std::max<std::size_t>(env.size(), 1));
    double var = 0;
    for (double e : env) var += (e - mean) * (e - mean);
    f.attack = rises > 0 ? rise / rises : 0.0;
    f.decay = falls > 0 ? fall / falls : 0.0;
    f.motion = std::sqrt(var / double(std::max<std::size_t>(env.size(), 1)));
    return f;
}
double distance(const Fingerprint& a, const Fingerprint& b) {
    double d = 0;
    for (std::size_t k = 0; k < a.band.size(); ++k) {
        const double x = std::clamp(a.band[k], -60.0, 0.0) - std::clamp(b.band[k], -60.0, 0.0);
        d += x * x / 24.0;
    }
    // The envelope counts as much as the spectrum: a few dB per frame of attack or decay.
    d += 4.0 * (a.attack - b.attack) * (a.attack - b.attack) + 16.0 * (a.decay - b.decay) * (a.decay - b.decay) + (a.motion - b.motion) * (a.motion - b.motion);
    return std::sqrt(d);
}

Host instrumentHost(int type, int character, int element, const std::vector<std::pair<std::string, float>>& extra = {}, double bpm = 120.0);

void testDistinct(Suite& s) {
    s.section("distinctness: every instrument against every other (report only)");
    struct Entry {
        std::string name;
        Fingerprint print;
    };
    std::vector<Entry> all;
    for (int type : builtPlaces())
        for (int e = 0; e < placeElements; ++e) {
            if (placeInfo(type).element[std::size_t(e)].kind == NoPattern) continue;
            // Dry (no reverb or delay sends): the instrument itself.
            auto h = instrumentHost(type, 0, e, { { "p1_activity", 0.7f }, { "spacemix", 0.0f }, { fxId(0, e, FxReverb), 0.0f }, { fxId(0, e, FxDelay), 0.0f } });
            const auto a = h.render(14.0);
            all.push_back({ std::string(placeNames[std::size_t(type)]) + " " + placeInfo(type).element[std::size_t(e)].name, fingerprint(a, 2.0) });
        }
    struct Pair {
        double d;
        std::size_t a, b;
    };
    std::vector<Pair> pairs;
    for (std::size_t i = 0; i < all.size(); ++i)
        for (std::size_t j = i + 1; j < all.size(); ++j) pairs.push_back({ distance(all[i].print, all[j].print), i, j });
    std::sort(pairs.begin(), pairs.end(), [](const Pair& x, const Pair& y) { return x.d < y.d; });
    double sum = 0;
    for (const auto& p : pairs) sum += p.d;
    s.metric("instruments compared", double(all.size()));
    s.metric("mean distance between instruments", sum / double(std::max<std::size_t>(pairs.size(), 1)));
    std::cout << "  closest pairs (listen to these side by side):\n";
    for (std::size_t k = 0; k < std::min<std::size_t>(20, pairs.size()); ++k)
        std::cout << "    " << fmt(pairs[k].d) << "  " << all[pairs[k].a].name << "  <->  " << all[pairs[k].b].name << "\n";
    // Report only for now: a pair this close is worth a listen, not a failure.
    s.metric("closest pair distance (report only, below 3 sounds alike)", pairs.empty() ? 0.0 : pairs.front().d);
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
        // Four Places, every built Place and every character in turn.
        const auto built = builtPlaces();
        for (int slot = 0; slot < placeSlots; ++slot) {
            h.set(slotId(slot, PlType), float(built[std::size_t(slot + quality * placeSlots) % built.size()]));
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

    // Memory: every slot keeps an engine of every built Place, prepared (report only).
    {
        const auto before = allocationProbe::bytes;
        Host m(48000, 512);
        s.metric("memory allocated by one instance at 48 kHz (" + std::to_string(builtPlaces().size()) + " built Places x 4 slots)",
                 double(allocationProbe::bytes - before) / 1048576.0, " MB");
    }
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
    // The budget: four Places of six instruments, busy, in Standard quality, well under
    // 15 % of one core (this machine; the brief's target is an Intel i5 laptop).
    {
        Host st(48000, 256);
        st.set("quality", 1);
        for (int w = 0; w < WorldControlCount; ++w) st.set(worldId(w), 0.7f);
        for (int slot = 0; slot < placeSlots; ++slot) {
            st.set(slotId(slot, PlType), float(Castle + slot % 3));
            st.set(slotId(slot, PlCharacter), float(slot % 3));
            st.set(slotId(slot, PlActivity), 1.0f);
        }
        st.render(1.0);
        const auto begin = juce::Time::getHighResolutionTicks();
        st.render(10.0);
        const double used = 100.0 * juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin) / 10.0;
        // Report only (Milestone 10): cloud and CI machines are not representative; the
        // developer measures CPU on the Mac.
        s.metric("CPU, Standard quality, four busy Places of six instruments (report only, budget 15 % of one core)", used, " %");
    }
    // Each built Place four times at once (its three Characters and a second seed), busy.
    for (int type : builtPlaces()) {
        Host st(48000, 256);
        st.set("quality", 1);
        for (int w = 0; w < WorldControlCount; ++w) st.set(worldId(w), 0.7f);
        for (int slot = 0; slot < placeSlots; ++slot) {
            st.set(slotId(slot, PlType), float(type));
            st.set(slotId(slot, PlCharacter), float(slot % 3));
            st.set(slotId(slot, PlSeed), float(1 + slot));
            st.set(slotId(slot, PlActivity), 1.0f);
        }
        st.render(1.0);
        const auto begin = juce::Time::getHighResolutionTicks();
        st.render(10.0);
        const double used = 100.0 * juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin) / 10.0;
        const std::string name = placeNames[std::size_t(type)];
        // Report only: on a shared container these swing by several points from run to run.
        s.metric("CPU, four busy Places, all " + name + ", Standard quality (report only, budget 15 %)", used, " %");
    }
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
    {
        // A new Place's instruments start from their own effect defaults.
        bool own = true;
        for (int e = 0; e < placeElements; ++e)
            for (int f = 0; f < FxFieldCount; ++f) own &= std::abs(h.get(fxId(2, e, f).c_str()) - instrumentFxDefault(Spaceport, e, f)) < 1.0e-4f;
        s.check(own, "a Place added from its card starts with its instruments' own effects (Spaceport)");
    }
    int unbuilt = NoPlace;
    for (int type = PlaceTypeCount - 1; type > NoPlace; --type)
        if (!placeImplemented[std::size_t(type)]) unbuilt = type;
    if (unbuilt != NoPlace) s.check(world->addPlace(unbuilt) == -1, "a Place from a later milestone cannot be added yet");
    const int fourth = world->addPlace(Castle);
    s.check(fourth == 3 && world->addPlace(Village) == -1, "at most four Places");
    h.render(0.5);
    {
        // CAPTURE: the footer's render and scene MIDI, as their buttons do it.
        auto& capture = world->capturePanel();
        const auto footerTop = editor->getHeight() - 100;
        s.check(capture.isVisible() && capture.getY() > footerTop && capture.getRight() < editor->getWidth() - 300 && capture.getWidth() >= 180,
                "the CAPTURE section sits in the footer, clear of the output knobs",
                juce::String(capture.getBounds().toString()).toStdString());
        capture.setBars(4);
        capture.setStems(true);
        capture.startRender();
        const bool finished = capture.waitForRender(60000);
        bool files = capture.renderedFiles().size() == 5;   // Castle, Village, Spaceport, Castle 2, Environment
        for (const auto& f : capture.renderedFiles()) files = files && f.existsAsFile() && f.getSize() > 100000;
        s.check(finished && files, "RENDER writes one WAV per Place and the environment (stems)", std::to_string(capture.renderedFiles().size()) + " files");
        capture.setStems(false);
        capture.startRender();
        capture.cancelRender();
        s.check(!capture.rendering() && capture.renderedFiles().isEmpty(), "RENDER can be cancelled");
        const auto midi = capture.tempMidiFile();
        s.check(capture.writeSceneMidi(midi) && midi.getSize() > 100, "MIDI writes the scene's patterns", midi.getFullPathName().toStdString());
        midi.deleteFile();
    }
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds());
    s.check(image.isValid(), "editor paints");
    save(image, snapshotPath);
    // The sci-fi category: the four earlier sci-fi Places and the six of Milestone 7.
    world->showCategory(SciFi);
    {
        int scifi = 0;
        for (int type = NoPlace + 1; type < PlaceTypeCount; ++type) scifi += placeInfo(type).category == SciFi ? 1 : 0;
        s.check(world->listedPlaces() == scifi && scifi == 10, "the SCI-FI chip lists the ten sci-fi Places", std::to_string(world->listedPlaces()));
        const auto scifiImage = editor->createComponentSnapshot(editor->getLocalBounds());
        save(scifiImage, snapshotPath.isEmpty() ? juce::String() : snapshotPath.upToLastOccurrenceOf(".", false, false) + "-scifi.png");
    }
    world->showCategory(-1);
    world->openDetail(1);
    const auto detailImage = editor->createComponentSnapshot(editor->getLocalBounds());
    s.check(detailImage.isValid(), "detail panel paints");
    save(detailImage, snapshotPath.isEmpty() ? juce::String() : snapshotPath.upToLastOccurrenceOf(".", false, false) + "-detail.png");
    // The Pattern Editor, on each patterned element of the first three slots.
    world->openPatterns(0);
    auto& patterns = world->patternEditor();
    s.check(patterns.isVisible() && patterns.slot() == 0 && patterns.element() == CastlePlace::Drone, "the Pattern Editor opens on the Place's first pattern");
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
        world->openPatterns(1, VillagePlace::Lute);
        auto& pe = world->patternEditor();
        const int before = pe.shown().pattern.count;
        pe.addNote(3, 4, 2, 90);
        const auto stored = h.p->patterns.element(1, Village, VillagePlace::Lute);
        s.check(stored.controls.edited && stored.pattern.count == before + 1, "drawing a note keeps the pattern as edited",
                std::to_string(before) + " -> " + std::to_string(stored.pattern.count));
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("wb-test-plucks.mid");
        const bool saved = pe.saveMidi(file, VillagePlace::Lute);
        const bool imported = pe.importMidi(file, VillagePlace::Marimba, false);
        const auto wood = h.p->patterns.element(1, Village, VillagePlace::Marimba);
        // Two notes of the same pitch starting together are one in MIDI (the drawn note
        // can land on a generated one).
        int doubled = 0;
        for (int i = 0; i < stored.pattern.count; ++i)
            for (int j = 0; j < i; ++j) {
                const auto &x = stored.pattern.notes[std::size_t(i)], &y = stored.pattern.notes[std::size_t(j)];
                doubled += x.step == y.step && x.degree == y.degree && x.alter == y.alter ? 1 : 0;
            }
        s.check(saved && imported && wood.controls.edited && wood.pattern.count == stored.pattern.count - doubled,
                "a pattern saved as MIDI imports into another element", std::to_string(wood.pattern.count) + " notes of " + std::to_string(stored.pattern.count)
                + " (" + std::to_string(doubled) + " doubled)");
        file.deleteFile();
        pe.generate();
        h.render(0.3);
        s.check(!h.p->patterns.element(1, Village, VillagePlace::Marimba).controls.edited, "Generate returns the element to a generated pattern");
    }
    // LOCATIONS: the row between the panels and the footer, and the markers on the globe.
    {
        auto& bar = world->locationBar();
        const auto barArea = bar.getBounds();
        s.check(editor->getLocalBounds().contains(barArea) && barArea.getWidth() > 900 && bar.isVisible(), "the LOCATIONS row spans the editor",
                barArea.toString().toStdString());
        bool chipsFit = true;
        for (int i = 0; i < locationCount; ++i) chipsFit &= bar.getLocalBounds().contains(bar.chipBounds(i)) && (i == 0 || bar.chipBounds(i).getX() > bar.chipBounds(i - 1).getRight());
        bool clear = true;
        for (auto* child : bar.getChildren()) clear &= !child->getBounds().intersects(bar.chipBounds(locationCount - 1)) && bar.getLocalBounds().contains(child->getBounds());
        s.check(chipsFit && clear, "eight Location chips side by side, clear of the Travel and Journey controls");
        bar.clickChip(2);
        s.check(h.p->locations.used(2) && !h.p->locations.used(0), "clicking an empty chip saves the world there");
        h.set("temperature", 0.9f);
        bar.clickChip(5);
        h.render(0.2);
        bar.clickChip(2);
        s.check(juce::roundToInt(h.get("destination")) == 3, "clicking a saved chip travels there (sets the Destination)");
        h.render(5.0);
        h.p->serviceLocations();
        s.check(std::abs(h.get("temperature") - 0.5f) < 1.0e-4f, "and the world arrives (Temperature back to where it was saved)", fmt(h.get("temperature")));
        bool onGlobe = true;
        for (int i = 0; i < locationCount; ++i) {
            const auto m = world->markerPosition(i);
            onGlobe &= editor->getLocalBounds().toFloat().contains(m);
            for (auto* child : editor->getChildren())
                if (child->isVisible() && child != &world->placeOverlays() && child != &world->patternEditor()) onGlobe &= !child->getBounds().toFloat().contains(m);
        }
        s.check(onGlobe, "the Location markers sit around the globe, clear of every control");
        h.set("traveltime", 4.0f);
        bar.clickChip(5);
        h.render(1.5);
        world->patternEditor().setVisible(false);   // the globe, with the comet on its orbit
        world->settle(0.2f);
        const auto img = editor->createComponentSnapshot(editor->getLocalBounds());
        s.check(img.isValid(), "the editor paints during a journey");
        save(img, snapshotPath.isEmpty() ? juce::String() : snapshotPath.upToLastOccurrenceOf(".", false, false) + "-travel.png");
    }
    editor.reset();
}


//==============================================================================
// Places.

// A host playing one Place alone in slot 1, at `bpm`, with extra settings.
Host placeHost(int type, int character, const std::vector<std::pair<std::string, float>>& extra, double bpm) {
    Host h;
    h.playHead.bpm = bpm;
    h.set("p1_type", float(type));
    // As a Place card does: its instruments start from their own effects.
    for (int e = 0; e < placeElements; ++e)
        for (int f = 0; f < FxFieldCount; ++f) h.set(fxId(0, e, f), instrumentFxDefault(type, e, f));
    h.set("p1_character", float(character));
    h.set("p1_solo", 1);
    for (const auto& [id, value] : extra) h.set(id, value);
    return h;
}

// Transport mode: the first playback after loading equals every later one, for every
// Place and Character (a Place engine reset after playing must be as it was when new).
void testFirstPlay(Suite& s) {
    s.section("Transport mode: the first playback equals later ones (every Place and Character)");
    std::string differ;
    int same = 0, total = 0;
    for (int type : builtPlaces())
        for (int character = 0; character < placeCharacters; ++character) {
            auto h = placeHost(type, character, { { "playmode", float(PlayTransport) } });
            h.playHead.playing = true;
            h.playHead.position = 0;
            const auto first = h.render(5.0);
            h.playHead.position = 0;
            const auto second = h.render(5.0);
            ++total;
            if (first.l == second.l && first.r == second.r) ++same;
            else differ += std::string(placeNames[std::size_t(type)]) + " " + std::to_string(character + 1) + "; ";
        }
    s.check(same == total, "playing from bar 1 again sounds exactly as the first time", std::to_string(same) + " of " + std::to_string(total) + ": " + differ);
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
        const int few = sparse.p->patterns.generated(0).pattern[VillagePlace::Lute].count, many = busy.p->patterns.generated(0).pattern[VillagePlace::Lute].count;
        s.check(sparse.p->patterns.generated(0).type == Village && many > few, "the generated pattern follows Activity (and is reported to the editor)",
                std::to_string(few) + " -> " + std::to_string(many) + " notes");
        // Generate: a new variant gives a fresh pattern.
        auto h = placeHost(Village, 0);
        h.render(1.0);
        const auto before = h.p->patterns.generated(0).pattern[VillagePlace::Lute];
        auto el = h.p->patterns.element(0, Village, VillagePlace::Lute);
        el.controls.variant = 1;
        h.p->patterns.setElement(0, Village, VillagePlace::Lute, el);
        h.render(1.0);
        const auto after = h.p->patterns.generated(0).pattern[VillagePlace::Lute];
        s.check(!before.sameNotes(after) && after.count > 0, "Generate makes a fresh pattern");
        // An edited pattern is kept when the knobs move.
        auto edited = editedElement({ makeNote(0, 0), makeNote(8, 2) });
        h.p->patterns.setElement(0, Village, VillagePlace::Lute, edited);
        h.render(1.0);
        h.p->engine.placeSlot(0).place();
        h.set("p1_activity", 1.0f);
        h.set("p1_character", 2.0f);
        h.render(2.0);
        const auto* playing = h.p->engine.placeSlot(0).playingPattern(VillagePlace::Lute);
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
        a.p->patterns.setElement(1, Village, VillagePlace::Marimba, e);
        auto generatedOnly = a.p->patterns.element(0, Castle, CastlePlace::Horns);
        generatedOnly.controls.randomness = 0.9f;
        generatedOnly.controls.variant = 3;
        a.p->patterns.setElement(0, Castle, CastlePlace::Horns, generatedOnly);
        juce::MemoryBlock state;
        a.p->getStateInformation(state);
        Host b;
        b.p->setStateInformation(state.getData(), int(state.getSize()));
        const auto got = b.p->patterns.element(1, Village, VillagePlace::Marimba);
        s.check(got.controls == e.controls && got.pattern.sameNotes(e.pattern), "an edited pattern and its controls are restored");
        const auto horns = b.p->patterns.element(0, Castle, CastlePlace::Horns);
        s.check(horns.controls == generatedOnly.controls && !horns.controls.edited, "a generated element keeps its controls and variant");
        const auto xml = juce::String::fromUTF8(static_cast<const char*>(state.getData()), int(state.getSize()));
        s.metric("saved state with one edited pattern", double(state.getSize()), " bytes");
        b.render(2.0);
        const auto* playing = b.p->engine.placeSlot(1).playingPattern(VillagePlace::Marimba);
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
        std::size_t firstDiff = x.l.size();
        double worstDiff = 0;
        for (std::size_t i = 0; i < x.l.size(); ++i)
            if (x.l[i] != y.l[i]) {
                firstDiff = std::min(firstDiff, i);
                worstDiff = std::max(worstDiff, double(std::abs(x.l[i] - y.l[i])));
            }
        s.check(x.l == y.l && x.r == y.r, "Transport: the same song position always gives the same sound (bounces match playback)",
                "first difference at sample " + std::to_string(firstDiff) + ", largest " + fmt(toDb(worstDiff)) + " dB");
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
    for (int type : builtPlaces()) {
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
            // Each switch takes its instrument out: none of its notes play, and the sound changes.
            auto full = placeHost(type, 0, { { "p1_activity", 0.8f } });
            const auto all = full.render(24.0);
            for (int e = 0; e < placeElements; ++e) {
                auto without = placeHost(type, 0, { { "p1_activity", 0.8f }, { slotId(0, PlElementOn + e), 0.0f } });
                const auto other = without.render(24.0);
                double diff = 0;
                for (std::size_t i = 0; i < all.l.size(); ++i) diff += std::abs(all.l[i] - other.l[i]);
                diff /= double(all.l.size());
                const auto left = placeLog(without, e).size();
                s.check(diff > 1.0e-5 && left == 0, place + ": switching off " + placeInfo(type).element[std::size_t(e)].name + " takes it out",
                        "mean difference " + fmt(diff * 1.0e4) + "e-4, " + std::to_string(left) + " of its notes left");
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
            const double own = double(h.p->engine.placeSlot(0).place()->freeTempo()) * (0.85 + 0.3 * 0.8);
            const double ownLock = eventGridLock(at, own), hostLock = eventGridLock(at, 132.0);
            s.check(ownLock > 0.9 && hostLock < 0.5, place + ": free, it keeps its own tempo",
                    "grid lock " + fmt(ownLock) + " at " + fmt(own) + " BPM vs " + fmt(hostLock) + " at the host's 132");
        }
    }
}

// Every instrument of every Place, alone (its solo in the Pattern Editor): audible,
// finite, in the key (each note's pitch class), on the host's tempo grid.
Host instrumentHost(int type, int character, int element, const std::vector<std::pair<std::string, float>>& extra, double bpm) {
    auto h = placeHost(type, character, extra, bpm);
    h.p->patterns.syncTypes(h.p->slotTypes());
    auto el = h.p->patterns.element(0, type, element);
    el.controls.solo = true;
    h.p->patterns.setElement(0, type, element, el);
    return h;
}

void testInstruments(Suite& s) {
    for (int type : builtPlaces()) {
        const std::string place = placeNames[std::size_t(type)];
        s.section((place + ": every instrument alone (solo), in key and on tempo").c_str());
        const auto& info = placeInfo(type);
        int instruments = 0;
        for (const auto& e : info.element) instruments += e.kind != NoPattern ? 1 : 0;
        s.check(instruments >= 5, place + " has at least five instruments", std::to_string(instruments));
        for (int e = 0; e < placeElements; ++e) {
            const auto& spec = info.element[std::size_t(e)];
            if (spec.kind == NoPattern) continue;
            const std::string name = place + " " + spec.name;
            // Audible and finite alone, in every Character (rare sounds get a busy setting).
            std::string levels;
            bool audible = true, finite = true;
            for (int c = 0; c < placeCharacters; ++c) {
                auto h = instrumentHost(type, c, e, { { "p1_activity", 0.9f } });
                const auto st = measure(h.render(36.0), 2.0);
                audible = audible && st.rmsDb > -72.0;
                finite = finite && st.finite && st.peak < 0.9;
                levels += (c > 0 ? ", " : "") + fmt(st.rmsDb);
            }
            s.check(audible && finite, name + ": audible and finite alone in every Character", levels + " dBFS RMS");
            // In the key: G major and A Phrygian, every note's pitch class.
            if (spec.kind == Melodic) {
                for (auto [root, mood] : { std::pair<int, int> { 7, 1 }, { 9, 3 } }) {
                    int notes = 0, outside = 0;
                    for (int c = 0; c < placeCharacters; ++c) {
                        auto h = instrumentHost(type, c, e, { { "p1_activity", 0.9f }, { "root", float(root) }, { "mood", float(mood) } });
                        h.render(24.0);
                        for (const auto& n : placeLog(h, e)) {
                            ++notes;
                            outside += music::inScale(mood, int(std::lround(n.pitch))) ? 0 : 1;
                        }
                    }
                    s.check(notes > 0 && outside == 0, name + ": every note in " + (mood == 1 ? "G major" : "A Phrygian"),
                            std::to_string(notes) + " notes, " + std::to_string(outside) + " outside");
                }
            }
            // On the host's grid at two tempos (no swing, no variation).
            for (double bpm : { 84.0, 132.0 }) {
                std::vector<double> at;
                for (int c = 0; c < placeCharacters; ++c) {
                    auto h = instrumentHost(type, c, e, { { "p1_activity", 0.9f }, { "p1_swing", 0.0f }, { "p1_variation", 0.0f } }, bpm);
                    h.render(30.0);
                    for (const auto& n : placeLog(h, e))
                        if (double(n.clock) / h.rate >= 1.0) at.push_back(double(n.clock) / h.rate);
                }
                const double lock = eventGridLock(at, bpm);
                s.check(!at.empty() && lock > 0.97, name + ": on the host's sixteenth grid at " + fmt(bpm) + " BPM",
                        std::to_string(at.size()) + " notes, grid lock " + fmt(lock));
            }
        }
    }
    s.section("instruments: mute, and held notes picked up mid-pattern");
    {
        // A muted instrument plays nothing; the rest go on.
        auto h = placeHost(Village, 0, { { "p1_activity", 0.9f } });
        h.p->patterns.syncTypes(h.p->slotTypes());
        auto el = h.p->patterns.element(0, Village, VillagePlace::Lute);
        el.controls.mute = true;
        h.p->patterns.setElement(0, Village, VillagePlace::Lute, el);
        h.render(16.0);
        s.check(placeLog(h, VillagePlace::Lute).empty() && !placeLog(h, VillagePlace::HandDrums).empty(), "a muted instrument is silent, the others play",
                std::to_string(placeLog(h, VillagePlace::Lute).size()) + " lute notes");
    }
    {
        // Transport mode located into the middle of a four-bar drone: it sounds at once.
        for (int type : builtPlaces()) {
            int e = -1;   // its first held instrument
            for (int k = placeElements - 1; k >= 0; --k)
                if (placeInfo(type).element[std::size_t(k)].sustained) e = k;
            if (e < 0) continue;
            Host h;
            h.set("playmode", float(PlayTransport));
            h.playHead.bpm = 120;
            h.set("p1_type", float(type));
            h.set("p1_solo", 1);
            h.p->patterns.syncTypes(h.p->slotTypes());
            auto el = h.p->patterns.element(0, type, e);
            el.controls.solo = true;
            h.p->patterns.setElement(0, type, e, el);
            h.playHead.playing = true;
            h.playHead.position = int64_t(48000.0 * 5.3);   // 2.65 bars into the pattern
            const auto st = measure(h.render(3.0), 1.0);
            s.check(st.rmsDb > -50.0, std::string(placeNames[std::size_t(type)]) + ": a held instrument sounds at once after a locate mid-pattern",
                    fmt(st.rmsDb) + " dBFS");
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
    for (int type : builtPlaces()) {
        const std::string place = placeNames[std::size_t(type)];
        {
            auto a = placeHost(type, 0, { { "population", 0.0f } }), b = placeHost(type, 0, { { "population", 1.0f } });
            const auto x = a.render(32.0), y = b.render(32.0);
            const auto few = placeEvents(a).size(), many = placeEvents(b).size();
            const double diff = difference(measure(x, 4.0), measure(y, 4.0));
            s.check(double(many) > 1.2 * double(few) && diff > 1.0, place + ": Population makes it busier and deepens its effect",
                    std::to_string(few) + " -> " + std::to_string(many) + " notes and hits, " + fmt(diff) + " dB");
        }
        // Machinery and Danger: 1 dB until Milestone 6. Each instrument's own reverb and
        // delay now add spaces these controls do not change, which compresses the measure
        // (0.5-1.5 dB depending on the effect settings alone): 0.5 dB, the metric reported.
        for (auto [id, threshold] : { std::pair<const char*, double> { "machinery", 0.5 }, { "life", 0.2 }, { "danger", 0.5 } }) {
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
    // A Place from a later milestone is silent (while any is left to build).
    if (std::find(placeImplemented.begin() + 1, placeImplemented.end(), false) != placeImplemented.end()) {
        int later = NoPlace;
        for (int type = PlaceTypeCount - 1; type > NoPlace; --type)
            if (!placeImplemented[std::size_t(type)]) later = type;
        auto h = placeHost(later, 0);
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
    for (int type : builtPlaces())
        for (int c = 0; c < placeCharacters; ++c) {
            std::printf("\n%-10s %-15s", placeNames[std::size_t(type)], placeInfo(type).characters[std::size_t(c)]);
            for (int e = -1; e < placeElements; ++e) {
                std::vector<std::pair<std::string, float>> settings;
                for (int k = 0; k < placeElements; ++k)
                    if (e >= 0 && k != e) settings.push_back({ slotId(0, PlElementOn + k), 0.0f });
                auto h = placeHost(type, c, settings);
                const auto a = h.render(32.0);
                const auto st = measure(a, 4.0);
                const auto fit = keyFit(chroma(a, 4.0, 100.0, 5000.0), 2, 0);
                const int notes = e < 0 ? -1 : h.p->engine.placeSlot(0).generatedPattern(e).count;
                std::printf("\n    %-12s %6.1f/%5.1f dBFS  %3zu notes played  %3d in the pattern  key %.1f", e < 0 ? "all" : placeInfo(type).element[std::size_t(e)].name,
                            st.rmsDb, toDb(st.peak), placeEvents(h, 4.0).size(), notes, fit.meanGap);
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

//==============================================================================
// Capture and export (Milestone 8): offline renders and scene MIDI.
void testCapture(Suite& s) {
    s.section("capture: offline render matches Transport playback, stems, scene MIDI");
    // A world of three Places with an edited pattern, as a host at 120 bpm sees it.
    const auto makeHost = [](int block = 512) {
        auto h = std::make_unique<Host>(48000, block);
        h->playHead.bpm = 120;
        h->set("playmode", float(PlayTransport));
        h->set("p1_type", float(Castle));
        h->set("p2_type", float(Megacity));
        h->set("p3_type", float(Forest));
        h->set("wetness", 0.6f);
        h->set("intensity", 0.7f);
        h->p->patterns.syncTypes(h->p->slotTypes());
        h->p->patterns.setElement(0, Castle, 1, editedElement({ makeNote(0, 0, 4), makeNote(6, 2, 2, 80), makeNote(10, 4, 3, 60) }, 0.4f));
        // The host has played once, so the plugin knows its tempo.
        h->playHead.playing = true;
        h->render(0.1);
        h->playHead.playing = false;
        h->render(0.3);
        return h;
    };
    auto host = makeHost();
    const auto scene = host->p->captureScene();
    s.check(scene.bpm == 120.0f && scene.sampleRate == 48000.0 && scene.controls.places[1].type == Megacity && scene.patterns[0].element[1].controls.edited,
            "the scene holds the host's tempo and rate, the Places and the edited patterns");
    const int bars = 4;
    const auto samples = capture::lengthInSamples(scene, bars);
    s.check(samples == 4 * 4 * 24000, "4 bars at 120 bpm are exactly 8 s", std::to_string(samples));

    const auto offline = [&](const EngineControls& c, int block = 512) {
        Audio a;
        a.rate = scene.sampleRate;
        capture::render(scene, c, samples, [&](const float* l, const float* r, int n) {
            a.l.insert(a.l.end(), l, l + n);
            a.r.insert(a.r.end(), r, r + n);
            return true;
        }, nullptr, nullptr, block);
        return a;
    };
    const auto begin = juce::Time::getMillisecondCounterHiRes();
    const auto mix = offline(scene.controls);
    const double ms = juce::Time::getMillisecondCounterHiRes() - begin;
    const auto st = measure(mix, 0.5);
    s.check(mix.l.size() == std::size_t(samples) && st.finite && st.rmsDb > -40.0 && st.peak < 1.0, "the mix renders, audible and finite",
            fmt(st.rmsDb) + " dB, " + fmt(toDb(st.peak)) + " dBFS peak");
    s.metric("offline render speed (3 Places)", 8000.0 / std::max(ms, 1.0), "x real time");
    s.check(ms < 8000.0, "the render is faster than real time", fmt(ms) + " ms for 8 s");
    const auto again = offline(scene.controls, 300);
    s.check(again.l == mix.l && again.r == mix.r, "the render is deterministic (any block size)");

    // Real-time playback from bar 1 in Transport mode, at another block size.
    {
        auto live = makeHost(333);
        live->playHead.playing = true;
        live->playHead.position = 0;
        const auto played = live->render(double(samples) / 48000.0);
        std::size_t firstDiff = played.l.size();
        for (std::size_t i = 0; i < played.l.size() && i < mix.l.size(); ++i)
            if (played.l[i] != mix.l[i] || played.r[i] != mix.r[i]) {
                firstDiff = i;
                break;
            }
        s.check(played.l == mix.l && played.r == mix.r, "the render matches real-time Transport playback from bar 1, sample for sample",
                "first difference at sample " + std::to_string(firstDiff));
    }

    // Stems.
    {
        auto c = scene.controls;
        auto stemScene = scene;
        stemScene.controls.places[2].mute = true;   // the Forest is muted: no stem
        const auto list = capture::passes(stemScene, true);
        std::string names;
        for (const auto& pass : list) names += pass.name.toStdString() + ", ";
        s.check(list.size() == 3 && list[0].name == "Castle" && list[1].name == "Cyberpunk Megacity" && list[2].name == "Environment",
                "stems: one per audible Place and the environment (muted Places have none)", names);
        double sum = 0;
        bool each = true;
        for (const auto& pass : list) {
            const auto stem = offline(pass.controls);
            const auto x = measure(stem, 0.5);
            each = each && x.finite && x.rmsDb > -55.0;
            sum += std::pow(10.0, x.rmsDb / 10.0);
        }
        const auto mixed = measure(offline(stemScene.controls), 0.5);
        s.check(each, "every stem is audible and finite");
        s.metric("stems' summed power against the mix", 10.0 * std::log10(sum) - mixed.rmsDb, " dB");
        s.check(std::abs(10.0 * std::log10(sum) - mixed.rmsDb) < 3.0, "the stems add up to about the mix", fmt(10.0 * std::log10(sum)) + " vs " + fmt(mixed.rmsDb) + " dB");
        // A soloed Place: only its stem.
        stemScene.controls.places[1].solo = true;
        const auto solo = capture::passes(stemScene, true);
        s.check(solo.size() == 1 && solo[0].name == "Cyberpunk Megacity", "stems follow the solos", std::to_string(solo.size()));
        s.check(capture::passes(scene, false).size() == 1, "the mix is one file");
        juce::ignoreUnused(c);
    }

    // WAV files.
    {
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("wb-capture-test.wav");
        std::atomic<float> progress { 0 };
        const bool written = capture::renderToWav(scene, capture::passes(scene, false)[0], bars, file, nullptr, &progress);
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(new juce::FileInputStream(file), true));
        bool same = reader != nullptr && reader->lengthInSamples == samples && reader->numChannels == 2 && reader->sampleRate == 48000.0 && reader->bitsPerSample == 24;
        if (same) {
            juce::AudioBuffer<float> back(2, int(samples));
            reader->read(&back, 0, int(samples), 0, true, true);
            double worst = 0;
            for (int i = 0; i < int(samples); ++i)
                worst = std::max({ worst, double(std::abs(back.getSample(0, i) - mix.l[std::size_t(i)])), double(std::abs(back.getSample(1, i) - mix.r[std::size_t(i)])) });
            same = worst < 1.0e-5;
        }
        s.check(written && same && progress.load() == 1.0f, "the render writes a 24-bit WAV at the host's rate, exactly as long as the bars");
        reader.reset();
        file.deleteFile();
        std::atomic<bool> cancel { true };
        s.check(!capture::renderToWav(scene, capture::passes(scene, false)[0], bars, file, &cancel) && !file.existsAsFile(), "a cancelled render leaves no file");
    }

    // Scene MIDI.
    {
        const auto tracks = capture::sceneTracks(scene);
        int castle = 0, megacity = 0, forest = 0;
        bool named = true;
        for (const auto& t : tracks) {
            const int type = scene.controls.places[std::size_t(t.slot)].type;
            (type == Castle ? castle : type == Megacity ? megacity : forest) += 1;
            named = named && t.name == juce::String(placeNames[std::size_t(type)]) + " - " + placeInfo(type).element[std::size_t(t.element)].name;
        }
        int expected = 0;
        for (int slot = 0; slot < 3; ++slot)
            for (int e = 0; e < placeElements; ++e) expected += patternInfo(scene.controls.places[std::size_t(slot)].type, e).kind != NoPattern ? 1 : 0;
        s.check(int(tracks.size()) == expected && castle > 0 && megacity > 0 && forest > 0 && named, "scene MIDI: one named track per patterned instrument of every active Place",
                std::to_string(tracks.size()) + " of " + std::to_string(expected));
        bool editedUsed = false;
        for (const auto& t : tracks)
            if (t.slot == 0 && t.element == 1) editedUsed = t.pattern.sameNotes(scene.patterns[0].element[1].pattern);
        s.check(editedUsed, "scene MIDI uses the edited pattern where there is one");
        // The generated ones are the patterns the slots play.
        bool generatedSame = true;
        for (const auto& t : tracks) {
            const auto* playing = host->p->engine.placeSlot(t.slot).playingPattern(t.element);
            generatedSame = generatedSame && playing != nullptr && playing->sameNotes(t.pattern);
        }
        s.check(generatedSame, "scene MIDI holds the patterns the Places play");

        const auto file = capture::sceneMidi(tracks, 16, scene.bpm);
        juce::MemoryOutputStream out;
        file.writeTo(out, 1);
        juce::MemoryInputStream in(out.getData(), out.getDataSize(), false);
        juce::MidiFile read;
        const bool parsed = read.readFrom(in);
        bool aligned = parsed && read.getNumTracks() == int(tracks.size()) + 1;
        double tempo = 0;
        for (int i = 0; parsed && i < read.getTrack(0)->getNumEvents(); ++i)
            if (const auto& m = read.getTrack(0)->getEventPointer(i)->message; m.isTempoMetaEvent()) tempo = 60.0 / m.getTempoSecondsPerQuarterNote();
        int notes = 0;
        for (int t = 1; aligned && t < read.getNumTracks(); ++t) {
            const auto* track = read.getTrack(t);
            juce::String name;
            double last = 0;
            for (int i = 0; i < track->getNumEvents(); ++i) {
                const auto& m = track->getEventPointer(i)->message;
                if (m.isTrackNameEvent()) name = m.getTextFromTextMetaEvent();
                if (m.isNoteOn()) ++notes;
                if (m.isNoteOnOrOff()) last = std::max(last, m.getTimeStamp());
                if (m.isNoteOn() && m.getTimeStamp() < 0) aligned = false;
            }
            aligned = aligned && name == tracks[std::size_t(t - 1)].name && last <= 16.0 * 4.0 * 960.0 && track->getEndTime() == 16.0 * 4.0 * 960.0;
        }
        s.check(aligned && std::abs(tempo - 120.0) < 0.01 && notes > 0, "the MIDI file: a tempo track at 120 bpm, then named tracks, all from bar 1 to bar 17",
                std::to_string(read.getNumTracks()) + " tracks, " + std::to_string(notes) + " notes, " + fmt(tempo) + " bpm");

        // Round trip: each track imported into the Pattern Editor writes the same notes again.
        int good = 0;
        std::string bad;
        for (const auto& t : tracks) {
            const int trackBars = std::max(1, int(std::ceil(t.pattern.lengthBeats() / 4.0 - 1.0e-9)));
            const auto one = capture::sceneMidi({ t }, trackBars, scene.bpm);
            juce::MemoryOutputStream o;
            one.writeTo(o, 1);
            juce::MemoryInputStream i(o.getData(), o.getDataSize(), false);
            juce::MidiFile r;
            r.readFrom(i);
            pat::Pattern imported;
            pat::fromMidi(r, imported, t.target, false, t.pattern.resolution);
            auto copy = t;
            copy.pattern = imported;
            const auto events = [&](const juce::MidiFile& f) {
                std::vector<std::tuple<double, int, int, bool>> e;
                for (int k = 0; k < f.getNumTracks(); ++k)
                    for (int x = 0; x < f.getTrack(k)->getNumEvents(); ++x)
                        if (const auto& m = f.getTrack(k)->getEventPointer(x)->message; m.isNoteOnOrOff())
                            e.emplace_back(m.getTimeStamp(), m.getNoteNumber(), m.isNoteOn() ? m.getVelocity() : 0, m.isNoteOn());
                std::sort(e.begin(), e.end());
                return e;
            };
            if (events(one) == events(capture::sceneMidi({ copy }, trackBars, scene.bpm))) ++good;
            else bad += t.name.toStdString() + "; ";
        }
        s.check(good == int(tracks.size()), "exported MIDI round-trips into the Pattern Editor (every track)", std::to_string(good) + " of " + std::to_string(tracks.size()) + " " + bad);

        // Switched off and muted instruments are left out; so are muted Places.
        auto fewer = scene;
        fewer.controls.places[0].elementOn[0] = false;
        fewer.controls.places[2].mute = true;
        fewer.patterns[1].element[0].controls.mute = true;
        const auto less = capture::sceneTracks(fewer);
        bool none = true;
        for (const auto& t : less) none = none && !(t.slot == 0 && t.element == 0) && t.slot != 2 && !(t.slot == 1 && t.element == 0);
        s.check(none && less.size() < tracks.size(), "instruments switched off or muted, and muted Places, are not exported");
    }
}

//==============================================================================
// Automation-first (Milestone 8): the controls a composer shapes a cue with are host
// parameters, and a sudden change of any of them (a step in the automation) never
// clicks or jumps in level beyond what the world does by itself in either state.
void testAutomation(Suite& s) {
    s.section("automation: every important control is automatable and changes smoothly");
    const std::vector<std::pair<std::string, std::string>> important {
        { "intensity", "Score Intensity" }, { "p1_type", "a slot's Place" }, { "p1_level", "Place level" }, { "p1_mute", "Place mute" },
        { "p1_solo", "Place solo" }, { "p1_character", "Character" }, { "p1_activity", "Place Activity" }, { "p1_tension", "Place Tension" },
        { "p1_tone", "Place Tone" }, { "p1_distance", "Place distance" }, { "p1_pan", "Place pan" }, { "p1_e1", "instrument level" },
        { "p1_e1_on", "instrument on" }, { "p1_e1_rev", "instrument reverb" }, { "p1_e1_char", "instrument character effect" },
        { "temperature", "Temperature" }, { "danger", "Danger" }, { "scale", "Scale" }, { "root", "Root" }, { "mood", "Mood" },
        { "master", "Master Level" }, { "power", "Power" }, { "destination", "Destination" }, { "traveltime", "Travel Time" },
        { "journeymode", "Journey Mode" }, { "journeyfrom", "Journey From" }, { "journeyto", "Journey To" }, { "journey", "Journey" } };
    {
        WorldbuilderProcessor p;
        std::string missing;
        for (const auto& [id, name] : important)
            if (auto* parameter = p.parameters.getParameter(id); parameter == nullptr || !parameter->isAutomatable()) missing += name + " ";
        for (int w = 0; w < WorldControlCount; ++w)
            if (p.parameters.getParameter(worldId(w)) == nullptr) missing += std::string(worldId(w)) + " ";
        s.check(missing.empty(), "Score Intensity, the Place controls, Characters, instrument effects, the World Controls and the Locations are host parameters", missing);
    }

    // A four-Place world playing in Transport mode from bar 5.
    const auto world = [](const std::vector<std::pair<std::string, float>>& settings) {
        auto h = std::make_unique<Host>(48000, 512);
        h->playHead.bpm = 120;
        h->set("playmode", float(PlayTransport));
        const std::array<int, 4> types { Castle, Megacity, Forest, Nebula };
        for (int slot = 0; slot < placeSlotCount; ++slot) {
            h->set(slotId(slot, PlType), float(types[std::size_t(slot)]));
            for (int e = 0; e < placeElements; ++e)
                for (int f = 0; f < FxFieldCount; ++f) h->set(fxId(slot, e, f), instrumentFxDefault(types[std::size_t(slot)], e, f));
        }
        for (const auto& [id, value] : settings) h->set(id, value);
        h->playHead.playing = true;
        h->playHead.position = 48000 * 8;
        return h;
    };
    // A click: the render with the change equals the steady one up to the change, so
    // their difference starts from zero. A smoothed change lets it grow over tens of
    // milliseconds; a jump makes it as bright as the music at once. The measure: the
    // high-frequency energy (second difference) of the difference in the first 3 ms,
    // against that of the steady world (dB).
    const auto suddenness = [](const Audio& changed, const Audio& base, double from) {
        double diff = 0, ref = 0;
        const auto begin = std::size_t(from * base.rate), end = begin + std::size_t(0.003 * base.rate);
        for (std::size_t i = begin; i < end && i < base.l.size(); ++i)
            for (const auto* ch : { &base.l, &base.r }) {
                const auto& x = *ch;
                const auto& y = ch == &base.l ? changed.l : changed.r;
                const double d2 = (y[i] - x[i]) - 2.0 * (y[i - 1] - x[i - 1]) + (y[i - 2] - x[i - 2]);
                const double x2 = x[i] - 2.0 * x[i - 1] + x[i - 2];
                diff += d2 * d2;
                ref += x2 * x2;
            }
        return 10.0 * std::log10((diff + 1.0e-30) / (ref + 1.0e-30));
    };
    // The largest rise of the 20 ms level in [from, to) s.
    const auto rise = [](const Audio& a, double from, double to) {
        const auto w = std::size_t(0.02 * a.rate);
        double top = -240, previous = 0;
        bool first = true;
        for (auto start = std::size_t(from * a.rate); start + w <= std::min(a.l.size(), std::size_t(to * a.rate)); start += w) {
            double sum = 0;
            for (std::size_t i = start; i < start + w; ++i) sum += 0.5 * (a.l[i] * a.l[i] + a.r[i] * a.r[i]);
            const double e = 10.0 * std::log10(sum / double(w) + 1.0e-12);
            if (!first) top = std::max(top, e - previous);
            previous = e;
            first = false;
        }
        return top;
    };
    // The change lands off the beat grid (between sixteenths at 120 bpm), so no note
    // starts with it. A whole number of 512-sample blocks.
    const double at = 512.0 * 283.0 / 48000.0, length = 6.0;
    const auto steady = world({})->render(length);
    // The detector itself: an unsmoothed 3 dB drop of the whole mix, and a 1.5 dB one,
    // are caught.
    const double threshold = -20.0;
    for (const float drop : { 0.708f, 0.841f }) {
        auto clicked = steady;
        for (auto i = std::size_t(at * clicked.rate); i < clicked.l.size(); ++i) {
            clicked.l[i] *= drop;
            clicked.r[i] *= drop;
        }
        const double x = suddenness(clicked, steady, at);
        s.metric("detector: an unsmoothed " + fmt(-toDb(drop)) + " dB step, suddenness", x, " dB");
        s.check(x > threshold, "the click detector catches an unsmoothed " + fmt(-toDb(drop)) + " dB step in the mix", fmt(x) + " dB");
    }
    struct Step {
        std::string id;
        float to;
        std::string name;
    };
    const std::vector<Step> steps {
        { "intensity", 1.0f, "Score Intensity 50 -> 100 %" }, { "p2_type", float(Tavern), "slot 2: Megacity -> Tavern" },
        { "p1_level", -24.0f, "Castle level 0 -> -24 dB" }, { "p1_mute", 1.0f, "Castle mute" }, { "p2_solo", 1.0f, "Megacity solo" },
        { "p1_character", 2.0f, "Castle Character 1 -> 3" }, { "p2_character", 2.0f, "Megacity Character 1 -> 3" },
        { "p3_character", 1.0f, "Forest Character 1 -> 2" }, { "p4_character", 2.0f, "Nebula Character 1 -> 3" },
        { "p1_activity", 1.0f, "Castle Activity -> 100 %" }, { "p2_tension", 1.0f, "Megacity Tension -> 100 %" },
        { "p1_tone", 1.0f, "Castle Tone -> bright" }, { "p1_distance", 1.0f, "Castle distance -> far" }, { "p1_pan", 1.0f, "Castle pan -> right" },
        { "p2_e1", 0.0f, "Megacity instrument 1 level -> 0" }, { "p2_e4_on", 0.0f, "Megacity drums off" },
        { "p1_e2_rev", 1.0f, "Castle instrument 2 reverb -> 100 %" }, { "p1_e2_revtype", 2.0f, "Castle instrument 2 reverb type" },
        { "p1_e2_dlytype", 3.0f, "Castle instrument 2 delay type" }, { "p1_e2_char", 5.0f, "Castle instrument 2 character -> Lo-Fi" },
        { "temperature", 1.0f, "Temperature -> scorching" }, { "danger", 1.0f, "Danger -> hostile" }, { "scale", 1.0f, "Scale -> colossal" },
        { "root", 7.0f, "Root D -> G" }, { "mood", 3.0f, "Mood Minor -> Phrygian" }, { "master", -12.0f, "Master Level -> -12 dB" },
        { "width", 0.0f, "Width -> mono" }, { "spacemix", 1.0f, "Space Mix -> 100 %" }, { "seed", 4321.0f, "World Seed" }, { "power", 0.0f, "Power off" } };
    int smooth = 0;
    std::string rough;
    for (const auto& step : steps) {
        auto changed = world({});
        auto a = changed->render(at);
        changed->set(step.id, step.to);
        const auto tail = changed->render(length - at);
        a.l.insert(a.l.end(), tail.l.begin(), tail.l.end());
        a.r.insert(a.r.end(), tail.r.begin(), tail.r.end());
        const auto target = world({ { step.id, step.to } })->render(length);
        const double click = suddenness(a, steady, at);
        const double jump = rise(a, at - 0.02, at + 0.5), jumpRef = std::max(rise(steady, at - 2.0, length), rise(target, at - 2.0, length));
        const bool okay = click < threshold && jump <= jumpRef + 3.0 && measure(a, 0.0).finite;
        s.metric(step.name + ": suddenness", click, " dB");
        s.metric(step.name + ": largest 20 ms rise against either state", jump - jumpRef, " dB");
        if (okay) ++smooth;
        else rough += step.name + " (" + fmt(click) + " dB sudden, " + fmt(jump - jumpRef) + " dB rise); ";
    }
    s.check(smooth == int(steps.size()), "a step in any of these controls is smooth: no click, no level jump beyond the world's own",
            std::to_string(smooth) + " of " + std::to_string(steps.size()) + ": " + rough);
}


// The world theme and the Place overlays, through the real editor and its pixels.
void testOverlays(Suite& s, const juce::String& snapshotPath) {
    s.section("overlays");
    const auto save = [&](const juce::Image& image, const juce::String& suffix) {
        if (snapshotPath.isEmpty()) return;
        juce::File file(snapshotPath.upToLastOccurrenceOf(".", false, false) + suffix + ".png");
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat().writeImageToStream(image, out);
        std::cout << "  snapshot written to " << file.getFullPathName() << "\n";
    };
    // The average colour of an area.
    const auto average = [](const juce::Image& image, juce::Rectangle<int> area) {
        float r = 0, g = 0, b = 0;
        int n = 0;
        for (int y = area.getY(); y < area.getBottom(); ++y)
            for (int x = area.getX(); x < area.getRight(); ++x, ++n) {
                const auto c = image.getPixelAt(x, y);
                r += c.getFloatRed();
                g += c.getFloatGreen();
                b += c.getFloatBlue();
            }
        return std::array<float, 3> { r / float(n), g / float(n), b / float(n) };
    };
    const auto distance = [](std::array<float, 3> a, std::array<float, 3> b) { return std::abs(a[0] - b[0]) + std::abs(a[1] - b[1]) + std::abs(a[2] - b[2]); };

    struct World {
        Host h;
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        WorldbuilderEditor* view = nullptr;
        explicit World(std::vector<int> places) {
            for (int slot = 0; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(NoPlace));   // the default world has a Castle
            h.render(0.3);
            editor.reset(h.p->createEditor());
            view = dynamic_cast<WorldbuilderEditor*>(editor.get());
            view->setOverlayMode(ui::OverlaysFull);   // Off by default
            add(places);
            view->settle(2.5f);
        }
        // Places go straight into the next slots (the editor's cards refuse Places that do
        // not sound yet; their overlays are tested all the same).
        void add(const std::vector<int>& places) {
            for (int type : places)
                for (int slot = 0; slot < placeSlotCount; ++slot)
                    if (placeType(h, slot) == NoPlace) {
                        h.set(slotId(slot, PlType), float(type));
                        break;
                    }
        }
        juce::Image snapshot() { return editor->createComponentSnapshot(editor->getLocalBounds()); }
    };
    std::vector<int> overlaid;   // every Place with an overlay
    for (int type = NoPlace + 1; type < PlaceTypeCount; ++type)
        if (ui::hasOverlay(type)) overlaid.push_back(type);
    const auto lower = [](int type) { return juce::String(placeKeys[std::size_t(type)]); };
    // An empty cell of the World Controls (inside a panel, no control) and a stretch of
    // the background (the header's right side, no text).
    const juce::Rectangle<int> panelSample(222, 530, 70, 60), backgroundSample(1020, 14, 50, 8);

    World plain({});
    plain.view->setOverlayMode(ui::OverlaysOff);
    const auto plainImage = plain.snapshot();
    save(plainImage, "-plain");
    const auto plainPanel = average(plainImage, panelSample), plainBackground = average(plainImage, backgroundSample);

    // Castle, Spaceport, the two together (for the theme), then every other overlay alone
    // and two combinations of the later ones.
    std::vector<std::pair<std::vector<int>, std::string>> cases { { { Castle }, "-castle" }, { { Spaceport }, "-spaceport" },
                                                                  { { Castle, Spaceport }, "-castle-spaceport" } };
    for (int type : overlaid)
        if (type != Castle && type != Spaceport) cases.push_back({ { type }, "-" + lower(type).toStdString() });
    if (ui::hasOverlay(Forest) && ui::hasOverlay(City)) cases.push_back({ { Forest, City }, "-forest-city" });
    if (ui::hasOverlay(Hive) && ui::hasOverlay(Temple) && ui::hasOverlay(Cavern)) cases.push_back({ { Factory, Hive, Temple, Cavern }, "-factory-hive-temple-cavern" });
    if (ui::hasOverlay(Ruins) && ui::hasOverlay(Biodome) && ui::hasOverlay(SeaShip) && ui::hasOverlay(Suburbs))
        cases.push_back({ { Ruins, Biodome, SeaShip, Suburbs }, "-ruins-biodome-seaship-suburbs" });
    if (ui::hasOverlay(Mall) && ui::hasOverlay(Beach) && ui::hasOverlay(Palace) && ui::hasOverlay(Tavern))
        cases.push_back({ { Mall, Beach, Palace, Tavern }, "-mall-beach-palace-tavern" });
    std::vector<std::array<float, 3>> panels(cases.size()), backgrounds(cases.size());
    for (std::size_t c = 0; c < cases.size(); ++c) {
        World w(cases[c].first);
        const auto full = w.snapshot();
        save(full, cases[c].second);
        auto& overlays = w.view->placeOverlays();
        for (int type : cases[c].first) s.check(overlays.strength(type) > 0.99f, std::string("overlay at full strength: ") + placeNames[std::size_t(type)]);
        w.view->setOverlayMode(ui::OverlaysOff);
        const auto off = w.snapshot();
        panels[c] = average(off, panelSample);
        backgrounds[c] = average(off, backgroundSample);
        // What the overlays drew: only on their drawable area, never on a control.
        int drawn = 0, outside = 0, onControls = 0, green = 0, neon = 0;
        const auto& area = overlays.drawableArea();
        std::vector<juce::Rectangle<int>> controls;
        juce::Rectangle<int> setting;   // the Overlays menu itself changes its text
        for (auto* child : w.editor->getChildren())
            if (child != &overlays && child->isVisible()) {
                if (auto* box = dynamic_cast<juce::ComboBox*>(child); box != nullptr && box->getText() == "Off") setting = box->getBounds();
                else controls.push_back(child->getBounds());
            }
        for (int y = 0; y < full.getHeight(); ++y)
            for (int x = 0; x < full.getWidth(); ++x) {
                if (setting.contains(x, y)) continue;
                const auto a = full.getPixelAt(x, y), b = off.getPixelAt(x, y);
                if (std::abs(a.getFloatRed() - b.getFloatRed()) + std::abs(a.getFloatGreen() - b.getFloatGreen()) + std::abs(a.getFloatBlue() - b.getFloatBlue()) < 0.02f) continue;
                ++drawn;
                if (!area.containsPoint({ x, y })) ++outside;
                for (const auto& r : controls) onControls += r.contains(x, y) ? 1 : 0;
                if (a.getFloatGreen() > a.getFloatRed() + 0.08f && a.getFloatGreen() > a.getFloatBlue() + 0.08f) ++green;
                if (a.getFloatBlue() > 0.55f && a.getFloatGreen() > 0.55f && a.getFloatRed() < a.getFloatBlue() - 0.15f) ++neon;
            }
        const std::string name = cases[c].second.substr(1);
        s.metric("overlay pixels (" + name + ")", drawn);
        s.check(drawn > 8000, "the overlay is clearly visible (" + name + ")", std::to_string(drawn) + " pixels");
        s.check(outside == 0, "the overlay draws only on the edges and empty space (" + name + ")", std::to_string(outside) + " pixels outside");
        s.check(onControls == 0, "the overlay never covers a control (" + name + ")", std::to_string(onControls) + " pixels");
        if (c < 3) {
            const bool castle = c != 1, spaceport = c != 0;
            if (castle) s.check(green > 1500, "Castle's grass and ivy show (" + name + ")", std::to_string(green) + " green pixels");
            if (spaceport) s.check(neon > 1500, "Spaceport's neon shows (" + name + ")", std::to_string(neon) + " neon pixels");
        }
    }
    // The theme: each Place clearly changes the panels and the background, and two
    // Places blend between them.
    for (std::size_t c = 0; c < 2; ++c) {
        const std::string name = cases[c].second.substr(1);
        s.metric("theme change, panel (" + name + ")", distance(panels[c], plainPanel));
        s.check(distance(panels[c], plainPanel) > 0.08f, "the theme clearly tints the panels (" + name + ")", fmt(distance(panels[c], plainPanel)));
        s.check(distance(backgrounds[c], plainBackground) > 0.08f, "the theme clearly changes the background (" + name + ")",
                fmt(distance(backgrounds[c], plainBackground)));
    }
    s.check(distance(panels[0], panels[1]) > 0.08f, "Castle and Spaceport look clearly different", fmt(distance(panels[0], panels[1])));
    s.check(distance(panels[2], panels[0]) > 0.02f && distance(panels[2], panels[1]) > 0.02f
                && distance(panels[2], panels[0]) + distance(panels[2], panels[1]) < distance(panels[0], panels[1]) * 1.3f,
            "Castle + Spaceport blends the two palettes");

    // Village, its detail panel (header art) and the Pattern Editor (instrument icons) with
    // the overlays, for looking at.
    {
        World w({ Village, Spaceport });
        save(w.snapshot(), "-village-spaceport");
        w.view->openDetail(0);
        w.view->settle(0.5f);
        const auto detailImage = w.snapshot();
        s.check(detailImage.isValid(), "the detail panel paints with overlays");
        save(detailImage, "-village-detail");
        w.view->openPatterns(1);
        w.view->settle(0.5f);
        const auto patternImage = w.snapshot();
        s.check(patternImage.isValid(), "the Pattern Editor paints with overlays");
        save(patternImage, "-spaceport-patterns");
    }
    {
        World w({ Forest, Factory });
        w.h.render(0.5);   // the audio thread writes the generated patterns
        w.view->openDetail(0);
        w.view->settle(0.5f);
        save(w.snapshot(), "-forest-detail");
        w.view->openPatterns(1, FactoryPlace::Pistons);
        w.view->settle(0.5f);
        const auto patternImage = w.snapshot();
        s.check(patternImage.isValid() && w.view->patternEditor().shown().pattern.count > 0, "a 5b Place's Pattern Editor shows its generated pattern",
                std::to_string(w.view->patternEditor().shown().pattern.count) + " notes");
        save(patternImage, "-factory-patterns");
    }
    // Cost, as the host draws it: the editor beneath what the overlays repaint, one paint
    // call per rectangle (as macOS hosts do), over three seconds. The share of a core is
    // reported, not gated: this container's software renderer says little about a Mac's.
    // How often each overlay moves, and how much it repaints, is checked.
    const auto cost = [&](std::vector<int> places, const std::string& name, int fps) {
        World w(places);
        auto& overlays = w.view->placeOverlays();
        juce::Image window(juce::Image::RGB, w.editor->getWidth(), w.editor->getHeight(), true);
        double busy = 0, pixels = 0;
        std::size_t most = 0;
        int frames = 0;
        for (int i = 0; i < 90; ++i) {
            w.view->settle(1.0f / 30.0f);
            const auto begin = juce::Time::getMillisecondCounterHiRes();
            for (auto r : overlays.lastRepaint()) {
                juce::Graphics g(window);
                g.reduceClipRegion(r);
                w.editor->paintEntireComponent(g, false);
                pixels += double(r.getWidth() * r.getHeight());
            }
            busy += juce::Time::getMillisecondCounterHiRes() - begin;
            frames += overlays.lastRepaint().empty() ? 0 : 1;
            most = std::max(most, overlays.lastRepaint().size());
        }
        const double share = busy / 3000.0 * 100.0;
        s.metric("overlays' share of one core, " + name + ", with the editor beneath (software renderer, report only)", share, " %");
        s.metric("overlay frames per second, " + name, frames / 3.0);
        s.metric("overlay frame: pixels repainted, " + name + " (of " + std::to_string(w.editor->getWidth() * w.editor->getHeight()) + ")",
                 pixels / std::max(1, frames));
        s.check(frames >= fps * 3 - 5 && frames <= fps * 3 + 2, "overlays move at " + std::to_string(fps) + " frames a second (" + name + ")",
                std::to_string(frames / 3.0));
        s.check(most > 0 && most <= 40, "an overlay frame repaints at most 40 small rectangles (" + name + ")", std::to_string(most));
    };
    cost({ Castle }, "Castle", 20);
    cost({ Spaceport }, "Spaceport", 20);
    cost({ Castle, Village, Spaceport }, "Castle + Village + Spaceport", 20);
    for (int type : overlaid)
        if (type != Castle && type != Village && type != Spaceport) {
            s.check(ui::PlaceOverlays::framesPerSecond(type, ui::OverlaysFull) <= 15, std::string("a later overlay moves at 15 frames a second or less: ") + placeNames[std::size_t(type)]);
            cost({ type }, placeNames[std::size_t(type)], 15);
        }
    if (ui::hasOverlay(Hive) && ui::hasOverlay(Temple) && ui::hasOverlay(Cavern)) cost({ Factory, Hive, Temple, Cavern }, "Factory + Alien Hive + Temple + Cavern", 15);
    if (ui::hasOverlay(Ruins) && ui::hasOverlay(Spaceship) && ui::hasOverlay(SeaShip) && ui::hasOverlay(Suburbs))
        cost({ Ruins, Spaceship, SeaShip, Suburbs }, "Ruins + Spaceship + Sea Ship + Suburbs", 15);
    if (ui::hasOverlay(Military) && ui::hasOverlay(Desert) && ui::hasOverlay(Palace) && ui::hasOverlay(Tavern))
        cost({ Military, Desert, Palace, Tavern }, "Military Camp + Desert + Palace + Tavern", 15);

    // Off by default: a new instance, and a project saved before overlays were off by
    // default (it stored Full), draw nothing and repaint nothing.
    {
        Host h;
        std::unique_ptr<juce::AudioProcessorEditor> editor(h.p->createEditor());
        auto* view = dynamic_cast<WorldbuilderEditor*>(editor.get());
        view->addPlace(Castle);
        view->settle(2.0f);
        auto& overlays = view->placeOverlays();
        s.check(overlays.mode() == ui::OverlaysOff && !overlays.showing() && overlays.lastRepaint().empty(), "overlays are Off by default and repaint nothing");
        auto older = h.p->parameters.copyState();
        older.setProperty("version", 2, nullptr);
        older.setProperty("overlays", int(ui::OverlaysFull), nullptr);
        juce::MemoryBlock saved;
        if (auto xml = older.createXml()) juce::AudioProcessor::copyXmlToBinary(*xml, saved);
        Host b;
        b.p->setStateInformation(saved.getData(), int(saved.getSize()));
        s.check(int(b.p->parameters.state.getProperty("overlays", -1)) == ui::OverlaysOff, "a project from before 5b (which stored Full) opens with overlays Off");
    }
    // Overlays grow in and fade out over about a second, and follow the Place's level.
    {
        World w({});
        auto& overlays = w.view->placeOverlays();
        const int slot = w.view->addPlace(Castle);
        w.view->settle(0.4f);
        const float early = overlays.strength(Castle);
        w.view->settle(1.2f);
        s.check(early > 0.02f && early < 0.9f && overlays.strength(Castle) > 0.99f, "an added Place's overlay grows in over about a second",
                fmt(early) + " then " + fmt(overlays.strength(Castle)));
        w.h.set(slotId(slot, PlLevel), -18.0f);
        w.view->settle(2.0f);
        const float quieter = overlays.strength(Castle);
        s.check(quieter > 0.1f && quieter < 0.8f, "the overlay follows the Place's level", fmt(quieter));
        w.h.set(slotId(slot, PlLevel), 0.0f);
        w.view->settle(2.0f);
        w.view->setOverlayMode(ui::OverlaysSubtle);
        s.check(overlays.strength(Castle) > 0.2f && overlays.strength(Castle) < 0.6f, "Subtle overlays are lighter", fmt(overlays.strength(Castle)));
        w.view->setOverlayMode(ui::OverlaysFull);
        w.h.set(slotId(slot, PlType), float(NoPlace));
        w.view->settle(0.4f);
        const float fading = overlays.strength(Castle);
        w.view->settle(1.2f);
        s.check(fading > 0.05f && overlays.strength(Castle) == 0.0f && !overlays.showing(), "a removed Place's overlay fades out", fmt(fading));
    }
    for (int type = NoPlace + 1; type < PlaceTypeCount; ++type)
        if (placeImplemented[std::size_t(type)]) s.check(ui::hasOverlay(type), std::string("every built Place has an overlay: ") + placeNames[std::size_t(type)]);
    // The setting is saved with the project.
    {
        Host a;
        a.p->parameters.state.setProperty("overlays", int(ui::OverlaysSubtle), nullptr);
        juce::MemoryBlock state;
        a.p->getStateInformation(state);
        Host b;
        b.p->setStateInformation(state.getData(), int(state.getSize()));
        s.check(int(b.p->parameters.state.getProperty("overlays", -1)) == ui::OverlaysSubtle, "the Overlays setting is saved and restored");
    }
}
} // namespace

//==============================================================================
// Score Intensity (Milestone 7): a calm bed at 0, the Places as set at 0.5, a climax
// at 1; monotonic in density and loudness, smoothed, in every Play Mode.

// Every note and hit all four slots have played so far.
uint32_t allEvents(const Host& h) {
    uint32_t n = 0;
    for (int slot = 0; slot < placeSlotCount; ++slot)
        if (const auto* place = h.p->engine.placeSlot(slot).place()) n += place->eventCount();
    return n;
}

// A world of up to four Places, each at its own defaults (as a Place card sets them),
// soloed (Score Intensity drives the Places, not the environment layers).
Host intensityWorld(const std::vector<int>& types, float intensity) {
    Host h;
    for (int slot = 0; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(NoPlace));
    for (std::size_t k = 0; k < types.size(); ++k) {
        const int slot = int(k);
        h.set(slotId(slot, PlType), float(types[k]));
        h.set(slotId(slot, PlSolo), 1.0f);
        for (int e = 0; e < placeElements; ++e)
            for (int f = 0; f < FxFieldCount; ++f) h.set(fxId(slot, e, f), instrumentFxDefault(types[k], e, f));
    }
    h.set("intensity", intensity);
    return h;
}

void testIntensity(Suite& s) {
    s.section("Score Intensity: monotonic density and loudness, smooth, every Play Mode");
    {
        const auto& spec = specs[std::size_t(PIntensity)];
        s.check(std::string(spec.id) == "intensity" && spec.def == 0.5f && spec.min == 0.0f && spec.max == 1.0f, "Score Intensity is a host parameter, 0-100 %, default 50 %");
        // At 50 % every Place is exactly as set.
        PlaceSlotControls c;
        c.type = Castle;
        WorldFrame w;
        w.c.fill(0.3f);
        w.intensity = 0.5f;
        const auto a = PlaceSlot::settingsFor(w, c);
        w.intensity = 0.0f;
        const auto calm = PlaceSlot::settingsFor(w, c);
        w.intensity = 1.0f;
        const auto climax = PlaceSlot::settingsFor(w, c);
        s.check(intensityDb(Castle, 0.5f) == 0.0f && calm.activity < a.activity && a.activity < climax.activity && calm.tension < a.tension
                    && a.tension < climax.tension && calm.tone < a.tone && a.tone < climax.tone && intensityDb(Castle, 0.0f) < 0.0f && intensityDb(Castle, 1.0f) > 0.0f,
                "50 % leaves a Place as set; 0 % calms it, 100 % drives it (activity, tension, tone, level)");
    }
    // Monotonic: more notes and louder at every step, for each kind of Place and a full world.
    const std::vector<std::pair<std::vector<int>, std::string>> worlds { { { Castle }, "Castle" }, { { Tavern }, "Tavern" }, { { Temple }, "Temple" },
                                                                         { { Megacity }, "Cyberpunk Megacity" }, { { Nebula }, "Nebula" },
                                                                         { { Bridge, Castle, Forest, Nebula }, "four Places" } };
    for (const auto& [types, name] : worlds) {
        std::vector<double> rms;
        std::vector<uint32_t> notes;
        std::string text;
        for (float x : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f }) {
            auto h = intensityWorld(types, x);
            const auto st = measure(h.render(24.0), 4.0);
            rms.push_back(st.rmsDb);
            notes.push_back(allEvents(h));
            text += fmt(st.rmsDb) + " dB/" + std::to_string(notes.back()) + (x < 1.0f ? ", " : "");
            s.check(st.finite && st.peak < 0.95, name + ": finite and within the limiter at " + fmt(x * 100.0) + " %", fmt(toDb(st.peak)) + " dBFS peak");
        }
        bool louder = true, denser = true;
        for (std::size_t k = 1; k < rms.size(); ++k) {
            louder = louder && rms[k] > rms[k - 1] + 0.3;
            denser = denser && notes[k] >= notes[k - 1];
        }
        s.check(louder, name + ": louder at every step of Score Intensity", text);
        s.check(denser && double(notes.back()) > 1.3 * double(notes.front()), name + ": busier with Score Intensity (never fewer notes)", text);
        s.check(rms.back() - rms.front() > 5.0 && rms.back() - rms[2] < 6.0, name + ": a calm bed to a climax, the climax within the loudness rules",
                fmt(rms.front()) + " -> " + fmt(rms[2]) + " -> " + fmt(rms.back()) + " dB");
        s.metric(name + " Score Intensity range", rms.back() - rms.front(), " dB");
    }
    // A jump from 0 % to 100 % glides: no level spike beyond what the climax itself has.
    {
        const std::vector<int> types { Megacity, Castle, Temple, Bridge };
        auto steady = intensityWorld(types, 1.0f);
        const auto st = steady.render(12.0);
        auto jump = intensityWorld(types, 0.0f);
        jump.render(4.0);
        jump.set("intensity", 1.0f);
        const auto after = jump.render(8.0);
        const auto steadyEnv = envelopeDb(st, 0.2, 4.0), jumpEnv = envelopeDb(after, 0.2);
        double steadyTop = -240, jumpTop = -240, rise = 0, steadyRise = 0;
        for (double e : steadyEnv) steadyTop = std::max(steadyTop, e);
        for (double e : jumpEnv) jumpTop = std::max(jumpTop, e);
        const auto fine = envelopeDb(after, 0.02), steadyFine = envelopeDb(st, 0.02, 4.0);
        for (std::size_t k = 1; k < fine.size(); ++k) rise = std::max(rise, fine[k] - fine[k - 1]);
        for (std::size_t k = 1; k < steadyFine.size(); ++k) steadyRise = std::max(steadyRise, fine.size() > k ? steadyFine[k] - steadyFine[k - 1] : 0.0);
        s.check(jumpTop < steadyTop + 1.5, "a jump to 100 % never overshoots the climax's own level", fmt(jumpTop) + " vs " + fmt(steadyTop) + " dB (200 ms windows)");
        s.check(rise < steadyRise + 3.0, "a jump to 100 % adds no sudden steps (20 ms windows)", fmt(rise) + " dB vs " + fmt(steadyRise) + " dB in the climax itself");
        // The level arrives over about a second, not at once.
        const double first = jumpEnv.empty() ? 0.0 : jumpEnv.front();
        s.check(!jumpEnv.empty() && first < steadyTop - 1.0, "the jump is smoothed (the first 200 ms are still on the way up)", fmt(first) + " dB");
    }
    // Automation: a ramp from 0 % to 100 % over 16 s rises steadily.
    {
        auto h = intensityWorld({ Castle, Tavern }, 0.0f);
        h.render(2.0);
        Audio a;
        a.rate = h.rate;
        const int blocks = int(16.0 * h.rate / double(h.block));
        for (int b = 0; b < blocks; ++b) {
            h.set("intensity", float(b) / float(blocks - 1));
            h.processOne(h.block);
            a.l.insert(a.l.end(), h.buffer.getReadPointer(0), h.buffer.getReadPointer(0) + h.block);
            a.r.insert(a.r.end(), h.buffer.getReadPointer(1), h.buffer.getReadPointer(1) + h.block);
        }
        const auto env = envelopeDb(a, 4.0);
        bool rising = env.size() == 4;
        for (std::size_t k = 1; k < env.size(); ++k) rising = rising && env[k] > env[k - 1];
        std::string text;
        for (double e : env) text += fmt(e) + " ";
        s.check(rising, "automating Score Intensity up raises the level steadily (4 s windows)", text);
        const auto fine = envelopeDb(a, 0.02);
        double worst = 0;
        for (std::size_t k = 1; k < fine.size(); ++k) worst = std::max(worst, std::abs(fine[k] - fine[k - 1]));
        s.metric("Score Intensity ramp: largest 20 ms step", worst, " dB");
    }
    // Transport mode: the same response while the host plays.
    {
        const auto level = [](float x) {
            auto h = intensityWorld({ Castle }, x);
            h.set("playmode", float(PlayTransport));
            h.playHead.playing = true;
            return measure(h.render(16.0), 4.0).rmsDb;
        };
        const double calm = level(0.0f), climax = level(1.0f);
        s.check(climax > calm + 5.0, "Score Intensity works in Transport mode", fmt(calm) + " -> " + fmt(climax) + " dB");
    }
    // State: saved and restored.
    {
        Host a;
        a.set("intensity", 0.8f);
        juce::MemoryBlock block;
        a.p->getStateInformation(block);
        Host b;
        b.p->setStateInformation(block.getData(), int(block.getSize()));
        s.check(std::abs(b.get("intensity") - 0.8f) < 1.0e-4f, "Score Intensity is saved with the project", fmt(b.get("intensity")));
    }
}

//==============================================================================
// Locations and Journeys.

// A Place in a slot as a card adds it: its instruments start from their own effects.
void setPlace(Host& h, int slot, int type, int character = 0) {
    h.set(slotId(slot, PlType), float(type));
    h.set(slotId(slot, PlCharacter), float(character));
    if (type != NoPlace)
        for (int e = 0; e < placeElements; ++e)
            for (int f = 0; f < FxFieldCount; ++f) h.set(fxId(slot, e, f), instrumentFxDefault(type, e, f));
}

// Two very different worlds. A: a frozen, magical, ancient valley (Castle, Tavern, -,
// Forest) in D minor. B: a scorching, stormy, futuristic, hostile one (Castle in another
// Character, Megacity, Nebula, -) in A Phrygian, with another seed. Slot 1 keeps its
// Place (it morphs), slot 2 changes Place (recede, then approach), slot 3 gains one
// (approach), slot 4 loses one (recede).
void worldA(Host& h) {
    const std::vector<std::pair<const char*, float>> values { { "temperature", 0.08f }, { "weather", 0.15f }, { "wetness", 0.6f }, { "magic", 0.7f },
        { "scale", 0.75f }, { "era", 0.1f }, { "danger", 0.05f }, { "activity", 0.3f }, { "root", 2 }, { "mood", 0 }, { "seed", 7 },
        { "intensity", 0.35f }, { "water_level", -3.0f }, { "thunder_mute", 1 }, { "fire_mute", 0 }, { "air_level", 0.0f }, { "spacemix", 0.5f } };
    for (const auto& [id, v] : values) h.set(id, v);
    setPlace(h, 0, Castle, 1);
    setPlace(h, 1, Tavern, 0);
    setPlace(h, 2, NoPlace);
    setPlace(h, 3, Forest, 0);
    h.set("p1_distance", 0.2f);
    h.set("p1_tension", 0.1f);
    h.p->patterns.syncTypes(h.p->slotTypes());
    h.p->patterns.setElement(0, Castle, CastlePlace::Bells, editedElement({ makeNote(0, 0, 4), makeNote(8, 2, 4, 90), makeNote(12, 4, 2, 70) }, 0.0f));
}
void worldB(Host& h) {
    const std::vector<std::pair<const char*, float>> values { { "temperature", 0.95f }, { "weather", 0.9f }, { "wetness", 0.1f }, { "magic", 0.1f },
        { "scale", 0.3f }, { "era", 0.95f }, { "danger", 0.8f }, { "activity", 0.8f }, { "root", 9 }, { "mood", 3 }, { "seed", 4242 },
        { "intensity", 0.8f }, { "water_level", -12.0f }, { "thunder_mute", 0 }, { "fire_mute", 1 }, { "air_level", -6.0f }, { "spacemix", 0.25f } };
    for (const auto& [id, v] : values) h.set(id, v);
    setPlace(h, 0, Castle, 2);
    setPlace(h, 1, Megacity, 1);
    setPlace(h, 2, Nebula, 0);
    setPlace(h, 3, NoPlace);
    h.set("p1_distance", 0.6f);
    h.set("p1_tension", 0.9f);
    h.p->patterns.syncTypes(h.p->slotTypes());
    h.p->patterns.setElement(1, Megacity, 0, editedElement({ makeNote(0, 0, 2), makeNote(3, 0, 1, 80), makeNote(6, 4, 2, 110) }, 0.0f));
}

// A host with A saved in Location 1 and B in Location 2, standing at A (arrived).
std::unique_ptr<Host> locationHost(bool transport = false) {
    auto h = std::make_unique<Host>(48000, 512);
    h->playHead.bpm = 120;
    if (transport) h->set("playmode", float(PlayTransport));
    // The plugin has played (its first block takes the Destination as already settled).
    h->render(0.1);
    worldA(*h);
    h->p->saveLocation(0);
    worldB(*h);
    h->p->saveLocation(1);
    h->set("traveltime", 0.5f);
    h->set("destination", 1);
    h->render(1.0);
    h->p->serviceLocations();
    h->render(0.1);
    return h;
}

// Every value a Location holds, compared with the host's parameters.
int locatedMismatches(const Host& h, const WorldValues& values, std::string& first) {
    int bad = 0;
    for (int p = 0; p < PCount; ++p)
        if (moveOf(p) != Move::Live && std::abs(h.get(specs[std::size_t(p)].id) - values[std::size_t(p)]) > 1.0e-5f * std::max(1.0f, std::abs(values[std::size_t(p)])))
            if (bad++ == 0) first = specs[std::size_t(p)].id;
    return bad;
}

// The largest rise and the largest fall of the 20 ms level in [from, to) s.
std::pair<double, double> levelSteps(const Audio& a, double from, double to) {
    const auto env = envelopeDb(a, 0.02, from);
    const auto count = std::min(env.size(), std::size_t((to - from) / 0.02));
    double up = 0, down = 0;
    for (std::size_t k = 1; k < count; ++k) {
        up = std::max(up, env[k] - env[k - 1]);
        down = std::max(down, env[k - 1] - env[k]);
    }
    return { up, down };
}

// A click where two renders part: the high-frequency energy (second difference) of
// their difference over the first 3 ms after `at` seconds, against that of the first
// render (dB). A smoothed change grows from nothing; a jump is as bright as the music.
double suddenness(const Audio& changed, const Audio& base, double at) {
    double diff = 0, ref = 0;
    const auto begin = std::max<std::size_t>(2, std::size_t(at * base.rate)), end = begin + std::size_t(0.003 * base.rate);
    for (std::size_t i = begin; i < end && i < base.l.size() && i < changed.l.size(); ++i)
        for (int ch = 0; ch < 2; ++ch) {
            const auto& x = ch == 0 ? base.l : base.r;
            const auto& y = ch == 0 ? changed.l : changed.r;
            const double d2 = (y[i] - x[i]) - 2.0 * (y[i - 1] - x[i - 1]) + (y[i - 2] - x[i - 2]);
            const double x2 = x[i] - 2.0 * x[i - 1] + x[i - 2];
            diff += d2 * d2;
            ref += x2 * x2;
        }
    return 10.0 * std::log10((diff + 1.0e-30) / (ref + 1.0e-30));
}

// The quietest 100 ms in [from, to) s (dBFS).
double quietest(const Audio& a, double from, double to) {
    const auto env = envelopeDb(a, 0.1, from);
    double low = 240;
    for (std::size_t k = 0; k < env.size() && double(k) * 0.1 + from < to; ++k) low = std::min(low, env[k]);
    return low;
}

void testLocations(Suite& s) {
    s.section("Locations and Journeys");
    {
        WorldbuilderProcessor p;
        std::string missing;
        for (const char* id : { "destination", "traveltime", "travelsync", "journeymode", "journeyfrom", "journeyto", "journey", "pathmode", "pathleg", "pathend" })
            if (auto* parameter = p.parameters.getParameter(id); parameter == nullptr || !parameter->isAutomatable()) missing += std::string(id) + " ";
        s.check(missing.empty(), "Destination, Travel Time, Travel Sync, Journey Mode, From, To, Journey and the Journey Path are automatable host parameters", missing);
        s.check(specs[PTravelTime].min == 0.5f && specs[PTravelTime].max == 60.0f && specs[PDestination].def == 0.0f,
                "Travel Time runs from 0.5 s to 60 s; a new plugin has no Destination");
        const bool live = moveOf(PPower) == Move::Live && moveOf(PMaster) == Move::Live && moveOf(PQuality) == Move::Live && moveOf(PPlayMode) == Move::Live
                          && moveOf(layerSolo(Air)) == Move::Live && moveOf(placeParam(2, PlSolo)) == Move::Live && moveOf(PDestination) == Move::Live
                          && moveOf(PJourney) == Move::Live;
        const bool held = moveOf(PTemperature) == Move::Glide && moveOf(PIntensity) == Move::Glide && moveOf(layerLevel(Water)) == Move::Gain
                          && moveOf(layerMute(Water)) == Move::Paired && moveOf(PRoot) == Move::Switch && moveOf(PSeed) == Move::Live
                          && moveOf(placeParam(1, PlType)) == Move::Switch && moveOf(placeParam(1, PlCharacter)) == Move::Switch
                          && moveOf(placeParam(1, PlLevel)) == Move::Gain && moveOf(placeParam(1, PlElement + 3)) == Move::Gain
                          && moveOf(placeParam(1, PlElementOn + 3)) == Move::Paired && moveOf(placeParam(3, fxField(2, FxReverb))) == Move::Glide
                          && moveOf(placeParam(3, fxField(2, FxReverbType))) == Move::Switch;
        s.check(live, "Power, Master, Quality, Play Mode, the World Seed, solos and the Location controls stay live (not part of a Location)");
        s.check(held, "a Location holds the World Controls, Score Intensity, levels, Root, Mood, the Places, their seeds, Characters and instrument effects");
    }

    // Saving, and arriving: the parameters and patterns become the Location's.
    auto h = locationHost();
    const auto a = h->p->locations.get(0), b = h->p->locations.get(1);
    {
        std::string first;
        const int bad = locatedMismatches(*h, a.values, first);
        s.check(a.used && b.used && !h->p->locations.used(2), "saving fills a Location slot (the others stay empty)");
        s.check(bad == 0 && !h->p->travel.active(), "after arriving at Location 1 the parameters are its world and the hold is released",
                std::to_string(bad) + " differ, first " + first);
        s.check(h->p->patterns.element(0, Castle, CastlePlace::Bells).pattern.sameNotes(a.patterns[0].element[CastlePlace::Bells].pattern)
                    && h->p->patterns.element(0, Castle, CastlePlace::Bells).controls.edited,
                "arriving restores the Location's edited patterns");
        s.check(juce::roundToInt(h->get("p2_type")) == Tavern && juce::roundToInt(h->get("root")) == 2 && juce::roundToInt(h->get("seed")) == 4242,
                "the Places and Root are Location 1's again (the World Seed stays as set)");
    }

    // The journey from A to B (6 s), against both worlds standing still.
    const double travel = 6.0;
    const auto steadyA = h->render(4.0);
    h->set("traveltime", float(travel));
    h->set("destination", 2);
    Audio way;
    way.rate = h->rate;
    std::vector<int> slot2, slot3, slot4;
    std::vector<double> slot2Peak;
    for (int piece = 0; piece < int((travel + 2.0) / 0.25); ++piece) {
        const auto part = h->render(0.25);
        way.l.insert(way.l.end(), part.l.begin(), part.l.end());
        way.r.insert(way.r.end(), part.r.begin(), part.r.end());
        slot2.push_back(h->p->engine.placeSlot(1).playingType());
        slot3.push_back(h->p->engine.placeSlot(2).playingType());
        slot4.push_back(h->p->engine.placeSlot(3).playingType());
        slot2Peak.push_back(toDb(h->p->engine.placeMeter[1].load()));
    }
    const bool holding = h->p->travel.currentMode() == Travel::Holding;
    h->p->serviceLocations();
    h->render(0.1);
    const auto steadyB = h->render(4.0);
    // A stretch staying at A, `skip` seconds after arriving there (the music's own
    // attacks at the same moments as a journey starting then).
    const auto stayed = [](double skip, double length) {
        auto x = locationHost();
        if (skip > 0) x->render(skip);
        return x->render(length);
    };
    {
        std::string first;
        const int bad = locatedMismatches(*h, b.values, first);
        s.check(holding && bad == 0 && !h->p->travel.active(), "the journey arrives: Location 2's world is written into the parameters, then they take over",
                std::to_string(bad) + " differ, first " + first);
        s.check(h->p->patterns.element(1, Megacity, 0).pattern.sameNotes(b.patterns[1].element[0].pattern), "and its patterns");
    }
    {
        const auto [upA, downA] = levelSteps(steadyA, 0.0, 4.0);
        const auto [upB, downB] = levelSteps(steadyB, 0.0, 4.0);
        const auto [up, down] = levelSteps(way, 0.0, travel + 0.5);
        s.metric("journey A -> B: largest 20 ms rise / fall", up, " dB");
        s.metric("  fall", down, " dB");
        s.metric("  the worlds' own: rise", std::max(upA, upB), " dB");
        s.metric("  fall", std::max(downA, downB), " dB");
        s.check(up <= std::max(upA, upB) + 3.0 && down <= std::max(downA, downB) + 3.0, "no level jumps on the way beyond the worlds' own (20 ms windows)",
                fmt(up) + " / " + fmt(down) + " dB vs " + fmt(std::max(upA, upB)) + " / " + fmt(std::max(downA, downB)) + " dB");
        const double low = quietest(way, 0.0, travel + 0.5), lowOwn = std::min(quietest(steadyA, 0.0, 4.0), quietest(steadyB, 0.0, 4.0));
        s.metric("journey A -> B: quietest 100 ms", low, " dBFS");
        s.check(low > -50.0 && low > lowOwn - 12.0, "no silence on the way", fmt(low) + " dBFS (the worlds' quietest " + fmt(lowOwn) + ")");
        s.check(measure(way, 0.0).finite && measure(way, 0.0).peak < 0.95, "finite and below the ceiling all the way");
    }
    {
        // Pieces of 0.25 s: the midpoint (eased) is at 3 s, piece 12.
        const bool tavernFirst = slot2[0] == Tavern && slot2[9] == Tavern, megacityLater = slot2[14] == Megacity && slot2.back() == Megacity;
        s.check(tavernFirst && megacityLater, "a slot whose Place changes plays the old Place until the far point, then the new one",
                std::to_string(slot2[9]) + " / " + std::to_string(slot2[14]));
        s.check(slot2Peak[10] < slot2Peak[0] - 10.0 && slot2Peak.back() > slot2Peak[13] + 6.0,
                "the old Place recedes (quieter by the far point), the new one approaches", fmt(slot2Peak[0]) + " -> " + fmt(slot2Peak[10]) + " dB; "
                + fmt(slot2Peak[13]) + " -> " + fmt(slot2Peak.back()) + " dB");
        s.check(slot3[0] == Nebula && slot4[0] == Forest && slot4[int(travel / 0.25) - 2] == Forest && slot4.back() == NoPlace,
                "a Place arriving in an empty slot approaches from the start; one leaving recedes until the arrival");
    }

    // The same journey again gives the same sound.
    {
        auto x = locationHost(), y = locationHost();
        for (auto* host : { x.get(), y.get() }) {
            host->set("traveltime", 3.0f);
            host->set("destination", 2);
        }
        const auto ax = x->render(4.0), ay = y->render(4.0);
        s.check(ax.l == ay.l && ax.r == ay.r, "a journey is deterministic (the same world, the same journey, the same sound)");
    }

    // Interrupted: back to A from 40 % of the way to B.
    {
        auto x = locationHost();
        x->set("traveltime", 6.0f);
        x->set("destination", 2);
        auto turned = x->render(2.4);
        x->set("destination", 1);
        const auto back = x->render(7.0);
        turned.l.insert(turned.l.end(), back.l.begin(), back.l.end());
        turned.r.insert(turned.r.end(), back.r.begin(), back.r.end());
        x->p->serviceLocations();
        x->render(0.1);
        std::string first;
        const int bad = locatedMismatches(*x, a.values, first);
        const auto [up, down] = levelSteps(turned, 0.0, 9.0);
        const auto [upA, downA] = levelSteps(steadyA, 0.0, 4.0);
        const auto [upB, downB] = levelSteps(steadyB, 0.0, 4.0);
        s.check(up <= std::max(upA, upB) + 3.0 && down <= std::max(downA, downB) + 3.0, "turning back halfway is smooth too", fmt(up) + " / " + fmt(down) + " dB");
        s.check(bad == 0, "and arrives back at Location 1", std::to_string(bad) + " differ, first " + first);
    }

    // What changes at the far point (seeds, Root, Mood, Characters, effect types) changes
    // without a click: a journey to a Location that differs from A only there is the same
    // as staying at A up to that moment, so the click detector of the automation test
    // can look at the very moment they part.
    {
        const double length = 2.0;
        const auto stay = [&] {
            auto x = locationHost();
            x->set("traveltime", float(length));
            return x->render(length + 1.0);
        }();
        // The detector: an unsmoothed 3 dB step of the mix reads above the limit.
        const double limit = -20.0;
        {
            auto stepped = stay;
            for (auto i = std::size_t(1.0 * stay.rate); i < stepped.l.size(); ++i) {
                stepped.l[i] *= 0.708f;
                stepped.r[i] *= 0.708f;
            }
            s.check(suddenness(stepped, stay, 1.0) > limit, "the click detector catches an unsmoothed 3 dB step in this world", fmt(suddenness(stepped, stay, 1.0)) + " dB");
        }
        const std::vector<std::pair<std::vector<std::pair<const char*, float>>, std::string>> changes {
            { { { "root", 7 } }, "Root" }, { { { "mood", 4 } }, "Mood" },
            { { { "p1_character", 0 } }, "Castle Character" }, { { { "p2_character", 2 } }, "Tavern Character" }, { { { "p1_seed", 500 } }, "Place seed" },
            { { { "p1_e2_revtype", 3 }, { "p2_e1_dlytype", 2 }, { "p4_e3_char", 4 } }, "instrument effect types" }, { { { "p2_sync", 0 } }, "Sync" } };
        std::string rough;
        int smooth = 0;
        for (const auto& [settings, name] : changes) {
            auto x = locationHost();
            for (const auto& [id, v] : settings) x->set(id, v);
            x->p->patterns.syncTypes(x->p->slotTypes());
            x->p->saveLocation(2);
            x->set("destination", 1);   // back to A as saved (the change is only in Location 3)
            x->render(0.0);
            x->p->serviceLocations();
            auto y = locationHost();
            y->p->locations.save(2, x->p->locations.get(2).values, x->p->locations.get(2).patterns);
            y->set("traveltime", float(length));
            y->set("destination", 3);
            const auto way = y->render(length + 1.0);
            std::size_t parted = 0;
            while (parted < way.l.size() && way.l[parted] == stay.l[parted] && way.r[parted] == stay.r[parted]) ++parted;
            const double at = double(parted) / way.rate, click = suddenness(way, stay, at);
            s.metric("far point: " + name + " changes at", at, " s");
            s.metric("  suddenness", click, " dB");
            if (at > 0.45 * length && click < limit) ++smooth;
            else rough += name + " (" + fmt(at) + " s, " + fmt(click) + " dB); ";
        }
        s.check(smooth == int(changes.size()), "Place seeds, Root, Mood, Characters, effect types and Sync change at the far point without a click",
                std::to_string(smooth) + " of " + std::to_string(changes.size()) + ": " + rough);
    }

    // An empty Location slot goes nowhere; Travel Sync counts bars of the host tempo.
    {
        auto x = locationHost();
        const auto before = x->get("temperature");
        x->set("destination", 5);
        x->render(1.0);
        s.check(!x->p->travel.active() && x->get("temperature") == before, "a Destination with no Location saved there changes nothing");
        x->set("travelsync", 1);   // one bar: 2 s at 120 bpm
        x->set("destination", 2);
        x->render(1.9);
        const bool still = x->p->travel.currentMode() == Travel::Travelling;
        x->render(0.2);
        s.check(still && x->p->travel.currentMode() == Travel::Holding, "Travel Sync: a one-bar journey at 120 bpm takes 2 s");
    }

    // Transport mode: a start or locate is where the world should be, at once, and
    // playing the same automation again gives the same sound.
    {
        auto x = locationHost(true);
        x->playHead.playing = true;
        x->playHead.position = 0;
        x->set("traveltime", 2.0f);
        const auto run = [&]() {
            auto first = x->render(2.0);
            x->set("destination", 2);
            const auto second = x->render(3.0);
            first.l.insert(first.l.end(), second.l.begin(), second.l.end());
            first.r.insert(first.r.end(), second.r.begin(), second.r.end());
            x->p->serviceLocations();
            return first;
        };
        const auto one = run();
        x->render(0.5);
        // Locate back to bar 1, where the Destination automation reads Location 1.
        x->playHead.position = 0;
        x->set("destination", 1);
        const auto two = run();
        s.check(one.l == two.l && one.r == two.r, "Transport mode: a locate jumps to the Destination's Location, and the same automation plays the same journey");
        // A start at Location 1 by Destination equals a world simply set to it.
        auto y = std::make_unique<Host>(48000, 512);
        y->playHead.bpm = 120;
        y->set("playmode", float(PlayTransport));
        worldA(*y);
        y->set("seed", 4242);   // the World Seed is not part of a Location: x kept B's
        y->set("traveltime", 2.0f);
        y->set("destination", 1);
        y->playHead.playing = true;
        y->playHead.position = 0;
        const auto direct = y->render(2.0);
        const bool same = std::equal(direct.l.begin(), direct.l.end(), one.l.begin()) && std::equal(direct.r.begin(), direct.r.end(), one.r.begin());
        s.check(same, "the world a locate jumps to is exactly the Location as saved");
    }

    // Journey Mode: a drawn walk from A to B, then kept where it stands.
    {
        auto x = locationHost();
        x->set("journeyfrom", 0);
        x->set("journeyto", 1);
        x->set("journey", 0.0f);
        x->set("traveltime", 1.0f);
        x->set("journeymode", 1);
        x->render(1.5);
        s.check(x->p->travel.currentMode() == Travel::Journeying, "Journey Mode morphs between From and To");
        Audio walk;
        walk.rate = x->rate;
        const int blocks = int(10.0 * x->rate / double(x->block));
        for (int k = 0; k < blocks; ++k) {
            x->set("journey", float(k) / float(blocks - 1));
            x->processOne(x->block);
            walk.l.insert(walk.l.end(), x->buffer.getReadPointer(0), x->buffer.getReadPointer(0) + x->block);
            walk.r.insert(walk.r.end(), x->buffer.getReadPointer(1), x->buffer.getReadPointer(1) + x->block);
        }
        const auto [up, down] = levelSteps(walk, 0.0, 10.0);
        const auto [upA, downA] = levelSteps(steadyA, 0.0, 4.0);
        const auto [upB, downB] = levelSteps(steadyB, 0.0, 4.0);
        s.metric("Journey 0 -> 1 over 10 s: largest 20 ms rise / fall", up, " dB");
        s.metric("  fall", down, " dB");
        s.check(up <= std::max(upA, upB) + 3.0 && down <= std::max(downA, downB) + 3.0 && quietest(walk, 0.0, 10.0) > -50.0,
                "automating the Journey from 0 to 1 is smooth (no jumps or silence)", fmt(up) + " / " + fmt(down) + " dB");
        x->set("journeymode", 0);
        x->render(0.2);
        x->p->serviceLocations();
        x->render(0.1);
        std::string first;
        const int bad = locatedMismatches(*x, b.values, first);
        s.check(bad == 0 && !x->p->travel.active(), "switching Journey Mode off keeps the world where the Journey stood (at To: Location 2)",
                std::to_string(bad) + " differ, first " + first);
    }

    // Journey Path: the saved Locations in order, a leg a bar (2 s at 120 bpm), the
    // journey (1 s) arriving on each downbeat; following the song position.
    {
        auto x = locationHost(true);
        x->set("pathleg", 0);
        x->set("pathend", 1);
        x->set("traveltime", 1.0f);
        x->set("pathmode", 1);
        x->playHead.playing = true;
        x->playHead.position = 0;
        Audio walk;
        walk.rate = x->rate;
        std::vector<std::array<float, 3>> seen;   // from, to, progress every 0.25 s
        for (int piece = 0; piece < 32; ++piece) {
            const auto part = x->render(0.25);
            walk.l.insert(walk.l.end(), part.l.begin(), part.l.end());
            walk.r.insert(walk.r.end(), part.r.begin(), part.r.end());
            seen.push_back({ float(x->p->travel.uiFrom.load()), float(x->p->travel.uiTo.load()), x->p->travel.uiProgress.load() });
        }
        // At 0.5 s: 1 -> 2 waiting; at 1.75 s on the way; at 2.5 s (the next leg) 2 -> 1 waiting (Loop).
        const bool legs = seen[1][0] == 0 && seen[1][1] == 1 && seen[1][2] == 0.0f && seen[6][2] > 0.3f && seen[6][2] < 1.0f
                          && seen[9][0] == 1 && seen[9][1] == 0 && seen[9][2] == 0.0f;
        s.check(x->p->travel.currentMode() == Travel::Journeying && legs, "the Journey Path visits the saved Locations in order, a leg a bar, looping",
                fmt(seen[1][0]) + ">" + fmt(seen[1][1]) + " " + fmt(seen[6][2]) + "; " + fmt(seen[9][0]) + ">" + fmt(seen[9][1]));
        // (From 0.2 s: playback starts from silence.)
        const auto [up, down] = levelSteps(walk, 0.2, 8.0);
        const auto [upA, downA] = levelSteps(steadyA, 0.0, 4.0);
        const auto [upB, downB] = levelSteps(steadyB, 0.0, 4.0);
        s.metric("Journey Path: largest 20 ms rise / fall", up, " dB");
        s.metric("  fall", down, " dB");
        s.check(up <= std::max(upA, upB) + 3.0 && down <= std::max(downA, downB) + 3.0 && quietest(walk, 0.2, 8.0) > -50.0,
                "the Path's journeys are smooth (no jumps or silence)", fmt(up) + " / " + fmt(down) + " dB");
        // The same song position, the same Path (two replays after a locate, as a bounce
        // after playback; the very first playback can differ slightly, see the DEVLOG).
        x->playHead.position = 0;
        const auto again = x->render(8.0);
        x->playHead.position = 0;
        const auto third = x->render(8.0);
        s.check(again.l == third.l && again.r == third.r, "the Journey Path follows the song: playing from the same position sounds the same");
        // One-way: it stays at the last Location; switching it off keeps it there.
        x->set("pathend", 0);
        x->playHead.position = 48000 * 10;
        x->render(1.0);
        const bool atEnd = x->p->travel.uiFrom.load() == 1 && x->p->travel.uiTo.load() == 1;
        x->set("pathmode", 0);
        x->render(0.2);
        x->p->serviceLocations();
        x->render(0.1);
        std::string first;
        const int bad = locatedMismatches(*x, b.values, first);
        s.check(atEnd && bad == 0 && !x->p->travel.active(), "One-way stops at the last Location; switching the Path off keeps the world there",
                std::to_string(bad) + " differ, first " + first);
    }

    // State: the Locations, their names and colours, and where the world stands.
    {
        h->p->locations.rename(0, "Frozen Gate");
        h->p->locations.setColour(1, 5);
        juce::MemoryBlock state;
        h->p->getStateInformation(state);
        Host y;
        y.p->setStateInformation(state.getData(), int(state.getSize()));
        const auto ya = y.p->locations.get(0), yb = y.p->locations.get(1);
        int differ = 0;
        for (int p = 0; p < PCount; ++p)
            if (moveOf(p) != Move::Live) differ += (ya.values[std::size_t(p)] != a.values[std::size_t(p)]) + (yb.values[std::size_t(p)] != b.values[std::size_t(p)]);
        const bool patternsBack = ya.patterns[0].element[CastlePlace::Bells].pattern.sameNotes(a.patterns[0].element[CastlePlace::Bells].pattern)
                                  && yb.patterns[1].element[0].pattern.sameNotes(b.patterns[1].element[0].pattern) && yb.patterns[1].element[0].controls.edited;
        s.check(ya.used && yb.used && !y.p->locations.used(2) && differ == 0 && patternsBack, "Locations survive a state round trip (worlds and patterns, exactly)",
                std::to_string(differ) + " values differ");
        s.check(ya.name == "Frozen Gate" && y.p->locations.colour(1) == locationColours[5] && y.p->locations.name(3) == "Location 4",
                "with their names and colours");
        y.render(1.0);
        s.check(!y.p->travel.active() && y.get("destination") == 2.0f, "loading a project never starts a journey");
        Host old;
        const char* xml = "<WORLDBUILDER><PARAM id=\"temperature\" value=\"0.2\"/></WORLDBUILDER>";
        juce::MemoryBlock legacy;
        juce::AudioProcessor::copyXmlToBinary(*juce::parseXML(xml), legacy);
        old.p->setStateInformation(legacy.getData(), int(legacy.getSize()));
        s.check(!old.p->locations.used(0) && old.p->locations.name(0) == "Location 1", "a project from before Locations opens with empty slots");
    }

    // .world files: the whole world and its Locations; the session's own settings stay.
    {
        const auto text = h->p->exportWorld();
        Host y;
        y.set("master", -9.0f);
        y.set("playmode", float(PlayFree));
        y.set("quality", 2);
        y.render(0.2);
        const bool loaded = y.p->importWorld(text);
        int differ = 0;
        std::string first;
        for (int p = 0; p < PCount; ++p) {
            if (p == PPower || p == PMaster || p == PQuality || p == PPlayMode) continue;
            if (std::abs(y.get(specs[std::size_t(p)].id) - h->get(specs[std::size_t(p)].id)) > 1.0e-5f)
                if (differ++ == 0) first = specs[std::size_t(p)].id;
        }
        const auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("wb-test.world");
        file.replaceWithText(text);
        s.check(loaded && differ == 0 && y.p->locations.used(1) && y.p->locations.name(0) == "Frozen Gate"
                    && y.p->locations.get(1).patterns[1].element[0].pattern.sameNotes(b.patterns[1].element[0].pattern),
                "a .world file carries the whole world, its patterns and its Locations", std::to_string(differ) + " differ, first " + first);
        s.check(y.get("master") == -9.0f && juce::roundToInt(y.get("playmode")) == PlayFree && juce::roundToInt(y.get("quality")) == 2,
                "loading a .world file keeps Master Level, Play Mode and Quality");
        y.render(0.5);
        s.check(!y.p->travel.active() && !y.p->importWorld("<NOT_A_WORLD/>") && !y.p->importWorld("garbage"), "it starts no journey; anything else is refused");
        file.deleteFile();
    }

    // No allocation while journeys start, run, turn and arrive, or the Journey moves.
    {
        auto x = locationHost(true);
        x->playHead.playing = true;
        x->set("traveltime", 0.5f);
        x->render(0.5);
        allocationProbe::calls = 0;
        allocationProbe::enabled = true;
        for (int i = 0; i < 400; ++i) {
            if (i == 10) x->set("destination", 2);
            if (i == 30) x->set("destination", 1);
            if (i == 150) x->set("journeymode", 1);
            if (i > 150 && i < 300) x->set("journey", float(i - 150) / 150.0f);
            if (i == 300) x->set("journeymode", 0);
            if (i == 350) x->playHead.position = 48000;
            x->processOne(i % 3 == 0 ? 512 : 100 + i % 200);
        }
        allocationProbe::enabled = false;
        s.check(allocationProbe::calls == 0, "journeys perform no heap allocations on the audio thread", std::to_string(allocationProbe::calls) + " allocations");
    }
}

// A short subset for a second architecture (x86_64 under Rosetta in macOS CI): the
// default world, one Place from each category, and a state round trip. The Linux job
// runs everything.
void testSmoke(Suite& s) {
    s.section("smoke: default world, one Place per category, state round trip");
    {
        Host h;
        const auto st = measure(h.render(4.0), 2.5);
        s.check(st.finite && st.rmsDb > -40.0 && st.rmsDb < -10.0 && st.peak < 0.9, "the default world is audible, finite and below the ceiling",
                fmt(st.rmsDb) + " dBFS, peak " + fmt(st.peak));
    }
    for (int type : { Castle, City, Spaceport, Forest }) {
        auto h = placeHost(type, 0);
        const auto st = measure(h.render(4.0), 1.5);
        s.check(st.finite && st.rmsDb > -60.0 && st.peak < 0.9, std::string(placeNames[std::size_t(type)]) + " (" + categoryNames[std::size_t(placeInfo(type).category)]
                + ") alone is audible and finite", fmt(st.rmsDb) + " dBFS");
    }
    {
        Host a;
        a.set("temperature", 0.2f);
        a.set("seed", 99);
        a.set("intensity", 0.7f);
        a.set("p2_type", float(Megacity));
        a.set("p2_character", 1);
        a.p->patterns.syncTypes(a.p->slotTypes());
        a.p->patterns.setElement(1, Megacity, 2, editedElement({ makeNote(0, 0, 4), makeNote(6, 3, 2, 80, true) }, 0.1f));
        juce::MemoryBlock state;
        a.p->getStateInformation(state);
        Host b;
        b.p->setStateInformation(state.getData(), int(state.getSize()));
        int mismatches = 0;
        for (const auto& spec : specs) mismatches += std::abs(a.get(spec.id) - b.get(spec.id)) > 1.0e-4f ? 1 : 0;
        const auto pa = a.p->patterns.element(1, Megacity, 2), pb = b.p->patterns.element(1, Megacity, 2);
        s.check(mismatches == 0 && pa.controls == pb.controls && pa.pattern.sameNotes(pb.pattern), "state round trip: parameters and an edited pattern",
                std::to_string(mismatches) + " parameters differ");
        const auto x = a.render(2.0), y = b.render(2.0);
        s.check(x.l == y.l && x.r == y.r, "the restored world plays exactly the same");
    }
}


// The Planet (Milestone 10): each planet type colours the whole world, as strongly as
// Planet Influence; changes crossfade.
void testPlanet(Suite& s) {
    s.section("planet");
    using dsp::Rocky, dsp::Ocean, dsp::IceGiant, dsp::GasGiant, dsp::DesertWorld, dsp::LavaWorld, dsp::ForestWorld, dsp::PlanetTypeCount, dsp::planetNames;
    const auto world = [](Host& h, int planet, float influence) {
        h.set("p2_type", float(Village));
        h.set("planet", float(planet));
        h.set("planetinfluence", influence);
    };
    {
        Host h;
        s.check(juce::roundToInt(h.get("planet")) == 0 && std::abs(h.get("planetinfluence") - 0.35f) < 1.0e-4f, "Planet defaults to Rocky at 35 % Influence");
        bool automatable = true;
        for (const char* id : { "planet", "planetinfluence" }) automatable &= h.p->parameters.getParameter(id)->isAutomatable();
        s.check(automatable, "Planet and Planet Influence are automatable");
        s.check(moveOf(PPlanet) == Move::Switch && moveOf(PPlanetInfluence) == Move::Glide, "a Location holds the Planet (changes at the far point) and its Influence (glides)");
    }
    // Influence 0: no colour at all, whatever the planet.
    Audio neutral;
    {
        Host a, b;
        world(a, LavaWorld, 0.0f);
        world(b, Ocean, 0.0f);
        neutral = a.render(6.0);
        const auto other = b.render(6.0);
        // (after the opening glide from the default Rocky at 35 %)
        float most = 0;
        for (std::size_t i = std::size_t(3.0 * neutral.rate); i < neutral.l.size(); ++i)
            most = std::max({ most, std::abs(neutral.l[i] - other.l[i]), std::abs(neutral.r[i] - other.r[i]) });
        s.check(most < 1.0e-5f, "at 0 % Influence every planet sounds the same (no colour)", fmt(toDb(most)) + " dB largest difference");
    }
    const auto base = measure(neutral, 2.0);
    std::array<Stats, PlanetTypeCount> at {};
    for (int planet = 0; planet < PlanetTypeCount; ++planet) {
        Host h;
        world(h, planet, 1.0f);
        const auto a = h.render(6.0);
        const auto m = measure(a, 2.0);
        at[std::size_t(planet)] = m;
        const std::string name = planetNames[std::size_t(planet)];
        s.metric("Planet " + name + " at 100 %: level / low / mid / high / side change (dB)", m.rmsDb - base.rmsDb, "");
        std::cout << "    " << fmt(m.lowDb - base.lowDb) << " / " << fmt(m.midDb - base.midDb) << " / " << fmt(m.highDb - base.highDb) << " / "
                  << fmt(m.sideDb - base.sideDb) << ", motion " << fmt(m.motionDb - base.motionDb) << "\n";
        s.check(m.finite && m.peak < 0.9, name + " at 100 %: finite, below the ceiling", fmt(m.peak));
        s.check(std::abs(m.rmsDb - base.rmsDb) < 3.0, name + " at 100 %: level within 3 dB of no colour", fmt(m.rmsDb - base.rmsDb) + " dB");
        s.check(difference(m, base) > 1.0, name + " at 100 %: a measurable colour (> 1 dB in some band)", fmt(difference(m, base)) + " dB");
    }
    // The colours point the right way.
    const auto tilt = [&](int planet) { return (at[std::size_t(planet)].highDb - at[std::size_t(planet)].lowDb) - (base.highDb - base.lowDb); };
    s.check(tilt(IceGiant) > 1.5, "Ice Giant is brighter (highs up against lows)", fmt(tilt(IceGiant)) + " dB");
    s.check(tilt(DesertWorld) < -1.0 && tilt(ForestWorld) < -1.0, "Desert and Forest worlds are darker (dusty, damped)",
            fmt(tilt(DesertWorld)) + ", " + fmt(tilt(ForestWorld)) + " dB");
    // (against the mids: the glue compressor evens out the overall level)
    const auto weight = [&](int planet) { return (at[std::size_t(planet)].lowDb - at[std::size_t(planet)].midDb) - (base.lowDb - base.midDb); };
    s.check(weight(GasGiant) > 1.5 && weight(LavaWorld) > 1.0, "Gas Giant and Lava World add low weight (lows against mids)",
            fmt(weight(GasGiant)) + ", " + fmt(weight(LavaWorld)) + " dB");
    s.check(at[DesertWorld].sideDb - at[DesertWorld].rmsDb > base.sideDb - base.rmsDb + 1.0, "Desert World is wider",
            fmt((at[DesertWorld].sideDb - at[DesertWorld].rmsDb) - (base.sideDb - base.rmsDb)) + " dB");
    s.check(at[Rocky].sideDb - at[Rocky].rmsDb < base.sideDb - base.rmsDb, "Rocky is closer and drier (narrower)");
    {
        int alike = 0;
        for (int a = 0; a < PlanetTypeCount; ++a)
            for (int b = a + 1; b < PlanetTypeCount; ++b) alike += difference(at[std::size_t(a)], at[std::size_t(b)]) < 0.5 ? 1 : 0;
        s.check(alike == 0, "every planet colours the world differently from every other", std::to_string(alike) + " pairs alike");
    }
    // Influence scales the colour.
    {
        Host h;
        world(h, IceGiant, 0.35f);
        const auto m = measure(h.render(6.0), 2.0);
        const double subtle = difference(m, base), full = difference(at[IceGiant], base);
        s.check(subtle > 0.3 && subtle < full, "35 % Influence is a subtler colour than 100 %", fmt(subtle) + " < " + fmt(full) + " dB");
    }
    // A new planet crossfades: no click and no level jump beyond the world's own.
    for (auto [from, to] : { std::pair<int, int> { Rocky, LavaWorld }, { GasGiant, IceGiant }, { Ocean, DesertWorld } }) {
        Host steady, changed;
        world(steady, from, 1.0f);
        world(changed, from, 1.0f);
        auto a = steady.render(3.0), b = changed.render(3.0);
        changed.set("planet", float(to));
        const auto rest = changed.render(4.0), restSteady = steady.render(4.0);
        b.l.insert(b.l.end(), rest.l.begin(), rest.l.end());
        b.r.insert(b.r.end(), rest.r.begin(), rest.r.end());
        a.l.insert(a.l.end(), restSteady.l.begin(), restSteady.l.end());
        a.r.insert(a.r.end(), restSteady.r.begin(), restSteady.r.end());
        const auto [up, down] = levelSteps(b, 2.5, 6.5);
        const auto [upA, downA] = levelSteps(a, 2.5, 6.5);
        const double click = suddenness(b, a, 3.0);
        const std::string name = std::string(planetNames[std::size_t(from)]) + " -> " + planetNames[std::size_t(to)];
        s.check(std::max(up, down) < std::max(upA, downA) + 3.0 && click < -20.0, name + ": crossfades without a click or jump",
                "step " + fmt(std::max(up, down)) + " dB (steady " + fmt(std::max(upA, downA)) + "), click " + fmt(click) + " dB");
    }
    // Influence automated from 0 to 100 % over 4 s: smooth.
    {
        Host h;
        world(h, LavaWorld, 0.0f);
        h.render(1.0);
        Audio sweep;
        for (int k = 0; k <= 100; ++k) {
            h.set("planetinfluence", float(k) / 100.0f);
            const auto part = h.render(0.04);
            sweep.l.insert(sweep.l.end(), part.l.begin(), part.l.end());
            sweep.r.insert(sweep.r.end(), part.r.begin(), part.r.end());
        }
        const auto [up, down] = levelSteps(sweep, 0.0, 4.0);
        s.check(std::max(up, down) < 6.0 && measure(sweep, 0).finite, "Planet Influence automation is smooth", fmt(std::max(up, down)) + " dB largest 20 ms step");
    }
    // The globe shows the Planet: each one its own surface, crossfading on a change.
    {
        Host h;
        h.render(2.5);
        std::unique_ptr<juce::AudioProcessorEditor> editor(h.p->createEditor());
        auto* view = dynamic_cast<WorldbuilderEditor*>(editor.get());
        std::vector<juce::Colour> centres;
        bool opaque = true;
        for (int planet = 0; planet < PlanetTypeCount; ++planet) {
            h.set("planet", float(planet));
            view->settle(1.5f);
            editor->createComponentSnapshot(editor->getLocalBounds());
            const auto& f = view->globeRenderer().lastFrame();
            const auto c = f.getPixelAt(f.getWidth() * 2 / 5, f.getHeight() * 2 / 5);
            opaque &= c.getAlpha() == 255;
            centres.push_back(c);
        }
        int alike = 0;
        for (std::size_t a = 0; a < centres.size(); ++a)
            for (std::size_t b = a + 1; b < centres.size(); ++b)
                alike += std::abs(centres[a].getFloatRed() - centres[b].getFloatRed()) + std::abs(centres[a].getFloatGreen() - centres[b].getFloatGreen())
                                 + std::abs(centres[a].getFloatBlue() - centres[b].getFloatBlue()) < 0.06f
                             ? 1 : 0;
        s.check(opaque && alike == 0 && view->shownPlanetType() == PlanetTypeCount - 1, "the globe draws each planet opaque and in its own colours",
                std::to_string(alike) + " alike");
        h.set("planet", float(Rocky));
        view->settle(0.3f);
        editor->createComponentSnapshot(editor->getLocalBounds());
        s.check(view->shownPlanetType() == Rocky, "a new planet starts its crossfade on the globe");
        // The Planet controls sit by the globe, clear of the Location markers; hidden while
        // the detail panel covers the globe.
        juce::Component* planetBox = nullptr;
        for (auto* c : editor->getChildren())
            if (auto* box = dynamic_cast<juce::ComboBox*>(c); box != nullptr && box->getText() == "Rocky") planetBox = box;
        bool clear = planetBox != nullptr && planetBox->isVisible();
        for (int i = 0; i < locationCount && clear; ++i) clear &= !planetBox->getBounds().toFloat().expanded(7.0f).contains(view->markerPosition(i));
        s.check(clear, "the PLANET menu is shown by the globe, clear of the Location markers");
        view->addPlace(Village);
        view->openDetail(0);
        view->settle(0.1f);
        s.check(planetBox != nullptr && !planetBox->isVisible(), "the Planet controls hide while a detail panel covers the globe");
    }
    // State: saved and restored; a project from before the Planet keeps its sound.
    {
        Host a;
        a.set("planet", float(GasGiant));
        a.set("planetinfluence", 0.8f);
        juce::MemoryBlock state;
        a.p->getStateInformation(state);
        Host b;
        b.p->setStateInformation(state.getData(), int(state.getSize()));
        s.check(juce::roundToInt(b.get("planet")) == GasGiant && std::abs(b.get("planetinfluence") - 0.8f) < 1.0e-4f, "the Planet survives a state round trip");
        auto xml = juce::AudioProcessor::getXmlFromBinary(state.getData(), int(state.getSize()));
        for (auto* child = xml->getFirstChildElement(); child != nullptr;) {
            auto* next = child->getNextElement();
            const auto id = child->getStringAttribute("id");
            if (id == "planet" || id == "planetinfluence") xml->removeChildElement(child, true);
            child = next;
        }
        juce::MemoryBlock old;
        juce::AudioProcessor::copyXmlToBinary(*xml, old);
        Host c;
        c.p->setStateInformation(old.getData(), int(old.getSize()));
        s.check(c.get("planetinfluence") == 0.0f, "a project saved before the Planet opens with no planet colour (its sound unchanged)");
    }
}

// --snapshots dir [prefix]: images of the interface for review by eye (the globe on
// each planet, the cards, the strips, the detail panel, the Pattern Editor, the bars),
// written as dir/prefix-name.png. Crops follow the editor's fixed 1160 x 812 layout.
int writeSnapshots(const juce::String& dir, const juce::String& prefix) {
    juce::File folder(dir);
    folder.createDirectory();
    const auto save = [&](const juce::Image& image, const juce::String& name) {
        juce::File file(folder.getChildFile(prefix + "-" + name + ".png"));
        file.deleteFile();
        juce::FileOutputStream out(file);
        juce::PNGImageFormat().writeImageToStream(image, out);
        std::cout << "  " << file.getFullPathName() << "\n";
    };
    const juce::Rectangle<int> header(0, 0, 1160, 58), globe(318, 54, 514, 342), cards(318, 394, 514, 234), left(6, 54, 312, 574),
        right(832, 54, 322, 574), centre(318, 54, 514, 574), editorArea(6, 54, 826, 574), bottom(6, 624, 1148, 182);
    struct View {
        Host h;
        std::unique_ptr<juce::AudioProcessorEditor> editor;
        WorldbuilderEditor* view = nullptr;
        View(std::vector<int> places, std::vector<std::pair<const char*, float>> settings) {
            for (int slot = 0; slot < placeSlotCount; ++slot) h.set(slotId(slot, PlType), float(NoPlace));
            for (int slot = 0; slot < placeSlotCount && slot < int(places.size()); ++slot) h.set(slotId(slot, PlType), float(places[std::size_t(slot)]));
            for (auto [id, value] : settings)
                if (h.p->parameters.getParameter(id) != nullptr) h.set(id, value);
            h.render(2.5);   // past the 2 s power fade-in
            editor.reset(h.p->createEditor());
            view = dynamic_cast<WorldbuilderEditor*>(editor.get());
            view->settle(2.5f);
        }
        juce::Image image() { return editor->createComponentSnapshot(editor->getLocalBounds()); }
    };
    {
        View v({ Castle }, {});
        const auto image = v.image();
        save(image, "full-default");
        save(image.getClippedImage(header), "header");
        save(image.getClippedImage(cards), "cards");
        save(image.getClippedImage(left), "controls");
        save(image.getClippedImage(bottom), "bottom");
        v.view->showCategory(SciFi);
        save(v.image().getClippedImage(cards), "cards-scifi");
    }
    {
        View v({ Castle, Village, Spaceport, Forest }, {});
        const auto image = v.image();
        save(image, "full-world");
        save(image.getClippedImage(right), "strips");
        save(image.getClippedImage(globe), "globe-places");
        v.view->openDetail(1);
        v.view->settle(0.1f);
        save(v.image().getClippedImage(centre), "detail");
        v.view->openPatterns(0);
        v.view->settle(0.1f);
        save(v.image().getClippedImage(editorArea), "patterns");
    }
    const std::vector<std::pair<const char*, std::vector<std::pair<const char*, float>>>> worlds {
        { "globe-frozen", { { "temperature", 0.05f }, { "weather", 0.2f } } },
        { "globe-scorching", { { "temperature", 0.95f }, { "danger", 0.7f } } },
        { "globe-storm", { { "weather", 0.9f }, { "wetness", 0.9f } } },
    };
    for (const auto& [name, settings] : worlds) {
        View v({ Castle }, settings);
        save(v.image().getClippedImage(globe), name);
    }
    {
        View v({ City, Suburbs, Megacity }, { { "population", 0.9f } });
        save(v.image().getClippedImage(globe), "globe-city");
    }
    // Each planet type (when the build has them).
    if (Host probe; probe.p->parameters.getParameter("planet") != nullptr) {
        const juce::StringArray planets { "rocky", "ocean", "icegiant", "gasgiant", "desert", "lava", "forest" };
        for (int planet = 0; planet < planets.size(); ++planet) {
            View v({ Castle, Village }, { { "planet", float(planet) }, { "population", 0.7f } });
            save(v.image().getClippedImage(globe), "globe-planet-" + planets[planet]);
            if (std::getenv("WB_DEBUG_GLOBE")) {
                const auto& f = v.view->globeRenderer().lastFrame();
                std::cout << planets[planet] << " shown " << v.view->shownPlanetType() << " centre " << juce::String::toHexString((int) f.getPixelAt(f.getWidth() / 2, f.getHeight() / 2).getARGB()) << " ul "
                          << juce::String::toHexString((int) f.getPixelAt(f.getWidth() / 3, f.getHeight() / 3).getARGB()) << "\n";
            }
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    juce::String snapshot;
    for (int i = 1; i + 1 < argc; ++i)
        if (juce::String(argv[i]) == "--place")
            for (const auto& key : juce::StringArray::fromTokens(argv[i + 1], ",", ""))
                for (int type = NoPlace + 1; type < PlaceTypeCount; ++type)
                    if (key == placeKeys[std::size_t(type)]) placeFilter.push_back(type);
    for (int i = 1; i < argc; ++i)
        if (juce::String(argv[i]) == "--snapshot" && i + 1 < argc) snapshot = argv[++i];
        else if (juce::String(argv[i]) == "--snapshots" && i + 1 < argc) return writeSnapshots(argv[i + 1], i + 2 < argc ? juce::String(argv[i + 2]) : juce::String("shot"));
        else if (juce::String(argv[i]) == "--probe") return probe();
        else if (juce::String(argv[i]) == "--probe-places") return probePlaces();
        else if (juce::String(argv[i]) == "--render" && i + 2 < argc) {
            juce::StringArray settings;
            for (int k = i + 3; k < argc; ++k) settings.add(argv[k]);
            return renderToFile(argv[i + 1], juce::String(argv[i + 2]).getDoubleValue(), settings);
        }

    // --only name[,name]: run only those groups; --place key[,key]: only those Places in
    // the Place groups (development aids).
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
    if (run("instruments")) testInstruments(s);
    if (run("placeworld")) testPlaceWorld(s);
    if (run("slots")) testPlaceSlots(s);
    if (run("patterns")) testPatterns(s);
    if (run("playmode")) testPlayMode(s);
    if (run("determinism")) {
        testDeterminism(s);
        testFirstPlay(s);
    }
    if (run("power")) testPowerAndMaster(s);
    if (run("tonal")) testTonalChanges(s);
    if (run("state")) testState(s);
    if (run("models")) testModels(s);
    if (run("distinct")) testDistinct(s);
    if (run("effects")) testEffects(s);
    if (run("intensity")) testIntensity(s);
    if (run("capture")) testCapture(s);
    if (run("automation")) testAutomation(s);
    if (run("realtime")) testRealtimeSafety(s);
    if (run("editor")) testEditor(s, snapshot);
    if (run("overlays")) testOverlays(s, snapshot);
    if (run("locations")) testLocations(s);
    if (run("smoke")) testSmoke(s);
    if (run("planet")) testPlanet(s);
    std::cout << "\n" << s.passed << " passed, " << s.failed << " failed" << std::endl;
    return s.failed == 0 ? 0 : 1;
}
