# WORLDBUILDER — Direction Update 5 (Milestones 7–11)

Read after the brief and Direction Updates 1–4. This file overrides them where they conflict, and **replaces the milestone list**.

Milestone 6 (sound overhaul + per-instrument effects) is complete and tested in GarageBand. Every Place keeps the current standard: **6 instruments**, each with its own effects, Characters, effect character and an efficient overlay. (Expanding to more instruments per Place is cancelled.)

## Working rules (every session)

- Push to main after each finished Place or feature with `[skip ci]` in the commit message; the final push of a session has no `[skip ci]`, so CI builds once.
- Keep the progress checklist at the top of the DEVLOG handoff; resume from it after a compaction or usage limit.
- Small test-metric misses (under 1 dB or similar) after 2 attempts → report-only, note them, move on. Never leave the build broken. Don't wait for CI. Don't ask questions when running unattended; record decisions in the DEVLOG.
- Keep CPU in check on a fanless MacBook Air; anything new scales with the Quality setting.

---

## Milestone 7 — Sci-fi Places + Score Intensity

### 7a. Six new sci-fi Places (synthetic is right here; rich, cinematic, never thin)

| Place | Instrument ideas (6) | Characters | Effect character | Overlay idea |
|---|---|---|---|---|
| **Space Station** | rotating-hull drone, docking-clamp hits, comms-beep motifs, airflow pad, reactor pulse bass, zero-g shimmer lead | Routine Orbit, Emergency, Abandoned | sealed metallic room, filtered delay | panel seams with blinking status lights |
| **Cyberpunk Megacity** | gritty synth bass, neon arp, rain-soaked pad, distorted drum machine groove, glitch stabs, distant megastructure hum | Night Market, Corporate Tower, Riot | wet neon reverb, bitcrush, ping-pong | neon signage glow and rain streaks |
| **Starship Bridge** | command-console arps, warp-core drone, tactical pulse rhythm, alert brass-synth swells, crew-chatter texture (abstract, no speech), sensor pings | Cruising, Battle Stations, Warp Jump | wide cinematic hall, risers | HUD frame lines and target brackets |
| **Asteroid Mining Colony** | drilling ostinato, low rock-grind bass, tuned metal clanks, dust-hiss texture, worker-hymn synth pad, lonely beacon lead | Shift Change, Cave-in, Night Shift | gritty industrial room, slapback | rock fragments and drill-light beams |
| **Alien Ruins** | eroded crystal tones, ancient-machine pulses, reversed alien choir, sub-harmonic drone, ritual percussion, whispering formant texture | Dormant, Reawakening, Guardian | vast strange reverb, warped delay | glowing glyphs and cracked stone |
| **Nebula / Deep Space** | vast evolving pads, cosmic-wind texture, slow star-pulse bells, sub drone, granular stardust, solar-flare swells | Drift, Stellar Nursery, Void | infinite shimmer reverb | drifting gas clouds and stars |

Same standards as Milestone 5/6: 6 distinct instruments, per-instrument effects with good defaults, Characters that change DSP and patterns, pattern-editor support, the Place selector gets a sci-fi category for them.

### 7b. Score Intensity macro

- One large, automatable **Score Intensity** knob (0–100%) in the top bar.
- It raises tension, activity, density, dynamics and brightness across **every active Place at once**, with per-Place response curves designed so that 0% is a calm bed, 50% is the Place's default, and 100% is a climax.
- Smoothed, never jumps in level beyond the loudness rules; works in all Play Modes.
- Tests: monotonic increase in density and loudness with Intensity; no level spikes; automation is smooth.

---

## Milestone 8 — Capture & export (replaces "MIDI control")

WORLDBUILDER already generates its own notes through the Pattern Editor, so host MIDI input zones are **cancelled**. Instead, make it easy to get WORLDBUILDER's output into the song:

- **Export scene MIDI:** one button exports the patterns of every active instrument as a multi-track MIDI file (one track per instrument, named "Place – Instrument"), aligned to bar 1 and the current tempo, for a chosen length (e.g. 4–64 bars). Drag-out into the host where supported; "Save MIDI file" otherwise. (Per-pattern Drag MIDI already exists; keep it.)
- **Render audio:** a "Render" button that renders a chosen number of bars of the current world, offline and faster than real time, to a WAV file (full mix, or one file per Place as stems), at the host's tempo and sample rate. Drag-out where supported, Save otherwise. This matters for GarageBand, which has no bounce-in-place.
- **Host sync:** Transport mode remains the default; make sure playback is identical every time the host plays from the same position, so live use and renders match.
- **Automation-first workflow:** confirm that every important control (Score Intensity, Place levels and mute, Characters, World Controls, Planet, Locations once built) is automatable and responds smoothly, since DAW automation replaces MIDI control for shaping a cue over time.
- README section "Getting WORLDBUILDER into your song": GarageBand (Transport mode → Render or export/re-import), Logic (Bounce in Place, Render, multi-out later).
- Tests: render is deterministic and matches real-time playback from the same position; exported MIDI round-trips into the Pattern Editor.

## Milestone 9 — Locations & Journeys

Implement **brief Section 4** (Locations & Journeys), now including: active Places and their Characters, patterns, per-instrument effects, Score Intensity, planet type (once Milestone 10 exists) and World Controls.

- 8 Location slots, Travel Time, automatable Destination and Journey (From/To morph).
- Travel sounds like moving through a world: Places recede and approach, continuous parts morph, tonal parts glide.
- No clicks, level jumps or silence gaps (test).

---

## Milestone 10 — Visual revamp + planet types

### Planet types (visual + sound)

A **Planet** selector (automatable): **Rocky, Ocean, Ice Giant, Gas Giant**, plus optionally **Desert World, Lava World, Forest World**. Each changes the globe's look and adds a global sound colour on the master:

| Planet | Globe | Sound colour |
|---|---|---|
| Rocky | cratered grey-brown surface, thin atmosphere | dry, close, grounded, slight low-mid weight |
| Ocean | deep blue oceans, moving clouds | watery chorus and filtering, slow rolling movement |
| Ice Giant | pale cyan banded ice, rings | cold, bright, crystalline shimmer, airy highs |
| Gas Giant | huge swirling bands and storms | vast swirling reverb, slow pressure modulation, deep sub |
| Desert World (optional) | dunes, dust haze | dry warmth, dusty high cut, wide |
| Lava World (optional) | glowing cracks | heat distortion, saturation, low rumble |
| Forest World (optional) | green continents | soft, damped, organic |

- An **Planet Influence** amount (0–100%) controls how strong the sound colour is. Default subtle (~35%).
- Planet type is saved in state, presets and Locations; changing it crossfades smoothly.

### Interface revamp

- **Revamp the globe**: much richer rendering (layered shading, atmosphere glow, rotating cloud layers, rings where relevant, lights on the night side for populated Places), still efficient (cached layers, ≤ 30 fps).
- **Revamp icons and buttons**: they currently look dull. Give all buttons, toggles, tabs, menus and Place cards a premium, tactile style: depth, subtle gradients and bevels, glow on hover/active, clear states, consistent icon set (original vector icons, same style throughout). Keep everything readable at 100%.
- **Keep the overlays** as they are (the developer likes them).
- Snapshots before/after for each part, checked by eye.

---

## Milestone 11 — Pro features, presets & release

- **Separate outputs per Place** (multi-output AU/VST3): main stereo out plus one stereo out per Place slot. Logic supports multi-out instruments; GarageBand doesn't, so the main output must always contain the full mix. Document how to use it in Logic.
- **Inspire button**: builds a random but musical world (Places, Characters, planet, Root/Mood, Score Intensity, effects) with sensible constraints so results are usable; uses the World Seed; undoable.
- **Factory scene presets**: at least 20, each a ready-made film moment combining 2–4 Places, a planet, effects and 2–4 Locations to travel between, e.g. "Ambush at the Tavern", "Docking at Orbital Station", "Siege of the Palace", "Neon Rain Chase", "Abandoned Mine", "First Contact", "Market at Dawn", "Storm at Sea", "Desert Caravan", "Into the Nebula".
- README: final control guide, keyboard map, troubleshooting; DEVLOG final handoff.
