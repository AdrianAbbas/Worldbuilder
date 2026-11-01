#pragma once
// The editor's own repaint requests, recorded for the tests (the hotfix after Instrument
// mode): which regions each animation frame asks to have painted, in the editor's
// coordinates. The tests paint exactly those regions, clipped, to measure what a frame
// costs (a headless test has no window to repaint). Off (a flag check) in the plug-in.
#include <juce_gui_basics/juce_gui_basics.h>

namespace wb::ui {

struct RepaintLog {
    static RepaintLog& get() {
        static RepaintLog log;
        return log;
    }
    bool enabled = false;
    juce::RectangleList<int> areas;
};

// repaint(area) on `c`, recorded (in its top-level component's coordinates) when the log is on.
inline void repaintArea(juce::Component& c, juce::Rectangle<int> area) {
    auto& log = RepaintLog::get();
    if (log.enabled) {
        auto* top = c.getTopLevelComponent();
        log.areas.add(top != nullptr && top != &c ? top->getLocalArea(&c, area) : area);
    }
    c.repaint(area);
}
inline void repaintAll(juce::Component& c) { repaintArea(c, c.getLocalBounds()); }

} // namespace wb::ui
