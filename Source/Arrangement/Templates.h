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

enum Template { ClassicArc, ExplorationLoop, Battle, BossFight, TownTheme, CinematicReveal, TemplateCount };
inline constexpr std::array<const char*, TemplateCount> templateNames { "Classic Arc", "Exploration Loop", "Battle", "Boss Fight", "Town Theme",
                                                                        "Cinematic Reveal" };

// An instrument's role in its Place, in the order instruments enter a build-up.
enum Role { RoleBed, RoleTexture, RoleBass, RoleRhythm, RoleMelody, RoleAccent, RoleCount };
inline constexpr std::array<const char*, RoleCount> roleNames { "bed", "texture", "bass", "rhythm", "melody", "accent" };
int roleOf(int placeType, int element);

// What a section does in the cue.
enum Kind { KindIntro, KindBuild, KindPeak, KindCalm, KindBreak, KindOutro, KindLoop, KindBridge, KindCount };
inline constexpr std::array<const char*, KindCount> kindNames { "intro", "build", "peak", "wind-down", "break", "outro", "loop", "bridge" };
int kindOf(const Arrangement& a, int index);

// A template's sections (every instrument on, variant A), ready for Auto-arrange.
Arrangement makeTemplate(int which);
// Auto-arrange: `a` with its grid, variants, ramps and transitions filled in for the
// Places in `types` (per slot, NoPlace for none). Names, lengths, plays, colours,
// Locations, overrides and the End stay as they are.
Arrangement autoArrange(const Arrangement& a, const std::array<int, placeSlots>& types, uint32_t seed);

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
// One section's clip shaped for its kind (`role`: the instrument's role; `kit`: a drum
// part; `runOut`: write a run into the next section in the last bar). Public for the tests.
void shapeClip(pat::Clip& c, int kind, int role, bool kit, int scaleSize, uint64_t key, bool runOut);

} // namespace wb::arr
