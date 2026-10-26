#pragma once
// Travel between Locations, on the audio thread.
//
// The host parameters are the live world. Choosing a Destination starts a journey from
// the world as it sounds now to that Location, over the Travel Time (eased in and
// out). While it lasts, the world is the blend of Locations.h, not the parameters. On
// arrival the Location is handed to the message thread, which writes it into the
// parameters and the patterns (so the knobs show it) and then releases the hold; from
// then on the parameters are the world again.
//
// Journey Mode morphs between two Locations (From, To) at the Journey position instead,
// for drawing a walk through the world as automation. Switching it on travels to the
// morph; switching it off keeps the world where the Journey stood and writes that back.
//
// Transport mode: a start or locate jumps straight to where the world should be (the
// Destination's Location, or the Journey morph), so the same song position always
// gives the same world.
#include "Locations.h"
#include <atomic>

namespace wb {

struct TravelInputs {
    int destination = 0;      // 0: none, else Location 1..8
    float seconds = 4;        // Travel Time
    int syncBars = 0;         // > 0: the Travel Time is this many 4/4 bars of the host tempo
    bool journeyOn = false;
    int from = 0, to = 1;     // Journey Locations, 0..7
    float journey = 0;        // Journey position
    double bpm = 96;
    // Journey Path: the saved Locations in order, a leg every pathBars bars of the song
    // (the beat), arriving on each leg's downbeat; at the end stop, or loop.
    bool pathOn = false, pathLoop = true;
    int pathBars = 4;
    double beat = 0;
};

// A world that arrived, for the message thread to write back into the parameters.
struct Arrival {
    uint32_t serial = 0;
    uint32_t generation = 0;   // the world it belongs to (a load starts a new one)
    int location = -1;   // 0..7, or -1: a Journey kept where it stood
    WorldValues values {};
    std::array<pat::SlotPatterns, locationSlots> patterns {};
};

class Travel {
public:
    enum Mode { Idle, Travelling, Holding, Journeying };

    void prepare(double sampleRate);
    // The world as it sounds now, after this block's changes (a new Destination, Journey
    // Mode switched, a request). live: the parameters; livePatterns: the editor's
    // patterns per slot (copied when a journey starts from the live world). restart: the
    // host transport started or located in Transport mode.
    const Sounding& advance(const WorldValues& live, const std::array<const pat::SlotPatterns*, locationSlots>& livePatterns,
                            const TravelInputs& in, bool restart);
    // n samples of that world have played: a journey moves on (and may arrive).
    void step(int n, const TravelInputs& in, const WorldValues& live);
    // Travelling, journeying or holding an arrival: the world is not the parameters.
    bool active() const { return mode != Idle; }
    Mode currentMode() const { return mode; }

    // Message thread -> audio thread: each Location's data.
    std::array<pat::TripleBuffer<LocationData>, locationCount> locationsIn;
    // Audio thread -> message thread: the latest arrival. The message thread writes it
    // back and then stores its serial in `released`.
    pat::TripleBuffer<Arrival> arrivalOut;
    std::atomic<uint32_t> released { 0 };
    // The interface asks to travel to a Location (also the one the Destination names).
    void request(int location) {
        requestTarget.store(location, std::memory_order_relaxed);
        requestSerial.fetch_add(1, std::memory_order_release);
    }
    // A new world was loaded (project, preset, .world file): any journey stops and the
    // parameters are the world again (from the next block).
    void requestReset() { resetSerial.fetch_add(1, std::memory_order_release); }
    // Arrivals from before the latest load are not written back.
    uint32_t generation() const { return resetSerial.load(std::memory_order_acquire); }
    // The Destination value the world last settled for (saved with the project): a
    // Transport start with another Destination jumps there.
    std::atomic<int> settledDestination { 0 };

    // For the editor.
    std::atomic<int> uiMode { Idle };
    std::atomic<int> uiTarget { -1 };      // the Location travelled to, -1: none (a Journey)
    std::atomic<int> uiArrived { -1 };     // the Location last arrived at, -1: none
    std::atomic<float> uiProgress { 0 };   // 0..1 along the way
    std::atomic<int> uiFrom { -1 }, uiTo { -1 };   // the Journey's (or Path leg's) two Locations

private:
    enum Target { ToLocation, ToJourney, ToKept };
    void locationState(int location, const WorldValues& live, Sounding& s) const;
    void journeyState(const TravelInputs& in, const WorldValues& live, Sounding& s);
    // Journey Mode's two Locations and position (From, To, Journey; or the Path's leg).
    void journeyPlace(const TravelInputs& in, int& from, int& to, float& position) const;
    void targetState(const TravelInputs& in, const WorldValues& live, Sounding& s);
    void holdState(const WorldValues& live, Sounding& s) const;
    // The world as it sounds in the current mode (before this block's changes).
    void soundingNow(const TravelInputs& in, const WorldValues& live, Sounding& s);
    void begin(Target target, int location, const TravelInputs& in, const WorldValues& live,
               const std::array<const pat::SlotPatterns*, locationSlots>& livePatterns);
    void arrive(const TravelInputs& in, const WorldValues& live);
    void hold(int location, const Sounding& world, const TravelInputs& in);
    // Keeps a world (a Journey where it stands): its patterns copied, every Place in place.
    void keep(const Sounding& world, const std::array<const pat::SlotPatterns*, locationSlots>& livePatterns);
    bool used(int location) const { return location >= 0 && location < locationCount && locationsIn[std::size_t(location)].read().used; }
    static void keepLive(const WorldValues& live, Sounding& s);

    double rate = 48000;
    Mode mode = Idle;
    Target target = ToLocation;
    int targetLocation = -1, holdLocation = -1;
    uint64_t elapsed = 0, duration = 1;
    // Where the journey started (its patterns copied: the live ones may change), two
    // stores so a journey starting from inside another can copy from the old one.
    Sounding origin;
    std::array<std::array<pat::SlotPatterns, locationSlots>, 2> originStore {};
    int originWhich = 0;
    // A Journey kept where it stood (Journey Mode switched off).
    Sounding kept;
    std::array<pat::SlotPatterns, locationSlots> keptStore {};
    Sounding out, a, b, c;
    int lastDestination = 0;
    uint32_t seenRequest = 0, arrivalSerial = 0;
    std::atomic<uint32_t> requestSerial { 0 }, resetSerial { 0 };
    uint32_t seenReset = 0;
    std::atomic<int> requestTarget { 0 };
    bool wasJourney = false, started = false;
    int journeyKey = -1;
    // The inputs the Journey morph is computed from: last block's while this block's
    // changes are handled (so leaving or changing a morph starts from where it stood).
    TravelInputs morphIn;   // which morph (the Path, or From and To): a change travels to the new one
    static int keyOf(const TravelInputs& in) { return in.pathOn ? 1000 : in.from * locationCount + in.to; }
};

} // namespace wb
