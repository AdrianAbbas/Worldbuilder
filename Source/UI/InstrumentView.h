#pragma once
// Instrument mode's screen: one instrument, played from the host's MIDI like a normal
// software instrument. In place of the world's panels (the top bar stays):
//
//  - the browser: every instrument, by Place ("TAVERN > LUTE STRUM") or by type (Plucks,
//    Pads, Bass...), searchable, with previous / next to step through sounds while playing;
//  - the instrument: its name and Place icon, its Character, Place Ambience and Octave,
//    and its presets (separate from world presets);
//  - its sound: a few macros (Tone, Attack or Strike, Release or Ring, Drive, Level) and
//    its effects (reverb and delay sends with their types, the character effect, width);
//  - an on-screen keyboard lit by the notes arriving (it plays too, for a quick try).
//
// Everything here is the processor's Instrument mode state and inst_* parameters; the
// view only shows and sets them.
#include "../Instruments/Presets.h"
#include "../PluginProcessor.h"

namespace wb::ui {

class InstrumentView final : public juce::Component, private juce::Timer {
public:
    explicit InstrumentView(WorldbuilderProcessor& processor);
    ~InstrumentView() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    // Reads the processor again (a project, a preset or the host changed it).
    void refresh();

    // The browser's order: by Place or by type.
    enum Order { ByPlace, ByType };
    void setOrder(Order o);
    Order order() const { return listOrder; }
    // The search text (case-insensitive words, all of which must match the Place, the
    // instrument or its type).
    void setSearch(const juce::String& text);
    // The instruments listed now, in order, and choosing one (as a click does).
    int listedCount() const { return int(listed.size()); }
    inst::Id listedAt(int i) const { return listed[std::size_t(i)]; }
    void choose(inst::Id id);
    // Previous / next: through the list as it is shown (all of them without a search).
    void step(int by);
    // The macro labels the instrument shows ("ATTACK" or "STRIKE"...).
    juce::String macroLabel(int which) const;

    // Presets (factory and the composer's), as the preset row does.
    void loadPreset(int index);   // index into presetList()
    const std::vector<inst::Preset>& presetList() const { return presets; }
    void stepPreset(int by);
    bool savePresetTo(const juce::File& file, const juce::String& name);
    bool loadPresetFrom(const juce::File& file);
    static juce::File presetFolder();
    void rescanPresets();

    // For the tests.
    juce::TextEditor& searchBox() { return search; }
    juce::TextButton& previousButton() { return prev; }
    juce::TextButton& nextButton() { return next; }
    juce::TextButton& byPlaceButton() { return byPlace; }
    juce::TextButton& byTypeButton() { return byType; }
    juce::TextButton& ambienceButton() { return ambience; }
    juce::TextButton& characterButton(int c) { return character[std::size_t(std::clamp(c, 0, 2))]; }
    juce::ComboBox& droneBox() { return droneChoice; }
    juce::Slider& macro(int which) { return macros[std::size_t(std::clamp(which, 0, macroCount - 1))]; }
    juce::Component& keyboardComponent();
    juce::Component& listComponent();
    juce::TextButton& presetButton() { return presetMenu; }
    juce::TextButton& presetTile(int i) { return *presetTiles[std::size_t(i)]; }
    int presetTileCount() const { return int(presetTiles.size()); }
    // The keys lit now on the on-screen keyboard (for the tests: reads the processor).
    bool keyLit(int note) const;

    static constexpr int macroCount = 5;   // Tone, Attack, Release, Drive, Level

private:
    class List;
    class Keyboard;
    void timerCallback() override;
    void rebuildList();
    void showMenu();
    void showPresetMenu();

    WorldbuilderProcessor& processor;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    // Browser.
    Order listOrder = ByPlace;
    juce::String searchText;
    std::vector<inst::Id> listed;
    juce::TextEditor search;
    juce::TextButton byPlace { "BY PLACE" }, byType { "BY TYPE" };
    std::unique_ptr<List> list;
    // The instrument.
    juce::TextButton prev { "<" }, next { ">" }, nameMenu;
    std::array<juce::TextButton, 3> character;
    juce::TextButton ambience { "PLACE AMBIENCE" }, octDown { "-" }, octUp { "+" };
    // Presets.
    std::vector<inst::Preset> presets;
    juce::TextButton presetPrev { "<" }, presetNext { ">" }, presetMenu, presetSave { "SAVE" }, presetLoad { "LOAD" };
    // The factory set as tiles, one click each.
    std::vector<std::unique_ptr<juce::TextButton>> presetTiles;
    std::unique_ptr<juce::FileChooser> chooser;
    // Sound and effects.
    std::array<juce::Slider, macroCount> macros;
    juce::Slider reverb { "REVERB" }, delay { "DELAY" }, amount { "AMOUNT" }, width { "WIDTH" };
    juce::ComboBox reverbType, delayType, characterFx, droneChoice;
    juce::Rectangle<int> droneLabel;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ComboAttachment>> comboAttachments;
    std::unique_ptr<Keyboard> keyboard;

    juce::Rectangle<int> browserPanel, instrumentPanel, soundPanel, effectsPanel, presetsPanel, keysPanel;
    juce::Rectangle<int> iconArea, descriptionArea, nameArea, subArea, characterLabel, octaveArea, presetLabel, hintArea, wheelArea;
    inst::Id shownId {};
    int shownCharacter = -1, shownOctave = 99;
    bool shownAmbience = false;
    juce::String shownPreset;
};

} // namespace wb::ui
