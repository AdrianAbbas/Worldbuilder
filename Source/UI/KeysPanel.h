#pragma once
// KEYS (Milestone 14), in the Pattern Editor: the Play target (a Place slot and one of
// its instruments) that the host's MIDI keyboard plays, its scale mode, octave, Monitor
// and "host regions" switch, an on-screen keyboard that lights up the notes arriving,
// and REC: record mode, variant, quantise and strength, count-in, length, quantise
// afterwards and undo the last take.
#include "../PluginProcessor.h"

namespace wb::ui {

class KeysPanel final : public juce::Component, private juce::Timer {
public:
    explicit KeysPanel(WorldbuilderProcessor& processor);
    ~KeysPanel() override;

    // The element the Pattern Editor shows (TARGET THIS, QUANTISE and the variant act on it).
    void setContext(int slot, int element, int variant);
    // A take or an undo changed the patterns: the Pattern Editor reads them again.
    std::function<void()> onPatternsChanged;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Also used by the tests.
    juce::ComboBox& targetChoice() { return target; }
    juce::TextButton& recButton() { return rec; }
    juce::String statusText() const { return status; }
    void refresh();
    // The target choice's id for a slot and element (1: no target).
    static int targetId(int slot, int element) { return slot < 0 ? 1 : 2 + slot * placeElements + element; }

private:
    class Keyboard;
    void timerCallback() override;
    void rebuildTargets();
    void setTarget(int slot, int element);

    WorldbuilderProcessor& processor;
    int slotShown = -1, elementShown = 0, variantShown = 0;
    std::array<int, placeSlots> typesShown {};
    uint32_t recorderSeen = 0;
    juce::ComboBox target, scale, mode, variant, quantise, length;
    juce::TextButton thisButton { "THIS" }, octDown { "-" }, octUp { "+" }, monitor { "MONITOR" }, regions { "HOST MIDI" };
    juce::TextButton rec { "REC" }, countIn { "COUNT-IN" }, quantiseNow { "QUANTISE" }, undoTake { "UNDO TAKE" };
    juce::Slider strength;
    std::unique_ptr<Keyboard> keyboard;
    juce::Rectangle<int> playRow, recRow, octaveArea, statusArea;
    juce::String status;
    double tipUntil = 0;
    bool updating = false;
};

} // namespace wb::ui
