#include "Travel.h"

namespace wb {

namespace {
// Empty slot patterns, copied from one static instance (a temporary would put tens of
// kilobytes on the audio thread's stack).
const pat::SlotPatterns& emptyPatterns() {
    static const pat::SlotPatterns empty {};
    return empty;
}
} // namespace


namespace {
// The parameters that are not part of a Location (always the host's).
const std::array<bool, PCount>& liveMask() {
    static const auto mask = [] {
        std::array<bool, PCount> m {};
        for (int p = 0; p < PCount; ++p) m[std::size_t(p)] = moveOf(p) == Move::Live;
        return m;
    }();
    return mask;
}
} // namespace

void Travel::prepare(double sampleRate) {
    rate = std::isfinite(sampleRate) && sampleRate > 0 ? sampleRate : 48000.0;
    (void) liveMask();
    // A journey under way when the host re-prepares finishes at once.
    if (mode == Travelling) elapsed = duration;
}

void Travel::keepLive(const WorldValues& live, Sounding& s) {
    const auto& mask = liveMask();
    for (std::size_t p = 0; p < mask.size(); ++p)
        if (mask[p]) s.v[p] = live[p];
}

void Travel::locationState(int location, const WorldValues& live, Sounding& s) const {
    s.far.fill(0.0f);
    if (!used(location)) {
        // An empty Location slot is the live world.
        s.v = live;
        s.patterns.fill(nullptr);
        return;
    }
    const auto& data = locationsIn[std::size_t(location)].read();
    s.v = data.values;
    keepLive(live, s);
    for (int slot = 0; slot < locationSlots; ++slot) s.patterns[std::size_t(slot)] = &data.patterns[std::size_t(slot)];
}

void Travel::journeyPlace(const TravelInputs& in, int& from, int& to, float& position) const {
    if (in.arranged) {
        from = std::clamp(in.from, -1, locationCount - 1);
        to = std::clamp(in.to, -1, locationCount - 1);
        position = easeTravel(std::isfinite(in.journey) ? std::clamp(in.journey, 0.0f, 1.0f) : 0.0f);
        return;
    }
    if (!in.pathOn) {
        from = std::clamp(in.from, 0, locationCount - 1);
        to = std::clamp(in.to, 0, locationCount - 1);
        position = std::isfinite(in.journey) ? std::clamp(in.journey, 0.0f, 1.0f) : 0.0f;
        return;
    }
    // The Path: the saved Locations in order. Each leg lasts pathBars bars; the journey
    // to the next stop takes the Travel Time (at most the leg) and ends on its downbeat.
    std::array<int, locationCount> stops {};
    int count = 0;
    for (int i = 0; i < locationCount; ++i)
        if (used(i)) stops[std::size_t(count++)] = i;
    position = 0.0f;
    if (count == 0) {
        from = to = -1;
        return;
    }
    const double bpm = std::isfinite(in.bpm) && in.bpm >= 20.0 ? in.bpm : 96.0;
    const double legBeats = 4.0 * double(std::max(1, in.pathBars));
    const double beat = std::isfinite(in.beat) ? std::max(0.0, in.beat) : 0.0;
    const auto leg = int64_t(std::floor(beat / legBeats));
    const double into = beat - double(leg) * legBeats;
    const double travelBeats = std::clamp(in.syncBars > 0 ? 4.0 * double(in.syncBars) : double(in.seconds) * bpm / 60.0, 1.0e-3, legBeats);
    const auto stop = [&](int64_t k) { return stops[std::size_t(in.pathLoop ? k % count : std::min<int64_t>(k, count - 1))]; };
    from = stop(leg);
    to = stop(leg + 1);
    if (into > legBeats - travelBeats) position = easeTravel(float((into - (legBeats - travelBeats)) / travelBeats));
}

void Travel::journeyState(const TravelInputs& in, const WorldValues& live, Sounding& s) {
    int from = 0, to = 0;
    float position = 0.0f;
    journeyPlace(in, from, to, position);
    locationState(from, live, a);
    locationState(to, live, b);
    s.v = live;
    blendWorlds(a, b, position, s);
}

void Travel::holdState(const WorldValues& live, Sounding& s) const {
    if (holdLocation >= 0) {
        locationState(holdLocation, live, s);
        return;
    }
    s = kept;
    keepLive(live, s);
}

void Travel::targetState(const TravelInputs& in, const WorldValues& live, Sounding& s) {
    switch (target) {
        case ToLocation: locationState(targetLocation, live, s); break;
        case ToJourney: journeyState(morphIn, live, s); break;
        case ToKept:
            s = kept;
            keepLive(live, s);
            break;
    }
}

void Travel::soundingNow(const TravelInputs& in, const WorldValues& live, Sounding& s) {
    switch (mode) {
        case Idle:
            s.v = live;
            s.far.fill(0.0f);
            s.patterns.fill(nullptr);
            break;
        case Travelling:
            targetState(in, live, c);
            s.v = live;
            blendWorlds(origin, c, easeTravel(float(double(elapsed) / double(duration))), s);
            break;
        case Holding: holdState(live, s); break;
        case Journeying: journeyState(morphIn, live, s); break;
    }
}

void Travel::begin(Target to, int location, const TravelInputs& in, const WorldValues& live,
                   const std::array<const pat::SlotPatterns*, locationSlots>& livePatterns) {
    // The journey starts from the world exactly as it sounds now; its patterns are
    // copied (the live ones, or those of the journey it interrupts).
    soundingNow(in, live, out);
    originWhich ^= 1;
    auto& store = originStore[std::size_t(originWhich)];
    for (int slot = 0; slot < locationSlots; ++slot) {
        const auto k = std::size_t(slot);
        const pat::SlotPatterns* from = out.patterns[k] != nullptr ? out.patterns[k] : livePatterns[k];
        if (from != nullptr) store[k] = *from;
        else store[k] = emptyPatterns();
        origin.patterns[k] = &store[k];
    }
    origin.v = out.v;
    origin.far = out.far;
    const double bpm = std::isfinite(in.bpm) && in.bpm >= 20.0 ? in.bpm : 96.0;
    const double seconds = in.syncBars > 0 ? double(in.syncBars) * 4.0 * 60.0 / bpm : std::clamp(double(in.seconds), 0.5, 60.0);
    duration = std::max<uint64_t>(1, uint64_t(seconds * rate));
    elapsed = 0;
    target = to;
    targetLocation = location;
    mode = Travelling;
}

void Travel::hold(int location, const Sounding& world, const TravelInputs& in) {
    mode = Holding;
    holdLocation = location;
    auto& arrival = arrivalOut.write();
    arrival.serial = ++arrivalSerial;
    arrival.generation = seenReset;
    arrival.location = location;
    arrival.values = world.v;
    for (int slot = 0; slot < locationSlots; ++slot) {
        const auto k = std::size_t(slot);
        if (world.patterns[k] != nullptr) arrival.patterns[k] = *world.patterns[k];
        else arrival.patterns[k].type = -1;   // keep the slot's patterns as they are
    }
    arrivalOut.publish();
    settledDestination.store(in.destination, std::memory_order_relaxed);
    uiArrived.store(location, std::memory_order_relaxed);
}

void Travel::arrive(const TravelInputs& in, const WorldValues& live) {
    switch (target) {
        case ToLocation:
            locationState(targetLocation, live, c);
            hold(targetLocation, c, in);
            break;
        case ToJourney: mode = Journeying; break;
        case ToKept: hold(-1, kept, in); break;
    }
}

void Travel::keep(const Sounding& world, const std::array<const pat::SlotPatterns*, locationSlots>& livePatterns) {
    kept.v = world.v;
    kept.far.fill(0.0f);
    for (int slot = 0; slot < locationSlots; ++slot) {
        const auto k = std::size_t(slot);
        const pat::SlotPatterns* from = world.patterns[k] != nullptr ? world.patterns[k] : livePatterns[k];
        if (from != &keptStore[k]) keptStore[k] = from != nullptr ? *from : emptyPatterns();
        kept.patterns[k] = &keptStore[k];
    }
}

const Sounding& Travel::advance(const WorldValues& live, const std::array<const pat::SlotPatterns*, locationSlots>& livePatterns,
                                const TravelInputs& in, bool restart) {
    for (auto& location : locationsIn) location.update();
    if (const uint32_t reset = resetSerial.load(std::memory_order_acquire); reset != seenReset) {
        seenReset = reset;
        started = false;
        mode = Idle;
    }
    if (!started || restart) morphIn = in;
    if (in.arranged != wasArranged) {
        wasArranged = in.arranged;
        if (!in.arranged) {
            // The arrangement stopped driving the world: the parameters take over again.
            if (mode == Journeying || (mode == Travelling && target == ToJourney)) mode = in.journeyOn ? Journeying : Idle;
            wasJourney = in.journeyOn;
            journeyKey = keyOf(in);
        }
    }
    if (!started) {
        // The first block after loading: the parameters already hold the world.
        started = true;
        lastDestination = in.destination;
        wasJourney = in.journeyOn;
        journeyKey = keyOf(in);
        seenRequest = requestSerial.load(std::memory_order_acquire);
        if (in.journeyOn) {
            mode = Journeying;
        } else if (in.destination > 0 && in.destination != settledDestination.load(std::memory_order_relaxed) && used(in.destination - 1)) {
            // Saved during a journey: open at its Destination.
            locationState(in.destination - 1, live, c);
            hold(in.destination - 1, c, in);
        }
    }

    if (restart) {
        // Transport mode started or located: be where the world should be, at once.
        if (in.journeyOn) {
            mode = Journeying;
        } else if (in.destination > 0 && in.destination != settledDestination.load(std::memory_order_relaxed) && used(in.destination - 1)) {
            locationState(in.destination - 1, live, c);
            hold(in.destination - 1, c, in);
        } else if (mode == Travelling && target == ToLocation) {
            arrive(in, live);
        } else if (mode == Travelling || mode == Journeying) {
            soundingNow(in, live, out);
            keep(out, livePatterns);
            hold(-1, kept, in);
        }
        lastDestination = in.destination;
        wasJourney = in.journeyOn;
        journeyKey = keyOf(in);
    }

    if (mode == Holding && released.load(std::memory_order_acquire) == arrivalSerial) mode = Idle;

    if (in.journeyOn != wasJourney) {
        wasJourney = in.journeyOn;
        if (in.journeyOn) {
            begin(ToJourney, -1, in, live, livePatterns);
        } else if (mode == Journeying || (mode == Travelling && target == ToJourney)) {
            // Keep the world where the Journey stands; a Place still on its way in or out
            // comes back to its place first.
            soundingNow(in, live, out);
            keep(out, livePatterns);
            bool moving = false;
            for (float f : out.far) moving |= f > 1.0e-4f;
            if (moving) begin(ToKept, -1, in, live, livePatterns);
            else hold(-1, kept, in);
        }
    }
    if (const int key = keyOf(in); key != journeyKey) {
        // Another morph (the Path switched, or From or To changed): travel to it.
        journeyKey = key;
        if (in.journeyOn && (mode == Journeying || (mode == Travelling && target == ToJourney))) begin(ToJourney, -1, in, live, livePatterns);
    }
    if (!in.journeyOn) {
        if (in.destination != lastDestination) {
            lastDestination = in.destination;
            if (in.destination > 0 && used(in.destination - 1)) begin(ToLocation, in.destination - 1, in, live, livePatterns);
        }
        const uint32_t request = requestSerial.load(std::memory_order_acquire);
        if (request != seenRequest) {
            seenRequest = request;
            const int to = requestTarget.load(std::memory_order_relaxed);
            if (used(to)) begin(ToLocation, to, in, live, livePatterns);
        }
    } else {
        lastDestination = in.destination;
        seenRequest = requestSerial.load(std::memory_order_acquire);
    }

    morphIn = in;
    soundingNow(in, live, out);
    float progress = 1.0f;
    if (mode == Travelling) progress = float(double(elapsed) / double(duration));
    int from = -1, to = -1;
    if (mode == Journeying) journeyPlace(in, from, to, progress);
    uiFrom.store(from, std::memory_order_relaxed);
    uiTo.store(to, std::memory_order_relaxed);
    uiMode.store(mode, std::memory_order_relaxed);
    uiTarget.store(mode == Travelling && target == ToLocation ? targetLocation : mode == Holding ? holdLocation : -1, std::memory_order_relaxed);
    uiProgress.store(progress, std::memory_order_relaxed);
    return out;
}

void Travel::step(int n, const TravelInputs& in, const WorldValues& live) {
    if (mode != Travelling) return;
    elapsed += uint64_t(std::max(0, n));
    if (elapsed >= duration) arrive(in, live);
}

} // namespace wb
