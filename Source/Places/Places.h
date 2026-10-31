#pragma once
// The Places built so far, each an ensemble of six instruments (see Ensemble.h).
// Every instrument plays from a pattern; generate() writes the starting pattern of
// each, from the Place's seed and settings.
#include "Ensemble.h"
#include "HurdyGurdy.h"
#include "SamplePart.h"
#include <memory>

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

// Village: lute-like plucked ostinatos on the shared chords, a hurdy-gurdy (a bowed
// wheel, drones on the root and fifth, a buzzing trompette, a keyed melody string), frame drum, hand drum and shaker, wooden marimba tones, a
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
    HurdyGurdyPart hurdy;
    DrumPart drums;
    ModalPart marimba, jingles;
    WindPart recorder;
    dsp::SmallSpace room;
    dsp::Warmth warmth;
    float roomSend = 0;
    std::array<dsp::OnePole, 2> shelf;   // Tone's presence above 2 kHz
    float presence = 0;
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
    // The developer's Korg Minilogue (Milestone 15b): the pulsating patch plays the
    // pulses, the harsh square the data blips; the synthesised parts behind them play
    // only without samples.
    SamplePart pulsesRec { pulses }, blipsRec { blips };
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

// Factory / Forge: tuned anvil hits, a piston ostinato, steam swells, a bowed-metal
// drone, gear clicks and a furnace bass, through gritty saturation and a metallic comb
// resonance tuned to the chord, in an iron hall. Steam Mill: a lilting water-wheel
// chug, a ringing anvil marking time, steam swells. Iron Forge: a blacksmith's
// hammer-and-bounce on root and fifth, slow bellows, a dark low drone and bass.
// Assembly Line: a driving sixteenth-note machine of pistons, stamping and ticking
// gears over an eighth-note bass.
class FactoryPlace final : public EnsemblePlace {
public:
    FactoryPlace();
    float freeTempo() const override { return 104.0f; }
    float spaceSend() const override { return 0.3f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Anvil, Pistons, Steam, BowedMetal, Gears, Bass };
    enum Character { SteamMill, IronForge, AssemblyLine };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    ModalPart anvil;
    DrumPart pistons, gears;
    NoisePart steam;
    BowedPart metal;
    SynthPart bass;
    dsp::CombResonance comb;
    dsp::SmallSpace hall;
    std::array<dsp::OnePole, 4> gritTone;   // two poles each side
    float drive = 1, combSend = 0, combWet = 0, hallSend = 0;
};

// City: muted bell chimes, a pulsing synth bass, electric-piano chords, a crowd that
// murmurs on the chord, tuned siren glides and a traffic hum, in a dense short
// reverb with a subtle chorus. Rush Hour: busy eighth-note bass and chords, a loud
// crowd, frequent sirens. Night City: slow jazzy chords and a walking bass, distant
// sirens, a lonely chime. Rain-soaked: soft, blurred and wet, sparse and melancholy.
class CityPlace final : public EnsemblePlace {
public:
    CityPlace();
    float freeTempo() const override { return 96.0f; }
    float spaceSend() const override { return 0.3f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Chimes, Bass, Piano, Crowd, Sirens, Traffic };
    enum Character { RushHour, NightCity, RainSoaked };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    ModalPart chimes;
    SynthPart bass, sirens, traffic;
    FmPart piano;
    ChoirPart crowd;
    std::array<dsp::Rumble, 2> rumble;
    uint64_t rumbleAt = 0;
    dsp::SmallSpace room;
    dsp::Chorus chorus;
    float roomSend = 0, rainLevel = 0, trafficLevel = 0;
    std::array<dsp::Svf, 2> rain;
    std::array<dsp::OnePole, 4> tilt;   // Tone, two poles each side
};

// Forest: a breathy flute, melodic bird calls, wood percussion, kalimba plucks, a
// string pad and rustling leaves, softly diffused in a damped space. Enchanted: a
// floating flute line, glittering kalimba, a glowing pad. Dark Woods: low, sparse
// and uneasy, a knocking log, owl-like calls. Morning Glade: bright birdsong, a
// lilting flute, wood clicks dancing.
class ForestPlace final : public EnsemblePlace {
public:
    ForestPlace();
    float freeTempo() const override { return 78.0f; }
    float spaceSend() const override { return 0.35f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Flute, Birds, Wood, Kalimba, Strings, Rustle };
    enum Character { Enchanted, DarkWoods, MorningGlade };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    WindPart flute, birds, rustle;
    DrumPart wood;
    ModalPart kalimba;
    // The kalimba plays the developer's recorded kalimba (Milestone 15); the modal model
    // behind it plays only without samples.
    SamplePart kalimbaRec { kalimba };
    // The string pad plays the recorded violin's sustains (the hotfix after 15a); the bowed
    // section model behind it plays only without samples.
    BowedPart strings;
    SamplePart stringsRec { strings };
    dsp::Diffuser diffuser;
    dsp::SmallSpace space;
    std::array<dsp::OnePole, 4> tilt;   // Tone, two poles each side
    float spaceSend_ = 0, diffusion = 0;
};

// Alien Hive: modulated organic pulses, glassy FM calls, a chittering rhythm, a
// throbbing bass, formant drones and a swarm, through a ring-modulated shimmer and a
// warped delay. Dormant: slow breathing pulses, a low throb, distant calls. Awakening:
// the pulses quicken, calls answer each other, the chitter starts. Swarm: a dense
// buzzing mass, racing chitter and pulses, the bass throbbing fast.
class HivePlace final : public EnsemblePlace {
public:
    HivePlace();
    float freeTempo() const override { return 90.0f; }
    float spaceSend() const override { return 0.35f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Pulses, Calls, Chitter, Throb, Formants, Swarm };
    enum Character { Dormant, Awakening, Swarming };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    SynthPart pulses, throb;
    FmPart calls, swarm;
    DrumPart chitter;
    ChoirPart formants;
    dsp::Shimmer shimmer;
    dsp::WarpDelay warp;
    dsp::Phasor ring;   // the shimmer's ring modulator
    std::array<dsp::OnePole, 4> tilt;
    float shimmerSend = 0, ringDepth = 0, delaySend = 0;
};

// Temple: a formant choir pad, gongs, singing bowls, a low chant drone, temple bells
// and zither plucks, in a long shimmer reverb. Sacred Dawn: a bright choir, bowls and
// rippling zither, a morning bell. Ancient Rite: a deep chant, a gong on the bar,
// bells and a processional zither ostinato. Forsaken: hollow and dark, a lone gong, a
// fading chant, broken bells.
class TemplePlace final : public EnsemblePlace {
public:
    TemplePlace();
    float freeTempo() const override { return 60.0f; }
    float spaceSend() const override { return 0.4f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Choir, Gongs, Bowls, Chant, Bells, Zither };
    enum Character { SacredDawn, AncientRite, Forsaken };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    ChoirPart choir, chant;
    ModalPart gongs, bowls, bells;
    PluckPart zither;
    dsp::Shimmer shimmer;
    dsp::SmallSpace hall;
    std::array<dsp::OnePole, 4> tilt;
    float shimmerSend = 0, hallSend = 0;
};

// Cavern / Mine: tuned drips, a sub drone, echoing metal taps, a bowed low bass,
// crystal resonances and a distant rumble, in a long dark reverb with a slapback.
// Crystal Cave: glassy crystals ringing, bright drips, a soft bowed bass. Deep Mine:
// pick-axe taps echoing, a heavy rumble and a dark drone. Flooded: dense dripping,
// a submerged, muffled world, slow bass swells.
class CavernPlace final : public EnsemblePlace {
public:
    CavernPlace();
    float freeTempo() const override { return 64.0f; }
    float spaceSend() const override { return 0.45f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Drips, SubDrone, Taps, BowedBass, Crystals, Rumble };
    enum Character { CrystalCave, DeepMine, Flooded };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    DripPart drips;
    SynthPart drone;
    ModalPart taps, crystals;
    BowedPart bass;
    DrumPart rumble;
    dsp::SmallSpace cave;
    dsp::Slapback slap;
    std::array<dsp::OnePole, 2> murk;   // under water
    std::array<dsp::OnePole, 4> tilt;
    float caveSend = 0, slapSend = 0, murkiness = 0;
};

// Ruins: reversed piano-like fragments, a lo-fi music box, broken bells, cold wind
// harmonics, a ghostly pad and sparse low toms, through a lo-fi decay and reverse
// echoes in a roofless hall. Overgrown: warm and worn, the music box still turning, a
// soft heartbeat. Haunted: a ghostly choir, long reverse swells, a slow uneven march.
// Desolate: wind whistling through the stones, tolling cracked bells, a lone tom.
class RuinsPlace final : public EnsemblePlace {
public:
    RuinsPlace();
    float freeTempo() const override { return 70.0f; }
    float spaceSend() const override { return 0.4f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { PianoShards, MusicBox, BrokenBells, Wind, ViolinTextures, LowToms };
    enum Character { Overgrown, Haunted, Desolate };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    FmPart shards;
    ModalPart box, bells;
    WindPart wind;
    // Violin textures (Milestone 15): the developer's recorded violin, bowed, plucked and
    // struck with the bow's wood. The ghostly choir pad it replaced stays behind it and
    // plays only without samples.
    ChoirPart ghost;
    SamplePart violin { ghost };
    DrumPart toms;
    dsp::LoFi lofi;
    dsp::ReverseEcho reverse, reverseViolin;
    dsp::SmallSpace hall;
    std::array<dsp::OnePole, 4> tilt;
    float reverseSend = 0, hallSend = 0;
};

// Spaceship Interior: an engine drone, a life-support pulse, computer tone arpeggios,
// airlock swells, hull resonances and a soft alarm motif, in a confined metallic room
// with a filtered delay. Cruising: a calm hum, gentle arpeggios, the pumps breathing.
// Red Alert: a throbbing engine, racing computers, a two-tone alarm every bar. Derelict:
// a sputtering engine, groaning hull, a stuttering computer and a dying alarm.
class SpaceshipPlace final : public EnsemblePlace {
public:
    SpaceshipPlace();
    float freeTempo() const override { return 100.0f; }
    float spaceSend() const override { return 0.25f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Engine, LifeSupport, Computer, Airlock, Hull, Alarm };
    enum Character { Cruising, RedAlert, Derelict };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    SynthPart engine;
    DrumPart life;
    FmPart computer, alarm;
    NoisePart airlock;
    BowedPart hull;
    dsp::SmallSpace room;
    dsp::CombResonance ring;
    dsp::FilteredDelay delay;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    float roomSend = 0, ringSend = 0, delaySend = 0, presence = 0, dry = 1;
};

// Biodome: lush pads, bright mallet arpeggios, synthetic birdsong, water-like plucks,
// a ventilation hum and a gentle shaker, in a bright airy reverb with a gentle chorus.
// Thriving: running mallets, birds everywhere, a busy shaker. Night Cycle: dim pads,
// slow mallets, a few birds and crickets. Failing: the power sags, the pads waver, the
// vents sputter and the birds fall quiet.
class BiodomePlace final : public EnsemblePlace {
public:
    BiodomePlace();
    float freeTempo() const override { return 88.0f; }
    float spaceSend() const override { return 0.35f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Pads, Mallets, Birdsong, WaterPlucks, Vents, Shaker };
    enum Character { Thriving, NightCycle, Failing };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    SynthPart pads;
    // The pads are the developer's Minilogue soft pad (Milestone 15b).
    SamplePart padsRec { pads };
    ModalPart mallets;
    WindPart birds, vents;
    DripPart water;
    // The water plucks are the developer's recorded kalimba, made watery (Milestone 15);
    // the synthesised drops behind them play only without samples.
    SamplePart waterRec { water };
    DrumPart shaker;
    dsp::SmallSpace dome;
    dsp::Chorus chorus;
    std::array<dsp::OnePole, 4> tilt;
    float domeSend = 0;
};

// Sea Ship: creaking wood, concertina chords, rope-pull swells, a ship's bell, a wave
// drone and gull calls, rolling with the sea in a wide, wet reverb. Calm Seas: a
// sailors' oom-pah, the bell telling the time, gulls overhead. Storm: the ship thrown
// about, ropes straining, the bell clanging, a heaving sea. Ghost Ship: a slow lament
// in the fog, a tolling bell, the timbers groaning.
class SeaShipPlace final : public EnsemblePlace {
public:
    SeaShipPlace();
    float freeTempo() const override { return 84.0f; }
    float spaceSend() const override { return 0.35f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Creaks, Concertina, RopeSwells, ShipBell, Waves, Gulls };
    enum Character { CalmSeas, Storm, GhostShip };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    DrumPart creaks;
    SynthPart concertina;
    NoisePart ropes;
    ModalPart bell;
    WindPart waves, gulls;
    dsp::Roll roll;
    dsp::SmallSpace sea;
    std::array<dsp::OnePole, 4> tilt;
    float seaSend = 0;
};

// Suburbs: warm electric-piano chords, a soft pop pulse, wind chimes, a sprinkler-like
// hi-hat, a music-box motif and a mellow bass, through warm tape in a short room.
// Sunny Afternoon: an easy off-beat comp, the sprinkler ticking round. Evening: long
// soft sevenths, chimes on the porch. Uncanny: the same chord held too long, a slowed
// music box, everything wavering like stretched tape.
class SuburbsPlace final : public EnsemblePlace {
public:
    SuburbsPlace();
    float freeTempo() const override { return 96.0f; }
    float spaceSend() const override { return 0.25f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { EPiano, PopPulse, WindChimes, Sprinkler, MusicBox, Bass };
    enum Character { SunnyAfternoon, Evening, Uncanny };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    FmPart piano;
    SynthPart pulse, bass;
    ModalPart chimes, box;
    DrumPart sprinkler;
    dsp::Warmth tape;
    dsp::SmallSpace room;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    float roomSend = 0, presence = 0;
};

// Mall: smooth electric-piano chords, vibraphone melodies with a motor tremolo, PA
// chime dings, an escalator hum, fountain shimmer and a soft drum groove, in a glossy
// reverb with a slight detune. Busy Weekend: a light shuffle, a running vibraphone,
// frequent chimes. Empty Night: long ninth chords, a lone vibraphone, the escalator and
// the fountain. Dead Mall: the vaporwave option, slowed right down, detuned flat and
// wobbling, a half-time groove in a dim hall.
class MallPlace final : public EnsemblePlace {
public:
    MallPlace();
    float freeTempo() const override { return 92.0f; }
    float spaceSend() const override { return 0.3f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { EPiano, Vibraphone, PAChimes, Escalator, Fountain, Drums };
    enum Character { BusyWeekend, EmptyNight, DeadMall };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    FmPart piano;
    ModalPart vibes, chimes;
    SynthPart escalator;
    WindPart fountain;
    DrumPart drums;
    dsp::SmallSpace hall;
    dsp::Detune detune;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    float hallSend = 0, presence = 0, motorPhase = 0, motorHz = 5, motorDepth = 0, dry = 1;
};

// Military Camp: snare march patterns, bugle calls on the notes of the harmonic
// series, a low drum pulse, tuned radio blips, metallic clanks and a tense string
// ostinato, in a tight room with a slapback off the barracks and a gritty radio
// filter. Morning Drill: a brisk march, reveille, the camp at work. On Alert: a
// driving ostinato, urgent radio traffic, drum rolls, a call to arms. Night Watch:
// a slow pulse, a distant taps over the radio, sparse clanks in the dark.
class MilitaryPlace final : public EnsemblePlace {
public:
    MilitaryPlace();
    float freeTempo() const override { return 112.0f; }
    float spaceSend() const override { return 0.25f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { SnareMarch, Bugle, LowDrums, RadioBlips, Clanks, Strings };
    enum Character { MorningDrill, OnAlert, NightWatch };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    DrumPart snare, drums, clanks;
    SynthPart bugle;
    FmPart radio;
    BowedPart strings;
    dsp::SmallSpace room;
    dsp::Slapback slap;
    dsp::Radio band;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    float roomSend = 0, presence = 0, bugleRadio = 0, dry = 1;
};

// Beach: warm guitar plucks, steel-drum tones, wave swells, a shaker and clave, a
// breezy pad and gull calls, in an open, airy space with a gentle chorus. Tropical Day:
// a lilting strum, a dancing steel-drum tune, a son clave. Sunset: slow fingerpicked
// arpeggios, a few long steel-drum notes, the pad glowing. Stormy Shore: a nervous
// tremolo, the surf rising, rain on the shaker, the gulls blown about.
class BeachPlace final : public EnsemblePlace {
public:
    BeachPlace();
    float freeTempo() const override { return 100.0f; }
    float spaceSend() const override { return 0.35f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Guitar, SteelDrum, Waves, Shaker, BreezePad, Gulls };
    enum Character { TropicalDay, Sunset, StormyShore };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    PluckPart guitar;
    ModalPart steel;
    WindPart waves, gulls;
    DrumPart shaker;
    SynthPart pad;
    dsp::SmallSpace air;
    dsp::Chorus chorus;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    float airSend = 0, presence = 0, dry = 1;
};

// Desert: oud plucks, frame drums (doum, tek and a riq's jingles), a breathy ney, a
// tanpura-like drone, wind whistles and sparse metal chimes, in a wide, dry space with a
// long, sparse echo. Oasis: a lilting maqsum, ornamented oud phrases, the ney singing.
// Endless Dunes: a slow, spacious taqsim over the drone, far-off drums, long echoes.
// Sandstorm: a driving rhythm, tremolo oud, the wind howling, the chimes thrashing.
class DesertPlace final : public EnsemblePlace {
public:
    DesertPlace();
    float freeTempo() const override { return 88.0f; }
    float spaceSend() const override { return 0.3f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Oud, FrameDrums, Ney, Tanpura, Wind, Chimes };
    enum Character { Oasis, EndlessDunes, Sandstorm };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    PluckPart oud;
    // The oud plays the developer's classical-guitar recordings (Milestone 15); the
    // synthesised oud behind it plays only without samples.
    SamplePart oudRec { oud };
    DrumPart drums;
    WindPart ney, wind;
    SynthPart tanpura;
    ModalPart chimes;
    dsp::SmallSpace space;
    dsp::TapeEcho echo;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    float roomSend = 0, echoSend = 0, presence = 0, dry = 1;
};

// Palace: harpsichord arpeggios, a string-ensemble pad, fanfare brass, a glockenspiel,
// courtly timpani and choir swells, in a grand hall. Grand Ball: a waltz (three beats
// over the 4/4 grid, four waltz bars to three of the song's), Alberti figures, a
// sparkling glockenspiel. Throne Room: stately rolled chords, fanfares with timpani
// rolls, a full choir. Fallen Palace: a broken, slow harpsichord, thin detuned strings,
// a lone distant call, a ghostly choir in an empty hall.
class PalacePlace final : public EnsemblePlace {
public:
    PalacePlace();
    float freeTempo() const override { return 96.0f; }
    float spaceSend() const override { return 0.35f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Harpsichord, Strings, Fanfare, Glockenspiel, Timpani, Choir };
    enum Character { GrandBall, ThroneRoom, FallenPalace };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    PluckPart harpsichord;
    BowedPart strings;
    SynthPart brass;
    ModalPart glock;
    DrumPart timpani;
    ChoirPart choir;
    dsp::SmallSpace hall;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    float hallSend = 0, hallWet = 0, presence = 0, dry = 1;
};

// Tavern: lute strumming, a frame-drum pulse, a fiddle playing tunes over its open
// string, tuned glass clinks, squeezebox chords and stomps and claps, in a warm wooden
// room with tape warmth. Lively Night: a reel, everyone stamping along. Quiet Corner:
// slow arpeggios, a lone fiddle air, a few glasses by the fire. Brawl: everything at
// once and too fast, the fiddler sawing, glasses flying, the drum and stomps chaotic.
class TavernPlace final : public EnsemblePlace {
public:
    TavernPlace();
    float freeTempo() const override { return 116.0f; }
    float spaceSend() const override { return 0.25f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { LuteStrum, FrameDrum, Fiddle, GlassClinks, Squeezebox, Stomps };
    enum Character { LivelyNight, QuietCorner, Brawl };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    bool shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    // The lute and the fiddle play the developer's recordings (Milestone 15: the classical
    // guitar and the violin); the synthesised models behind them play only without samples.
    PluckPart lute;
    DrumPart drum, stomps;
    BowedPart fiddle;
    SamplePart luteRec { lute }, fiddleRec { fiddle };
    ModalPart glasses;
    SynthPart squeezebox;
    dsp::SmallSpace room;
    dsp::Warmth warmth;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    float roomSend = 0, presence = 0, dry = 1;
};

// Space Station: a rotating-hull drone that turns slowly around the listener,
// docking-clamp hits, comms-beep motifs, an airflow pad, a reactor pulse bass and a
// zero-g shimmer lead that glides, in a sealed metallic room with a filtered delay.
// Routine Orbit: calm and steady, the reactor ticking over, beeps now and then.
// Emergency: the clamps slamming, a racing reactor, urgent two-note calls. Abandoned:
// the hull groaning alone, air hissing through a breach, a lead lost in the dark.
class StationPlace final : public EnsemblePlace {
public:
    StationPlace();
    float freeTempo() const override { return 94.0f; }
    float spaceSend() const override { return 0.3f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { HullDrone, Clamps, Comms, Airflow, Reactor, Shimmer };
    enum Character { RoutineOrbit, Emergency, Abandoned };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    SynthPart hull, reactor;
    // The reactor pulse is the developer's Minilogue static-y bass (Milestone 15b).
    SamplePart reactorRec { reactor };
    DrumPart clamps;
    ModalPart comms;
    WindPart air;
    FmPart lead;
    dsp::SmallSpace room;
    dsp::CombResonance seal;
    dsp::FilteredDelay delay;
    dsp::Phasor rotation;   // the hull's slow turn
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    float roomSend = 0, sealSend = 0, delaySend = 0, presence = 0, dry = 1, turn = 0.5f;
};

// Cyberpunk Megacity: a gritty synth bass, a neon FM arpeggio, a rain-soaked pad, a
// distorted drum machine, glitch stabs and the hum of a distant megastructure, in a
// wet neon reverb with a bitcrusher and a ping-pong delay. Night Market: a lazy
// half-time groove, the arp glinting, stabs answering. Corporate Tower: cold and
// clean, a steady four-on-the-floor pulse, glassy arps far up. Riot: a breakbeat, the
// bass snarling in sixteenths, crushed stabs everywhere.
class MegacityPlace final : public EnsemblePlace {
public:
    MegacityPlace();
    float freeTempo() const override { return 100.0f; }
    float spaceSend() const override { return 0.3f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { GritBass, NeonArp, RainPad, Drums, Stabs, Hum };
    enum Character { NightMarket, CorporateTower, Riot };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    SynthPart bass, pad, stabs;
    // The developer's Minilogue (Milestone 15b): the static-y bass is the grit bass, the
    // cyberpunk patch the glitch stabs.
    SamplePart bassRec { bass }, stabsRec { stabs };
    FmPart arp;
    DrumPart drums;
    BowedPart hum;
    dsp::SmallSpace neon;
    dsp::LoFi crush;
    dsp::PingPong pingpong;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    std::array<dsp::Svf, 2> rain;        // the rain on the pad: a bright hiss
    float neonSend = 0, crushMix = 0, pingSend = 0, presence = 0, dry = 1, drive = 1, rainLevel = 0;
};

// Starship Bridge: command-console arpeggios, a warp-core drone, a tactical pulse,
// alert brass swells, abstract crew chatter over the intercom and sensor pings, in a
// wide cinematic hall with risers. Cruising: calm consoles, a slow core, pings far
// out. Battle Stations: a driving tactical pulse, brass alerts every two bars, busy
// comms. Warp Jump: an eight-bar build, the core spinning up under a riser, the
// brass swelling to the jump on the downbeat.
class BridgePlace final : public EnsemblePlace {
public:
    BridgePlace();
    float freeTempo() const override { return 108.0f; }
    float spaceSend() const override { return 0.35f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Consoles, WarpCore, Tactical, Alerts, Chatter, Pings };
    enum Character { Cruising, BattleStations, WarpJump };
    static constexpr double buildBars = 8;   // Warp Jump: one build cycle

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    FmPart consoles;
    SynthPart core, brass;
    // The warp core is the developer's Minilogue pulsating patch, held (Milestone 15b).
    SamplePart coreRec { core };
    DrumPart tactical;
    ChoirPart chatter;
    ModalPart pings;
    dsp::SmallSpace hall;
    dsp::Radio intercom;
    dsp::PingPong echo;
    std::array<dsp::Svf, 2> riser;
    dsp::Smoother riserGain;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    float hallSend = 0, echoSend = 0, presence = 0, dry = 1, riserTarget = 0, width = 1;
};

// Asteroid Mining Colony: a drilling ostinato, a low rock-grind bass, tuned metal
// clanks, a dust-hiss texture, a worker-hymn pad and a lonely beacon lead, in a
// gritty industrial room with a slapback off the rock. Shift Change: the drills
// winding down, the hymn rising, clanks of tools put away. Cave-in: the drills
// stalling and racing, the bass grinding, metal falling. Night Shift: one drill far
// off, dust hissing, the beacon calling over the hymn.
class MiningPlace final : public EnsemblePlace {
public:
    MiningPlace();
    float freeTempo() const override { return 96.0f; }
    float spaceSend() const override { return 0.3f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Drill, GrindBass, Clanks, Dust, Hymn, Beacon };
    enum Character { ShiftChange, CaveIn, NightShift };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    FmPart drill;
    // The drill is the developer's Minilogue harsh square (Milestone 15b).
    SamplePart drillRec { drill };
    SynthPart bass;
    ModalPart clanks;
    WindPart dust, beacon;
    ChoirPart hymn;
    dsp::SmallSpace room;
    dsp::Slapback slap;
    dsp::Warmth grit;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    float roomSend = 0, slapSend = 0, presence = 0, dry = 1, drive = 1;
};

// Alien Ruins: eroded crystal tones, ancient-machine pulses, a reversed alien choir, a
// sub-harmonic drone, ritual percussion and a whispering texture, in a vast, strange
// reverb with a warped delay. Dormant: the machines asleep, a crystal now and then,
// the drone breathing. Reawakening: the pulses stirring and quickening, glyphs
// chiming in answer. Guardian: something wakes: a ritual rhythm, the choir rising, the
// drone pressing down.
class AlienRuinsPlace final : public EnsemblePlace {
public:
    AlienRuinsPlace();
    float freeTempo() const override { return 76.0f; }
    float spaceSend() const override { return 0.4f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Crystals, Pulses, Choir, SubDrone, Ritual, Whispers };
    enum Character { Dormant, Reawakening, Guardian };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    ModalPart crystals;
    FmPart pulses;
    ChoirPart choir;
    SynthPart drone;
    DrumPart ritual;
    WindPart whispers;
    dsp::Shimmer vast;
    dsp::WarpDelay warp;
    dsp::ReverseEcho reverse;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    dsp::Smoother whisper;   // the whispers' syllables
    float vastSend = 0, warpSend = 0, reverseSend = 0, presence = 0, dry = 1, whisperTarget = 1;
};

// Nebula / Deep Space: vast evolving pads, a cosmic-wind texture, slow star-pulse
// bells, a sub drone, granular stardust and solar-flare swells, in an endless shimmer
// reverb. Drift: slow and weightless, the pads turning. Stellar Nursery: bright and
// glittering, stardust everywhere, flares igniting. Void: almost nothing, a dark drone,
// a bell in the distance, the wind between the stars.
class NebulaPlace final : public EnsemblePlace {
public:
    NebulaPlace();
    float freeTempo() const override { return 66.0f; }
    float spaceSend() const override { return 0.45f; }
    void generate(int element, const GenInputs& in, pat::Pattern& out) const override;
    enum Element { Pads, Wind, StarBells, SubDrone, Stardust, Flares };
    enum Character { Drift, StellarNursery, Void };

private:
    void prepareFx() override;
    void resetFx() override;
    void configure(const PlaceContext& c) override;
    void mix(const PlaceContext& c, float* left, float* right, int n) override;

    SynthPart pads;
    // The pads are the developer's Minilogue soft pad (Milestone 15b).
    SamplePart padsRec { pads };
    WindPart wind;
    ModalPart bells;
    BowedPart drone;
    DripPart dust;
    NoisePart flares;
    dsp::Shimmer infinite;
    dsp::SmallSpace space;
    std::array<dsp::OnePole, 4> tilt;
    std::array<dsp::OnePole, 2> shelf;   // presence
    std::array<dsp::Svf, 2> windSweep;   // the cosmic wind's slowly sweeping resonance
    float infiniteSend = 0, spaceSend_ = 0, presence = 0, dry = 1, wet = 1;
};

// A new engine for a built Place (nullptr for the others).
std::unique_ptr<Place> makePlace(int type);

} // namespace wb
