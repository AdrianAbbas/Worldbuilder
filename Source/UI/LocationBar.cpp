#include "LocationBar.h"
#include "Style.h"
#include "WorldLookAndFeel.h"

namespace wb::ui {

namespace {
juce::Font font(float size, bool bold = false) { return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)); }
juce::Colour locationColour(const WorldbuilderProcessor& p, int index) { return juce::Colour(p.locations.colour(index)); }
const char* colourNames[] = { "Brass", "Cyan", "Rose", "Moss", "Violet", "Ember", "Teal", "Bone" };
} // namespace

LocationBar::LocationBar(WorldbuilderProcessor& p) : processor(p) {
    auto& state = processor.parameters;
    travelTime.setSliderStyle(juce::Slider::LinearHorizontal);
    travelTime.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    travelTime.setTooltip("Travel Time: how long a journey to a Location takes (0.5 to 60 s), eased in and out. "
                          "Automatable, like the Destination.");
    travelTime.setDoubleClickReturnValue(true, specs[PTravelTime].def);
    addAndMakeVisible(travelTime);
    travelTimeAttachment = std::make_unique<SliderAttachment>(state, specs[PTravelTime].id, travelTime);

    travelSync.addItemList(juce::StringArray::fromTokens(travelSyncChoices, "|", ""), 1);
    travelSync.setTooltip("Seconds: the Travel Time. Or a number of 4/4 bars of the host tempo, so a journey lands on a downbeat.");
    addAndMakeVisible(travelSync);
    travelSyncAttachment = std::make_unique<ComboAttachment>(state, specs[PTravelSync].id, travelSync);

    journeyMode.setTooltip("Journey Mode: the world morphs between From and To at the Journey position, so a drawn automation curve "
                           "walks the listener from one Location to the other. Switching it off keeps the world where it stands.");
    journeyMode.setColour(juce::TextButton::buttonOnColourId, colours::cyan);
    setIcon(journeyMode, Icon::Journey);
    addAndMakeVisible(journeyMode);
    journeyModeAttachment = std::make_unique<ButtonAttachment>(state, specs[PJourneyOn].id, journeyMode);
    for (auto* box : { &from, &to }) {
        box->addItemList(juce::StringArray::fromTokens(locationChoices, "|", ""), 1);
        addAndMakeVisible(*box);
    }
    from.setTooltip("Journey From: the Location at Journey 0 %.");
    to.setTooltip("Journey To: the Location at Journey 100 %.");
    fromAttachment = std::make_unique<ComboAttachment>(state, specs[PJourneyFrom].id, from);
    toAttachment = std::make_unique<ComboAttachment>(state, specs[PJourneyTo].id, to);
    journey.setSliderStyle(juce::Slider::LinearHorizontal);
    journey.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    journey.setTooltip("Journey: the position between From (0 %) and To (100 %) in Journey Mode. Draw it as automation.");
    journey.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(journey);
    journeyAttachment = std::make_unique<SliderAttachment>(state, specs[PJourney].id, journey);

    pathMode.setTooltip("Journey Path: the world visits the saved Locations in order, one leg every so many bars of the song, "
                        "arriving at each on the downbeat (the journey takes the Travel Time). It follows the song position, so it plays "
                        "the same every time. Switching it off keeps the world where it stands.");
    pathMode.setColour(juce::TextButton::buttonOnColourId, colours::cyan);
    setIcon(pathMode, Icon::Path);
    setIcon(saveFile, Icon::Save);
    setIcon(loadFile, Icon::Load);
    addAndMakeVisible(pathMode);
    pathModeAttachment = std::make_unique<ButtonAttachment>(state, specs[PPathOn].id, pathMode);
    pathLeg.addItemList(juce::StringArray::fromTokens(pathLegChoices, "|", ""), 1);
    pathLeg.setTooltip("Path Leg: how long the Path stays with each Location, journey included.");
    addAndMakeVisible(pathLeg);
    pathLegAttachment = std::make_unique<ComboAttachment>(state, specs[PPathLeg].id, pathLeg);
    pathEnd.addItemList(juce::StringArray::fromTokens(pathEndChoices, "|", ""), 1);
    pathEnd.setTooltip("One-way: the Path stops at the last Location. Loop: it travels back to the first and goes round again.");
    addAndMakeVisible(pathEnd);
    pathEndAttachment = std::make_unique<ComboAttachment>(state, specs[PPathEnd].id, pathEnd);

    saveFile.setTooltip("Save the whole world as a .world file: the Places, patterns, effects, World Controls, Score Intensity and all "
                        "eight Locations. Power, Master Level, Quality and Play Mode are not part of it.");
    loadFile.setTooltip("Load a .world file: its world and its eight Locations replace these.");
    saveFile.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Save the world", worldFolder().getChildFile("My World.world"), "*.world");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                             [this](const juce::FileChooser& c) {
                                 const auto file = c.getResult();
                                 if (file != juce::File()) saveWorld(file.withFileExtension("world"));
                             });
    };
    loadFile.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Load a world", worldFolder(), "*.world");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& c) {
            const auto file = c.getResult();
            if (file.existsAsFile()) loadWorld(file);
        });
    };
    for (auto* b : { &saveFile, &loadFile }) addAndMakeVisible(*b);
    startTimerHz(30);
}

LocationBar::~LocationBar() { stopTimer(); }

void LocationBar::resized() {
    auto r = getLocalBounds();
    // The top line: the title and what the world is doing; PATH and the files on the right.
    auto top = r.removeFromTop(22);
    {
        auto right = top;
        loadFile.setBounds(right.removeFromRight(84).withSizeKeepingCentre(84, 20));
        right.removeFromRight(6);
        saveFile.setBounds(right.removeFromRight(84).withSizeKeepingCentre(84, 20));
        right.removeFromRight(18);
        pathEnd.setBounds(right.removeFromRight(86).withSizeKeepingCentre(86, 20));
        right.removeFromRight(6);
        pathLeg.setBounds(right.removeFromRight(80).withSizeKeepingCentre(80, 20));
        right.removeFromRight(6);
        pathMode.setBounds(right.removeFromRight(62).withSizeKeepingCentre(62, 20));
        title = top.withRight(pathMode.getX() - 12);
    }
    r.removeFromTop(4);
    auto labels = r.removeFromTop(13);
    auto row = r.withSizeKeepingCentre(r.getWidth(), 30);
    // The chips, then TRAVEL and JOURNEY on the right.
    const int chipWidth = 66, gap = 5;
    auto chipRow = row.removeFromLeft(locationCount * chipWidth + (locationCount - 1) * gap);
    for (int i = 0; i < locationCount; ++i) {
        chips[std::size_t(i)] = chipRow.removeFromLeft(chipWidth);
        chipRow.removeFromLeft(gap);
    }
    row.removeFromLeft(18);
    auto travelArea = row.removeFromLeft(202);
    travelLabel = labels.withX(travelArea.getX()).withWidth(travelArea.getWidth());
    travelTime.setBounds(travelArea.removeFromLeft(80).withSizeKeepingCentre(80, 18));
    travelValue = travelArea.removeFromLeft(42);
    travelSync.setBounds(travelArea.removeFromLeft(80).withSizeKeepingCentre(80, 22));
    row.removeFromLeft(16);
    journeyLabel = labels.withX(row.getX()).withWidth(row.getWidth());
    journeyMode.setBounds(row.removeFromLeft(80).withSizeKeepingCentre(80, 22));
    row.removeFromLeft(6);
    from.setBounds(row.removeFromLeft(46).withSizeKeepingCentre(46, 22));
    arrow = row.removeFromLeft(14);
    to.setBounds(row.removeFromLeft(46).withSizeKeepingCentre(46, 22));
    row.removeFromLeft(8);
    journey.setBounds(row.withSizeKeepingCentre(row.getWidth(), 18));
}

bool LocationBar::canSave() const {
    return processor.travel.uiMode.load(std::memory_order_relaxed) == Travel::Idle && processor.parameters.getRawParameterValue(specs[PJourneyOn].id)->load() < 0.5f;
}

void LocationBar::paint(juce::Graphics& g) {
    g.setFont(font(11.5f, true).withExtraKerningFactor(0.15f));
    g.setColour(colours::brass.withAlpha(0.85f));
    g.drawText("LOCATIONS", title.withTrimmedLeft(4), juce::Justification::centredLeft, false);
    const int mode = processor.travel.uiMode.load(std::memory_order_relaxed);
    const int target = processor.travel.uiTarget.load(std::memory_order_relaxed);
    const int arrived = processor.travel.uiArrived.load(std::memory_order_relaxed);
    const float progress = processor.travel.uiProgress.load(std::memory_order_relaxed);
    {
        // What the world is doing, on the title row.
        juce::String state;
        if (mode == Travel::Travelling && target >= 0) state = "travelling to " + processor.locations.name(target) + "  " + juce::String(juce::roundToInt(progress * 100.0f)) + "%";
        else if (mode == Travel::Journeying) {
            const int a = processor.travel.uiFrom.load(std::memory_order_relaxed), b = processor.travel.uiTo.load(std::memory_order_relaxed);
            state = a >= 0 && b >= 0 ? (processor.locations.name(a) + " -> " + processor.locations.name(b) + "  " + juce::String(juce::roundToInt(progress * 100.0f)) + "%")
                                     : juce::String("Journey: no Locations saved");
        }
        else if (arrived >= 0 && processor.locations.used(arrived)) state = "at " + processor.locations.name(arrived);
        else state = "click an empty slot to save the world there";
        g.setFont(font(10.5f));
        g.setColour(mode == Travel::Idle || mode == Travel::Holding ? colours::dim : colours::cyan);
        g.drawText(state, title.withTrimmedLeft(96).withTrimmedRight(100), juce::Justification::centredLeft, true);
        g.setFont(font(10.5f, true).withExtraKerningFactor(0.12f));
        g.setColour(colours::brass.withAlpha(0.8f));
        g.drawText("JOURNEY PATH", pathMode.getBounds().withX(title.getRight() - 96).withWidth(92), juce::Justification::centredRight, false);
    }
    const bool journeyOn = mode == Travel::Journeying;
    const int journeyFrom = processor.travel.uiFrom.load(std::memory_order_relaxed);
    const int journeyTo = processor.travel.uiTo.load(std::memory_order_relaxed);
    for (int i = 0; i < locationCount; ++i) {
        const auto r = chips[std::size_t(i)].toFloat().reduced(0.5f);
        const bool used = processor.locations.used(i);
        const auto colour = locationColour(processor, i);
        const bool here = (mode == Travel::Idle || mode == Travel::Holding) && arrived == i;
        const bool going = mode == Travel::Travelling && target == i;
        const float corner = 6.0f;
        if (used) {
            // A raised chip in the Location's colour; the one the world stands at glows.
            drawSurface(g, r.reduced(0.5f), corner, colours::panel.interpolatedWith(colour, 0.22f), colour, SurfaceState { hover == i, false, false, true });
            if (here) drawGlow(g, r, corner, colour, 5.0f);
            // Journey Mode: how much of From and To is in the world.
            float fill = going ? progress : 0.0f;
            if (journeyOn) fill = i == journeyTo ? progress : i == journeyFrom ? 1.0f - progress : 0.0f;
            if (journeyOn && journeyFrom == journeyTo && i == journeyTo) fill = 1.0f;
            if (fill > 0.0f) {
                g.saveState();
                g.reduceClipRegion(r.withHeight(4.0f).withY(r.getBottom() - 4.0f).withWidth(r.getWidth() * juce::jlimit(0.0f, 1.0f, fill)).toNearestInt());
                g.setColour(colours::cyan.withAlpha(0.9f));
                g.fillRoundedRectangle(r, corner);
                g.restoreState();
            }
            g.setColour(colour);
            g.fillRoundedRectangle(r.withWidth(3.5f).reduced(0.0f, 4.0f), 1.5f);
            g.setColour(here ? colour.brighter(0.3f) : going ? colours::cyan : colour.withAlpha(0.55f));
            g.drawRoundedRectangle(r, corner, here || going ? 1.8f : 1.0f);
            if (here) {
                g.setColour(colour.withAlpha(0.22f));
                g.drawRoundedRectangle(r.expanded(2.0f), corner + 2.0f, 2.5f);
            }
            g.setFont(font(10.5f, true));
            g.setColour(colours::parchment);
            g.drawFittedText(processor.locations.name(i), r.withTrimmedLeft(7.0f).withTrimmedRight(3.0f).withTrimmedTop(6.0f).toNearestInt(),
                             juce::Justification::centred, 1, 0.8f);
        } else {
            juce::Path outline;
            outline.addRoundedRectangle(r, corner);
            juce::Path dashed;
            const float dashes[] = { 3.0f, 3.0f };
            juce::PathStrokeType(1.0f).createDashedStroke(dashed, outline, dashes, 2);
            g.setColour((hover == i ? colours::parchment : colours::dim).withAlpha(0.7f));
            g.fillPath(dashed);
            g.setFont(font(10.0f, true));
            g.drawText("+ SAVE", r.withTrimmedTop(6.0f), juce::Justification::centred, false);
        }
        g.setFont(font(8.5f, true));
        g.setColour(used ? colour.withAlpha(0.9f) : colours::dim);
        g.drawText(juce::String(i + 1), r.withTrimmedLeft(7.0f).withHeight(11.0f).translated(0.0f, 1.0f), juce::Justification::centredLeft, false);
    }
    g.setFont(font(10.5f, true).withExtraKerningFactor(0.12f));
    g.setColour(colours::brass.withAlpha(0.8f));
    g.drawText("TRAVEL", travelLabel, juce::Justification::centredLeft, false);
    g.drawText("JOURNEY      FROM       TO", journeyLabel, juce::Justification::centredLeft, false);
    const bool synced = juce::roundToInt(processor.parameters.getRawParameterValue(specs[PTravelSync].id)->load()) > 0;
    g.setFont(font(11.0f));
    g.setColour(synced ? colours::dim : colours::parchment);
    g.drawText(travelTime.getTextFromValue(travelTime.getValue()) + " s", travelValue, juce::Justification::centred, false);
    juce::Path p;
    const auto c = arrow.toFloat().getCentre();
    p.addTriangle(c.x - 3.0f, c.y - 4.0f, c.x - 3.0f, c.y + 4.0f, c.x + 3.0f, c.y);
    g.setColour(colours::brass.withAlpha(0.8f));
    g.fillPath(p);
}

void LocationBar::timerCallback() {
    if (!isShowing()) return;
    const uint32_t revision = processor.locations.revision();
    const int mode = processor.travel.uiMode.load(std::memory_order_relaxed);
    const int target = processor.travel.uiTarget.load(std::memory_order_relaxed);
    const int arrived = processor.travel.uiArrived.load(std::memory_order_relaxed);
    const float progress = processor.travel.uiProgress.load(std::memory_order_relaxed);
    if (revision != shownRevision || mode != shownMode || target != shownTarget || arrived != shownArrived || std::abs(progress - shownProgress) > 0.004f) {
        shownRevision = revision;
        shownMode = mode;
        shownTarget = target;
        shownArrived = arrived;
        shownProgress = progress;
        repaint();
    }
    travelTime.setEnabled(juce::roundToInt(processor.parameters.getRawParameterValue(specs[PTravelSync].id)->load()) == 0);
}

int LocationBar::chipAt(juce::Point<int> p) const {
    for (int i = 0; i < locationCount; ++i)
        if (chips[std::size_t(i)].contains(p)) return i;
    return -1;
}

void LocationBar::clickChip(int index) {
    if (index < 0 || index >= locationCount) return;
    if (processor.locations.used(index)) processor.travelTo(index);
    else if (canSave()) processor.saveLocation(index);
    repaint();
}

void LocationBar::mouseDown(const juce::MouseEvent& e) {
    const int index = chipAt(e.getPosition());
    if (index < 0) return;
    if (e.mods.isPopupMenu() || e.mods.isAltDown()) showMenu(index);
    else if (e.getNumberOfClicks() == 1) clickChip(index);
}

void LocationBar::mouseDoubleClick(const juce::MouseEvent& e) {
    if (const int index = chipAt(e.getPosition()); index >= 0 && processor.locations.used(index)) rename(index);
}

void LocationBar::mouseMove(const juce::MouseEvent& e) {
    if (const int index = chipAt(e.getPosition()); index != hover) {
        hover = index;
        repaint();
    }
}

void LocationBar::mouseExit(const juce::MouseEvent&) {
    hover = -1;
    repaint();
}

juce::File LocationBar::worldFolder() {
    auto folder = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("WORLDBUILDER").getChildFile("Worlds");
    folder.createDirectory();
    return folder;
}

bool LocationBar::saveWorld(const juce::File& file) {
    const auto text = processor.exportWorld();
    return text.isNotEmpty() && file.replaceWithText(text);
}

bool LocationBar::loadWorld(const juce::File& file) {
    const bool ok = file.getSize() < 4 * 1024 * 1024 && processor.importWorld(file.loadFileAsString());
    repaint();
    return ok;
}

juce::String LocationBar::getTooltip() {
    const int index = chipAt(getMouseXYRelative());
    if (index < 0) return {};
    if (!processor.locations.used(index))
        return "Location " + juce::String(index + 1) + " is empty: click to save the world here (its Places, patterns, effects, "
               "World Controls and Score Intensity). Right-click for more.";
    return processor.locations.name(index) + ": click to travel here over the Travel Time. Double-click to rename; "
           "right-click to save over it, change its colour or clear it.";
}

void LocationBar::showMenu(int index) {
    const bool used = processor.locations.used(index);
    juce::PopupMenu menu;
    menu.addSectionHeader(processor.locations.name(index));
    menu.addItem(1, used ? "Save the world here (replace)" : "Save the world here", canSave());
    menu.addItem(2, "Travel here", used);
    menu.addItem(3, "Rename...", used);
    juce::PopupMenu colours;
    for (int c = 0; c < int(locationColours.size()); ++c) colours.addColouredItem(10 + c, colourNames[c], juce::Colour(locationColours[std::size_t(c)]));
    menu.addSubMenu("Colour", colours, used);
    menu.addSeparator();
    menu.addItem(4, "Clear", used);
    juce::Component::SafePointer<LocationBar> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(localAreaToGlobal(chips[std::size_t(index)])),
                       [safe, index](int result) {
                           if (safe == nullptr) return;
                           auto& p = safe->processor;
                           if (result == 1) p.saveLocation(index);
                           else if (result == 2) p.travelTo(index);
                           else if (result == 3) safe->rename(index);
                           else if (result == 4) p.locations.clear(index);
                           else if (result >= 10) p.locations.setColour(index, result - 10);
                           safe->repaint();
                       });
}

void LocationBar::rename(int index) {
    renameWindow = std::make_unique<juce::AlertWindow>("Rename Location " + juce::String(index + 1), "", juce::MessageBoxIconType::NoIcon, this);
    renameWindow->addTextEditor("name", processor.locations.name(index));
    renameWindow->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
    renameWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<LocationBar> safe(this);
    renameWindow->enterModalState(true, juce::ModalCallbackFunction::create([safe, index](int result) {
                                      if (safe == nullptr || safe->renameWindow == nullptr) return;
                                      if (result == 1) safe->processor.locations.rename(index, safe->renameWindow->getTextEditorContents("name"));
                                      safe->renameWindow.reset();
                                      safe->repaint();
                                  }),
                                  false);
}

} // namespace wb::ui
