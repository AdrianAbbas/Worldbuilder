#pragma once
// Keeps the editor's animation from ever piling up on the message thread.
//
// The editor's 30 Hz timer asks beginFrame() whether to draw a frame; its paint reports
// what the frame cost through painted(). The rules:
// - one frame at a time: while a requested frame has not been painted yet, the next
//   ones are skipped (the host paints when it can; the timer never stacks up requests);
// - late frames are skipped: a tick that arrives long after the last one (the message
//   thread was busy) moves the animation on without drawing;
// - the work per second is capped: when frames cost more than the budget, frames are
//   spaced out so the interface takes at most about a third of the message thread;
// - when frames stay slow for two seconds with the overlays on Full, wantsSubtle()
//   asks the editor to drop them to Subtle (once; the composer can turn Full back on).
// The audio thread never waits on any of this: the editor only reads atomics.
#include <algorithm>
#include <cstdint>

namespace wb::ui {

class FrameGovernor {
public:
    static constexpr double tickMs = 1000.0 / 30.0;
    static constexpr double budgetMs = 11.0;        // paint time a frame may take at 30 fps
    static constexpr double slowMs = 16.0;          // a frame this costly (smoothed) is slow
    static constexpr double pendingTimeoutMs = 400; // a frame the host never painted is dropped

    // The timer ticked at `nowMs`: true to draw a frame now. `elapsed` is the time the
    // animation should move on (every tick's time, drawn or not).
    bool beginFrame(double nowMs, double& elapsedSeconds) {
        const double since = last < 0 ? tickMs : nowMs - last;
        last = nowMs;
        elapsedSeconds = std::clamp(since, 0.0, 250.0) / 1000.0;
        ++ticks;
        if (pending && nowMs - pendingSince < pendingTimeoutMs) {
            ++skippedBusy;
            return false;
        }
        pending = false;
        if (since > 2.5 * tickMs && ticks > 1) {   // late: the message thread was busy
            ++skippedLate;
            return false;
        }
        if (++sinceDrawn < interval) {
            ++skippedBudget;
            return false;
        }
        sinceDrawn = 0;
        pending = true;
        pendingSince = nowMs;
        ++drawn;
        return true;
    }
    // A frame was painted in `costMs` (its whole paint, all components).
    void painted(double costMs, double nowMs) {
        pending = false;
        cost = samples == 0 ? costMs : cost + 0.2 * (costMs - cost);
        ++samples;
        peak = std::max(peak, costMs);
        // Space frames so paint time stays under budget (at most every fourth tick).
        interval = std::clamp(int(cost / budgetMs) + 1, 1, 4);
        if (cost > slowMs) {
            if (slowSince < 0) slowSince = nowMs;
        } else {
            slowSince = -1;
        }
    }
    // True once frames have been slow for two seconds (the editor drops to Subtle).
    bool slowFor(double nowMs, double ms = 2000.0) const { return slowSince >= 0 && nowMs - slowSince >= ms; }
    void resetSlow() { slowSince = -1; }

    double averageCost() const { return cost; }
    double peakCost() const { return peak; }
    int frameInterval() const { return interval; }
    int64_t ticks = 0, drawn = 0, skippedBusy = 0, skippedLate = 0, skippedBudget = 0;

private:
    double last = -1, pendingSince = 0, cost = 0, peak = 0, slowSince = -1;
    int64_t samples = 0;
    int interval = 1, sinceDrawn = 0;
    bool pending = false;
};

} // namespace wb::ui
