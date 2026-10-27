#pragma once
// The hurdy-gurdy (the GarageBand fix round): rebuilt from how the real instrument makes
// its sound (sources and numbers in docs/INSTRUMENT_NOTES.md).
//
// - A rosined wheel, turned by a crank, bows every string at once. Each string is a
//   bowed digital waveguide: two delay lines (nut side and bridge side of the contact
//   point) meeting at a stick-slip friction junction (the bow table of McIntyre,
//   Schumacher and Woodhouse, in the form of Smith's and STK's bowed string). The wheel
//   speed is the bow velocity, so the tone is a true Helmholtz sawtooth that speaks,
//   swells and roughens with the crank. The wheel touches the strings near the bridge
//   (about 1/11 of the length), which makes the tone nasal.
// - Wheel friction noise: rosin hiss on the strings and a low rumble of the wheel, both
//   following its speed.
// - The melody string (chanterelle) is stopped by keys (tangents): its pitch steps from
//   note to note with no glide, each key press a small wooden tick. Between notes it
//   sounds its open string (the fifth).
// - Two drones bowed by the same wheel: the bourdon on the root an octave down and the
//   mouche on the fifth below the root.
// - The trompette, a drone on the root whose string rests on the buzzing bridge (the
//   chien): one foot of that bridge is free. When the player gives the crank a sharp
//   stroke of the wrist (the coup de poignet), the string's force lifts the free foot
//   and it raps on the soundboard once per string period: the rhythmic rasp. The
//   strokes come in time (beats, off-beats, or four to a turn of the wheel).
// - A small wooden body with a nasal resonance: a few body modes (a resonator bank).

#include "Ensemble.h"

namespace wb {

namespace hurdy {

// One bowed string: a waveguide with a friction junction at the wheel.
struct BowedWaveguide {
    std::vector<float> nut, bridge;   // delay lines, sized in prepare()
    int nutW = 0, bridgeW = 0, size = 0;
    float nutDelay = 100, bridgeDelay = 10, nutTarget = 100, bridgeTarget = 10;
    float loss = 0;          // the bridge's one-pole loss filter state
    float pole = 0.6f, lossGain = 0.95f;
    float pressure = 0.5f;   // bow (wheel) pressure: the friction curve's slope
    bool settled = false;
    float last = 0;
    void prepare(float rate, float lowestHz) {
        size = int(rate / lowestHz) + 8;
        nut.assign(std::size_t(size), 0.0f);
        bridge.assign(std::size_t(size), 0.0f);
        pole = std::clamp(0.75f - 0.2f * 22050.0f / rate, 0.1f, 0.9f);
        reset();
    }
    void reset() {
        std::fill(nut.begin(), nut.end(), 0.0f);
        std::fill(bridge.begin(), bridge.end(), 0.0f);
        nutW = bridgeW = 0;
        loss = last = 0;
        settled = false;   // the next tuning is taken at once (a replay sounds the same)
    }
    // The string's length for `hz`, the wheel at `position` of it from the bridge.
    void tune(float hz, float rate, float position) {
        const float total = std::clamp(rate / std::max(hz, 20.0f) - 4.0f, 8.0f, float(size - 4));
        bridgeTarget = std::max(2.0f, total * position);
        nutTarget = std::max(2.0f, total - bridgeTarget);
        if (!settled) {
            nutDelay = nutTarget;
            bridgeDelay = bridgeTarget;
            settled = true;
        }
    }
    static float read(const std::vector<float>& line, int write, int size, float delay) {
        float at = float(write) - delay;
        while (at < 0) at += float(size);
        const int i = int(at);
        const float frac = at - float(i);
        const float a = line[std::size_t(i % size)], b = line[std::size_t((i + 1) % size)];
        return a + (b - a) * frac;
    }
    // One sample at wheel (bow) velocity `bow`; returns the force on the bridge.
    float tick(float bow) {
        // A new length is reached within about 2 ms: a step to the ear (a key, not a
        // slide), without the click of a delay line jumping mid-waveform.
        nutDelay += (nutTarget - nutDelay) * 0.02f;
        bridgeDelay += (bridgeTarget - bridgeDelay) * 0.02f;
        const float fromBridge = read(bridge, bridgeW, size, bridgeDelay);
        const float fromNut = read(nut, nutW, size, nutDelay);
        loss = (1.0f - pole) * fromBridge + pole * loss;
        const float bridgeRefl = -lossGain * loss;
        const float nutRefl = -fromNut;
        const float stringVel = bridgeRefl + nutRefl;
        const float diff = bow - stringVel;
        // The friction curve: sticking when the relative velocity is small, slipping
        // (with less force) when it is large.
        const float slope = 5.0f - 4.0f * std::clamp(pressure, 0.0f, 1.0f);
        float table = std::abs(diff * slope) + 0.75f;
        table = std::min(1.0f, 1.0f / (table * table * table * table));
        const float newVel = diff * table;
        nut[std::size_t(nutW)] = dsp::finiteOr(bridgeRefl + newVel);
        bridge[std::size_t(bridgeW)] = dsp::finiteOr(nutRefl + newVel);
        nutW = (nutW + 1) % size;
        bridgeW = (bridgeW + 1) % size;
        last = fromBridge;
        return fromBridge;
    }
};

// The body's modes: Hz, Q, gain. No measured hurdy-gurdy body data was found; these
// follow the small bowed and plucked bodies in INSTRUMENT_NOTES.md (a main air and
// wood resonance near 280 Hz, a plate mode near 520 Hz) plus the nasal region of the
// instrument's spectrum around 1.1-1.3 kHz and a bright mode near 2.4 kHz.
struct BodyMode {
    float hz, q, gain;
};
inline constexpr std::array<BodyMode, 5> bodyModes { { { 280, 4.0f, 0.55f }, { 520, 5.0f, 0.45f }, { 1180, 6.0f, 0.95f }, { 2400, 5.0f, 0.5f }, { 3600, 4.0f, 0.25f } } };

} // namespace hurdy

// How the player turns the wheel and strikes the trompette (a Place sets it per chunk).
struct HurdyPatch {
    float gain = 1;
    float wheel = 0.7f;          // crank speed: how hard the strings are bowed (0..1)
    float pressure = 0.45f;      // the wheel's pressure on the strings (rougher when high)
    float brightness = 0.6f;     // the bridge's loss: brighter when high
    float chanter = 1.0f, bourdon = 0.8f, mouche = 0.6f, trompette = 0.8f;   // string levels
    float buzz = 0.7f;           // how hard the strokes lift the chien (0: no buzz)
    float strokesPerBeat = 1;    // wrist strokes in time (0: none; 2: off-beats too; 4: coup de quatre)
    float strokeOffset = 0;      // where in the beat they fall (0..1)
    float noise = 0.35f;         // rosin hiss and wheel rumble
    float body = 0.8f;           // the wooden body's colour
    float pan = 0, width = 0.3f;
};

class HurdyGurdyPart final : public Part {
public:
    HurdyPatch patch;
    void prepare(float r) override {
        rate = r;
        for (auto& s : strings) s.prepare(rate, 30.0f);
        reset();
    }
    void reset() override {
        for (auto& s : strings) s.reset();
        for (auto& side : body)
            for (auto& f : side) f.reset();
        hiss.reset();
        rumble.reset();
        rasp.reset();
        keyTick.reset();
        raspHigh.reset();
        held = {};
        playing = false;
        velocity = 0.8f;
        beat = 0;
        wheelLevel = 0;
        chien = 0;
        tick = 0;
        noiseAt = 0;
        bodyTuned = false;
        melodySemis = openSemis;
        periodPhase = raspEnv = 0;
        until = -1;
    }
    bool sustains() const override { return true; }
    void stopLive(int id) override {
        if (held.on && held.live == id) {
            held.until = -1.0e18;
            until = beat;   // the crank slows after the key
        }
    }
    void start(const PartFrame& f, const NoteOn& n) override {
        // A key pressed: the melody string's length steps to the note (no glide), and
        // the tangent ticks against the string.
        melodySemis = n.semis;
        held.set(n);
        playing = true;
        until = std::max(until, n.beat + n.length);
        velocity = n.velocity;
        tick = 1.0f;
        (void)f;
    }
    void update(const PartFrame& f) override {
        root = f.root;
        beat = f.beat;
        beatsPerSecond = float(f.beatsPerSecond);
        if (held.on && f.beat >= held.until) {
            held.on = false;
            melodySemis = openSemis;   // the key released: the open string
        }
        // The crank keeps turning a little after the last note, then stops.
        if (playing && f.beat >= until + 1.0) playing = false;
        const float position = 0.09f;   // the wheel touches the strings near the bridge
        strings[Chanter].tune(music::noteHz(root, melodySemis + bend), rate, position);
        strings[Bourdon].tune(music::noteHz(root, -12.0f), rate, position);
        strings[Mouche].tune(music::noteHz(root, -5.0f), rate, position);
        strings[Trompette].tune(music::noteHz(root, 0.0f), rate, position);
        for (auto& s : strings) {
            s.pressure = patch.pressure;
            s.lossGain = 0.93f + 0.05f * dsp::clamp01(patch.brightness + 0.5f * mod);
        }
        trompettePeriod = rate / music::noteHz(root, 0.0f);
        if (!bodyTuned) {
            for (auto& side : body)
                for (std::size_t m = 0; m < side.size(); ++m) side[m].set(hurdy::bodyModes[m].hz, hurdy::bodyModes[m].q, rate);
            hiss.set(3200.0f, 0.7f, rate);
            rumble.setCutoff(90.0f, rate);
            rasp.set(1700.0f, 1.6f, rate);
            raspHigh.setCutoff(500.0f, rate);
            keyTick.set(900.0f, 2.0f, rate);
            bodyTuned = true;
        }
    }
    void render(float* left, float* right, int n) override {
        const float target = playing ? dsp::clamp01(patch.wheel) * (0.8f + 0.2f * velocity) : 0.0f;
        const float smooth = 1.0f - std::exp(-1.0f / (0.12f * rate));   // the wheel speeds up and slows down
        const float strokes = std::max(0.0f, patch.strokesPerBeat);
        const auto pan = dsp::panGains(patch.pan);
        const float spread = patch.width;
        for (int i = 0; i < n; ++i) {
            wheelLevel += (target - wheelLevel) * smooth;
            if (wheelLevel < 1.0e-5f && !playing) {
                tick *= 0.99f;
                continue;
            }
            // The wrist strokes: a sharp acceleration of the crank, in time.
            float stroke = 0;
            if (strokes > 0 && patch.buzz > 0) {
                const double b = beat + double(i) / double(rate) * double(beatsPerSecond);
                double ph = b * double(strokes) + double(patch.strokeOffset);
                ph -= std::floor(ph);
                // A quick rise and a slower fall: about 60 ms of push at 120 bpm.
                const float x = float(ph);
                stroke = x < 0.08f ? x / 0.08f : std::max(0.0f, 1.0f - (x - 0.08f) / 0.22f);
                stroke *= stroke;
            }
            const float jitter = 0.03f * noise.white(noiseAt++);   // the wheel is never perfectly true
            const float wheel = wheelLevel * (0.85f + 0.25f * stroke * dsp::clamp01(patch.buzz)) * (1.0f + jitter);
            const float bow = 0.03f + 0.22f * wheel;
            const float on = wheelLevel > 0.002f ? 1.0f : 0.0f;
            const float chanter = strings[Chanter].tick(bow * on) * patch.chanter;
            const float bourdon = strings[Bourdon].tick(bow * on) * patch.bourdon;
            const float mouche = strings[Mouche].tick(bow * on) * patch.mouche;
            const float trompetteForce = strings[Trompette].tick(bow * on);
            // The chien: lifted when the stroke pushes the string's force past its weight;
            // once up it raps the soundboard once per string period.
            const float lift = dsp::clamp01((wheel - 0.62f) * 3.0f) * dsp::clamp01(patch.buzz) * (0.25f + 0.75f * stroke);
            chien += (lift - chien) * 0.004f;
            periodPhase += 1.0f / std::max(8.0f, trompettePeriod);
            float rap = 0;
            if (periodPhase >= 1.0f) {
                periodPhase -= 1.0f;
                rap = chien > 0.02f ? chien * (0.7f + 0.3f * noise.unit(noiseAt++)) : 0.0f;
            }
            raspEnv = std::max(rap, raspEnv * 0.97f);
            // While up, the chien's free foot chatters on the soundboard: the string's
            // force is clipped by the contact (only the part that lifts the foot gets
            // through), a buzz locked to the string's pitch, and each rap adds a short
            // burst of wooden noise.
            const float lifted = trompetteForce * 6.0f;
            const float contact = chien * (lifted > 0.35f ? lifted - 0.35f : (lifted < -0.35f ? lifted + 0.35f : 0.0f));
            const float raspNoise = noise.white(noiseAt++) * raspEnv;
            const float raspSound = rasp.bandpass(raspHigh.hp(contact * 1.5f + raspNoise * 1.2f + rap)) * 4.0f;
            const float trompette = trompetteForce * patch.trompette * (1.0f - 0.35f * chien) + raspSound * patch.trompette;
            // Rosin hiss and wheel rumble, with the wheel's speed.
            const float grit = noise.white(noiseAt++);
            const float friction = (hiss.bandpass(grit) * 0.6f + rumble.lp(grit) * 1.4f) * wheel * patch.noise * 0.08f;
            // A key's tangent ticking on the string.
            const float key = tick > 0.001f ? keyTick.bandpass(noise.white(noiseAt++)) * tick * 0.03f : 0.0f;   // a soft wooden knock
            tick *= 0.99f;
            const float strings2 = chanter * 1.0f + (bourdon + mouche) * 0.7f + trompette * 0.6f + friction + key;
            // The body: a nasal wooden box; drones a little to one side, the melody to the other.
            const float l = strings2 + spread * (bourdon - chanter) * 0.3f, r = strings2 - spread * (bourdon - chanter) * 0.3f;
            const float amount = dsp::clamp01(patch.body);
            float bl = l * (1.0f - 0.5f * amount), br = r * (1.0f - 0.5f * amount);
            for (std::size_t m = 0; m < hurdy::bodyModes.size(); ++m) {
                bl += amount * hurdy::bodyModes[m].gain * body[0][m].bandpass(l);
                br += amount * hurdy::bodyModes[m].gain * body[1][m].bandpass(r);
            }
            const float g = patch.gain * 0.4f;
            left[i] += dsp::finiteOr(bl * g * pan.l * 1.4142f);
            right[i] += dsp::finiteOr(br * g * pan.r * 1.4142f);
        }
        beat += double(n) / double(rate) * double(beatsPerSecond);
    }
    // For the tests: the melody string's pitch now (semitones above the key root).
    float melodyNow() const { return melodySemis; }
    float chienLift() const { return chien; }

    static constexpr float openSemis = 7;   // the melody string's open note: the fifth

private:
    enum String { Chanter, Bourdon, Mouche, Trompette, StringCount };
    std::array<hurdy::BowedWaveguide, StringCount> strings;
    std::array<std::array<dsp::Svf, hurdy::bodyModes.size()>, 2> body;
    dsp::Svf hiss, rasp, keyTick;
    dsp::OnePole rumble, raspHigh;
    HeldNote held {};
    bool playing = false, bodyTuned = false;
    double beat = 0, until = -1;
    float beatsPerSecond = 2, velocity = 0.8f;
    int root = 2;
    float melodySemis = openSemis;
    float wheelLevel = 0, chien = 0, tick = 0, raspEnv = 0, periodPhase = 0, trompettePeriod = 100;
};

} // namespace wb
