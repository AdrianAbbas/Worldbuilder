# WORLDBUILDER

A film-scoring soundscape instrument for Logic Pro and GarageBand on macOS (Apple
Silicon and Intel, macOS 10.15+). It scores a place rather than recording it:
musical, tonal and rhythmic sound in your song's key and tempo that evokes a castle,
a forge, a city at night, a forest or a cavern, over a living environment of wind,
water, fire, ice and thunder. Everything is synthesised in real time. It uses no samples and no
recordings.

**Status: Milestone 12, feature-complete for release (26 Places of six instruments,
each with its own instrument effects; Score Intensity; render to WAV and scene MIDI
export; Locations and Journeys; seven planet types; the Arrangement with transitions,
templates, Auto-arrange and game-audio export; and now separate outputs per Place, 24
factory scenes and INSPIRE), now with the 8-bit interface of Milestone 13:** pixel-art
controls, icons and text in moods that follow the world (whimsical fantasy, neon sci-fi,
the "magitech" blend, neutral modern), a pixel-art tiny planet for the Living Globe, and
a UI size of 100, 150 or 200 %.
These work now:
- **Scenes and INSPIRE** (the header): 24 factory scenes, each a ready-made film or game
  moment with its Places, Planet, key, effects, Locations and an arrangement; INSPIRE
  builds a new, musical world from the next World Seed. See
  [Scenes and INSPIRE](#scenes-and-inspire).
- **Separate outputs**: each Place slot also plays on its own stereo output in Logic.
  See [Separate outputs in Logic Pro](#separate-outputs-in-logic-pro).
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
    Full, Subtle or **Off (the default)**. The same setting runs the interface's small
    delights: sparkles, little stars, scanline flicker and HUD brackets on hovered
    controls, and the Place icons' idle animations (Full: always; Subtle: still, the
    hovered card moves; Off: none).
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
  Desert World, Lava World or Forest World. Each one changes the tiny planet's horizon
  and sky (craters; waves and islands; snow and ice spikes; swirling bands with rings
  arcing across the sky; dunes; glowing cracks and a volcano; trees) and gives the
  whole world a sound colour:
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
- **The interface** (Milestone 13): 8-bit pixel art throughout, drawn on whole pixels
  at every UI size. Its **mood follows the active Places**: fantasy and nature Places
  make it whimsical and storybook (berry and gold, mint curls, sparkles and little
  stars on an enchanted meadow), sci-fi Places futuristic (dark metal, neon LEDs,
  scanlines, HUD brackets), both together "magitech" (wooden frames with neon inlays,
  circuit runes), and modern Places pull it toward a clean, neutral graphite; it
  changes smoothly as Places come and go. Pixel knobs with gem, rune or LED rings, the
  large Score Intensity dial, sprite sliders (horizontal and vertical), bevelled
  buttons and toggles, tabs, menus, LED meters, one pixel icon set for every tool and
  instrument, and 27 animated pixel Place icons (a waving flag, a swinging tavern sign,
  a flickering thruster...). Two original pixel fonts (no third-party font); sentences
  such as descriptions and tooltips stay in a clean font. **The Living Globe is a tiny
  planet**: its curved horizon across the panel, a pixel sky with sun, clouds, rain,
  snow, lightning and an aurora with Magic, day turning to night as it spins; the Places
  stand on the horizon (ships, stations and the asteroid colony in orbit, the Nebula out
  in space), turn with it and go over the horizon; the Locations are stars and moons in
  the sky. **UI SIZE** (footer): 100, 150 or 200 %. Hand-drawn pixel art can replace
  any icon (see [assets/pixel/README.md](assets/pixel/README.md)).

See [docs/DEVLOG.md](docs/DEVLOG.md) for progress and known limits. The plan is in
[docs/PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md) and
[docs/DIRECTION_UPDATE.md](docs/DIRECTION_UPDATE.md) to
[docs/DIRECTION_UPDATE_7.md](docs/DIRECTION_UPDATE_7.md); later files override earlier
ones. How the instruments are modelled, and the acoustics data behind them:
[docs/INSTRUMENT_NOTES.md](docs/INSTRUMENT_NOTES.md). The [control guide](#control-guide),
[keyboard and mouse map](#keyboard-and-mouse) and [troubleshooting](#troubleshooting)
are below.

## Scenes and INSPIRE

**Factory scenes** (the header, right of the title): **<** and **>** step through them,
and the name in between opens a menu of all of them by world (Medieval & Fantasy,
Sci-Fi, Modern, Nature). A scene is a complete film or game moment: two to four Places
with their Characters, levels, distances and pans, the Planet, Root and Mood, the World
Controls, every instrument's effects, two to four **Locations** to travel between, and an
**arrangement** (a template, auto-arranged for its Places) whose calm sections sit at
the first Location and whose climax travels to the last.

| World | Scenes |
|---|---|
| Medieval & fantasy | Ambush at the Tavern, Siege of the Palace, Market at Dawn, Frozen Citadel, Dragon's Caldera, Drowned Temple, Haunted Moor, Royal Coronation |
| Sci-fi | Docking at Orbital Station, Neon Rain Chase, First Contact, Into the Nebula, Abandoned Reactor, Alien Jungle, Asteroid Shift Change, Battle Stations, Void Between Stars |
| Modern | Clockwork Metropolis, Storm at Sea, Dead Mall Nights |
| Nature | Abandoned Mine, Desert Caravan, Elven Canopy, Summer by the Sea |

- Loading a scene switches Play Mode to **Arrangement** (unless it is Free), so pressing
  play in the host plays its sections and travels between its Locations. In Transport
  mode you hear its first Location as it stands.
- It keeps your session settings: Power, Master Level, Quality and Place Outputs.
- **UNDO** (beside INSPIRE) brings back the world from before the last scene or
  INSPIRE, including your arrangement, Locations and Play Mode.
- A scene is a starting point: change anything, save it as a `.world` file or with the
  project. The scene's name stays in the browser until you load another.

**INSPIRE** moves the World Seed on and builds a random but musical world from it: two to
four Places that belong together (one world, fantasy, sci-fi or modern, plus nature;
now and then one from another world for a blend), their Characters, a Mood that suits
them (often the first Place's own), a root, a Planet that fits (an ocean world for a
beach or a ship, a desert world for the desert), World Controls near their defaults with
the right Era, small changes to every instrument's reverb, delay and character amount,
two or three Locations that rise in danger, and a template auto-arranged for them. The
same World Seed always inspires the same world, so a seed you liked can be typed back
in (INSPIRE uses the seed after the one shown). UNDO takes it back.

## Separate outputs in Logic Pro

WORLDBUILDER has a main stereo output and one stereo output per Place slot (Place 1-4),
so each Place can have its own channel strip, effects and bounce.

1. Insert WORLDBUILDER on a software instrument track as **Multi Output (5xStereo)**
   (in the instrument slot: AU Instruments → Veil Audio → WORLDBUILDER → Multi Output).
2. In the mixer, click the **+** under the instrument's channel strip to add aux
   channels for outputs 3-4, 5-6, 7-8 and 9-10: those are Place 1, 2, 3 and 4.
3. Choose **PLACE OUTPUTS** in the footer:
   - **Copy** (the default): the main output keeps the full mix and each Place output
     carries a copy of its Place. Use the aux strips for extra processing or to bounce
     stems; mute the auxes when you want the main mix alone.
   - **Split**: a Place that has its own output leaves the main mix. The main then
     carries the environment layers, the Places without an output, and **all reverb and
     delay returns** (the World Space, the per-instrument reverbs and delays and the
     transitions are shared and stay on the main, like an aux return).
4. A Place output carries the Place's dry sound: its instruments with their own
   character effects, its level, distance, pan and arrangement, after Master Level and
   Power, through its own safety limiter. The master's Planet colour, width and glue are
   on the main only.

**GarageBand** has only the main output, so the main always contains every Place, in
Copy and in Split.

## Locations and Journeys

A **Location** is a complete world: the Places in their four slots with their
Characters, settings, patterns and instrument effects, the World Controls, Score
Intensity, Root and Mood. Power, Master Level, Quality, Play Mode, the World Seed and
the environment mix (the layers' levels, mutes and solos and the Environment Level) are
not part of a Location: they stay as you set them in every journey, arrangement, scene,
Inspire result and render.

- **Save:** click an empty chip in the LOCATIONS row (or right-click a chip, "Save the
  world here"). Double-click a saved chip to rename it ("Frozen Gate", "Ember
  Market"); right-click for its colour, to save over it or to clear it.
- **Travel:** click a saved chip, or its star or moon in the planet's sky. The world travels
  there over the **Travel Time** (0.5-60 s, or 1-16 bars of the host tempo with Travel
  Sync), eased in and out. On the way the World Controls and every Place's settings
  glide; a Place that stays morphs; a Place that leaves recedes into the distance
  (quieter, darker, more distant in the reverb) and a new one approaches from there;
  Root, Mood, the Places' seeds and Characters change at the far point, the middle of the way. The
  planet turns faster and a comet crosses the sky to the destination; its chip fills.
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

## Arrangement: a cue in sections

WORLDBUILDER can play a track that evolves through sections you design, like a
composed game or film cue: for example 8 bars of Intro, 16 of Build, 16 of Climax, 16
of Wind-down and 8 of Outro, in any order, lengths and number.

- **ARRANGE** (top of the window) opens the timeline. **+ SECTION** adds Intro, Build,
  Climax, Wind-down, Outro, Loop, Bridge and Break in turn; rename one by typing in its
  name box. Each section has a colour, a length (1-64 bars; or drag its right edge),
  how many times it plays (or **Loop (endless)** for a game-style loop), and **Score
  Intensity** at its start and end with a curve (linear, ease in, ease out, swell).
  Drag a section to reorder it; right-click it for more. Every edit undoes (Cmd-Z).
- **The instrument grid** under the timeline has a row for every instrument of every
  active Place and a cell under each section. Click a cell to switch the instrument on
  or off there; Shift-click to step its pattern (A, B, C); right-click for **enter
  halfway**, **fade in**, **fade out** and **Fill** (the instrument plays its Fill
  pattern in the last bar before the section changes).
- **Pattern variants:** in the Pattern Editor every instrument has tabs **A, B, C** and
  **FILL**. Until you edit them, B is a busier take on A, C a sparser one and the Fill
  the busiest, building over its bar.
- **Play Mode Arrangement** (footer): like Transport, the plugin plays while the host
  plays and follows its song position, and plays the section the arrangement has
  there. Starting playback at bar 37 plays exactly what bar 37 holds, so playback and
  bounces match. After the last section it stops (the world fades out over two bars) or
  loops back to a section you choose (the End menu).
- **PREVIEW** plays the arrangement from the selected section while the host is
  stopped; click another section to jump there.
- **Transitions** (INTO NEXT, for the selected section): how it hands over to the next
  one. **Cut**; **Crossfade** over 1-4 bars (instruments leaving fade out, those
  entering fade in, and Score Intensity glides across); **Riser** (noise and the chord
  rising into the downbeat); **Drum fill** (the Places' rhythm instruments play their
  Fill patterns); **Drop** (1-4 beats of silence, then a sub hit on the downbeat);
  **Reverse swell** (the chord swelling up into the downbeat); **Filter sweep** (the
  whole mix closes down, then opens again over the first beat). The sounds are tuned
  to Root and Mood and go through the world's reverb. The timeline marks each one.
- **LOCATION:** a section can name one of the eight Locations. The world travels there
  over the Travel Time (at most the section before it) and arrives on the section's
  first downbeat; later sections stay there until another names a Location. While the
  arrangement drives the Locations, the Destination and Journey controls wait, and
  nothing is written into your knobs: leaving Arrangement mode returns to them.
- **PLANET** and **WORLD** (overrides): a section can have its own Planet and its own
  values for any World Control (e.g. Danger 100 % in the Combat), only while it plays.
- **TEMPLATE** starts from a ready-made cue and arranges it for your Places: Classic Arc
  (8 Intro / 16 Build / 16 Climax / 16 Wind-down / 8 Outro), Exploration Loop (8 / 32
  Loop, played twice / 8; set the Loop to Loop (endless) for a game), Battle, Boss Fight,
  Town Theme (8 A / 8 B / 8 A / 8 C, looping) and Cinematic Reveal.
- **AUTO-ARRANGE** fills the grid, pattern variants, intensity ramps and transitions for
  the sections you have and the active Places: sparse beds in an intro, instruments
  entering by role in a build-up (drones and pads, then bass, rhythm, melody, accents),
  everything at a climax, thinning out in a wind-down. It reads what each section is
  from its name (Intro, Build, Climax, Break, Outro, Loop, Bridge, and words like
  Tension, Combat, Phase, Reveal, Victory). It uses the World Seed and undoes in one step.
- Automating Score Intensity still works: it adds on top of the sections' ramps (50 %
  adds nothing).
- Bars are 4/4. The arrangement is saved with the project and in `.world` files.

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

**From the arrangement** (CAPTURE, BARS: **Arrangement**):
- **Mix** or **Stems** renders the whole arrangement exactly as the host plays it in Play
  Mode Arrangement from bar 1 (sections, transitions, Locations and overrides), every
  section once through, with End: stop's fade-out.
- **Sections** renders each section as its own file for a game engine, named after the
  first Place: `Tavern_Intro.wav`, `Tavern_Loop.wav`, `Tavern_Outro.wav`. Each file is
  exactly its section's length, so they chain back to back. A section that plays more
  than once, or loops endlessly, becomes a **seamless loop**: one pass whose start
  already carries the reverb and echoes from its end, so it loops without a gap or a
  click.
- **Stingers** renders each section's ending (its last bar or its transition, up to two
  bars) ringing out to silence, as one-shot cues for game events
  (`Tavern_Victory_Stinger.wav`).
- **MIDI** then follows the arrangement: each instrument plays where its sections have it
  on, with their variants and Fills, fades as velocity.

**GarageBand** has no bounce-in-place, and in Play Mode Free it exports a song only up
to its last region. So: keep Play Mode on Transport and Share → Export Song, or RENDER
and drag the WAV (or save it and drag it in from Finder) onto an audio track at bar 1.
For MIDI, save the file and drag it onto the tracks area (GarageBand makes a track per
MIDI track). Whether GarageBand accepts files dragged straight out of a plugin window
is not yet tested; saving always works.

**Logic Pro:** Transport mode, then **Bounce in Place** (Track → Bounce and Replace, or
Ctrl-B) or **File → Bounce → Project or Section**; or RENDER and drag the WAV or stems
into the arrange area; or drag the MIDI onto the tracks area. For each Place on its own
channel strip, see [Separate outputs in Logic Pro](#separate-outputs-in-logic-pro).

**Automation replaces MIDI control.** Every control that shapes a cue is a host
parameter and responds smoothly when automated: Score Intensity, each Place's level,
mute and solo, its Character, Activity, Tension, Tone, distance and pan, each
instrument's level, switch and effects, the World Controls, Root and Mood, Master
Level and Power, and the Locations (Destination, Travel Time, Journey Mode, From, To
and Journey), the Planet and Planet Influence. In GarageBand,
show automation (A) and choose WORLDBUILDER's parameters on the track; in Logic, the
same in the track's automation lane. Draw Score Intensity rising into a climax, bring a
Place in by its level, or switch Characters at a scene change.

## Control guide

**Header.** WORLDBUILDER; the scene browser (**<**, the scene menu, **>**), **INSPIRE**
and **UNDO**; **ARRANGE** opens the arrangement timeline; **SCORE INTENSITY** (0 %
calm bed, 50 % the Places as set, 100 % climax); **POWER** fades the world in or out
over two seconds.

**Left: World Controls**, shaping every layer and Place together.
- Climate: **Temperature** (frozen to scorching), **Weather** (still to storm),
  **Wetness** (arid to drenched).
- Civilisation: **Population** (empty to crowded: how busy the Places are),
  **Machinery** (none to industrial: straighter, more metallic), **Era** (ancient to
  futuristic).
- Mystery: **Magic** (mundane to arcane), **Life** (barren to teeming), **Danger**
  (peaceful to hostile: darker harmony, tighter rhythm, more pressure).
- Size: **Scale** (intimate to colossal: distance, reverb size, low weight),
  **Activity** (calm to chaotic: event density and how fast the world evolves).

**Centre.** The **Living Globe** is a tiny planet showing the world: its Places stand
on the horizon (or orbit it) and turn with it; drag a Place along the horizon for its
pan and up or down for its distance, double-click it for its detail panel; click a
Location's star or moon in the sky to travel there. **PLANET** and **INFLUENCE** in its corner. Below it,
**ADD A PLACE**: the 26 Place cards (ALL, FANTASY, MODERN, SCI-FI, NATURE); click a card
to add it to the first empty slot.

**A Place's detail panel** (EDIT on its strip): **Character** (three per Place),
**Activity**, **Tension**, **Tone**, **Size**, **Variation**, **Density**, **Swing**,
**SYNC TO HOST** (or its own free tempo), **Seed** and New seed, its six instruments'
levels and switches, the suggested Mood, and **PATTERNS** for the Pattern Editor.

**Pattern Editor.** A tab per instrument, variants A, B, C and FILL, the piano roll with
velocity and chance lanes, DRAW and SELECT, undo and redo, copy and paste, transpose,
double and halve, lock, Generate, randomness (amount, what varies, Evolve), the
instrument's level and effects (reverb and type, delay and type, character and amount,
tone, width), MIDI import (drop a file) and DRAG MIDI / save.

**Right.** **ENVIRONMENT**: Air, Water, Fire & Heat, Ice & Earth, Thunder and Tonal Bed,
each with level, M (mute), S (solo) and a meter. At 0 dB each layer sits about 12 dB under
a typical Place, as a background. The slider beside the ENVIRONMENT title is the
**Environment Level**: one automatable level over all six layers. **PLACES**: the four slots, each with
level, distance, pan, M, S, EDIT and X (remove).

**LOCATIONS row.** Eight Location chips (click an empty one to save, a saved one to
travel), Travel Time and its sync, **JOURNEY** with From, To and the Journey slider,
**PATH** with its leg length and end, **SAVE .world** and **LOAD .world**. See
[Locations and Journeys](#locations-and-journeys).

**Footer.** **ROOT** and **MOOD** (the key everything tonal follows), **QUALITY** (Eco,
Standard, High), **PLAY MODE** (Free, Transport, Arrangement), **WORLD SEED** and **New
world** (a random seed), **UI SIZE** (100, 150, 200 %), the host tempo, **OVERLAYS**
(Full, Subtle, Off: the Place overlays and the interface's small delights), **PLACE
OUTPUTS** (Copy, Split), **CAPTURE** (bars or Arrangement; Mix, Stems, Sections or
Stingers; MIDI, RENDER and the WAV to drag or save), **Space Mix**, **Width**, **Master
Level** and the master meter.

Every knob, slider, switch and menu that shapes the sound is a host parameter: automate
it in GarageBand (A on the track) or Logic.

## Keyboard and mouse

| Where | Action | Does |
|---|---|---|
| Any knob or slider | drag (up/right) | change the value |
| | **Shift**-drag | fine control |
| | double-click | back to the default |
| | hover | tooltip with what it does |
| Place card | click | add the Place to the first empty slot |
| Globe (tiny planet) | drag a Place | sideways: pan; up and down: distance |
| | double-click a Place | open its detail panel |
| | click a Location's star or moon | travel there |
| Location chip | click (empty) | save the world there |
| | click (saved) | travel there |
| | double-click | rename |
| | right-click or **Alt**-click | menu: save over, colour, clear |
| Pattern Editor grid | click (DRAW) | add a note |
| | drag a note | move it; drag its end to lengthen |
| | **Shift**-click | add to or remove from the selection |
| | double-click a note | lock or unlock it |
| | right-click or **Alt**-click a note | delete it |
| | wheel / **Shift**-wheel / **Cmd**-wheel | scroll up and down / sideways / zoom |
| Pattern Editor keys | **Delete**, **Backspace** | delete the selection |
| | **Cmd-Z**, **Cmd-Shift-Z** or **Cmd-Y** | undo, redo |
| | **Cmd-C**, **Cmd-V**, **Cmd-A** | copy, paste, select all |
| | **Up**, **Down** | transpose the selection (or the whole pattern) one row |
| | **L** | lock or unlock the selection |
| ARRANGE timeline | drag a section | reorder it; drag its right edge to resize |
| | right-click or **Alt**-click a section | menu: preview from here, duplicate, delete, plays, colour, every instrument on or off |
| Instrument grid | click a cell | instrument on or off in that section |
| | **Shift**-click a cell | step its pattern variant (A, B, C) |
| | right-click or **Alt**-click a cell | its entry (on, off, enter halfway, fade in, fade out), pattern A/B/C, Fill |
| ARRANGE keys | **Left**, **Right** | select the previous or next section |
| | **Space** | PREVIEW from the selected section (host stopped) |
| | **Delete**, **Backspace** | delete the section |
| | **Cmd-D** | duplicate it |
| | **Cmd-Z**, **Cmd-Shift-Z** or **Cmd-Y** | undo, redo |

On Windows and Linux (the standalone app), Cmd is Ctrl.

## Troubleshooting

- **No sound in GarageBand, a grey slot or generic sliders.** macOS rejected the
  download. Run `bash install-mac.sh` again from the unzipped folder (it removes the
  quarantine flag, re-signs, refreshes the Audio Unit list and runs `auval`), then quit
  and reopen GarageBand. `bash scripts/diagnose-mac.sh` (in this repository) prints
  what macOS reports about the installed plug-in.
- **Silent until I press play.** Play Mode Transport (the default) and Arrangement play
  only while the host plays. Choose **Free** in the footer to hear it with the song
  stopped, or use **PREVIEW** in ARRANGE.
- **A loaded scene sounds the same all the way through.** Its arrangement plays in Play
  Mode **Arrangement**; in Transport the world stays at the first Location.
- **The export ends early in GarageBand.** In Play Mode Free, GarageBand exports only up
  to the last region. Use Transport (or Arrangement), or RENDER and drag the WAV in.
- **Too quiet or too loud.** Each Place's level and distance, the environment levels and
  Master Level set the balance; a gentle glue compressor and a safety limiter (-1
  dBFS) keep the output in range. Score Intensity also raises the level towards a
  climax.
- **CPU too high on a laptop.** Choose Quality **Eco**, use fewer Places (each plays six
  instruments), or switch OVERLAYS **Off** (the default). Four busy Places at Standard
  quality use about 20 % of one core in the Linux tests.
- **Place outputs are silent in Logic.** Insert the Multi Output version and add the aux
  channels (see [Separate outputs](#separate-outputs-in-logic-pro)); a slot with no
  Place, or a muted one, is silent on its output.
- **The Place outputs double the mix.** PLACE OUTPUTS is on Copy: the main keeps every
  Place. Choose Split.
- **A Location or scene does not travel.** Travelling needs a saved Location; while the
  arrangement drives Locations (Play Mode Arrangement), the Destination and Journey
  controls wait.
- **Dragging a WAV or MIDI file out of the plugin does nothing.** Some hosts do not
  accept files dragged from a plugin window. Click WAV or MIDI to save the file, then
  drag it in from Finder.

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
