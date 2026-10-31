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
        Pass pass { placeNames[std::size_t(c.places[std::size_t(slot)].type)], c, true };
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
        Pass pass { "Environment", c, true };
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

juce::String trackName(int type, int element) {
    if (type <= NoPlace || type >= PlaceTypeCount) return {};
    return juce::String(placeNames[std::size_t(type)]) + " - " + placeInfo(type).element[std::size_t(std::clamp(element, 0, placeElements - 1))].name;
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
            // Milestone 16: an instrument whose notes are only in the arrangement's clips.
            bool clips = false;
            for (int i = 0; i < scene.arrangement.count; ++i)
                if (const auto& clip = scene.arrangement.sections[std::size_t(i)].cells[std::size_t(slot)][std::size_t(e)].clip) clips |= clip->count > 0;
            if ((pattern == nullptr || pattern->count <= 0) && !clips) continue;
            Track t;
            t.name = trackName(pc.type, e);
            t.slot = slot;
            t.element = e;
            if (pattern != nullptr) t.pattern = *pattern;
            else t.pattern.clear();
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

//==============================================================================
// Milestone 11c.

int64_t samplesFor(const Scene& scene, double bars) {
    const double seconds = std::max(bars, 0.0) * double(beatsPerBar) * 60.0 / double(std::clamp(scene.bpm, 20.0f, 999.0f));
    return int64_t(std::llround(seconds * scene.sampleRate));
}

double arrangementBars(const arr::Arrangement& a) {
    if (a.count <= 0) return 0;
    const int endless = a.firstEndless();
    if (endless < a.count) return double(a.startOf(endless) + a.sections[std::size_t(endless)].bars);
    return double(a.totalBars()) + (a.end == arr::EndStop ? 2.0 : 0.0);
}

bool renderArranged(const Scene& scene, const arr::Arrangement& a, const Pass& pass, double startBeat, int64_t samples, const Sink& sink,
                    std::atomic<bool>* cancel, std::atomic<float>* progress, int block) {
    auto base = pass.controls;
    base.gate = true;
    base.hostBpm = scene.bpm;
    base.arrangement = &a;
    auto engine = std::make_unique<WorldEngine>();
    engine->prepare(scene.sampleRate, base);
    std::array<const pat::SlotPatterns*, placeSlots> live {};
    for (int slot = 0; slot < placeSlots; ++slot) {
        auto& buffer = engine->patternsIn[std::size_t(slot)];
        buffer.write() = scene.patterns[std::size_t(slot)];
        buffer.publish();
        live[std::size_t(slot)] = &scene.patterns[std::size_t(slot)];
    }
    // The sections' Locations: the processor's journeys, in the same pieces.
    const bool located = a.usesLocations() && scene.controlsOf != nullptr && scene.locations != nullptr;
    std::unique_ptr<Travel> travel;
    if (located) {
        travel = std::make_unique<Travel>();
        travel->prepare(scene.sampleRate);
        for (int i = 0; i < locationCount; ++i) {
            travel->locationsIn[std::size_t(i)].write() = (*scene.locations)[std::size_t(i)];
            travel->locationsIn[std::size_t(i)].publish();
        }
    }
    const double bpm = double(std::clamp(scene.bpm, 20.0f, 999.0f));
    const auto startAt = uint64_t(std::llround(std::max(startBeat, 0.0) * 60.0 / bpm * scene.sampleRate));
    block = std::clamp(block, 1, 8192);
    std::vector<float> left(static_cast<std::size_t>(block)), right(static_cast<std::size_t>(block));
    bool first = true;
    for (int64_t done = 0; done < samples;) {
        if (cancel != nullptr && cancel->load(std::memory_order_relaxed)) return false;
        const int n = int(std::min<int64_t>(block, samples - done));
        for (int piece = 0; piece < n;) {
            int count = n - piece;
            EngineControls c = base;
            TravelInputs in = scene.travel;
            if (located) {
                in.bpm = bpm;
                in.beat = first ? startBeat : engine->nextBeat();
                const double travelBeats = in.syncBars > 0 ? 4.0 * double(in.syncBars) : double(in.seconds) * bpm / 60.0;
                float position = 0;
                a.locationAt(in.beat, travelBeats, in.from, in.to, position);
                in.journey = position;
                in.arranged = in.journeyOn = true;
                in.pathOn = false;
                const auto& world = travel->advance(scene.values, live, in, first);
                if (travel->active()) count = std::min(count, 128);
                c = scene.controlsOf(world.v);
                for (int slot = 0; slot < placeSlots; ++slot) {
                    c.places[std::size_t(slot)].travelFar = world.far[std::size_t(slot)];
                    c.patterns[std::size_t(slot)] = world.patterns[std::size_t(slot)];
                    if (pass.stem) {
                        c.places[std::size_t(slot)].mute = base.places[std::size_t(slot)].mute;
                        c.places[std::size_t(slot)].solo = base.places[std::size_t(slot)].solo;
                    }
                }
                if (pass.stem) {
                    c.mute = base.mute;
                    c.solo = base.solo;
                }
                c.gate = true;
                c.hostBpm = scene.bpm;
                c.arrangement = &a;
            }
            if (first) engine->restart(startAt, startBeat, c);
            engine->process(left.data() + piece, right.data() + piece, count, c);
            if (located) travel->step(count, in, scene.values);
            first = false;
            piece += count;
        }
        if (!sink(left.data(), right.data(), n)) return false;
        done += n;
        if (progress != nullptr) progress->store(float(double(done) / double(std::max<int64_t>(samples, 1))), std::memory_order_relaxed);
    }
    return true;
}

juce::String sceneName(const Scene& scene) {
    const auto& c = scene.controls;
    const bool anySolo = anySoloIn(c);
    for (int slot = 0; slot < placeSlots; ++slot)
        if (slotAudible(c, slot, anySolo)) return juce::String(placeNames[std::size_t(c.places[std::size_t(slot)].type)]).removeCharacters(" -/'");
    return "World";
}

namespace {
// A name part for a file: letters, digits and hyphens.
juce::String filePart(const juce::String& text) {
    auto s = text.retainCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-");
    return s.isEmpty() ? juce::String("Section") : s;
}
} // namespace

std::vector<GameFile> gameFiles(const Scene& scene, bool stingers) {
    std::vector<GameFile> out;
    const auto& a = scene.arrangement;
    const auto base = sceneName(scene);
    const int reachable = std::min(a.count, a.firstEndless() + 1);
    juce::StringArray used;
    for (int i = 0; i < reachable; ++i) {
        const auto& s = a.sections[std::size_t(i)];
        juce::String name = base + "_" + filePart(juce::String::fromUTF8(s.nameText()));
        // The same name twice (Town Theme's A): A, A_2.
        const juce::String unique = name;
        for (int k = 2; used.contains(name); ++k) name = unique + "_" + juce::String(k);
        used.add(name);
        GameFile f;
        f.section = i;
        if (stingers) {
            f.kind = GameFile::Stinger;
            f.name = name + "_Stinger";
        } else {
            f.kind = s.endless() || s.repeats > 1 ? GameFile::Loop : GameFile::Section;
            f.name = name;
        }
        out.push_back(f);
    }
    return out;
}

double stingerBars(const arr::Section& s) {
    double bars = 1.0;
    if (s.transition != arr::TransCut && s.transition != arr::TransDrop)
        bars = std::min(2.0, std::ceil(arr::transitionBeats(s.transition, s.transitionLength) / double(beatsPerBar)));
    return std::min(bars, double(std::max<int>(s.bars, 1)));
}

int64_t renderGameFile(const Scene& scene, const GameFile& file, const Pass& pass, const Sink& sink, std::atomic<bool>* cancel, std::atomic<float>* progress) {
    const auto& a = scene.arrangement;
    if (a.count <= 0) return -1;
    const auto from = [&scene](double bars) { return samplesFor(scene, bars); };
    if (file.kind == GameFile::Whole) {
        const auto samples = from(arrangementBars(a));
        return renderArranged(scene, a, pass, 0.0, samples, sink, cancel, progress) ? samples : -1;
    }
    const int i = std::clamp(file.section, 0, a.count - 1);
    const auto& s = a.sections[std::size_t(i)];
    const double start = a.startOf(i);
    if (file.kind == GameFile::Section) {
        double bars = s.endless() ? double(s.bars) : double(s.span());
        if (i == a.count - 1 && a.end == arr::EndStop && a.firstEndless() == a.count) bars += 2.0;
        // Exactly the bars between its positions on the timeline, so files chain sample for sample.
        const auto samples = from(start + bars) - from(start);
        return renderArranged(scene, a, pass, start * beatsPerBar, samples, sink, cancel, progress) ? samples : -1;
    }
    if (file.kind == GameFile::Stinger) {
        auto ring = a;
        ring.count = i + 1;
        ring.end = arr::EndRingOut;
        const double end = start + double(s.endless() ? s.bars : s.span()), sb = stingerBars(s);
        const auto samples = from(end + stingerRingBars) - from(end - sb);
        return renderArranged(scene, ring, pass, (end - sb) * beatsPerBar, samples, sink, cancel, progress) ? samples : -1;
    }
    // Loop: the second pass of the section played endlessly, its start crossfaded from
    // what follows its end.
    auto loop = a;
    loop.sections[std::size_t(i)].repeats = 0;
    const int64_t p0 = from(start), p1 = from(start + s.bars), p2 = from(start + 2.0 * s.bars);
    const int64_t length = p2 - p1;
    // A quarter of a beat.
    const int64_t fade = std::clamp<int64_t>(from(1.0 / 16.0), 1, std::max<int64_t>(length / 2, 1));
    const auto size = static_cast<std::size_t>(length), tail = static_cast<std::size_t>(fade);
    std::vector<float> l(size), r(size), tl(tail), tr(tail);
    int64_t at = 0;
    const int64_t warm = p1 - p0;
    const bool ok = renderArranged(scene, loop, pass, start * beatsPerBar, warm + length + fade,
                                   [&](const float* bl, const float* br, int n) {
                                       for (int k = 0; k < n; ++k, ++at) {
                                           const int64_t x = at - warm;
                                           if (x < 0) continue;
                                           if (x < length) {
                                               l[std::size_t(x)] = bl[k];
                                               r[std::size_t(x)] = br[k];
                                           } else if (x - length < fade) {
                                               tl[std::size_t(x - length)] = bl[k];
                                               tr[std::size_t(x - length)] = br[k];
                                           }
                                       }
                                       return true;
                                   },
                                   cancel, progress);
    if (!ok) return -1;
    for (int64_t k = 0; k < fade; ++k) {
        const float w = 0.5f - 0.5f * std::cos(dsp::pi * float(double(k) / double(fade)));
        l[std::size_t(k)] = tl[std::size_t(k)] * (1.0f - w) + l[std::size_t(k)] * w;
        r[std::size_t(k)] = tr[std::size_t(k)] * (1.0f - w) + r[std::size_t(k)] * w;
    }
    for (int64_t k = 0; k < length; k += 4096) {
        const int n = int(std::min<int64_t>(4096, length - k));
        if (!sink(l.data() + k, r.data() + k, n)) return -1;
    }
    return length;
}

bool renderGameFileToWav(const Scene& scene, const GameFile& game, const Pass& pass, const juce::File& file, std::atomic<bool>* cancel, std::atomic<float>* progress) {
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    bool ok = false;
    {
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream>(file);
        if (!static_cast<juce::FileOutputStream*>(stream.get())->openedOk()) return false;
        const auto options = juce::AudioFormatWriterOptions().withSampleRate(scene.sampleRate).withNumChannels(2).withBitsPerSample(24);
        const auto writer = juce::WavAudioFormat().createWriterFor(stream, options);
        if (writer == nullptr) return false;
        ok = renderGameFile(scene, game, pass,
                            [&](const float* l, const float* r, int n) {
                                const float* channels[] = { l, r };
                                return writer->writeFromFloatArrays(channels, 2, n);
                            },
                            cancel, progress)
             >= 0;
    }
    if (!ok) file.deleteFile();
    return ok;
}

juce::MidiFile arrangementMidi(const Scene& scene, const std::vector<Track>& tracks) {
    const auto& a = scene.arrangement;
    double bars = arrangementBars(a);
    if (a.end == arr::EndStop && a.firstEndless() == a.count) bars -= 2.0;
    const double beats = std::max(bars, 1.0) * beatsPerBar;
    // Every variant of every pattern: edited ones as edited, the others as the Place
    // generates them from bar 1 (at the scene's settings).
    auto playing = scene.controls;
    playing.power = true;
    for (auto& p : playing.places) {
        p.mute = false;
        p.solo = false;
    }
    auto engine = startEngine(scene, playing);
    for (auto& flag : engine->showVariants) flag.store(true);
    std::array<float, dsp::chunk> l {}, r {};
    for (int k = 0; k < 4; ++k) engine->process(l.data(), r.data(), dsp::chunk, playing);

    juce::MidiFile file;
    file.setTicksPerQuarterNote(pat::ticksPerBeat);
    juce::MidiMessageSequence tempo;
    tempo.addEvent(juce::MidiMessage::textMetaEvent(3, "WORLDBUILDER"), 0);
    tempo.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0);
    tempo.addEvent(juce::MidiMessage::tempoMetaEvent(int(60000000.0 / std::clamp(double(scene.bpm), 20.0, 999.0))), 0);
    tempo.addEvent(juce::MidiMessage::endOfTrack(), beats * pat::ticksPerBeat);
    file.addTrack(tempo);
    for (const auto& t : tracks) {
        const int type = scene.controls.places[std::size_t(t.slot)].type;
        const auto& own = scene.patterns[std::size_t(t.slot)];
        juce::MidiMessageSequence track;
        track.addEvent(juce::MidiMessage::textMetaEvent(3, t.name), 0);
        const int channel = t.target.kit ? 10 : 1;
        for (int v = 0; v < pat::variantCount; ++v) {
            const auto& el = own.element[std::size_t(t.element)];
            const pat::Pattern& pattern = own.type == type && el.edited(v) ? el.patternOf(v) : engine->placeSlot(t.slot).generatedPattern(t.element, v);
            if (pattern.count <= 0) continue;
            juce::MidiMessageSequence all;
            pat::appendNotes(all, pattern, t.target, beats, channel);
            all.updateMatchedPairs();
            for (int k = 0; k < all.getNumEvents(); ++k) {
                const auto* ev = all.getEventPointer(k);
                if (!ev->message.isNoteOn()) continue;
                const double beat = ev->message.getTimeStamp() / double(pat::ticksPerBeat);
                const auto st = a.evaluate(beat + 1.0e-6);
                if (st.at.ended) continue;
                const float g = st.gain[std::size_t(t.slot)][std::size_t(t.element)];
                int variant = st.variant[std::size_t(t.slot)][std::size_t(t.element)];
                const bool fill = st.drumFill && g > 0.0f && WorldEngine::isRhythm(type, t.element);
                if (fill) variant = pat::VariantFill;
                // Where the section plays its own clip, the clip's notes are written instead (below).
                if (!fill && st.clip[std::size_t(t.slot)][std::size_t(t.element)] != nullptr) continue;
                if (g < 0.05f || variant != v) continue;
                auto on = ev->message;
                on.setVelocity(juce::jlimit(1.0f / 127.0f, 1.0f, on.getFloatVelocity() * g));
                track.addEvent(on);
                if (ev->noteOffObject != nullptr) track.addEvent(ev->noteOffObject->message);
            }
        }
        // Milestone 16: each section's own clip, every pass, where the section plays it
        // (entries and fades as velocity; a Fill or a Drum fill transition plays instead).
        for (int i = 0; i < a.count; ++i) {
            const auto& sec = a.sections[std::size_t(i)];
            const auto& clip = sec.cells[std::size_t(t.slot)][std::size_t(t.element)].clip;
            if (clip == nullptr || clip->count <= 0) continue;
            const double passBeats = double(sec.bars) * beatsPerBar;
            const int passes = sec.endless() ? 1 : std::max<int>(sec.repeats, 1);
            for (int pass = 0; pass < passes; ++pass) {
                const double from = double(a.startOf(i)) * beatsPerBar + double(pass) * passBeats;
                if (from >= beats - 1.0e-9) break;
                juce::MidiMessageSequence all;
                pat::appendNotes(all, *clip, t.target, std::min(passBeats, beats - from), channel, from);
                all.updateMatchedPairs();
                for (int k = 0; k < all.getNumEvents(); ++k) {
                    const auto* ev = all.getEventPointer(k);
                    if (!ev->message.isNoteOn()) continue;
                    const auto st = a.evaluate(ev->message.getTimeStamp() / double(pat::ticksPerBeat) + 1.0e-6);
                    if (st.at.ended || st.clip[std::size_t(t.slot)][std::size_t(t.element)] != clip.get()) continue;
                    if (st.drumFill && WorldEngine::isRhythm(type, t.element)) continue;
                    const float g = st.gain[std::size_t(t.slot)][std::size_t(t.element)];
                    if (g < 0.05f) continue;
                    auto on = ev->message;
                    on.setVelocity(juce::jlimit(1.0f / 127.0f, 1.0f, on.getFloatVelocity() * g));
                    track.addEvent(on);
                    if (ev->noteOffObject != nullptr) track.addEvent(ev->noteOffObject->message);
                }
            }
        }
        track.sort();
        track.updateMatchedPairs();
        track.addEvent(juce::MidiMessage::endOfTrack(), beats * pat::ticksPerBeat);
        file.addTrack(track);
    }
    return file;
}

} // namespace wb::capture
