#pragma once
// KEYS (Milestone 14), in the Pattern Editor. The instrument tab shown is the Play
// target: the host's MIDI keyboard plays it and REC records into it (the hotfix after
// Milestone 14 removed the separate target list). Its scale mode, octave, Monitor and
// "host regions" switch, an on-screen keyboard that lights up the notes arriving, and
// REC: record mode, variant, quantise and strength, count-in, punch, length, quantise
// afterwards and undo the last take.
#include "../PluginProcessor.h"

namespace wb::ui {

class KeysPanel final : public juce::Component, private juce::Timer {
public:
    explicit KeysPanel(WorldbuilderProcessor& processor);
    ~KeysPanel() override;

    // The element the Pattern Editor shows: it becomes the Play target (QUANTISE and the
    // variant act on it too).
    void setContext(int slot, int element, int variant);
    // A take or an undo changed the patterns: the Pattern Editor reads them again.
    std::function<void()> onPatternsChanged;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Also used by the tests.
    juce::TextButton& recButton() { return rec; }
    juce::TextButton& punchButton() { return punch; }
    juce::String statusText() const { return status; }
    // What the keyboard plays, as the row says it ("PLAYS BELLS").
    juce::String targetText() const;
    void refresh();

private:
    class Keyboard;
    void timerCallback() override;
    void setTarget(int slot, int element);

    WorldbuilderProcessor& processor;
    int slotShown = -1, elementShown = 0, variantShown = 0;
    std::array<int, placeSlots> typesShown {};
    uint32_t recorderSeen = 0;
    juce::ComboBox scale, mode, variant, quantise, length;
    juce::TextButton octDown { "-" }, octUp { "+" }, monitor { "MONITOR" }, regions { "HOST MIDI" };
    juce::TextButton rec { "REC" }, countIn { "COUNT-IN" }, punch { "PUNCH" }, quantiseNow { "QUANTISE" }, undoTake { "UNDO TAKE" };
    juce::Slider strength;
    std::unique_ptr<Keyboard> keyboard;
    juce::Rectangle<int> playRow, recRow, targetArea, octaveArea, statusArea;
    juce::String status;
    double tipUntil = 0;
    bool updating = false;
};

} // namespace wb::ui
