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
