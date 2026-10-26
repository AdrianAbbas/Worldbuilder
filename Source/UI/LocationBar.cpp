#include "LocationBar.h"
#include "Pixel.h"
#include "Style.h"
#include "WorldLookAndFeel.h"

namespace wb::ui {

namespace {
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
        loadFile.setBounds(right.removeFromRight(96).withSizeKeepingCentre(96, 20));
        right.removeFromRight(6);
        saveFile.setBounds(right.removeFromRight(96).withSizeKeepingCentre(96, 20));
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
    pixel::snapChildren(*this);
}

bool LocationBar::canSave() const {
    return processor.travel.uiMode.load(std::memory_order_relaxed) == Travel::Idle && processor.parameters.getRawParameterValue(specs[PJourneyOn].id)->load() < 0.5f;
}

void LocationBar::paint(juce::Graphics& g) {
    const auto& pal = pixel::theme().colours;
    pixel::drawText(g, "LOCATIONS", title.withTrimmedLeft(4).withWidth(116), pal.rim, juce::Justification::centredLeft, pixel::Font::Large);
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
            state = a >= 0 && b >= 0 ? (processor.locations.name(a) + " > " + processor.locations.name(b) + "  " + juce::String(juce::roundToInt(progress * 100.0f)) + "%")
                                     : juce::String("Journey: no Locations saved");
        }
        else if (arrived >= 0 && processor.locations.used(arrived)) state = "at " + processor.locations.name(arrived);
        else state = "click an empty slot to save the world there";
        pixel::drawText(g, state, title.withTrimmedLeft(124).withTrimmedRight(104), mode == Travel::Idle || mode == Travel::Holding ? pal.inkDim : pal.inlay,
                        juce::Justification::centredLeft, pixel::Font::Small);
        pixel::drawText(g, "JOURNEY PATH", pathMode.getBounds().withX(title.getRight() - 104).withWidth(100), pal.rim, juce::Justification::centredRight, pixel::Font::Small);
    }
    const bool journeyOn = mode == Travel::Journeying;
    const int journeyFrom = processor.travel.uiFrom.load(std::memory_order_relaxed);
    const int journeyTo = processor.travel.uiTo.load(std::memory_order_relaxed);
    for (int i = 0; i < locationCount; ++i) {
        const auto r = chips[std::size_t(i)];
        const bool used = processor.locations.used(i);
        const auto colour = locationColour(processor, i);
        const bool here = (mode == Travel::Idle || mode == Travel::Holding) && arrived == i;
        const bool going = mode == Travel::Travelling && target == i;
        const int w = r.getWidth() / pixel::unit, h = r.getHeight() / pixel::unit;
        if (used) {
            // A raised chip in the Location's colour; the one the world stands at glows.
            float fill = going ? progress : 0.0f;
            if (journeyOn) fill = i == journeyTo ? progress : i == journeyFrom ? 1.0f - progress : 0.0f;
            if (journeyOn && journeyFrom == journeyTo && i == journeyTo) fill = 1.0f;
            const int filled = juce::roundToInt(juce::jlimit(0.0f, 1.0f, fill) * float(w - 2));
            pixel::drawCached(g, "loc/" + juce::String(w) + "x" + juce::String(h) + colour.toString() + juce::String(int(here)) + juce::String(int(going)) + juce::String(int(hover == i))
                                     + "/" + juce::String(filled),
                              r.getPosition(), [&] {
                                  auto k = pixel::strip(pal, w, h - 1, colour, here || going);
                                  if (hover == i) k.outlineBox(0, 0, w, h - 1, pal.glow.withAlpha(0.7f));
                                  if (filled > 0) k.fill(1, h - 4, filled, 2, pal.inlay);
                                  return k;
                              });
            pixel::drawText(g, processor.locations.name(i), r.withTrimmedLeft(8).withTrimmedRight(3).withTrimmedTop(8), pal.ink, juce::Justification::centred, pixel::Font::Small);
        } else {
            pixel::drawCached(g, "locfree/" + juce::String(w) + "x" + juce::String(h) + juce::String(int(hover == i)), r.getPosition(), [&] {
                pixel::Canvas k(w, h);
                const auto c = hover == i ? pal.ink : pal.inkDim.withAlpha(0.7f);
                for (int x = 1; x < w - 1; ++x)
                    if ((x / 2) % 2 == 0) { k.set(x, 0, c); k.set(x, h - 1, c); }
                for (int y = 1; y < h - 1; ++y)
                    if ((y / 2) % 2 == 0) { k.set(0, y, c); k.set(w - 1, y, c); }
                return k;
            });
            pixel::drawText(g, "+ SAVE", r.withTrimmedTop(8), hover == i ? pal.ink : pal.inkDim, juce::Justification::centred, pixel::Font::Small);
        }
        pixel::drawText(g, juce::String(i + 1), r.withTrimmedLeft(8).withHeight(12).translated(0, 2), used ? colour.brighter(0.3f) : pal.inkDim,
                        juce::Justification::centredLeft, pixel::Font::Small);
    }
    pixel::drawText(g, "TRAVEL", travelLabel, pal.rim, juce::Justification::centredLeft, pixel::Font::Small);
    pixel::drawText(g, "JOURNEY", journeyLabel.withX(journeyMode.getX()).withWidth(journeyMode.getWidth()), pal.rim, juce::Justification::centredLeft, pixel::Font::Small);
    pixel::drawText(g, "FROM", journeyLabel.withX(from.getX()).withWidth(from.getWidth()), pal.rim, juce::Justification::centredLeft, pixel::Font::Small);
    pixel::drawText(g, "TO", journeyLabel.withX(to.getX()).withWidth(to.getWidth()), pal.rim, juce::Justification::centredLeft, pixel::Font::Small);
    const bool synced = juce::roundToInt(processor.parameters.getRawParameterValue(specs[PTravelSync].id)->load()) > 0;
    pixel::drawText(g, travelTime.getTextFromValue(travelTime.getValue()) + " S", travelValue, synced ? pal.inkDim : pal.accent, juce::Justification::centred, pixel::Font::Small);
    pixel::drawCached(g, "arrow", { arrow.getCentreX() - 4, arrow.getCentreY() - 6 }, [&] {
        pixel::Canvas k(4, 6);
        for (int c = 0; c < 3; ++c) k.vline(c, c, 5 - c, pal.rim);
        return k;
    });
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
