#pragma once
// KEYS (Milestone 14): playing and recording a Place's instrument from a MIDI keyboard.
//
// In GarageBand and Logic a keyboard plays the instrument plugin on the selected track,
// so WORLDBUILDER receives the keyboard's notes from the host. The Play target (a Place
// slot and one of its instruments) plays them live: velocity, note length, the sustain
// pedal (CC64), pitch bend and the mod wheel. Armed for recording, the notes are also
// captured into that instrument's pattern, on the host's bars and tempo (or, with the
// transport stopped, on an internal clock at the host tempo that starts on the first
// note).
//
// The audio thread owns Input (the keys' state) and Capture (what a take records, as a
// stream of events); the message thread turns the events into pattern notes (see
// Recorder.h). Fixed storage only: nothing here allocates.
#include "../DSP/Core.h"
#include "../DSP/Music.h"
#include "../Patterns/Pattern.h"
#include <array>
#include <atomic>
#include <cstdint>

namespace wb::keys {

// Scale mode: play exactly what is pressed, or move wrong notes to the nearest note of
// the current Root and Mood.
enum ScaleMode { Chromatic, SnapToScale, ScaleModeCount };
inline constexpr std::array<const char*, ScaleModeCount> scaleModeNames { "CHROMATIC", "SNAP TO SCALE" };

// What a take does to the pattern: a new pattern, notes added to the existing one, or a
// new pattern in one of the Arrangement's variants (A, B, C or Fill).
enum RecordMode { Replace, Overdub, IntoVariant, RecordModeCount };
inline constexpr std::array<const char*, RecordModeCount> recordModeNames { "REPLACE", "OVERDUB", "INTO VARIANT" };

// Quantise on input: the grid, in beats (0: off).
enum Quantise { QuantiseOff, QuantiseQuarter, QuantiseEighth, QuantiseSixteenth, QuantiseThirtySecond, QuantiseEighthT, QuantiseSixteenthT, QuantiseCount };
inline constexpr std::array<const char*, QuantiseCount> quantiseNames { "OFF", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T" };
inline constexpr std::array<double, QuantiseCount> quantiseBeats { 0.0, 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0 };
inline bool tripletGrid(int q) { return q == QuantiseEighthT || q == QuantiseSixteenthT; }

// A beat moved toward the nearest grid line by `strength` (0..1): 1 lands on it, less
// keeps some of the player's timing.
inline double quantiseBeat(double beat, int q, float strength) {
    const double grid = quantiseBeats[std::size_t(std::clamp(q, 0, QuantiseCount - 1))];
    if (grid <= 0.0) return beat;
    const double target = std::round(beat / grid) * grid;
    return beat + double(std::clamp(strength, 0.0f, 1.0f)) * (target - beat);
}

constexpr int octaveMin = -2, octaveMax = 2;
// MIDI note of the key root in the octave the instruments count from (music::rootHz:
// C3 = 130.8 Hz = MIDI 48).
inline int rootNote(int root) { return 48 + std::clamp(root, 0, 11); }

// The Play target's settings, as the processor reads them for each block.
struct Controls {
    int slot = -1, element = 0;   // slot -1: no target (incoming notes do nothing)
    int scale = Chromatic, octave = 0;
    bool monitor = true;          // hear the keyboard live
    bool armed = false;           // REC: capture notes into the pattern
    bool countIn = false;         // a bar of clicks before a take with the transport
    int variant = 0;              // the variant recorded into (and heard while armed)
    bool hostPlaying = false;     // the host transport (or the Pattern Editor's) is rolling (not the ARRANGE preview)
    // Single-instrument mode: this instance plays only the target, from host MIDI, with
    // its effects; the rest of the scene (and its pattern) is silent.
    bool single = false;
    // Punch: a take records only notes inside [punchIn, punchOut) beats of the pattern
    // (which repeats every `length` beats).
    bool punch = false;
    double punchIn = 0, punchOut = 0, length = 16;
    bool inPunch(double beat) const {
        if (!punch || punchOut <= punchIn) return true;
        const double at = length > 0 ? beat - std::floor(beat / length) * length : beat;
        return at >= punchIn - 1.0e-9 && at < punchOut - 1.0e-9;
    }
    bool valid() const { return slot >= 0 && slot < 4 && element >= 0 && element < pat::elementsPerPlace; }
};

//==============================================================================
// One timed thing from the host's MIDI, at a sample offset in the block.
struct Event {
    enum Kind : uint8_t { NoteOn, NoteOff, Sustain, Bend, Mod, AllOff };
    int offset = 0;
    uint8_t kind = NoteOn;
    uint8_t note = 0;
    float value = 0;   // velocity 0..1, pedal 0/1, bend -1..1, mod 0..1
};

// What the target instrument plays within a chunk: notes starting and stopping (after
// the sustain pedal), at sample offsets.
struct LiveNote {
    int offset = 0;
    bool on = true;
    int id = 0;        // the MIDI note + 1
    float semis = 0;   // concert pitch above the key root in octave 3 (the part subtracts its own transpose)
    int row = 0;       // a kit's row
    float velocity = 1;
};
struct Live {
    static constexpr int maxNotes = 64;
    std::array<LiveNote, maxNotes> notes {};
    int count = 0;
    float bend = 0;   // semitones, already scaled by the instrument's range
    float mod = 0;    // 0..1
    bool monitor = true;
    void clear() { count = 0; }
    void add(const LiveNote& n) {
        if (count < maxNotes) notes[std::size_t(count++)] = n;
    }
};

//==============================================================================
// The keys as the audio thread knows them: what is held by a finger, what the pedal
// holds, the pitch wheel and the mod wheel. Events are timed in the engine's stream of
// rendered samples, so they land on their sample whatever the block size.
class Input {
public:
    static constexpr int queueSize = 512;

    void reset() {
        head = tail = 0;
        down.fill(false);
        pedalled.fill(false);
        velocity.fill(0);
        pedal = false;
        bendValue = 0;
        modValue = 0;
        for (auto& w : downBits) w.store(0, std::memory_order_relaxed);
        for (auto& w : soundBits) w.store(0, std::memory_order_relaxed);
    }
    // Audio thread: queues one host event at stream position `at`.
    void push(const Event& e, uint64_t at) {
        if (((tail + 1) & (queueSize - 1)) == head) return;   // full: dropped
        queue[std::size_t(tail & (queueSize - 1))] = { e, at };
        tail = (tail + 1) & (queueSize - 1);
    }
    bool pending() const { return head != tail; }
    // The next queued event before stream position `before` (false: none).
    bool next(uint64_t before, Event& e, uint64_t& at) {
        if (head == tail) return false;
        const auto& q = queue[std::size_t(head)];
        if (q.at >= before) return false;
        e = q.event;
        at = q.at;
        head = (head + 1) & (queueSize - 1);
        return true;
    }
    // Applies one event; calls on(note, velocity) when a note starts sounding and
    // off(note) when it stops (a key released while the pedal holds it stops when the
    // pedal lifts).
    template <typename On, typename Off>
    void apply(const Event& e, On&& on, Off&& off) {
        const auto k = std::size_t(e.note & 127);
        switch (e.kind) {
            case Event::NoteOn:
                if (e.value <= 0.0f) {
                    release(k, off);
                    break;
                }
                // The same key again while it (or the pedal) still holds it: the old note stops first.
                if (down[k] || pedalled[k]) off(int(k));
                down[k] = true;
                pedalled[k] = false;
                velocity[k] = e.value;
                on(int(k), e.value);
                break;
            case Event::NoteOff: release(k, off); break;
            case Event::Sustain:
                pedal = e.value >= 0.5f;
                if (!pedal)
                    for (std::size_t i = 0; i < 128; ++i)
                        if (pedalled[i]) {
                            pedalled[i] = false;
                            off(int(i));
                        }
                break;
            case Event::Bend: bendValue = std::clamp(e.value, -1.0f, 1.0f); break;
            case Event::Mod: modValue = std::clamp(e.value, 0.0f, 1.0f); break;
            case Event::AllOff:
                for (std::size_t i = 0; i < 128; ++i)
                    if (down[i] || pedalled[i]) {
                        down[i] = pedalled[i] = false;
                        off(int(i));
                    }
                pedal = false;
                break;
            default: break;
        }
        publish();
    }
    float bend() const { return bendValue; }
    float mod() const { return modValue; }
    bool sustain() const { return pedal; }
    bool held(int note) const { return down[std::size_t(note & 127)]; }
    bool sounding(int note) const { return down[std::size_t(note & 127)] || pedalled[std::size_t(note & 127)]; }
    bool anySounding() const {
        for (std::size_t i = 0; i < 128; ++i)
            if (down[i] || pedalled[i]) return true;
        return false;
    }

    // For the editor's keyboard: the keys held (down) and those still sounding (down or
    // held by the pedal), as bits.
    std::array<std::atomic<uint32_t>, 4> downBits {}, soundBits {};

private:
    template <typename Off>
    void release(std::size_t k, Off&& off) {
        if (!down[k]) return;
        down[k] = false;
        if (pedal) pedalled[k] = true;
        else off(int(k));
    }
    void publish() {
        for (std::size_t w = 0; w < 4; ++w) {
            uint32_t d = 0, s = 0;
            for (std::size_t b = 0; b < 32; ++b) {
                const auto k = w * 32 + b;
                if (down[k]) d |= 1u << b;
                if (down[k] || pedalled[k]) s |= 1u << b;
            }
            downBits[w].store(d, std::memory_order_relaxed);
            soundBits[w].store(s, std::memory_order_relaxed);
        }
    }
    struct Queued {
        Event event;
        uint64_t at = 0;
    };
    std::array<Queued, queueSize> queue {};
    int head = 0, tail = 0;
    std::array<bool, 128> down {}, pedalled {};
    std::array<float, 128> velocity {};
    bool pedal = false;
    float bendValue = 0, modValue = 0;
};

//==============================================================================
// What a take records, handed from the audio thread to the message thread through a
// lock-free single-producer, single-consumer queue.
struct Captured {
    enum Kind : uint8_t { TakeStart, TakeEnd, NoteOn, NoteOff };
    uint8_t kind = NoteOn;
    bool free = false;      // a free take (transport stopped): beats count from its first note
    bool kit = false;
    int8_t note = 0;        // the MIDI note pressed
    int16_t pitch = 0;      // the pitch the instrument plays, semitones above the key root (before its transpose); kit: the row
    uint8_t velocity = 100;
    int8_t chord = 0;       // the harmony's chord degree there (for patterns that follow the chords)
    uint8_t slot = 0, element = 0, variant = 0;
    uint16_t type = 0;      // the Place in the slot
    double beat = 0;        // the host's beat (free takes: beats since the first note)
};

template <typename T, int Size>
class Fifo {
public:
    bool push(const T& x) {
        const int t = tail.load(std::memory_order_relaxed), n = (t + 1) & (Size - 1);
        if (n == head.load(std::memory_order_acquire)) return false;
        items[std::size_t(t)] = x;
        tail.store(n, std::memory_order_release);
        return true;
    }
    bool pop(T& x) {
        const int h = head.load(std::memory_order_relaxed);
        if (h == tail.load(std::memory_order_acquire)) return false;
        x = items[std::size_t(h)];
        head.store((h + 1) & (Size - 1), std::memory_order_release);
        return true;
    }
    void clear() { head.store(tail.load(std::memory_order_acquire), std::memory_order_release); }

private:
    std::array<T, Size> items {};
    std::atomic<int> head { 0 }, tail { 0 };
};

// The take's timeline on the audio thread: armed and the host rolling, a take starts
// where the host starts (after a bar of count-in when asked) and ends when it stops;
// armed with the host stopped, a free take starts on the first note, on an internal
// clock at the host tempo, and ends when REC is released or the host starts.
class Capture {
public:
    enum State { Idle, Waiting, CountIn, Taking, FreeWaiting, FreeTaking };
    Fifo<Captured, 2048> out;
    std::atomic<int> stateOut { Idle };
    std::atomic<float> countInLeft { 0 };   // beats of count-in left (for the editor)
    std::atomic<double> freeBeatOut { 0 };

    void reset() {
        state = Idle;
        freeBeat = 0;
        lastEnd = 0;
        stateOut.store(Idle, std::memory_order_relaxed);
    }
    State current() const { return state; }
    bool free() const { return state == FreeWaiting || state == FreeTaking; }
    double freeClock() const { return freeBeat; }
    // The metronome: during the count-in and a free take.
    bool clicking() const { return state == CountIn || state == FreeTaking; }

    // Once per piece, before its notes: where the take stands. `beat` is the target's beat
    // at the piece start; perSample its beats per sample.
    void step(const Controls& k, double beat, double perSample, int n, Captured base) {
        const bool rolling = k.hostPlaying;
        punch = k;
        // The song went back (a loop around, or the host cycling): the pass ends and the
        // next one is a new take.
        if (state == Taking && rolling && k.armed && beat < lastEnd - perSample - 1.0e-9) {
            end(base, lastEnd);
            start(base, beat, false);
        }
        lastEnd = beat + perSample * double(n);
        if (!k.armed || !k.valid()) {
            if (state == Taking || state == CountIn || state == FreeTaking) end(base, state == FreeTaking ? freeBeat : beat);
            state = Idle;
        } else if (rolling) {
            if (state == FreeTaking) end(base, freeBeat);
            if (state == Idle || state == FreeWaiting || state == FreeTaking) {
                // The host started with REC armed (or REC was armed while it plays).
                if (k.countIn) {
                    takeFrom = std::floor(beat / 4.0 + 1.0e-9) * 4.0 + 4.0;
                    state = CountIn;
                } else {
                    start(base, beat, false);
                }
            }
            if (state == CountIn) {
                countInLeft.store(float(std::max(0.0, takeFrom - beat)), std::memory_order_relaxed);
                if (beat + perSample * double(n) > takeFrom) start(base, takeFrom, false);
            }
        } else {
            // The host stopped: a take with it ends; a free take waits for the first note.
            if (state == Taking || state == CountIn) end(base, beat);
            if (state != FreeTaking) state = FreeWaiting;
        }
        stateOut.store(state, std::memory_order_relaxed);
        if (state != CountIn) countInLeft.store(0, std::memory_order_relaxed);
    }
    // After the piece's notes: the free take's clock moves on.
    void finish(double perSample, int n) {
        if (state == FreeTaking) freeBeat += perSample * double(n);
        freeBeatOut.store(state == FreeTaking ? freeBeat : 0.0, std::memory_order_relaxed);
    }
    // A note at `beat` (the target's beat there; a free take keeps its own clock).
    // Returns whether it was recorded.
    bool note(Captured c, double beat, double freeOffset) {
        if (state == FreeWaiting && c.kind == Captured::NoteOn) {
            freeBeat = -freeOffset;   // the first note is beat 0 of the take
            start(c, 0.0, true);
        }
        if (state == Taking) {
            if (beat < takeFrom - 1.0e-9) return false;
            if (c.kind == Captured::NoteOn && !punch.inPunch(beat)) return false;   // outside the punch region
            c.beat = beat;
            c.free = false;
        } else if (state == FreeTaking) {
            c.beat = freeBeat + freeOffset;
            c.free = true;
        } else {
            return false;
        }
        return out.push(c);
    }

private:
    void start(Captured c, double at, bool isFree) {
        c.kind = Captured::TakeStart;
        c.beat = at;
        c.free = isFree;
        takeFrom = at;
        state = isFree ? FreeTaking : Taking;
        out.push(c);
    }
    void end(Captured c, double at) {
        c.kind = Captured::TakeEnd;
        c.beat = at;
        c.free = state == FreeTaking;
        out.push(c);
    }
    State state = Idle;
    double takeFrom = 0, freeBeat = 0, lastEnd = 0;
    Controls punch;   // the punch region in force
};

//==============================================================================
// The metronome for the count-in and free takes: a short tuned click, higher on the
// bar's first beat.
class Click {
public:
    void prepare(float sampleRate) {
        rate = sampleRate;
        decay = std::exp(-1.0f / (0.025f * rate));
        reset();
    }
    void reset() {
        amp = 0;
        phase = 0;
        lastBeat = -1;
    }
    // Adds n samples; `beat` and `perSample` place the clicks on the beats.
    void render(float* left, float* right, int n, double beat, double perSample, bool on) {
        if (!on && amp < 1.0e-5f) {
            lastBeat = int64_t(std::floor(beat + perSample * double(n) + 1.0e-9));
            return;
        }
        for (int i = 0; i < n; ++i) {
            const double b = beat + perSample * double(i);
            const auto whole = int64_t(std::floor(b + 1.0e-9));
            if (on && whole != lastBeat) {
                lastBeat = whole;
                const bool bar = ((whole % 4) + 4) % 4 == 0;
                hz = bar ? 1760.0f : 1320.0f;
                amp = bar ? 0.22f : 0.15f;
            }
            if (!on) lastBeat = whole;
            if (amp < 1.0e-5f) continue;
            phase += hz / rate;
            phase -= std::floor(phase);
            const float y = amp * std::sin(dsp::twoPi * phase);
            amp *= decay;
            left[i] += y;
            right[i] += y;
        }
    }
private:
    float rate = 48000, amp = 0, phase = 0, hz = 1320, decay = 0.999f;
    int64_t lastBeat = -1;
};

} // namespace wb::keys
