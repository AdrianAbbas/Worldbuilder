#include "PluginEditor.h"
#include "UI/PlaceArt.h"

using namespace wb;
namespace colours = wb::ui::colours;

namespace {
struct Group {
    const char* title;
    std::array<int, 3> params;   // -1 = empty slot
};
constexpr std::array<Group, 4> groups { {
    { "CLIMATE", { PTemperature, PWeather, PWetness } },
    { "CIVILISATION", { PPopulation, PMachinery, PEra } },
    { "MYSTERY", { PMagic, PLife, PDanger } },
    { "SIZE", { PScale, PActivity, -1 } },
} };

const char* tooltipFor(int p) {
    switch (p) {
        case PTemperature: return "Frozen to scorching: singing ice and thin high wind, or crackling fire and lava in the key. Cold Places sound brighter.";
        case PWeather: return "Still to storm: wind and howling, drizzle to downpour, thunder (a tuned impact and swell). Storms push Places further away.";
        case PWetness: return "Arid to drenched: tuned drips, then rain plucks and shimmer, streams, a waterfall and lapping waves. Douses fire.";
        case PMachinery: return "None to industrial: every Place plays straighter and denser, with anvil-like rings and heavier engine throb.";
        case PPopulation: return "Empty to crowded: how busy every Place is, and how deep its effect (echo, tape, ping-pong) goes.";
        case PEra: return "Ancient to futuristic: organic or synthetic colour across the world; brighter, cleaner Places when futuristic.";
        case PMagic: return "Mundane to arcane: widens and brightens the Tonal Bed; longer, brighter Place halls.";
        case PLife: return "Barren to teeming: more human feel in the Places: looser timing and velocity, vibrato, grace notes.";
        case PDanger: return "Peaceful to hostile: darker chords for everything, more Tension in every Place, rumbling ground, closer storms.";
        case PScale: return "Intimate to colossal: World Space size and decay; Places, water, rockfalls and thunder further away and bigger.";
        case PActivity: return "Calm to chaotic: how fast the world moves and how much the Places vary; more drips, crackles and creaks.";
        case PSpaceMix: return "Amount of the shared World Space reverb.";
        case PWidth: return "Stereo width of the whole world.";
        case PMaster: return "Output level.";
        default: return "";
    }
}

float lerp(float a, float b, float t) { return a + (b - a) * t; }

juce::Colour globeColour(float temperature, float danger) {
    const juce::Colour ice { 0xff9fd3f0 }, temperate { 0xff3f8f7a }, scorch { 0xffd2622c };
    auto c = temperature < 0.5f ? ice.interpolatedWith(temperate, temperature * 2.0f)
                                : temperate.interpolatedWith(scorch, (temperature - 0.5f) * 2.0f);
    return c.interpolatedWith(juce::Colour(0xff2a0f14), danger * 0.55f);
}

// Each Place's accent colour on its card, strip and landmark.
juce::Colour placeColour(int type) {
    static const std::array<juce::Colour, PlaceTypeCount> table { juce::Colour(0xff6c7a8f), juce::Colour(0xffc9a45c), juce::Colour(0xffd08a4e),
                                                                  juce::Colour(0xff5fd4e6), juce::Colour(0xffb36a4a), juce::Colour(0xff8fa6c9),
                                                                  juce::Colour(0xffd8c27a), juce::Colour(0xff6fb37a), juce::Colour(0xff9a8f86),
                                                                  juce::Colour(0xff7a8fb3), juce::Colour(0xffb78cff) };
    return table[std::size_t(std::clamp(type, 0, PlaceTypeCount - 1))];
}

const char* slotTooltip(int field) {
    switch (field) {
        case PlActivity: return "Sparse to busy: how often bells, horns, notes and hits play. Population scales it for every Place.";
        case PlTension: return "Calm to menacing: darker voicings, tighter and faster rhythm. Danger adds to it.";
        case PlTone: return "Dark to bright.";
        case PlSize: return "Size of the Place's own space (hall, room, hangar).";
        case PlVariation: return "How much the patterns evolve: 0 repeats exactly, 100 % never repeats.";
        case PlDensity: return "Rhythmic density: how many of the in-between notes and hits play.";
        case PlSwing: return "Swing on the off-beat sixteenths.";
        default: return "";
    }
}
} // namespace

//==============================================================================
WorldbuilderEditor::WorldbuilderEditor(WorldbuilderProcessor& p) : AudioProcessorEditor(p), processor(p) {
    setLookAndFeel(&lookAndFeel);

    for (const auto& group : groups)
        for (int param : group.params)
            if (param >= 0) addKnob(param);
    for (int param : { PSpaceMix, PWidth, PMaster }) addKnob(param);

    for (int l = 0; l < LayerCount; ++l) {
        auto& s = strips[std::size_t(l)];
        s.level.setSliderStyle(juce::Slider::LinearHorizontal);
        s.level.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.level.setTooltip(juce::String(layerNames[std::size_t(l)]) + " level");
        s.mute.setColour(juce::TextButton::buttonOnColourId, colours::danger);
        s.solo.setColour(juce::TextButton::buttonOnColourId, colours::cyan);
        s.mute.setTooltip("Mute");
        s.solo.setTooltip("Solo");
        for (juce::Component* c : { static_cast<juce::Component*>(&s.level), static_cast<juce::Component*>(&s.mute),
                                    static_cast<juce::Component*>(&s.solo) })
            addAndMakeVisible(c);
        s.levelAttachment = std::make_unique<SliderAttachment>(processor.parameters, specs[std::size_t(layerLevel(l))].id, s.level);
        s.level.setDoubleClickReturnValue(true, specs[std::size_t(layerLevel(l))].def);
        s.muteAttachment = std::make_unique<ButtonAttachment>(processor.parameters, specs[std::size_t(layerMute(l))].id, s.mute);
        s.soloAttachment = std::make_unique<ButtonAttachment>(processor.parameters, specs[std::size_t(layerSolo(l))].id, s.solo);
    }

    for (int slot = 0; slot < placeSlotCount; ++slot) {
        auto& s = placeStrips[std::size_t(slot)];
        const auto id = [slot](int field) { return specs[std::size_t(placeParam(slot, field))].id; };
        for (auto* slider : { &s.level, &s.distance, &s.pan }) {
            slider->setSliderStyle(juce::Slider::LinearHorizontal);
            slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addChildComponent(*slider);
        }
        s.level.setTooltip("Level");
        s.distance.setTooltip("Distance: near to far (quieter, darker, narrower and more reverberant)");
        s.pan.setTooltip("Pan, left to right");
        s.levelAttachment = std::make_unique<SliderAttachment>(processor.parameters, id(PlLevel), s.level);
        s.distanceAttachment = std::make_unique<SliderAttachment>(processor.parameters, id(PlDistance), s.distance);
        s.panAttachment = std::make_unique<SliderAttachment>(processor.parameters, id(PlPan), s.pan);
        s.level.setDoubleClickReturnValue(true, specs[std::size_t(placeParam(slot, PlLevel))].def);
        s.distance.setDoubleClickReturnValue(true, specs[std::size_t(placeParam(slot, PlDistance))].def);
        s.pan.setDoubleClickReturnValue(true, 0.0);
        s.mute.setColour(juce::TextButton::buttonOnColourId, colours::danger);
        s.solo.setColour(juce::TextButton::buttonOnColourId, colours::cyan);
        s.mute.setTooltip("Mute");
        s.solo.setTooltip("Solo");
        s.muteAttachment = std::make_unique<ButtonAttachment>(processor.parameters, id(PlMute), s.mute);
        s.soloAttachment = std::make_unique<ButtonAttachment>(processor.parameters, id(PlSolo), s.solo);
        s.edit.setTooltip("Open this Place's controls");
        s.remove.setTooltip("Remove this Place");
        s.edit.onClick = [this, slot] { detail.slot == slot ? closeDetail() : openDetail(slot); };
        s.remove.onClick = [this, slot] {
            if (detail.slot == slot) closeDetail();
            setParam(placeParam(slot, PlType), 0);
            refreshPlaceStrips();
        };
        for (juce::Component* c : { static_cast<juce::Component*>(&s.mute), static_cast<juce::Component*>(&s.solo),
                                    static_cast<juce::Component*>(&s.edit), static_cast<juce::Component*>(&s.remove) })
            addChildComponent(c);
    }

    // Detail panel controls; their attachments are made when a slot is opened.
    for (int i = 0; i < 3; ++i) {
        auto& b = detail.character[std::size_t(i)];
        b.setColour(juce::TextButton::buttonOnColourId, colours::brass);
        b.onClick = [this, i] {
            if (detail.slot >= 0) setParam(placeParam(detail.slot, PlCharacter), float(i));
            for (int k = 0; k < 3; ++k) detail.character[std::size_t(k)].setToggleState(k == i, juce::dontSendNotification);
        };
        b.setTooltip("Character: each one changes the patterns, harmony and sound, not just levels");
        addChildComponent(b);
    }
    for (auto* knob : { &detail.activity, &detail.tension, &detail.tone, &detail.size, &detail.variation, &detail.density, &detail.swing }) {
        setupRotary(*knob, "");
        addChildComponent(*knob);
    }
    detail.activity.setTooltip(slotTooltip(PlActivity));
    detail.tension.setTooltip(slotTooltip(PlTension));
    detail.tone.setTooltip(slotTooltip(PlTone));
    detail.size.setTooltip(slotTooltip(PlSize));
    detail.variation.setTooltip(slotTooltip(PlVariation));
    detail.density.setTooltip(slotTooltip(PlDensity));
    detail.swing.setTooltip(slotTooltip(PlSwing));
    detail.sync.setColour(juce::TextButton::buttonOnColourId, colours::cyan);
    detail.sync.setTooltip("On: rhythm follows the host tempo. Off: the Place keeps its own free tempo.");
    addChildComponent(detail.sync);
    detail.seed.setSliderStyle(juce::Slider::LinearBar);
    detail.seed.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    detail.seed.setTooltip("This Place's seed: its patterns and melodies. Drag to change.");
    addChildComponent(detail.seed);
    detail.dice.onClick = [this] {
        if (detail.slot >= 0) setParam(placeParam(detail.slot, PlSeed), float(juce::Random::getSystemRandom().nextInt({ 1, 10000 })));
    };
    detail.dice.setTooltip("Roll a new seed for this Place");
    addChildComponent(detail.dice);
    detail.close.onClick = [this] { closeDetail(); };
    addChildComponent(detail.close);
    for (int e = 0; e < placeElements; ++e) {
        auto& slider = detail.element[std::size_t(e)];
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setDoubleClickReturnValue(true, 0.8);
        addChildComponent(slider);
        auto& toggle = detail.elementOn[std::size_t(e)];
        toggle.setColour(juce::TextButton::buttonOnColourId, colours::brass);
        toggle.setTooltip("Switch this element on or off");
        addChildComponent(toggle);
    }

    power.setColour(juce::TextButton::buttonOnColourId, colours::cyan);
    power.setTooltip("Fades the whole world in or out over 2 seconds");
    addAndMakeVisible(power);
    powerAttachment = std::make_unique<ButtonAttachment>(processor.parameters, "power", power);

    const auto setupCombo = [this](juce::ComboBox& box, int param, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& attachment) {
        box.addItemList(juce::StringArray::fromTokens(specs[std::size_t(param)].choices, "|", ""), 1);
        addAndMakeVisible(box);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, specs[std::size_t(param)].id, box);
    };
    setupCombo(root, PRoot, rootAttachment);
    setupCombo(mood, PMood, moodAttachment);
    setupCombo(quality, PQuality, qualityAttachment);
    root.setTooltip("Key root: everything tonal (bed, drips, rain, wind, thunder, every Place) follows it");
    mood.setTooltip("Scale and mood: everything tonal follows it");
    quality.setTooltip("Eco / Standard / High: voice counts and detail");

    seed.setSliderStyle(juce::Slider::LinearBar);
    seed.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    seed.setTooltip("Seed: the same seed and settings always build the same world. Drag to change.");
    addAndMakeVisible(seed);
    seedAttachment = std::make_unique<SliderAttachment>(processor.parameters, "seed", seed);
    dice.setTooltip("Roll a new seed for the whole world");
    dice.onClick = [this] { setParam(PSeed, float(juce::Random::getSystemRandom().nextInt({ 1, 10000 }))); };
    addAndMakeVisible(dice);

    setSize(baseWidth, baseHeight);
    refreshPlaceStrips();
    startTimerHz(30);
}

WorldbuilderEditor::~WorldbuilderEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void WorldbuilderEditor::setupRotary(juce::Slider& slider, const juce::String& tooltip) {
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::degreesToRadians(225.0f), juce::degreesToRadians(495.0f), true);
    slider.setMouseDragSensitivity(220);
    slider.setVelocityBasedMode(false);
    if (tooltip.isNotEmpty()) slider.setTooltip(tooltip);
}

juce::Slider& WorldbuilderEditor::addKnob(int param) {
    const auto& spec = specs[std::size_t(param)];
    auto slider = std::make_unique<juce::Slider>(spec.name);
    setupRotary(*slider, tooltipFor(param));
    addAndMakeVisible(*slider);
    knobAttachments.push_back(std::make_unique<SliderAttachment>(processor.parameters, spec.id, *slider));
    slider->setDoubleClickReturnValue(true, spec.def);
    knobFor[std::size_t(param)] = slider.get();
    knobs.push_back(std::move(slider));
    return *knobs.back();
}

void WorldbuilderEditor::setParam(int p, float value) {
    if (auto* parameter = processor.parameters.getParameter(specs[std::size_t(p)].id)) {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
        parameter->endChangeGesture();
    }
}

float WorldbuilderEditor::param(int p) const { return processor.parameters.getRawParameterValue(specs[std::size_t(p)].id)->load(); }
int WorldbuilderEditor::placeType(int slot) const { return juce::roundToInt(param(placeParam(slot, PlType))); }

//==============================================================================
int WorldbuilderEditor::addPlace(int type) {
    if (type <= NoPlace || type >= PlaceTypeCount || !placeImplemented[std::size_t(type)]) return -1;
    for (int slot = 0; slot < placeSlotCount; ++slot) {
        if (placeType(slot) != NoPlace) continue;
        // A fresh Place: every control at its default, a new seed.
        for (int field = 0; field < PlaceParamCount; ++field)
            if (field != PlType && field != PlPan) setParam(placeParam(slot, field), specs[std::size_t(placeParam(slot, field))].def);
        setParam(placeParam(slot, PlSeed), float(juce::Random::getSystemRandom().nextInt({ 1, 10000 })));
        setParam(placeParam(slot, PlType), float(type));
        refreshPlaceStrips();
        return slot;
    }
    fullMessageUntil = juce::Time::getMillisecondCounterHiRes() + 2500.0;
    repaint(cardsPanel);
    return -1;
}

void WorldbuilderEditor::openDetail(int slot) {
    if (slot < 0 || slot >= placeSlotCount || placeType(slot) == NoPlace) return;
    closeDetail();
    detail.slot = slot;
    detail.shownType = -1;
    const auto id = [slot](int field) { return specs[std::size_t(placeParam(slot, field))].id; };
    auto& d = detail;
    const std::array<std::pair<juce::Slider*, int>, 8> sliders { { { &d.activity, PlActivity }, { &d.tension, PlTension }, { &d.tone, PlTone },
                                                                   { &d.size, PlSize }, { &d.variation, PlVariation }, { &d.density, PlDensity },
                                                                   { &d.swing, PlSwing }, { &d.seed, PlSeed } } };
    for (auto [slider, field] : sliders) {
        d.sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.parameters, id(field), *slider));
        slider->setDoubleClickReturnValue(true, specs[std::size_t(placeParam(slot, field))].def);
    }
    for (int e = 0; e < placeElements; ++e) {
        d.sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.parameters, id(PlElement + e), d.element[std::size_t(e)]));
        d.buttonAttachments.push_back(std::make_unique<ButtonAttachment>(processor.parameters, id(PlElementOn + e), d.elementOn[std::size_t(e)]));
    }
    d.buttonAttachments.push_back(std::make_unique<ButtonAttachment>(processor.parameters, id(PlSync), d.sync));
    const int character = juce::roundToInt(param(placeParam(slot, PlCharacter)));
    for (int k = 0; k < 3; ++k) d.character[std::size_t(k)].setToggleState(k == character, juce::dontSendNotification);
    for (auto& b : d.character) b.setVisible(true);
    for (auto* c : std::initializer_list<juce::Component*> { &d.activity, &d.tension, &d.tone, &d.size, &d.variation, &d.density, &d.swing,
                                                             &d.seed, &d.sync, &d.dice, &d.close })
        c->setVisible(true);
    for (auto& s : d.element) s.setVisible(true);
    for (auto& t : d.elementOn) t.setVisible(true);
    updateDetailLabels();
    layoutDetail();
    repaint();
}

void WorldbuilderEditor::closeDetail() {
    auto& d = detail;
    d.sliderAttachments.clear();
    d.buttonAttachments.clear();
    d.slot = -1;
    for (auto& b : d.character) b.setVisible(false);
    for (auto* c : std::initializer_list<juce::Component*> { &d.activity, &d.tension, &d.tone, &d.size, &d.variation, &d.density, &d.swing,
                                                             &d.seed, &d.sync, &d.dice, &d.close })
        c->setVisible(false);
    for (auto& s : d.element) s.setVisible(false);
    for (auto& t : d.elementOn) t.setVisible(false);
    repaint();
}

void WorldbuilderEditor::refreshPlaceStrips() {
    for (int slot = 0; slot < placeSlotCount; ++slot) {
        auto& s = placeStrips[std::size_t(slot)];
        const int type = placeType(slot);
        if (type == s.shownType) continue;
        s.shownType = type;
        const bool shown = type != NoPlace;
        for (juce::Component* c : { static_cast<juce::Component*>(&s.level), static_cast<juce::Component*>(&s.distance),
                                    static_cast<juce::Component*>(&s.pan), static_cast<juce::Component*>(&s.mute),
                                    static_cast<juce::Component*>(&s.solo), static_cast<juce::Component*>(&s.edit),
                                    static_cast<juce::Component*>(&s.remove) })
            c->setVisible(shown);
        if (!shown && detail.slot == slot) closeDetail();
    }
    repaint(placesPanel);
}

//==============================================================================
void WorldbuilderEditor::resized() {
    auto area = getLocalBounds().reduced(12, 10);
    header = area.removeFromTop(44);
    power.setBounds(header.withTrimmedLeft(header.getWidth() - 96).withSizeKeepingCentre(90, 28));
    area.removeFromTop(6);
    footer = area.removeFromBottom(80);
    area.removeFromBottom(8);
    controlsPanel = area.removeFromLeft(300);
    area.removeFromLeft(12);
    auto right = area.removeFromRight(310);
    area.removeFromRight(12);
    layersPanel = right.removeFromTop(186);
    right.removeFromTop(10);
    placesPanel = right;
    globePanel = area.removeFromTop(330);
    area.removeFromTop(10);
    cardsPanel = area;
    const float globeSize = std::min(float(globePanel.getWidth()) - 40.0f, float(globePanel.getHeight()) - 40.0f);
    globeArea = juce::Rectangle<float>(globeSize, globeSize).withCentre(globePanel.toFloat().getCentre().translated(0, 8));

    // World Controls: one row per group, title on top.
    auto rows = controlsPanel.reduced(8, 8);
    const int rowHeight = rows.getHeight() / int(groups.size());
    for (std::size_t g = 0; g < groups.size(); ++g) groupAreas[g] = rows.removeFromTop(rowHeight);
    for (std::size_t g = 0; g < groups.size(); ++g) {
        auto row = groupAreas[g].withTrimmedTop(18);
        const int cell = row.getWidth() / 3;
        for (int param : groups[g].params) {
            auto c = row.removeFromLeft(cell);
            if (param >= 0) knobFor[std::size_t(param)]->setBounds(c.reduced(6, 0));
        }
    }

    // Environment layers.
    auto mixer = layersPanel.reduced(10, 8);
    mixer.removeFromTop(20);
    const int stripHeight = mixer.getHeight() / LayerCount;
    for (auto& s : strips) {
        s.row = mixer.removeFromTop(stripHeight);
        auto r = s.row;
        r.removeFromLeft(86);
        s.solo.setBounds(r.removeFromRight(24).withSizeKeepingCentre(22, 18));
        r.removeFromRight(3);
        s.mute.setBounds(r.removeFromRight(24).withSizeKeepingCentre(22, 18));
        r.removeFromRight(6);
        s.level.setBounds(r.withTrimmedBottom(7));
    }

    // Place strips: name and buttons, then level, distance and pan.
    auto places = placesPanel.reduced(10, 8);
    places.removeFromTop(20);
    const int placeHeight = places.getHeight() / placeSlotCount;
    for (auto& s : placeStrips) {
        s.row = places.removeFromTop(placeHeight).reduced(0, 3);
        auto r = s.row.reduced(6, 4);
        auto top = r.removeFromTop(22);
        s.remove.setBounds(top.removeFromRight(22).withSizeKeepingCentre(22, 20));
        top.removeFromRight(4);
        s.edit.setBounds(top.removeFromRight(46).withSizeKeepingCentre(46, 20));
        top.removeFromRight(4);
        s.solo.setBounds(top.removeFromRight(22).withSizeKeepingCentre(22, 18));
        top.removeFromRight(3);
        s.mute.setBounds(top.removeFromRight(22).withSizeKeepingCentre(22, 18));
        r.removeFromTop(14);   // labels
        auto sliders = r.removeFromTop(16);
        const int w = sliders.getWidth() / 3;
        s.level.setBounds(sliders.removeFromLeft(w).reduced(4, 0));
        s.distance.setBounds(sliders.removeFromLeft(w).reduced(4, 0));
        s.pan.setBounds(sliders.reduced(4, 0));
    }

    // Place cards: two rows of five.
    auto grid = cardsPanel.reduced(10, 8);
    grid.removeFromTop(20);
    const int cardW = grid.getWidth() / 5, cardH = grid.getHeight() / 2;
    for (int i = 0; i < 10; ++i)
        cards[std::size_t(i)] = juce::Rectangle<int>(grid.getX() + (i % 5) * cardW, grid.getY() + (i / 5) * cardH, cardW, cardH).reduced(4);

    layoutDetail();

    // Footer: key and global settings, then the output knobs.
    auto f = footer.reduced(12, 8);
    auto settings = f.removeFromLeft(480);
    auto top = settings.removeFromTop(settings.getHeight() / 2).withTrimmedTop(14);
    auto bottom = settings.withTrimmedTop(14);
    root.setBounds(top.removeFromLeft(80).withSizeKeepingCentre(80, 22));
    top.removeFromLeft(10);
    mood.setBounds(top.removeFromLeft(140).withSizeKeepingCentre(140, 22));
    top.removeFromLeft(10);
    quality.setBounds(top.removeFromLeft(120).withSizeKeepingCentre(120, 22));
    seed.setBounds(bottom.removeFromLeft(230).withSizeKeepingCentre(230, 20));
    bottom.removeFromLeft(10);
    dice.setBounds(bottom.removeFromLeft(110).withSizeKeepingCentre(110, 22));
    f.removeFromRight(40);   // master meter
    for (int param : { PMaster, PWidth, PSpaceMix })
        knobFor[std::size_t(param)]->setBounds(f.removeFromRight(92).withSizeKeepingCentre(78, 66));
}

void WorldbuilderEditor::layoutDetail() {
    auto& d = detail;
    d.area = globePanel.getUnion(cardsPanel);
    auto r = d.area.reduced(16, 10);
    d.titleArea = r.removeFromTop(52);
    d.close.setBounds(d.titleArea.withTrimmedLeft(d.titleArea.getWidth() - 70).withSizeKeepingCentre(66, 24).translated(0, -8));
    r.removeFromTop(4);
    d.characterArea = r.removeFromTop(34);
    {
        auto row = d.characterArea.withTrimmedLeft(90);
        const int w = row.getWidth() / 3;
        for (auto& b : d.character) b.setBounds(row.removeFromLeft(w).reduced(4, 4));
    }
    r.removeFromTop(8);
    auto knobsRow = r.removeFromTop(96);
    {
        const int w = knobsRow.getWidth() / 5;
        for (auto* k : { &d.activity, &d.tension, &d.tone, &d.size, &d.variation }) k->setBounds(knobsRow.removeFromLeft(w).reduced(8, 0));
    }
    r.removeFromTop(6);
    d.rhythmArea = r.removeFromTop(96);
    {
        auto row = d.rhythmArea.withTrimmedTop(16);
        const int w = row.getWidth() / 5;
        d.sync.setBounds(row.removeFromLeft(w + 20).withSizeKeepingCentre(w + 10, 24));
        d.density.setBounds(row.removeFromLeft(w).reduced(8, 0));
        d.swing.setBounds(row.removeFromLeft(w).reduced(8, 0));
        auto seedArea = row.reduced(6, 0);
        d.seed.setBounds(seedArea.removeFromTop(seedArea.getHeight() / 2).withSizeKeepingCentre(seedArea.getWidth(), 20));
        d.dice.setBounds(seedArea.withSizeKeepingCentre(seedArea.getWidth(), 22));
    }
    r.removeFromTop(8);
    d.elementsArea = r.removeFromTop(20 + 5 * 26);
    {
        auto rows = d.elementsArea.withTrimmedTop(20);
        for (int e = 0; e < placeElements; ++e) {
            auto row = rows.removeFromTop(26);
            d.elementOn[std::size_t(e)].setBounds(row.removeFromLeft(130).reduced(2, 3));
            row.removeFromLeft(10);
            d.element[std::size_t(e)].setBounds(row.reduced(0, 5));
        }
    }
}

//==============================================================================
void WorldbuilderEditor::paint(juce::Graphics& g) {
    g.setGradientFill(juce::ColourGradient(colours::background.brighter(0.08f), 0, 0, colours::background.darker(0.4f), 0,
                                           float(getHeight()), false));
    g.fillAll();

    g.setColour(colours::brass);
    g.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)).withExtraKerningFactor(0.18f));
    g.drawText("WORLDBUILDER", header.withTrimmedLeft(4), juce::Justification::centredLeft, false);
    g.setColour(colours::dim);
    g.setFont(juce::Font(juce::FontOptions(12.0f)).withExtraKerningFactor(0.08f));
    g.drawText("FILM-SCORING SOUNDSCAPE INSTRUMENT  -  MILESTONE 3 PREVIEW", header.withTrimmedLeft(300).withTrimmedRight(110),
               juce::Justification::centredLeft, false);
    g.setColour(colours::brass.withAlpha(0.35f));
    g.drawHorizontalLine(header.getBottom() + 2, float(header.getX()), float(header.getRight()));

    const bool detailOpen = detail.slot >= 0;
    for (auto panel : { controlsPanel, globePanel, cardsPanel, layersPanel, placesPanel, footer }) {
        if (detailOpen && (panel == globePanel || panel == cardsPanel)) continue;
        g.setColour(colours::panel.withAlpha(0.85f));
        g.fillRoundedRectangle(panel.toFloat(), 10.0f);
        g.setColour(colours::panelEdge);
        g.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 10.0f, 1.0f);
    }

    g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)).withExtraKerningFactor(0.15f));
    for (std::size_t i = 0; i < groups.size(); ++i) {
        g.setColour(colours::brass.withAlpha(0.85f));
        g.drawText(groups[i].title, groupAreas[i].withHeight(16).withTrimmedLeft(6), juce::Justification::centredLeft, false);
    }
    g.setColour(colours::brass.withAlpha(0.85f));
    g.drawText("ENVIRONMENT", layersPanel.reduced(14, 8).withHeight(16), juce::Justification::centredLeft, false);
    g.drawText("PLACES", placesPanel.reduced(14, 8).withHeight(16), juce::Justification::centredLeft, false);

    for (int l = 0; l < LayerCount; ++l) {
        const auto& s = strips[std::size_t(l)];
        auto nameArea = s.row.withWidth(86).withTrimmedLeft(4);
        g.setFont(juce::Font(juce::FontOptions(12.5f)));
        g.setColour(colours::parchment.withAlpha(0.95f));
        g.drawText(layerNames[std::size_t(l)], nameArea, juce::Justification::centredLeft, false);
        const auto meterArea = s.level.getBounds().toFloat().withY(float(s.level.getBottom()) + 1.0f).withHeight(3.0f);
        g.setColour(colours::panelEdge);
        g.fillRoundedRectangle(meterArea, 1.5f);
        const float level = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(meters[std::size_t(l)], -60.0f) + 60.0f) / 60.0f);
        g.setColour(colours::cyan.withAlpha(0.8f));
        g.fillRoundedRectangle(meterArea.withWidth(meterArea.getWidth() * level), 1.5f);
    }
    paintPlaceStrips(g);

    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)).withExtraKerningFactor(0.12f));
    g.setColour(colours::brass.withAlpha(0.8f));
    for (auto [component, text] : { std::pair<juce::Component*, const char*> { &root, "ROOT" }, { &mood, "MOOD" },
                                    { &quality, "QUALITY" }, { &seed, "WORLD SEED" } })
        g.drawText(text, component->getBounds().translated(0, -15).withHeight(14), juce::Justification::centredLeft, false);
    g.setColour(colours::dim);
    g.drawText(juce::String(processor.engine.frame().bpm, 1) + " BPM", dice.getBounds().translated(dice.getWidth() + 14, 0).withWidth(90),
               juce::Justification::centredLeft, false);
    const auto meter = juce::Rectangle<float>(float(footer.getRight() - 36), float(footer.getY() + 12), 10.0f, float(footer.getHeight() - 24));
    g.setColour(colours::panelEdge);
    g.fillRoundedRectangle(meter, 3.0f);
    const float masterLevel = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(masterMeter, -60.0f) + 60.0f) / 60.0f);
    g.setColour(masterMeter > 0.85f ? colours::danger : colours::cyan);
    g.fillRoundedRectangle(meter.withTop(meter.getBottom() - meter.getHeight() * masterLevel), 3.0f);

    if (detailOpen) {
        paintDetail(g);
    } else {
        g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)).withExtraKerningFactor(0.15f));
        g.setColour(colours::brass.withAlpha(0.85f));
        g.drawText("LIVING GLOBE", globePanel.reduced(14, 8).withHeight(16), juce::Justification::centredLeft, false);
        paintGlobe(g, globeArea);
        paintCards(g);
    }
}

void WorldbuilderEditor::paintPlaceStrips(juce::Graphics& g) {
    for (int slot = 0; slot < placeSlotCount; ++slot) {
        const auto& s = placeStrips[std::size_t(slot)];
        const int type = placeType(slot);
        const auto row = s.row.toFloat();
        if (type == NoPlace) {
            g.setColour(colours::panelEdge.withAlpha(0.6f));
            g.drawRoundedRectangle(row.reduced(1.0f), 6.0f, 1.0f);
            g.setColour(colours::dim);
            g.setFont(juce::Font(juce::FontOptions(12.0f)));
            g.drawText("Slot " + juce::String(slot + 1) + ": empty  -  click a Place card to add one", s.row, juce::Justification::centred, false);
            continue;
        }
        const auto accent = placeColour(type);
        g.setColour(accent.withAlpha(detail.slot == slot ? 0.22f : 0.1f));
        g.fillRoundedRectangle(row, 6.0f);
        g.setColour(accent.withAlpha(0.6f));
        g.drawRoundedRectangle(row.reduced(0.5f), 6.0f, 1.0f);
        auto inner = s.row.reduced(6, 4);
        auto top = inner.removeFromTop(22).withTrimmedRight(128);   // the buttons sit on the right
        ui::drawPlaceIcon(g, type, top.removeFromLeft(24).toFloat(), colours::parchment, accent.withAlpha(0.6f));
        top.removeFromLeft(4);
        g.setColour(colours::parchment);
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText(placeNames[std::size_t(type)], top.removeFromLeft(76), juce::Justification::centredLeft, false);
        const int character = juce::roundToInt(param(placeParam(slot, PlCharacter)));
        g.setColour(accent);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.drawText(placeInfo(type).characters[std::size_t(std::clamp(character, 0, 2))], top, juce::Justification::centredLeft, true);
        auto labels = inner.removeFromTop(14);
        const int w = labels.getWidth() / 3;
        g.setColour(colours::dim);
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)).withExtraKerningFactor(0.1f));
        for (const char* text : { "LEVEL", "DISTANCE", "PAN" }) g.drawText(text, labels.removeFromLeft(w).reduced(4, 0), juce::Justification::bottomLeft, false);
        // Activity meter under the sliders.
        const auto meterArea = s.level.getBounds().toFloat().withY(float(s.level.getBottom()) + 1.0f).withHeight(2.5f);
        const float level = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(placeMeters[std::size_t(slot)], -60.0f) + 60.0f) / 60.0f);
        g.setColour(accent.withAlpha(0.85f));
        g.fillRoundedRectangle(meterArea.withWidth(meterArea.getWidth() * level), 1.0f);
    }
}

void WorldbuilderEditor::paintCards(juce::Graphics& g) {
    g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)).withExtraKerningFactor(0.15f));
    g.setColour(colours::brass.withAlpha(0.85f));
    g.drawText("ADD A PLACE", cardsPanel.reduced(14, 8).withHeight(16), juce::Justification::centredLeft, false);
    if (juce::Time::getMillisecondCounterHiRes() < fullMessageUntil) {
        g.setColour(colours::danger);
        g.drawText("All four slots are in use: remove a Place first", cardsPanel.reduced(14, 8).withHeight(16), juce::Justification::centredRight, false);
    }
    for (int i = 0; i < 10; ++i) {
        const int type = i + 1;
        const auto card = cards[std::size_t(i)].toFloat();
        const bool built = placeImplemented[std::size_t(type)];
        const auto accent = placeColour(type);
        int count = 0;
        for (int slot = 0; slot < placeSlotCount; ++slot) count += placeType(slot) == type ? 1 : 0;
        const bool hover = hoverCard == i && built;
        g.setColour((built ? colours::panel.brighter(hover ? 0.25f : 0.1f) : colours::panel).withAlpha(0.95f));
        g.fillRoundedRectangle(card, 8.0f);
        g.setColour(built ? accent.withAlpha(hover ? 0.95f : 0.55f) : colours::panelEdge);
        g.drawRoundedRectangle(card.reduced(0.5f), 8.0f, hover ? 1.6f : 1.0f);
        auto art = card.reduced(10.0f, 6.0f).withTrimmedBottom(18.0f);
        ui::drawPlaceIcon(g, type, art, built ? colours::parchment.withAlpha(0.95f) : colours::dim.withAlpha(0.6f),
                          built ? accent.withAlpha(0.55f) : colours::panelEdge);
        g.setColour(built ? colours::parchment : colours::dim);
        g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)));
        g.drawText(placeNames[std::size_t(type)], card.withTrimmedTop(card.getHeight() - 20.0f), juce::Justification::centred, false);
        if (!built) {
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.setColour(colours::dim);
            g.drawText("MILESTONE 4", card.reduced(6.0f, 4.0f).withHeight(12.0f), juce::Justification::centredRight, false);
        } else if (count > 0) {
            const auto badge = juce::Rectangle<float>(16.0f, 16.0f).withPosition(card.getRight() - 20.0f, card.getY() + 4.0f);
            g.setColour(accent);
            g.fillEllipse(badge);
            g.setColour(colours::background);
            g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)));
            g.drawText(juce::String(count), badge, juce::Justification::centred, false);
        }
    }
}

void WorldbuilderEditor::paintDetail(juce::Graphics& g) {
    const auto& d = detail;
    const int type = placeType(d.slot);
    const auto accent = placeColour(type);
    g.setColour(colours::panel.withAlpha(0.97f));
    g.fillRoundedRectangle(d.area.toFloat(), 10.0f);
    g.setColour(accent.withAlpha(0.7f));
    g.drawRoundedRectangle(d.area.toFloat().reduced(0.5f), 10.0f, 1.2f);

    auto title = d.titleArea;
    ui::drawPlaceIcon(g, type, title.removeFromLeft(52).toFloat(), colours::parchment, accent.withAlpha(0.6f));
    title.removeFromLeft(10);
    g.setColour(colours::parchment);
    g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)).withExtraKerningFactor(0.12f));
    g.drawText(juce::String(placeNames[std::size_t(type)]).toUpperCase(), title.removeFromTop(28), juce::Justification::centredLeft, false);
    g.setColour(colours::dim);
    g.setFont(juce::Font(juce::FontOptions(11.5f)));
    g.drawFittedText("Place " + juce::String(d.slot + 1) + "  -  " + placeInfo(type).description, title.withTrimmedRight(80),
                     juce::Justification::topLeft, 2);

    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)).withExtraKerningFactor(0.15f));
    g.setColour(colours::brass.withAlpha(0.85f));
    g.drawText("CHARACTER", d.characterArea.withWidth(90), juce::Justification::centredLeft, false);
    g.drawText("RHYTHM", d.rhythmArea.withHeight(14), juce::Justification::centredLeft, false);
    g.drawText("SEED", d.seed.getBounds().translated(0, -15).withHeight(14), juce::Justification::centredLeft, false);
    g.drawText("ELEMENTS", d.elementsArea.withHeight(14), juce::Justification::centredLeft, false);
    g.setColour(colours::dim);
    g.setFont(juce::Font(juce::FontOptions(10.5f)));
    g.drawText("Level, distance and pan are on the strip. Population scales Activity and the effect depth.",
               d.elementsArea.withHeight(14).withTrimmedLeft(90), juce::Justification::centredLeft, true);
}

// Placeholder globe: shaded sphere coloured by the world, turning meridians, clouds,
// weather, lava, lightning, and the active Places as landmarks.
void WorldbuilderEditor::paintGlobe(juce::Graphics& g, juce::Rectangle<float> area) {
    const auto& world = processor.engine.frame();
    const float temperature = world[Temperature], danger = world[Danger], weather = world[Weather], magic = world[Magic];
    const float alpha = 0.25f + 0.75f * processor.engine.powerLevel.load(std::memory_order_relaxed);
    const auto centre = area.getCentre();
    const float r = area.getWidth() * 0.5f;

    const auto glow = colours::cyan.interpolatedWith(juce::Colour(0xffb78cff), magic).withAlpha(0.18f + 0.3f * magic);
    g.setGradientFill(juce::ColourGradient(glow, centre.x, centre.y, glow.withAlpha(0.0f), centre.x + r * 1.18f, centre.y, true));
    g.fillEllipse(area.expanded(r * 0.18f));

    const auto base = globeColour(temperature, danger);
    juce::ColourGradient sphere(base.brighter(0.6f).withMultipliedAlpha(alpha), centre.x - r * 0.4f, centre.y - r * 0.45f,
                                base.darker(1.6f).withMultipliedAlpha(alpha), centre.x + r * 0.7f, centre.y + r * 0.8f, true);
    g.setGradientFill(sphere);
    g.fillEllipse(area);

    {
        juce::Graphics::ScopedSaveState clip(g);
        juce::Path disc;
        disc.addEllipse(area);
        g.reduceClipRegion(disc);

        g.setColour(colours::parchment.withAlpha(0.18f * alpha));
        for (int k = 0; k < 6; ++k) {
            const float phase = spin + float(k) * juce::MathConstants<float>::pi / 6.0f;
            const float w = std::abs(std::cos(phase)) * r;
            g.drawEllipse(juce::Rectangle<float>(w * 2.0f, r * 2.0f).withCentre(centre), 1.0f);
        }
        for (int k = 1; k < 4; ++k) {
            const float y = r * float(k) / 4.0f;
            const float half = std::sqrt(r * r - y * y);
            g.drawHorizontalLine(int(centre.y - y), centre.x - half, centre.x + half);
            g.drawHorizontalLine(int(centre.y + y), centre.x - half, centre.x + half);
        }
        g.drawHorizontalLine(int(centre.y), centre.x - r, centre.x + r);

        juce::Random random(7);
        const int clouds = 3 + int(weather * 9.0f);
        for (int i = 0; i < clouds; ++i) {
            const float lat = (random.nextFloat() - 0.5f) * 1.6f * r;
            const float speed = 0.6f + random.nextFloat();
            const float x = std::fmod(random.nextFloat() * 2.0f * r + spin * speed * r * 0.35f, 2.6f * r) - 1.3f * r;
            const float w = r * (0.25f + 0.3f * random.nextFloat()), h = r * 0.07f;
            g.setColour(juce::Colours::white.interpolatedWith(juce::Colours::grey, weather).withAlpha((0.12f + 0.25f * weather) * alpha));
            g.fillEllipse(centre.x + x - w * 0.5f, centre.y + lat - h * 0.5f, w, h);
        }

        const float lava = juce::jlimit(0.0f, 1.0f, (temperature - 0.72f) / 0.28f);
        if (lava > 0.0f) {
            juce::Random spots(23);
            for (int i = 0; i < 9; ++i) {
                const float lon = spots.nextFloat() * juce::MathConstants<float>::twoPi + spin, lat = (spots.nextFloat() - 0.5f) * 2.2f;
                const float facing = std::cos(lon) * std::cos(lat);
                const float size = r * (0.05f + 0.06f * spots.nextFloat());
                if (facing <= 0.0f) continue;
                const juce::Point<float> p { centre.x + r * std::sin(lon) * std::cos(lat), centre.y + r * std::sin(lat) * 0.9f };
                const auto hot = juce::Colour(0xffffc061).withAlpha(lava * facing * 0.95f * alpha);
                g.setGradientFill(juce::ColourGradient(hot, p.x, p.y, hot.withAlpha(0.0f), p.x + size, p.y, true));
                g.fillEllipse(juce::Rectangle<float>(size * 2.0f, size * 1.4f).withCentre(p));
            }
        }

        const auto water = WaterLayer::amounts(world);
        const float slant = 0.15f + 0.6f * weather;
        if (water.rain > 0.02f) {
            juce::Random drops(41);
            g.setColour(colours::cyan.interpolatedWith(colours::parchment, 0.4f).withAlpha((0.15f + 0.3f * water.rain) * alpha));
            const int count = 10 + int(70.0f * water.rain);
            for (int i = 0; i < count; ++i) {
                const float x = (drops.nextFloat() * 2.4f - 1.2f) * r, speed = 0.8f + 0.5f * drops.nextFloat();
                const float y = std::fmod(drops.nextFloat() * 2.0f * r + precipitation * speed * r, 2.0f * r) - r;
                const float len = r * (0.06f + 0.05f * water.rain);
                g.drawLine(centre.x + x - slant * y, centre.y + y, centre.x + x - slant * (y + len), centre.y + y + len, 1.0f);
            }
        }
        if (water.snow > 0.02f) {
            juce::Random flakes(43);
            g.setColour(juce::Colours::white.withAlpha((0.25f + 0.4f * water.snow) * alpha));
            const int count = 10 + int(50.0f * water.snow);
            for (int i = 0; i < count; ++i) {
                const float x = (flakes.nextFloat() * 2.2f - 1.1f) * r;
                const float y = std::fmod(flakes.nextFloat() * 2.0f * r + precipitation * 0.25f * r, 2.0f * r) - r;
                const float sway = 0.03f * r * std::sin(precipitation * 2.0f + float(i));
                g.fillEllipse(centre.x + x + sway - slant * y * 0.3f, centre.y + y, 2.2f, 2.2f);
            }
        }

        g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, centre.x - r * 0.1f, centre.y,
                                               juce::Colours::black.withAlpha(0.55f), centre.x + r, centre.y + r * 0.3f, false));
        g.fillEllipse(area);
    }
    g.setColour(colours::brass.withAlpha(0.5f));
    g.drawEllipse(area, 1.2f);

    if (flash > 0.02f) {
        g.setColour(juce::Colour(0xffdff6ff).withAlpha(0.3f * flash));
        g.fillEllipse(area);
        juce::Random bolt(boltSeed);
        juce::Path path;
        juce::Point<float> p { centre.x + (bolt.nextFloat() - 0.5f) * r, centre.y - r * (0.55f + 0.3f * bolt.nextFloat()) };
        path.startNewSubPath(p);
        for (int i = 0; i < 7; ++i) {
            p += { (bolt.nextFloat() - 0.5f) * r * 0.22f, r * (0.08f + 0.06f * bolt.nextFloat()) };
            path.lineTo(p);
        }
        g.setColour(juce::Colour(0xffe8f8ff).withAlpha(0.9f * flash));
        g.strokePath(path, juce::PathStrokeType(1.6f));
        g.setColour(colours::cyan.withAlpha(0.35f * flash));
        g.strokePath(path, juce::PathStrokeType(5.0f));
    }

    // Landmarks: each active Place where it sounds (pan left to right, near at the
    // front, far towards the horizon). Far ones are smaller and dimmer; they glow
    // with their activity. Drag a landmark to move the Place.
    std::array<int, placeSlotCount> order { 0, 1, 2, 3 };
    std::sort(order.begin(), order.end(), [this](int a, int b) { return param(placeParam(a, PlDistance)) > param(placeParam(b, PlDistance)); });
    for (int slot : order) {
        const int type = placeType(slot);
        if (type == NoPlace) continue;
        const float d = param(placeParam(slot, PlDistance));
        const auto p = landmarkPosition(slot);
        const float size = r * lerp(0.34f, 0.18f, d);
        const auto accent = placeColour(type);
        const float glowLevel = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(placeMeters[std::size_t(slot)], -60.0f) + 50.0f) / 40.0f);
        const auto halo = accent.withAlpha((0.15f + 0.45f * glowLevel) * alpha * lerp(1.0f, 0.5f, d));
        g.setGradientFill(juce::ColourGradient(halo, p.x, p.y, halo.withAlpha(0.0f), p.x + size * 0.9f, p.y, true));
        g.fillEllipse(juce::Rectangle<float>(size * 1.8f, size * 1.8f).withCentre(p));
        ui::drawPlaceIcon(g, type, juce::Rectangle<float>(size, size).withCentre(p.translated(0, -size * 0.1f)),
                          colours::parchment.withAlpha(alpha * lerp(1.0f, 0.55f, d)), accent.withAlpha(0.75f * alpha));
        if (dragLandmark == slot) {
            g.setColour(colours::cyan.withAlpha(0.8f));
            g.drawEllipse(juce::Rectangle<float>(size * 1.3f, size * 1.3f).withCentre(p), 1.2f);
        }
    }
    g.setColour(colours::dim);
    g.setFont(juce::Font(juce::FontOptions(10.5f)));
    g.drawText("LISTENER", juce::Rectangle<float>(80.0f, 14.0f).withCentre({ centre.x, area.getBottom() + 12.0f }), juce::Justification::centred, false);
}

juce::Point<float> WorldbuilderEditor::landmarkPosition(int slot) const {
    const auto centre = globeArea.getCentre();
    const float r = globeArea.getWidth() * 0.5f;
    const float pan = param(placeParam(slot, PlPan)), d = param(placeParam(slot, PlDistance));
    return { centre.x + pan * r * 0.75f * lerp(1.0f, 0.75f, d), centre.y + r * 0.6f - d * r * 1.15f };
}

int WorldbuilderEditor::landmarkAt(juce::Point<float> p) const {
    const float r = globeArea.getWidth() * 0.5f;
    int best = -1;
    float bestDistance = r * 0.16f;
    for (int slot = 0; slot < placeSlotCount; ++slot) {
        if (placeType(slot) == NoPlace) continue;
        const float dist = landmarkPosition(slot).getDistanceFrom(p);
        if (dist < bestDistance) {
            bestDistance = dist;
            best = slot;
        }
    }
    return best;
}

int WorldbuilderEditor::cardAt(juce::Point<int> p) const {
    for (int i = 0; i < 10; ++i)
        if (cards[std::size_t(i)].contains(p)) return i;
    return -1;
}

void WorldbuilderEditor::mouseDown(const juce::MouseEvent& e) {
    if (detail.slot >= 0) return;
    const int card = cardAt(e.getPosition());
    if (card >= 0) {
        addPlace(card + 1);
        return;
    }
    dragLandmark = landmarkAt(e.position);
    if (dragLandmark >= 0) {
        for (int field : { PlPan, PlDistance })
            if (auto* parameter = processor.parameters.getParameter(specs[std::size_t(placeParam(dragLandmark, field))].id)) parameter->beginChangeGesture();
        if (e.getNumberOfClicks() >= 2) openDetail(dragLandmark);
    }
}

void WorldbuilderEditor::mouseDrag(const juce::MouseEvent& e) {
    if (dragLandmark < 0) return;
    const auto centre = globeArea.getCentre();
    const float r = globeArea.getWidth() * 0.5f;
    const float d = juce::jlimit(0.0f, 1.0f, (centre.y + r * 0.6f - e.position.y) / (r * 1.15f));
    const float pan = juce::jlimit(-1.0f, 1.0f, (e.position.x - centre.x) / (r * 0.75f * lerp(1.0f, 0.75f, d)));
    for (auto [field, value] : { std::pair<int, float> { PlPan, pan }, { PlDistance, d } })
        if (auto* parameter = processor.parameters.getParameter(specs[std::size_t(placeParam(dragLandmark, field))].id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    repaint(globePanel);
}

void WorldbuilderEditor::mouseUp(const juce::MouseEvent&) {
    if (dragLandmark >= 0)
        for (int field : { PlPan, PlDistance })
            if (auto* parameter = processor.parameters.getParameter(specs[std::size_t(placeParam(dragLandmark, field))].id)) parameter->endChangeGesture();
    dragLandmark = -1;
}

void WorldbuilderEditor::mouseMove(const juce::MouseEvent& e) {
    const int card = detail.slot >= 0 ? -1 : cardAt(e.getPosition());
    if (card != hoverCard) {
        hoverCard = card;
        repaint(cardsPanel);
    }
    const bool overLandmark = detail.slot < 0 && landmarkAt(e.position) >= 0;
    const bool clickable = (card >= 0 && placeImplemented[std::size_t(card + 1)]) || overLandmark;
    setMouseCursor(clickable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (card >= 0) {
        const int type = card + 1;
        setHelpText(placeInfo(type).description);
    }
}

void WorldbuilderEditor::mouseExit(const juce::MouseEvent&) {
    if (hoverCard != -1) {
        hoverCard = -1;
        repaint(cardsPanel);
    }
}

// Detail panel: names follow the Place type and the chosen character.
void WorldbuilderEditor::updateDetailLabels() {
    if (detail.slot < 0) return;
    const int type = placeType(detail.slot);
    const auto& info = placeInfo(type);
    if (type != detail.shownType) {
        detail.shownType = type;
        for (int k = 0; k < 3; ++k) detail.character[std::size_t(k)].setButtonText(info.characters[std::size_t(k)]);
        for (int e = 0; e < placeElements; ++e) {
            detail.elementOn[std::size_t(e)].setButtonText(info.elements[std::size_t(e)]);
            detail.element[std::size_t(e)].setTooltip(juce::String(info.elements[std::size_t(e)]) + " level");
        }
        repaint(detail.area);
    }
    const int character = juce::roundToInt(param(placeParam(detail.slot, PlCharacter)));
    for (int k = 0; k < 3; ++k) detail.character[std::size_t(k)].setToggleState(k == character, juce::dontSendNotification);
}

void WorldbuilderEditor::timerCallback() {
    if (!isShowing()) return;
    spin += 0.004f + 0.02f * processor.engine.frame()[Activity];
    if (spin > juce::MathConstants<float>::twoPi * 100.0f) spin = 0;
    precipitation += 0.05f + 0.06f * processor.engine.frame()[Weather];
    if (precipitation > 1000.0f) precipitation = 0;
    const float strike = processor.engine.lightning.load(std::memory_order_relaxed);
    if (strike > flash + 0.2f) ++boltSeed;
    flash = strike;
    for (std::size_t i = 0; i < meters.size(); ++i) meters[i] = processor.engine.layerMeter[i].load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < placeMeters.size(); ++i) placeMeters[i] = processor.engine.placeMeter[i].load(std::memory_order_relaxed);
    masterMeter = processor.engine.masterMeter.load(std::memory_order_relaxed);
    refreshPlaceStrips();

    if (detail.slot >= 0) updateDetailLabels();
    else repaint(globePanel);
    repaint(layersPanel);
    repaint(placesPanel);
    repaint(footer.withTrimmedLeft(footer.getWidth() - 50));
}
