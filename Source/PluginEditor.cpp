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
juce::Colour placeColour(int type) { return ui::placeAccent(type); }

// A weighted blend of colours.
juce::Colour blend(const juce::Colour* c, const float* w, int n) {
    float r = 0, g = 0, b = 0, total = 0;
    for (int i = 0; i < n; ++i) {
        r += c[i].getFloatRed() * w[i];
        g += c[i].getFloatGreen() * w[i];
        b += c[i].getFloatBlue() * w[i];
        total += w[i];
    }
    if (total <= 0.0f) return c[0];
    return juce::Colour::fromFloatRGBA(r / total, g / total, b / total, 1.0f);
}

constexpr const char* headerText = "FILM-SCORING SOUNDSCAPE INSTRUMENT  -  MILESTONE 6 PREVIEW";

// Short names for the category chips.
constexpr std::array<const char*, PlaceCategoryCount + 1> chipNames { "ALL", "FANTASY", "MODERN", "SCI-FI", "NATURE" };

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
    detail.mood.setTooltip("This Place suggests a Mood. Click to use it (the Mood stays your choice).");
    detail.mood.onClick = [this] {
        if (detail.slot >= 0)
            if (const int m = placeInfo(placeType(detail.slot)).mood; m >= 0) setParam(PMood, float(m));
    };
    addChildComponent(detail.mood);
    detail.patterns.setTooltip("Open the Pattern Editor: edit the notes each element of this Place plays");
    detail.patterns.setColour(juce::TextButton::buttonColourId, colours::brass.withAlpha(0.35f));
    detail.patterns.onClick = [this] {
        if (detail.slot >= 0) openPatterns(detail.slot);
    };
    addChildComponent(detail.patterns);
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
    setupCombo(playMode, PPlayMode, playModeAttachment);
    playMode.setTooltip("Transport: plays only while the host plays, from silence and locked to the song position, so bounces and exports "
                        "always contain it. Free: plays all the time (GarageBand exports only up to the last region).");
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

    patterns = std::make_unique<ui::PatternEditor>(processor);
    patterns->onClose = [this] {
        const int slot = patterns->slot();
        patterns->setVisible(false);
        openDetail(slot);
    };
    addChildComponent(*patterns);

    for (int m = 0; m < ui::OverlayModeCount; ++m) overlayChoice.addItem(ui::overlayModeNames[std::size_t(m)], m + 1);
    overlayChoice.setTooltip("Place overlays: each Place decorates the edges of the interface (grass and ivy, neon, wooden beams...). "
                             "Full, Subtle or Off. They never cover the controls.");
    overlayChoice.onChange = [this] { setOverlayMode(overlayChoice.getSelectedId() - 1); };
    addAndMakeVisible(overlayChoice);
    // The overlays sit above everything and never take the mouse.
    overlays = std::make_unique<ui::PlaceOverlays>();
    addAndMakeVisible(*overlays);
    setOverlayMode(int(processor.parameters.state.getProperty("overlays", ui::defaultOverlayMode)));

    themeTint = colours::panel;
    themeGlow = colours::cyan;
    themeSky = colours::background;
    listCards();
    setSize(baseWidth, baseHeight);
    refreshPlaceStrips();
    updateTheme(0.0f, true);
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
        // A fresh Place: every control at its default (each instrument's effects at
        // that instrument's own), a new seed.
        for (int field = 0; field < PlaceParamCount; ++field)
            if (field >= PlFx) setParam(placeParam(slot, field), instrumentFxDefault(type, (field - PlFx) / FxFieldCount, (field - PlFx) % FxFieldCount));
            else if (field != PlType && field != PlPan) setParam(placeParam(slot, field), specs[std::size_t(placeParam(slot, field))].def);
        setParam(placeParam(slot, PlSeed), float(juce::Random::getSystemRandom().nextInt({ 1, 10000 })));
        setParam(placeParam(slot, PlType), float(type));
        refreshPlaceStrips();
        return slot;
    }
    fullMessageUntil = juce::Time::getMillisecondCounterHiRes() + 2500.0;
    repaint(cardsPanel);
    return -1;
}

void WorldbuilderEditor::setOverlayMode(int mode) {
    mode = juce::jlimit(0, ui::OverlayModeCount - 1, mode);
    overlays->setMode(mode);
    overlayChoice.setSelectedId(mode + 1, juce::dontSendNotification);
    processor.parameters.state.setProperty("overlays", mode, nullptr);
}

void WorldbuilderEditor::openPatterns(int slot, int element) {
    if (slot < 0 || slot >= placeSlotCount || !hasPatterns(placeType(slot))) return;
    closeDetail();
    patterns->setVisible(true);
    patterns->toFront(false);
    overlays->toFront(false);
    patterns->open(slot, element);
    repaint();
}

void WorldbuilderEditor::openDetail(int slot) {
    if (slot < 0 || slot >= placeSlotCount || placeType(slot) == NoPlace) return;
    closeDetail();
    if (patterns->isVisible()) patterns->setVisible(false);
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
    d.patterns.setVisible(hasPatterns(placeType(slot)));
    d.mood.setVisible(placeInfo(placeType(slot)).mood >= 0);
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
                                                             &d.seed, &d.sync, &d.dice, &d.close, &d.patterns, &d.mood })
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
    {
        // The header's text, and the open space between it and POWER (the overlays may use it).
        const auto width = [](const juce::Font& font, const char* text) { return juce::roundToInt(juce::GlyphArrangement::getStringWidth(font, text)) + 8; };
        headerTitle = header.withTrimmedLeft(4).withWidth(width(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)).withExtraKerningFactor(0.18f), "WORLDBUILDER"));
        headerSubtitle = juce::Rectangle<int>(header.getX() + 300, header.getCentreY() - 9,
                                              width(juce::Font(juce::FontOptions(12.0f)).withExtraKerningFactor(0.08f), headerText), 18);
        headerOpen = header.withLeft(headerSubtitle.getRight() + 16).withRight(power.getX() - 16);
        if (headerOpen.getWidth() < 60) headerOpen = {};
    }
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

    // Place cards: category chips on the title row, then a scrolling grid, five across.
    {
        auto top = cardsPanel.reduced(10, 6).removeFromTop(20);
        for (int k = PlaceCategoryCount; k >= 0; --k) {
            const int w = k == 0 ? 40 : 64;
            categoryChips[std::size_t(k)] = top.removeFromRight(w).reduced(2, 2);
        }
        cardGrid = cardsPanel.reduced(8, 6).withTrimmedTop(22);
        cardScroll = juce::jlimit(0.0f, maxCardScroll(), cardScroll);
    }

    layoutDetail();
    patterns->setBounds(controlsPanel.getUnion(globePanel).getUnion(cardsPanel));

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
    top.removeFromLeft(10);
    playMode.setBounds(top.removeFromLeft(100).withSizeKeepingCentre(100, 22));
    seed.setBounds(bottom.removeFromLeft(230).withSizeKeepingCentre(230, 20));
    bottom.removeFromLeft(10);
    dice.setBounds(bottom.removeFromLeft(110).withSizeKeepingCentre(110, 22));
    {
        f.removeFromLeft(14);
        auto column = f.removeFromLeft(100);
        overlayChoice.setBounds(column.removeFromTop(column.getHeight() / 2).withTrimmedTop(14).withSizeKeepingCentre(100, 22));
    }
    f.removeFromRight(40);   // master meter
    for (int param : { PMaster, PWidth, PSpaceMix })
        knobFor[std::size_t(param)]->setBounds(f.removeFromRight(92).withSizeKeepingCentre(78, 66));
    overlays->setBounds(getLocalBounds());
    frameOutlines.clear();
    overlayKeepClear.clear();
    updateOverlayFrames();
}

void WorldbuilderEditor::layoutDetail() {
    auto& d = detail;
    d.area = globePanel.getUnion(cardsPanel);
    auto r = d.area.reduced(16, 10);
    // The header: the Place's scene, its name and buttons on top, its description below.
    d.titleArea = r.removeFromTop(86);
    d.close.setBounds(d.titleArea.withHeight(30).withTrimmedLeft(d.titleArea.getWidth() - 70).withSizeKeepingCentre(66, 24));
    d.patterns.setBounds(d.close.getBounds().translated(-96, 0).withWidth(88));
    d.mood.setBounds(d.patterns.getBounds().translated(-134, 0).withWidth(126));
    r.removeFromTop(4);
    d.characterArea = r.removeFromTop(34);
    {
        auto row = d.characterArea.withTrimmedLeft(90);
        const int w = row.getWidth() / 3;
        for (auto& b : d.character) b.setBounds(row.removeFromLeft(w).reduced(4, 4));
    }
    r.removeFromTop(8);
    auto knobsRow = r.removeFromTop(90);
    {
        const int w = knobsRow.getWidth() / 5;
        for (auto* k : { &d.activity, &d.tension, &d.tone, &d.size, &d.variation }) k->setBounds(knobsRow.removeFromLeft(w).reduced(8, 0));
    }
    r.removeFromTop(6);
    d.rhythmArea = r.removeFromTop(88);
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
    d.elementsArea = r.removeFromTop(20 + placeElements * 24);
    {
        auto rows = d.elementsArea.withTrimmedTop(20);
        for (int e = 0; e < placeElements; ++e) {
            auto row = rows.removeFromTop(24);
            row.removeFromLeft(24);   // the instrument's glyph
            d.elementOn[std::size_t(e)].setBounds(row.removeFromLeft(130).reduced(2, 2));
            row.removeFromLeft(10);
            d.element[std::size_t(e)].setBounds(row.reduced(0, 5));
        }
    }
}

//==============================================================================
void WorldbuilderEditor::paint(juce::Graphics& g) {
    // The world's colours: the active Places set the background, the panels' tint and
    // the glow (blended by level, see updateTheme).
    const float height = float(getHeight());
    g.setGradientFill(juce::ColourGradient(themeSky.brighter(0.25f), 0, 0, themeSky.darker(0.7f), 0, height, false));
    g.fillAll();
    {
        // The Places' light: a wide glow behind the globe and along the horizon.
        const auto c = globePanel.toFloat().getCentre();
        const auto glow = themeGlow.withAlpha(0.06f + 0.16f * themeStrength);
        g.setGradientFill(juce::ColourGradient(glow, c.x, c.y, glow.withAlpha(0.0f), c.x + float(getWidth()) * 0.55f, c.y, true));
        g.fillAll();
        const auto horizon = themeGlow.withAlpha(0.14f * themeStrength);
        g.setGradientFill(juce::ColourGradient(horizon.withAlpha(0.0f), 0, height * 0.55f, horizon, 0, height, false));
        g.fillAll();
    }
    const auto panelColour = themeTint;

    // Text only where the clip reaches: frames often repaint small areas.
    if (g.clipRegionIntersects(headerTitle)) {
        g.setColour(colours::brass);
        g.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)).withExtraKerningFactor(0.18f));
        g.drawText("WORLDBUILDER", header.withTrimmedLeft(4), juce::Justification::centredLeft, false);
    }
    if (g.clipRegionIntersects(headerSubtitle)) {
        g.setColour(colours::dim);
        g.setFont(juce::Font(juce::FontOptions(12.0f)).withExtraKerningFactor(0.08f));
        g.drawText(headerText, header.withTrimmedLeft(300).withTrimmedRight(110), juce::Justification::centredLeft, false);
    }
    g.setColour(colours::brass.withAlpha(0.35f));
    g.drawHorizontalLine(header.getBottom() + 2, float(header.getX()), float(header.getRight()));

    const bool detailOpen = detail.slot >= 0;
    for (auto panel : { controlsPanel, globePanel, cardsPanel, layersPanel, placesPanel, footer }) {
        if ((detailOpen && (panel == globePanel || panel == cardsPanel)) || !g.clipRegionIntersects(panel.expanded(3))) continue;
        const auto r = panel.toFloat();
        // The frame's outlines (glow and edge), stroked once per size.
        auto outline = std::find_if(frameOutlines.begin(), frameOutlines.end(), [&](const FrameOutline& o) { return o.panel == panel; });
        if (outline == frameOutlines.end()) {
            FrameOutline o { panel, {}, {} };
            juce::Path p;
            p.addRoundedRectangle(r.expanded(1.5f), 11.5f);
            juce::PathStrokeType(3.0f).createStrokedPath(o.glow, p);
            p.clear();
            p.addRoundedRectangle(r.reduced(0.5f), 10.0f);
            juce::PathStrokeType(1.0f).createStrokedPath(o.edge, p);
            frameOutlines.push_back(std::move(o));
            outline = frameOutlines.end() - 1;
        }
        if (themeStrength > 0.01f) {   // the frame glows in the Places' light
            g.setColour(themeGlow.withAlpha(0.12f * themeStrength));
            g.fillPath(outline->glow);
        }
        g.setGradientFill(juce::ColourGradient(panelColour.brighter(0.12f).withAlpha(0.9f), 0, r.getY(), panelColour.darker(0.2f).withAlpha(0.9f), 0, r.getBottom(), false));
        g.fillRoundedRectangle(r, 10.0f);
        g.setColour(colours::panelEdge.interpolatedWith(themeGlow, 0.15f + 0.45f * themeStrength));
        g.fillPath(outline->edge);
    }

    g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)).withExtraKerningFactor(0.15f));
    g.setColour(colours::brass.withAlpha(0.85f));
    for (std::size_t i = 0; i < groups.size(); ++i)
        if (const auto title = groupAreas[i].withHeight(16).withTrimmedLeft(6); g.clipRegionIntersects(title))
            g.drawText(groups[i].title, title, juce::Justification::centredLeft, false);
    if (g.clipRegionIntersects(layersPanel.reduced(14, 8).withHeight(16)))
        g.drawText("ENVIRONMENT", layersPanel.reduced(14, 8).withHeight(16), juce::Justification::centredLeft, false);
    if (g.clipRegionIntersects(placesPanel.reduced(14, 8).withHeight(16)))
        g.drawText("PLACES", placesPanel.reduced(14, 8).withHeight(16), juce::Justification::centredLeft, false);

    for (int l = 0; l < LayerCount && g.clipRegionIntersects(layersPanel.reduced(ui::PlaceOverlays::frameBand)); ++l) {
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
    if (g.clipRegionIntersects(placesPanel.reduced(ui::PlaceOverlays::frameBand))) paintPlaceStrips(g);

    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)).withExtraKerningFactor(0.12f));
    g.setColour(colours::brass.withAlpha(0.8f));
    for (auto [component, text] : { std::pair<juce::Component*, const char*> { &root, "ROOT" }, { &mood, "MOOD" },
                                    { &quality, "QUALITY" }, { &playMode, "PLAY MODE" }, { &seed, "WORLD SEED" },
                                    { &overlayChoice, "OVERLAYS" } })
        if (const auto label = component->getBounds().translated(0, -15).withHeight(14); g.clipRegionIntersects(label))
            g.drawText(text, label, juce::Justification::centredLeft, false);
    if (const auto tempo = dice.getBounds().translated(dice.getWidth() + 14, 0).withWidth(90); g.clipRegionIntersects(tempo)) {
        g.setColour(colours::dim);
        g.drawText(juce::String(processor.engine.frame().bpm, 1) + " BPM", tempo, juce::Justification::centredLeft, false);
    }
    const auto meter = juce::Rectangle<float>(float(footer.getRight() - 36), float(footer.getY() + 12), 10.0f, float(footer.getHeight() - 24));
    g.setColour(colours::panelEdge);
    g.fillRoundedRectangle(meter, 3.0f);
    const float masterLevel = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(masterMeter, -60.0f) + 60.0f) / 60.0f);
    g.setColour(masterMeter > 0.85f ? colours::danger : colours::cyan);
    g.fillRoundedRectangle(meter.withTop(meter.getBottom() - meter.getHeight() * masterLevel), 3.0f);

    // The expensive parts only where the clip reaches inside their panel (overlay frames
    // repaint small areas, often only a panel's edge).
    if (detailOpen) {
        if (g.clipRegionIntersects(detail.area)) paintDetail(g);
    } else {
        if (g.clipRegionIntersects(globePanel.reduced(ui::PlaceOverlays::frameBand))) {
            g.setFont(juce::Font(juce::FontOptions(11.5f, juce::Font::bold)).withExtraKerningFactor(0.15f));
            g.setColour(colours::brass.withAlpha(0.85f));
            g.drawText("LIVING GLOBE", globePanel.reduced(14, 8).withHeight(16), juce::Justification::centredLeft, false);
            paintGlobe(g, globeArea);
        }
        if (g.clipRegionIntersects(cardsPanel.reduced(ui::PlaceOverlays::frameBand))) paintCards(g);
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
    const auto titleRow = cardsPanel.reduced(14, 6).withHeight(20);
    if (juce::Time::getMillisecondCounterHiRes() < fullMessageUntil) {
        g.setColour(colours::danger);
        g.drawText("All four slots are in use", titleRow, juce::Justification::centredLeft, false);
    } else {
        g.drawText("ADD A PLACE", titleRow, juce::Justification::centredLeft, false);
    }
    // Category chips.
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)).withExtraKerningFactor(0.1f));
    for (int k = 0; k <= PlaceCategoryCount; ++k) {
        const auto chip = categoryChips[std::size_t(k)].toFloat();
        const bool on = category == k - 1;
        g.setColour(on ? colours::brass.withAlpha(0.85f) : colours::panelEdge.withAlpha(0.6f));
        g.fillRoundedRectangle(chip, chip.getHeight() * 0.5f);
        g.setColour(on ? colours::background : colours::parchment.withAlpha(0.8f));
        g.drawText(chipNames[std::size_t(k)], chip, juce::Justification::centred, false);
    }

    juce::Graphics::ScopedSaveState clip(g);
    g.reduceClipRegion(cardGrid);
    for (int i = 0; i < listedCount; ++i) {
        const int type = listed[std::size_t(i)];
        const auto bounds = cardBounds(i);
        if (!bounds.intersects(cardGrid)) continue;
        const bool built = placeImplemented[std::size_t(type)];
        const float hover = hoverAmount[std::size_t(type)];
        // A hovered card lifts a little.
        const auto card = bounds.toFloat().translated(0.0f, -2.0f * hover);
        const auto accent = placeColour(type);
        int count = 0;
        for (int slot = 0; slot < placeSlotCount; ++slot) count += placeType(slot) == type ? 1 : 0;
        if (hover > 0.01f) {
            const auto halo = accent.withAlpha(0.3f * hover);
            g.setColour(halo);
            g.fillRoundedRectangle(card.expanded(2.0f), 9.0f);
        }
        ui::drawPlaceScene(g, type, card, hover, animTime, built);
        g.setColour(built ? accent.withAlpha(0.45f + 0.5f * hover) : colours::panelEdge);
        g.drawRoundedRectangle(card.reduced(0.5f), 6.0f, 1.0f + 0.6f * hover);
        g.setColour(built ? colours::parchment : colours::dim.brighter(0.3f));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawFittedText(placeNames[std::size_t(type)], card.withTrimmedTop(card.getHeight() - 18.0f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
        if (!built) {
            g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
            g.setColour(colours::parchment.withAlpha(0.6f));
            g.drawText(juce::String("IN ") + placeArrives[std::size_t(type)], card.reduced(5.0f, 3.0f).withHeight(11.0f), juce::Justification::centredRight, false);
        } else if (count > 0) {
            const auto badge = juce::Rectangle<float>(15.0f, 15.0f).withPosition(card.getRight() - 18.0f, card.getY() + 3.0f);
            g.setColour(accent);
            g.fillEllipse(badge);
            g.setColour(colours::background);
            g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
            g.drawText(juce::String(count), badge, juce::Justification::centred, false);
        }
    }
    // The scroll position, when the list is longer than the panel.
    if (const float range = maxCardScroll(); range > 0.0f) {
        const auto track = juce::Rectangle<float>(float(cardGrid.getRight()) - 3.0f, float(cardGrid.getY()) + 2.0f, 2.5f, float(cardGrid.getHeight()) - 4.0f);
        const float visible = float(cardGrid.getHeight()) / (float(cardGrid.getHeight()) + range);
        g.setColour(colours::panelEdge);
        g.fillRoundedRectangle(track, 1.2f);
        g.setColour(colours::brass.withAlpha(0.7f));
        g.fillRoundedRectangle(track.withHeight(track.getHeight() * visible).withY(track.getY() + track.getHeight() * (1.0f - visible) * cardScroll / range), 1.2f);
    }
}

void WorldbuilderEditor::listCards() {
    listedCount = 0;
    for (int type = NoPlace + 1; type < PlaceTypeCount; ++type)
        if (category < 0 || placeInfo(type).category == category) listed[std::size_t(listedCount++)] = type;
    cardScroll = juce::jlimit(0.0f, maxCardScroll(), cardScroll);
}

juce::Rectangle<int> WorldbuilderEditor::cardBounds(int index) const {
    const int columns = 5, cardW = cardGrid.getWidth() / columns, cardH = cardGrid.getHeight() / 2;
    return juce::Rectangle<int>(cardGrid.getX() + (index % columns) * cardW, cardGrid.getY() + (index / columns) * cardH - juce::roundToInt(cardScroll), cardW,
                                cardH)
        .reduced(3);
}

float WorldbuilderEditor::maxCardScroll() const {
    const int rows = (listedCount + 4) / 5;
    return std::max(0.0f, float(rows * (cardGrid.getHeight() / 2) - cardGrid.getHeight()));
}

int WorldbuilderEditor::categoryAt(juce::Point<int> p) const {
    for (int k = 0; k <= PlaceCategoryCount; ++k)
        if (categoryChips[std::size_t(k)].contains(p)) return k;
    return -1;
}

void WorldbuilderEditor::paintDetail(juce::Graphics& g) {
    const auto& d = detail;
    const int type = placeType(d.slot);
    const auto accent = placeColour(type);
    const auto& theme = ui::placeTheme(type);
    const auto panel = colours::panel.interpolatedWith(theme.tint, 0.85f);
    g.setGradientFill(juce::ColourGradient(panel.brighter(0.1f).withAlpha(0.97f), 0, float(d.area.getY()), panel.darker(0.25f).withAlpha(0.97f), 0,
                                           float(d.area.getBottom()), false));
    g.fillRoundedRectangle(d.area.toFloat(), 10.0f);
    // Header art: the Place's scene across the top. The left, under the name, is shaded
    // so the text reads; the scene shows clearly on the right.
    {
        const auto banner = d.area.toFloat().reduced(1.5f).withHeight(float(d.titleArea.getBottom() - d.area.getY()) + 4.0f);
        ui::drawPlaceScene(g, type, banner, 1.0f, animTime, true);
        g.setGradientFill(juce::ColourGradient(panel.withAlpha(0.85f), banner.getX(), 0.0f, panel.withAlpha(0.0f), banner.getX() + banner.getWidth() * 0.5f, 0.0f,
                                               false));
        g.fillRect(banner.withHeight(float(d.titleArea.getY() - d.area.getY()) + 32.0f));
        const float strip = float(d.titleArea.getY()) + 52.0f;   // under the description
        g.setGradientFill(juce::ColourGradient(panel.withAlpha(0.0f), 0.0f, strip - 8.0f, panel.withAlpha(0.85f), 0.0f, strip + 4.0f, false));
        g.fillRect(banner.withTop(strip - 8.0f));
        g.setColour(theme.glow.withAlpha(0.6f));
        g.fillRect(banner.withTop(banner.getBottom() - 1.0f).withTrimmedLeft(12.0f).withTrimmedRight(12.0f));
    }
    g.setColour(accent.withAlpha(0.7f));
    g.drawRoundedRectangle(d.area.toFloat().reduced(0.5f), 10.0f, 1.2f);

    auto title = d.titleArea;
    ui::drawPlaceIcon(g, type, title.removeFromLeft(52).withHeight(52).toFloat(), colours::parchment, accent.withAlpha(0.6f));
    title.removeFromLeft(10);
    const int buttonsLeft = (d.mood.isVisible() ? d.mood : d.patterns.isVisible() ? d.patterns : d.close).getX();
    g.setColour(colours::parchment);
    g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)).withExtraKerningFactor(0.12f));
    g.drawFittedText(juce::String(placeNames[std::size_t(type)]).toUpperCase(), title.withHeight(30).withRight(buttonsLeft - 8), juce::Justification::centredLeft, 1, 0.7f);
    g.setColour(colours::parchment.withAlpha(0.8f));
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawFittedText("Place " + juce::String(d.slot + 1) + "  -  " + placeInfo(type).description, title.withTrimmedTop(56).withTrimmedRight(4),
                     juce::Justification::centredLeft, 2);

    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)).withExtraKerningFactor(0.15f));
    g.setColour(colours::brass.withAlpha(0.85f));
    g.drawText("CHARACTER", d.characterArea.withWidth(90), juce::Justification::centredLeft, false);
    g.drawText("RHYTHM", d.rhythmArea.withHeight(14), juce::Justification::centredLeft, false);
    g.drawText("SEED", d.seed.getBounds().translated(0, -15).withHeight(14), juce::Justification::centredLeft, false);
    g.drawText("ELEMENTS", d.elementsArea.withHeight(14), juce::Justification::centredLeft, false);
    g.setColour(colours::dim);
    g.setFont(juce::Font(juce::FontOptions(10.5f)));
    g.drawText(juce::String("Effect: ") + placeInfo(type).effect + " (deeper with Activity and Population)",
               d.elementsArea.withHeight(14).withTrimmedLeft(90), juce::Justification::centredLeft, true);
    for (int e = 0; e < placeElements; ++e) {
        const auto row = d.elementOn[std::size_t(e)].getBounds();
        ui::drawInstrumentIcon(g, placeInfo(type).element[std::size_t(e)].icon, row.withX(row.getX() - 22).withWidth(18).toFloat().reduced(0.0f, 2.0f),
                               d.elementOn[std::size_t(e)].getToggleState() ? accent : colours::dim);
    }
}

// Placeholder globe: shaded sphere coloured by the world, turning meridians, clouds,
// weather, lava, lightning, and the active Places as landmarks.
void WorldbuilderEditor::paintGlobe(juce::Graphics& g, juce::Rectangle<float> area) {
    const auto& world = processor.engine.frame();
    const float temperature = world[Temperature], danger = world[Danger], weather = world[Weather], magic = world[Magic];
    const float alpha = 0.25f + 0.75f * processor.engine.powerLevel.load(std::memory_order_relaxed);
    const auto centre = area.getCentre();
    const float r = area.getWidth() * 0.5f;

    const auto glow = themeGlow.interpolatedWith(juce::Colour(0xffb78cff), magic * 0.6f).withAlpha(0.18f + 0.3f * magic);
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

        paintGlobeFeatures(g, area, alpha);

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
    g.setColour(colours::brass.interpolatedWith(themeGlow, 0.4f).withAlpha(0.5f));
    g.drawEllipse(area, 1.2f);
    // Atmosphere marks outside the disc: a golden aura (temples, palaces) and orbits (ports, ships).
    if (const float aura = featureAmount[ui::GlobeGlow]; aura > 0.01f) {
        const auto c = juce::Colour(0xffffe0a0).withAlpha(0.25f * aura * (0.8f + 0.2f * std::sin(animTime * 1.5f)));
        g.setGradientFill(juce::ColourGradient(c, centre.x, centre.y - r * 1.02f, c.withAlpha(0.0f), centre.x, centre.y - r * 1.35f, false));
        g.drawEllipse(area.expanded(r * 0.06f), r * 0.08f);
    }
    if (const float orbit = featureAmount[ui::GlobeOrbit]; orbit > 0.01f) {
        // Two tilted rings (their front halves; the back halves pass behind the planet),
        // ships with glowing trails.
        for (int ringIndex = 0; ringIndex < 2; ++ringIndex) {
            const float tilt = ringIndex == 0 ? -0.12f : 0.22f, scale = ringIndex == 0 ? 1.0f : 0.86f;
            const auto ring = juce::Rectangle<float>(r * 2.7f * scale, r * 0.55f * scale).withCentre(centre.translated(0, r * 0.08f));
            const auto rotate = juce::AffineTransform::rotation(tilt, centre.x, centre.y);
            juce::Path front;
            front.addCentredArc(ring.getCentreX(), ring.getCentreY(), ring.getWidth() * 0.5f, ring.getHeight() * 0.5f, 0.0f, juce::MathConstants<float>::halfPi,
                                juce::MathConstants<float>::halfPi * 3.0f, true);
            front.applyTransform(rotate);
            g.setColour(colours::cyan.withAlpha(0.25f * orbit * alpha));
            g.strokePath(front, juce::PathStrokeType(5.0f));
            g.setColour(colours::cyan.interpolatedWith(juce::Colours::white, 0.3f).withAlpha(0.8f * orbit * alpha));
            g.strokePath(front, juce::PathStrokeType(1.6f));
            for (int k = 0; k < 2; ++k) {
                const float speed = 0.35f + 0.15f * float(k) + 0.1f * float(ringIndex);
                const float a = animTime * speed + float(k) * 2.4f + float(ringIndex) * 1.1f;
                if (std::sin(a) < 0.0f) continue;   // behind the planet
                for (int trail = 6; trail >= 0; --trail) {
                    const float b = a - float(trail) * 0.035f;
                    const auto ship = juce::Point<float> { ring.getCentreX() + ring.getWidth() * 0.5f * std::cos(b), ring.getCentreY() + ring.getHeight() * 0.5f * std::sin(b) }
                                          .transformedBy(rotate);
                    const float fade = 1.0f - float(trail) / 7.0f, size = trail == 0 ? 5.0f : 3.0f * fade;
                    g.setColour((trail == 0 ? juce::Colours::white : colours::cyan).withAlpha(0.9f * fade * orbit * alpha));
                    g.fillEllipse(juce::Rectangle<float>(size, size).withCentre(ship));
                }
            }
        }
    }

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
        const auto landmark = juce::Rectangle<float>(size, size).withCentre(p.translated(0, -size * 0.1f));
        if (const auto art = ui::artworkLayer(type, "front"); art.isValid()) {
            g.setOpacity(alpha * lerp(1.0f, 0.6f, d));
            g.drawImage(art, landmark, juce::RectanglePlacement::centred);
            g.setOpacity(1.0f);
        } else {
            ui::drawPlaceIcon(g, type, landmark, colours::parchment.withAlpha(alpha * lerp(1.0f, 0.55f, d)), accent.withAlpha(0.75f * alpha));
        }
        if (dragLandmark == slot) {
            g.setColour(colours::cyan.withAlpha(0.8f));
            g.drawEllipse(juce::Rectangle<float>(size * 1.3f, size * 1.3f).withCentre(p), 1.2f);
        }
    }
    g.setColour(colours::dim);
    g.setFont(juce::Font(juce::FontOptions(10.5f)));
    g.drawText("LISTENER", juce::Rectangle<float>(80.0f, 14.0f).withCentre({ centre.x, area.getBottom() + 12.0f }), juce::Justification::centred, false);
    // The air of the active Places drifts across the panel.
    for (int kind = 0; kind < int(particleAmount.size()); ++kind)
        ui::drawParticles(g, kind, globePanel.toFloat().reduced(6.0f), animTime, particleAmount[std::size_t(kind)] * alpha, themeGlow, 3);
}

void WorldbuilderEditor::paintGlobeFeatures(juce::Graphics& g, juce::Rectangle<float> area, float alpha) {
    const auto centre = area.getCentre();
    const float r = area.getWidth() * 0.5f;
    // A point on the turning sphere: where it is drawn and how much it faces the listener.
    const auto at = [&](float lon, float lat, float& facing) {
        const float l = lon + spin;
        facing = std::cos(l) * std::cos(lat);
        return juce::Point<float> { centre.x + r * std::sin(l) * std::cos(lat), centre.y + r * std::sin(lat) * 0.9f };
    };
    const auto spots = [&](int feature, int seed, int count, auto&& draw) {
        const float amount = featureAmount[std::size_t(feature)];
        if (amount <= 0.01f) return;
        juce::Random random(seed);
        for (int i = 0; i < count; ++i) {
            const float lon = random.nextFloat() * juce::MathConstants<float>::twoPi, lat = (random.nextFloat() - 0.5f) * 2.4f;
            const float size = random.nextFloat();
            float facing = 0;
            const auto p = at(lon, lat, facing);
            if (facing <= 0.0f) continue;
            draw(p, facing, amount * alpha, size, i);
        }
    };
    spots(ui::GlobeOcean, 51, 12, [&](juce::Point<float> p, float facing, float a, float size, int) {
        const float w = r * (0.3f + 0.25f * size);
        const auto c = juce::Colour(0xff1f5f9a).withAlpha(0.55f * a * facing);
        g.setGradientFill(juce::ColourGradient(c, p.x, p.y, c.withAlpha(0.0f), p.x + w * 0.6f, p.y, true));
        g.fillEllipse(juce::Rectangle<float>(w * 1.4f, w * 0.8f * (0.3f + 0.7f * facing)).withCentre(p));
        g.setColour(juce::Colours::white.withAlpha(0.25f * a * facing * (0.5f + 0.5f * std::sin(animTime * 2.0f + size * 9.0f))));
        g.fillEllipse(juce::Rectangle<float>(2.0f, 1.2f).withCentre(p.translated(w * 0.15f, -w * 0.1f)));
    });
    spots(ui::GlobeDunes, 53, 16, [&](juce::Point<float> p, float facing, float a, float size, int) {
        const float w = r * (0.12f + 0.12f * size);
        juce::Path dune;
        dune.startNewSubPath(p.x - w, p.y);
        dune.quadraticTo(p.x - w * 0.3f, p.y - w * 0.35f * facing, p.x + w, p.y + w * 0.05f);
        g.setColour(juce::Colour(0xffe8c078).withAlpha(0.55f * a * facing));
        g.strokePath(dune, juce::PathStrokeType(1.6f));
    });
    spots(ui::GlobeFields, 57, 34, [&](juce::Point<float> p, float facing, float a, float size, int i) {
        // Patchwork farmland: a few fields side by side, with hedges.
        const float w = r * (0.07f + 0.07f * size) * (0.35f + 0.65f * facing), h = w * 0.7f * (0.5f + 0.5f * facing);
        for (int k = 0; k < 3; ++k) {
            const auto field = juce::Rectangle<float>(w, h).withCentre(p.translated(float(k - 1) * w * 1.04f, float(k % 2) * h * 0.3f));
            const juce::uint32 tones[] { 0xffd8b84e, 0xff7fae4e, 0xff9cc45a, 0xffb89a44 };
            g.setColour(juce::Colour(tones[(i + k) % 4]).withAlpha(0.7f * a * facing));
            g.fillRect(field);
            g.setColour(juce::Colour(0xff2f4a24).withAlpha(0.6f * a * facing));
            g.drawRect(field, 0.8f);
        }
    });
    spots(ui::GlobeForest, 59, 16, [&](juce::Point<float> p, float facing, float a, float size, int) {
        const float w = r * (0.12f + 0.1f * size);
        const auto c = juce::Colour(0xff2f6a3a).withAlpha(0.6f * a * facing);
        g.setGradientFill(juce::ColourGradient(c, p.x, p.y, c.withAlpha(0.0f), p.x + w * 0.6f, p.y, true));
        g.fillEllipse(juce::Rectangle<float>(w, w * 0.75f * (0.4f + 0.6f * facing)).withCentre(p));
    });
    spots(ui::GlobeCityLights, 61, 70, [&](juce::Point<float> p, float facing, float a, float size, int i) {
        const float twinkle = 0.6f + 0.4f * std::sin(animTime * (2.0f + size * 3.0f) + float(i));
        g.setColour(juce::Colour(i % 4 == 0 ? 0xffff8ad0 : 0xffffe08a).withAlpha(0.8f * a * facing * twinkle));
        g.fillEllipse(juce::Rectangle<float>(1.6f + size, 1.6f + size).withCentre(p));
    });
    spots(ui::GlobeSmoke, 63, 8, [&](juce::Point<float> p, float facing, float a, float size, int i) {
        const float rise = std::fmod(animTime * 0.15f + size, 1.0f);
        const auto q = p.translated(r * 0.04f * std::sin(animTime + float(i)), -r * 0.25f * rise);
        const auto c = juce::Colour(0xff8a8a8a).withAlpha(0.35f * a * facing * (1.0f - rise));
        g.setGradientFill(juce::ColourGradient(c, q.x, q.y, c.withAlpha(0.0f), q.x + r * 0.1f, q.y, true));
        g.fillEllipse(juce::Rectangle<float>(r * 0.2f, r * 0.14f).withCentre(q));
    });
    spots(ui::GlobeHive, 65, 10, [&](juce::Point<float> p, float facing, float a, float size, int i) {
        juce::Path vein;
        vein.startNewSubPath(p);
        vein.quadraticTo(p.x + r * 0.1f * std::sin(size * 6.0f), p.y - r * 0.1f, p.x + r * 0.16f * std::cos(size * 4.0f + float(i)), p.y + r * 0.06f);
        g.setColour(juce::Colour(0xffd07aff).withAlpha(0.5f * a * facing * (0.6f + 0.4f * std::sin(animTime * 3.0f + float(i)))));
        g.strokePath(vein, juce::PathStrokeType(1.4f));
    });
    spots(ui::GlobeCrystals, 67, 16, [&](juce::Point<float> p, float facing, float a, float size, int i) {
        const float s2 = 2.0f + 3.0f * size;
        juce::Path gem;
        gem.addTriangle(p.x, p.y - s2, p.x + s2 * 0.6f, p.y, p.x - s2 * 0.6f, p.y);
        gem.addTriangle(p.x - s2 * 0.6f, p.y, p.x + s2 * 0.6f, p.y, p.x, p.y + s2 * 0.8f);
        g.setColour(juce::Colour(0xff7ae8e0).withAlpha(0.7f * a * facing * (0.5f + 0.5f * std::sin(animTime * 2.5f + float(i)))));
        g.fillPath(gem);
    });
    spots(ui::GlobeRuins, 69, 12, [&](juce::Point<float> p, float facing, float a, float size, int) {
        const float w = 2.0f + 3.0f * size;
        g.setColour(juce::Colour(0xff9a9a90).withAlpha(0.5f * a * facing));
        g.fillRect(juce::Rectangle<float>(w, w * 1.6f).withCentre(p));
        g.fillRect(juce::Rectangle<float>(w * 0.8f, w).withCentre(p.translated(w * 1.3f, w * 0.3f)));
    });
}

void WorldbuilderEditor::updateTheme(float seconds, bool snap) {
    // Each Place's presence follows its level (0 dB: full, -36 dB: none; nothing when
    // muted or removed) and moves there linearly in about 1.2 s, so Places fade in and
    // out of the look, the globe and the overlays together.
    std::array<float, PlaceTypeCount> target {};
    for (int slot = 0; slot < placeSlotCount; ++slot) {
        const int type = placeType(slot);
        if (type <= NoPlace || type >= PlaceTypeCount || param(placeParam(slot, PlMute)) > 0.5f) continue;
        const float level = juce::jlimit(0.0f, 1.0f, (param(placeParam(slot, PlLevel)) + 36.0f) / 36.0f);
        target[std::size_t(type)] = std::min(1.0f, target[std::size_t(type)] + level);
    }
    themeMoving = false;
    const float step = seconds / 1.2f;
    for (std::size_t type = 0; type < presence.size(); ++type) {
        auto& p = presence[type];
        if (p == target[type]) continue;
        p = snap ? target[type] : target[type] > p ? std::min(target[type], p + step) : std::max(target[type], p - step);
        themeMoving = true;
    }

    // Blend the present Places' palettes (eased); the plain look fills in what they leave.
    std::array<juce::Colour, PlaceTypeCount> tints, glows, backdrops;
    std::array<float, PlaceTypeCount> weights {};
    std::array<float, ui::GlobeFeatureCount> features {};
    std::array<float, 8> particles {};
    float total = 0;
    for (int type = NoPlace + 1; type < PlaceTypeCount; ++type) {
        const auto& t = ui::placeTheme(type);
        const float p = presence[std::size_t(type)], w = p * p * (3.0f - 2.0f * p);
        tints[std::size_t(type)] = t.tint;
        glows[std::size_t(type)] = t.glow;
        backdrops[std::size_t(type)] = t.backdrop;
        weights[std::size_t(type)] = w;
        total += w;
        features[std::size_t(t.globe)] = std::max(features[std::size_t(t.globe)], w);
        particles[std::size_t(t.particles)] = std::max(particles[std::size_t(t.particles)], w);
        overlays->setAmount(type, p);
    }
    themeStrength = std::min(1.0f, total);
    const auto& plain = ui::placeTheme(NoPlace);
    tints[0] = plain.tint;
    glows[0] = plain.glow;
    backdrops[0] = plain.backdrop;
    weights[0] = total > 0.0f ? 0.0f : 1.0f;
    themeTint = plain.tint.interpolatedWith(blend(tints.data(), weights.data(), PlaceTypeCount), themeStrength);
    themeGlow = plain.glow.interpolatedWith(blend(glows.data(), weights.data(), PlaceTypeCount), themeStrength);
    themeSky = plain.backdrop.interpolatedWith(blend(backdrops.data(), weights.data(), PlaceTypeCount), themeStrength);
    featureAmount = features;
    for (std::size_t i = 0; i < particleAmount.size(); ++i) particleAmount[i] = particles[i];
}

void WorldbuilderEditor::updateOverlayFrames() {
    if (overlays == nullptr) return;
    std::vector<juce::Rectangle<int>> panels { controlsPanel, layersPanel, placesPanel, footer };
    if (detail.slot >= 0) {
        panels.push_back(detail.area);
    } else {
        panels.push_back(globePanel);
        panels.push_back(cardsPanel);
    }
    // Never on a control, nor on text near a frame (the header, the footer's labels).
    std::vector<juce::Rectangle<int>> keep { headerTitle, headerSubtitle };
    for (auto* c : getChildren())
        if (c != overlays.get() && c->isVisible()) keep.push_back(c->getBounds().expanded(2));
    for (auto* c : std::initializer_list<juce::Component*> { &root, &mood, &quality, &playMode, &seed, &overlayChoice })
        keep.push_back(c->getBounds().translated(0, -16).withHeight(16));
    keep.push_back(dice.getBounds().translated(dice.getWidth() + 10, 0).withWidth(100));   // tempo
    for (const auto& area : groupAreas) keep.push_back(area.withHeight(18));
    keep.push_back(layersPanel.reduced(10, 6).withHeight(20));
    keep.push_back(placesPanel.reduced(10, 6).withHeight(20));
    if (detail.slot < 0) keep.push_back(juce::Rectangle<float>(90.0f, 16.0f).withCentre({ globeArea.getCentreX(), globeArea.getBottom() + 12.0f }).toNearestInt());   // LISTENER
    if (keep == overlayKeepClear) return;
    overlayKeepClear = keep;
    juce::RectangleList<int> clear;
    for (auto r : keep) clear.addWithoutMerging(r);
    std::vector<juce::Rectangle<int>> open;
    if (!headerOpen.isEmpty()) open.push_back(headerOpen);
    overlays->setFrames(panels, open, clear);
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
    if (!cardGrid.contains(p)) return -1;
    for (int i = 0; i < listedCount; ++i)
        if (cardBounds(i).contains(p)) return listed[std::size_t(i)];
    return -1;
}

void WorldbuilderEditor::mouseDown(const juce::MouseEvent& e) {
    if (detail.slot >= 0 || patterns->isVisible()) return;
    if (const int chip = categoryAt(e.getPosition()); chip >= 0) {
        category = chip - 1;
        cardScroll = 0;
        listCards();
        repaint(cardsPanel);
        return;
    }
    const int card = cardAt(e.getPosition());
    if (card >= 0) {
        addPlace(card);
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
    const bool clickable = (card >= 0 && placeImplemented[std::size_t(card)]) || overLandmark || (detail.slot < 0 && categoryAt(e.getPosition()) >= 0);
    setMouseCursor(clickable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (card >= 0) setHelpText(placeInfo(card).description);
}

void WorldbuilderEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (detail.slot >= 0 || patterns->isVisible() || !cardsPanel.contains(e.getPosition())) return;
    const float before = cardScroll;
    cardScroll = juce::jlimit(0.0f, maxCardScroll(), cardScroll - wheel.deltaY * 160.0f);
    if (cardScroll != before) repaint(cardsPanel);
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
        if (info.mood >= 0)
            detail.mood.setButtonText("USE " + juce::StringArray::fromTokens(moodChoices, "|", "")[info.mood].toUpperCase());
        for (int e = 0; e < placeElements; ++e) {
            detail.elementOn[std::size_t(e)].setButtonText(info.element[std::size_t(e)].name);
            detail.element[std::size_t(e)].setTooltip(juce::String(info.element[std::size_t(e)].name) + " level");
        }
        repaint(detail.area);
    }
    const int character = juce::roundToInt(param(placeParam(detail.slot, PlCharacter)));
    for (int k = 0; k < 3; ++k) detail.character[std::size_t(k)].setToggleState(k == character, juce::dontSendNotification);
}

void WorldbuilderEditor::timerCallback() {
    if (!isShowing()) return;
    stepAnimation(1.0f / 30.0f);
}

void WorldbuilderEditor::settle(float seconds) {
    for (float t = 0; t < seconds; t += 1.0f / 30.0f) stepAnimation(1.0f / 30.0f);
}

void WorldbuilderEditor::stepAnimation(float seconds) {
    animTime += seconds;
    if (animTime > 3600.0f) animTime = 0;
    refreshPlaceStrips();
    updateTheme(seconds);
    // The whole interface repaints while the Places' colours are moving.
    if (themeMoving) repaint();
    if (const int stored = int(processor.parameters.state.getProperty("overlays", ui::defaultOverlayMode)); stored != overlays->mode()) setOverlayMode(stored);
    updateOverlayFrames();
    overlays->advance(seconds);
    bool animating = false;
    for (int type = 0; type < PlaceTypeCount; ++type) {
        auto& h = hoverAmount[std::size_t(type)];
        const float target = hoverCard == type && detail.slot < 0 ? 1.0f : 0.0f;
        h += 0.2f * (target - h);
        if (std::abs(target - h) < 0.002f) h = target;
        animating |= h > 0.0f;
    }
    if (animating && detail.slot < 0 && !patterns->isVisible()) repaint(cardsPanel);
    if (detail.slot >= 0) repaint(detail.area.withHeight(detail.titleArea.getBottom() - detail.area.getY() + 8));
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

    if (detail.slot >= 0) updateDetailLabels();
    else if (!patterns->isVisible()) repaint(globePanel);
    repaint(layersPanel);
    repaint(placesPanel);
    repaint(footer.withTrimmedLeft(footer.getWidth() - 50));
}
