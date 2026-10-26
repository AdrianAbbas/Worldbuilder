# WORLDBUILDER

A film-scoring soundscape instrument for Logic Pro and GarageBand on macOS (Apple
Silicon and Intel, macOS 10.15+). It scores a place rather than recording it:
musical, tonal and rhythmic sound in your song's key and tempo that evokes a castle,
a forge, a city at night, a forest or a cavern, over a living environment of wind,
water, fire, ice and thunder. Everything is synthesised in real time. It uses no samples and no
recordings.

**Status: Milestone 10 (26 Places of six instruments, each with its own instrument
effects; Score Intensity; render to WAV and scene MIDI export; Locations and
Journeys; seven planet types with their own sound colour; the revamped globe and
controls).**
These work now:
- **Places**, the centrepiece. Up to four play at once. All 26 are built: Castle,
  Village, Spaceport, Factory / Forge, City, Forest, Alien Hive, Temple, Cavern / Mine,
  Ruins, Spaceship Interior, Biodome, Sea Ship, Suburbs, Mall, Military Camp, Beach,
  Desert, Palace and Tavern, and six more sci-fi Places: Space Station, Cyberpunk
  Megacity, Starship Bridge, Asteroid Mining Colony, Alien Ruins and Nebula / Deep Space.
  - Each is an ensemble of **six instruments**, for example the Castle's brass drone,
    tuned bells, horn calls, timpani, harp and wordless choir, or the Factory's tuned
    anvil, pistons, steam, bowed metal, gear clicks and furnace bass.
  - Each has three Characters that change its patterns, harmony and sound.
  - Every instrument has its own pattern, level, on/off switch, mute and solo.
  - Each Place has its own effect character (stone hall and tape echo; gritty
    saturation and a comb resonance tuned to the chord; a ring-modulated shimmer and a
    warped delay; a long dark cave and a slapback; a glossy, detuned mall that slows
    into vaporwave; a gritty field radio...).
  - Each Place colours the interface and can decorate its edges with an animated
    overlay (grass, ivy and fireflies; girders, pipes and steam; neon signs and a
    night skyline; vines and falling leaves; tendrils and glowing pods; gold trim and
    incense; crystals and dripping water; neon storefronts; barbed wire and sandbags;
    drifting sand and lanterns; chandeliers and velvet drapes...), blended when
    several play. Overlays:
    Full, Subtle or **Off (the default)**.
  - Controls: Activity, Tension, Tone, Size, Variation, Density, Swing, Sync (to the
    host tempo, or the Place's own free tempo) and a Seed.
  - Each slot also has level, distance and pan.
- **Patterns.** Every instrument (drones and pads too) plays from a pattern of notes. The Place generates it from
  its seed and knobs until you edit it in the **Pattern Editor** (PATTERNS in a
  Place's detail panel):
  - a piano roll on the scale's notes (or all twelve semitones, or a drum kit), with
    velocity and chance lanes and a playhead;
  - draw, select, copy and paste, transpose, double and halve, lock, undo and redo,
    and Generate for a fresh pattern;
  - controlled randomness: how much, and what may vary (timing, velocity, pitch,
    skipped and extra notes); locked notes never change; Evolve slowly mutates the
    pattern and can return to it every N bars;
  - MIDI files: drop one on the editor to import it, drag a pattern out to the host,
    or save it as a .mid file.
- **Score Intensity**, the large knob in the header: every active Place at once, from
  a calm bed (0 %) through the Places as set (50 %) to a climax (100 %): busier,
  tenser, brighter and louder, each Place on its own curve. It glides rather than
  jumps; automate it to shape a cue over time.
- **Capture** (the footer's CAPTURE section): **RENDER** the current world offline to a
  WAV file (the full mix, or stems: one per Place plus the environment), and **MIDI**
  exports the patterns of every active instrument as one multi-track MIDI file. Both
  from bar 1 at the host's tempo, for 4 to 64 bars. See
  [Getting WORLDBUILDER into your song](#getting-worldbuilder-into-your-song).
- **Locations and Journeys** (the LOCATIONS row): save up to eight complete worlds and
  travel between them so it sounds like moving through a place, not a crossfade. See
  [Locations and Journeys](#locations-and-journeys).
- **Planet** (the PLANET menu by the globe): Rocky, Ocean, Ice Giant, Gas Giant,
  Desert World, Lava World or Forest World. Each one changes the globe (cratered rock,
  oceans under moving clouds, banded ice with rings, a storm-swirled giant with rings,
  dunes, glowing cracks, green continents) and gives the whole world a sound colour:
  Rocky is dry and close, Ocean has a watery, rolling movement, Ice Giant a cold
  crystalline shimmer, Gas Giant a vast swirling space and deep sub, Desert a dusty
  high cut and width, Lava heat haze, saturation and rumble, Forest a soft, damped air.
  **INFLUENCE** sets how strong the colour is (35 % by default, subtle; 0 % is none).
  A new planet crossfades in about a second and a half. Both are automatable and are
  part of a Location. Projects saved before Milestone 10 open with Influence at 0 %, so
  they sound as they did.
- **Play Mode.** Transport (the default) plays only while the host plays, from silence
  and locked to the song position, so exports and bounces always contain it. Free
  plays all the time.
- **Key and tempo.** Everything tonal follows Root and Mood, and all Places share one
  slow chord progression. Rhythmic parts follow the host tempo and song position.
- **The environment**, retuned toward music. Air whistles on scale notes. Water drips
  ring as tuned drops, and rain carries pitched plucks and an in-key shimmer. Fire and
  ice sing in the key. Thunder lands as a tuned impact with a swell. The Tonal Bed
  holds it together.
- **World Controls** shape everything. Population sets how busy the Places are,
  Machinery makes them straighter and more metallic, Life more human, and Danger
  darker.
- the shared World Space reverb; Power, Master Level, Width, Seed and Quality;
- **The interface**: a textured, lit planet that turns under its clouds, with an
  atmosphere, rings on the giants and city lights on its night side (from the populated
  Places); raised, bevelled buttons, toggles, tabs and cards that glow on hover and
  light up when on, with one set of vector icons; a Place selector of 26 illustrated cards (all, or by category:
  Fantasy, Modern, Sci-Fi, Nature) that come alive on hover; Place strips; a detail
  panel with header art and an icon per instrument; the Pattern Editor with a tab per
  instrument; landmarks on the globe that you can drag to move a Place. The colours,
  light and particles of the interface follow the active Places, and the globe shows
  them (fields, orbits, city lights, oceans, dunes...). Finished artwork can be
  dropped in per Place (see [assets/places/README.md](assets/places/README.md)).

Now: Milestone 10, the visual revamp and planet types. Next (Direction Update 5): pro
features, presets and release (11). See [docs/DEVLOG.md](docs/DEVLOG.md) for progress and known limits. The
plan is in [docs/PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md) and
[docs/DIRECTION_UPDATE.md](docs/DIRECTION_UPDATE.md) to
[docs/DIRECTION_UPDATE_5.md](docs/DIRECTION_UPDATE_5.md); later files override earlier
ones. How the instruments are modelled, and the acoustics data behind them:
[docs/INSTRUMENT_NOTES.md](docs/INSTRUMENT_NOTES.md).

## Locations and Journeys

A **Location** is a complete world: the Places in their four slots with their
Characters, settings, patterns and instrument effects, the World Controls, Score
Intensity, the environment levels, Root and Mood. Power, Master Level, Quality,
Play Mode and the World Seed are not part of a Location.

- **Save:** click an empty chip in the LOCATIONS row (or right-click a chip, "Save the
  world here"). Double-click a saved chip to rename it ("Frozen Gate", "Ember
  Market"); right-click for its colour, to save over it or to clear it.
- **Travel:** click a saved chip, or its marker on the globe's orbit. The world travels
  there over the **Travel Time** (0.5-60 s, or 1-16 bars of the host tempo with Travel
  Sync), eased in and out. On the way the World Controls and every Place's settings
  glide; a Place that stays morphs; a Place that leaves recedes into the distance
  (quieter, darker, more distant in the reverb) and a new one approaches from there;
  Root, Mood, the Places' seeds and Characters change at the far point, the middle of the way. The
  globe turns faster and a comet runs along the orbit; the destination chip fills.
  Choosing another Location half way turns back smoothly. On arrival the knobs show
  the Location, and you can carry on shaping it.
- **Automate it:** **Destination** (a host parameter, - or 1-8) travels when it
  changes, so draw steps in the automation lane to move between Locations at scene
  changes (Travel Time is automatable too). In Transport mode, starting playback
  anywhere puts the world straight at the Location the Destination names there, so
  playback and bounces always match.
- **Journey Mode:** switch on JOURNEY, choose **From** and **To**, and the **Journey**
  slider (a host parameter, 0-100 %) morphs between them. Draw it as automation to walk
  the listener from one place to the other in sync with picture. Switching Journey
  Mode off keeps the world where the Journey stands.
- **Places are matched by slot.** A slot with the same Place at both ends morphs it;
  a slot whose Place changes sends one away and brings the other in. Build Locations
  from each other (save, change, save the next) so the Places that should stay keep
  their slots.
- **.world files:** SAVE .world writes the whole world and its eight Locations to a
  file (in Documents/WORLDBUILDER/Worlds by default); LOAD .world brings one back.
  Locations are also saved with the project.

## Getting WORLDBUILDER into your song

WORLDBUILDER writes its own notes (the Pattern Editor), so there is no MIDI part to
record. Three ways to get it onto the timeline:

1. **Play it in Transport mode (the default) and let the host bounce it.** In
   Transport mode it plays only while the host plays, starts from silence and is
   locked to the song position: the same position always sounds the same, so playback,
   exports and bounces match. This is the way to keep **automation** (see below).
2. **RENDER** (footer, CAPTURE): choose the length (4-64 bars) and **Mix** or
   **Stems**, then click RENDER. It renders offline, faster than real time, while the
   plugin keeps playing; click again to cancel. The result is exactly what the host
   plays from bar 1 in Transport mode, at the host's tempo and sample rate (24-bit
   WAV, exactly the bars' length, so it loops cleanly; the reverb tail is not added).
   Then drag **WAV** onto a track at bar 1, or click it to save the file (a folder for
   stems). Stems are one file per Place plus one for the environment layers; each is
   rendered alone through the master section, so together they come to about, not
   exactly, the mix. A render uses the current settings: automation is not included.
3. **MIDI** (footer, CAPTURE): the patterns of every active instrument as one MIDI
   file, one track per instrument named "Place - Instrument", from bar 1 at the host's
   tempo for the chosen length. Drag it onto the tracks area, or click to save it.
   The notes are the patterns as written (randomness, Evolve and swing are the
   plugin's playing and are not written). Use it to double a part with another
   instrument, or edit it and drop it back onto a Pattern Editor tab. The single
   pattern DRAG MIDI in the Pattern Editor still works too.

**GarageBand** has no bounce-in-place, and in Play Mode Free it exports a song only up
to its last region. So: keep Play Mode on Transport and Share → Export Song, or RENDER
and drag the WAV (or save it and drag it in from Finder) onto an audio track at bar 1.
For MIDI, save the file and drag it onto the tracks area (GarageBand makes a track per
MIDI track). Whether GarageBand accepts files dragged straight out of a plugin window
is not yet tested; saving always works.

**Logic Pro:** Transport mode, then **Bounce in Place** (Track → Bounce and Replace, or
Ctrl-B) or **File → Bounce → Project or Section**; or RENDER and drag the WAV or stems
into the arrange area; or drag the MIDI onto the tracks area. Separate outputs per
Place (multi-output) arrive in a later milestone.

**Automation replaces MIDI control.** Every control that shapes a cue is a host
parameter and responds smoothly when automated: Score Intensity, each Place's level,
mute and solo, its Character, Activity, Tension, Tone, distance and pan, each
instrument's level, switch and effects, the World Controls, Root and Mood, Master
Level and Power, and the Locations (Destination, Travel Time, Journey Mode, From, To
and Journey), the Planet and Planet Influence. In GarageBand,
show automation (A) and choose WORLDBUILDER's parameters on the track; in Logic, the
same in the track's automation lane. Draw Score Intensity rising into a climax, bring a
Place in by its level, or switch Characters at a scene change.

## Install (macOS)

1. Download `WORLDBUILDER-macOS-Universal` from the latest green
   [CI run](../../actions) and unzip it.
2. In Terminal: `bash install-mac.sh` from the unzipped folder. It removes the
   download quarantine, re-signs the plug-in for your Mac, refreshes the Audio Unit
   list and runs Apple's validator. See `INSTALL.txt` inside the package for why
   this is needed.
3. GarageBand: on a Software Instrument track, open Smart Controls (B) → Plug-ins →
   Instrument slot → AU Instruments → Veil Audio → WORLDBUILDER. Press play: in the
   default Transport mode it plays while the song plays; no MIDI is needed. (Choose
   Play Mode Free in the footer to hear it with the song stopped.)

Uninstall with `bash uninstall-mac.sh`.

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release      # fetches JUCE 8.0.16
cmake --build build --parallel
ctest --test-dir build --output-on-failure          # offline processor tests
```

On Linux this builds the VST3, the Standalone app and the tests. You need the JUCE
Linux dependencies (ALSA, FreeType, fontconfig, X11 headers); see
`.github/workflows/build.yml`. Pass `-DJUCE_PATH=/path/to/JUCE` to use an existing
JUCE checkout. On macOS it also builds the AU as a universal binary.

`WorldbuilderTests --snapshot editor.png` writes images of the interface (and of a
Place's detail panel and the Pattern Editor); `WorldbuilderTests --probe` prints per-layer measurements for
each World Control; `WorldbuilderTests --probe-places` prints each Place's level per
character and element (`--place forest,cavern` narrows the Place tests and probes to
those Places);
`WorldbuilderTests --render storm.wav 30 weather=1 wetness=0.8` renders any world
(parameter IDs from `Source/Parameters.h`, e.g. `p1_type=2 p1_character=2` for a
Village festival in slot 1) to a WAV file.

CI (`.github/workflows/build.yml`) runs on every push:
- universal macOS build and the tests on both architectures;
- bundle and Info.plist checks, then signing and packaging;
- install of a quarantined copy with `install-mac.sh`;
- `auval` and the AU host harness (`tools/au-harness`) on arm64 and x86_64;
- the Linux build and tests.

The installable zip is uploaded only when every check passes.
