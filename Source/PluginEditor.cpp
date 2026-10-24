#include "PluginEditor.h"

using namespace wb;
namespace colours = wb::ui::colours;

namespace {
struct Group {
    const char* title;
    std::array<int, 3> params;   // -1 = empty slot
};
constexpr std::array<Group, 4> groups { {
    { "CLIMATE", { PTemperature, PWeather, PWetness } },
    { "CIVILISATION", { PMachinery, PPopulation, PEra } },
    { "MYSTERY", { PMagic, PLife, PDanger } },
    { "SIZE", { PScale, PActivity, -1 } },
} };

const char* tooltipFor(int p) {
    switch (p) {
        case PTemperature: return "Frozen to scorching: creaking, singing ice and thin high wind; or crackling fire, lava and a low dry rush.";
        case PWeather: return "Still to storm: wind and howling, drizzle to downpour, thunder more often and closer.";
        case PWetness: return "Arid to drenched: drips, then rain, streams, a waterfall and lapping waves. Douses fire.";
        case PMachinery: return "None to industrial. Machinery layer arrives in Milestone 3.";
        case PPopulation: return "Empty to crowded. Population layer arrives in Milestone 3.";
        case PEra: return "Ancient to futuristic: organic or synthetic colour across the world.";
        case PMagic: return "Mundane to arcane: in this build it widens and brightens the Tonal Bed.";
        case PLife: return "Barren to teeming. Life layer arrives in Milestone 3.";
        case PDanger: return "Peaceful to hostile: darker tuning, a dissonant voice, rumbling ground, rockfalls, closer storms.";
        case PScale: return "Intimate to colossal: World Space size and decay; water, rockfalls and thunder further away.";
        case PActivity: return "Calm to chaotic: how fast the world moves; more drips, crackles, creaks and rockfalls.";
        case PSpaceMix: return "Amount of the shared World Space reverb.";
        case PWidth: return "Stereo width of the whole world.";
        case PMaster: return "Output level.";
        default: return "";
    }
}

juce::Colour globeColour(float temperature, float danger) {
    const juce::Colour ice { 0xff9fd3f0 }, temperate { 0xff3f8f7a }, scorch { 0xffd2622c };
    auto c = temperature < 0.5f ? ice.interpolatedWith(temperate, temperature * 2.0f)
                                : temperate.interpolatedWith(scorch, (temperature - 0.5f) * 2.0f);
    return c.interpolatedWith(juce::Colour(0xff2a0f14), danger * 0.55f);
}
} // namespace

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
        s.level.setTooltip(juce::String(layerNames[std::size_t(l)]) + " level"
                           + (layerImplemented[std::size_t(l)] ? "" : " (layer arrives in a later milestone)"));
        s.mute.setColour(juce::TextButton::buttonOnColourId, colours::danger);
        s.solo.setColour(juce::TextButton::buttonOnColourId, colours::cyan);
        s.mute.setTooltip("Mute");
        s.solo.setTooltip("Solo");
        for (juce::Component* c : { static_cast<juce::Component*>(&s.level), static_cast<juce::Component*>(&s.mute),
                                    static_cast<juce::Component*>(&s.solo) })
            addAndMakeVisible(c);
        s.levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.parameters, specs[std::size_t(layerLevel(l))].id, s.level);
        s.level.setDoubleClickReturnValue(true, specs[std::size_t(layerLevel(l))].def);
        s.muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.parameters, specs[std::size_t(layerMute(l))].id, s.mute);
        s.soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.parameters, specs[std::size_t(layerSolo(l))].id, s.solo);
    }

    power.setColour(juce::TextButton::buttonOnColourId, colours::cyan);
    power.setTooltip("Fades the whole world in or out over 2 seconds");
    addAndMakeVisible(power);
    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "power", power);

    const auto setupCombo = [this](juce::ComboBox& box, int param, std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& attachment) {
        box.addItemList(juce::StringArray::fromTokens(specs[std::size_t(param)].choices, "|", ""), 1);
        addAndMakeVisible(box);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.parameters, specs[std::size_t(param)].id, box);
    };
    setupCombo(root, PRoot, rootAttachment);
    setupCombo(mood, PMood, moodAttachment);
    setupCombo(quality, PQuality, qualityAttachment);
    root.setTooltip("Root note of the Tonal Bed, so the world sits in your song's key");
    mood.setTooltip("Scale and mood of the Tonal Bed");
    quality.setTooltip("Eco / Standard / High: voice counts and detail");

    seed.setSliderStyle(juce::Slider::LinearBar);
    seed.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    seed.setTooltip("Seed: the same seed and settings always build the same world. Drag to change.");
    addAndMakeVisible(seed);
    seedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "seed", seed);
    dice.setTooltip("Roll a new seed");
    dice.onClick = [this] {
        if (auto* parameter = processor.parameters.getParameter("seed")) {
            const int value = juce::Random::getSystemRandom().nextInt({ 1, 10000 });
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(float(value)));
            parameter->endChangeGesture();
        }
    };
    addAndMakeVisible(dice);

    setSize(baseWidth, baseHeight);
    startTimerHz(30);
}

WorldbuilderEditor::~WorldbuilderEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

juce::Slider& WorldbuilderEditor::addKnob(int param) {
    const auto& spec = specs[std::size_t(param)];
    auto slider = std::make_unique<juce::Slider>(spec.name);
    slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider->setRotaryParameters(juce::degreesToRadians(225.0f), juce::degreesToRadians(495.0f), true);
    slider->setMouseDragSensitivity(220);
    slider->setVelocityBasedMode(false);
    slider->setTooltip(tooltipFor(param));
    addAndMakeVisible(*slider);
    knobAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, spec.id, *slider));
    slider->setDoubleClickReturnValue(true, spec.def);
    knobFor[std::size_t(param)] = slider.get();
    knobs.push_back(std::move(slider));
    return *knobs.back();
}

void WorldbuilderEditor::resized() {
    auto area = getLocalBounds().reduced(14, 10);
    header = area.removeFromTop(48);
    power.setBounds(header.withTrimmedLeft(header.getWidth() - 96).withSizeKeepingCentre(90, 28));
    area.removeFromTop(6);
    footer = area.removeFromBottom(92);
    area.removeFromBottom(8);
    controlsPanel = area.removeFromLeft(400);
    area.removeFromLeft(12);
    mixerPanel = area.removeFromRight(290);
    area.removeFromRight(12);
    globePanel = area;
    const float globeSize = std::min(float(globePanel.getWidth()) - 30.0f, float(globePanel.getHeight()) - 90.0f);
    globeArea = juce::Rectangle<float>(globeSize, globeSize).withCentre(globePanel.toFloat().getCentre().translated(0, -14));

    // World Controls: one row per group, title on the left.
    auto rows = controlsPanel.reduced(10, 8);
    const int rowHeight = rows.getHeight() / int(groups.size());
    for (std::size_t g = 0; g < groups.size(); ++g) {
        auto row = rows.removeFromTop(rowHeight);
        groupAreas[g] = row;
        row.removeFromTop(18);
        row.removeFromLeft(4);
        for (int param : groups[g].params) {
            auto cell = row.removeFromLeft(124);
            if (param >= 0) knobFor[std::size_t(param)]->setBounds(cell.reduced(14, 0));
        }
    }

    // Layer mixer.
    auto mixer = mixerPanel.reduced(10, 8);
    mixer.removeFromTop(22);
    const int stripHeight = mixer.getHeight() / LayerCount;
    for (auto& s : strips) {
        s.row = mixer.removeFromTop(stripHeight);
        auto r = s.row;
        r.removeFromLeft(92);   // name
        s.solo.setBounds(r.removeFromRight(26).withSizeKeepingCentre(24, 20));
        r.removeFromRight(3);
        s.mute.setBounds(r.removeFromRight(26).withSizeKeepingCentre(24, 20));
        r.removeFromRight(6);
        s.level.setBounds(r.withTrimmedBottom(10));
    }

    // Footer: tonal and global settings, then the output knobs.
    auto f = footer.reduced(12, 8);
    auto settings = f.removeFromLeft(470);
    auto top = settings.removeFromTop(settings.getHeight() / 2).withTrimmedTop(16);
    auto bottom = settings.withTrimmedTop(16);
    root.setBounds(top.removeFromLeft(80).withSizeKeepingCentre(80, 24));
    top.removeFromLeft(10);
    mood.setBounds(top.removeFromLeft(140).withSizeKeepingCentre(140, 24));
    top.removeFromLeft(10);
    quality.setBounds(top.removeFromLeft(120).withSizeKeepingCentre(120, 24));
    seed.setBounds(bottom.removeFromLeft(230).withSizeKeepingCentre(230, 22));
    bottom.removeFromLeft(10);
    dice.setBounds(bottom.removeFromLeft(110).withSizeKeepingCentre(110, 24));
    f.removeFromRight(40);   // master meter
    for (int param : { PMaster, PWidth, PSpaceMix })
        knobFor[std::size_t(param)]->setBounds(f.removeFromRight(96).withSizeKeepingCentre(80, 76));
}

void WorldbuilderEditor::paint(juce::Graphics& g) {
    g.setGradientFill(juce::ColourGradient(colours::background.brighter(0.08f), 0, 0, colours::background.darker(0.4f), 0,
                                           float(getHeight()), false));
    g.fillAll();

    // Header: wordmark and subtitle.
    g.setColour(colours::brass);
    g.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)).withExtraKerningFactor(0.18f));
    g.drawText("WORLDBUILDER", header.withTrimmedLeft(4), juce::Justification::centredLeft, false);
    g.setColour(colours::dim);
    g.setFont(juce::Font(juce::FontOptions(12.0f)).withExtraKerningFactor(0.08f));
    g.drawText("PROCEDURAL SOUNDSCAPE ENGINE  -  MILESTONE 2 PREVIEW", header.withTrimmedLeft(300).withTrimmedRight(110),
               juce::Justification::centredLeft, false);
    g.setColour(colours::brass.withAlpha(0.35f));
    g.drawHorizontalLine(header.getBottom() + 2, float(header.getX()), float(header.getRight()));

    for (auto panel : { controlsPanel, globePanel, mixerPanel, footer }) {
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
    g.drawText("LAYERS", mixerPanel.reduced(16, 8).withHeight(16), juce::Justification::centredLeft, false);
    g.drawText("LIVING GLOBE", globePanel.reduced(16, 8).withHeight(16), juce::Justification::centredLeft, false);

    // Mixer names, activity meters and "later" tags.
    for (int l = 0; l < LayerCount; ++l) {
        const auto& s = strips[std::size_t(l)];
        const bool implemented = layerImplemented[std::size_t(l)];
        auto nameArea = s.row.withWidth(92).withTrimmedLeft(4);
        g.setFont(juce::Font(juce::FontOptions(12.5f)));
        g.setColour(colours::parchment.withAlpha(implemented ? 0.95f : 0.4f));
        g.drawText(layerNames[std::size_t(l)], nameArea.withTrimmedBottom(10), juce::Justification::centredLeft, false);
        if (!implemented) {
            g.setFont(juce::Font(juce::FontOptions(9.5f)));
            g.setColour(colours::dim);
            g.drawText("MILESTONE 3", nameArea.withTrimmedTop(nameArea.getHeight() - 14),
                       juce::Justification::centredLeft, false);
        }
        const auto meterArea = s.level.getBounds().toFloat().withY(float(s.level.getBottom()) + 2.0f).withHeight(3.0f);
        g.setColour(colours::panelEdge);
        g.fillRoundedRectangle(meterArea, 1.5f);
        const float level = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(meters[std::size_t(l)], -60.0f) + 60.0f) / 60.0f);
        g.setColour(colours::cyan.withAlpha(0.8f));
        g.fillRoundedRectangle(meterArea.withWidth(meterArea.getWidth() * level), 1.5f);
    }

    // Footer labels and master meter.
    g.setFont(juce::Font(juce::FontOptions(10.5f, juce::Font::bold)).withExtraKerningFactor(0.12f));
    g.setColour(colours::brass.withAlpha(0.8f));
    for (auto [component, text] : { std::pair<juce::Component*, const char*> { &root, "ROOT" }, { &mood, "MOOD" },
                                    { &quality, "QUALITY" }, { &seed, "SEED" } })
        g.drawText(text, component->getBounds().translated(0, -16).withHeight(14), juce::Justification::centredLeft, false);
    const auto meter = juce::Rectangle<float>(float(footer.getRight() - 36), float(footer.getY() + 14), 10.0f, float(footer.getHeight() - 28));
    g.setColour(colours::panelEdge);
    g.fillRoundedRectangle(meter, 3.0f);
    const float masterLevel = juce::jlimit(0.0f, 1.0f, (juce::Decibels::gainToDecibels(masterMeter, -60.0f) + 60.0f) / 60.0f);
    g.setColour(masterMeter > 0.85f ? colours::danger : colours::cyan);
    g.fillRoundedRectangle(meter.withTop(meter.getBottom() - meter.getHeight() * masterLevel), 3.0f);

    paintGlobe(g, globeArea);
}

// Placeholder globe: shaded sphere coloured by the world, turning meridians, a cloud
// band for weather and an atmosphere ring for magic.
void WorldbuilderEditor::paintGlobe(juce::Graphics& g, juce::Rectangle<float> area) {
    const auto& world = processor.engine.frame();
    const float temperature = world[Temperature], danger = world[Danger], weather = world[Weather], magic = world[Magic];
    const float alpha = 0.25f + 0.75f * processor.engine.powerLevel.load(std::memory_order_relaxed);
    const auto centre = area.getCentre();
    const float r = area.getWidth() * 0.5f;

    // Atmosphere glow.
    const auto glow = colours::cyan.interpolatedWith(juce::Colour(0xffb78cff), magic).withAlpha(0.18f + 0.3f * magic);
    g.setGradientFill(juce::ColourGradient(glow, centre.x, centre.y, glow.withAlpha(0.0f), centre.x + r * 1.18f, centre.y, true));
    g.fillEllipse(area.expanded(r * 0.18f));

    const auto base = globeColour(temperature, danger);
    juce::ColourGradient sphere(base.brighter(0.6f).withMultipliedAlpha(alpha), centre.x - r * 0.4f, centre.y - r * 0.45f,
                                base.darker(1.6f).withMultipliedAlpha(alpha), centre.x + r * 0.7f, centre.y + r * 0.8f, true);
    g.setGradientFill(sphere);
    g.fillEllipse(area);

    juce::Graphics::ScopedSaveState clip(g);
    juce::Path disc;
    disc.addEllipse(area);
    g.reduceClipRegion(disc);

    // Meridians turn with `spin`; latitudes are fixed.
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

    // Clouds drift with the spin; denser and greyer with weather.
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

    // Lava fields glow on scorching worlds, turning with the planet.
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

    // Rain or snow across the face, slanted by the wind.
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

    // Terminator shading on the far side.
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, centre.x - r * 0.1f, centre.y,
                                           juce::Colours::black.withAlpha(0.55f), centre.x + r, centre.y + r * 0.3f, false));
    g.fillEllipse(area);
    g.setColour(colours::brass.withAlpha(0.5f));
    g.drawEllipse(area, 1.2f);

    // Lightning: the globe flashes and a bolt forks across the cloud tops. The thunder
    // itself arrives later, by the strike's distance.
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

}

void WorldbuilderEditor::timerCallback() {
    if (!isShowing()) return;
    spin += 0.004f + 0.02f * processor.engine.frame()[Activity];
    if (spin > juce::MathConstants<float>::twoPi * 100.0f) spin = 0;
    precipitation += 0.05f + 0.06f * processor.engine.frame()[Weather];
    if (precipitation > 1000.0f) precipitation = 0;
    const float strike = processor.engine.lightning.load(std::memory_order_relaxed);
    if (strike > flash + 0.2f) ++boltSeed;   // a new strike: a new bolt
    flash = strike;
    for (std::size_t i = 0; i < meters.size(); ++i) meters[i] = processor.engine.layerMeter[i].load(std::memory_order_relaxed);
    masterMeter = processor.engine.masterMeter.load(std::memory_order_relaxed);
    repaint(globePanel);
    repaint(mixerPanel);
    repaint(footer.withTrimmedLeft(footer.getWidth() - 50));
}
