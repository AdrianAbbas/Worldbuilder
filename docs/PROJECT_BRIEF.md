# WORLDBUILDER — Procedural Soundscape Engine (Claude Code brief)

You are a senior C++ audio DSP engineer and JUCE plugin developer with strong sound-design and UI taste. Build **WORLDBUILDER**, a real, shippable Audio Unit plugin for **Logic Pro and GarageBand on macOS** (Apple Silicon and Intel) that **generates** fantasy and science-fiction environments entirely from synthesis.

The user describes a world with a handful of high-level controls (temperature, weather, machinery, population, scale…) and WORLDBUILDER builds a living, evolving soundscape from layered procedural sound generators. Its signature feature is **Locations**: save complete world states (including the structures on the globe) and travel between them smoothly, under automation.

All sound must be synthesised in real time. **No samples, no recordings, no copyrighted material.**

---

## 0. HOW TO WORK (read first)

**The developer's setup:** Claude Code runs on a Windows PC or in the cloud. The developer's Mac is used only to test finished builds in GarageBand. **All macOS building, signing and testing happens on GitHub Actions macOS runners.** The developer has a limited daily usage budget, so efficiency matters as much as quality.

**Reuse the proven VEIL pipeline.** The repo https://github.com/AdrianAbbas/Dark-Fantasy-Plugin contains a working, GarageBand-verified setup. Copy and adapt rather than reinvent:
- `CMakeLists.txt` structure (JUCE 8.0.16, `AU_SANDBOX_SAFE TRUE`, universal arm64 + x86_64, deployment target 10.15);
- `.github/workflows/build.yml` (universal build, `lipo` check, ad-hoc signing, quarantined-install test, auval, artifact packaging);
- `scripts/install-mac.sh`, `scripts/uninstall-mac.sh`, `INSTALL.txt`;
- `tools/au-harness/` (the AudioToolbox host test: in-process and out-of-process);
- `docs/DEVLOG.md` lessons, especially the GarageBand root-cause findings.

Read VEIL's DEVLOG first so you don't repeat solved problems. Use a new AU identity for this plugin (manufacturer `Veil` can stay; new subtype, e.g. `Wbld`; new bundle ID and plugin name).

**Rules:**
- Work in milestones (Section 6). Commit and push at the end of each. Stop after each milestone and tell the developer what to test.
- **After pushing, stop. Never wait for or poll CI.** The developer will report the result.
- When CI fails: read only the last 100 lines of the failing step. At most 2 fix attempts, then stop and explain in plain words.
- Don't write anything you can't compile here without saying so clearly. Prefer code that the Linux CI job also compiles and tests.
- Never claim something works unless it has been tested. auval passing does not mean it works in GarageBand.
- Prefer a simple generator that sounds great over an ambitious one that sounds synthetic or broken.
- Keep `docs/DEVLOG.md` updated, ending with a short handoff (done / next / known issues) so any fresh session can continue.

---

## 1. PLUGIN TYPE AND PLATFORM

- **Type: software instrument** (`IS_SYNTH TRUE`, `kAudioUnitType_MusicDevice`), **stereo output, no audio input.** This is deliberate: GarageBand and Logic only reliably host generators as instruments, and an instrument track gives the plugin a place to live and be automated.
- **It plays continuously while the plugin is active**, with or without MIDI. A global **Power/Play** toggle (automatable) fades the world in and out over ~2 s.
- **Optional MIDI:** incoming notes fire one-shot "events" from the current world (thunder strike, creature call, machine clank, spell burst). Note pitch selects the event type; velocity sets intensity. This makes the soundscape playable for scoring.
- Formats: AUv2 (primary), VST3, Standalone (dev only).
- Universal binary (arm64 + x86_64), macOS 10.15+, sandbox-safe, ad-hoc signed in CI (reuse VEIL's setup).
- Real-time safe: no allocation, locks, file I/O or logging on the audio thread; everything allocated in `prepareToPlay`; denormal protection; NaN/Inf guards per layer; correct behaviour from 44.1 to 192 kHz and any block size.
- **CPU budget:** under ~15% of one core on an Intel i5 MacBook at 48 kHz/256 with every layer active. Include a quality setting (Eco / Standard / High) that scales voice counts and oversampling.
- **Deterministic:** all randomness comes from a seeded generator. A **Seed** parameter plus transport position makes playback and offline bounce repeatable from the same start point.

---

## 2. THE WORLD CONTROLS (macro layer)

These are the controls the user mainly touches. Each one drives many internal layer parameters through hand-designed mapping curves, so turning one knob changes the world believably rather than just one volume.

| Control | Range | What it changes |
|---|---|---|
| **Temperature** | frozen → scorching | Frozen: brittle ice creaks, thin high wind, crystalline shimmer, dull low end. Scorching: heat-haze rumble, crackling fire, lava bubbling, dry air. |
| **Weather** | still → storm | Wind strength and gustiness, rain density, thunder frequency and distance. |
| **Wetness** | arid → drenched | Rain vs. dust, dripping, streams and waterfalls, water lapping. |
| **Machinery** | none → industrial | Hums, engines, turbines, pistons, servo whirs, electrical arcs, distant factory pulse. |
| **Magic** | mundane → arcane | Shimmering drones, chimes, resonant crystal tones, energy swells, rune pulses. |
| **Life** | barren → teeming | Creature chirps, insects, bird-like calls, distant beasts. |
| **Population** | empty → crowded | Abstract crowd murmur, footsteps, distant activity, bells. Keep it synthetic and textural, never fake speech. |
| **Scale** | intimate → colossal | Reverb size and pre-delay, distance filtering, layer spread, low-end weight, how far away events seem. |
| **Era** | ancient → futuristic | Biases every layer: ancient = organic, wooden, stone, fire; futuristic = synthetic, metallic, digital, sci-fi. Works with Machinery and Magic (ancient + machinery = steampunk mills; futuristic + magic = cosmic energy). |
| **Danger** | peaceful → hostile | Darker tuning, dissonant drones, faster rhythms, more aggressive events, sub-bass pressure. |
| **Activity** | calm → chaotic | Event density and how fast the world evolves. |

Global: **Master Level**, **Width**, **Seed**, **Quality**, **Power/Play**, **Space Mix** (amount of the world reverb).

---

## 3. THE SOUND LAYERS (procedural generators)

Build each as its own class with its own tests. Every layer has: Level, Mute/Solo, and its internal parameters driven by the World Controls. Focus on **convincing, organic sound**: smoothly evolving modulation, randomised timing (Poisson-distributed events, never metronomic), stereo placement, distance cues (level, high-cut, more reverb when far).

1. **Air** — wind from filtered noise with gust envelopes, whistling resonances, howls when stormy. Frozen = thin and high; scorching = low dry rush.
2. **Water** — rain (dense tiny filtered impulses with randomised resonances), drips (pitched resonant pings), streams/waterfalls (shaped noise with movement), lapping waves.
3. **Fire & Heat** — crackle and pop (random impulses through resonators), roar (low noise with flicker), lava bubbling (low resonant blips).
4. **Ice & Earth** — creaks and cracks (modal resonators excited by impulses), rumbles, distant rockfalls, low tectonic groan.
5. **Thunder** — distant rumbles and close strikes: crack transient + long low-passed rumble, delayed by distance.
6. **Machinery** — hums with harmonics, engines (FM/additive with cycle rhythm), pistons/clanks (modal impacts on a loose rhythm), servo sweeps, electrical arcs (filtered noise bursts with crackle).
7. **Magic** — evolving drones (additive/FM pads tuned to a selectable root and scale), chimes and crystal tones (modal synthesis), energy swells and rune pulses.
8. **Life** — creature and insect calls from chirp synthesis (FM sweeps with formant shaping), insect drones, occasional distant beast calls (low growl synthesis). Varied enough not to sound repetitive.
9. **Population** — abstract murmur (formant-filtered noise bands with random syllable-like envelopes, never speech), footsteps (filtered impacts), distant bells.
10. **Tonal Bed** — a quiet harmonic drone tying the world together. **Root note** and **Scale/Mood** (e.g. minor, Phrygian, Lydian, whole-tone, chromatic) controls let the soundscape sit in the key of a song. Magic and Machinery tonal elements follow the same root.

**World Space:** all layers feed a shared stereo space: an FDN reverb whose size, decay and tone are driven by Scale, Temperature and Wetness (for example, ice caverns sound bright and long; a jungle sounds dense and damped). Then a gentle master glue compressor and a transparent safety limiter.

**Level management:** however the controls are set, the output should stay musically balanced and never clip. Use per-layer gain staging plus a slow loudness normaliser on the master so extreme worlds don't jump in level.

---

## 3B. STRUCTURES — BUILD THE WORLD BY PLACING THINGS ON IT

Alongside the World Controls, the user builds the world by **placing structures on a spinning globe** (Section 5). Each structure type brings its own sounds and its own effect character. How many of a type are placed, and where, shapes the result.

| Structure | Sounds it adds | Effect character it adds |
|---|---|---|
| **Castle** | distant bells, banners in wind, horses, forge hammering, horns | stone hall reverb, slow tape-style echo |
| **Village** | murmur, footsteps, livestock, cartwheels, hearth crackle | warm tape saturation, gentle wow & flutter |
| **Factory** | engines, pistons, steam hiss, conveyor clatter, sirens | gritty saturation, metallic comb resonance |
| **Space Port** | ship engines, landing thrusters, PA tones, energy hum | wide stereo, ping-pong delay, sci-fi shimmer |
| **Temple / Shrine** | chants (abstract, no real speech), chimes, gongs, drones | long shimmer reverb, choral resonance |
| **Forest** | wind in leaves, birds, insects, creaks | soft diffusion, damped space |
| **Ruins** | crumbling stone, eerie wind, distant echoes | lo-fi decay, reverse echoes |
| **Volcano** | rumble, lava bubbling, eruptions | heavy low-end, distortion bursts |
| **Lake / Ocean** | waves, lapping, gulls or alien water creatures | modulated chorus, watery filter movement |
| **Crystal Spire** | resonant crystal tones, magical swells | bright modal resonance, glassy delay |

Rules:
- **Count = population and intensity.** One castle is a distant, quiet presence; five castles make a bustling kingdom. Each structure type has a population value (0–8) that scales the density, loudness and variety of its sounds **and** the depth of its effect character. For example, more villages increase the tape effect's wow, flutter and echo feedback; more space ports widen the stereo field and add more delay repeats.
- **Placement matters.** A structure's position on the globe sets its stereo pan. Distance from the "listener point" (the front-centre of the globe) sets how far away it sounds: quieter, darker, more reverberant. As the globe spins, the sounds pan and move with it, so the world audibly turns around the listener.
- **Spin** has its own controls: Spin Speed (including stopped and tempo-synced) and Spin Direction. Spin is automatable.
- Structures combine with the World Controls. Era changes their flavour (an ancient factory is a watermill and smithy; a futuristic one is a reactor), Weather and Temperature affect all of them, and Danger can set structures "under siege" (alarms, fire, chaos).
- Each structure's effect is a real DSP block in a shared **World FX** chain: tape delay with wow/flutter and saturation, shimmer reverb, comb resonator, ping-pong delay, chorus, lo-fi. Each block's depth is the weighted sum of the structures that use it, smoothed and level-safe.
- Structures are saved in plugin state and in Locations, so travelling between Locations can also rebuild the globe (structures fade in and out of view and sound).
- Expose automatable parameters for each structure type's population, plus Spin Speed. Positions are saved in state, not automated.
- Limit: up to 32 placed structures total, with voice limits scaled by the Quality setting, to keep CPU reasonable.

## 4. SIGNATURE FEATURE — LOCATIONS & JOURNEYS

**Locations**: a Location is a saved snapshot of the complete world: every World Control, layer level, root/scale, seed and space setting.

- **8 Location slots**, each with a name and a colour, shown as markers around the **Living Globe** (Section 5). Click a slot to save the current world into it; rename it (e.g. "Frozen Gate", "Clockwork City", "Ember Market").
- **Travel:** selecting another Location travels there over a **Travel Time** (0.5 s – 60 s, or tempo-synced in bars). Every parameter interpolates along a smooth curve (ease-in-out).
- **Travel must sound like moving through a world, not a crossfade.** During travel:
  - continuous layers (wind, hum, drones, rain) morph their parameters smoothly;
  - event layers ramp density out of the old world and into the new;
  - layers belonging only to the destination fade in from a distance (low-passed, reverberant) and approach; layers only in the origin recede the same way;
  - tonal elements glide or crossfade to the new root/scale without dissonant clashes.
- **Automation:** expose **Destination** (choice 1–8) and **Travel Time** as automatable parameters, plus a continuous **Journey** parameter (0–1) that morphs directly between two chosen Locations (**From** and **To**). The Journey parameter lets the user draw an automation curve that walks the listener from one place to another in sync with a film or a song.
- **Journey Path (stretch goal for a later milestone):** chain up to 8 Locations into a path that plays automatically over a set number of bars, looping or one-way.
- Locations save inside the project (plugin state) and can be exported/imported as `.world` preset files.
- **Never** produce clicks, level jumps or silence gaps during travel. Add a test that renders a travel between two very different Locations and asserts smooth level (no frame-to-frame jumps beyond a threshold) and no silence.

---

## 5. INTERFACE

The plugin should look like a premium cartographer's instrument: part fantasy map, part sci-fi navigation console.

- **Centre: the Living Globe.** A large, slowly spinning planet drawn procedurally (vector/2D shading that simulates 3D; no OpenGL). Its look reflects the world: colour from Temperature, animated clouds, rain or snow from Weather and Wetness, smoke and city lights from Machinery and Population, aurora or rune-glow from Magic, a darker atmosphere from Danger, and land style from Era.
  - **Structure palette** beside the globe: icons for Castle, Village, Factory, Space Port, Temple, Forest, Ruins, Volcano, Lake, Crystal Spire. Click a structure, then click the globe to place it; it appears as a small, original illustrated icon on the surface and turns with the planet. Drag to move, right-click (or Alt-click) to remove. Each placement animates in (rising from the ground, lights flickering on).
  - Structures on the side facing the listener glow brighter; those on the far side dim, matching how they sound.
  - Population counts per structure type are shown and adjustable beside the palette.
  - The 8 Locations appear as small named markers around the globe's orbit; during travel, the globe spins toward the destination and the scene transforms.
- **Left: World Controls** as large, beautiful custom knobs, grouped (Climate: Temperature / Weather / Wetness; Civilisation: Machinery / Population / Era; Mystery: Magic / Life / Danger; Size: Scale / Activity).
- **Right: Layer Mixer**: compact strips with level, mute/solo and a small activity meter per layer.
- **Bottom:** Location slots (save, rename, colour), Travel Time, Journey From/To and Journey slider, Root/Scale, Seed (with a dice "new world" button), Quality, master meter, Power/Play.
- **Top:** WORLDBUILDER wordmark, preset browser (prev/next, save, load), settings.
- Visual style: deep navy/charcoal background, parchment-and-brass map tones blending into cool sci-fi cyan accents, clean readable typography (SIL Open Font License fonts embedded via BinaryData, licences included). Custom-drawn controls only, no default JUCE look.
- Knobs: smooth drag, shift for fine control, double-click reset, value readout, tooltips, host automation linkage.
- Performance: cache static layers, animate at ≤ 30 fps, pause animation when hidden, software rendering (no OpenGL), resizable 75–150% with fixed aspect ratio.

---

## 6. MILESTONES

Each milestone ends with: Linux tests pass, push, DEVLOG handoff, stop. The developer tests the CI build in GarageBand and reports back.

1. **Skeleton:** copy the VEIL pipeline, new identity, instrument type, parameter tree, Power/Play, Air + Tonal Bed layers, World Space reverb, simple placeholder editor. **Gate:** CI green; developer confirms WORLDBUILDER loads on a GarageBand software instrument track, plays sound immediately, custom editor appears.
2. **Layers part 1:** Water, Fire & Heat, Ice & Earth, Thunder, and the World Controls mapping for Climate and Scale.
3. **Layers part 2:** Machinery, Magic, Life, Population, the remaining World Controls, MIDI events, level management.
4. **Structures & World FX:** placing structures, population scaling, globe-position panning and distance, spin, and the World FX chain.
5. **Locations & Journeys:** slots, travel, automation parameters, state saving, travel tests.
6. **Full interface:** Living Globe, structure palette, mixer, all controls, animation.
7. **Presets & release:** at least 15 factory worlds (e.g. Frozen Citadel, Clockwork Metropolis, Dragon's Caldera, Drowned Temple, Neon Rainfall, Orbital Station, Haunted Moor, Desert Bazaar, Crystal Caverns, Storm Coast, Abandoned Reactor, Elven Canopy, Volcanic Forge, Alien Jungle, Void Between Stars), each with its own set of structures and 2–4 Locations pre-saved so travel can be heard immediately. Plus README with install, GarageBand/Logic instructions, a control guide and troubleshooting.

---

## 7. TESTING (keep it lean)

Linux CI (hard gate), through the real `processBlock`:
- Default world produces meaningful, finite stereo output at 44.1/48/96 kHz and several block sizes.
- Every World Control at min and max: finite output, peak bounded, and a measurable change compared to the default.
- Each layer solo: audible and finite.
- Same seed + same settings → identical output; different seed → different output.
- Travel between two contrasting Locations: no clicks, no level jumps, no silence.
- Each structure type at population 1 and 8: audible, finite, and clearly louder/denser at 8; its World FX block measurably deepens.
- Structure position changes the stereo balance; spin moves it over time.
- State round trip, including Locations.
- No heap allocation in `processBlock`.
- Every factory preset: audible, finite, within a sensible loudness window.

macOS CI: the reused auval + AU host harness (in-process, rendering with no input, checking non-zero output and editor creation). Keep any experimental harness checks report-only.

---

## 8. DEFINITION OF DONE

1. A universal, signed `WORLDBUILDER.component` is built by CI and installs with the script.
2. The developer confirms in GarageBand: it plays immediately, the interface appears, the World Controls audibly reshape the world, and travelling between Locations sounds like moving through a place.
3. Ten layers, the World Controls, Locations with automatable travel, the Living Globe interface with placeable Structures, and 15+ factory worlds are implemented.
4. Tests and validators pass; README and DEVLOG are complete and honest about limitations.

Start with Milestone 1: read VEIL's DEVLOG and build pipeline, then set up WORLDBUILDER's skeleton.
