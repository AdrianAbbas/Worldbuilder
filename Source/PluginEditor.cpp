#include "PluginEditor.h"
#include "UI/PlaceArt.h"
#include "UI/Style.h"
#include "UI/Pixel.h"

using namespace wb;
namespace colours = wb::ui::colours;
namespace pixel = wb::ui::pixel;

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
        case PIntensity: return "Score Intensity: every active Place at once, from a calm bed (0%) through the Places as set (50%) to a climax (100%): busier, tenser, brighter and louder. Automate it to shape a cue.";
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
namespace {
// Shift-drag: fine control on every knob and slider (a slow, velocity-sensitive drag).
void enableFineDrag(juce::Component& root) {
    for (auto* child : root.getChildren()) {
        if (auto* slider = dynamic_cast<juce::Slider*>(child)) slider->setVelocityModeParameters(0.3, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        enableFineDrag(*child);
    }
}
} // namespace

WorldbuilderEditor::WorldbuilderEditor(WorldbuilderProcessor& p) : AudioProcessorEditor(p), processor(p) {
    setLookAndFeel(&lookAndFeel);

    for (const auto& group : groups)
        for (int param : group.params)
            if (param >= 0) addKnob(param);
    for (int param : { PSpaceMix, PWidth, PMaster }) addKnob(param);
    // Score Intensity: the big macro in the header, labelled by the editor.
    {
        auto& knob = addKnob(PIntensity);
        knob.getProperties().set("compact", true);
        knob.onValueChange = [this] { repaint(intensityLabel); };
    }

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

    // The Environment master: one level over all six layers, beside the panel's title.
    environmentLevel.setSliderStyle(juce::Slider::LinearHorizontal);
    environmentLevel.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    environmentLevel.setTooltip("Environment level: all six environment layers together (automatable)");
    addAndMakeVisible(environmentLevel);
    environmentAttachment = std::make_unique<SliderAttachment>(processor.parameters, specs[std::size_t(PEnvironment)].id, environmentLevel);
    environmentLevel.setDoubleClickReturnValue(true, specs[std::size_t(PEnvironment)].def);
    environmentLevel.onValueChange = [this] { repaint(environmentValue); };

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
        ui::setIcon(s.remove, ui::Icon::Close);
        s.remove.setButtonText({});
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
    ui::setIcon(detail.dice, ui::Icon::Dice);
    ui::setIcon(detail.close, ui::Icon::Close);
    detail.close.setButtonText({});
    detail.close.setTooltip("Close this Place's panel");
    ui::setIcon(detail.patterns, ui::Icon::Patterns);
    ui::setIcon(detail.mood, ui::Icon::Note);
    ui::setIcon(detail.sync, ui::Icon::Sync);
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
    ui::setIcon(power, ui::Icon::Power);
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
    setupCombo(placeOuts, PPlaceOuts, placeOutsAttachment);
    placeOuts.setTooltip("Place outputs (Logic's multi-output instrument): each Place slot also plays on its own stereo output. "
                         "Copy: the main output keeps the full mix. Split: a Place with its own output leaves the main mix "
                         "(its reverb and delay returns stay on the main). GarageBand has only the main output, which always "
                         "keeps every Place.");
    playMode.setTooltip("Transport: plays only while the host plays, from silence and locked to the song position, so bounces and exports "
                        "always contain it. Arrangement: as Transport, playing the ARRANGE section at the song position. "
                        "Free: plays all the time (GarageBand exports only up to the last region).");
    root.setTooltip("Key root: everything tonal (bed, drips, rain, wind, thunder, every Place) follows it");
    mood.setTooltip("Scale and mood: everything tonal follows it");
    quality.setTooltip("Eco / Standard / High: voice counts and detail");

    seed.setSliderStyle(juce::Slider::LinearBar);
    seed.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    seed.setTooltip("Seed: the same seed and settings always build the same world. Drag to change.");
    addAndMakeVisible(seed);
    seedAttachment = std::make_unique<SliderAttachment>(processor.parameters, "seed", seed);
    dice.setTooltip("Roll a new seed for the whole world");
    ui::setIcon(dice, ui::Icon::Dice);
    dice.onClick = [this] { setParam(PSeed, float(juce::Random::getSystemRandom().nextInt({ 1, 10000 }))); };
    addAndMakeVisible(dice);
    capture = std::make_unique<ui::CapturePanel>(processor);
    addAndMakeVisible(*capture);
    locationRow = std::make_unique<ui::LocationBar>(processor);
    addAndMakeVisible(*locationRow);

    patterns = std::make_unique<ui::PatternEditor>(processor);
    patterns->onClose = [this] {
        const int slot = patterns->slot();
        patterns->setVisible(false);
        openDetail(slot);
    };
    addChildComponent(*patterns);

    arrange = std::make_unique<ui::ArrangeView>(processor);
    arrange->onClose = [this] { closeArrange(); };
    addChildComponent(*arrange);
    arrangeToggle.setTooltip("ARRANGE: design the track as a timeline of sections (Intro, Build, Climax...): their lengths, Score Intensity "
                             "ramps and which instruments and pattern variants play in each. Play Mode Arrangement follows it.");
    arrangeToggle.setColour(juce::TextButton::buttonOnColourId, ui::colours::brass);
    ui::setIcon(arrangeToggle, ui::Icon::Path);
    arrangeToggle.onClick = [this] { arrange->isVisible() ? closeArrange() : openArrange(); };
    addAndMakeVisible(arrangeToggle);

    // Scenes: the factory scenes (previous, the menu, next), INSPIRE and its UNDO.
    scenePrev.setTooltip("The previous factory scene");
    sceneNext.setTooltip("The next factory scene");
    sceneMenu.setTooltip("Factory scenes: ready-made film and game moments, each with its Places, Planet, effects, Locations and an "
                         "arrangement. Play Mode Arrangement plays its sections and travels between its Locations.");
    inspireToggle.setTooltip("INSPIRE: a random but musical world from the next World Seed (Places that belong together, Characters, "
                             "Planet, key, effects, Locations and an arranged template). UNDO brings the world before it back.");
    sceneUndo.setTooltip("Undo the last scene or INSPIRE: the world before it comes back");
    inspireToggle.setColour(juce::TextButton::buttonOnColourId, ui::colours::brass);
    ui::setIcon(inspireToggle, ui::Icon::Generate);
    ui::setIcon(sceneUndo, ui::Icon::Undo);
    scenePrev.onClick = [this] {
        const int at = processor.factoryIndex();
        loadScene(at < 0 ? scenes::factoryCount() - 1 : (at + scenes::factoryCount() - 1) % scenes::factoryCount());
    };
    sceneNext.onClick = [this] {
        const int at = processor.factoryIndex();
        loadScene(at < 0 ? 0 : (at + 1) % scenes::factoryCount());
    };
    sceneMenu.onClick = [this] { showSceneMenu(); };
    inspireToggle.onClick = [this] { inspire(); };
    sceneUndo.onClick = [this] { undoScene(); };
    for (auto* b : { &scenePrev, &sceneNext, &sceneMenu, &inspireToggle, &sceneUndo }) addAndMakeVisible(*b);
    sceneUndo.setEnabled(processor.canUndoScene());
    sceneMenu.setButtonText(processor.sceneName().isEmpty() ? juce::String("Scenes...") : processor.sceneName());

    setupCombo(planetChoice, PPlanet, planetAttachment);
    planetChoice.setTooltip("Planet: the globe's look and a sound colour on the whole world (Rocky: dry and close; Ocean: watery movement; "
                            "Ice Giant: cold shimmer; Gas Giant: vast and swirling; Desert: dusty and wide; Lava: hot and saturated; Forest: soft and damped)");
    planetInfluence.setSliderStyle(juce::Slider::LinearHorizontal);
    planetInfluence.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    planetInfluence.setTooltip("Planet Influence: how strongly the Planet colours the sound (subtle at 35 %)");
    addAndMakeVisible(planetInfluence);
    planetInfluenceAttachment = std::make_unique<SliderAttachment>(processor.parameters, specs[PPlanetInfluence].id, planetInfluence);
    planetInfluence.setDoubleClickReturnValue(true, specs[PPlanetInfluence].def);
    planetInfluence.onValueChange = [this] { repaint(influenceLabel); };
    shownPlanet = previousPlanet = juce::roundToInt(param(PPlanet));

    for (int m = 0; m < ui::OverlayModeCount; ++m) overlayChoice.addItem(ui::overlayModeNames[std::size_t(m)], m + 1);
    overlayChoice.setTooltip("Place overlays: each Place decorates the edges of the interface (grass and ivy, neon, wooden beams...). "
                             "Full, Subtle or Off. They never cover the controls.");
    overlayChoice.onChange = [this] { setOverlayMode(overlayChoice.getSelectedId() - 1); };
    addAndMakeVisible(overlayChoice);
    uiSize.addItemList({ "100%", "150%", "200%" }, 1);
    uiSize.setTooltip("UI size: 100, 150 or 200 %. The pixel art stays sharp at each (whole pixels).");
    uiSize.onChange = [this] { setUiSize(uiSize.getSelectedId() - 1); };
    addAndMakeVisible(uiSize);
    uiSize.setSelectedId(juce::jlimit(0, 2, int(processor.parameters.state.getProperty("uiscale", 0))) + 1, juce::dontSendNotification);
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
    enableFineDrag(*this);
    if (uiSizeIndex() > 0) setUiSize(uiSizeIndex());
    startTimerHz(30);
}

void WorldbuilderEditor::setUiSize(int index) {
    index = juce::jlimit(0, 2, index);
    uiSize.setSelectedId(index + 1, juce::dontSendNotification);
    processor.parameters.state.setProperty("uiscale", index, nullptr);
    // The editor keeps its 1160 x 812 layout; the host window grows with the scale.
    setScaleFactor(index == 0 ? 1.0f : index == 1 ? 1.5f : 2.0f);
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

void WorldbuilderEditor::loadScene(int index) {
    if (!processor.loadFactoryScene(index)) return;
    sceneChanged();
}

void WorldbuilderEditor::inspire() {
    processor.inspire();
    sceneChanged();
}

void WorldbuilderEditor::undoScene() {
    if (processor.undoScene()) sceneChanged();
}

void WorldbuilderEditor::sceneChanged() {
    closeDetail();
    if (patterns->isVisible()) patterns->setVisible(false);
    refreshPlaceStrips();
    if (arrange->isVisible()) arrange->refresh();
    const auto name = processor.sceneName();
    sceneMenu.setButtonText(name.isEmpty() ? juce::String("Scenes...") : name);
    sceneUndo.setEnabled(processor.canUndoScene());
    repaint();
}

void WorldbuilderEditor::showSceneMenu() {
    // The factory scenes by their first Place's world.
    juce::PopupMenu menu;
    const int at = processor.factoryIndex();
    for (int category : { int(MedievalFantasy), int(SciFi), int(Modern), int(Nature) }) {
        juce::PopupMenu sub;
        for (int i = 0; i < scenes::factoryCount(); ++i) {
            const auto& scene = scenes::factory(i);
            if (placeInfo(scene.slots[0].type).category != category) continue;
            sub.addItem(i + 1, scene.name, true, i == at);
        }
        if (sub.getNumItems() > 0) menu.addSubMenu(juce::String(categoryNames[std::size_t(category)]).toLowerCase().replace("&", "and").toUpperCase(), sub);
    }
    menu.addSeparator();
    menu.addItem(1000, "INSPIRE (a new world from the next World Seed)");
    menu.addItem(1001, "Undo the last scene", processor.canUndoScene());
    juce::Component::SafePointer<WorldbuilderEditor> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&sceneMenu), [safe](int result) {
        if (safe == nullptr || result <= 0) return;
        if (result == 1000) safe->inspire();
        else if (result == 1001) safe->undoScene();
        else safe->loadScene(result - 1);
    });
}

void WorldbuilderEditor::setOverlayMode(int mode) {
    mode = juce::jlimit(0, ui::OverlayModeCount - 1, mode);
    overlays->setMode(mode);
    overlayChoice.setSelectedId(mode + 1, juce::dontSendNotification);
    processor.parameters.state.setProperty("overlays", mode, nullptr);
}

bool WorldbuilderEditor::covered() const { return patterns->isVisible() || arrange->isVisible(); }

void WorldbuilderEditor::openArrange() {
    enableFineDrag(*arrange);
    closeDetail();
    if (patterns->isVisible()) patterns->setVisible(false);
    arrange->refresh();
    arrange->setVisible(true);
    arrange->toFront(true);
    overlays->toFront(false);
    arrangeToggle.setToggleState(true, juce::dontSendNotification);
    updateOverlayFrames();
    repaint();
}

void WorldbuilderEditor::closeArrange() {
    if (!arrange->isVisible()) return;
    arrange->setVisible(false);
    processor.stopPreview();
    arrangeToggle.setToggleState(false, juce::dontSendNotification);
    updateOverlayFrames();
    repaint();
}

void WorldbuilderEditor::openPatterns(int slot, int element) {
    if (slot < 0 || slot >= placeSlotCount || !hasPatterns(placeType(slot))) return;
    closeDetail();
    closeArrange();
    patterns->setVisible(true);
    enableFineDrag(*patterns);
    patterns->toFront(false);
    overlays->toFront(false);
    patterns->open(slot, element);
    repaint();
}

void WorldbuilderEditor::openDetail(int slot) {
    if (slot < 0 || slot >= placeSlotCount || placeType(slot) == NoPlace) return;
    closeDetail();
    if (patterns->isVisible()) patterns->setVisible(false);
    closeArrange();
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
        // The pixel title: Large letters at twice the art grid.
        headerTitle = header.withTrimmedLeft(4).withWidth((pixel::Canvas::textWidth("WORLDBUILDER") + 2) * pixel::unit * 2 + 4);
        // Score Intensity: its label and a large knob, clear of POWER.
        const auto block = header.withRight(power.getX() - 48).withLeft(power.getX() - 48 - 170);
        knobFor[std::size_t(PIntensity)]->setBounds(block.withLeft(block.getRight() - 46).withSizeKeepingCentre(46, 46));
        intensityLabel = block.withRight(block.getRight() - 50).withSizeKeepingCentre(124, 30);
        arrangeToggle.setBounds(juce::Rectangle<int>(intensityLabel.getX() - 16 - 100, header.getCentreY() - 13, 100, 26));
        // The scene browser, INSPIRE and UNDO, right of the title.
        auto row = juce::Rectangle<int>(headerTitle.getRight() + 14, header.getCentreY() - 13, arrangeToggle.getX() - 18 - headerTitle.getRight() - 14, 26);
        sceneUndo.setBounds(row.removeFromRight(62));
        row.removeFromRight(4);
        inspireToggle.setBounds(row.removeFromRight(90));
        row.removeFromRight(10);
        scenePrev.setBounds(row.removeFromLeft(24));
        sceneNext.setBounds(row.removeFromRight(24));
        sceneMenu.setBounds(row.reduced(3, 0));
        headerSubtitle = {};
        headerOpen = {};
    }
    area.removeFromTop(6);
    footer = area.removeFromBottom(80);
    area.removeFromBottom(8);
    locationsPanel = area.removeFromBottom(84);
    area.removeFromBottom(8);
    locationRow->setBounds(locationsPanel.reduced(12, 6));
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
    planetScene = pixel::snap(globePanel.reduced(4 * pixel::unit));
    globeArea = planetScene.toFloat();
    {
        // The Planet in the globe panel's top right corner, clear of the Location orbit.
        auto corner = globePanel.reduced(10, 8).removeFromRight(104);
        planetLabel = corner.removeFromTop(14);
        planetChoice.setBounds(corner.removeFromTop(22));
        corner.removeFromTop(4);
        influenceLabel = corner.removeFromTop(13);
        planetInfluence.setBounds(corner.removeFromTop(14));
    }

    // World Controls: one row per group, title on top.
    auto rows = controlsPanel.reduced(8, 8);
    const int rowHeight = rows.getHeight() / int(groups.size());
    for (std::size_t g = 0; g < groups.size(); ++g) groupAreas[g] = rows.removeFromTop(rowHeight);
    for (std::size_t g = 0; g < groups.size(); ++g) {
        auto row = groupAreas[g].withTrimmedTop(18);
        const int cell = row.getWidth() / 3;
        for (int param : groups[g].params) {
            auto c = row.removeFromLeft(cell);
            if (param >= 0) knobFor[std::size_t(param)]->setBounds(c.reduced(2, 0));
        }
    }

    // Environment layers.
    auto mixer = layersPanel.reduced(10, 8);
    {
        // The title row: ENVIRONMENT, then its master level and value.
        auto title = mixer.removeFromTop(20);
        title.removeFromLeft(118);
        environmentValue = title.removeFromRight(46);
        environmentLevel.setBounds(title.reduced(0, 3));
    }
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
        auto top = r.removeFromTop(28);   // the name row: room for the Place's pixel icon
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
            const int w = k == 0 ? 40 : 66;
            categoryChips[std::size_t(k)] = top.removeFromRight(w).reduced(2, 2);
        }
        cardGrid = cardsPanel.reduced(8, 6).withTrimmedTop(22);
        cardScroll = juce::jlimit(0.0f, maxCardScroll(), cardScroll);
    }

    layoutDetail();
    patterns->setBounds(controlsPanel.getUnion(globePanel).getUnion(cardsPanel));
    arrange->setBounds(patterns->getBounds());

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
    seed.setBounds(bottom.removeFromLeft(170).withSizeKeepingCentre(170, 20));
    bottom.removeFromLeft(10);
    dice.setBounds(bottom.removeFromLeft(104).withSizeKeepingCentre(104, 22));
    bottom.removeFromLeft(10);
    uiSize.setBounds(bottom.removeFromLeft(76).withSizeKeepingCentre(76, 22));
    {
        f.removeFromLeft(14);
        auto column = f.removeFromLeft(100);
        overlayChoice.setBounds(column.removeFromTop(column.getHeight() / 2).withTrimmedTop(14).withSizeKeepingCentre(100, 22));
        placeOuts.setBounds(column.withTrimmedTop(14).withSizeKeepingCentre(100, 20));
    }
    f.removeFromRight(40);   // master meter
    for (int param : { PMaster, PWidth, PSpaceMix })
        knobFor[std::size_t(param)]->setBounds(f.removeFromRight(92).withSizeKeepingCentre(92, 66));
    // Capture (render and scene MIDI) in the space between.
    f.removeFromLeft(14);
    capture->setBounds(f.withTrimmedRight(8).withWidth(std::min(f.getWidth() - 8, 214)));
    overlays->setBounds(getLocalBounds());
    pixel::snapChildren(*this);   // every control on whole art pixels
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
    d.close.setBounds(d.titleArea.withHeight(30).withTrimmedLeft(d.titleArea.getWidth() - 34).withSizeKeepingCentre(30, 26));
    d.patterns.setBounds(d.close.getBounds().translated(-112, 0).withWidth(106));
    d.mood.setBounds(d.patterns.getBounds().translated(-140, 0).withWidth(134));
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
    pixel::snapChildren(*this);
}

//==============================================================================
void WorldbuilderEditor::paint(juce::Graphics& g) {
    paintStarted = juce::Time::getMillisecondCounterHiRes();
    // The world's colours: the active Places set the background and the glow (blended by
    // level, see updateTheme); the panels and controls are pixel art in the mood of the
    // world (pixel::theme(): fantasy, blend, sci-fi, neutral).
    pixel::setTheme(pixelMood, int(animTime * 8.0f) % 600, pixelDelights);   // this editor's look (another may share the theme)
    const auto& p = pixel::theme().colours;
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

    // Text only where the clip reaches: frames often repaint small areas.
    if (g.clipRegionIntersects(headerTitle)) pixel::drawTitle(g, "WORLDBUILDER", headerTitle, p.heading, p.heading.brighter(0.4f), p.outline);
    if (g.clipRegionIntersects(intensityLabel)) {
        pixel::drawText(g, "SCORE INTENSITY", intensityLabel.withHeight(15), p.heading, juce::Justification::centredRight, pixel::Font::Small);
        pixel::drawText(g, juce::String(juce::roundToInt(param(PIntensity) * 100.0f)) + "%", intensityLabel.withTrimmedTop(15), p.accent,
                        juce::Justification::centredRight, pixel::Font::Large);
    }
    {
        // A dotted rule under the header.
        pixel::drawCached(g, "rule/" + juce::String(header.getWidth()), { header.getX(), header.getBottom() + 2 }, [&] {
            pixel::Canvas k(header.getWidth() / pixel::unit, 1);
            for (int x = 0; x < k.width(); x += 3) k.set(x, 0, p.rim.withAlpha(0.6f));
            return k;
        });
    }

    // The panels: framed in the mood's material, tinted a little by the world.
    const bool detailOpen = detail.slot >= 0;
    const auto tint = juce::Colour(themeTint.getRed() & 0xf0, themeTint.getGreen() & 0xf0, themeTint.getBlue() & 0xf0);
    const float tintAmount = std::round(0.3f * themeStrength * 8.0f) / 8.0f;
    for (auto panel : { controlsPanel, globePanel, cardsPanel, layersPanel, placesPanel, locationsPanel, footer }) {
        if ((detailOpen && (panel == globePanel || panel == cardsPanel)) || !g.clipRegionIntersects(panel.expanded(3))) continue;
        const int w = panel.getWidth() / pixel::unit, h = panel.getHeight() / pixel::unit;
        pixel::drawCached(g, "panel/" + juce::String(w) + "x" + juce::String(h) + "/" + tint.toString() + juce::String(tintAmount), panel.getPosition(),
                          [&] { return pixel::panel(p, w, h, 0, tint, tintAmount); });
    }

    for (std::size_t i = 0; i < groups.size(); ++i)
        if (const auto title = groupAreas[i].withHeight(16).withTrimmedLeft(6); g.clipRegionIntersects(title))
            pixel::drawText(g, groups[i].title, title, p.heading, juce::Justification::centredLeft, pixel::Font::Large);
    if (const auto title = layersPanel.reduced(14, 8).withHeight(16); g.clipRegionIntersects(title))
        pixel::drawText(g, "ENVIRONMENT", title, p.heading, juce::Justification::centredLeft, pixel::Font::Large);
    if (g.clipRegionIntersects(environmentValue)) {
        const double db = environmentLevel.getValue();
        pixel::drawText(g, db <= -59.9 ? juce::String("OFF") : juce::String(db, 1) + " DB", environmentValue, p.ink, juce::Justification::centredRight, pixel::Font::Small);
    }
    if (const auto title = placesPanel.reduced(14, 8).withHeight(16); g.clipRegionIntersects(title))
        pixel::drawText(g, "PLACES", title, p.heading, juce::Justification::centredLeft, pixel::Font::Large);

    for (int l = 0; l < LayerCount && g.clipRegionIntersects(layersPanel.reduced(ui::PlaceOverlays::frameBand)); ++l) {
        const auto& s = strips[std::size_t(l)];
        pixel::drawText(g, layerNames[std::size_t(l)], s.row.withWidth(86).withTrimmedLeft(4), p.ink, juce::Justification::centredLeft, pixel::Font::Small);
        // The layer's activity: an LED meter under its slider.
        const float level = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(meters[std::size_t(l)], -60.0f) + 60.0f) / 60.0f);
        const int length = s.level.getWidth() / pixel::unit - 2;
        const float shown = std::round(level * float(length)) / float(length);
        pixel::drawCached(g, "meter/" + juce::String(length) + "/" + juce::String(shown), { s.level.getX() + pixel::unit, s.level.getBottom() - 1 },
                          [&] { return pixel::meter(p, length, 3, shown, false); });
    }
    if (g.clipRegionIntersects(placesPanel.reduced(ui::PlaceOverlays::frameBand))) paintPlaceStrips(g);

    for (auto [component, text] : { std::pair<juce::Component*, const char*> { &root, "ROOT" }, { &mood, "MOOD" },
                                    { &quality, "QUALITY" }, { &playMode, "PLAY MODE" }, { &seed, "WORLD SEED" },
                                    { &overlayChoice, "OVERLAYS" }, { &placeOuts, "PLACE OUTPUTS" }, { &uiSize, "UI SIZE" } })
        if (const auto label = component->getBounds().translated(0, -15).withHeight(14); g.clipRegionIntersects(label))
            pixel::drawText(g, text, label, p.heading, juce::Justification::centredLeft, pixel::Font::Small);
    if (const auto tempo = uiSize.getBounds().translated(uiSize.getWidth() + 10, 0).withWidth(80); g.clipRegionIntersects(tempo))
        pixel::drawText(g, juce::String(processor.engine.frame().bpm, 1) + " BPM", tempo, p.inkDim, juce::Justification::centredLeft, pixel::Font::Small);
    {
        // The master meter: a column of LEDs.
        const auto meter = juce::Rectangle<int>(footer.getRight() - 36, footer.getY() + 12, 12, footer.getHeight() - 24);
        const float masterLevel = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(masterMeter, -60.0f) + 60.0f) / 60.0f);
        const int length = meter.getHeight() / pixel::unit;
        const float shown = std::round(masterLevel * float(length)) / float(length);
        pixel::drawCached(g, "master/" + juce::String(length) + "/" + juce::String(shown), meter.getPosition(), [&] { return pixel::meter(p, length, 6, shown, true); });
    }

    // The expensive parts only where the clip reaches inside their panel (overlay frames
    // repaint small areas, often only a panel's edge).
    if (detailOpen) {
        if (g.clipRegionIntersects(detail.area)) paintDetail(g);
    } else {
        if (g.clipRegionIntersects(globePanel.reduced(ui::PlaceOverlays::frameBand))) {
            paintGlobe(g, globeArea);
            pixel::drawText(g, "LIVING GLOBE", globePanel.reduced(14, 10).withHeight(16), p.ink, juce::Justification::centredLeft, pixel::Font::Large);
            pixel::drawText(g, "PLANET", planetLabel, p.heading, juce::Justification::centredLeft, pixel::Font::Small);
            pixel::drawText(g, "INFLUENCE", influenceLabel, p.heading, juce::Justification::centredLeft, pixel::Font::Small);
            pixel::drawText(g, juce::String(juce::roundToInt(param(PPlanetInfluence) * 100.0f)) + "%", influenceLabel, p.accent, juce::Justification::centredRight,
                            pixel::Font::Small);
        }
        if (g.clipRegionIntersects(cardsPanel.reduced(ui::PlaceOverlays::frameBand))) paintCards(g);
    }
}

void WorldbuilderEditor::paintPlaceStrips(juce::Graphics& g) {
    const auto& p = pixel::theme().colours;
    for (int slot = 0; slot < placeSlotCount; ++slot) {
        const auto& s = placeStrips[std::size_t(slot)];
        const int type = placeType(slot);
        const int w = s.row.getWidth() / pixel::unit, h = s.row.getHeight() / pixel::unit;
        if (type == NoPlace) {
            pixel::drawCached(g, "emptyslot/" + juce::String(w) + "x" + juce::String(h), s.row.getPosition(), [&] {
                auto k = pixel::field(p, w, h);
                for (int x = 3; x < w - 3; x += 4) k.set(x, h / 2 + 6, p.wellLight);
                return k;
            });
            pixel::drawText(g, "SLOT " + juce::String(slot + 1) + ": EMPTY - CLICK A PLACE CARD", s.row, p.inkDim, juce::Justification::centred, pixel::Font::Small);
            continue;
        }
        const auto accent = placeColour(type);
        const bool selected = detail.slot == slot;
        pixel::drawCached(g, "strip/" + juce::String(w) + "x" + juce::String(h) + "/" + accent.toString() + juce::String(int(selected)), s.row.getPosition(),
                          [&] { return pixel::strip(p, w, h, accent, selected); });
        auto inner = s.row.reduced(6, 4);
        auto top = inner.removeFromTop(28).withTrimmedRight(128);   // the buttons sit on the right
        // The Place's pixel icon (half size, its top 28 rows), its name and Character.
        pixel::drawPlaceIcon(g, type, { top.getX(), top.getY() }, true, pixel::iconFrame(type, false), 28);
        top.removeFromLeft(36);
        pixel::drawText(g, placeNames[std::size_t(type)], top.removeFromTop(16), p.ink, juce::Justification::centredLeft, pixel::Font::Large);
        const int character = juce::roundToInt(param(placeParam(slot, PlCharacter)));
        pixel::drawText(g, placeInfo(type).characters[std::size_t(std::clamp(character, 0, 2))], top, accent.interpolatedWith(p.ink, 0.25f),
                        juce::Justification::centredLeft, pixel::Font::Small);
        auto labels = inner.removeFromTop(14);
        const int third = labels.getWidth() / 3;
        for (const char* text : { "LEVEL", "DISTANCE", "PAN" })
            pixel::drawText(g, text, labels.removeFromLeft(third).reduced(4, 0), p.inkDim, juce::Justification::bottomLeft, pixel::Font::Small);
        // The Place's activity: an LED meter in its colour under the sliders.
        const float level = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(placeMeters[std::size_t(slot)], -60.0f) + 60.0f) / 60.0f);
        const int length = s.level.getWidth() / pixel::unit - 2;
        const float shown = std::round(level * float(length)) / float(length);
        pixel::drawCached(g, "pmeter/" + juce::String(length) + "/" + juce::String(shown), { s.level.getX() + pixel::unit, s.level.getBottom() },
                          [&] { return pixel::meter(p, length, 3, shown, false); });
    }
}

void WorldbuilderEditor::paintCards(juce::Graphics& g) {
    const auto& p = pixel::theme().colours;
    const auto titleRow = cardsPanel.reduced(14, 6).withHeight(20);
    if (juce::Time::getMillisecondCounterHiRes() < fullMessageUntil)
        pixel::drawText(g, "ALL FOUR SLOTS ARE IN USE", titleRow, p.peak, juce::Justification::centredLeft, pixel::Font::Large);
    else
        pixel::drawText(g, "ADD A PLACE", titleRow, p.heading, juce::Justification::centredLeft, pixel::Font::Large);
    // Category chips.
    for (int k = 0; k <= PlaceCategoryCount; ++k) {
        const auto chip = categoryChips[std::size_t(k)];
        pixel::State st;
        st.hover = hoverChip == k;
        st.on = category == k - 1;
        const int w = chip.getWidth() / pixel::unit, h = chip.getHeight() / pixel::unit;
        pixel::drawCached(g, "chip/" + juce::String(k) + "/" + juce::String(w) + "x" + juce::String(h) + juce::String(int(st.hover)) + juce::String(int(st.on)), chip.getPosition(),
                          [&] { return pixel::chip(p, chipNames[std::size_t(k)], w, h, st); });
    }

    juce::Graphics::ScopedSaveState clip(g);
    g.reduceClipRegion(cardGrid);
    for (int i = 0; i < listedCount; ++i) {
        const int type = listed[std::size_t(i)];
        const auto bounds = cardBounds(i);
        if (!bounds.intersects(cardGrid)) continue;
        const bool built = placeImplemented[std::size_t(type)];
        int count = 0;
        for (int slot = 0; slot < placeSlotCount; ++slot) count += placeType(slot) == type ? 1 : 0;
        pixel::State st;
        st.hover = hoverCard == type;
        st.enabled = built;
        const int w = bounds.getWidth() / pixel::unit, h = bounds.getHeight() / pixel::unit - 1;
        const int frame = pixel::iconFrame(type, st.hover);
        pixel::drawCached(g, "card/" + juce::String(type) + "/" + juce::String(w) + "x" + juce::String(h) + "/" + juce::String(int(st.hover)) + juce::String(frame)
                                 + juce::String(st.hover ? pixel::theme().frame % 6 : 0),
                          { bounds.getX() - pixel::margin * pixel::unit, bounds.getY() - pixel::margin * pixel::unit },
                          [&] { return pixel::placeCard(p, type, pixel::placeShortName(type), st, st.hover ? pixel::theme().frame : frame, w, h); });
        if (built && count > 0)
            pixel::drawCached(g, "badge/" + juce::String(count) + placeColour(type).toString(), { bounds.getRight() - 20, bounds.getY() + 6 },
                              [&] { return pixel::badge(p, juce::String(count), placeColour(type)); });
    }
    // The scroll position, when the list is longer than the panel.
    if (const float range = maxCardScroll(); range > 0.0f) {
        const auto track = juce::Rectangle<int>(cardGrid.getRight() - 6, cardGrid.getY() + 2, 6, cardGrid.getHeight() - 4);
        const float visible = float(cardGrid.getHeight()) / (float(cardGrid.getHeight()) + range);
        const int length = track.getHeight() / pixel::unit, thumb = std::max(4, juce::roundToInt(float(length) * visible));
        const int at = juce::roundToInt(float(length - thumb) * cardScroll / range);
        pixel::drawCached(g, "cardscroll/" + juce::String(length) + "/" + juce::String(at) + "/" + juce::String(thumb), track.getPosition(), [&] {
            pixel::Canvas k(3, length);
            k.box(0, 0, 3, length, p.well, p.outline);
            k.box(0, at, 3, thumb, p.rim, p.outline);
            return k;
        });
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
    const auto& p = pixel::theme().colours;
    const int type = placeType(d.slot);
    const auto accent = placeColour(type);
    const auto& theme = ui::placeTheme(type);
    // The panel in the mood's material, tinted with the Place's own colour.
    const int w = d.area.getWidth() / pixel::unit, h = d.area.getHeight() / pixel::unit;
    const int bannerH = (d.titleArea.getBottom() - d.area.getY() + 4) / pixel::unit;
    pixel::drawCached(g, "detail/" + juce::String(type) + "/" + juce::String(w) + "x" + juce::String(h), d.area.getPosition(), [&] {
        auto k = pixel::panel(p, w, h, 0, theme.tint, 0.45f);
        // The banner: the Place's sky in dithered bands across the top, a ground line.
        const int x0 = 4, x1 = w - 5, y0 = 4, y1 = bannerH;
        const auto sky = theme.sky.interpolatedWith(p.well, 0.3f), horizon = theme.horizon.interpolatedWith(p.well, 0.2f);
        for (int y = y0; y < y1; ++y)
            for (int x = x0; x <= x1; ++x) {
                const float t = float(y - y0) / float(std::max(1, y1 - y0 - 1)) * 3.0f;
                int band = int(t);
                if (t - float(band) > 0.7f && ((x + y) & 1)) ++band;
                const auto c = sky.interpolatedWith(horizon, juce::jlimit(0.0f, 1.0f, float(band) / 3.0f));
                // Shaded on the left, where the name and description sit.
                const float shade = juce::jlimit(0.0f, 1.0f, 1.0f - float(x - x0) / float(std::max(1, (x1 - x0) / 2)));
                k.set(x, y, c.interpolatedWith(p.panelDark, 0.75f * shade));
            }
        k.hline(x0, x1, y1, theme.glow);
        k.hline(x0, x1, y1 + 1, p.outline);
        return k;
    });
    // The Place itself standing on the banner's ground, toward the right.
    {
        const int iconX = d.area.getRight() - 14 * pixel::unit - pixel::iconSize * pixel::unit;
        const int iconY = d.area.getY() + bannerH * pixel::unit - pixel::iconSize * pixel::unit;
        pixel::drawPlaceIcon(g, type, { iconX, iconY }, false, pixel::iconFrame(type, true));
    }
    pixel::drawCached(g, "detailedge/" + juce::String(w) + "x" + juce::String(h) + accent.toString(), d.area.getPosition(), [&] {
        pixel::Canvas k(w, h);
        k.outlineBox(0, 0, w, h, accent.withAlpha(0.8f));
        return k;
    });

    auto title = d.titleArea;
    pixel::drawPlaceIcon(g, type, { title.getX(), title.getY() + 4 }, true, pixel::iconFrame(type, false));
    title.removeFromLeft(42);
    const int buttonsLeft = (d.mood.isVisible() ? d.mood : d.patterns.isVisible() ? d.patterns : d.close).getX();
    pixel::drawTitle(g, juce::String(placeNames[std::size_t(type)]).toUpperCase(), title.withHeight(34).withRight(buttonsLeft - 8), p.ink, p.glow, p.outline);
    // The description is a sentence: a clean, readable font.
    g.setColour(p.ink.withAlpha(0.9f));
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawFittedText("Place " + juce::String(d.slot + 1) + "  -  " + placeInfo(type).description, title.withTrimmedTop(50).withTrimmedRight(4).withRight(d.area.getRight() - 90),
                     juce::Justification::centredLeft, 2);

    pixel::drawText(g, "CHARACTER", d.characterArea.withWidth(90), p.heading, juce::Justification::centredLeft, pixel::Font::Small);
    pixel::drawText(g, "RHYTHM", d.rhythmArea.withHeight(14), p.heading, juce::Justification::centredLeft, pixel::Font::Large);
    pixel::drawText(g, "SEED", d.seed.getBounds().translated(0, -15).withHeight(14), p.heading, juce::Justification::centredLeft, pixel::Font::Small);
    pixel::drawText(g, "ELEMENTS", d.elementsArea.withHeight(14), p.heading, juce::Justification::centredLeft, pixel::Font::Large);
    g.setColour(p.inkDim);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText(juce::String("Effect: ") + placeInfo(type).effect + " (deeper with Activity and Population)", d.elementsArea.withHeight(14).withTrimmedLeft(110),
               juce::Justification::centredLeft, true);
    for (int e = 0; e < placeElements; ++e) {
        const auto row = d.elementOn[std::size_t(e)].getBounds();
        const bool on = d.elementOn[std::size_t(e)].getToggleState();
        const auto ink = on ? accent.interpolatedWith(p.ink, 0.2f) : p.inkDim;
        const int icon = placeInfo(type).element[std::size_t(e)].icon;
        pixel::drawCached(g, "inst/" + juce::String(icon) + ink.toString(), { row.getX() - 22, row.getCentreY() - 10 },
                          [&] { return pixel::instrumentIcon(icon, 9, ink, p.outline); });
    }
}

// The Living Globe as a pixel-art tiny planet (see TinyPlanet): the Planet's horizon
// across the panel, its sky with the world's weather, the Places standing on it as it
// turns, the Locations as stars and moons in the sky.
float WorldbuilderEditor::planetTurn() const { return spin * 0.3f; }

float WorldbuilderEditor::landmarkLongitude(int slot) const {
    // Pan places a Place along the horizon (the slots' default pans spread them apart).
    return param(placeParam(slot, PlPan));
}

void WorldbuilderEditor::paintGlobe(juce::Graphics& g, juce::Rectangle<float>) {
    const auto& world = processor.engine.frame();
    const auto& pal = pixel::theme().colours;
    ui::TinyPlanetScene scene;
    scene.width = planetScene.getWidth() / pixel::unit;
    scene.height = planetScene.getHeight() / pixel::unit;
    scene.planet = shownPlanet;
    scene.previous = previousPlanet;
    scene.blend = planetBlend;
    scene.turn = planetTurn();
    scene.time = animTime;
    scene.frame = pixel::theme().frame;
    const auto water = WaterLayer::amounts(world);
    scene.clouds = juce::jlimit(0.0f, 1.0f, world[Weather] * 0.85f + world[Wetness] * 0.3f);
    scene.rain = water.rain;
    scene.snow = water.snow;
    scene.flash = flash;
    scene.boltSeed = boltSeed;
    scene.magic = world[Magic];
    scene.danger = world[Danger];
    // Day and night follow the turning planet (a day is two turns).
    scene.day = 0.5f + 0.5f * std::cos(planetTurn() * 0.5f);
    for (int slot = 0; slot < placeSlotCount; ++slot) {
        const int type = placeType(slot);
        if (type == NoPlace) continue;
        ui::TinyPlanetScene::Landmark lm;
        lm.type = type;
        lm.kind = ui::TinyPlanet::kindOf(type);
        lm.longitude = landmarkLongitude(slot);
        lm.near = 1.0f - param(placeParam(slot, PlDistance));
        lm.frame = pixel::iconFrame(type, false);
        lm.highlight = dragLandmark == slot || detail.slot == slot;
        scene.landmarks.push_back(lm);
    }
    // Near Places in front.
    std::sort(scene.landmarks.begin(), scene.landmarks.end(), [](const auto& a, const auto& b) { return a.near < b.near; });
    const int mode = processor.travel.uiMode.load(std::memory_order_relaxed);
    const int target = processor.travel.uiTarget.load(std::memory_order_relaxed);
    const int arrived = processor.travel.uiArrived.load(std::memory_order_relaxed);
    const float progress = processor.travel.uiProgress.load(std::memory_order_relaxed);
    const int journeyKey = mode == Travel::Travelling ? target : -2;
    if (journeyKey != journeySeen) {
        journeySeen = journeyKey;
        if (mode == Travel::Travelling) journeyStart = arrived;
    }
    for (int i = 0; i < locationCount; ++i) {
        ui::TinyPlanetScene::Marker m;
        m.used = processor.locations.used(i);
        m.colour = juce::Colour(processor.locations.colour(i));
        m.here = (mode == Travel::Idle || mode == Travel::Holding) && arrived == i;
        scene.markers.push_back(m);
    }
    if (mode == Travel::Travelling && target >= 0) {
        scene.comet = true;
        scene.cometFrom = float(journeyStart >= 0 ? journeyStart : (locationCount - 1) / 2);
        scene.cometTo = float(target);
        scene.cometProgress = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * progress);
    } else if (mode == Travel::Journeying) {
        const int from = processor.travel.uiFrom.load(std::memory_order_relaxed), to = processor.travel.uiTo.load(std::memory_order_relaxed);
        scene.comet = from >= 0 && to >= 0;
        scene.cometFrom = float(std::max(0, from));
        scene.cometTo = float(std::max(0, to));
        scene.cometProgress = progress;
    }
    const auto& canvas = tiny.render(scene, pal);
    const float alpha = 0.35f + 0.65f * processor.engine.powerLevel.load(std::memory_order_relaxed);
    g.setOpacity(alpha);
    pixel::draw(g, canvas, planetScene.getPosition());
    g.setOpacity(1.0f);
    pixel::frameArt(g, planetScene.expanded(pixel::unit), pal.outline);
    paintLocationMarkers(g, alpha);
}

// The Locations' names beside their stars and moons (the planet draws the markers).
void WorldbuilderEditor::paintLocationMarkers(juce::Graphics& g, float alpha) {
    const auto& pal = pixel::theme().colours;
    for (int i = 0; i < locationCount; ++i) {
        if (!processor.locations.used(i)) continue;
        const auto p = markerPosition(i).toInt();
        // Names alternate above and below the arc so neighbours never collide.
        const bool below = i % 2 == 1;
        const auto box = juce::Rectangle<int>(90, 14).withCentre(p.translated(0, below ? 15 : -15));
        pixel::drawText(g, processor.locations.name(i), box, pal.ink.withMultipliedAlpha(0.95f * alpha), juce::Justification::centred, pixel::Font::Small);
    }
}

juce::Point<float> WorldbuilderEditor::markerPosition(int index) const {
    const auto at = ui::TinyPlanet::markerPoint(planetScene.getWidth() / pixel::unit, planetScene.getHeight() / pixel::unit, float(index), locationCount);
    return { float(planetScene.getX()) + (at.x + 0.5f) * float(pixel::unit), float(planetScene.getY()) + (at.y + 0.5f) * float(pixel::unit) };
}

int WorldbuilderEditor::markerAt(juce::Point<float> p) const {
    for (int i = 0; i < locationCount; ++i)
        if (p.getDistanceFrom(markerPosition(i)) < 11.0f) return i;
    return -1;
}

juce::Point<float> WorldbuilderEditor::landmarkPosition(int slot) const {
    const int w = planetScene.getWidth() / pixel::unit, h = planetScene.getHeight() / pixel::unit;
    juce::Point<float> at;
    float tilt = 0;
    if (!ui::TinyPlanet::placePoint(w, h, ui::TinyPlanet::kindOf(placeType(slot)), landmarkLongitude(slot), planetTurn(), animTime, at, tilt))
        return { -1000.0f, -1000.0f };   // over the horizon, or behind the planet
    return { float(planetScene.getX()) + at.x * float(pixel::unit), float(planetScene.getY()) + at.y * float(pixel::unit) };
}

int WorldbuilderEditor::landmarkAt(juce::Point<float> p) const {
    int best = -1;
    float bestDistance = 26.0f;
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
    // The pixel mood follows the same presences: fantasy, blend, sci-fi, neutral. Its
    // decoration animates at 8 frames a second; the Overlays setting sets the delights.
    std::array<float, PlaceTypeCount> eased {};
    for (int type = NoPlace + 1; type < PlaceTypeCount; ++type) eased[std::size_t(type)] = weights[std::size_t(type)];
    const int delights = overlays == nullptr ? 0 : overlays->mode() == ui::OverlaysFull ? 2 : overlays->mode() == ui::OverlaysSubtle ? 1 : 0;
    const int before = pixel::theme().version;
    pixelMood = pixel::moodOf(eased.data(), PlaceTypeCount);
    pixelDelights = delights;
    pixel::setTheme(pixelMood, int(animTime * 8.0f) % 600, delights);
    if (pixel::theme().version != before) themeMoving = true;
}

void WorldbuilderEditor::updateOverlayFrames() {
    if (overlays == nullptr) return;
    std::vector<juce::Rectangle<int>> panels { controlsPanel, layersPanel, placesPanel, locationsPanel, footer };
    if (detail.slot >= 0) {
        panels.push_back(detail.area);
    } else {
        panels.push_back(globePanel);
        panels.push_back(cardsPanel);
    }
    // Never on a control, nor on text near a frame (the header, the footer's labels).
    std::vector<juce::Rectangle<int>> keep { headerTitle, headerSubtitle, intensityLabel };
    for (auto* c : getChildren())
        if (c != overlays.get() && c->isVisible()) keep.push_back(c->getBounds().expanded(2));
    for (auto* c : std::initializer_list<juce::Component*> { &root, &mood, &quality, &playMode, &seed, &overlayChoice, &placeOuts, &uiSize })
        keep.push_back(c->getBounds().translated(0, -16).withHeight(16));
    keep.push_back(uiSize.getBounds().translated(uiSize.getWidth() + 6, 0).withWidth(90));   // tempo
    for (const auto& area : groupAreas) keep.push_back(area.withHeight(18));
    keep.push_back(layersPanel.reduced(10, 6).withHeight(20));
    keep.push_back(placesPanel.reduced(10, 6).withHeight(20));
    if (detail.slot < 0) {
        // The planet's sky and horizon are a picture: the overlays keep off the whole of it.
        keep.push_back(planetScene);
    }
    if (keep == overlayKeepClear) return;
    overlayKeepClear = keep;
    juce::RectangleList<int> clear;
    for (auto r : keep) clear.addWithoutMerging(r);
    std::vector<juce::Rectangle<int>> open;
    if (!headerOpen.isEmpty()) open.push_back(headerOpen);
    overlays->setFrames(panels, open, clear);
}

int WorldbuilderEditor::cardAt(juce::Point<int> p) const {
    if (!cardGrid.contains(p)) return -1;
    for (int i = 0; i < listedCount; ++i)
        if (cardBounds(i).contains(p)) return listed[std::size_t(i)];
    return -1;
}

void WorldbuilderEditor::showCategory(int c) {
    category = juce::jlimit(-1, PlaceCategoryCount - 1, c);
    cardScroll = 0;
    listCards();
    repaint(cardsPanel);
}

void WorldbuilderEditor::mouseDown(const juce::MouseEvent& e) {
    if (detail.slot >= 0 || covered()) return;
    if (const int marker = markerAt(e.position); marker >= 0) {
        if (processor.locations.used(marker)) processor.travelTo(marker);
        return;
    }
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
        dragFrom = e.position;
        dragPan = param(placeParam(dragLandmark, PlPan));
        dragDistance = param(placeParam(dragLandmark, PlDistance));
        for (int field : { PlPan, PlDistance })
            if (auto* parameter = processor.parameters.getParameter(specs[std::size_t(placeParam(dragLandmark, field))].id)) parameter->beginChangeGesture();
        if (e.getNumberOfClicks() >= 2) openDetail(dragLandmark);
    }
}

void WorldbuilderEditor::mouseDrag(const juce::MouseEvent& e) {
    if (dragLandmark < 0) return;
    // Along the horizon: Pan; up and down: Distance (up is nearer). The planet holds still
    // while a Place is dragged.
    const float r = ui::TinyPlanet::radius(planetScene.getWidth() / pixel::unit, planetScene.getHeight() / pixel::unit) * float(pixel::unit);
    const float pan = juce::jlimit(-1.0f, 1.0f, dragPan + (e.position.x - dragFrom.x) / r);
    const float d = juce::jlimit(0.0f, 1.0f, dragDistance + (e.position.y - dragFrom.y) / 120.0f);
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
    if (const int chip = detail.slot >= 0 ? -1 : categoryAt(e.getPosition()); chip != hoverChip) {
        hoverChip = chip;
        repaint(cardsPanel);
    }
    const bool overLandmark = detail.slot < 0 && landmarkAt(e.position) >= 0;
    const bool clickable = (card >= 0 && placeImplemented[std::size_t(card)]) || overLandmark || (detail.slot < 0 && categoryAt(e.getPosition()) >= 0);
    setMouseCursor(clickable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (card >= 0) setHelpText(placeInfo(card).description);
}

void WorldbuilderEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    if (detail.slot >= 0 || covered() || !cardsPanel.contains(e.getPosition())) return;
    const float before = cardScroll;
    cardScroll = juce::jlimit(0.0f, maxCardScroll(), cardScroll - wheel.deltaY * 160.0f);
    if (cardScroll != before) repaint(cardsPanel);
}

void WorldbuilderEditor::mouseExit(const juce::MouseEvent&) {
    if (hoverCard != -1 || hoverChip != -1) {
        hoverCard = hoverChip = -1;
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
    frameTick(juce::Time::getMillisecondCounterHiRes());
}

void WorldbuilderEditor::frameTick(double nowMs) {
    // Slow frames for two seconds with the overlays on Full: drop them to Subtle.
    if (overlays->mode() == ui::OverlaysFull && governor.slowFor(nowMs)) {
        setOverlayMode(ui::OverlaysSubtle);
        governor.resetSlow();
        autoSubtle = true;
    }
    // Skipped ticks still move the animation on: the next drawn frame catches up.
    double elapsed = 0;
    const bool draw = governor.beginFrame(nowMs, elapsed);
    owedSeconds += elapsed;
    if (!draw) return;
    stepAnimation(float(std::min(0.25, owedSeconds)));
    owedSeconds = 0;
}

void WorldbuilderEditor::paintOverChildren(juce::Graphics&) {
    // The whole frame (this editor and every component in it) has painted: its cost.
    if (paintStarted < 0) return;
    const double now = juce::Time::getMillisecondCounterHiRes();
    governor.painted(now - paintStarted, now);
    paintStarted = -1;
}

void WorldbuilderEditor::settle(float seconds) {
    for (float t = 0; t < seconds; t += 1.0f / 30.0f) stepAnimation(1.0f / 30.0f);
}

void WorldbuilderEditor::stepAnimation(float seconds) {
    animTime += seconds;
    if (animTime > 3600.0f) animTime = 0;
    refreshPlaceStrips();
    {
        // A project or .world loaded by the host: the scene browser follows.
        const auto name = processor.sceneName().isEmpty() ? juce::String("Scenes...") : processor.sceneName();
        if (sceneMenu.getButtonText() != name) sceneMenu.setButtonText(name);
        if (sceneUndo.isEnabled() != processor.canUndoScene()) sceneUndo.setEnabled(processor.canUndoScene());
    }
    updateTheme(seconds);
    // The whole interface repaints while the Places' colours are moving.
    if (themeMoving) repaint();
    if (const int stored = int(processor.parameters.state.getProperty("overlays", ui::defaultOverlayMode)); stored != overlays->mode()) setOverlayMode(stored);
    updateOverlayFrames();
    overlays->advance(seconds);
    // Frames' worth of time (the governor may skip ticks; the motion keeps its speed).
    const float frames = seconds * 30.0f;
    bool animating = false;
    for (int type = 0; type < PlaceTypeCount; ++type) {
        auto& h = hoverAmount[std::size_t(type)];
        const float target = hoverCard == type && detail.slot < 0 ? 1.0f : 0.0f;
        h += std::min(1.0f, 0.2f * frames) * (target - h);
        if (std::abs(target - h) < 0.002f) h = target;
        animating |= h > 0.0f;
    }
    if (pixel::theme().frame != pixelFrame) {
        // The decoration moved on: animated icons and hovered sprites (Full only, or the
        // hovered card when Subtle).
        pixelFrame = pixel::theme().frame;
        if (pixel::theme().delights > 0 && detail.slot < 0 && !covered()) repaint(cardsPanel);
    }
    if (animating && detail.slot < 0 && !covered()) repaint(cardsPanel);
    if (detail.slot >= 0) repaint(detail.area.withHeight(detail.titleArea.getBottom() - detail.area.getY() + 8));
    {
        // A new Planet crossfades on the globe over about a second, as it does in the sound.
        const int planet = juce::jlimit(0, int(dsp::PlanetTypeCount) - 1, juce::roundToInt(param(PPlanet)));
        if (planet != shownPlanet) {
            previousPlanet = planetBlend < 0.5f ? previousPlanet : shownPlanet;
            shownPlanet = planet;
            planetBlend = 0.0f;
        }
        planetBlend = std::min(1.0f, planetBlend + seconds / 1.1f);
        const bool globeShown = detail.slot < 0 && !covered();
        planetChoice.setVisible(globeShown);
        planetInfluence.setVisible(globeShown);
    }
    if (dragLandmark < 0) spin += frames * (0.004f + 0.02f * processor.engine.frame()[Activity]);
    // On a journey the globe turns faster, most in the middle of the way.
    if (processor.travel.uiMode.load(std::memory_order_relaxed) == Travel::Travelling)
        spin += frames * 0.06f * std::sin(juce::MathConstants<float>::pi * processor.travel.uiProgress.load(std::memory_order_relaxed));
    if (spin > juce::MathConstants<float>::twoPi * 100.0f) spin = 0;
    precipitation += frames * (0.05f + 0.06f * processor.engine.frame()[Weather]);
    if (precipitation > 1000.0f) precipitation = 0;
    const float strike = processor.engine.lightning.load(std::memory_order_relaxed);
    if (strike > flash + 0.2f) ++boltSeed;
    flash = strike;
    for (std::size_t i = 0; i < meters.size(); ++i) meters[i] = processor.engine.layerMeter[i].load(std::memory_order_relaxed);
    for (std::size_t i = 0; i < placeMeters.size(); ++i) placeMeters[i] = processor.engine.placeMeter[i].load(std::memory_order_relaxed);
    masterMeter = processor.engine.masterMeter.load(std::memory_order_relaxed);

    if (detail.slot >= 0) updateDetailLabels();
    else if (!covered()) repaint(globePanel);
    repaint(layersPanel);
    repaint(placesPanel);
    repaint(footer.withTrimmedLeft(footer.getWidth() - 50));
}
