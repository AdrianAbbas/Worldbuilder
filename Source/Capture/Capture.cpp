#include "Capture.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace wb::capture {

namespace {

bool anySoloIn(const EngineControls& c) {
    bool any = false;
    for (bool s : c.solo) any |= s;
    for (const auto& p : c.places) any |= p.solo && p.type != NoPlace;
    return any;
}

bool slotAudible(const EngineControls& c, int slot, bool anySolo) {
    const auto& p = c.places[std::size_t(slot)];
    const bool built = p.type > NoPlace && p.type < PlaceTypeCount && placeImplemented[std::size_t(p.type)];
    return built && !p.mute && (!anySolo || p.solo);
}

// A fresh engine, holding the scene's patterns, started at song position 0 (bar 1),
// as the host's Transport playback from the start does.
std::unique_ptr<WorldEngine> startEngine(const Scene& scene, EngineControls& controls) {
    controls.gate = true;
    controls.hostBpm = scene.bpm;
    auto engine = std::make_unique<WorldEngine>();
    engine->prepare(scene.sampleRate, controls);
    for (int slot = 0; slot < placeSlots; ++slot) {
        auto& buffer = engine->patternsIn[std::size_t(slot)];
        buffer.write() = scene.patterns[std::size_t(slot)];
        buffer.publish();
    }
    engine->restart(0, 0.0, controls);
    return engine;
}

} // namespace

int64_t lengthInSamples(const Scene& scene, int bars) {
    const double seconds = double(std::max(bars, 0) * beatsPerBar) * 60.0 / double(std::clamp(scene.bpm, 20.0f, 999.0f));
    return int64_t(std::llround(seconds * scene.sampleRate));
}

std::vector<Pass> passes(const Scene& scene, bool stems) {
    const auto& c = scene.controls;
    if (!stems) return { { "Mix", c } };
    std::vector<Pass> out;
    const bool anySolo = anySoloIn(c);
    std::array<int, PlaceTypeCount> seen {};
    for (int slot = 0; slot < placeSlots; ++slot) {
        if (!slotAudible(c, slot, anySolo)) continue;
        Pass pass { placeNames[std::size_t(c.places[std::size_t(slot)].type)], c };
        // The same Place in two slots: "Castle", "Castle 2".
        if (++seen[std::size_t(c.places[std::size_t(slot)].type)] > 1) pass.name << " " << seen[std::size_t(c.places[std::size_t(slot)].type)];
        for (int other = 0; other < placeSlots; ++other) {
            pass.controls.places[std::size_t(other)].mute = other != slot;
            pass.controls.places[std::size_t(other)].solo = false;
        }
        pass.controls.mute.fill(true);
        pass.controls.solo.fill(false);
        out.push_back(pass);
    }
    bool environment = false;
    for (int l = 0; l < LayerCount; ++l)
        environment |= !c.mute[std::size_t(l)] && (!anySolo || c.solo[std::size_t(l)]) && c.layerDb[std::size_t(l)] > -59.9f;
    if (environment) {
        Pass pass { "Environment", c };
        for (auto& p : pass.controls.places) {
            p.mute = true;
            p.solo = false;
        }
        out.push_back(pass);
    }
    return out;
}

bool render(const Scene& scene, const EngineControls& controls, int64_t samples, const Sink& sink, std::atomic<bool>* cancel,
            std::atomic<float>* progress, int block) {
    auto c = controls;
    auto engine = startEngine(scene, c);
    block = std::clamp(block, 1, 8192);
    std::vector<float> left(static_cast<std::size_t>(block)), right(static_cast<std::size_t>(block));
    for (int64_t done = 0; done < samples;) {
        if (cancel != nullptr && cancel->load(std::memory_order_relaxed)) return false;
        const int n = int(std::min<int64_t>(block, samples - done));
        engine->process(left.data(), right.data(), n, c);
        if (!sink(left.data(), right.data(), n)) return false;
        done += n;
        if (progress != nullptr) progress->store(float(double(done) / double(samples)), std::memory_order_relaxed);
    }
    return true;
}

bool renderToWav(const Scene& scene, const Pass& pass, int bars, const juce::File& file, std::atomic<bool>* cancel, std::atomic<float>* progress) {
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    bool ok = false;
    {
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
        if (!static_cast<juce::FileOutputStream*>(stream.get())->openedOk()) return false;
        const auto options = juce::AudioFormatWriterOptions().withSampleRate(scene.sampleRate).withNumChannels(2).withBitsPerSample(24);
        const auto writer = juce::WavAudioFormat().createWriterFor(stream, options);
        if (writer == nullptr) return false;
        ok = render(scene, pass.controls, lengthInSamples(scene, bars),
                    [&](const float* l, const float* r, int n) {
                        const float* channels[] = { l, r };
                        return writer->writeFromFloatArrays(channels, 2, n);
                    },
                    cancel, progress);
    }
    if (!ok) file.deleteFile();
    return ok;
}

std::vector<Track> sceneTracks(const Scene& scene) {
    const auto& c = scene.controls;
    const bool anySolo = anySoloIn(c);
    // Every Place sounding (whatever the mix's mutes) so each one writes its patterns.
    auto playing = c;
    playing.power = true;
    for (auto& p : playing.places) {
        p.mute = false;
        p.solo = false;
    }
    playing.solo.fill(false);
    auto engine = startEngine(scene, playing);
    std::array<float, dsp::chunk> l {}, r {};
    engine->process(l.data(), r.data(), dsp::chunk, playing);

    std::vector<Track> out;
    for (int slot = 0; slot < placeSlots; ++slot) {
        if (!slotAudible(c, slot, anySolo)) continue;
        const auto& pc = c.places[std::size_t(slot)];
        const auto& slotPatterns = scene.patterns[std::size_t(slot)];
        const bool mine = slotPatterns.type == pc.type;
        bool elementSolo = false;
        if (mine)
            for (const auto& el : slotPatterns.element) elementSolo |= el.controls.solo;
        for (int e = 0; e < placeElements; ++e) {
            const auto info = patternInfo(pc.type, e);
            if (info.kind == NoPattern || !pc.elementOn[std::size_t(e)] || pc.element[std::size_t(e)] <= 0.001f) continue;
            if (mine) {
                const auto& controls = slotPatterns.element[std::size_t(e)].controls;
                if (controls.mute || (elementSolo && !controls.solo)) continue;
            }
            const auto* pattern = engine->placeSlot(slot).playingPattern(e);
            if (pattern == nullptr || pattern->count <= 0) continue;
            Track t;
            t.name = juce::String(placeNames[std::size_t(pc.type)]) + " - " + placeInfo(pc.type).element[std::size_t(e)].name;
            t.slot = slot;
            t.element = e;
            t.pattern = *pattern;
            t.target.root = c.root;
            t.target.mood = c.mood;
            t.target.kit = info.kind == Kit;
            t.target.kitRows = info.rows;
            t.target.bpm = scene.bpm;
            t.target.danger = dsp::clamp01(c.world[Danger]);
            out.push_back(t);
        }
    }
    return out;
}

juce::MidiFile sceneMidi(const std::vector<Track>& tracks, int bars, float bpm) {
    const double beats = double(std::max(bars, 1) * beatsPerBar);
    juce::MidiFile file;
    file.setTicksPerQuarterNote(pat::ticksPerBeat);
    juce::MidiMessageSequence tempo;
    tempo.addEvent(juce::MidiMessage::textMetaEvent(3, "WORLDBUILDER"), 0);
    tempo.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0);
    tempo.addEvent(juce::MidiMessage::tempoMetaEvent(int(60000000.0 / std::clamp(double(bpm), 20.0, 999.0))), 0);
    tempo.addEvent(juce::MidiMessage::endOfTrack(), beats * pat::ticksPerBeat);
    file.addTrack(tempo);
    for (const auto& t : tracks) {
        juce::MidiMessageSequence track;
        track.addEvent(juce::MidiMessage::textMetaEvent(3, t.name), 0);
        // Drums on channel 10 (General MIDI), everything else on channel 1.
        pat::appendNotes(track, t.pattern, t.target, beats, t.target.kit ? 10 : 1);
        track.updateMatchedPairs();
        // Every track ends at the last bar, so the regions line up in the host.
        track.addEvent(juce::MidiMessage::endOfTrack(), beats * pat::ticksPerBeat);
        file.addTrack(track);
    }
    return file;
}

} // namespace wb::capture
