# WORLDBUILDER — Direction Update 6 (Milestone 11: Arrangement — game-soundtrack maker)

Read after docs/PROJECT_BRIEF.md and Direction Updates 1–5. This file overrides them where they conflict, and **inserts a new Milestone 11**. The old Milestone 11 (multi-out, Inspire, presets & release) becomes **Milestone 12**.

## 0. The idea

WORLDBUILDER should feel like a **game-soundtrack maker**, not only an ambience generator. A track should **evolve through sections** the user designs, like a composed cue: e.g. 8-bar intro → 16-bar build-up (instruments and pattern variations enter) → 16-bar climax → 16-bar wind-down → 8-bar outro. Any order, any lengths, any number of sections.

## 1. Arrangement timeline

- A new **ARRANGE** view (tab or panel next to the globe / Pattern Editor) showing a horizontal **timeline of sections**, locked to the host's bars and tempo.
- Each **section** has:
  - **Name** (Intro, Build, Climax, Wind-down, Outro, Loop, Bridge, Break, custom) and **colour**;
  - **Length** in bars (1–64), resizable by dragging its edge;
  - **Score Intensity** at the start and end of the section (ramps across it), and an optional curve shape (linear, ease-in, ease-out, swell);
  - **Instrument grid:** for every instrument of every active Place, whether it plays in this section (on / off / "enter halfway" / "fade in" / "fade out");
  - **Pattern variant** per instrument: **A, B, C** (each instrument gets up to three pattern variations in the Pattern Editor, plus an optional **Fill** variant that plays in the last bar before a section change);
  - optional **Location** to be at (uses Milestone 9 travel, timed to arrive at the section start);
  - optional **Planet / World Control overrides** for that section.
- **Transitions** between sections (chosen per boundary): Cut, Crossfade (1–4 bars), Riser into next, Drum fill, Drop (silence/sub hit on the downbeat), Reverse swell, Filter sweep. Transition sounds use the active Places' own instruments and effects so they fit the world.
- Users can add, delete, duplicate, reorder (drag) and resize sections; undo/redo.

## 2. Templates and auto-arrange

- **Templates** to start from:
  - *Classic Arc*: 8 Intro / 16 Build / 16 Climax / 16 Wind-down / 8 Outro;
  - *Exploration Loop*: 8 Intro / 32 Loop (repeats) / 8 Outro;
  - *Battle*: 4 Intro / 8 Tension / 16 Combat / 8 Break / 16 Combat II / 4 Victory;
  - *Boss Fight*: 8 Approach / 16 Phase 1 / 4 Transition / 16 Phase 2 / 8 Defeat;
  - *Town Theme*: 8 A / 8 B / 8 A / 8 C (loopable);
  - *Cinematic Reveal*: 16 Mystery / 16 Rise / 8 Reveal / 16 Afterglow.
- **Auto-arrange** button: fills the instrument grid, variants, intensities and transitions musically for the chosen template and the active Places: sparse beds in intros, instruments entering in order of role (drone → bass → rhythm → melody → accents) during build-ups, everything at climax, thinning out in wind-downs. The user then edits. Uses the World Seed; undoable.

## 3. Playback and DAW sync

- **Arrangement mode** joins Play Mode (Free / Transport / Arrangement). In Arrangement mode the plugin follows the host position: starting playback at bar 37 plays exactly what the arrangement has at bar 37 (deterministic, matches renders).
- **Loop toggle per section** (for game-style looping sections) and an **"End: stop / loop to section X"** setting.
- A playhead and current-section highlight in the ARRANGE view; clicking a section while stopped jumps the preview there (in Free/preview mode).
- The arrangement is saved in the project, presets and `.world` files.
- The arrangement drives the same parameters as automation would; host automation of Score Intensity etc. still works and **adds on top** of the arrangement.

## 4. Game-audio export (extends Milestone 8 Capture)

- **Render arrangement:** the whole arrangement to WAV (full mix or stems per Place).
- **Render sections separately** with an option to make **loop sections seamless** (render with the tail of the loop wrapped into its start), named for game engines, e.g. `Tavern_Intro.wav`, `Tavern_Loop.wav`, `Tavern_Outro.wav`.
- **Stingers:** render short one-shot cues from a section's ending or a transition (e.g. "victory", "danger") for game events.
- **Scene MIDI export** follows the arrangement (variants and on/off per section).

## 5. Tests (lean)

- Arrangement playback is deterministic and matches renders from any start bar.
- Instruments enter/leave exactly at section boundaries (or as their entry mode says), with no clicks.
- Score Intensity ramps follow the section curves; transitions produce no level spikes.
- Pattern variants A/B/C and Fill play as written.
- Seamless loop renders: the end joins the start without a click or level jump.
- State round trip of a 6-section arrangement with variants, transitions and Locations.

## 6. Revised milestones

11. **Arrangement** (this file). Suggested split across sessions: **11a** timeline, sections, instrument grid, pattern variants, Arrangement play mode; **11b** transitions, templates, auto-arrange; **11c** game-audio export and polish.
12. **Pro features, presets & release** (old Milestone 11): separate outputs, Inspire (now also picks a template and auto-arranges), factory presets as complete arranged scenes, release docs.

Same working rules: push after each finished part with `[skip ci]`, the final push of a session without it; DEVLOG checklist; small metric misses report-only after 2 attempts; don't wait for CI; don't ask questions.
