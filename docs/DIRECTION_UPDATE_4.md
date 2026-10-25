# WORLDBUILDER — Direction Update 4 (Milestone 6: Sound overhaul + per-instrument effects)

Read after the brief and Direction Updates 1–3. This file overrides them where they conflict. **Milestone 5 is complete** (20 Places × 6 instruments, tested in GarageBand). MIDI control moves to Milestone 7; later milestones shift down by one.

The developer's verdict on the current sound: many instruments sound similar, too light and bouncy, and too obviously synthetic where they should be natural. This milestone fixes that.

## 0. Unattended overnight run — read carefully

The developer is asleep. **Do not ask questions; make reasonable decisions and record them in the DEVLOG.**

- **Work in the priority order of Section 4.** Earlier steps lift every Place at once; later steps refine individual Places.
- Keep a **progress checklist at the top of the DEVLOG handoff** (one line per step and per Place, ticked as done). Update it after each step. If the session is compacted or resumed after a usage limit, read that checklist first and continue from the first unticked item.
- **Commit and push to main after every finished step or Place** with `[skip ci]` in the commit message. The **final push of the session must not** contain `[skip ci]`, so CI builds the result once.
- Before every push, the Linux test suite must pass. If a sound-quality metric fails after 2 attempts, make it report-only, note it, and move on. Never leave the build broken.
- Never wait for CI. Never stop to ask. If something is blocked, note it and continue with the next item.

## 1. Sound philosophy

**Musical, not literal** still applies (Update 1). Within that:

| Place group | Target sound |
|---|---|
| **Fantasy & historical** (Castle, Village, Tavern, Palace, Temple, Ruins, Forest, Cavern, Desert, Sea Ship, Military Camp) | **Acoustic-first.** Instruments should sound like real, physical instruments played by people: wood, gut and metal strings, skin drums, breath, bowed strings, brass, voices. A *light* touch of stylisation (shimmer, slow modulation, space) is welcome for a magical feel, but the core tone must be natural. |
| **Modern** (City, Suburbs, Mall, Factory, Beach) | **Hybrid.** Real-sounding instruments (electric piano, bass, guitar, percussion, metal) mixed with tasteful electronic elements where they fit. |
| **Sci-fi & alien** (Spaceport, Spaceship Interior, Biodome, Alien Hive) | **Synthetic is right.** Rich, deep, cinematic synthesis — but still distinct per instrument and never cheap or thin. |

Use judgment per instrument; this table is the default, not a cage.

**Weight and character (all Places):**
- Less light and bouncy. Default to **slower, heavier, more sustained** playing: longer envelopes, more legato, fuller low-mids and bass, fewer staccato blips, calmer default patterns (Activity can still make them busy).
- **Human feel:** micro-timing, velocity variation, natural attack variation, breath/bow/pick noise, release tails, sympathetic resonance where appropriate.
- **Every instrument must be clearly distinguishable** from every other instrument in the plugin: different timbre family, register, articulation or envelope. No two instruments may share the same generator with only a pitch or filter change.

## 2. Make acoustic instruments real (research-driven)

Synthesis stays the engine (no shipped samples), but it must be **grounded in how real instruments actually sound**:

- **Research each acoustic instrument before rebuilding it.** Use web search and published acoustics data (instrument acoustics papers, physical-modelling literature, measured partial ratios, inharmonicity coefficients, body/formant resonances, decay times, attack transients, spectral envelopes). Record the key numbers used, with sources, in `docs/INSTRUMENT_NOTES.md`.
- **Use the right technique per family:**
  - plucked strings (lute, oud, guitar, harp, zither, kalimba): Karplus-Strong/digital waveguide with pluck position, string damping, inharmonicity and a resonant **body filter** modelled on measured body modes;
  - bowed strings (fiddle, string ensemble, bowed metal): bowed-waveguide or well-shaped additive with bow noise, vibrato, slow attack and body resonances; ensembles use several detuned, slightly delayed players;
  - brass and horns: lip-reed waveguide or brass-modelled additive with brightness rising with velocity, correct attack "blat";
  - woodwinds (flute, ney, recorder): air-jet/waveguide or additive plus filtered breath noise, chiff on attack;
  - mallets, bells, gongs, glass, singing bowls, steel drums: **modal synthesis** with measured mode ratios and per-mode decays (e.g. bell strike and hum tones, glass partials, gong pitch glide);
  - drums (frame drum, timpani, snare, toms, war drums): membrane modal models with skin/shell resonances, proper noise snares;
  - choir/voices: formant synthesis with vowel morphing, breath, ensemble detune and slow vibrato;
  - keys (harpsichord, electric piano, music box, accordion/concertina): specific models (jack pluck; tine + pickup; comb modes; free reed).
- **Reference audio (optional):** if a `reference-audio/` folder exists in the repo, analyse those files (spectra, envelopes, partials) and match the instruments to them. They are reference only: **never ship them in the plugin**.
- Upgrade the shared instrument library first (Section 4, step 2) so improvements reach every Place that uses a model.

## 3. Per-instrument effects

Each of a Place's 6 instruments gets its own small effects section in the detail panel / pattern editor tab:

- **Reverb send** with a **type** choice: Room, Hall, Cathedral, Plate, Cave.
- **Delay send** with a **type** choice: Tape, Ping-Pong, Dotted, Reverse; time follows host tempo.
- **Character slot** with a choice: Off, Saturation, Chorus, Phaser, Filter (low-pass with resonance), Lo-Fi; plus an Amount slider.
- **Tone** (dark ↔ bright tilt) and **Width**.
- Defaults are set per instrument to sound good (e.g. Temple choir: Cathedral reverb; Tavern lute: Room).

Implementation rules:
- **CPU:** reverbs and delays are **shared buses** (one per type, used by sends), never one reverb per instrument. Character effects are per instrument but cheap.
- Parameters exist per **active Place slot** (4 slots × 6 instruments), automatable, smoothed, saved in state and presets; values travel with the Place when it moves slots.
- UI: compact, styled like the rest of the plugin, readable at 100%.

## 4. Priority order for this run

1. **Per-instrument effects** (Section 3): shared buses, controls, UI, state, tests. Push.
2. **Shared instrument library upgrade:** research and rebuild the core models (plucked string, bowed string, brass, woodwind, modal mallet/bell, membrane drum, formant choir, keys). Push after each model.
3. **Weight pass:** retune default envelopes, registers, patterns and levels across all 20 Places toward heavier, slower, more sustained (Section 1). Push.
4. **Per-Place uniqueness and realism pass**, one Place at a time, pushing after each. Order: Tavern, Castle, Village, Palace, Temple, Forest, Desert, Sea Ship, Military Camp, Ruins, Cavern, then the modern Places (City, Suburbs, Mall, Factory, Beach), then sci-fi (Spaceport, Spaceship Interior, Biodome, Alien Hive). For each: check every instrument against Section 1, make sure all 6 are clearly distinct from each other and from other Places, set good effect defaults.
5. Final: full suite, DEVLOG handoff with the checklist and a short list of what to listen for, final push **without** `[skip ci]`.

Whatever isn't reached tonight stays in the checklist for the next session.

## 5. Tests (lean)

- Effects: each send/type/character audible and finite; shared buses within CPU budget with 4 Places × 6 instruments; state round trip.
- Distinctness: for every pair of instruments in the plugin, a spectral/envelope distance measure above a threshold (report-only at first, listing the closest pairs in the log so the developer can check them by ear).
- Each rebuilt model: in tune (partials where expected), finite, no clicks, velocity changes brightness and level.
- Existing tests keep passing.
