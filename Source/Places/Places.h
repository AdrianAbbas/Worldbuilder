#pragma once
// The Places built so far: Castle, Village and Spaceport, each an ensemble of six
// instruments (see Ensemble.h). Every instrument plays from a pattern; generate()
// writes the starting pattern of each, from the Place's seed and settings.
#include "Ensemble.h"

namespace wb {

// Writes notes into a generated pattern.
struct PatternWriter {
    pat::Pattern& out;
    void note(int step, int degree, int length, float velocity, int alter = 0, float probability = 1.0f) {
        pat::Note n;
        n.step = int16_t(step);
        n.degree = int8_t(std::clamp(degree, -60, 60));
        n.alter = int8_t(alter);
        n.length = int16_t(std::max(length, 1));
        n.velocity = uint8_t(std::clamp(int(std::lround(velocity * 127.0f)), 1, 127));
        n.probability = uint8_t(std::clamp(int(std::lround(probability * 100.0f)), 1, 100));
        out.add(n);
    }
};

// Castle: a brass-like drone on the shared chords over a tonic pedal, tuned bells,
// horn calls, a processional timpani pulse, harp arpeggios and a wordless choir, in
// a stone hall with a slow tape echo. Fortress: open fifths, tolling bells, rolled
// harp chords, a half-note processional. Royal Court: triads, carillon runs,
// fanfares in thirds, flowing harp, a march. Besieged: a low trembling drone that
// turns to a semitone cluster as Tension rises, alarm bells, a tense low harp
// ostinato and a driving 3-3-2 timpani figure.
class CastlePlace final : public EnsemblePlace {
public:
    CastlePlace();
    float freeTempo() const override { return 66.0f; }
    float spaceSend() const override { return 0.4f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Drone, Bells, Horns, Pulse, Harp, Choir };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    int addNotes(const PlaceContext& c, int n, std::array<Scheduled, maxScheduled>& notes, int count) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    SynthPart drone, horns;
    ModalPart bells;
    DrumPart timpani;
    PluckPart harp;
    ChoirPart choir;
    dsp::SmallSpace hall;
    dsp::TapeEcho echo;
    float echoSend = 0, hallWet = 0, hallSend = 0;
};

// Village: lute-like plucked ostinatos on the shared chords, a hurdy-gurdy drone on
// the root and fifth, frame drum, hand drum and shaker, wooden marimba tones, a
// recorder and small bell jingles, in a small wooden room through tape warmth.
// Market Day: a lively eighth-note ostinato, shop-bell jingles, recorder phrases.
// Harvest Night: swung sixteenths, a buzzing drone, busy drums, xylophone runs, a
// dance tune and tambourine-like jingles. Quiet Dawn: slow soft arpeggios, a
// heartbeat drum, wind-chime wood, long birdlike recorder notes and a distant bell.
class VillagePlace final : public EnsemblePlace {
public:
    VillagePlace();
    float freeTempo() const override { return 92.0f; }
    float spaceSend() const override { return 0.25f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Lute, HurdyGurdy, HandDrums, Marimba, Recorder, Jingles };
    enum Character { MarketDay, HarvestNight, QuietDawn };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    int addNotes(const PlaceContext& c, int n, std::array<Scheduled, maxScheduled>& notes, int count) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    PluckPart lute;
    SynthPart hurdy;
    DrumPart drums;
    ModalPart marimba, jingles;
    WindPart recorder;
    dsp::SmallSpace room;
    dsp::Warmth warmth;
    float roomSend = 0;
};

// Spaceport: arpeggiated synth pulses over the shared chords, wide gliding pads,
// thruster risers, PA tones with an abstract murmur, an engine sub drone that throbs
// and data blips, in a metal hangar with a ping-pong delay and a shimmer. Busy Hub:
// steady sixteenths, frequent chimes, chattering blips, pass-bys. Abandoned Dock:
// sparse, detuned, dark, flat broken chimes and glitches. Launch: an eight-bar build
// that rises to a tuned impact, with a countdown and accelerating telemetry.
class SpaceportPlace final : public EnsemblePlace {
public:
    SpaceportPlace();
    float freeTempo() const override { return 112.0f; }
    float spaceSend() const override { return 0.3f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Pulses, Pads, Risers, Announce, Engines, Blips };
    static constexpr double buildBars = 8;   // Launch: the length of one build cycle

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    SynthPart pulses, pads, engine;
    NoisePart risers;
    ModalPart pa;
    FmPart blips;
    // Abstract PA murmur: a voiced buzz through two moving formants, never speech.
    dsp::Saw murmurOsc;
    std::array<dsp::Svf, 2> formant;
    dsp::Svf murmurBand;
    dsp::Smoother murmurGain, f1, f2;
    double murmurUntil = -1;
    float murmurTarget = 0, engineLevel = 0;
    std::array<dsp::Rumble, 2> rumble;
    uint64_t rumbleAt = 0;
    dsp::SmallSpace hangar;
    dsp::PingPong delay;
    dsp::Shimmer shimmer;
    float delaySend = 0, shimmerSend = 0;
};

} // namespace wb
