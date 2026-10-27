#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Samples/SampleBank.h"

using namespace wb;

juce::AudioProcessorValueTreeState::ParameterLayout WorldbuilderProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : specs) {
        const juce::ParameterID id { s.id, 1 };
        switch (s.kind) {
            case Kind::Float: {
                const bool percent = s.max == 1.0f || juce::String(s.id).endsWith("width");
                auto attributes = juce::AudioParameterFloatAttributes()
                    .withLabel(percent ? "%" : "")
                    .withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)); })
                    .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue() / 100.0f; });
                layout.add(std::make_unique<juce::AudioParameterFloat>(id, s.name, juce::NormalisableRange<float>(s.min, s.max), s.def, attributes));
                break;
            }
            case Kind::Decibels: {
                juce::NormalisableRange<float> range(s.min, s.max);
                range.setSkewForCentre(-12.0f);
                auto attributes = juce::AudioParameterFloatAttributes()
                    .withLabel("dB")
                    .withStringFromValueFunction([](float v, int) { return v <= -59.95f ? juce::String("-inf") : juce::String(v, 1); })
                    .withValueFromStringFunction([](const juce::String& t) { return t.trim().startsWith("-inf") ? -60.0f : t.getFloatValue(); });
                layout.add(std::make_unique<juce::AudioParameterFloat>(id, s.name, range, s.def, attributes));
                break;
            }
            case Kind::Bool:
                layout.add(std::make_unique<juce::AudioParameterBool>(id, s.name, s.def > 0.5f));
                break;
            case Kind::Choice:
                layout.add(std::make_unique<juce::AudioParameterChoice>(id, s.name, juce::StringArray::fromTokens(s.choices, "|", ""), int(s.def)));
                break;
            case Kind::Int:
                layout.add(std::make_unique<juce::AudioParameterInt>(id, s.name, int(s.min), int(s.max), int(s.def)));
                break;
            case Kind::Seconds: {
                juce::NormalisableRange<float> range(s.min, s.max);
                range.setSkewForCentre(6.0f);
                auto attributes = juce::AudioParameterFloatAttributes()
                    .withLabel("s")
                    .withStringFromValueFunction([](float v, int) { return juce::String(v, v < 10.0f ? 1 : 0); })
                    .withValueFromStringFunction([](const juce::String& t) { return t.getFloatValue(); });
                layout.add(std::make_unique<juce::AudioParameterFloat>(id, s.name, range, s.def, attributes));
                break;
            }
        }
    }
    return layout;
}

WorldbuilderProcessor::WorldbuilderProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                         // Separate outputs (Milestone 12): one stereo output per Place slot.
                         .withOutput("Place 1", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Place 2", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Place 3", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Place 4", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "WORLDBUILDER", makeLayout()) {
    for (std::size_t i = 0; i < specs.size(); ++i) raw[i] = parameters.getRawParameterValue(specs[i].id);
    patterns.syncTypes(slotTypes());
    // The recorded instruments load in the background, once per process (Milestone 15).
    samples::Bank::get().load();
    startTimerHz(10);
}

std::array<int, placeSlots> WorldbuilderProcessor::slotTypes() const {
    std::array<int, placeSlots> types {};
    for (int slot = 0; slot < placeSlots; ++slot)
        types[std::size_t(slot)] = juce::jlimit(0, PlaceTypeCount - 1, juce::roundToInt(raw[std::size_t(placeParam(slot, PlType))]->load()));
    return types;
}

WorldValues WorldbuilderProcessor::liveValues() const noexcept {
    WorldValues v;
    for (int p = 0; p < PCount; ++p) {
        const float x = raw[std::size_t(p)]->load(std::memory_order_relaxed);
        const auto& s = specs[std::size_t(p)];
        v[std::size_t(p)] = std::isfinite(x) ? juce::jlimit(s.min, s.max, x) : s.def;
    }
    return v;
}

EngineControls WorldbuilderProcessor::readControls() const noexcept { return controlsFor(liveValues()); }

EngineControls WorldbuilderProcessor::controlsFor(const WorldValues& values) const noexcept {
    auto c = controlsFromValues(values);
    c.hostBpm = hostBpm.load(std::memory_order_relaxed);
    c.gate = gateOpen;
    // The Play target (KEYS).
    auto& k = c.keys;
    k.slot = keys.slot.load(std::memory_order_relaxed);
    k.element = keys.element.load(std::memory_order_relaxed);
    k.scale = keys.scale.load(std::memory_order_relaxed);
    k.octave = keys.octave.load(std::memory_order_relaxed);
    k.monitor = keys.monitor.load(std::memory_order_relaxed);
    // The Pattern Editor's transport counts in itself (REC waits for it), then the take
    // starts on the playhead: the host's count-in is for the host's transport only.
    k.single = keys.single.load(std::memory_order_relaxed);
    k.armed = keys.armed.load(std::memory_order_relaxed) && txState != TransportCountIn && !k.single;
    k.countIn = keys.countIn.load(std::memory_order_relaxed) && txState == TransportStopped;
    k.variant = keys.mode.load(std::memory_order_relaxed) == keys::IntoVariant ? keys.variant.load(std::memory_order_relaxed) : 0;
    k.hostPlaying = hostPlayingNow;
    k.punch = transport.punch.load(std::memory_order_relaxed);
    k.punchIn = transport.punchIn.load(std::memory_order_relaxed);
    k.punchOut = transport.punchOut.load(std::memory_order_relaxed);
    k.length = transport.length.load(std::memory_order_relaxed);
    if (k.single) k.monitor = true;   // it is what the track plays
    return c;
}

EngineControls WorldbuilderProcessor::controlsFromValues(const WorldValues& values) noexcept {
    const auto value = [&values](int p) { return values[std::size_t(p)]; };
    EngineControls c;
    for (int w = 0; w < WorldControlCount; ++w) c.world[std::size_t(w)] = value(PTemperature + w);
    for (int l = 0; l < LayerCount; ++l) {
        c.layerDb[std::size_t(l)] = value(layerLevel(l));
        c.mute[std::size_t(l)] = value(layerMute(l)) > 0.5f;
        c.solo[std::size_t(l)] = value(layerSolo(l)) > 0.5f;
    }
    c.power = value(PPower) > 0.5f;
    c.masterDb = value(PMaster);
    c.environmentDb = value(PEnvironment);
    c.width = value(PWidth);
    c.spaceMix = value(PSpaceMix);
    c.intensity = value(PIntensity);
    c.seed = uint32_t(juce::roundToInt(value(PSeed)));
    c.quality = juce::roundToInt(value(PQuality));
    c.root = juce::roundToInt(value(PRoot));
    c.mood = juce::roundToInt(value(PMood));
    c.planet = juce::roundToInt(value(PPlanet));
    c.planetInfluence = value(PPlanetInfluence);
    c.splitOuts = juce::roundToInt(value(PPlaceOuts)) == 1;
    for (int slot = 0; slot < placeSlots; ++slot) {
        auto& pc = c.places[std::size_t(slot)];
        const auto field = [&](int f) { return value(placeParam(slot, f)); };
        pc.type = juce::roundToInt(field(PlType));
        pc.levelDb = field(PlLevel);
        pc.distance = field(PlDistance);
        pc.pan = field(PlPan);
        pc.mute = field(PlMute) > 0.5f;
        pc.solo = field(PlSolo) > 0.5f;
        pc.character = juce::roundToInt(field(PlCharacter));
        pc.activity = field(PlActivity);
        pc.tension = field(PlTension);
        pc.sync = field(PlSync) > 0.5f;
        pc.density = field(PlDensity);
        pc.swing = field(PlSwing);
        pc.tone = field(PlTone);
        pc.size = field(PlSize);
        pc.variation = field(PlVariation);
        pc.seed = uint32_t(juce::roundToInt(field(PlSeed)));
        for (int e = 0; e < placeElements; ++e) {
            pc.element[std::size_t(e)] = field(PlElement + e);
            pc.elementOn[std::size_t(e)] = field(PlElementOn + e) > 0.5f;
            auto& fx = pc.fx[std::size_t(e)];
            fx.reverb = field(fxField(e, FxReverb));
            fx.reverbType = juce::roundToInt(field(fxField(e, FxReverbType)));
            fx.delay = field(fxField(e, FxDelay));
            fx.delayType = juce::roundToInt(field(fxField(e, FxDelayType)));
            fx.character = juce::roundToInt(field(fxField(e, FxCharacter)));
            fx.amount = field(fxField(e, FxAmount));
            fx.tone = field(fxField(e, FxTone));
            fx.width = field(fxField(e, FxWidth));
        }
    }
    return c;
}

capture::Scene WorldbuilderProcessor::captureScene() {
    capture::Scene scene;
    scene.controls = readControls();
    scene.sampleRate = rate;
    const float bpm = hostBpm.load(std::memory_order_relaxed);
    scene.bpm = bpm >= 20.0f && bpm <= 999.0f ? bpm : WorldEngine::defaultBpm;
    scene.controls.hostBpm = scene.bpm;
    scene.controls.gate = true;
    scene.controls.keys = {};   // renders play no keyboard
    patterns.syncTypes(slotTypes());
    for (int slot = 0; slot < placeSlots; ++slot) scene.patterns[std::size_t(slot)] = patterns.get(slot);
    // The arrangement, and what its sections' Locations need (Milestone 11c).
    scene.arrangement = arrangement.get();
    scene.values = liveValues();
    auto all = std::make_shared<std::array<LocationData, locationCount>>();
    for (int i = 0; i < locationCount; ++i) {
        const auto location = locations.get(i);
        auto& data = (*all)[std::size_t(i)];
        data.used = location.used;
        data.values = location.values;
        data.patterns = location.patterns;
    }
    scene.locations = std::move(all);
    scene.travel = travelInputs(scene.values);
    scene.travel.bpm = scene.bpm;
    scene.controlsOf = &WorldbuilderProcessor::controlsFromValues;
    return scene;
}

void WorldbuilderProcessor::prepareToPlay(double sampleRate, int) {
    rate = std::isfinite(sampleRate) ? juce::jlimit(8000.0, 384000.0, sampleRate) : 48000.0;
    // Not on the audio thread: wait (briefly) for the recordings, so the first notes and
    // every render use them.
    samples::Bank::get().waitUntilLoaded(5000);
    travel.prepare(rate);
    engine.prepare(rate, readControls());
    countClick.prepare(float(rate));
    expectedHostPosition = -1;
    hostWasPlaying = false;
    txState = TransportStopped;
    transport.state.store(TransportStopped);
}

// Hosts call reset() on transport stops and locates. The world is continuous, so it
// keeps playing; only prepareToPlay starts it afresh.
void WorldbuilderProcessor::reset() {}

bool WorldbuilderProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled()) return false;
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    // The Place outputs: each stereo, or switched off by the host.
    for (int bus = 1; bus < layouts.outputBuses.size(); ++bus) {
        const auto set = layouts.getChannelSet(false, bus);
        if (set != juce::AudioChannelSet::stereo() && set != juce::AudioChannelSet::disabled()) return false;
    }
    return true;
}

void WorldbuilderProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto begin = juce::Time::getHighResolutionTicks();
    const int n = buffer.getNumSamples();
    if (n <= 0 || buffer.getNumChannels() <= 0) {
        midi.clear();
        return;
    }

    // Follow the host transport while it plays, so the same start point gives the
    // same world. While stopped the world keeps evolving on its own clock.
    // Rhythmic parts of the Places follow the host tempo whenever the host gives one.
    // Play Mode Free: the world always plays, and follows the transport while it rolls.
    // Play Mode Transport: it plays only while the host plays, starting from silence at
    // every start or locate, so the same song position always sounds the same.
    // A host with no transport at all (no tempo, no song position: a test host, the
    // standalone app) cannot start or stop the world, so there it plays freely.
    const int mode = juce::roundToInt(raw[PPlayMode]->load(std::memory_order_relaxed));
    bool transportMode = mode == PlayTransport || mode == PlayArrangement;
    bool hostHasTransport = false;
    bool playing = false;
    bool restart = false;
    uint64_t restartAt = 0;
    double restartBeat = -1;
    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition()) {
            hostHasTransport = position->getBpm().hasValue() || position->getPpqPosition().hasValue();
            transportMode = transportMode && hostHasTransport;
            if (const auto bpm = position->getBpm(); bpm.hasValue() && std::isfinite(*bpm) && *bpm >= 20.0 && *bpm <= 999.0) hostBpm.store(float(*bpm), std::memory_order_relaxed);
            if (position->getIsPlaying())
                if (const auto samples = position->getTimeInSamples()) {
                    playing = true;
                    if (!hostWasPlaying || *samples != expectedHostPosition) {
                        const auto ppq = position->getPpqPosition();
                        const auto at = uint64_t(std::max<int64_t>(0, *samples));
                        const double beat = ppq.hasValue() && std::isfinite(*ppq) && *ppq >= 0 ? *ppq : -1.0;
                        if (transportMode) {
                            // The world restarts below, once the Locations have said where it is.
                            restart = true;
                            restartAt = at;
                            restartBeat = beat;
                        } else {
                            engine.resync(at, beat);
                        }
                    }
                    expectedHostPosition = *samples + n;
                }
        }
    if (!hostHasTransport) transportMode = false;
    // The ARRANGE preview: while the host is stopped, the arrangement plays from the
    // chosen bar on the world's own clock. The host starting playback ends it.
    if (playing) {
        previewOn.store(false, std::memory_order_relaxed);
        previewSeen = previewSerial.load(std::memory_order_acquire);
    } else if (const auto serial = previewSerial.load(std::memory_order_acquire); serial != previewSeen) {
        previewSeen = serial;
        if (previewOn.load(std::memory_order_relaxed)) {
            const float bpm = hostBpm.load(std::memory_order_relaxed);
            const double tempo = bpm >= 20.0f && bpm <= 999.0f ? double(bpm) : double(WorldEngine::defaultBpm);
            restart = true;
            restartBeat = double(std::max(0, previewBar.load(std::memory_order_relaxed)) * arr::beatsPerBar);
            restartAt = uint64_t(restartBeat * 60.0 / tempo * rate);
        }
    }
    // The Pattern Editor's transport: Play, Stop and locates from the editor; the host
    // starting playback ends it (the editor then follows the host).
    const float tempoNow = hostBpm.load(std::memory_order_relaxed);
    const double tempo = tempoNow >= 20.0f && tempoNow <= 999.0f ? double(tempoNow) : double(WorldEngine::defaultBpm);
    const double perSample = tempo / 60.0 / rate;
    const auto samplesFor = [&](double beats) { return uint64_t(std::max(0.0, beats) * 60.0 / tempo * rate); };
    int restartOffset = 0;
    bool restartResync = false;   // Play Mode Free: the world plays on from the new position (no restart from silence)
    if (playing) {
        if (txState != TransportStopped) transport.wanted.store(false, std::memory_order_relaxed);
        txState = TransportStopped;
        txSeen = transport.serial.load(std::memory_order_acquire);
    } else if (const auto serial = transport.serial.load(std::memory_order_acquire); serial != txSeen) {
        txSeen = serial;
        if (transport.wanted.load(std::memory_order_relaxed)) {
            txFrom = transport.playhead.load(std::memory_order_relaxed);
            previewOn.store(false, std::memory_order_relaxed);
            if (keys.armed.load(std::memory_order_relaxed) && keys.countIn.load(std::memory_order_relaxed)) {
                // A bar of clicks first; the transport starts on the playhead after it.
                txState = TransportCountIn;
                txCountBeat = 0;
                countClick.reset();
            } else {
                txState = TransportRolling;
                restart = true;
                restartResync = !transportMode;
                restartBeat = txFrom;
                restartAt = samplesFor(txFrom);
            }
        } else {
            txState = TransportStopped;
        }
    }
    const bool previewing = !playing && previewOn.load(std::memory_order_relaxed);
    const auto editorRolling = [&] { return !playing && txState == TransportRolling; };
    engine.transportRolling.store(playing || previewing || editorRolling(), std::memory_order_relaxed);
    gateOpen = !transportMode || playing || previewing || editorRolling();
    hostWasPlaying = playing;
    hostRollingOut.store(playing, std::memory_order_relaxed);
    hostPlayingNow = playing || editorRolling();
    // The host's MIDI: the keyboard (KEYS). With "Host regions play the target" off, notes
    // arriving while the host plays (a recorded region playing back) are left out unless
    // REC is armed (then they are the take).
    int keyCount = 0;
    {
        const bool armed = keys.armed.load(std::memory_order_relaxed);
        const bool regions = keys.hostRegions.load(std::memory_order_relaxed);
        // Single-instrument mode: the track's regions are the part, so they always play.
        const bool notes = keys.single.load(std::memory_order_relaxed) || !(playing && !armed && !regions);
        for (const auto meta : midi) {
            if (keyCount >= int(keyEvents.size())) break;
            const auto m = meta.getMessage();
            keys::Event e;
            e.offset = juce::jlimit(0, n - 1, meta.samplePosition);
            if (m.isNoteOn()) {
                if (!notes) continue;
                e.kind = keys::Event::NoteOn;
                e.note = uint8_t(m.getNoteNumber());
                e.value = float(m.getVelocity()) / 127.0f;
            } else if (m.isNoteOff()) {
                e.kind = keys::Event::NoteOff;
                e.note = uint8_t(m.getNoteNumber());
            } else if (m.isSustainPedalOn() || m.isSustainPedalOff()) {
                e.kind = keys::Event::Sustain;
                e.value = m.isSustainPedalOn() ? 1.0f : 0.0f;
            } else if (m.isPitchWheel()) {
                e.kind = keys::Event::Bend;
                e.value = float(m.getPitchWheelValue() - 8192) / 8192.0f;
            } else if (m.isController() && m.getControllerNumber() == 1) {
                e.kind = keys::Event::Mod;
                e.value = float(m.getControllerValue()) / 127.0f;
            } else if (m.isAllNotesOff() || m.isAllSoundOff()) {
                e.kind = keys::Event::AllOff;
            } else {
                continue;
            }
            keyEvents[std::size_t(keyCount++)] = e;
        }
        midi.clear();
    }
    // Play Mode Arrangement (with the host playing) and the preview follow the arrangement.
    engine.arrangementIn.update();
    // (A single-instrument instance plays its instrument whatever the arrangement says.)
    const bool soloInstrument = keys.single.load(std::memory_order_relaxed);
    const arr::Arrangement* arranged = !soloInstrument && ((transportMode && mode == PlayArrangement && playing) || previewing) ? &engine.arrangementIn.read() : nullptr;
    // Its sections' Locations: while the host plays or is stopped in Arrangement mode
    // (the world stays where the song stopped), and during the preview.
    const arr::Arrangement* located = !soloInstrument && ((transportMode && mode == PlayArrangement) || previewing) ? &engine.arrangementIn.read() : nullptr;
    if (located != nullptr && !located->usesLocations()) located = nullptr;

    // The world: the parameters, or a journey between Locations. While one is under way
    // the block is rendered in short pieces so it glides.
    const auto live = liveValues();
    auto in = travelInputs(live);
    std::array<const pat::SlotPatterns*, placeSlots> livePatterns {};
    for (int slot = 0; slot < placeSlots; ++slot) {
        engine.patternsIn[std::size_t(slot)].update();
        livePatterns[std::size_t(slot)] = &engine.patternsIn[std::size_t(slot)].read();
    }
    float* left = buffer.getWritePointer(0);
    const int mainChannels = getBusCount(false) > 0 ? getChannelCountOfBus(false, 0) : buffer.getNumChannels();
    float* right = buffer.getNumChannels() > 1 && mainChannels > 1 ? buffer.getWritePointer(1) : nullptr;
    // The Place outputs the host has (Logic's multi-output instrument; a host with only
    // the main output, like GarageBand, has none, and the main keeps the full mix).
    std::array<float*, 2 * placeSlots> slotOut {};
    std::array<bool, placeSlots> hasOut {};
    bool anyOut = false;
    for (int slot = 0; slot < placeSlots && right != nullptr; ++slot) {
        const int bus = slot + 1;
        if (bus >= getBusCount(false) || !getBus(false, bus)->isEnabled() || getChannelCountOfBus(false, bus) != 2) continue;
        const int first = getChannelIndexInProcessBlockBuffer(false, bus, 0);
        if (first + 1 >= buffer.getNumChannels()) continue;
        slotOut[std::size_t(2 * slot)] = buffer.getWritePointer(first);
        slotOut[std::size_t(2 * slot + 1)] = buffer.getWritePointer(first + 1);
        hasOut[std::size_t(slot)] = anyOut = true;
    }
    for (int done = 0; done < n;) {
        // The editor's transport: the count-in ends, or the loop goes round, on its sample.
        int limit = n - done;
        if (txState == TransportCountIn) {
            const auto left = int64_t(std::ceil((countInBeats - txCountBeat) / perSample - 1.0e-6));
            if (left <= 0) {
                txState = TransportRolling;
                restart = true;
                restartOffset = done;
                restartResync = !transportMode;
                restartBeat = txFrom;
                restartAt = samplesFor(txFrom);
                gateOpen = true;
                hostPlayingNow = true;
                engine.transportRolling.store(true, std::memory_order_relaxed);
            } else {
                limit = int(std::min<int64_t>(limit, left));
            }
        }
        const bool restartHere = restart && done == restartOffset;
        if (txState == TransportRolling && !restartHere && transport.loop.load(std::memory_order_relaxed)) {
            const double from = transport.loopStart.load(std::memory_order_relaxed), to = transport.loopEnd.load(std::memory_order_relaxed);
            if (to > from + 1.0e-6) {
                const double at = engine.outputBeat();
                if (at >= to - 1.0e-9) {
                    // Round the loop: the world plays on from its start (its tails ring on).
                    engine.resync(samplesFor(from), from);
                } else {
                    limit = std::min(limit, std::max(1, int(std::ceil((to - at) / perSample - 1.0e-6))));
                }
            }
        }
        // The Journey Path follows the song: the beat this piece starts on.
        in.beat = restartHere ? (restartBeat >= 0 ? restartBeat : double(restartAt) / rate * in.bpm / 60.0) : engine.nextBeat();
        if (located != nullptr) {
            // The journey to the next section's Location takes the Travel Time and arrives
            // on its first downbeat.
            const double travelBeats = in.syncBars > 0 ? 4.0 * double(in.syncBars) : double(in.seconds) * in.bpm / 60.0;
            float position = 0;
            located->locationAt(in.beat, travelBeats, in.from, in.to, position);
            in.journey = position;
            in.arranged = in.journeyOn = true;
            in.pathOn = false;
        }
        const auto& world = travel.advance(live, livePatterns, in, restartHere);
        const int count = std::min(limit, travel.active() ? std::min(n - done, travelPiece) : n - done);
        auto controls = controlsFor(world.v);
        controls.arrangement = arranged;
        for (int slot = 0; slot < placeSlots; ++slot) {
            controls.places[std::size_t(slot)].travelFar = world.far[std::size_t(slot)];
            controls.patterns[std::size_t(slot)] = world.patterns[std::size_t(slot)];
        }
        controls.slotOut = hasOut;
        if (restartHere) {
            if (restartResync) engine.resync(restartAt, restartBeat);
            else engine.restart(restartAt, restartBeat, controls);
        }
        if (done == 0) engine.keysBlock(keyEvents.data(), keyCount, controls);
        if (right != nullptr && anyOut) {
            std::array<float*, 2 * placeSlots> outs {};
            for (std::size_t c = 0; c < outs.size(); ++c) outs[c] = slotOut[c] != nullptr ? slotOut[c] + done : nullptr;
            engine.process(left + done, right + done, count, controls, outs.data());
        } else if (right != nullptr) {
            engine.process(left + done, right + done, count, controls);
        } else {
            // Mono output: render stereo in chunks through a small stack buffer.
            std::array<float, 256> scratch {};
            for (int start = done; start < done + count; start += int(scratch.size())) {
                const int part = std::min(int(scratch.size()), done + count - start);
                engine.process(left + start, scratch.data(), part, controls);
                for (int i = 0; i < part; ++i) left[start + i] = 0.5f * (left[start + i] + scratch[std::size_t(i)]);
            }
        }
        if (txState == TransportCountIn) {
            // The count-in's clicks, over whatever the world plays meanwhile.
            for (int start = 0; start < count; start += int(clickScratch.size())) {
                const int part = std::min(int(clickScratch.size()), count - start);
                std::fill_n(clickScratch.begin(), part, 0.0f);
                std::fill_n(clickScratchR.begin(), part, 0.0f);
                countClick.render(clickScratch.data(), clickScratchR.data(), part, txCountBeat, perSample, true);
                txCountBeat += perSample * double(part);
                for (int i = 0; i < part; ++i) {
                    left[done + start + i] += clickScratch[std::size_t(i)];
                    if (right != nullptr) right[done + start + i] += clickScratchR[std::size_t(i)];
                }
            }
        }
        travel.step(count, in, live);
        done += count;
    }
    transport.state.store(txState, std::memory_order_relaxed);
    transport.countLeft.store(txState == TransportCountIn ? float(std::max(0.0, countInBeats - txCountBeat)) : 0.0f, std::memory_order_relaxed);
    if (txState == TransportRolling) transport.beat.store(engine.outputBeat(), std::memory_order_relaxed);
    // Channels nothing wrote (a mono main's second channel, Place outputs not in use).
    for (int ch = std::max(1, mainChannels); ch < buffer.getNumChannels(); ++ch) {
        bool written = false;
        for (const float* p : slotOut) written |= p != nullptr && p == buffer.getReadPointer(ch);
        if (!written) buffer.clear(ch, 0, n);
    }

    const auto seconds = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin);
    cpuLoad.store(0.9f * cpuLoad.load() + 0.1f * float(seconds * rate / n), std::memory_order_relaxed);
}

TravelInputs WorldbuilderProcessor::travelInputs(const WorldValues& v) const noexcept {
    TravelInputs in;
    in.destination = juce::roundToInt(v[PDestination]);
    in.seconds = v[PTravelTime];
    in.syncBars = travelSyncBars[std::size_t(juce::jlimit(0, int(travelSyncBars.size()) - 1, juce::roundToInt(v[PTravelSync])))];
    in.pathOn = v[PPathOn] > 0.5f;
    in.journeyOn = v[PJourneyOn] > 0.5f || in.pathOn;
    in.pathBars = pathLegBars[std::size_t(juce::jlimit(0, int(pathLegBars.size()) - 1, juce::roundToInt(v[PPathLeg])))];
    in.pathLoop = juce::roundToInt(v[PPathEnd]) == 1;
    in.from = juce::roundToInt(v[PJourneyFrom]);
    in.to = juce::roundToInt(v[PJourneyTo]);
    in.journey = v[PJourney];
    const float bpm = hostBpm.load(std::memory_order_relaxed);
    in.bpm = bpm >= 20.0f && bpm <= 999.0f ? double(bpm) : double(WorldEngine::defaultBpm);
    return in;
}

void WorldbuilderProcessor::transportPlay() {
    if (hostRolling()) return;   // the host plays: the editor follows it
    stopPreview();
    // Looping: Play starts inside the loop region.
    if (transport.loop.load()) {
        const double from = transport.loopStart.load(), to = transport.loopEnd.load(), at = transport.playhead.load();
        if (to > from && (at < from || at >= to)) transport.playhead.store(from);
    }
    transport.wanted.store(true);
    transport.serial.fetch_add(1, std::memory_order_acq_rel);
}

void WorldbuilderProcessor::transportStop() {
    if (transport.state.load() == TransportStopped && !transport.wanted.load()) {
        // Stopped already: back to the start.
        transport.playhead.store(transport.loop.load() ? transport.loopStart.load() : 0.0);
    }
    if (keys.armed.load()) setArmed(false);
    transport.wanted.store(false);
    transport.serial.fetch_add(1, std::memory_order_acq_rel);
}

void WorldbuilderProcessor::transportLocate(double beat) {
    transport.playhead.store(std::max(0.0, beat));
    // Rolling: it plays on from there.
    if (transport.wanted.load() && !hostRolling()) transport.serial.fetch_add(1, std::memory_order_acq_rel);
}

void WorldbuilderProcessor::previewFrom(int bar) {
    // The preview and the Pattern Editor's transport share the world's clock: one at a time.
    if (transport.wanted.exchange(false)) transport.serial.fetch_add(1, std::memory_order_acq_rel);
    previewBar.store(std::max(0, bar), std::memory_order_relaxed);
    previewOn.store(true, std::memory_order_relaxed);
    previewSerial.fetch_add(1, std::memory_order_acq_rel);
}

void WorldbuilderProcessor::stopPreview() { previewOn.store(false, std::memory_order_relaxed); }

void WorldbuilderProcessor::saveLocation(int index) {
    patterns.syncTypes(slotTypes());
    std::array<pat::SlotPatterns, locationSlots> slots;
    for (int slot = 0; slot < placeSlots; ++slot) slots[std::size_t(slot)] = patterns.get(slot);
    locations.save(index, liveValues(), slots);
}

void WorldbuilderProcessor::travelTo(int index) {
    if (index < 0 || index >= locationCount || !locations.used(index)) return;
    auto* destination = parameters.getParameter(specs[PDestination].id);
    if (juce::roundToInt(raw[PDestination]->load()) != index + 1) {
        destination->beginChangeGesture();
        destination->setValueNotifyingHost(destination->convertTo0to1(float(index + 1)));
        destination->endChangeGesture();
    } else {
        travel.request(index);
    }
}

void WorldbuilderProcessor::serviceLocations() {
    // A world arrived: write it into the parameters and the patterns, then let the
    // parameters take over again.
    travel.arrivalOut.update();
    const auto& arrival = travel.arrivalOut.read();
    if (arrival.serial == 0 || arrival.serial == appliedArrival) return;
    appliedArrival = arrival.serial;
    if (arrival.generation != travel.generation()) return;   // from before a load
    for (int p = 0; p < PCount; ++p) {
        if (moveOf(p) == Move::Live) continue;
        auto* parameter = parameters.getParameter(specs[std::size_t(p)].id);
        const float value = arrival.values[std::size_t(p)];
        if (std::abs(raw[std::size_t(p)]->load() - value) > 1.0e-6f * std::max(1.0f, std::abs(value)))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }
    const auto types = slotTypes();
    for (int slot = 0; slot < placeSlots; ++slot) {
        const auto& p = arrival.patterns[std::size_t(slot)];
        if (p.type == types[std::size_t(slot)]) patterns.set(slot, p);
    }
    patterns.syncTypes(types);
    travel.released.store(arrival.serial, std::memory_order_release);
}

juce::String WorldbuilderProcessor::exportWorld() {
    juce::MemoryBlock state;
    getStateInformation(state);
    if (const auto xml = getXmlFromBinary(state.getData(), int(state.getSize()))) {
        xml->setAttribute("worldFile", 1);
        return xml->toString();
    }
    return {};
}

bool WorldbuilderProcessor::importWorld(const juce::String& text) {
    const auto xml = juce::parseXML(text);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return false;
    // The session's own settings stay as they are.
    std::array<float, 5> kept {};
    const std::array<int, 5> session { PPower, PMaster, PQuality, PPlayMode, PPlaceOuts };
    for (std::size_t i = 0; i < session.size(); ++i) kept[i] = raw[std::size_t(session[i])]->load();
    juce::MemoryBlock block;
    copyXmlToBinary(*xml, block);
    setStateInformation(block.getData(), int(block.getSize()));
    for (std::size_t i = 0; i < session.size(); ++i) {
        auto* parameter = parameters.getParameter(specs[std::size_t(session[i])].id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(kept[i]));
    }
    return true;
}

void WorldbuilderProcessor::setValue(int p, float value) {
    auto* parameter = parameters.getParameter(specs[std::size_t(p)].id);
    const float normalised = parameter->convertTo0to1(value);
    if (std::abs(parameter->getValue() - normalised) < 1.0e-7f) return;
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(normalised);
    parameter->endChangeGesture();
}

void WorldbuilderProcessor::loadScene(const scenes::Scene& scene) {
    sceneUndo = exportWorld();
    auto session = liveValues();
    if (juce::roundToInt(session[PPlayMode]) != PlayFree) session[PPlayMode] = float(PlayArrangement);
    const auto world = scenes::worldValues(scene, session);
    // No journey under way or waiting: the scene's Locations are reached by its arrangement.
    for (int p = 0; p < PCount; ++p) setValue(p, world[std::size_t(p)]);
    patterns.syncTypes(slotTypes());
    std::array<pat::SlotPatterns, locationSlots> fresh {};
    for (int slot = 0; slot < placeSlots; ++slot) {
        fresh[std::size_t(slot)] = PatternStore::defaultsFor(slotTypes()[std::size_t(slot)]);
        patterns.set(slot, fresh[std::size_t(slot)]);
    }
    for (int i = 0; i < locationCount; ++i) {
        if (i >= scene.locationCount) {
            locations.clear(i);
            continue;
        }
        const auto& l = scene.locations[std::size_t(i)];
        locations.save(i, scenes::locationValues(scene, i, world), fresh);
        locations.rename(i, l.name);
        locations.setColour(i, l.colour);
    }
    arrangement.set(scenes::arrangement(scene), true);
    arrangementSeen = arrangement.get().count;
    parameters.state.setProperty("scene", juce::String(scene.name), nullptr);
}

bool WorldbuilderProcessor::loadFactoryScene(int index) {
    if (index < 0 || index >= scenes::factoryCount()) return false;
    loadScene(scenes::factory(index));
    return true;
}

void WorldbuilderProcessor::inspire() {
    const auto seed = scenes::nextSeed(uint32_t(juce::roundToInt(raw[PSeed]->load())));
    // The composer's own arrangement stays (UNDO brings the whole world back anyway);
    // with none, Inspire's scene brings one.
    const auto kept = arrangement.get();
    loadScene(scenes::inspire(seed));
    if (kept.count > 0) {
        arrangement.set(kept, false);
        if (juce::roundToInt(raw[PPlayMode]->load()) != PlayFree) setValue(PPlayMode, float(PlayArrangement));
    }
    arrangementSeen = arrangement.get().count;
}

keys::RecordSettings WorldbuilderProcessor::recordSettings() const {
    keys::RecordSettings r;
    r.mode = keys.mode.load();
    r.variant = keys.variant.load();
    r.quantise = keys.quantise.load();
    r.strength = keys.strength.load();
    r.autoLength = keys.autoLength.load();
    r.punch = transport.punch.load();
    r.punchIn = transport.punchIn.load();
    r.punchOut = transport.punchOut.load();
    return r;
}

void WorldbuilderProcessor::serviceKeys() {
    recorder.service(recordSettings(), juce::roundToInt(raw[PMood]->load()));
    // A take recorded with the host rolling: the host has most likely recorded the same
    // notes as a MIDI region, which would play the instrument again on top of the
    // pattern. Host regions stop playing the target (the editor says so; one click
    // turns it back on).
    if (recorder.takeWithTransportEnded() && keys.hostRegions.load()) {
        keys.hostRegions.store(false);
        keys.regionsNotice = true;
    }
}

void WorldbuilderProcessor::setArmed(bool on) {
    if (on && keys.single.load()) return;
    keys.armed.store(on);
    if (!on) serviceKeys();
}

void WorldbuilderProcessor::setSingle(bool on) {
    if (on) {
        setArmed(false);
        if (transport.wanted.load()) transportStop();
    }
    keys.single.store(on && keys.slot.load() >= 0);
}

juce::String WorldbuilderProcessor::singleName() const {
    const int slot = keys.slot.load();
    if (slot < 0 || slot >= placeSlots) return {};
    const int type = slotTypes()[std::size_t(slot)];
    if (type == NoPlace || patternInfo(type, keys.element.load()).kind == NoPattern) return {};
    return capture::trackName(type, keys.element.load());
}

void WorldbuilderProcessor::serviceArrangement() {
    const int count = arrangement.get().count;
    const int mode = juce::roundToInt(raw[PPlayMode]->load());
    if (arrangementSeen == 0 && count > 0 && mode != PlayArrangement) {
        modeBeforeArrangement = float(mode);
        setValue(PPlayMode, float(PlayArrangement));
    }
    arrangementSeen = count;
}

bool WorldbuilderProcessor::arrangementOn() const {
    return arrangement.get().count > 0 && juce::roundToInt(raw[PPlayMode]->load()) == PlayArrangement;
}

void WorldbuilderProcessor::setArrangementOn(bool on) {
    const int mode = juce::roundToInt(raw[PPlayMode]->load());
    if (on && mode != PlayArrangement) {
        modeBeforeArrangement = float(mode);
        setValue(PPlayMode, float(PlayArrangement));
    } else if (!on && mode == PlayArrangement) {
        setValue(PPlayMode, juce::roundToInt(modeBeforeArrangement) == PlayArrangement ? float(PlayTransport) : modeBeforeArrangement);
    }
}

bool WorldbuilderProcessor::undoScene() {
    if (sceneUndo.isEmpty()) return false;
    const auto text = sceneUndo;
    sceneUndo.clear();
    // The Play Mode the scene chose goes back too.
    const float mode = raw[PPlayMode]->load();
    if (!importWorld(text)) return false;
    if (const auto xml = juce::parseXML(text))
        for (auto* child : xml->getChildWithTagNameIterator("PARAM"))
            if (child->getStringAttribute("id") == specs[PPlayMode].id) {
                setValue(PPlayMode, float(child->getDoubleAttribute("value", mode)));
                break;
            }
    return true;
}

juce::String WorldbuilderProcessor::sceneName() const { return parameters.state.getProperty("scene").toString(); }

int WorldbuilderProcessor::factoryIndex() const {
    const auto name = sceneName();
    for (int i = 0; i < scenes::factoryCount(); ++i)
        if (name == juce::String(scenes::factory(i).name)) return i;
    return -1;
}

juce::AudioProcessorEditor* WorldbuilderProcessor::createEditor() { return new WorldbuilderEditor(*this); }

void WorldbuilderProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    state.setProperty("version", 4, nullptr);
    // How many recorded notes this instance plays (information for diagnostics and the AU
    // host harness; never read back).
    state.setProperty("samples", samples::Bank::get().zoneCount(), nullptr);
    patterns.syncTypes(slotTypes());
    state.removeChild(state.getChildWithName("Patterns"), nullptr);
    state.appendChild(patterns.toValueTree(), nullptr);
    state.removeChild(state.getChildWithName("Locations"), nullptr);
    auto locationTree = locations.toValueTree();
    locationTree.setProperty("settled", travel.settledDestination.load(), nullptr);
    state.appendChild(locationTree, nullptr);
    state.removeChild(state.getChildWithName("Arrangement"), nullptr);
    state.appendChild(arrangement.toValueTree(), nullptr);
    // KEYS: the Play target and REC's settings (not its arm).
    state.removeChild(state.getChildWithName("Keys"), nullptr);
    {
        juce::ValueTree k("Keys");
        k.setProperty("slot", keys.slot.load(), nullptr);
        k.setProperty("element", keys.element.load(), nullptr);
        k.setProperty("scale", keys.scale.load(), nullptr);
        k.setProperty("octave", keys.octave.load(), nullptr);
        k.setProperty("monitor", keys.monitor.load(), nullptr);
        k.setProperty("regions", keys.hostRegions.load(), nullptr);
        k.setProperty("countin", keys.countIn.load(), nullptr);
        k.setProperty("auto", keys.autoLength.load(), nullptr);
        k.setProperty("mode", keys.mode.load(), nullptr);
        k.setProperty("variant", keys.variant.load(), nullptr);
        k.setProperty("quantise", keys.quantise.load(), nullptr);
        k.setProperty("strength", keys.strength.load(), nullptr);
        k.setProperty("tip", keys.tipShown, nullptr);
        k.setProperty("single", keys.single.load(), nullptr);
        // The Pattern Editor's transport: loop and punch.
        k.setProperty("loop", transport.loop.load(), nullptr);
        k.setProperty("loopset", transport.loopSet.load(), nullptr);
        k.setProperty("loopstart", transport.loopStart.load(), nullptr);
        k.setProperty("loopend", transport.loopEnd.load(), nullptr);
        k.setProperty("punch", transport.punch.load(), nullptr);
        k.setProperty("punchin", transport.punchIn.load(), nullptr);
        k.setProperty("punchout", transport.punchOut.load(), nullptr);
        k.setProperty("playhead", transport.playhead.load(), nullptr);
        state.appendChild(k, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

namespace {
// The default of parameter p in a project saved without it: an instrument effect takes
// the default for the Place stored in its slot.
float legacyDefault(const juce::ValueTree& state, int p, float fallback) {
    if (p < PPlaceFirst || p >= PPlaceFirst + placeSlotCount * PlaceParamCount) return fallback;
    const int slot = (p - PPlaceFirst) / PlaceParamCount, field = (p - PPlaceFirst) % PlaceParamCount;
    if (field < PlFx) return fallback;
    const auto typeChild = state.getChildWithProperty("id", juce::String(specs[std::size_t(placeParam(slot, PlType))].id));
    const int type = typeChild.isValid() ? juce::jlimit(0, PlaceTypeCount - 1, juce::roundToInt(float(typeChild.getProperty("value")))) : 0;
    return instrumentFxDefault(type, (field - PlFx) / FxFieldCount, (field - PlFx) % FxFieldCount);
}
} // namespace

void WorldbuilderProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0 || sizeInBytes > 4 * 1024 * 1024) return;
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName(parameters.state.getType())) return;
    const auto state = juce::ValueTree::fromXml(*xml);
    // Apply known parameters one by one, clamped; anything missing keeps its default.
    for (const auto& s : specs) {
        auto* parameter = parameters.getParameter(s.id);
        if (parameter == nullptr) continue;
        float value = s.def;
        const auto child = state.getChildWithProperty("id", juce::String(s.id));
        // Projects from before Play Mode keep playing as they did (Free).
        if (!child.isValid() && juce::String(s.id) == "playmode") value = float(PlayFree);
        // Projects from before the Planet keep their sound: no planet colour.
        if (!child.isValid() && juce::String(s.id) == "planetinfluence") value = 0.0f;
        // Projects from before the instrument effects: each instrument starts from its
        // Place's own effect defaults.
        if (!child.isValid()) value = legacyDefault(state, int(&s - specs.data()), value);
        if (child.isValid() && child.hasProperty("value")) {
            const float stored = float(child.getProperty("value"));
            if (std::isfinite(stored)) value = juce::jlimit(s.min, s.max, stored);
        }
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }
    // Patterns (Milestone 4 and later); older projects have none and keep the generated ones.
    patterns.fromValueTree(state.getChildWithName("Patterns"), slotTypes());
    // Locations (Milestone 9). The world loaded is settled for the Destination it was
    // saved with, so loading never starts a journey.
    const auto locationTree = state.getChildWithName("Locations");
    locations.fromValueTree(locationTree);
    travel.settledDestination.store(int(locationTree.getProperty("settled", juce::roundToInt(raw[PDestination]->load()))), std::memory_order_relaxed);
    travel.requestReset();
    // The arrangement (Milestone 11); older projects have none.
    arrangement.fromValueTree(state.getChildWithName("Arrangement"));
    // Projects saved before version 4 could hold an arrangement that never played: Play
    // Mode stayed Transport after it was built. They open playing it; later projects keep
    // the Play Mode they were saved with.
    arrangementSeen = arrangement.get().count;
    if (int(state.getProperty("version", 0)) < 4 && arrangementSeen > 0 && juce::roundToInt(raw[PPlayMode]->load()) == PlayTransport) {
        modeBeforeArrangement = float(PlayTransport);
        parameters.getParameter(specs[PPlayMode].id)->setValueNotifyingHost(parameters.getParameter(specs[PPlayMode].id)->convertTo0to1(float(PlayArrangement)));
    }
    // KEYS (Milestone 14); older projects have no Play target (incoming notes do nothing).
    {
        const auto k = state.getChildWithName("Keys");
        const auto number = [&](const char* id, int fallback, int lo, int hi) { return juce::jlimit(lo, hi, int(k.getProperty(id, fallback))); };
        keys.slot.store(number("slot", -1, -1, placeSlots - 1));
        keys.element.store(number("element", 0, 0, placeElements - 1));
        keys.scale.store(number("scale", keys::Chromatic, 0, keys::ScaleModeCount - 1));
        keys.octave.store(number("octave", 0, keys::octaveMin, keys::octaveMax));
        keys.monitor.store(bool(k.getProperty("monitor", true)));
        keys.hostRegions.store(bool(k.getProperty("regions", true)));
        keys.countIn.store(bool(k.getProperty("countin", false)));
        keys.autoLength.store(bool(k.getProperty("auto", false)));
        keys.mode.store(number("mode", keys::Replace, 0, keys::RecordModeCount - 1));
        keys.variant.store(number("variant", 0, 0, pat::variantCount - 1));
        keys.quantise.store(number("quantise", keys::QuantiseOff, 0, keys::QuantiseCount - 1));
        const float strength = float(double(k.getProperty("strength", 1.0)));
        keys.strength.store(std::isfinite(strength) ? juce::jlimit(0.0f, 1.0f, strength) : 1.0f);
        keys.tipShown = bool(k.getProperty("tip", false));
        keys.single.store(bool(k.getProperty("single", false)) && keys.slot.load() >= 0);
        keys.armed.store(false);
        const auto beats = [&](const char* id, double fallback) {
            const double v = double(k.getProperty(id, fallback));
            return std::isfinite(v) ? juce::jlimit(0.0, 4.0 * pat::maxBars, v) : fallback;
        };
        transport.loop.store(bool(k.getProperty("loop", true)));
        transport.loopSet.store(bool(k.getProperty("loopset", false)));
        transport.loopStart.store(beats("loopstart", 0.0));
        transport.loopEnd.store(std::max(transport.loopStart.load() + 0.25, beats("loopend", 16.0)));
        transport.punch.store(bool(k.getProperty("punch", false)));
        transport.punchIn.store(beats("punchin", 0.0));
        transport.punchOut.store(std::max(transport.punchIn.load() + 0.25, beats("punchout", 16.0)));
        transport.playhead.store(beats("playhead", 0.0));
    }
    // Interface settings (the Place overlays: Full, Subtle, Off). Before version 3 every
    // project stored Full, the old default, so those open with the new default (Off).
    const int overlays = int(state.getProperty("version", 0)) >= 3 ? int(state.getProperty("overlays", ui::defaultOverlayMode)) : ui::defaultOverlayMode;
    parameters.state.setProperty("overlays", juce::jlimit(0, ui::OverlayModeCount - 1, overlays), nullptr);
    // The scene loaded last (Milestone 12), for the scene browser.
    parameters.state.setProperty("scene", state.getProperty("scene").toString(), nullptr);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WorldbuilderProcessor(); }
