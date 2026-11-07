#pragma once
// Arrangement templates and Auto-arrange (Milestone 11b).
//
// A template is a ready-made set of sections for a kind of game cue (Classic Arc,
// Exploration Loop, Battle, Boss Fight, Town Theme, Cinematic Reveal): names, colours,
// lengths, how often each plays, and the End.
//
// Auto-arrange fills an arrangement's instrument grid, pattern variants, Score
// Intensity ramps and transitions for its sections and the active Places, the way a
// composer would: sparse beds in intros, instruments entering in order of their role
// (drone, then bass, rhythm, melody, accents) during build-ups, everything at a
// climax, thinning out in wind-downs. What each section is comes from its name (or,
// for a name of its own, from where it sits and its intensity). The World Seed picks
// between equally good choices, so the same seed always gives the same arrangement.
#include "Arrangement.h"
#include <functional>

namespace wb::arr {

enum Template { ClassicArc, ExplorationLoop, Battle, BossFight, TownTheme, CinematicReveal,
                // The Drama tier (after the performance pass): dramatic cues with arcs.
                Chase, Siege, FinalStand, Lament, TemplateCount };
inline constexpr std::array<const char*, TemplateCount> templateNames { "Classic Arc", "Exploration Loop", "Battle", "Boss Fight", "Town Theme",
                                                                        "Cinematic Reveal", "Chase", "Siege", "Final Stand", "Lament" };

// An instrument's role in its Place, in the order instruments enter a build-up.
enum Role { RoleBed, RoleTexture, RoleBass, RoleRhythm, RoleMelody, RoleAccent, RoleCount };
inline constexpr std::array<const char*, RoleCount> roleNames { "bed", "texture", "bass", "rhythm", "melody", "accent" };
int roleOf(int placeType, int element);

// What a section does in the cue.
enum Kind { KindIntro, KindBuild, KindPeak, KindCalm, KindBreak, KindOutro, KindLoop, KindBridge, KindCount };
inline constexpr std::array<const char*, KindCount> kindNames { "intro", "build", "peak", "wind-down", "break", "outro", "loop", "bridge" };
int kindOf(const Arrangement& a, int index);

// A template's sections (every instrument on, variant A), ready for Auto-arrange: long
// scoring sections that evolve (the Drama tier rework), or (`legacy`) the short ones from
// before it, which the older factory scenes keep. makeTemplate(which) is fitted to the
// default Length (Medium) at 120 BPM (Orchestra.h: makeTemplate(which, seconds, bpm));
// makeTemplate(which, false) gives the scoring proportions as written.
Arrangement makeTemplate(int which);
Arrangement makeTemplate(int which, bool legacy);
// The bars a kind of section is given at least for scoring (Auto-arrange): intros, breaks
// and endings 8, fights and loops 32, the rest 16.
int scoringBars(int kind);
// An evolving section's variant shift on its phrase k of `phrases` (-1 sparser, +1 busier).
int phraseShiftOf(const Section& s, int kind, int k, int phrases);
// Auto-arrange: `a` with its grid, variants, ramps and transitions filled in for the
// Places in `types` (per slot, NoPlace for none), then orchestrated (Orchestra.h: the part
// budget and the voices). Names, lengths, plays, colours, Locations, overrides and the End
// stay as they are (the Length control fits the lengths first); every section evolves.
Arrangement autoArrange(const Arrangement& a, const std::array<int, placeSlots>& types, uint32_t seed);
// Auto-arrange as before the rework (lengths kept, no evolution, the Drama instruments'
// cells left as they are): the older factory scenes.
Arrangement autoArrangeLegacy(const Arrangement& a, const std::array<int, placeSlots>& types, uint32_t seed);

// Milestone 16: Auto-arrange with clips. Where an instrument plays in an intro,
// build-up, climax, wind-down, break or outro, it gets a clip of its own, made from the
// pattern variant it plays and shaped for the section: intros sparse and lower;
// build-ups rising (thin at first, then more notes, rhythms tightening, pitch climbing,
// a run into the next section in the last bar); climaxes full (louder, the melody
// doubled an octave up on its strong notes); wind-downs and outros thinning out and
// softening; breaks sparse. Loops and bridges stay linked (a game loop stays as its
// pattern). Locked notes are always kept. Everything stays editable.
struct ClipSource {
    // The pattern an instrument's variant plays (nullptr: none) and whether it is edited.
    std::function<const pat::Pattern*(int slot, int element, int variant, bool& edited)> pattern;
    int scaleSize = 7;   // notes per octave of the current Mood
};
Arrangement autoArrange(const Arrangement& a, const std::array<int, placeSlots>& types, uint32_t seed, const ClipSource& clips);
// The Drama design pass: Auto-arrange for an arrangement the orchestra scores live
// (Arrangement::autoOrchestra): the grid keeps the score's suggestions where the orchestra did not
// choose and its choices (with their clips) where it did, and each section the loudness it was
// written with (refDb), so that played() gives exactly what autoArrange() gives.
Arrangement autoArrangeLive(const Arrangement& a, const std::array<int, placeSlots>& types, uint32_t seed, const ClipSource& clips);
// Auto-arrange as before the Drama tier, with clips (the Drama setting Off).
Arrangement autoArrangeLegacy(const Arrangement& a, const std::array<int, placeSlots>& types, uint32_t seed, const ClipSource& clips);
void addSectionClips(Arrangement& a, const std::array<int, placeSlots>& types, uint32_t seed, const ClipSource& clips);
// An arrangement from before the design pass scored live from now on (ARRANGE's AUTO): sections
// written by hand keep the parts the composer turned off (as overrides); sections scored once
// keep their loudness reference (what they kept plus their make-up).
Arrangement liveFrom(const Arrangement& a, const std::array<int, placeSlots>& types);
// The Drama tier (after the performance pass): an arrangement's arcs. Each section gets
// its tier (intros and breaks calm, build-ups at Tension, the first climax at Action and
// the later ones (or one after a build-up) at Climax, wind-downs back to Tension or
// Calm), its Score Intensity to match, and the transitions a game score changes gear
// with: a Riser from a build-up into a climax, a Drop from a climax into a break, a Drum
// fill from a break back into the fight, a Drop between two climaxes. Applied by
// Auto-arrange when Drama is above 0, and by the dramatic scenes and Inspire.
void dramatize(Arrangement& a, uint32_t seed);
// The Drama design pass: risers by rules. biggerSection: `to` is a bigger section than `from`
// (a climax after a build-up, a fight after a break; a higher tier of the same kind).
// riserAllowed: section i may end in a Riser (or a Reverse swell): it is 16 bars or more, a
// bigger section follows, and the transition into it was not one. disciplineRisers: every
// Riser the rules refuse becomes a Drum fill (into a climax) or a Crossfade, the others at most
// riserMaxBars long. Auto-arrange and dramatize() apply it.
bool biggerSection(const Arrangement& a, int from, int to);
bool riserAllowed(const Arrangement& a, int i);
void disciplineRisers(Arrangement& a);

// The Drama tier rework: how the Drama meter and a section's own evolution shape what the
// arrangement plays at a song position (applied after Arrangement::evaluate; a pure
// function of the arrangement, the position and these inputs).
struct Shaping {
    float drama = 0;            // the Drama meter (0: the Classic score, nothing changes)
    float intensityOffset = 0;  // Score Intensity's offset from its middle (tierless sections)
    std::array<int, placeSlots> types {};   // the Place in each slot (the instruments' roles)
};
// A section's gear (0 Calm .. 3 Climax, continuous) as the Drama meter offsets its tier:
// at 100 % the section's own tier, every 50 % less one tier lower (Classic at 0: -1).
float gearOf(const Section& s, float sectionIntensity, const Shaping& shaping);
// Drama: each section's gear decides which of the Place's instruments play and how hard
// (calm sections thin out: accents and rhythm leave, the melody turns sparse; dramatic
// ones fill up: instruments enter in order of their role, busier variants, the rhythm
// section drives with fills on the four-bar lines, the melody doubles an octave up and
// the bass down at a climax, Score Intensity pushed with it), and the Drama instruments'
// tier. Evolve (a section's own, see Section::evolve): instruments entering over its
// first bars in order of their role, Score Intensity rising across it, the variants
// changing every 8 bars (16 in sections of 48 bars or more), small fills on the phrase
// lines; calm and closing sections thin out towards their end instead.
void shape(State& st, const Arrangement& a, const Shaping& shaping);
// The Drama design pass: which slot lands an arrival (st.arrivalSlot: the first whose Drama hits
// play at a bigger section's start or after a Drop), after shape().
void markArrival(State& st, const Arrangement& a, const Shaping& shaping);
// The Drama loudness pass (the engine's section balance, WorldEngine.cpp).
// A section's Drama tier as the layer plays it (the gear rounded; or from its intensity).
int sectionTier(const Arrangement& a, int section, const Shaping& shaping);
// The variants as gears (C sparse 0, A 1, B busy 2) and back.
inline int variantGear(int v) { return v == VariantC ? 0 : (v == VariantB ? 2 : 1); }
inline uint8_t gearVariant(int g) { return uint8_t(g <= 0 ? VariantC : (g >= 2 ? VariantB : VariantA)); }

// One section's clip shaped for its kind (`role`: the instrument's role; `kit`: a drum
// part; `runOut`: write a run into the next section in the last bar). Public for the tests.
void shapeClip(pat::Clip& c, int kind, int role, bool kit, int scaleSize, uint64_t key, bool runOut);

} // namespace wb::arr
