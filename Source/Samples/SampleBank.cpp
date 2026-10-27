#include "SampleBank.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace wb::samples {

namespace {
juce::File& folderOverride() {
    static juce::File f;
    return f;
}
constexpr int padBefore = 2, padAfter = 4;
} // namespace

//==============================================================================
int Instrument::foldKey(int key) const {
    if (highKey - lowKey < 12) return std::clamp(key, lowKey, highKey);
    while (key < lowKey) key += 12;
    while (key > highKey) key -= 12;
    return key;
}

void Instrument::index() {
    lowKey = 127;
    highKey = 0;
    lowRoot = 127;
    highRoot = 0;
    for (const auto& z : zones) {
        lowKey = std::min(lowKey, z.loKey);
        highKey = std::max(highKey, z.hiKey);
        lowRoot = std::min(lowRoot, z.root);
        highRoot = std::max(highRoot, z.root);
    }
    for (int layer = 0; layer < int(layerMedianDb.size()); ++layer) {
        std::vector<float> levels;
        for (const auto& z : zones)
            if (z.layer == layer) levels.push_back(z.loudnessDb);
        if (levels.empty()) continue;
        std::nth_element(levels.begin(), levels.begin() + std::ptrdiff_t(levels.size() / 2), levels.end());
        layerMedianDb[std::size_t(layer)] = levels[levels.size() / 2];
    }
    lowKey = std::clamp(lowKey, 0, 127);
    highKey = std::clamp(highKey, lowKey, 127);
    table.assign(std::size_t(128 * layers), Slot {});
    order.clear();
    for (int layer = 0; layer < layers; ++layer)
        for (int key = 0; key < 128; ++key) {
            std::vector<int> found;
            for (int i = 0; i < int(zones.size()); ++i) {
                const auto& z = zones[std::size_t(i)];
                if (z.layer == layer && key >= z.loKey && key <= z.hiKey) found.push_back(i);
            }
            if (found.empty()) {
                // A key between the zones (or outside them): the nearest recorded pitch.
                float best = 1.0e9f;
                int nearest = -1;
                for (int i = 0; i < int(zones.size()); ++i) {
                    const auto& z = zones[std::size_t(i)];
                    if (z.layer != layer) continue;
                    const float d = std::abs(z.root - float(key));
                    if (d < best - 0.01f) {
                        best = d;
                        nearest = i;
                    }
                }
                if (nearest < 0) continue;
                for (int i = 0; i < int(zones.size()); ++i)
                    if (zones[std::size_t(i)].layer == layer && std::abs(zones[std::size_t(i)].root - zones[std::size_t(nearest)].root) < 0.5f) found.push_back(i);
            }
            std::sort(found.begin(), found.end(), [&](int a, int b) { return zones[std::size_t(a)].rr < zones[std::size_t(b)].rr; });
            auto& slot = table[std::size_t(key * layers + layer)];
            slot.first = int16_t(order.size());
            slot.count = int16_t(found.size());
            order.insert(order.end(), found.begin(), found.end());
        }
}

const Zone* Instrument::find(int key, float velocity, float pick) const {
    if (zones.empty() || table.empty()) return nullptr;
    key = std::clamp(foldKey(key), 0, 127);
    // The layer whose velocity range holds this velocity.
    int layer = layers - 1;
    for (int l = 0; l < layers; ++l) {
        const auto& s = table[std::size_t(key * layers + l)];
        if (s.count <= 0) continue;
        const auto& z = zones[std::size_t(order[std::size_t(s.first)])];
        if (velocity < z.hiVel || l == layers - 1) {
            layer = l;
            break;
        }
    }
    auto s = table[std::size_t(key * layers + layer)];
    for (int l = layer; s.count <= 0 && l >= 0; --l) s = table[std::size_t(key * layers + l)];
    if (s.count <= 0) return nullptr;
    const int k = std::clamp(int(pick * float(s.count)), 0, s.count - 1);
    return &zones[std::size_t(order[std::size_t(s.first + k)])];
}

//==============================================================================
Bank& Bank::get() {
    static Bank bank;
    return bank;
}

Bank::~Bank() {
    cancel = true;
    if (thread.joinable()) thread.join();
}

void Bank::setFolder(const juce::File& folder) { folderOverride() = folder; }

juce::File Bank::locate() {
    const auto valid = [](const juce::File& f) { return f.getChildFile("index.json").existsAsFile(); };
    if (valid(folderOverride())) return folderOverride();
    const auto env = juce::SystemStats::getEnvironmentVariable("WORLDBUILDER_SAMPLES", {});
    if (env.isNotEmpty() && valid(juce::File(env))) return juce::File(env);
    // Inside the plugin's own bundle: .../Contents/MacOS/WORLDBUILDER -> Contents/Resources/Samples
    // (on Linux .../Contents/x86_64-linux/WORLDBUILDER.so; a Linux standalone keeps them beside it).
    const auto binary = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    for (const auto& candidate : { binary.getParentDirectory().getParentDirectory().getChildFile("Resources").getChildFile("Samples"),
                                   binary.getParentDirectory().getChildFile("Samples"),
                                   binary.getParentDirectory().getChildFile("Resources").getChildFile("Samples") })
        if (valid(candidate)) return candidate;
    return {};
}

void Bank::load() {
    if (started.exchange(true)) return;
    status.store(Loading, std::memory_order_release);
    thread = std::thread([this] { run(); });
}

bool Bank::waitUntilLoaded(int ms) const {
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (state() == Loading || state() == Idle) {
        if (state() == Idle || std::chrono::steady_clock::now() >= until) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return ready();
}

const Instrument* Bank::instrument(const char* id) const {
    if (!ready()) return nullptr;
    for (const auto& i : instruments)
        if (i->id == id) return i.get();
    return nullptr;
}

int Bank::zoneCount() const {
    if (!ready()) return 0;
    int n = 0;
    for (const auto& i : instruments) n += int(i->zones.size());
    return n;
}

juce::String Bank::message() const {
    switch (state()) {
        case Idle: return "samples not loaded";
        case Loading: return "loading samples";
        case Missing: return "no samples found (sampled instruments use their synthesised fallbacks)";
        case Failed: return "samples failed to load: " + error;
        default: break;
    }
    return juce::String(zoneCount()) + " recorded notes from " + from.getFullPathName() + ", "
           + juce::String(double(memoryBytes()) / 1048576.0, 1) + " MB in memory, loaded in " + juce::String(loadSeconds(), 2) + " s";
}

void Bank::run() {
    const auto t0 = std::chrono::steady_clock::now();
    from = locate();
    if (from == juce::File()) {
        status.store(Missing, std::memory_order_release);
        return;
    }
    const auto index = juce::JSON::parse(from.getChildFile("index.json"));
    const auto* list = index["instruments"].getArray();
    if (list == nullptr) {
        error = "index.json lists no instruments";
        status.store(Failed, std::memory_order_release);
        return;
    }
    std::size_t total = 0;
    for (const auto& id : *list) {
        if (cancel) return;
        auto inst = std::make_unique<Instrument>();
        std::vector<int16_t> pool;
        if (!loadInstrument(from.getChildFile(id.toString() + ".json"), *inst, pool)) {
            status.store(Failed, std::memory_order_release);
            return;
        }
        total += pool.size() * sizeof(int16_t) + inst->zones.size() * sizeof(Zone);
        instruments.push_back(std::move(inst));
        pools.push_back(std::move(pool));   // moving keeps the buffer (the zones point into it)
    }
    bytes = total;
    seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    status.store(Ready, std::memory_order_release);
}

bool Bank::loadInstrument(const juce::File& map, Instrument& into, std::vector<int16_t>& pool) {
    const auto json = juce::JSON::parse(map);
    const auto* zones = json["zones"].getArray();
    if (zones == nullptr) {
        error = map.getFileName() + " has no zones";
        return false;
    }
    into.id = json["id"].toString();
    into.name = json["name"].toString();
    into.rate = float(double(json["rate"]));
    if (!(into.rate > 1000.0f)) into.rate = 44100.0f;
    into.layers = std::max(1, json["layers"].getArray() != nullptr ? json["layers"].getArray()->size() : 1);
    // One buffer for the instrument, sized from the mapping before anything points into it.
    std::size_t need = 0;
    for (const auto& z : *zones) need += std::size_t(int(z["length"])) + padBefore + padAfter;
    pool.assign(need, 0);
    juce::FlacAudioFormat flac;
    juce::AudioBuffer<float> buffer;
    std::size_t at = 0;
    for (const auto& z : *zones) {
        if (cancel) return false;
        const auto file = from.getChildFile(z["file"].toString());
        auto stream = std::make_unique<juce::FileInputStream>(file);
        if (!stream->openedOk()) {
            error = "cannot open " + file.getFullPathName();
            return false;
        }
        std::unique_ptr<juce::AudioFormatReader> reader(flac.createReaderFor(stream.release(), true));
        const int length = int(z["length"]);
        if (reader == nullptr || int(reader->lengthInSamples) < length) {
            error = "cannot read " + file.getFullPathName();
            return false;
        }
        buffer.setSize(1, length, false, false, true);
        reader->read(&buffer, 0, length, 0, true, false);
        Zone zone;
        int16_t* data = pool.data() + at + padBefore;
        const float* src = buffer.getReadPointer(0);
        for (int i = 0; i < length; ++i) data[i] = int16_t(std::lrint(std::clamp(src[i], -1.0f, 32767.0f / 32768.0f) * 32768.0f));
        zone.data = data;
        zone.length = length;
        zone.root = float(double(z["root"]));
        zone.loKey = int(z["loKey"]);
        zone.hiKey = int(z["hiKey"]);
        zone.loVel = float(double(z["loVel"]));
        zone.hiVel = float(double(z["hiVel"]));
        zone.layer = std::clamp(int(z["layer"]), 0, into.layers - 1);
        zone.rr = int(z["rr"]);
        zone.rmsDb = float(double(z["rms"]));
        zone.loudnessDb = z.hasProperty("loudness") ? float(double(z["loudness"])) : zone.rmsDb;
        if (z.hasProperty("loopStart")) {
            zone.loopStart = int(z["loopStart"]);
            zone.loopEnd = int(z["loopEnd"]);
            zone.release = z.hasProperty("release") ? int(z["release"]) : -1;
            if (!(zone.loopStart >= 1 && zone.loopEnd > zone.loopStart + 64 && zone.loopEnd + 3 < length)) zone.loopStart = zone.loopEnd = -1;
            // The interpolator reads past the loop's end: there, the loop's start.
            if (zone.loops())
                for (int k = 0; k < 3; ++k) data[zone.loopEnd + k] = data[zone.loopStart + k];
            if (zone.release <= zone.loopEnd || zone.release >= length - 64) zone.release = -1;
        }
        into.zones.push_back(zone);
        at += std::size_t(length) + padBefore + padAfter;
    }
    into.index();
    return true;
}

} // namespace wb::samples
