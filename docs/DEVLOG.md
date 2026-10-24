# WORLDBUILDER development log

Decisions, problems and fixes, newest milestone last. The brief is in
[PROJECT_BRIEF.md](PROJECT_BRIEF.md). The build, install and validation pipeline
comes from VEIL (https://github.com/AdrianAbbas/Dark-Fantasy-Plugin); read its
`docs/DEVLOG.md` for the history behind it.

---

## Lessons carried over from VEIL

- **GarageBand silence was a Gatekeeper rejection, not a DSP bug.** A browser
  download marks every file with `com.apple.quarantine`. macOS then refuses to load
  an ad-hoc-signed bundle into `AUHostingServiceXPC_arrow`, and GarageBand shows a
  silent slot with generic sliders. `install-mac.sh` removes the flag, re-signs,
  resets `AudioComponentRegistrar` and runs `auval`. CI installs a quarantined copy
  with the script before validating it.
- **CI `auval` passing does not prove GarageBand works.** The runner's copy is never
  quarantined, and GarageBand on Apple Silicon hosts AUv2 plug-ins out-of-process.
  The AU host harness exercises both the in-process and out-of-process routes.
- **Sign whole bundles** (`codesign --force --deep --sign -`) before packaging, and
  publish the installable artifact only when every check passed.
- **Universal binary, 10.15, `AU_SANDBOX_SAFE TRUE`, JUCE 8.0.16** (which includes
  the AU-client editor-ownership fixes).
- **Out-of-process rendering needs host-like pacing** (one block per block
  duration, host time set, a pause before teardown), or AudioToolbox traps in
  `caulk::semaphore`. The harness keeps VEIL's pacer.
- **JUCE publishes continuous AU parameters normalised to 0–1.** The harness sets
  them through min/max or `ParameterValueFromString`.
- **The AUAudioUnit route as driven by the harness is unreliable.** It returned
  silence even for Apple's own AULowpass. It stays diagnostic, not a gate.

---

## Milestone 1: skeleton (2026-10-24)

### Working conditions

| | |
|---|---|
| Machine | Linux cloud container (Ubuntu 24.04, x86_64). No macOS, Xcode, GarageBand or Logic. |
| Verified here | Linux build (VST3, Standalone, tests) with JUCE 8.0.16: **284 checks pass, 0 fail**. Editor rendered to an image and inspected. |
| Not verified here | Anything macOS: the AU build, `auval`, the adapted AU harness (`tools/au-harness/WorldbuilderAUHarness.mm` was edited for an instrument but **has never been compiled**), the install script for the new identity, GarageBand. CI on the macOS runner is the first test of all of these. |

### Identity

| | |
|---|---|
| Type | `aumu` (`kAudioUnitType_MusicDevice`), `IS_SYNTH TRUE`, MIDI in, no audio input, stereo (or mono) out |
| Subtype / manufacturer | `Wbld` / `Veil` |
| Name | "Veil Audio: WORLDBUILDER", bundle ID `audio.veil.worldbuilder` |
| Factory function | `WORLDBUILDERAUFactory` (JUCE derives it from the product name) |
| Formats | AUv2, VST3, Standalone (dev only, not shipped in the zip) |

### What was built

- **Pipeline** copied from VEIL and renamed: `build.yml`, `ci-check-bundles.sh`
  (now checks `aumu`/`Wbld`), `package-mac.sh`, `ci-user-install-test.sh`,
  `install-mac.sh`/`uninstall-mac.sh` (instrument-slot instructions),
  `diagnose-mac.sh`, `INSTALL.txt`, `ci-developer-id.sh` (optional, untested, as in VEIL).
  VEIL's `au-harness-baseline.yml` (experiments on old VEIL bundles) was not copied.
- **AU harness, adapted for an instrument.** There is no input bus, so it sets only
  the output format and renders without a render callback. Per route (v2
  out-of-process, v2 in-process; AUAudioUnit route diagnostic) it checks:
  - the default world is audible with no input and no MIDI;
  - the right channel is written and the peak stays below 0 dBFS;
  - Master Level at minimum is audible as a drop;
  - Power off is silent after the fade, and Power on brings the world back;
  - MIDI note on/off is accepted;
  - presets are audible;
  - state round trip into a fresh instance;
  - 44.1 and 96 kHz;
  - mono output (report only);
  - the editor (Cocoa view in-process, remote view controller out-of-process).

  The AUAudioUnit control is now Apple's DLS synth (`aumu:dls :appl`) instead of
  AULowpass.
- **Parameter tree** (`Source/Parameters.h`, one table): 11 World Controls; Power,
  Master Level, Width, Seed, Quality, Space Mix, Root, Mood; Level/Mute/Solo for all
  10 layers. That is 49 parameters. Structure, spin and Location parameters are
  added in their milestones. JUCE hashes string IDs into AU parameter IDs, so adding
  parameters later does not disturb saved automation.
- **Engine** (`Source/WorldEngine.*`):
  - the layers go to a dry bus and a send bus (send = layer send × Scale distance);
  - the World Space is applied;
  - then the master chain: Width (M/S), glue compressor (2:1 above −18 dBFS, slow),
    Master Level, the 2 s raised-cosine Power fade, and a safety limiter (instant
    attack, −1 dBFS ceiling, hard clamp).

  It also applies per-layer NaN guards and Scale distance (a high cut on the dry
  bus and more send when colossal).
- **Air layer**:
  - decorrelated noise through a gust-driven band-pass;
  - Temperature moves the band from thin and high (frozen) to a low dry rush (scorching);
  - 1–3 whistling resonances (by Quality): Era tightens their Q, from breathy
    (ancient) to singing metal (futuristic);
  - a storm howl above Weather ≈ 0.5, which Danger pushes in earlier;
  - Wetness damps the highs (humid air);
  - Activity sets gust speed and depth.
- **Tonal Bed**: 7 voices, each a pair of detuned additive oscillators (Chebyshev
  recurrence; 4/6/9 harmonics by Quality, with a Nyquist guard):
  - sub, root, fifth, third and a colour tone from a per-mood table (9 moods);
  - a dissonant ♭9 voice that throbs with Danger;
  - an octave halo for Magic.

  Root and mood changes glide over about 1.2 s to the nearest octave. Era sets the
  harmonic tilt: near-pure tones when ancient, bright and hollow (odd harmonics)
  when futuristic. Temperature adds heat-haze pitch shimmer and dulls the sub when
  frozen. Scale adds low-end weight and stereo spread.
- **World Space**:
  - pre-delay (4–110 ms), 2 or 4 all-pass diffusers per side, then an 8-line FDN
    with a Householder matrix, in-loop damping and a 40 Hz cut, and slow delay
    modulation (off in Eco);
  - Scale sets size (0.45–2.4×), decay (0.7–9 s) and pre-delay;
  - Temperature, Wetness and Magic set decay and damping.
- **Editor** (placeholder, `Source/PluginEditor.*`, `Source/UI/WorldLookAndFeel.*`),
  1000×620, all custom drawn:
  - the World Controls as grouped knobs (double-click resets, tooltips);
  - a shaded, turning placeholder globe coloured by Temperature and Danger, with
    weather clouds and a Magic glow;
  - a layer mixer with level, M/S and activity meters (unbuilt layers dimmed and
    tagged with their milestone);
  - Root, Mood, Quality, Seed with a "New world" dice, Space Mix, Width, Master and a
    master meter;
  - 30 fps, skipped when the editor is not showing.

### Decisions

- **Deterministic randomness without state.** Every random value is a hash of
  (seed, stream, sample index). Gusts and breathing are Catmull-Rom curves through
  hashed points at fixed rates, blended by the controls. A knob therefore never jumps
  a modulation phase, and noise layers are a pure function of Seed and position.
- **Block-size independence.** The engine renders fixed 32-sample chunks into a
  small FIFO, so control updates land at the same positions whatever block size the
  host uses. An instrument has no input, so this adds no latency. A test checks that
  256- and 1000-sample hosts started at the same song position give the same output
  (to within 1e-4).
- **Transport.** While the host plays, the world follows `timeInSamples`; a jump or a
  play start resyncs the clock. While stopped, the world keeps evolving on its own
  clock. Oscillator phases (Tonal Bed) are **not** reset on a jump, because that
  would click. So exact repeatability holds from a fresh instance or a
  `prepareToPlay` (which is how offline bounces start). After a live transport jump,
  noise layers repeat at once but the bed's phases do not.
- **`reset()` does nothing.** Hosts call it on transport stops and locates. Clearing
  the reverb then would make the world drop out every time playback stops. Only
  `prepareToPlay` starts afresh, with a 2 s fade-in.
- **One program, "Current World", whose `setCurrentProgram` does nothing.** Some
  hosts call `setCurrentProgram(0)` on load, and that must never wipe the user's
  world. Factory worlds come in Milestone 7.
- **Tail 20 s.** A continuous generator should not be put to sleep by hosts that
  use the tail time.
- **Placeholder look, real controls.** Fonts are the system sans for now. Embedded OFL
  fonts, the resizable 75–150 % layout, the real Living Globe and the structure
  palette are Milestone 6.

### Tests (Linux, through `processBlock`), 284 checks

- Parameter table: unique IDs, defaults in range. The processor is an instrument
  with no input, stereo out, and accepts MIDI.
- Default world at 44.1/48/96 kHz × blocks 32/333/1024/4096: finite, −40…−10 dBFS,
  peak < −1 dBFS, stereo. Mono output is audible. Default world: **−21.6 dBFS RMS,
  −9.4 dBFS peak**.
- Every World Control at 0 and 1: finite, peak bounded, still audible. Each extreme
  changes the world measurably (> 0.2 dB in level, band, side or motion), and at
  least one extreme changes it clearly (> 1.5 dB). Measured largest change:
  Temperature 7.9, Weather 12.9, Wetness 1.7, Magic 2.3, Scale 3.3, Era 4.0, Danger
  1.7, Activity 1.5 dB. **Machinery, Life and Population do nothing yet** (their
  layers are Milestone 3); the test reports them as info.
- Layer solo: Air (−23 dBFS) and Tonal Bed (−26 dBFS) are audible and no louder
  than the mix. The other eight are silent until implemented. All-muted is silent;
  levels at −60 dB are near silent.
- Seed: same seed gives identical output, a different seed gives different output.
  The same transport start gives the same output at any block size.
- Power off: silent after 2 s, fades rather than cutting, and no 20 ms step is more
  than 1.5 dB worse than the world's own movement. Power on brings the world back.
  Master at minimum drops the level by more than 40 dB.
- Root/mood: 12 changes on the bed alone. No 40 ms level step above 6 dB (the
  voices beat while they glide; the steady drone has 2.4 dB steps on its own). No
  high-frequency click bursts.
- State: round trip of every parameter; garbage data is ignored.
- No heap allocation in `processBlock` at Eco, Standard and High, with odd block
  sizes and a MIDI event. 24 seeded random worlds are finite and below the ceiling.
- Editor: created at full size and paints (`--snapshot file.png` writes an image).
- CPU, High quality, every control at maximum, 48 kHz/256: **1.5 % of one core** of
  this container (a rough guide only; the brief's target is < 15 % on an Intel i5).

`WorldbuilderTests --probe` prints per-layer measurements for each control (a
tuning aid).

### Milestone 1 handoff

**Done:** pipeline with the new identity, instrument type, full M1 parameter tree,
Power/Play fade, Air and Tonal Bed layers, World Space reverb, master glue and
limiter, placeholder custom editor, and Linux tests (all passing).

**Next (needs the developer):**
1. CI on the macOS runner: universal build, bundle gate, quarantined install,
   `auval` and the harness on arm64 and x86_64.
2. The Milestone 1 gate in GarageBand: see [MANUAL_TEST.md](MANUAL_TEST.md).

**Result (reported by the developer):** confirmed in GarageBand. It loads as an
instrument, Air and Tonal Bed play, and Weather, Temperature, the Root/Mood glide and
Power all work. The interface looks and feels right; keep its style.

**Then Milestone 2:** Water, Fire & Heat, Ice & Earth and Thunder, plus the Climate
and Scale mappings for them.

**Known issues / limits:**
- The AU harness edits have never been compiled. If CI fails in "Build AU host
  harness", that is the first thing to fix.
- Machinery, Life and Population have no effect yet. Wetness only shapes the air and
  the World Space until Water arrives.
- Notes are accepted and ignored. MIDI events are Milestone 3.
- The bed's oscillator phases are not position-locked after a live transport jump
  (see Decisions).
- The sound has been checked only by measurement, not by ear.

---

## Milestone 2: climate layers (2026-10-24)

### Working conditions

Same as Milestone 1: Linux cloud container, no Mac. Verified here: Linux build with
GCC and Clang, **320 checks pass, 0 fail**. Sound checked by measurement and by
looking at spectrograms of rendered scenes (`--render`), **not by ear**. Nothing
macOS-specific changed (no new parameters, no harness or script changes).

### What was built

Four new layers, each its own class in `Source/DSP/`, plus shared event building
blocks in `Source/DSP/Events.h` (Poisson-style triggers, a two-pole modal resonator,
a gliding decaying sine "Ping", a cheap rumble-noise source, voice picking).

- **Water** (`WaterLayer`):
  - rain as three parts: a dense patter of tiny ticks with power-law sizes (up to
    about 3500 per second), bubbling drop voices (decaying sines whose pitch rises,
    as real drops do), and a quiet hiss bed;
  - drips: sparse pitched plinks, lower and longer in colossal worlds (caverns);
  - a babbling stream (bubbles, gurgling bands, a low rush) that grows into a
    waterfall roar when drenched;
  - lapping waves: two fixed-period wave clocks (calm swell, storm surf) blended
    by Weather; each wave swells, breaks and washes back with a little fizz.

  Wetness brings it in. Weather turns drizzle into a downpour. Frozen worlds get a
  soft snow hush instead of rain and the water stills; scorching worlds dry it up.
  Scale lowers and darkens it.
- **Fire & Heat** (`FireLayer`):
  - crackles and pops: noise bursts through resonant bands, arriving in clusters
    as a flicker curve flares and settles;
  - a low flickering roar, and the fizz of embers;
  - lava when scorching: low resonant bloops that rise in pitch, over a thick churn.

  Temperature brings it in (a faint campfire at temperate, a furnace at
  scorching). Wetness douses it, Weather fans it, and Era makes the crackle hotter
  and tighter (plasma rather than wood).
- **Ice & Earth** (`EarthLayer`):
  - ice creaks: stick-slip pulse trains through 3-mode resonators;
  - ice sheets cracking and singing: a sharp tick, then a falling dispersive
    "pew" (two gliding tones);
  - a low rumble that heaves (slowly when calm, restlessly when chaotic);
  - a tectonic groan (narrow noise band at 36-80 Hz drifting in pitch);
  - distant rockfalls: clusters of stone impacts (2-mode resonators) that build
    and thin out, heard through a distance low-pass.

  Cold brings the ice. Danger, heat and Scale bring the rumble, groan and rockfalls.
- **Thunder** (`ThunderLayer`):
  - Poisson-timed strikes, each at its own distance. The flash comes at once (the
    globe shows it) and the sound follows 0-2.2 s later, by distance;
  - close strikes tear with a crack (3-7 noise bursts over 0.1-0.35 s) and a sub
    thump; distant ones are a soft, dark roll;
  - every strike rumbles for 4-9 s: low-passed noise with an uneven rolling
    envelope that darkens as it fades.

  Weather (and Danger) set how often and how close: none below about Weather 40 %,
  about one every 7 s in a full storm. Scale puts the storm further away. At most
  3 strikes sound at once (2 in Eco); while all are busy the next strike waits,
  because cutting off a rumble would click.
- **Climate and Scale mappings**: every mapping is a function of the World
  Controls. `WaterLayer::amounts`, `FireLayer::amounts`, `EarthLayer::amounts` and
  `ThunderLayer::strikeRate` are public, so the tests and the globe use the same
  numbers the sound does.
- **Activity** now also sets event density: drips (0.3-6 per second), fire crackle
  density and the ground's heave. Its main targets (creatures, machines, MIDI
  events) arrive in Milestone 3.
- **Quality** scales voice counts: drops 6/10/16, drips 2/4/4, crackles 6/10/16,
  lava bloops 3/4/6, creaks 2/3/4, ice sheets 2/3/4, stones 6/9/12, thunder 2/3/3.
- **Editor** (same style):
  - the four layers are live in the mixer (only Milestone 3 layers are tagged);
  - tooltips describe the new mappings, and the header says Milestone 2;
  - the globe shows rain streaks slanted by the wind, snow, glowing lava fields
    turning with the planet, and a lightning flash with a forked bolt.
- **Tests and tools**: `--render file.wav seconds id=value ...` writes a WAV of
  any world (a listening aid), and `--probe` covers every implemented layer.

### Decisions

- **Events are hashed from the position, like the noise.** Each chunk (32 samples)
  or each sample (patter, embers) is a Bernoulli trial hashed from (seed, stream,
  position). The resulting timing is Poisson-like and never metronomic, and it is
  identical at any block size. The voices themselves (resonators, oscillators)
  carry state, so after a live transport jump they ring on while new events follow
  the new position. Exact repeatability from a fresh instance still holds; a test
  checks a busy world (rain, crackle, lava, rockfalls, thunder) at block sizes
  64 and 999 started at the same song position.
- **Random polarity for drops and bloops.** A decaying sine that always starts
  upward adds a small DC bias over thousands of drops. Each drop now starts in a
  random polarity.
- **Voice stealing takes the quietest voice** (or the nearest-to-silent for
  resonators). Thunder never steals: it holds the next strike instead.
- **No new parameters.** Milestone 2 only uses existing World Controls and layer
  strips, so automation and saved projects from Milestone 1 load unchanged.
- **Gain staging.** The default world moved from −21.6 to −20.9 dBFS RMS (the new
  layers are quiet at default: light drips, a faint campfire, a low rumble). A
  featured world for each layer puts its solo at −20 (Water, wetness 80 %) to
  −34 dBFS (Thunder, averaged over gaps between strikes). The slow loudness
  normaliser in the brief is still Milestone 3; the glue and the limiter keep
  peaks below −1 dBFS.

### Problems and fixes

- **Activity's ">1.5 dB at one extreme" check failed (1.42 dB).** In Milestone 1 it
  passed at exactly 1.5 dB. With more steady layers in the default world, the air's
  gusts are a smaller share of the mix. Activity changes how much and how fast the
  world moves, not its tone, so the band-level comparison was the wrong measure.
  The test now requires the 50 ms level movement at Activity max to be more than
  twice that at Activity min. Measured: 0.42 → 1.40 dB. The other World Controls
  keep the 1.5 dB check.
- **First tuning pass** (from spectrograms): fire crackles peaked at −7 dBFS in a
  temperate world (now about −13 dBFS); the rain hiss covered the patter; the ice
  creaks rang like tones (shorter resonator decays and more jitter made them
  rougher); and the thunder rumble faded too smoothly (now two layers of rolling
  swells).

### Tests (Linux, through `processBlock`), 320 checks

New or changed in this milestone:
- Layer solo: every implemented layer in a world that features it is audible,
  finite, stereo, no louder than that world's mix, and free of DC. Milestone 3
  layers are silent.
- Water: none when arid; drenched more than 10 dB above damp; Weather adds rain
  highs (more than 3 dB); frozen gives a quiet snow hush instead of rain; colossal
  Scale is darker.
- Fire: nothing when frozen; a faint campfire at temperate; scorching more than
  12 dB louder and more than 10 dB more low end (lava, roar); Wetness douses it.
- Ice & Earth: frozen is at least 6 dB brighter and moves more (creaks, cracks);
  Danger adds more than 4 dB of low end.
- Thunder: no strikes in still weather over 40 s; 10-60 strikes in 150 s of storm
  (measured 22); interval variation above 0.4 (irregular); strikes stand out
  from the gaps; colossal Scale is more than 3 dB darker.
- Busy-world block-size independence (above).
- No heap allocation at Eco, Standard and High, both scorching (fire, lava, storm,
  waves) and frozen (ice, snow), with odd block sizes and a MIDI event.
- CPU, High quality, every control at maximum, 48 kHz/256: **2.6 % of one core**
  of this container (was 1.5 % with two layers; a rough guide only).

### Milestone 2 handoff

**Done:** Water, Fire & Heat, Ice & Earth and Thunder layers. Climate (Temperature,
Weather, Wetness) and Scale drive them, and Danger and Activity drive them in part.
The globe shows rain, snow, lava and lightning. Linux tests all pass, with GCC and
Clang.

**Next (needs the developer):**
1. CI on the macOS runner (the AU, `auval` and the harness).
2. The Milestone 2 check in GarageBand: see [MANUAL_TEST.md](MANUAL_TEST.md).
   Mostly listening: do the new layers sound like rain, fire, ice and thunder?

**Then Milestone 3:** Machinery, Magic, Life and Population layers, the remaining
World Control mappings, MIDI events and level management (the slow loudness
normaliser).

**Known issues / limits:**
- None of the new sound has been heard by a person yet. Expect a tuning pass from
  the developer's listening notes (levels between layers especially).
- Machinery, Life and Population still do nothing. Notes are accepted and ignored.
- Lowering Quality while many events sound freezes the voices above the new count
  until Quality is raised again (they then finish their decay). This is harmless
  but not ideal.
- The globe's rain, snow, lava and lightning are drawn on the placeholder globe;
  the real Living Globe is Milestone 6.

---

## Milestone 3: musical retune, Place engine, Castle, Village, Spaceport (2026-10-24)

Direction changed after Milestone 2 ([DIRECTION_UPDATE.md](DIRECTION_UPDATE.md),
which overrides the brief): WORLDBUILDER is now a film-scoring instrument. It is
musical rather than literal, in the song's key and tempo, and **Places** are the
centrepiece. The revised milestone list is in that file.

### Working conditions

Same as before: Linux cloud container, no Mac. Verified here: Linux build (VST3,
Standalone, tests) with GCC and Clang, **647 checks pass, 0 fail** on both. Sound was
checked by measurement, spectrograms, chroma and event logs, **not by ear**. The
editor was rendered and inspected (`--snapshot`). Nothing macOS-specific changed.

Note: the Milestone 2 commit was on the `milestone-2` branch and `main` was still at
Milestone 1. This milestone builds on Milestone 2 and fast-forwards `main` through it.

### What was built

- **Music core** (`Source/DSP/Music.h`):
  - scales for the nine Moods (Chromatic uses the half-whole octatonic for melodies);
  - scale degrees, chord tones and quantising to the scale;
  - one shared chord progression, a chord every four bars: i–VI–iv–v when calm, and
    i–ii–i–VI once Danger passes 55 %. Every Place and the drips use it, so nothing
    clashes;
  - a world beat clock in the engine. It follows the host tempo and its PPQ position
    at transport jumps; without a host tempo it runs at 96 BPM.
- **Place framework** (`Source/Places/`):
  - `Place` is the engine interface. It comes with a step sequencer with swing, a
    pattern hash in which Variation is the chance that a step departs from a repeating
    4-bar pattern (16 bars for sparse events), a phrase player, and an event log for
    the tests;
  - `PlaceSlot` hosts one Place:
    - a 0.3 s fade-out and fade-in when the type changes;
    - it folds the World Controls into the Place's settings;
    - distance makes a Place quieter, darker, narrower and wetter; pan; a DC blocker;
    - synced or free beat. A free Place uses its own tempo scaled by Activity, but its
      harmony still follows the shared chords.
  - Shared voices (`Source/DSP/Voices.h`):
    - PolyBLEP saw;
    - an envelope;
    - a 3-saw synth voice with filter envelope, glide and delayed vibrato;
    - a Karplus-Strong pluck;
    - a modal bell or bar with a raised-cosine strike;
    - a tuned two-partial drum.
  - Place effects (`Source/DSP/PlaceFx.h`): a 4-line FDN room or hall, a tape echo
    (wow, flutter and saturating feedback), a ping-pong delay, and tape warmth.
- **Castle**:
  - elements: Drones, Bells, Horns, Pulse, Hall;
  - brass-like drones on a tonic pedal plus the chord;
  - bells whose partials are tuned to the scale's own third and fifth;
  - horn calls;
  - timpani tuned to the root and the fifth below, plus a side drum;
  - a stone hall, and a dotted-quarter tape echo whose depth follows
    Activity × Population;
  - Characters: Fortress (open fifths, tolls, half-note processional), Royal Court
    (triads, carillon runs, fanfares in thirds, a march), Besieged (a trembling drone,
    alarm bells, a driving 3-3-2 ostinato).
- **Village**:
  - elements: Plucks, Drone, Hand Drums, Wood, Tape;
  - Karplus-Strong ostinatos on chord tones with passing notes;
  - a nasal root-and-fifth drone;
  - frame drum, hand drum and shaker;
  - marimba-like wood;
  - a small room and tape warmth;
  - Characters: Market Day (eighths), Hearth (slow arpeggios, heartbeat drum,
    wind-chime wood), Festival (swung sixteenths, xylophone runs).
- **Spaceport**:
  - elements: Pulses, Pads, Risers, Announce, Engines;
  - resonant 16th arpeggios;
  - four wide gliding pads;
  - noise risers: pass-bys, a launch build, falling sweeps;
  - PA chimes plus an abstract two-formant murmur (never speech);
  - a sub-engine throb at half notes;
  - a hangar and a dotted-eighth ping-pong delay;
  - Characters: Busy Hub, Abandoned Dock (sparse, detuned, flat broken chimes), Launch
    (an 8-bar build cycle, countdown beeps and a tuned impact).
- **Every Place**: Tension darkens the harmony as it rises. The fifth or third fades
  out near 60 % and returns as a semitone cluster above the root (no jump). Rhythm
  tightens and decays shorten.
- **World Controls on the Places**:
  - Population scales Activity (×0.5–1.5) and the effect depth;
  - Machinery adds density, removes swing and adds anvil rings or a mill clack;
  - Life adds grace notes, looser velocity, vibrato, more calls and chimes;
  - Danger adds Tension and switches to the dark progression;
  - Era and Temperature set Tone; Scale sets Size and pushes Places further away;
  - Magic lengthens the halls.
- **Musical retune of the environment**:
  - **Drips (the fix asked for):** tuned, softly ringing drops (modal; partials 1, 2 and
    4.07) on the current chord's tones, mostly two octaves up, lower and longer in
    caverns. A tiny rising "plink" leads into each. Timing comes in loose Poisson
    clusters; velocity follows a power law; decay (T60 0.4–2.4 s) and pan are random.
    Sometimes a smaller second drop follows. A little room of early reflections.
  - Rain: drops land on scale notes, and occasional raindrops ring as drip-like
    plucks. Four narrow resonances on the current chord, rung by the patter, make a
    shimmer.
  - Wind: whistles glide between scale notes; the howl rises through root, fifth and
    octave.
  - Thunder: each strike lands as a tuned low impact on the root plus a swell on the
    root and fifth; the rumble is slightly lower.
  - Lava bloops and singing ice start on scale notes (the ice's second tone on the
    scale's fifth). The tectonic groan drifts between the low root, fifth and octave.
- **Removed:** the Machinery, Magic, Life and Population *layers* and their 12
  parameters. They were never implemented: their roles are now the Places and the
  Tonal Bed (Magic). The World Controls of those names stay, with the same IDs.
- **Parameters**:
  - 4 slots × 26: type (Empty plus all ten Places, with the last seven silent until
    Milestone 4, so the range never changes), level, distance, pan, mute, solo,
    character, activity, tension, sync, density, swing, tone, size, variation, seed,
    five element levels and five switches;
  - 141 parameters in total;
  - slot 1 defaults to a Castle, so a fresh instance plays the new concept at once;
  - Population's default moved from 0 to 50 % (it now means "as set" for every Place).
- **Editor** (1160 × 720, same style):
  - Place cards with original vector art for all ten Places (the Milestone 4 ones are
    dimmed); clicking a card fills the next empty slot with a fresh Place and a new seed;
  - four Place strips: level, distance, pan, M/S, EDIT, remove, an activity meter;
  - a detail panel over the globe: Characters, the five knobs, Sync, Density, Swing,
    Seed, and element switches and levels;
  - landmarks on the globe at each Place's pan and distance, glowing with its level;
    drag one to move the Place, double-click to edit it;
  - the host BPM in the footer.
- **Tools:** `--probe-places`. The `--snapshot` option also writes `-detail.png`.

### Decisions

- **One harmonic clock for everything.** Places that are free in rhythm still follow
  the shared chords, so four Places never clash. The Tonal Bed holds the root, which
  works as a pedal under every chord of both progressions.
- **Determinism kept.** Every random choice is hashed from (seed, step) or (seed,
  position). The world beat accumulates per 32-sample chunk, so the output is identical
  at any block size (tested with four Places, one of them free-running). Each Place's
  key mixes the world seed, the Place's seed and the slot.
- **Events start at chunk boundaries** (0.67 ms at 48 kHz), not per sample. This is
  inaudible and keeps the voices simple.
- **Decays are T60s.** Bells, drums and drips take "time to fall 60 dB". At first they
  were treated as time constants, so bells rang about seven times too long and too loud.
- **Soft strikes.** Modal voices are struck by a raised-cosine pulse of up to half a
  period of the note, so drips and bells swell in over a few milliseconds instead of
  clicking on.
- **Level target:** each Place solo sits at −27 to −35 dBFS RMS with peaks about
  −12 dBFS, as a bed under a score. The default world (environment plus Castle)
  measures −20.3 dBFS RMS on the test window, the same as Milestone 2's −20.9.

### Problems and fixes

- Bells, wood and chimes were silent: a voice retired itself in the chunk it started,
  before it was struck. Fixed.
- Sparse events (bells, horns) used the 4-bar pattern, so a seed could simply never
  ring them. They now use a 16-bar pattern.
- DC offset: the Karplus-Strong loop passes DC, and runs of timpani that all started
  upward added up. Fixed with alternate drum polarity and a 12 Hz DC blocker per slot
  (20 Hz on the Village plucks).
- Onset counting from audio could not tell how busy a Place was: drones and halls
  mask the onsets. The tests use each Place's event log for busyness and tempo.
- Rain came out 2 dB louder with peaks 3 dB hotter, and then too thin. It was
  rebalanced to Milestone 2's level and midrange.

### Tests (Linux, through `processBlock`), 647 checks

New in this milestone:
- **Each Place × Character**, 32 s, D minor at 120 BPM:
  - finite; −42…−18 dBFS RMS; peaks below −4.4 dBFS; stereo; no DC;
  - **in key**: the in-scale pitch classes are 6–11 dB above the rest on average.
    The threshold is 5 dB, because saw drones carry natural off-scale harmonics;
  - the three Characters differ from each other by more than 1 dB in some band.
- **Each Place**:
  - follows G major;
  - Activity gives more than 1.3× the notes;
  - Tension changes it by more than 1.5 dB; Tone brightens it by more than 3 dB;
  - every element changes it, and all elements off is silent;
  - its seed sets its patterns;
  - **tempo**: synced, every note sits on the host's sixteenth grid at 84 and 132 BPM
    (vector strength above 0.9; below 0.5 off-tempo); free, it keeps its own tempo.
- **World Controls on each Place**: Population (more notes, and an audible change),
  Machinery, Life, Danger, Era (brighter) and Scale (further away).
- **Slots**:
  - distance (quieter and darker); pan both ways;
  - a Place solo is exactly the Place alone, and a layer solo silences the Places;
  - a Milestone 4 Place is silent;
  - switching Places five times makes no clicks (the HF burst check);
  - four Places in a storm stay finite and below the ceiling (−13.6 dBFS RMS).
- **Environment in key**:
  - drips in D minor, G major and A Phrygian, 8–11 dB in-scale gap;
  - drip intervals vary (CV above 0.5), and the drips are spread across the stereo;
  - rain and wind lean to the key;
  - thunder's strongest low pitch class is the root or the fifth.
- **Existing tests**:
  - the World Controls test now measures the environment with the Castle muted.
    Machinery, Life and Population are informational there and are tested on the
    Places;
  - the busy-world block-size check now includes four Places;
  - the allocation check covers four Places of every Character;
  - the state round trip includes Place parameters;
  - the editor test covers the card behaviour (fill next slot, refuse Milestone 4
    Places, at most four) and paints the detail panel.
- **CPU**, High quality, every control at maximum, four busy Places, 48 kHz/256:
  **7–9 % of one core** of this container (was 2.6 % without Places; a rough guide).

### Milestone 3 handoff

**Done:**
- the drip fix and a musical retune of every environment layer to key;
- key and tempo sync for everything tonal and rhythmic;
- the Place framework (slots, strips, detail panel, element switches, Character,
  Activity, Tension, Rhythm, Seed);
- Castle, Village and Spaceport, each with three Characters;
- the Place selector cards and the globe landmarks;
- the old Machinery, Magic, Life and Population layers are removed, and their World
  Controls now shape the Places;
- Linux tests all pass, with GCC and Clang.

**Next (needs the developer):**
1. CI on the macOS runner: the AU, `auval` and the harness. The parameter count
   changed (49 → 141); the harness only looks parameters up by name.
2. The Milestone 3 check in GarageBand: see [MANUAL_TEST.md](MANUAL_TEST.md). This is
   mostly listening. Do the drips sound beautiful now? Do the three Places score their
   settings? Do they follow the song's key and tempo?

**Then Milestone 4:**
- Factory, City, Temple, Forest, Ruins, Cavern and Alien Hive (the slots already
  accept them; add a class, a case in `PlaceSlot::engineFor`, its `placeInfo` entry
  and `placeImplemented`);
- MIDI events per Place, in key.

**Known issues / limits:**
- None of it has been heard by a person yet. Expect a tuning pass: the relative levels
  of elements, the murmur's character, and whether the horn calls are frequent enough.
- Old projects: the removed layer parameters are ignored on load. Population now
  defaults to 50 %, so a Milestone 1/2 project saved with Population at 0 loads with
  0, which makes its Places sparser. Slot 1 defaults to a Castle only for a fresh
  instance or a missing value: an old project, which has no Place parameters, also
  loads with a Castle. Remove it with X if it is not wanted.
- Tempo changes during playback are followed block by block. A host tempo map that
  changes inside a block lands at the next block.
- Events are quantised to 32-sample chunks. Free-running Places keep their own phase
  after a host transport jump; synced ones resync to the host PPQ.
- Four Places at once plus a storm reach about −13.6 dBFS RMS. That is fine for a
  dense scene, but no longer quiet.
- The globe, landmarks and cards are still the placeholder style. The full Living
  Globe and the resizable layout are Milestone 6.
