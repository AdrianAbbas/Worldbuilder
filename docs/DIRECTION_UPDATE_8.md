# WORLDBUILDER — Direction Update 8 (Milestones 14 & 15)

Read after docs/PROJECT_BRIEF.md and Direction Updates 1–7. This file overrides them where they conflict and adds Milestones 14 and 15.

Same working rules as before: push after each finished part with `[skip ci]`, the final push of a session without it; DEVLOG checklist; small metric misses report-only after 2 attempts; don't wait for CI; don't ask questions when unattended.

---

## Milestone 14 — Play and record with a MIDI keyboard

Many composers prefer to **play** parts on a keyboard rather than draw them. In GarageBand and Logic, a MIDI keyboard plays whatever instrument plugin is on the selected track, so WORLDBUILDER receives the keyboard's notes from the host. Use them.

### 14a. Play target

- A **KEYS** section (top bar or the Pattern Editor) with a **Play target** selector: choose **Place slot → instrument** (e.g. "Tavern → Lute").
- Incoming host MIDI notes **play that instrument live**: velocity, note length, sustain pedal (CC64), pitch bend (sensible default range per instrument, e.g. ±2 semitones), mod wheel → that instrument's vibrato or brightness.
- **Scale mode:** *Chromatic* (play exactly what's pressed) or *Snap to scale* (wrong notes move to the nearest note of the current Root/Mood). Default: Chromatic.
- **Octave shift** −2…+2 and a **Monitor** toggle (hear the keyboard live or not).
- The selected instrument's pattern keeps playing unless the user mutes it or arms recording (below).
- If no target is selected, incoming notes do nothing (as now), so existing projects are unaffected.
- A small on-screen keyboard/indicator in the Pattern Editor lights up the notes being received, so the user can see MIDI is arriving.

### 14b. Record into the pattern

- A **REC** (arm) button on the Pattern Editor for the target instrument.
- **Recording follows the host transport:** press Record or Play in GarageBand/Logic; WORLDBUILDER captures notes into the target instrument's pattern, aligned to the host's bars and tempo. Optional **count-in** (1 bar) using the host position.
- **Record modes:** *Replace* (new pattern), *Overdub* (add to existing notes), *Into variant* (record into A, B, C or Fill for the Arrangement).
- **Pattern length** follows the existing pattern length, or *Auto* (rounds up to whole bars after recording stops).
- **Quantise** on input: Off, 1/4, 1/8, 1/16, 1/32, triplets, plus a **Strength** % (keeps human feel); can be applied or undone afterwards.
- While armed, that instrument's generator/randomness is paused so the recording isn't mixed with generated notes. After recording, the pattern counts as "edited" (Generate no longer overwrites it).
- Recorded notes appear instantly in the Pattern Editor grid; one Undo removes the whole take.
- Works when the transport is stopped too ("free record": an internal metronome at the host tempo, starting on the first note).

### 14c. Avoid double-playing (important)

GarageBand/Logic will **also** record the keyboard notes as a MIDI region on the track. When played back, that region would trigger the target instrument again on top of the recorded pattern. Handle it clearly:
- Option **"Host regions play the target instrument"** (on by default only while REC is not armed). Document in the README: *either* record into WORLDBUILDER's pattern (recommended — the part then lives in the scene and the Arrangement), *or* record a normal MIDI region in the DAW that plays the target instrument — not both.
- Show a short tip in the UI the first time REC is armed.

### Tests (14)

Notes received in processBlock play the target instrument (audible, correct pitch, velocity changes level/brightness); sustain and pitch bend work; recording at several tempos lands notes on the right steps (with and without quantise); overdub/replace/variant work; undo; transport-follow and free record; state round trip; no allocation on the audio thread (preallocated note buffer, lock-free handoff to the editor). AU harness: send MIDI to the instrument, assert audible output.

---

## Milestone 15 — Real instrument samples (the developer's recordings)

The developer is recording their own sounds (voice, guitar, breath, percussion, glass, metal, wood, synths, textures; see the sample-target spreadsheet). They are 100% the developer's own, so they can ship inside the plugin.

### 15a. Where the samples go

- **Before any samples are committed, the repository must be private.** (The developer will make it private; check and stop with a clear message if it is still public.)
- Folder: `samples/` in the repo, with the subfolders `01-voice … 08-texture`. Filenames follow the convention `category-sound-pitch-dynamic-take.wav`, e.g. `voice-aah-A3-soft-01.wav`, `glass-tap-high-02.wav`.
- Large files: if the folder exceeds ~1 GB or any file exceeds 50 MB, use **Git LFS** and document it.
- A `samples/README.txt` from the developer lists how each set was recorded.

### 15b. Sample preparation (automatic, in a script the developer can rerun)

- Trim silence (keeping the natural tail and attack), normalise consistently, remove DC offset, short fades to prevent clicks.
- Detect the pitch of pitched samples (and cross-check the filename); detect velocity layer from the name or loudness.
- Find **loop points** for sustained sounds (voice, drones, bowed or blown tones, synth pads) with crossfaded loops.
- Convert to a compact format for shipping (e.g. FLAC or high-quality compressed) to keep the plugin size reasonable; report the total size.
- Produce a mapping file per instrument (key ranges, velocity layers, round-robins, loop points) that the sampler reads.

### 15c. Sampler engine

- A real multisample engine: key mapping with pitch-shifting between recorded notes, **velocity layers**, **round-robin** takes, sustain loops, release tails, per-voice filter and envelope, natural legato/retrigger behaviour.
- **Sample-based instruments are just another engine** for an instrument slot: they get the same patterns, Arrangement, per-instrument effects, Characters and Score Intensity behaviour as synthesised ones.
- **Hybrid option:** layer a sample (e.g. a real pluck attack or breath) with the existing physical model, with a blend control, so even sparse recordings improve many instruments.
- Use the developer's recordings to **upgrade or replace** the weakest instruments first: choirs and chants (voice), lute/oud/harp/plucks (guitar), flutes and ney (breath and bottles), frame drums and hand percussion, bells and glass, wooden percussion, and add analog-synth character to the sci-fi Places from the ARP 2600 / Minilogue recordings.
- Samples load into memory at plugin load (or on first use of a Place) on a background thread, never on the audio thread; shared between plugin instances where possible. Keep memory reasonable on an 8 GB MacBook Air.
- **Sandbox:** GarageBand runs Audio Units sandboxed, so ship the samples **inside the plugin bundle** (Resources), not in a user folder that GarageBand may not be allowed to read. Update install-mac.sh and the CI packaging accordingly.

### Tests (15)

Every mapped sample plays at the right pitch; velocity layers and round-robins switch as expected; loops are click-free; memory and load time are reported; a Place using sampled instruments still passes its Place tests; the AU harness loads the plugin with samples and renders audio; package size reported.

---

## Milestone order

- Hotfix session (freeze, environment levels, Inspire bug, 16-bit readability, planet rotation, forest lakes, hurdy-gurdy, sky labels).
- **14** MIDI keyboard play and record (14a → 14b/c).
- **15** Samples: 15a/15b once the developer has uploaded the first batch, then 15c. Start with whichever categories are complete; more batches can follow.
