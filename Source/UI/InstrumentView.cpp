#include "InstrumentView.h"
#include "Pixel.h"
#include "Style.h"
#include "WorldLookAndFeel.h"

namespace wb::ui {

namespace {
juce::String upper(const char* s) { return juce::String(s).toUpperCase(); }
juce::String placeName(int type) { return upper(placeNames[std::size_t(std::clamp(type, 0, PlaceTypeCount - 1))]); }
juce::String elementName(inst::Id id) { return upper(placeInfo(id.type).element[std::size_t(std::clamp(id.element, 0, placeElements - 1))].name); }
// "TAVERN > LUTE STRUM"
juce::String fullName(inst::Id id) { return placeName(id.type) + " > " + elementName(id); }
bool struck(inst::Id id) {
    const int g = inst::groupOf(id.type, id.element);
    return patternInfo(id.type, id.element).kind == Kit || g == inst::Plucks || g == inst::BellsMallets || g == inst::Percussion;
}
} // namespace

//==============================================================================
// The browser's list: group headings (a Place, or a type) over their instruments, one
// row each; the instrument playing is lit. Scrolls with the wheel (or a drag of the bar).
class InstrumentView::List final : public juce::Component {
public:
    explicit List(InstrumentView& v) : view(v) {}
    struct Row {
        bool heading = false;
        juce::String text;
        inst::Id id {};
        int place = 0;   // the Place icon shown on a heading (by Place)
    };
    std::vector<Row> rows;
    float scroll = 0;
    static constexpr int rowHeight = 18, headingHeight = 22;

    int heightOf(const Row& r) const { return r.heading ? headingHeight : rowHeight; }
    int contentHeight() const {
        int h = 0;
        for (const auto& r : rows) h += heightOf(r);
        return h;
    }
    float maxScroll() const { return float(std::max(0, contentHeight() - getHeight())); }
    void clampScroll() { scroll = juce::jlimit(0.0f, maxScroll(), scroll); }
    // Scrolls so that the row of `id` is in view.
    void reveal(inst::Id id) {
        int y = 0;
        for (const auto& r : rows) {
            if (!r.heading && r.id == id) {
                if (float(y) < scroll) scroll = float(std::max(0, y - headingHeight));
                else if (float(y + rowHeight) > scroll + float(getHeight())) scroll = float(y + rowHeight - getHeight() + 4);
                break;
            }
            y += heightOf(r);
        }
        clampScroll();
        repaint();
    }
    int rowAt(int y) const {
        int top = -int(scroll);
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const int h = heightOf(rows[i]);
            if (y >= top && y < top + h) return int(i);
            top += h;
        }
        return -1;
    }
    void paint(juce::Graphics& g) override {
        const auto& pal = pixel::theme().colours;
        g.setColour(pal.well.withAlpha(0.65f));
        g.fillRect(getLocalBounds());
        const auto now = view.processor.instrument();
        int y = -int(scroll);
        const int textRight = getWidth() - 10;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const auto& r = rows[i];
            const int h = heightOf(r);
            if (y + h >= 0 && y < getHeight()) {
                const juce::Rectangle<int> area(0, y, getWidth(), h);
                if (r.heading) {
                    g.setColour(pal.panelDark.withAlpha(0.9f));
                    g.fillRect(area.withTrimmedTop(2));
                    pixel::drawText(g, r.text, area.withTrimmedLeft(6).withRight(textRight), pal.heading, juce::Justification::centredLeft, pixel::Font::Small);
                } else {
                    const bool on = r.id == now, over = int(i) == hover;
                    if (on) {
                        g.setColour(pal.accent.withAlpha(0.85f));
                        g.fillRect(area.reduced(2, 1));
                    } else if (over) {
                        g.setColour(pal.faceLight.withAlpha(0.35f));
                        g.fillRect(area.reduced(2, 1));
                    }
                    pixel::drawText(g, r.text, area.withTrimmedLeft(14).withRight(textRight), on ? pal.inkOn : pal.ink, juce::Justification::centredLeft,
                                    pixel::Font::Small);
                }
            }
            y += h;
        }
        if (rows.empty())
            pixel::drawText(g, "NO INSTRUMENT MATCHES", getLocalBounds().withHeight(40), pal.inkDim, juce::Justification::centred, pixel::Font::Small);
        // The scroll bar.
        if (maxScroll() > 0.0f) {
            const float frac = float(getHeight()) / float(contentHeight());
            const int barH = std::max(16, int(float(getHeight()) * frac));
            const int barY = int((float(getHeight() - barH)) * scroll / maxScroll());
            g.setColour(pal.rim.withAlpha(0.8f));
            g.fillRect(getWidth() - 6, barY, 4, barH);
        }
    }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override {
        scroll -= w.deltaY * 160.0f;
        clampScroll();
        repaint();
    }
    void mouseMove(const juce::MouseEvent& e) override {
        const int r = rowAt(e.y);
        if (r != hover) {
            hover = r;
            repaint();
        }
    }
    void mouseExit(const juce::MouseEvent&) override {
        hover = -1;
        repaint();
    }
    void mouseDown(const juce::MouseEvent& e) override {
        if (e.x >= getWidth() - 10 && maxScroll() > 0.0f) {
            dragging = true;
            mouseDrag(e);
            return;
        }
        dragging = false;
        const int r = rowAt(e.y);
        if (r >= 0 && !rows[std::size_t(r)].heading) view.choose(rows[std::size_t(r)].id);
    }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (!dragging) return;
        scroll = maxScroll() * juce::jlimit(0.0f, 1.0f, float(e.y) / float(std::max(1, getHeight())));
        repaint();
    }

private:
    InstrumentView& view;
    int hover = -1;
    bool dragging = false;
};

//==============================================================================
// The keyboard: C1 to C8, lit by the notes arriving from the host (bright while held,
// dimmer while the pedal holds them). A click plays it too (higher on a key: louder).
class InstrumentView::Keyboard final : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit Keyboard(WorldbuilderProcessor& p) : processor(p) {
        setTooltip("The notes arriving from the host (Logic's or GarageBand's piano roll and keyboards), lit while they sound. "
                   "Click a key to hear it (lower on the key: louder).");
    }
    static constexpr int first = 24, last = 108;
    static constexpr bool black[12] { false, true, false, true, false, false, true, false, true, false, true, false };
    bool lit(int note) const {
        const auto& in = processor.engine.keysInput;
        return ((in.soundBits[std::size_t(note / 32)].load(std::memory_order_relaxed) >> (note % 32)) & 1u) != 0;
    }
    bool down(int note) const {
        const auto& in = processor.engine.keysInput;
        return ((in.downBits[std::size_t(note / 32)].load(std::memory_order_relaxed) >> (note % 32)) & 1u) != 0;
    }
    int whites() const {
        int w = 0;
        for (int n = first; n <= last; ++n) w += black[n % 12] ? 0 : 1;
        return w;
    }
    juce::Rectangle<int> keyArea(int note) const {
        const int count = whites(), w = std::max(4, getWidth() / count), offset = (getWidth() - w * count) / 2;
        int x = 0;
        for (int n = first; n < note; ++n) x += black[n % 12] ? 0 : 1;
        if (!black[note % 12]) return { offset + x * w, 0, w - 2, getHeight() };
        const int bw = std::max(4, w * 2 / 3) / 2 * 2;
        return { offset + x * w - bw / 2 - 1, 0, bw, getHeight() * 3 / 5 };
    }
    int noteAt(juce::Point<int> p) const {
        for (int n = first; n <= last; ++n)
            if (black[n % 12] && keyArea(n).contains(p)) return n;
        for (int n = first; n <= last; ++n)
            if (!black[n % 12] && keyArea(n).contains(p)) return n;
        return -1;
    }
    void paint(juce::Graphics& g) override {
        const auto& pal = pixel::theme().colours;
        const auto fill = [&](int n, juce::Colour base) {
            if (down(n)) return pal.accent;
            if (lit(n)) return pal.accent.withAlpha(0.6f).overlaidWith(base.withAlpha(0.25f));
            return base;
        };
        for (int n = first; n <= last; ++n) {
            if (black[n % 12]) continue;
            const auto r = keyArea(n);
            g.setColour(pal.outline);
            g.fillRect(r.expanded(1, 0));
            g.setColour(fill(n, pal.ink.withAlpha(0.92f)));
            g.fillRect(r.withTrimmedBottom(2));
            g.setColour(pal.rimDark.withAlpha(0.5f));
            g.fillRect(r.withTop(r.getBottom() - 4));
            if (n % 12 == 0)
                pixel::drawTinyText(g, "C" + juce::String(n / 12 - 1), r.withTop(r.getBottom() - 16).withTrimmedBottom(4), n == 60 ? pal.accent : pal.panelDark,
                                    juce::Justification::centred);
        }
        for (int n = first; n <= last; ++n) {
            if (!black[n % 12]) continue;
            const auto r = keyArea(n);
            g.setColour(pal.outline);
            g.fillRect(r.expanded(1, 1));
            g.setColour(fill(n, pal.panelDark));
            g.fillRect(r);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override { press(e.getPosition()); }
    void mouseDrag(const juce::MouseEvent& e) override {
        if (const int n = noteAt(e.getPosition()); n != playing) press(e.getPosition());
    }
    void mouseUp(const juce::MouseEvent&) override { release(); }
    int playing = -1;

private:
    void press(juce::Point<int> p) {
        release();
        const int n = noteAt(p);
        if (n < 0) return;
        const auto r = keyArea(n);
        const int velocity = juce::jlimit(30, 127, 40 + juce::roundToInt(87.0f * float(p.y - r.getY()) / float(std::max(1, r.getHeight()))));
        processor.playKey(n, velocity);
        playing = n;
        repaint();
    }
    void release() {
        if (playing >= 0) processor.playKey(playing, 0);
        playing = -1;
    }
    WorldbuilderProcessor& processor;
};

//==============================================================================
InstrumentView::InstrumentView(WorldbuilderProcessor& p) : processor(p) {
    setOpaque(false);
    list = std::make_unique<List>(*this);
    addAndMakeVisible(*list);
    keyboard = std::make_unique<Keyboard>(processor);
    addAndMakeVisible(*keyboard);

    // The browser.
    search.setTextToShowWhenEmpty("SEARCH: LUTE, BELLS, SPACEPORT...", pixel::theme().colours.inkDim);
    search.setTooltip("Type to find an instrument: words of its Place, its name or its type (e.g. \"pad\", \"tavern\", \"bells\").");
    search.setSelectAllWhenFocused(true);
    search.onTextChange = [this] { setSearch(search.getText()); };
    search.onEscapeKey = [this] {
        search.clear();
        setSearch({});
    };
    addAndMakeVisible(search);
    byPlace.setTooltip("List the instruments by Place (Castle, Village, Spaceport...)");
    byType.setTooltip("List the instruments by type (Plucks, Pads, Bass, Leads, Keys, Percussion, Bells & Mallets, Strings, Choirs & Voices, Textures, FX)");
    for (auto* b : { &byPlace, &byType }) {
        b->setColour(juce::TextButton::buttonOnColourId, ui::colours::brass);
        addAndMakeVisible(*b);
    }
    byPlace.onClick = [this] { setOrder(ByPlace); };
    byType.onClick = [this] { setOrder(ByType); };

    // The instrument.
    prev.setTooltip("The previous instrument (in the list as it is shown)");
    next.setTooltip("The next instrument (in the list as it is shown)");
    prev.onClick = [this] { step(-1); };
    next.onClick = [this] { step(1); };
    nameMenu.setTooltip("Every instrument, by Place and by type: choose one");
    nameMenu.onClick = [this] { showMenu(); };
    for (auto* b : { &prev, &next, &nameMenu }) addAndMakeVisible(*b);
    for (int c = 0; c < 3; ++c) {
        auto& b = character[std::size_t(c)];
        b.setColour(juce::TextButton::buttonOnColourId, ui::colours::brass);
        b.setTooltip("The instrument's Character: its Place's variations (they change its sound and playing, not just its level)");
        b.onClick = [this, c] {
            if (auto* parameter = processor.parameters.getParameter(specs[PInstCharacter].id)) {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(float(c)));
                parameter->endChangeGesture();
            }
            refresh();
        };
        addAndMakeVisible(b);
    }
    ambience.setClickingTogglesState(true);
    ambience.setColour(juce::TextButton::buttonOnColourId, ui::colours::brass);
    ambience.setTooltip("Place Ambience: the world's environment (wind, water, fire...), the World Space and the Place's own room under the "
                        "instrument. Off (the default): just the instrument and its own effects.");
    ambience.onClick = [this] {
        if (auto* parameter = processor.parameters.getParameter(specs[PInstAmbience].id)) {
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(ambience.getToggleState() ? 1.0f : 0.0f);
            parameter->endChangeGesture();
        }
    };
    addAndMakeVisible(ambience);
    const auto octave = [this](int by) {
        if (auto* parameter = processor.parameters.getParameter(specs[PInstOctave].id)) {
            const int now = juce::roundToInt(processor.parameters.getRawParameterValue(specs[PInstOctave].id)->load());
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(float(juce::jlimit(keys::octaveMin, keys::octaveMax, now + by))));
            parameter->endChangeGesture();
        }
        refresh();
    };
    octDown.setTooltip("Octave down (to -2)");
    octUp.setTooltip("Octave up (to +2)");
    octDown.onClick = [octave] { octave(-1); };
    octUp.onClick = [octave] { octave(1); };
    for (auto* b : { &octDown, &octUp }) addAndMakeVisible(*b);

    // Presets.
    presetPrev.setTooltip("The previous preset");
    presetNext.setTooltip("The next preset");
    presetMenu.setTooltip("Instrument presets: the factory set and yours (Documents/WORLDBUILDER/Instrument Presets). Separate from world presets.");
    presetSave.setTooltip("Save the instrument, its Character, effects and controls as a preset");
    presetLoad.setTooltip("Load an instrument preset file (.wbinst)");
    presetPrev.onClick = [this] { stepPreset(-1); };
    presetNext.onClick = [this] { stepPreset(1); };
    presetMenu.onClick = [this] { showPresetMenu(); };
    presetSave.onClick = [this] {
        const auto suggested = processor.instrumentPresetName().isNotEmpty() ? processor.instrumentPresetName() : processor.instrumentName();
        presetFolder().createDirectory();
        chooser = std::make_unique<juce::FileChooser>("Save the instrument preset", presetFolder().getChildFile(juce::File::createLegalFileName(suggested) + ".wbinst"), "*.wbinst");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                             [this](const juce::FileChooser& c) {
                                 const auto file = c.getResult();
                                 if (file != juce::File()) savePresetTo(file.withFileExtension("wbinst"), file.getFileNameWithoutExtension());
                             });
    };
    presetLoad.onClick = [this] {
        chooser = std::make_unique<juce::FileChooser>("Load an instrument preset", presetFolder(), "*.wbinst");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this](const juce::FileChooser& c) {
            const auto file = c.getResult();
            if (file.existsAsFile()) loadPresetFrom(file);
        });
    };
    ui::setIcon(presetSave, ui::Icon::Save);
    ui::setIcon(presetLoad, ui::Icon::Load);
    for (auto* b : { &presetPrev, &presetNext, &presetMenu, &presetSave, &presetLoad }) addAndMakeVisible(*b);
    {
        const auto& factory = inst::factoryPresets();
        for (int i = 0; i < int(factory.size()); ++i) {
            auto tile = std::make_unique<juce::TextButton>(juce::String(factory[std::size_t(i)].name).toUpperCase());
            tile->setColour(juce::TextButton::buttonOnColourId, ui::colours::brass);
            tile->setTooltip(factory[std::size_t(i)].name + ": " + fullName(factory[std::size_t(i)].id).toLowerCase() + " (a factory preset)");
            tile->onClick = [this, i] { loadPreset(i); };   // the factory presets come first in presetList()
            addAndMakeVisible(*tile);
            presetTiles.push_back(std::move(tile));
        }
    }

    // The sound: macros and effects, attached to the inst_* parameters.
    const auto knob = [this](juce::Slider& s, int param, const juce::String& tooltip) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters(juce::degreesToRadians(225.0f), juce::degreesToRadians(495.0f), true);
        s.setMouseDragSensitivity(220);
        s.setTooltip(tooltip);
        sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.parameters, specs[std::size_t(param)].id, s));
        s.setDoubleClickReturnValue(true, specs[std::size_t(param)].def);
        addAndMakeVisible(s);
    };
    const std::array<int, macroCount> macroParams { PInstTone, PInstAttack, PInstRelease, PInstDrive, PInstLevel };
    const std::array<const char*, macroCount> macroTips {
        "Tone: dark to bright",
        "Attack: how the notes start (the middle is as the instrument was designed; up: a slower swell; down: quicker). On plucked and struck "
        "instruments: the strike, softer or harder.",
        "Release: how the notes end (the middle as designed). On plucked and struck instruments: how long they ring.",
        "Drive: a warm saturation, more harmonics as it rises",
        "Level: the instrument's volume"
    };
    for (int m = 0; m < macroCount; ++m) knob(macros[std::size_t(m)], macroParams[std::size_t(m)], macroTips[std::size_t(m)]);
    knob(reverb, PInstReverb, "Reverb send: how much reaches the reverb chosen above it");
    knob(delay, PInstDelay, "Delay send: how much reaches the tempo delay chosen above it");
    knob(amount, PInstFxAmount, "The character effect's amount");
    knob(width, PInstWidth, "Stereo width: mono to extra wide");
    const auto combo = [this](juce::ComboBox& box, int param, const char* choices, const juce::String& tooltip) {
        box.addItemList(juce::StringArray::fromTokens(choices, "|", ""), 1);
        box.setTooltip(tooltip);
        comboAttachments.push_back(std::make_unique<ComboAttachment>(processor.parameters, specs[std::size_t(param)].id, box));
        addAndMakeVisible(box);
    };
    combo(reverbType, PInstReverbType, reverbChoices, "The reverb: Room, Hall, Cathedral, Plate or Cave");
    combo(delayType, PInstDelayType, delayChoices, "The delay (in time with the host): Tape, Ping-Pong, Dotted or Reverse");
    combo(characterFx, PInstFx, characterFxChoices, "The character effect: Saturation, Chorus, Phaser, Filter or Lo-Fi");

    rescanPresets();
    rebuildList();
    refresh();
    startTimerHz(20);
}

InstrumentView::~InstrumentView() { stopTimer(); }

juce::Component& InstrumentView::keyboardComponent() { return *keyboard; }
juce::Component& InstrumentView::listComponent() { return *list; }
bool InstrumentView::keyLit(int note) const { return keyboard->lit(note); }

juce::String InstrumentView::macroLabel(int which) const {
    const auto id = processor.instrument();
    const bool hit = struck(id);
    switch (which) {
        case 0: return "TONE";
        case 1: return hit ? "STRIKE" : "ATTACK";
        case 2: return hit ? "RING" : "RELEASE";
        case 3: return "DRIVE";
        default: return "LEVEL";
    }
}

juce::File InstrumentView::presetFolder() {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("WORLDBUILDER").getChildFile("Instrument Presets");
}

void InstrumentView::rescanPresets() {
    presets = inst::factoryPresets();
    const auto folder = presetFolder();
    if (!folder.isDirectory()) return;
    auto files = folder.findChildFiles(juce::File::findFiles, false, "*.wbinst");
    files.sort();
    for (const auto& f : files) {
        const auto xml = juce::parseXML(f);
        if (xml == nullptr || !xml->hasTagName("WorldbuilderInstrument")) continue;
        inst::Preset p;
        p.name = xml->getStringAttribute("name", f.getFileNameWithoutExtension());
        p.file = f;
        presets.push_back(p);
    }
}

void InstrumentView::loadPreset(int index) {
    if (index < 0 || index >= int(presets.size())) return;
    const auto& p = presets[std::size_t(index)];
    if (p.factory) processor.applyInstrumentPreset(p);
    else if (!loadPresetFrom(p.file)) return;
    refresh();
}

void InstrumentView::stepPreset(int by) {
    if (presets.empty()) return;
    int at = -1;
    for (int i = 0; i < int(presets.size()); ++i)
        if (presets[std::size_t(i)].name == processor.instrumentPresetName()) at = i;
    const int count = int(presets.size());
    loadPreset(at < 0 ? (by > 0 ? 0 : count - 1) : ((at + by) % count + count) % count);
}

bool InstrumentView::savePresetTo(const juce::File& file, const juce::String& name) {
    file.getParentDirectory().createDirectory();
    if (!file.replaceWithText(processor.exportInstrumentPreset(name))) return false;
    processor.setInstrumentPresetName(name);
    rescanPresets();
    refresh();
    return true;
}

bool InstrumentView::loadPresetFrom(const juce::File& file) {
    if (!file.existsAsFile() || !processor.importInstrumentPreset(file.loadFileAsString())) return false;
    refresh();
    return true;
}

void InstrumentView::showPresetMenu() {
    juce::PopupMenu factory, mine, menu;
    for (int i = 0; i < int(presets.size()); ++i) {
        const auto& p = presets[std::size_t(i)];
        const bool on = p.name == processor.instrumentPresetName();
        (p.factory ? factory : mine).addItem(i + 1, p.name.toUpperCase() + (p.factory ? "  (" + fullName(p.id) + ")" : juce::String()), true, on);
    }
    menu.addSectionHeader("FACTORY");
    for (juce::PopupMenu::MenuItemIterator it(factory); it.next();) menu.addItem(it.getItem());
    menu.addSectionHeader("YOURS");
    if (mine.getNumItems() == 0) menu.addItem(-1, "NONE YET: SAVE ONE", false);
    else
        for (juce::PopupMenu::MenuItemIterator it(mine); it.next();) menu.addItem(it.getItem());
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetMenu), [this](int r) {
        if (r > 0) loadPreset(r - 1);
    });
}

void InstrumentView::showMenu() {
    // Every instrument: by Place, then by type, as submenus.
    juce::PopupMenu places, types, menu;
    const auto& catalog = inst::Catalog::get();
    const auto now = processor.instrument();
    for (int t = NoPlace + 1; t < PlaceTypeCount; ++t) {
        juce::PopupMenu sub;
        bool here = false;
        for (int i = 0; i < catalog.count; ++i) {
            const auto id = catalog.entry[std::size_t(i)];
            if (id.type != t) continue;
            sub.addItem(i + 1, elementName(id), true, id == now);
            here |= id == now;
        }
        places.addSubMenu(placeName(t), sub, true, nullptr, here);
    }
    for (int g = 0; g < inst::GroupCount; ++g) {
        juce::PopupMenu sub;
        bool here = false;
        for (int i = 0; i < catalog.count; ++i) {
            const auto id = catalog.entry[std::size_t(i)];
            if (inst::groupOf(id.type, id.element) != g) continue;
            sub.addItem(1000 + i + 1, fullName(id), true, id == now);
            here |= id == now;
        }
        types.addSubMenu(upper(inst::groupNames[std::size_t(g)]), sub, true, nullptr, here);
    }
    menu.addSubMenu("BY PLACE", places);
    menu.addSubMenu("BY TYPE", types);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&nameMenu), [this](int r) {
        if (r <= 0) return;
        const int i = (r > 1000 ? r - 1000 : r) - 1;
        const auto& c = inst::Catalog::get();
        if (i >= 0 && i < c.count) choose(c.entry[std::size_t(i)]);
    });
}

void InstrumentView::setOrder(Order o) {
    listOrder = o;
    rebuildList();
    refresh();
}

void InstrumentView::setSearch(const juce::String& text) {
    searchText = text.trim();
    if (search.getText() != text) search.setText(text, juce::dontSendNotification);
    rebuildList();
    list->reveal(processor.instrument());
}

void InstrumentView::rebuildList() {
    const auto& catalog = inst::Catalog::get();
    const auto words = juce::StringArray::fromTokens(searchText.toLowerCase(), " ,", "");
    const auto matches = [&](inst::Id id) {
        if (words.isEmpty()) return true;
        const auto hay = (juce::String(placeNames[std::size_t(id.type)]) + " " + placeInfo(id.type).element[std::size_t(id.element)].name + " "
                          + inst::groupNames[std::size_t(inst::groupOf(id.type, id.element))] + " " + placeKeys[std::size_t(id.type)])
                             .toLowerCase();
        for (const auto& w : words)
            if (w.isNotEmpty() && !hay.contains(w)) return false;
        return true;
    };
    listed.clear();
    list->rows.clear();
    if (listOrder == ByPlace) {
        for (int t = NoPlace + 1; t < PlaceTypeCount; ++t) {
            bool heading = false;
            for (int i = 0; i < catalog.count; ++i) {
                const auto id = catalog.entry[std::size_t(i)];
                if (id.type != t || !matches(id)) continue;
                if (!heading) list->rows.push_back({ true, placeName(t), {}, t });
                heading = true;
                list->rows.push_back({ false, elementName(id), id, 0 });
                listed.push_back(id);
            }
        }
    } else {
        for (int g = 0; g < inst::GroupCount; ++g) {
            bool heading = false;
            for (int i = 0; i < catalog.count; ++i) {
                const auto id = catalog.entry[std::size_t(i)];
                if (inst::groupOf(id.type, id.element) != g || !matches(id)) continue;
                if (!heading) list->rows.push_back({ true, upper(inst::groupNames[std::size_t(g)]), {}, 0 });
                heading = true;
                list->rows.push_back({ false, fullName(id), id, 0 });
                listed.push_back(id);
            }
        }
    }
    list->clampScroll();
    list->repaint();
}

void InstrumentView::choose(inst::Id id) {
    if (!id.valid()) return;
    processor.setInstrument(id, true);
    refresh();
    list->reveal(id);
}

void InstrumentView::step(int by) {
    // Through the list as shown; a search that hides the instrument playing starts from
    // the list's first (or last).
    const auto now = processor.instrument();
    if (listed.empty()) {
        choose(inst::Catalog::get().step(now, by));
        return;
    }
    const auto it = std::find(listed.begin(), listed.end(), now);
    const int count = int(listed.size());
    const int at = it == listed.end() ? (by > 0 ? -1 : count) : int(it - listed.begin());
    choose(listed[std::size_t(((at + by) % count + count) % count)]);
}

void InstrumentView::refresh() {
    const auto id = processor.instrument();
    const int ch = juce::roundToInt(processor.parameters.getRawParameterValue(specs[PInstCharacter].id)->load());
    const int oct = juce::roundToInt(processor.parameters.getRawParameterValue(specs[PInstOctave].id)->load());
    const bool amb = processor.parameters.getRawParameterValue(specs[PInstAmbience].id)->load() > 0.5f;
    const auto preset = processor.instrumentPresetName();
    byPlace.setToggleState(listOrder == ByPlace, juce::dontSendNotification);
    byType.setToggleState(listOrder == ByType, juce::dontSendNotification);
    ambience.setToggleState(amb, juce::dontSendNotification);
    for (int c = 0; c < 3; ++c) {
        auto& b = character[std::size_t(c)];
        b.setButtonText(upper(placeInfo(id.type).characters[std::size_t(c)]));
        b.setToggleState(c == ch, juce::dontSendNotification);
    }
    nameMenu.setButtonText(fullName(id));
    presetMenu.setButtonText(preset.isNotEmpty() ? preset.toUpperCase() : juce::String("PRESETS..."));
    for (int i = 0; i < int(presetTiles.size()); ++i)
        presetTiles[std::size_t(i)]->setToggleState(preset.isNotEmpty() && presets[std::size_t(i)].name == preset, juce::dontSendNotification);
    for (int m = 0; m < macroCount; ++m) macros[std::size_t(m)].setName(macroLabel(m));
    if (id != shownId) list->reveal(id);
    if (id != shownId || ch != shownCharacter || oct != shownOctave || amb != shownAmbience || preset != shownPreset) {
        shownId = id;
        shownCharacter = ch;
        shownOctave = oct;
        shownAmbience = amb;
        shownPreset = preset;
        list->repaint();
        for (auto& m : macros) m.repaint();
        repaint();
    }
}

void InstrumentView::timerCallback() {
    if (!isShowing()) return;
    refresh();
    keyboard->repaint();
    repaint(wheelArea);
}

void InstrumentView::resized() {
    auto area = getLocalBounds();
    browserPanel = area.removeFromLeft(300);
    area.removeFromLeft(12);
    instrumentPanel = area.removeFromTop(222);
    area.removeFromTop(10);
    const auto row = area.removeFromTop(186);
    area.removeFromTop(10);
    presetsPanel = area.removeFromTop(124);
    area.removeFromTop(10);
    keysPanel = area;
    area = row;
    soundPanel = area.removeFromLeft(area.getWidth() * 5 / 11);
    area.removeFromLeft(12);
    effectsPanel = area;

    {
        auto b = browserPanel.reduced(12, 10);
        b.removeFromTop(22);   // title
        auto chips = b.removeFromTop(24);
        byPlace.setBounds(chips.removeFromLeft(chips.getWidth() / 2).reduced(2, 1));
        byType.setBounds(chips.reduced(2, 1));
        b.removeFromTop(6);
        search.setBounds(b.removeFromTop(24));
        b.removeFromTop(6);
        list->setBounds(b);
    }
    {
        auto r = instrumentPanel.reduced(14, 10);
        auto top = r.removeFromTop(30);
        prev.setBounds(top.removeFromLeft(30).reduced(0, 2));
        top.removeFromLeft(4);
        next.setBounds(top.removeFromRight(30).reduced(0, 2));
        top.removeFromRight(4);
        nameMenu.setBounds(top.reduced(0, 2));
        r.removeFromTop(8);
        auto mid = r.removeFromTop(76);
        iconArea = mid.removeFromLeft(76).withSizeKeepingCentre(72, 72);
        mid.removeFromLeft(12);
        nameArea = mid.removeFromTop(30);
        subArea = mid.removeFromTop(16);
        mid.removeFromTop(6);
        auto chars = mid.removeFromTop(22);
        characterLabel = chars.removeFromLeft(86);
        for (auto& c : character) {
            c.setBounds(chars.removeFromLeft(122).reduced(2, 0));
        }
        r.removeFromTop(4);
        descriptionArea = r.removeFromTop(28);
        r.removeFromTop(6);
        auto bottom = r.removeFromTop(26);
        ambience.setBounds(bottom.removeFromLeft(150).reduced(0, 1));
        bottom.removeFromLeft(16);
        octaveArea = bottom.removeFromLeft(84);
        octDown.setBounds(bottom.removeFromLeft(26).reduced(0, 1));
        bottom.removeFromLeft(2);
        octUp.setBounds(bottom.removeFromLeft(26).reduced(0, 1));
        bottom.removeFromLeft(18);
        presetLabel = bottom.removeFromLeft(62);
        presetLoad.setBounds(bottom.removeFromRight(70).reduced(0, 1));
        bottom.removeFromRight(4);
        presetSave.setBounds(bottom.removeFromRight(70).reduced(0, 1));
        bottom.removeFromRight(8);
        presetPrev.setBounds(bottom.removeFromLeft(24).reduced(0, 1));
        presetNext.setBounds(bottom.removeFromRight(24).reduced(0, 1));
        presetMenu.setBounds(bottom.reduced(3, 1));
    }
    {
        auto r = soundPanel.reduced(12, 10);
        r.removeFromTop(30);
        auto knobs = r.removeFromTop(124);
        const int w = knobs.getWidth() / macroCount;
        for (auto& m : macros) m.setBounds(knobs.removeFromLeft(w).reduced(2, 0));
    }
    {
        auto r = effectsPanel.reduced(12, 10);
        r.removeFromTop(24);
        auto labels = r.removeFromTop(14);
        juce::ignoreUnused(labels);
        auto combos = r.removeFromTop(22);
        const int w = combos.getWidth() / 4;
        reverbType.setBounds(combos.removeFromLeft(w).reduced(3, 0));
        delayType.setBounds(combos.removeFromLeft(w).reduced(3, 0));
        characterFx.setBounds(combos.removeFromLeft(w).reduced(3, 0));
        r.removeFromTop(8);
        auto knobs = r.removeFromTop(104);
        reverb.setBounds(knobs.removeFromLeft(w).reduced(2, 0));
        delay.setBounds(knobs.removeFromLeft(w).reduced(2, 0));
        amount.setBounds(knobs.removeFromLeft(w).reduced(2, 0));
        width.setBounds(knobs.reduced(2, 0));
    }
    {
        auto r = presetsPanel.reduced(12, 10);
        r.removeFromTop(24);
        const int columns = 7, rows = std::max(1, (int(presetTiles.size()) + columns - 1) / columns);
        const int w = r.getWidth() / columns, h = std::min(22, r.getHeight() / rows);
        for (int i = 0; i < int(presetTiles.size()); ++i)
            presetTiles[std::size_t(i)]->setBounds(juce::Rectangle<int>(r.getX() + (i % columns) * w, r.getY() + (i / columns) * h, w, h).reduced(2, 1));
    }
    {
        auto r = keysPanel.reduced(12, 10);
        r.removeFromTop(22);
        hintArea = r.removeFromBottom(16);
        wheelArea = r.removeFromLeft(72);
        r.removeFromLeft(8);
        keyboard->setBounds(r.withTrimmedBottom(4));
    }
    pixel::snapChildren(*this);
    list->reveal(processor.instrument());
}

void InstrumentView::paint(juce::Graphics& g) {
    const auto& pal = pixel::theme().colours;
    for (auto panel : { browserPanel, instrumentPanel, soundPanel, effectsPanel, presetsPanel, keysPanel }) {
        if (!g.clipRegionIntersects(panel.expanded(3))) continue;
        const int w = panel.getWidth() / pixel::unit, h = panel.getHeight() / pixel::unit;
        pixel::drawCached(g, "panel/" + juce::String(w) + "x" + juce::String(h) + "/inst", panel.getPosition(), [&] { return pixel::panel(pal, w, h); });
    }
    const auto title = [&](juce::Rectangle<int> panel, const juce::String& text) {
        pixel::drawText(g, text, panel.reduced(20, 8).withHeight(16), pal.heading, juce::Justification::centredLeft, pixel::Font::Large);
    };
    title(browserPanel, "INSTRUMENTS");
    title(soundPanel, "SOUND");
    title(effectsPanel, "EFFECTS");
    title(keysPanel, "KEYBOARD");
    title(presetsPanel, "FACTORY PRESETS");
    pixel::drawText(g, juce::String(int(listed.size())) + " OF " + juce::String(inst::Catalog::get().count),
                    browserPanel.reduced(20, 8).withHeight(16), pal.inkDim, juce::Justification::centredRight, pixel::Font::Small);

    // The instrument: its Place's icon, its name, its Place and type.
    const auto id = processor.instrument();
    if (g.clipRegionIntersects(iconArea.expanded(4))) {
        g.setColour(pal.well.withAlpha(0.7f));
        g.fillRect(iconArea);
        pixel::drawPlaceIcon(g, id.type, { iconArea.getX() + (iconArea.getWidth() - pixel::iconSize * pixel::unit) / 2,
                                           iconArea.getY() + (iconArea.getHeight() - pixel::iconSize * pixel::unit) / 2 },
                             false, pixel::iconFrame(id.type, true));
    }
    pixel::drawText(g, elementName(id), nameArea, pal.ink, juce::Justification::centredLeft, pixel::Font::Large);
    const auto kind = patternInfo(id.type, id.element).kind == Kit ? juce::String(" - A KIT: EACH KEY ITS OWN DRUM") : juce::String();
    pixel::drawText(g, placeName(id.type) + " - " + upper(inst::groupNames[std::size_t(inst::groupOf(id.type, id.element))]) + kind, subArea, pal.heading,
                    juce::Justification::centredLeft, pixel::Font::Small);
    pixel::drawText(g, "CHARACTER", characterLabel, pal.heading, juce::Justification::centredLeft, pixel::Font::Small);
    {
        // The Place's description, as its card says it (two lines at most).
        const auto text = juce::String(placeInfo(id.type).description).toUpperCase();
        const int perLine = std::max(10, descriptionArea.getWidth() / (pixel::textWidthLogical("ABCDEFGHIJ", pixel::Font::Small) / 10 + 1));
        juce::String first = text, second;
        if (text.length() > perLine) {
            int cut = text.substring(0, perLine).lastIndexOfChar(' ');
            if (cut <= 0) cut = perLine;
            first = text.substring(0, cut);
            second = text.substring(cut).trim();
        }
        pixel::drawText(g, first, descriptionArea.withHeight(14), pal.inkDim, juce::Justification::centredLeft, pixel::Font::Small);
        pixel::drawText(g, second, descriptionArea.withTrimmedTop(14), pal.inkDim, juce::Justification::centredLeft, pixel::Font::Small);
    }
    const int oct = juce::roundToInt(processor.parameters.getRawParameterValue(specs[PInstOctave].id)->load());
    pixel::drawText(g, "OCTAVE " + juce::String(oct > 0 ? "+" : "") + juce::String(oct), octaveArea, pal.heading, juce::Justification::centredLeft, pixel::Font::Small);
    pixel::drawText(g, "PRESET", presetLabel, pal.heading, juce::Justification::centredLeft, pixel::Font::Small);

    // The effects' labels.
    const auto above = [&](juce::Component& c, const juce::String& text) {
        pixel::drawText(g, text, c.getBounds().translated(0, -15).withHeight(13), pal.heading, juce::Justification::centredLeft, pixel::Font::Small);
    };
    above(reverbType, "REVERB");
    above(delayType, "DELAY");
    above(characterFx, "CHARACTER FX");

    // The wheels and the pedal, as they arrive.
    {
        const auto& in = processor.engine.keysInput;
        auto r = wheelArea;
        const float bend = in.bendOut.load(std::memory_order_relaxed), mod = in.modOut.load(std::memory_order_relaxed);
        const bool pedal = in.pedalOut.load(std::memory_order_relaxed);
        auto pedalArea = r.removeFromBottom(26);
        auto wheels = r.withTrimmedBottom(16);
        auto bendBar = wheels.removeFromLeft(wheels.getWidth() / 2).reduced(9, 0), modBar = wheels.reduced(9, 0);
        for (auto bar : { bendBar, modBar }) {
            g.setColour(pal.outline);
            g.fillRect(bar.expanded(1));
            g.setColour(pal.well);
            g.fillRect(bar);
        }
        const int mid = bendBar.getCentreY();
        const int bendY = mid - juce::roundToInt(bend * float(bendBar.getHeight() / 2 - 2));
        g.setColour(pal.accent);
        g.fillRect(bendBar.getX(), std::min(mid, bendY), bendBar.getWidth(), std::max(2, std::abs(bendY - mid)));
        const int modH = juce::roundToInt(mod * float(modBar.getHeight()));
        g.fillRect(modBar.withTop(modBar.getBottom() - modH));
        pixel::drawTinyText(g, "BEND", bendBar.withY(bendBar.getBottom() + 3).withHeight(8).expanded(8, 0), pal.heading, juce::Justification::centred);
        pixel::drawTinyText(g, "MOD", modBar.withY(modBar.getBottom() + 3).withHeight(8).expanded(8, 0), pal.heading, juce::Justification::centred);
        g.setColour(pal.outline);
        g.fillRect(pedalArea.withTrimmedTop(16).withSizeKeepingCentre(42, 8).expanded(1));
        g.setColour(pedal ? pal.accent : pal.accentDim);
        g.fillRect(pedalArea.withTrimmedTop(16).withSizeKeepingCentre(42, 8));
        pixel::drawTinyText(g, "PEDAL", pedalArea.withTrimmedTop(4).withHeight(10), pal.heading, juce::Justification::centred);
    }
    // What the wheels do on this instrument.
    {
        const float range = WorldEngine::bendRange(id.type, id.element);
        const juce::String bendText = range <= 0.0f ? "NO PITCH BEND (A KIT)" : "PITCH BEND +-" + juce::String(juce::roundToInt(range)) + " SEMITONES";
        pixel::drawText(g, bendText + " - MOD WHEEL: VIBRATO OR BRIGHTNESS - SUSTAIN PEDAL - VELOCITY: LEVEL AND TONE", hintArea, pal.inkDim,
                        juce::Justification::centredLeft, pixel::Font::Small);
    }
}

} // namespace wb::ui
