#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "WORLDBUILDER", makeLayout()) {
    for (std::size_t i = 0; i < specs.size(); ++i) raw[i] = parameters.getRawParameterValue(specs[i].id);
    patterns.syncTypes(slotTypes());
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
    c.width = value(PWidth);
    c.spaceMix = value(PSpaceMix);
    c.intensity = value(PIntensity);
    c.seed = uint32_t(juce::roundToInt(value(PSeed)));
    c.quality = juce::roundToInt(value(PQuality));
    c.root = juce::roundToInt(value(PRoot));
    c.mood = juce::roundToInt(value(PMood));
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
    c.hostBpm = hostBpm.load(std::memory_order_relaxed);
    c.gate = gateOpen;
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
    patterns.syncTypes(slotTypes());
    for (int slot = 0; slot < placeSlots; ++slot) scene.patterns[std::size_t(slot)] = patterns.get(slot);
    return scene;
}

void WorldbuilderProcessor::prepareToPlay(double sampleRate, int) {
    rate = std::isfinite(sampleRate) ? juce::jlimit(8000.0, 384000.0, sampleRate) : 48000.0;
    travel.prepare(rate);
    engine.prepare(rate, readControls());
    expectedHostPosition = -1;
    hostWasPlaying = false;
}

// Hosts call reset() on transport stops and locates. The world is continuous, so it
// keeps playing; only prepareToPlay starts it afresh.
void WorldbuilderProcessor::reset() {}

bool WorldbuilderProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled()) return false;
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void WorldbuilderProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const auto begin = juce::Time::getHighResolutionTicks();
    const int n = buffer.getNumSamples();
    // MIDI control (Play Mode MIDI, keyboard zones) arrives in Milestone 6.
    midi.clear();
    if (n <= 0 || buffer.getNumChannels() <= 0) return;

    // Follow the host transport while it plays, so the same start point gives the
    // same world. While stopped the world keeps evolving on its own clock.
    // Rhythmic parts of the Places follow the host tempo whenever the host gives one.
    // Play Mode Free: the world always plays, and follows the transport while it rolls.
    // Play Mode Transport: it plays only while the host plays, starting from silence at
    // every start or locate, so the same song position always sounds the same.
    // A host with no transport at all (no tempo, no song position: a test host, the
    // standalone app) cannot start or stop the world, so there it plays freely.
    bool transportMode = juce::roundToInt(raw[PPlayMode]->load(std::memory_order_relaxed)) == PlayTransport;
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
    engine.transportRolling.store(playing, std::memory_order_relaxed);
    gateOpen = !transportMode || playing;
    hostWasPlaying = playing;

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
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    for (int done = 0; done < n;) {
        // The Journey Path follows the song: the beat this piece starts on.
        in.beat = restart && done == 0 ? (restartBeat >= 0 ? restartBeat : double(restartAt) / rate * in.bpm / 60.0) : engine.nextBeat();
        const auto& world = travel.advance(live, livePatterns, in, restart && done == 0);
        const int count = travel.active() ? std::min(n - done, travelPiece) : n - done;
        auto controls = controlsFor(world.v);
        for (int slot = 0; slot < placeSlots; ++slot) {
            controls.places[std::size_t(slot)].travelFar = world.far[std::size_t(slot)];
            controls.patterns[std::size_t(slot)] = world.patterns[std::size_t(slot)];
        }
        if (restart && done == 0) engine.restart(restartAt, restartBeat, controls);
        if (right != nullptr) {
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
        travel.step(count, in, live);
        done += count;
    }
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, n);

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
    std::array<float, 4> kept {};
    const std::array<int, 4> session { PPower, PMaster, PQuality, PPlayMode };
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

juce::AudioProcessorEditor* WorldbuilderProcessor::createEditor() { return new WorldbuilderEditor(*this); }

void WorldbuilderProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    state.setProperty("version", 3, nullptr);
    patterns.syncTypes(slotTypes());
    state.removeChild(state.getChildWithName("Patterns"), nullptr);
    state.appendChild(patterns.toValueTree(), nullptr);
    state.removeChild(state.getChildWithName("Locations"), nullptr);
    auto locationTree = locations.toValueTree();
    locationTree.setProperty("settled", travel.settledDestination.load(), nullptr);
    state.appendChild(locationTree, nullptr);
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
    // Interface settings (the Place overlays: Full, Subtle, Off). Before version 3 every
    // project stored Full, the old default, so those open with the new default (Off).
    const int overlays = int(state.getProperty("version", 0)) >= 3 ? int(state.getProperty("overlays", ui::defaultOverlayMode)) : ui::defaultOverlayMode;
    parameters.state.setProperty("overlays", juce::jlimit(0, ui::OverlayModeCount - 1, overlays), nullptr);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WorldbuilderProcessor(); }
