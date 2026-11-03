#pragma once
// The Drama tier (after the performance pass): a game score's vertical layering on top
// of every Place. Score Intensity (and, near its top, the Place's Tension) moves the
// world through four tiers: Calm -> Tension -> Action -> Climax. Calm is the Place as it
// always was. From Tension on, a Drama layer joins the Place and changes gear on the bar
// lines: a pedal-point pulse and distant drums at Tension; at Action a driving ostinato,
// war drums and stabs on the downbeats; at Climax double-time drums, toms, cymbal swells,
// choir and brass stabs, low piano strikes and impacts on the phrase lines.
//
// Architecture (DEVLOG, "Drama tier"): one shared Drama layer per Place slot, themed by
// the Place it plays (a table: which drums, which ostinato, which hits), not six more
// instruments per Place. The host parameters, the session format and every Place stay as
// they were, and a session or scene that does not ask for drama (Drama 0: projects from
// before it, the existing factory scenes) never builds or renders it, so it sounds
// exactly as before and costs nothing. The layer renders only while a tier above Calm
// plays (and its tails ring).
//
// The arrangement uses it: a section can set its tier (or follow its Score Intensity),
// and the transitions get their dramatic sounds (a drum roll and a cymbal swell into a
// Riser or a Drum fill, impacts and stabs landing on the downbeat after Riser, Drum fill
// and Drop). The shared harmonic clock changes gear with the world's tier (music.h).
//
// Nothing here allocates after prepare().
#include "Places.h"

namespace wb::drama {

enum Tier { Calm, Tension, Action, Climax, TierCount };
inline constexpr std::array<const char*, TierCount> tierNames { "Calm", "Tension", "Action", "Climax" };

// The feel of the drama (the Drama Style parameter; Auto: the Place's own).
enum Style { StyleAuto, StyleDriving, StyleEpic, StyleTragic, StyleTriumphant, StyleSuspense, StyleCount };
inline constexpr const char* styleChoices = "Auto|Driving|Epic|Tragic|Triumphant|Suspense";

// The drama amount x (0..1) of a slot: how far above the middle Score Intensity is; the
// Place's own Tension (with Danger) pushes it further once Intensity is above the middle.
// Below or at the middle nothing changes at all.
inline float amountFor(float intensity, float tension, float drama) {
    const float above = std::clamp(2.0f * (intensity - 0.5f), 0.0f, 1.0f);
    const float push = 1.0f + 0.6f * std::max(0.0f, std::clamp(tension, 0.0f, 1.0f) - 0.5f);
    return std::clamp(drama, 0.0f, 1.0f) * std::clamp(above * push, 0.0f, 1.0f);
}
inline int tierFor(float x) { return x < 0.1f ? Calm : (x < 0.45f ? Tension : (x < 0.78f ? Action : Climax)); }

// What a slot's Drama layer is told each chunk (from the engine and the arrangement).
struct Cue {
    float drama = 0;          // the Drama parameter (0: Classic, no layer at all)
    int style = StyleAuto;
    int tier = -1;            // the arrangement section's tier (-1: from Score Intensity)
    int rank = 0;             // the slot's place among the active slots (0 leads: drums)
    // The arrangement's transition under way and the one that led into this section.
    uint8_t transition = 0, arrived = 0;
    float transitionT = -1;
    double transitionLength = 0, sinceStart = 1.0e9;
    bool drumFill = false;
};

// Each Place's dramatic colour.
enum Kit { KitWar, KitTribal, KitMetal, KitHybrid, KitMarch };
enum Ostinato { OstStrings, OstPulse, OstBass, OstPluck, OstPiano };
enum Hit { HitBrass, HitChoir, HitPiano, HitBraam, HitSynth, HitMetal };
struct Theme {
    int kit = KitWar, ostinato = OstStrings, hitA = HitBrass, hitB = HitChoir, style = StyleDriving;
    float drumSemis = -7;     // the djembes pitched down (war drums) or up
    float hall = 0.6f;        // the layer's own space
    const char* text = "";
};
const Theme& theme(int placeType);
// The style that plays for a Place with the Drama Style parameter at `style`.
inline int styleFor(int placeType, int style) { return style > StyleAuto && style < StyleCount ? style : theme(placeType).style; }

// The Drama layer of one slot.
class Layer {
public:
    Layer();
    void prepare(float sampleRate);
    void reset();
    void setSeed(uint32_t key);
    // Adds the layer to left/right (n <= dsp::chunk). `world`: the frame the Place sees;
    // `controls`: the slot's controls (Tension); `beat`/`beatsPerSample`: the Place's
    // beat; `notes`: new notes may start (false: an arrangement's ring-out, keys alone).
    // Returns false (adding nothing) while the layer is silent.
    bool render(int placeType, const WorldFrame& world, const Cue& cue, float tension, double beat, double beatsPerSample, bool sync,
                bool notes, uint64_t clock, float* left, float* right, int n);
    // The tier playing now (latched on the bar lines) and the drama amount.
    int tier() const { return latched; }
    float amount() const { return x; }
    // Notes started since prepare() (for the tests: rhythmic density).
    uint32_t started() const { return startedCount; }
    bool active() const { return awake; }

    enum PartId {
        PDrums, PPerc, PMetal, PSwell, PStrings, PPulse, PBass, PPluck, PLow, PBrass, PChoir, PPiano, PBraam, PSynth, PHands, PartCount
    };

private:
    struct Note {
        int offset = 0, part = 0;
        NoteOn on;
    };
    static constexpr int maxNotes = 96;
    void configure(int placeType, const WorldFrame& world, int style, float tension, float bpm);
    void scheduleStep(int64_t step, int offset, int placeType, int style, const WorldFrame& world, float tension, bool sync, double stepBeat,
                      const Cue& cue, double beatsPerSample);
    void add(int part, int offset, float semis, float velocity, double beat, double lengthBeats, int row = 0, float pan = 2.0f);
    void chord(int part, int offset, int root, int mood, int chordDegree, float velocity, double beat, double length, bool power, int colour = 0);
    float rand(int64_t step, uint32_t salt) const;

    float rate = 48000;
    uint32_t seedKey = 0;
    dsp::Stream noise;
    // Fallbacks (the synthesised parts that play when the bank has no recordings).
    DrumPart drumsSynth, handsSynth, perc, metal;
    NoisePart swell;
    BowedPart stringsSynth;
    SynthPart pulseSynth, bassSynth, low, brass, pianoSynth, braamSynth, synthStabSynth;
    PluckPart pluckSynth;
    ChoirPart choirSynth;
    // The war drums (the low and mid strokes) and the hands (slaps and ghost notes) apart,
    // so the dense hand strokes never cut the booms short.
    HitPart drums { drumsSynth }, hands { handsSynth };
    SamplePart strings { stringsSynth }, pulse { pulseSynth }, bass { bassSynth }, pluck { pluckSynth }, piano { pianoSynth }, braam { braamSynth },
        synthStab { synthStabSynth };
    VoicePart choir { choirSynth, &choirSynth };
    std::array<Part*, PartCount> parts {};
    std::array<float, PartCount> partGain {}, gainNow {};
    std::array<int, PartCount> quiet {};   // samples since the part's last note
    std::array<Note, maxNotes> pending {};
    int count = 0;
    PartFrame frame;
    std::array<float, dsp::chunk> busL {}, busR {}, partL {}, partR {};
    dsp::SmallSpace hall;
    float hallWet = 0.5f;
    dsp::Smoother level;
    int type = -1;
    int latched = Calm, previous = Calm;
    float x = 0;
    bool awake = false, fresh = true, gearChange = false;
    int64_t lastBar = INT64_MIN;
    int idleSamples = 0;
    uint32_t startedCount = 0;
    double lastBeat = -1.0e18;
    float heat = 0;   // x within its tier, for velocities
    uint64_t patchKey = 0;   // what the patches were last set for
    static constexpr float layerTrimDb = 0.0f;   // the layer's level against its Place (Tests "drama")
public:
    // Tests only (the balance probe): the parts that sound (a bit per PartId).
    static inline uint32_t partMask = 0xffffffffu;
};

} // namespace wb::drama
