#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace wb;

juce::AudioProcessorValueTreeState::ParameterLayout WorldbuilderProcessor::makeLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const auto& s : specs) {
        const juce::ParameterID id { s.id, 1 };
        switch (s.kind) {
            case Kind::Float: {
                const bool percent = s.max == 1.0f || juce::String(s.id) == "width";
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
        }
    }
    return layout;
}

WorldbuilderProcessor::WorldbuilderProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "WORLDBUILDER", makeLayout()) {
    for (std::size_t i = 0; i < specs.size(); ++i) raw[i] = parameters.getRawParameterValue(specs[i].id);
    patterns.syncTypes(slotTypes());
    startTimerHz(5);
}

std::array<int, placeSlots> WorldbuilderProcessor::slotTypes() const {
    std::array<int, placeSlots> types {};
    for (int slot = 0; slot < placeSlots; ++slot)
        types[std::size_t(slot)] = juce::jlimit(0, PlaceTypeCount - 1, juce::roundToInt(raw[std::size_t(placeParam(slot, PlType))]->load()));
    return types;
}

EngineControls WorldbuilderProcessor::readControls() const noexcept {
    const auto value = [this](int p) {
        const float v = raw[std::size_t(p)]->load(std::memory_order_relaxed);
        const auto& s = specs[std::size_t(p)];
        return std::isfinite(v) ? juce::jlimit(s.min, s.max, v) : s.def;
    };
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
        }
    }
    c.hostBpm = hostBpm;
    c.gate = gateOpen;
    return c;
}

void WorldbuilderProcessor::prepareToPlay(double sampleRate, int) {
    rate = std::isfinite(sampleRate) ? juce::jlimit(8000.0, 384000.0, sampleRate) : 48000.0;
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
    if (auto* playHead = getPlayHead())
        if (const auto position = playHead->getPosition()) {
            hostHasTransport = position->getBpm().hasValue() || position->getPpqPosition().hasValue();
            transportMode = transportMode && hostHasTransport;
            if (const auto bpm = position->getBpm(); bpm.hasValue() && std::isfinite(*bpm) && *bpm >= 20.0 && *bpm <= 999.0) hostBpm = float(*bpm);
            if (position->getIsPlaying())
                if (const auto samples = position->getTimeInSamples()) {
                    playing = true;
                    if (!hostWasPlaying || *samples != expectedHostPosition) {
                        const auto ppq = position->getPpqPosition();
                        const auto at = uint64_t(std::max<int64_t>(0, *samples));
                        const double beat = ppq.hasValue() && std::isfinite(*ppq) && *ppq >= 0 ? *ppq : -1.0;
                        if (transportMode) {
                            auto controls = readControls();
                            engine.restart(at, beat, controls);
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

    float* left = buffer.getWritePointer(0);
    float* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
    if (right != nullptr) {
        engine.process(left, right, n, readControls());
    } else {
        // Mono output: render stereo in chunks through a small stack buffer.
        std::array<float, 256> scratch {};
        for (int start = 0; start < n; start += int(scratch.size())) {
            const int count = std::min(int(scratch.size()), n - start);
            engine.process(left + start, scratch.data(), count, readControls());
            for (int i = 0; i < count; ++i) left[start + i] = 0.5f * (left[start + i] + scratch[std::size_t(i)]);
        }
    }
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch) buffer.clear(ch, 0, n);

    const auto seconds = juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - begin);
    cpuLoad.store(0.9f * cpuLoad.load() + 0.1f * float(seconds * rate / n), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* WorldbuilderProcessor::createEditor() { return new WorldbuilderEditor(*this); }

void WorldbuilderProcessor::getStateInformation(juce::MemoryBlock& destData) {
    auto state = parameters.copyState();
    state.setProperty("version", 2, nullptr);
    patterns.syncTypes(slotTypes());
    state.removeChild(state.getChildWithName("Patterns"), nullptr);
    state.appendChild(patterns.toValueTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

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
        if (child.isValid() && child.hasProperty("value")) {
            const float stored = float(child.getProperty("value"));
            if (std::isfinite(stored)) value = juce::jlimit(s.min, s.max, stored);
        }
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    }
    // Patterns (Milestone 4 and later); older projects have none and keep the generated ones.
    patterns.fromValueTree(state.getChildWithName("Patterns"), slotTypes());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WorldbuilderProcessor(); }
