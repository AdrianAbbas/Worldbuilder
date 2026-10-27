#include "KeysPanel.h"
#include "Pixel.h"
#include "Style.h"
#include "WorldLookAndFeel.h"

namespace wb::ui {

// The on-screen keyboard: four octaves from C2 (with C6), lit where notes arrive (bright while a
// key is down, dimmer while the sustain pedal holds it).
class KeysPanel::Keyboard final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit Keyboard(WorldbuilderProcessor& p) : processor(p) {}
    static constexpr int first = 36, count = 49;
    void paint(juce::Graphics& g) override {
        const auto& pal = pixel::theme().colours;
        const auto& in = processor.engine.keysInput;
        const auto bit = [&](const std::array<std::atomic<uint32_t>, 4>& bits, int note) {
            return (bits[std::size_t(note / 32)].load(std::memory_order_relaxed) >> (note % 32)) & 1u;
        };
        static constexpr bool black[12] { false, true, false, true, false, false, true, false, true, false, true, false };
        int whites = 0;
        for (int n = first; n < first + count; ++n) whites += black[n % 12] ? 0 : 1;
        const float w = float(getWidth()) / float(whites), h = float(getHeight());
        const auto colourOf = [&](int note, juce::Colour base) {
            if (bit(in.downBits, note)) return pal.accent;
            if (bit(in.soundBits, note)) return pal.accent.withAlpha(0.55f).overlaidWith(base.withAlpha(0.3f));
            return base;
        };
        int x = 0;
        for (int n = first; n < first + count; ++n) {
            if (black[n % 12]) continue;
            const auto r = juce::Rectangle<float>(float(x) * w, 0, w - 1.0f, h);
            g.setColour(colourOf(n, pal.ink.withAlpha(0.85f)));
            g.fillRect(r);
            ++x;
        }
        x = 0;
        for (int n = first; n < first + count; ++n) {
            if (black[n % 12]) {
                g.setColour(colourOf(n, pal.well));
                g.fillRect(juce::Rectangle<float>(float(x) * w - 0.3f * w, 0, 0.6f * w, 0.6f * h));
            } else {
                ++x;
            }
        }
        // Middle C marked.
        int c4 = 0;
        for (int n = first; n < 60; ++n) c4 += black[n % 12] ? 0 : 1;
        g.setColour(pal.heading);
        g.fillRect(juce::Rectangle<float>((float(c4) + 0.5f) * w - 1.0f, h - 3.0f, 2.0f, 2.0f));
    }

private:
    WorldbuilderProcessor& processor;
};

KeysPanel::KeysPanel(WorldbuilderProcessor& p) : processor(p) {
    keyboard = std::make_unique<Keyboard>(processor);
    keyboard->setTooltip("The notes WORLDBUILDER receives from the host's keyboard (lit while held; dimmer while the sustain pedal holds them).");
    addAndMakeVisible(*keyboard);

    for (int i = 0; i < keys::ScaleModeCount; ++i) scale.addItem(keys::scaleModeNames[std::size_t(i)], i + 1);
    scale.setTooltip("Chromatic: play exactly what is pressed. Snap to scale: wrong notes move to the nearest note of the Root and Mood.");
    scale.onChange = [this] {
        if (!updating) processor.keys.scale.store(scale.getSelectedId() - 1);
    };
    octDown.setTooltip("Octave down (to -2)");
    octUp.setTooltip("Octave up (to +2)");
    octDown.onClick = [this] {
        processor.keys.octave.store(std::max(keys::octaveMin, processor.keys.octave.load() - 1));
        repaint();
    };
    octUp.onClick = [this] {
        processor.keys.octave.store(std::min(keys::octaveMax, processor.keys.octave.load() + 1));
        repaint();
    };
    monitor.setClickingTogglesState(true);
    monitor.setTooltip("Monitor: hear the keyboard live. Off: the notes are still recorded when REC is armed, but not heard.");
    monitor.onClick = [this] { processor.keys.monitor.store(monitor.getToggleState()); };
    regions.setClickingTogglesState(true);
    regions.setTooltip("Host regions play the target instrument: MIDI regions in GarageBand or Logic play it while the song plays. "
                       "Recommended: record into the pattern with REC (the part lives in the scene and the Arrangement) OR record a "
                       "MIDI region in the DAW, not both. Switched off after a take recorded with the transport, so the part never "
                       "plays twice.");
    regions.onClick = [this] {
        processor.keys.hostRegions.store(regions.getToggleState());
        processor.keys.regionsNotice = false;
    };

    single.setClickingTogglesState(true);
    single.setTooltip("Single instrument: this WORLDBUILDER plays only this instrument, from the host's MIDI, with its effects; the rest "
                      "of the scene is silent. For a scene's MIDI on separate GarageBand tracks: one WORLDBUILDER per track, each on "
                      "its instrument (the tracks are named \"Place - Instrument\" to match). Saved with the project and a patch.");
    single.onClick = [this] {
        // On: the tab shown is the instrument; off: the tab shown is the target again.
        const bool on = single.getToggleState();
        processor.setSingle(false);
        if (slotShown >= 0) setTarget(slotShown, elementShown);
        processor.setSingle(on);
        refresh();
    };
    rec.setClickingTogglesState(true);
    rec.setTooltip("REC: record the keyboard into this instrument's pattern. With the host stopped, WORLDBUILDER's transport plays from "
                   "the playhead (after a bar of count-in with COUNT-IN) and records; looping, each pass is a take. While the host plays, "
                   "the take follows its bars. Press again to stop recording (STOP stops both).");
    rec.onClick = [this] {
        const bool on = rec.getToggleState();
        if (on && processor.keys.slot.load() < 0 && slotShown >= 0) setTarget(slotShown, elementShown);
        processor.setArmed(on);
        // Stopped: REC starts the editor's transport from the playhead.
        if (on && !processor.hostRolling() && processor.transportState() == WorldbuilderProcessor::TransportStopped) processor.transportPlay();
        if (on && !processor.keys.tipShown) {
            processor.keys.tipShown = true;
            tipUntil = juce::Time::getMillisecondCounterHiRes() + 15000.0;
        }
        refresh();
    };
    for (int i = 0; i < keys::RecordModeCount; ++i) mode.addItem(keys::recordModeNames[std::size_t(i)], i + 1);
    mode.setTooltip("Replace: a new pattern. Overdub: add to the notes there. Into variant: a new pattern in A, B, C or the Fill.");
    mode.onChange = [this] {
        if (!updating) processor.keys.mode.store(mode.getSelectedId() - 1);
        refresh();
    };
    const char* variants[] = { "A", "B", "C", "FILL" };
    for (int v = 0; v < pat::variantCount; ++v) variant.addItem(variants[v], v + 1);
    variant.setTooltip("The variant Into variant records into (and plays while armed)");
    variant.onChange = [this] {
        if (!updating) processor.keys.variant.store(variant.getSelectedId() - 1);
    };
    for (int i = 0; i < keys::QuantiseCount; ++i) quantise.addItem(keys::quantiseNames[std::size_t(i)], i + 1);
    quantise.setTooltip("Quantise on input: the grid notes move toward (by Strength). Also used by QUANTISE afterwards.");
    quantise.onChange = [this] {
        if (!updating) processor.keys.quantise.store(quantise.getSelectedId() - 1);
    };
    strength.setSliderStyle(juce::Slider::LinearBar);
    strength.setRange(0, 100, 1);
    strength.setTextValueSuffix(" %");
    strength.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    strength.setTooltip("Quantise strength: 100 % lands on the grid; less keeps some of your timing.");
    strength.onValueChange = [this] {
        if (!updating) processor.keys.strength.store(float(strength.getValue() / 100.0));
    };
    countIn.setClickingTogglesState(true);
    countIn.setTooltip("Count-in: one bar of clicks before the take. REC with the host stopped: then the take starts on the playhead. "
                       "When the host starts with REC armed: the take starts on the next bar.");
    countIn.onClick = [this] { processor.keys.countIn.store(countIn.getToggleState()); };
    punch.setClickingTogglesState(true);
    punch.setTooltip("Punch: REC records only inside the punch region (the red bar on the ruler; drag in its lane to set it). Replace "
                     "replaces only the notes inside it and keeps the rest of the pattern.");
    punch.onClick = [this] { processor.transport.punch.store(punch.getToggleState()); };
    length.addItem("PATTERN", 1);
    length.addItem("AUTO", 2);
    length.setTooltip("Length of a new pattern: the pattern's own, or Auto (whole bars covering the take, up to 8).");
    length.onChange = [this] {
        if (!updating) processor.keys.autoLength.store(length.getSelectedId() == 2);
    };
    quantiseNow.setTooltip("Quantise this pattern now, with the grid and strength above (UNDO TAKE undoes it)");
    quantiseNow.onClick = [this] {
        if (slotShown < 0) return;
        const int t = processor.slotTypes()[std::size_t(slotShown)];
        if (processor.recorder.quantise(slotShown, t, elementShown, variantShown, processor.keys.quantise.load(), processor.keys.strength.load())
            && onPatternsChanged)
            onPatternsChanged();
        refresh();
    };
    undoTake.setTooltip("Undo the last take (or the last QUANTISE), all of it");
    undoTake.onClick = [this] {
        if (processor.recorder.undo() && onPatternsChanged) onPatternsChanged();
        refresh();
    };
    for (auto* c : std::initializer_list<juce::Component*> { &single, &scale, &octDown, &octUp, &monitor, &regions, &rec, &mode, &variant,
                                                             &quantise, &strength, &countIn, &punch, &length, &quantiseNow, &undoTake })
        addAndMakeVisible(*c);
    setIcon(rec, Icon::Record);
    for (auto* b : { &monitor, &regions, &countIn, &punch, &rec, &single }) b->setColour(juce::TextButton::buttonOnColourId, colours::brass);
    typesShown = processor.slotTypes();
    refresh();
    startTimerHz(30);
}

KeysPanel::~KeysPanel() { stopTimer(); }

void KeysPanel::setContext(int slot, int element, int v) {
    slotShown = slot;
    elementShown = element;
    variantShown = v;
    // The tab shown is what the keyboard plays and REC records into (a single-instrument
    // instance keeps its instrument).
    if (slot >= 0 && element >= 0 && !processor.single()) setTarget(slot, element);
    refresh();
}

void KeysPanel::setTarget(int slot, int element) {
    // A new target while a take is under way ends it first.
    if (slot != processor.keys.slot.load() || element != processor.keys.element.load()) processor.setArmed(false);
    processor.keys.slot.store(slot);
    processor.keys.element.store(std::clamp(element, 0, placeElements - 1));
    refresh();
}

juce::String KeysPanel::targetText() const {
    const int slot = processor.keys.slot.load(), element = processor.keys.element.load();
    if (slot < 0 || slot >= placeSlots) return "PLAYS NOTHING";
    const int t = typesShown[std::size_t(slot)];
    if (t == NoPlace || patternInfo(t, element).kind == NoPattern) return "PLAYS NOTHING";
    // A single-instrument instance may play another tab than the one shown: its Place too.
    if (processor.single() && (slot != slotShown || element != elementShown)) return "PLAYS " + processor.singleName().toUpperCase();
    return "PLAYS " + juce::String(placeInfo(t).element[std::size_t(element)].name).toUpperCase();
}

void KeysPanel::refresh() {
    updating = true;
    const auto& k = processor.keys;
    const int slot = k.slot.load(), element = k.element.load();
    const bool valid = slot >= 0 && typesShown[std::size_t(slot)] != NoPlace && patternInfo(typesShown[std::size_t(slot)], element).kind != NoPattern;
    scale.setSelectedId(k.scale.load() + 1, juce::dontSendNotification);
    monitor.setToggleState(k.monitor.load(), juce::dontSendNotification);
    regions.setToggleState(k.hostRegions.load(), juce::dontSendNotification);
    rec.setToggleState(k.armed.load(), juce::dontSendNotification);
    single.setToggleState(processor.single(), juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &rec, &mode, &countIn, &punch, &quantise, &strength, &length, &monitor, &regions })
        c->setEnabled(!processor.single());
    mode.setSelectedId(k.mode.load() + 1, juce::dontSendNotification);
    variant.setSelectedId(k.variant.load() + 1, juce::dontSendNotification);
    variant.setEnabled(k.mode.load() == keys::IntoVariant);
    quantise.setSelectedId(k.quantise.load() + 1, juce::dontSendNotification);
    strength.setValue(double(k.strength.load()) * 100.0, juce::dontSendNotification);
    countIn.setToggleState(k.countIn.load(), juce::dontSendNotification);
    punch.setToggleState(processor.transport.punch.load(), juce::dontSendNotification);
    length.setSelectedId(k.autoLength.load() ? 2 : 1, juce::dontSendNotification);
    undoTake.setEnabled(processor.recorder.canUndo() && !processor.recorder.taking());

    // The line under the controls: what REC is doing, or a tip.
    juce::String text;
    const auto state = processor.engine.capture.stateOut.load(std::memory_order_relaxed);
    const bool own = !processor.hostRolling() && processor.transportState() != WorldbuilderProcessor::TransportStopped;
    if (k.armed.load() && processor.transportState() == WorldbuilderProcessor::TransportCountIn) {
        text = "COUNT-IN: " + juce::String(int(std::ceil(processor.transport.countLeft.load()))) + " beats, then the take starts on the playhead";
    } else if (k.armed.load()) {
        switch (state) {
            case keys::Capture::CountIn: text = "COUNT-IN: " + juce::String(int(std::ceil(processor.engine.capture.countInLeft.load()))) + " beats"; break;
            case keys::Capture::Taking:
                text = (own ? juce::String(processor.transport.loop.load() ? "RECORDING (looping: each pass is a take): " : "RECORDING: ")
                            : juce::String("RECORDING with the host: "))
                       + juce::String(processor.recorder.takeNotes()) + " notes" + (processor.transport.punch.load() ? " (punch region only)" : "");
                break;
            case keys::Capture::FreeTaking: text = "FREE TAKE (metronome at the host tempo): " + juce::String(processor.recorder.takeNotes()) + " notes. REC again to stop."; break;
            case keys::Capture::FreeWaiting: text = "REC armed: press PLAY here or Play in the host, or just play (a free take starts on your first note)."; break;
            default: text = "REC armed."; break;
        }
    } else if (!valid) {
        text = "Open an instrument's tab: the keyboard plays it and REC records into it.";
    } else if (own) {
        text = processor.transport.loop.load() ? "PLAYING the loop at the host tempo. REC records from here; STOP stops." : "PLAYING at the host tempo. STOP stops.";
    }
    if (processor.single())
        text = "SINGLE INSTRUMENT: this WORLDBUILDER plays only " + processor.singleName().toUpperCase()
               + " from the host's MIDI, with its effects. The rest of the scene is silent. SINGLE again: the whole scene.";
    else if (juce::Time::getMillisecondCounterHiRes() < tipUntil)
        text = "TIP: GarageBand/Logic also record your notes as a MIDI region. Record into WORLDBUILDER's pattern (recommended) OR a DAW "
               "region, not both: after a take, HOST MIDI switches off so the region won't play the instrument twice.";
    else if (k.regionsNotice && !k.armed.load())
        text = "HOST MIDI is off: the take is in the pattern, so a MIDI region the host recorded won't play the instrument again. "
               "Delete that region, or switch HOST MIDI back on.";
    if (text != status) {
        status = text;
        repaint(statusArea);
    }
    updating = false;
}

void KeysPanel::timerCallback() {
    if (!isShowing()) return;
    processor.serviceKeys();
    if (processor.slotTypes() != typesShown) {
        typesShown = processor.slotTypes();
        repaint(targetArea);
    }
    if (const auto r = processor.recorder.revision(); r != recorderSeen) {
        recorderSeen = r;
        if (onPatternsChanged) onPatternsChanged();
    }
    refresh();
    keyboard->repaint();
    repaint(octaveArea);
}

void KeysPanel::paint(juce::Graphics& g) {
    const auto& pal = pixel::theme().colours;
    pixel::drawText(g, "KEYS", playRow.withWidth(40), pal.heading, juce::Justification::centredLeft, pixel::Font::Small);
    pixel::drawText(g, "REC", recRow.withWidth(40), pal.heading, juce::Justification::centredLeft, pixel::Font::Small);
    pixel::drawText(g, targetText(), targetArea, pal.accent, juce::Justification::centredLeft, pixel::Font::Small);
    const int octave = processor.keys.octave.load();
    pixel::drawText(g, "OCT " + juce::String(octave > 0 ? "+" : "") + juce::String(octave), octaveArea, pal.ink, juce::Justification::centred, pixel::Font::Small);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.setColour(status.startsWith("TIP") || status.startsWith("HOST MIDI") ? pal.accent : pal.inkDim);
    g.drawText(status, statusArea, juce::Justification::centredLeft, true);
}

void KeysPanel::resized() {
    auto r = getLocalBounds();
    playRow = r.removeFromTop(24);
    r.removeFromTop(2);
    recRow = r.removeFromTop(24);
    statusArea = r;
    {
        auto t = playRow.withTrimmedLeft(40);
        const auto place = [&t](juce::Component& c, int w) { c.setBounds(t.removeFromLeft(w).reduced(1, 1)); };
        targetArea = t.removeFromLeft(150).withTrimmedLeft(2);
        place(single, 62);
        t.removeFromLeft(4);
        place(scale, 104);
        t.removeFromLeft(6);
        place(octDown, 22);
        octaveArea = t.removeFromLeft(44);
        place(octUp, 22);
        t.removeFromLeft(6);
        place(monitor, 70);
        place(regions, 80);
        t.removeFromLeft(4);
        keyboard->setBounds(t.reduced(1, 2));
    }
    {
        auto t = recRow.withTrimmedLeft(40);
        const auto place = [&t](juce::Component& c, int w) { c.setBounds(t.removeFromLeft(w).reduced(1, 1)); };
        place(rec, 54);
        place(mode, 110);
        place(variant, 58);
        t.removeFromLeft(4);
        place(quantise, 62);
        place(strength, 70);
        t.removeFromLeft(4);
        place(countIn, 80);
        place(punch, 58);
        place(length, 82);
        undoTake.setBounds(t.removeFromRight(86).reduced(1, 1));
        quantiseNow.setBounds(t.removeFromRight(80).reduced(1, 1));
    }
}

} // namespace wb::ui
