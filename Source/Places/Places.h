#pragma once
// The Places built so far: Castle, Village and Spaceport. Their rhythmic and
// melodic elements play from patterns (see Pattern.h); generate() writes the
// starting pattern of each, from the Place's seed and settings.
#include "Place.h"

namespace wb {

// Castle: low brass-like drones on the shared chords over a tonic pedal, distant
// bells tuned to the key, horn calls, a slow processional timpani pulse, a stone
// hall and a slow tape echo. Fortress: open fifths, tolling bells, half-note pulse.
// Royal Court: full triads, carillon runs, fanfares, a march. Besieged: a low,
// trembling drone with a semitone cluster as Tension rises, alarm bells and a
// driving 3-3-2 ostinato.
class CastlePlace final : public Place {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setSeed(uint32_t key) override;
    void render(const PlaceContext& context, float* left, float* right, int n) override;
    float freeTempo() const override { return 66.0f; }
    float spaceSend() const override { return 0.4f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;

    static constexpr int drones = 3, bells = 6, horns = 2, drums = 4;
    enum Element { Drones, Bells, Horns, Pulse, Hall };

private:
    void trigger(const PlaceContext& c, const Scheduled& x);
    void playBell(const PlaceContext& c, int semitones, float velocity, float decay, float pan);

    float rate = 48000;
    std::array<dsp::SynthVoice, drones> drone;
    std::array<dsp::Bell, bells> bell;
    std::array<dsp::SynthVoice, horns> horn;
    std::array<double, horns> hornRelease {};
    std::array<dsp::Drum, drums> drum;
    dsp::Stream noise, drift;
    dsp::SmallSpace hall;
    dsp::TapeEcho echo;
    std::array<dsp::OnePole, 2> bellTone;
    uint64_t noiseAt = 0;
    int bellVoices = bells;
};

// Village: warm plucked ostinatos on the shared chords, a folk drone on the root
// and fifth, frame drum, hand drum and shaker, wooden marimba-like tones, a small
// wooden room and tape warmth. Market Day: a lively eighth-note ostinato. Hearth:
// slow, soft arpeggios, a heartbeat drum and wind-chime wood. Festival: swung
// sixteenths, busy percussion and xylophone runs.
class VillagePlace final : public Place {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setSeed(uint32_t key) override;
    void render(const PlaceContext& context, float* left, float* right, int n) override;
    float freeTempo() const override { return 92.0f; }
    float spaceSend() const override { return 0.25f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;

    static constexpr int plucks = 8, drones = 2, drums = 6, woods = 5;
    enum Element { Plucks, Drone, HandDrums, Wood, Tape };

private:
    void trigger(const PlaceContext& c, const Scheduled& x);
    void playWood(const PlaceContext& c, int semitones, float velocity, float pan);

    float rate = 48000;
    std::array<dsp::Pluck, plucks> pluck;
    std::array<dsp::SynthVoice, drones> drone;
    std::array<dsp::Drum, drums> drum;
    std::array<dsp::Bell, woods> wood;
    dsp::Stream noise, drift;
    dsp::SmallSpace room;
    dsp::Warmth warmth;
    std::array<dsp::OnePole, 2> dcBlock;
    uint64_t noiseAt = 0;
    int pluckVoices = plucks;
};

// Spaceport: arpeggiated synth pulses over the shared chords, wide detuned pads,
// thruster risers, PA chimes with an abstract murmur, a throbbing engine hum, a
// metal hangar and a ping-pong delay. Busy Hub: steady sixteenths, frequent chimes
// and pass-bys. Abandoned Dock: sparse, detuned, broken chimes, dark. Launch: an
// eight-bar build cycle that rises to a tuned impact.
class SpaceportPlace final : public Place {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setSeed(uint32_t key) override;
    void render(const PlaceContext& context, float* left, float* right, int n) override;
    float freeTempo() const override { return 112.0f; }
    float spaceSend() const override { return 0.3f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;

    static constexpr int arps = 6, pads = 4, chimes = 4, engines = 2;
    enum Element { Pulses, Pads, Risers, Announce, Engines };

private:
    void trigger(const PlaceContext& c, const Scheduled& x);

    struct Riser {
        bool active = false;
        int shape = 0;   // 0 pass-by (rise and fall), 1 build to an impact, 2 falling sweep
        double start = 0, length = 8;
        float panFrom = -0.7f, panTo = 0.7f, size = 1;
        std::array<dsp::Svf, 2> band;
        dsp::Smoother gain;
        float hz = 200, pan = 0;
    };

    float rate = 48000;
    std::array<dsp::SynthVoice, arps> arp;
    std::array<dsp::SynthVoice, pads> pad;
    std::array<dsp::Bell, chimes> chime;
    std::array<dsp::SynthVoice, engines> engine;
    dsp::Drum impact, beep;
    Riser riser;
    // Abstract PA murmur: a voiced buzz through two moving formants, never speech.
    dsp::Saw murmurOsc;
    std::array<dsp::Svf, 2> formant;
    dsp::Svf murmurBand;
    dsp::Smoother murmurGain, f1, f2;
    double murmurUntil = -1;
    std::array<dsp::Rumble, 2> rumble;
    dsp::Stream noise, drift, hits;
    dsp::SmallSpace hangar;
    dsp::PingPong delay;
    uint64_t noiseAt = 0;
    int arpVoices = arps;
};

} // namespace wb
