# WORLDBUILDER — Direction Update 2 (after Milestone 3)

Read after WORLDBUILDER_PROMPT.md and DIRECTION_UPDATE.md. This file overrides both where they conflict.

Milestone 3 feedback: the Place selector is excellent. Keep its workflow. Three new requirements follow, plus a revised milestone order.

## 1. Pattern Editor (next milestone, top priority)

Composers need control over what each Place plays. Patterns must become **editable data**, not only seeded randomness.

**Pattern model**
- Each rhythmic or melodic element of a Place (e.g. Castle Bells, Castle Pulse, Village Plucks, Spaceport Pulses) plays from a **Pattern**: a list of notes with step position, pitch (scale degree, so it follows Root/Mood), velocity, length, and probability.
- Patterns have a length (1–8 bars, or free length in steps), a step resolution (1/4 to 1/32, triplets) and swing. They follow host tempo and transport position, so they line up with the song.
- The current seeded generators become **pattern generators**: they produce a starting pattern the user can then edit. A "Generate" button (with the Place's Seed) creates a fresh one; edited patterns are kept, not overwritten.

**Controlled randomness** (per element)
- **Randomness** 0–100%: 0% plays the pattern exactly; higher values allow per-note changes.
- What may vary (toggles): timing, velocity, pitch (within scale), note skipping, extra notes.
- Per-note **probability** and per-note **lock** (a locked note never changes).
- **Evolve**: slowly mutates the pattern over time, within the randomness rules, with an option to return to the original every N bars.

**Editor UI (inside the plugin)**
- Opens from a Place's detail panel: a tab per element.
- A **piano-roll grid**: rows are scale notes of the current Root/Mood (with an option to show all 12 semitones), columns are steps. Click to add a note, drag to move or lengthen, right-click or Alt-click to delete, a velocity lane underneath, probability shown as note opacity and editable in a lane.
- Tools: draw, select, copy/paste, clear, generate, transpose, double/halve length, undo/redo.
- Shows the playhead while the host plays.
- Styled to match the plugin's aesthetic, never a generic grid.

**MIDI integration**
- **Import:** drag a MIDI file onto an element to use its notes as the pattern (snapped to the current scale, with an option not to snap).
- **Export:** drag a pattern out of the plugin into the DAW as a MIDI clip, where the host supports it. Test in Logic and GarageBand and document what works; if a host doesn't accept drag-out, offer "Save as MIDI file" instead.
- **Record from MIDI input (optional, if time allows):** arm an element, play notes on the instrument track, and they are captured into the pattern.

**State**: all patterns save in the project and in presets and Locations. Saved state stays compact (patterns are small data, not audio).

**Tests**: pattern playback at 0% randomness is identical every time and sample-aligned to host position; randomness only changes what its toggles allow; locked notes never change; MIDI import/export round trip; state round trip; no allocation on the audio thread (preallocate pattern storage, edit from the UI through a lock-free swap).

## 1B. MIDI control from the host (part of Milestone 4)

Currently WORLDBUILDER free-runs and ignores the host's piano roll, so users can't write or record a part for it in GarageBand or Logic. Fix this so it behaves like a proper instrument on the timeline:

- **Play Mode** (automatable): **Free** (current behaviour), **Transport** (plays only while the host is playing, starting from silence and locked to song position, so bounces and exports always include it), **MIDI** (sound is driven by notes from the host's piano roll).
- In **MIDI mode**, the keyboard is split into zones (shown in the UI and the README):
  - **Scene keys** (a low octave): holding a key brings in a Place or element (e.g. C1 = Place 1, D1 = Place 2…; or per-element). Release fades it out. Velocity sets intensity. This lets users "score" when each Place enters and leaves by drawing notes.
  - **Key follow**: an optional toggle where the lowest held note in a chosen octave sets the world's Root, so the soundscape follows chord changes.
  - **Play keys** (middle and upper range): play the active Place's instruments directly, in key (castle bells, tavern lute, spaceport pulses), so users can perform melodies and record them.
  - **Event keys**: trigger the Place's one-shot events (impacts, calls, risers).
- MIDI CC: mod wheel = Tension, expression (CC11) = Activity; both mappable.
- **Recording:** notes recorded in GarageBand/Logic play back identically through these zones. Test that a recorded region plays back the same performance.
- **Export safety:** in Transport and MIDI modes, a project export/bounce must contain the plugin's sound. Document that in Free mode, GarageBand only exports up to the last region, so users should use Transport mode or place a region covering the section.
- Default Play Mode for new instances: **Transport**.

## 2. More Places (later milestone)

Add new Places with their own instrumentation and effects, in the same musical, film-scoring style:
- **Spaceship Interior (in orbit)**: low engine drones, pulsing life-support rhythms, soft computer tones, airlock swells, a confined metallic space.
- **Tavern**: warm folk instruments (plucked lute-like strings, frame-drum pulse, fiddle-like drones), clinking tuned glass tones, cosy wooden room.
- **Battlefield**: war drums, low brass-like swells, metallic clash hits, distant horn calls, smoke-like noise textures, big open space.
- Plus the remaining Places from Direction Update 1 (Factory, City, Temple, Forest, Ruins, Cavern, Alien Hive).

Every Place uses the Pattern Editor from day one.

## 3. Visual overhaul (later milestone)

- The whole interface reacts to the world: background colour, lighting, particles and atmosphere change with the active Places and World Controls (e.g. tavern warmth, cold blue spaceship light, smoky red battlefield glow). Transitions are smooth.
- Place art becomes **rich illustrations instead of simple 2D sketches**: layered artwork with lighting, depth (parallax layers), subtle animation (flickering windows, smoke, lights, moving clouds) and atmosphere.
- **Artwork approach:** the plugin can't do real-time 3D cheaply (OpenGL is deprecated on macOS and JUCE has no Metal renderer), so use layered 2.5D art: high-quality procedural vector illustration with gradients, lighting and parallax, **and** support for bundled image assets (PNG layers) loaded from BinaryData. Build an asset pipeline so the developer can drop in finished artwork per Place (e.g. `assets/places/castle/{back,mid,front,lights}.png`) and it is used automatically, falling back to procedural art when missing.
- Keep animation ≤ 30 fps, cached, and light on CPU. Keep the installed plugin size reasonable (under ~60 MB).

## 4. Revised milestone order

4. **Pattern Editor + MIDI control**: Play Modes and host MIDI zones (Section 1B), pattern model, controlled randomness, piano-roll editor, MIDI import/export, and conversion of Castle, Village and Spaceport to editable patterns.
5. **More Places**: Spaceship Interior, Tavern, Battlefield, then Factory, City, Temple, Forest, Ruins, Cavern, Alien Hive (split across two sessions if needed).
6. **Locations & Journeys** (from the brief, now including Places and their patterns).
7. **Visual overhaul** (Section 3).
8. **Presets & release**.

Same working rules: push to main, stop after pushing, don't wait for CI, short logs, max 2 fix attempts, update the DEVLOG handoff. Split a milestone across sessions rather than running one session very long.
