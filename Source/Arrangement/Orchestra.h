#pragma once
// Orchestration (the orchestration pass, 2026-11-05): an arrangement of several Places
// written the way a composer would score it, not every instrument of every Place at once.
//
// Each section gets a part budget (Intro 2-4, Build 4-7, Climax 7-10, Wind-down 3-5, Outro
// 2-3 in all, across every Place) and its parts are chosen by their voice: one lead melody,
// one bass, one kit (a Place's drum kit, or the Drama drums of the leading slot while a
// tier above Calm plays: never two kits at once), one ostinato (the Drama ostinato takes
// over from a Place's while the drama plays), pads (two at most), one environment bed, and
// colour (accents, the Drama hits). The lead passes between the Places from section to
// section; the other Places fill the other voices or rest, and their space (World Space and
// reverb sends) is thinned so several Places do not stack into mud.
//
// Lengths by time: the templates are written in proportions and fitted to a duration at
// the host's tempo (the Length control: Short about 1 minute, Medium 3.5, Long 5, Epic 8),
// each section rounded to a phrase (4, 8 or 16 bars).
#include "Templates.h"

namespace wb::arr {

// A part's voice in the orchestra.
enum Voice { VoiceLead, VoiceBass, VoiceKit, VoiceOstinato, VoicePad, VoiceEnvironment, VoiceColour, VoiceHits, VoiceCount };
inline constexpr std::array<const char*, VoiceCount> voiceNames { "lead", "bass", "kit", "ostinato", "pad", "environment", "colour", "hits" };
// How many of a voice may sound at once (two of a voice and register mask each other).
inline constexpr std::array<int, VoiceCount> voiceLimit { 1, 1, 1, 1, 2, 1, 2, 1 };
int voiceOf(int placeType, int element);

// A section's part budget (sounding parts, every Place together) by its kind.
struct Budget {
    int low = 0, high = 0;
};
Budget budgetOf(int kind);

// Orchestrates every section of `a` for the Places in `types`: which parts play (by voice,
// within the budget), the lead slot, `orchestrated` set. Lengths, names, ramps, transitions,
// tiers and clips stay (a part left out keeps its clip, off). A part kept plays as its cell
// said; one the orchestra brings in plays its kind's variant. The World Seed picks between
// equal choices.
Arrangement orchestrate(const Arrangement& a, const std::array<int, placeSlots>& types, uint32_t seed);

// What sounds at a State after shape() (the Drama instruments only while their section plays
// a tier above Calm and their slot's rank gives them notes): per slot and instrument.
using Sounding = std::array<std::array<bool, slotElements>, placeSlots>;
Sounding sounding(const State& st, const Arrangement& a, const Shaping& shaping);
int countParts(const Sounding& on);
int countVoice(const Sounding& on, const std::array<int, placeSlots>& types, int voice);

// The Length control: 0 Short, 1 Medium, 2 Long, 3 Epic, continuous in between.
constexpr float lengthMin = 0.0f, lengthMax = 3.0f, lengthDefault = 1.0f;
inline constexpr std::array<const char*, 4> lengthNames { "Short", "Medium", "Long", "Epic" };
double lengthSeconds(float position);
// Every section scaled to make `seconds` at `bpm` (their proportions kept, each rounded to a
// phrase: 4 bars under 12, 8 under 64, 16 above; the total within 5 % where the phrases allow).
// Endless sections count one pass; repeats stay. Clips are fitted to their new lengths.
Arrangement fitLength(const Arrangement& a, double seconds, double bpm);
// The arrangement's time at `bpm` (seconds; an endless section counts one pass).
inline double secondsOf(const Arrangement& a, double bpm) { return double(a.totalBars()) * double(beatsPerBar) * 60.0 / std::max(bpm, 1.0); }

// A template fitted to a Length at a tempo (the default: Medium at 120 BPM).
Arrangement makeTemplate(int which, double seconds, double bpm);
// A new section of a kind (ARRANGE's "+"): its share of a Medium cue at that Length and tempo.
int newSectionBars(int kind, double seconds, double bpm);

} // namespace wb::arr
