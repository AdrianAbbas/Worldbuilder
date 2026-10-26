#pragma once
// The footer's CAPTURE section (Milestone 8): get the world into the song.
// - BARS: how long a render or a MIDI export is (4-64 bars of 4/4 at the host tempo);
// - MIX / STEMS: one WAV of the full mix, or one per Place plus the environment;
// - MIDI: the patterns of every active instrument as a multi-track MIDI file. Drag it
//   into the host, or click to save it;
// - RENDER: renders the bars offline, faster than real time, on a background thread
//   (click again to cancel). The plugin keeps playing meanwhile;
// - WAV: the last render. Drag it into the host, or click to save it.
// Milestone 11c: BARS also offers ARRANGEMENT (the whole arrangement, as the host plays
// it in Arrangement mode), and the mode menu SECTIONS (each section as its own file for a
// game engine, loop sections seamless: Tavern_Intro.wav, Tavern_Loop.wav...) and
// STINGERS (each section's ending and transition as a one-shot). MIDI then follows the
// arrangement.
#include "../PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace wb::ui {

class CapturePanel final : public juce::Component, private juce::Timer {
public:
    explicit CapturePanel(WorldbuilderProcessor&);
    ~CapturePanel() override;
    void paint(juce::Graphics&) override;
    void resized() override;

    int bars() const;
    bool stems() const;
    void setBars(int bars);
    void setStems(bool on);
    // Milestone 11c.
    enum Mode { MixMode = 1, StemsMode, SectionsMode, StingersMode };
    static constexpr int arrangementId = 1;   // BARS: the arrangement
    void setArrangement(bool on);
    void setMode(int m);
    int currentMode() const;
    // Whether renders and MIDI follow the arrangement now.
    bool followsArrangement() const;
    // Starts a render of the current world (cancels one that is running).
    void startRender();
    void cancelRender();
    bool rendering() const { return worker != nullptr; }
    // For the tests: waits for a running render to finish (the timer's work done here).
    bool waitForRender(int timeoutMs);
    // The files of the last finished render.
    const juce::Array<juce::File>& renderedFiles() const { return rendered; }
    // Writes the scene's patterns as MIDI (false when nothing is playing a pattern).
    bool writeSceneMidi(const juce::File& file);
    juce::File tempMidiFile() const;
    // Where renders and drag-out files are kept until replaced.
    static juce::File workFolder();

private:
    class Chip;
    class Worker;
    void timerCallback() override;
    void finishRender();
    void saveMidi();
    void saveRender();

    WorldbuilderProcessor& processor;
    juce::ComboBox barChoice, mode;
    std::unique_ptr<Chip> midi, render, wav;
    std::unique_ptr<Worker> worker;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File> rendered;
    juce::String status;
    juce::Rectangle<int> label;
};

} // namespace wb::ui
