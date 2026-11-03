# WORLDBUILDER

A film-scoring soundscape instrument for Logic Pro and GarageBand on macOS (Apple
Silicon and Intel, macOS 10.15+). It scores a place rather than recording it:
musical, tonal and rhythmic sound in your song's key and tempo that evokes a castle,
a forge, a city at night, a forest or a cavern, over a living environment of wind,
water, fire, ice and thunder. Almost everything is synthesised in real time; since
Milestone 15 a few instruments play the developer's own recordings (see
[Recorded instruments](#recorded-instruments)). Nothing comes from sample libraries or
other people's recordings.

## Two ways to use WORLDBUILDER

**1. Use WORLDBUILDER as a world** (the top bar's **WORLD**, the default). One
WORLDBUILDER on one software-instrument track plays a whole place: up to four Places of
six instruments each, their patterns, the environment, Locations and the Arrangement,
following the host's tempo and transport. You shape it with the World Controls, Score
Intensity, the Pattern Editor and ARRANGE, and get it into the song by playing it,
rendering it or exporting its MIDI. Everything below the next heading describes this
workflow.

**2. Use one instrument per track** (the top bar's **INSTRUMENT**). Put WORLDBUILDER on
several software-instrument tracks, switch each to **Instrument mode**, choose an
instrument on each (e.g. "Tavern > Lute Strum" on one, "Spaceport > Pads" on another,
"Castle > Timpani" on a third) and play each from Logic's or GarageBand's piano roll or
your MIDI keyboard, like any software instrument: polyphonic, with velocity, note
length, the sustain pedal, pitch bend and the mod wheel. No world, no patterns, no
arrangement; the transport doesn't matter. See [Instrument mode](#instrument-mode).

You can mix both: one WORLDBUILDER as the world under the cue, and a few more in
Instrument mode playing your own parts on top, each on its own track.

**Status: Milestone 12, feature-complete for release (26 Places of six instruments,
each with its own instrument effects; Score Intensity; render to WAV and scene MIDI
export; Locations and Journeys; seven planet types; the Arrangement with transitions,
templates, Auto-arrange and game-audio export; and now separate outputs per Place, 24
factory scenes and INSPIRE), now with the 16-bit pixel interface:** pixel-art
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
  jumps; automate it to shape a cue over time. With **Drama** (the small knob beside it)
  the top half changes gear like game music when a fight starts (see *Drama: Calm,
  Tension, Action, Climax* below); the label shows the tier the world is in.
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
- **The interface** (Milestone 13, made 16-bit after the GarageBand test): pixel art
  throughout, drawn on whole pixels at every UI size, with fine 16-bit pixel fonts
  (one-pixel strokes) and dithered shading. Its **mood follows the active Places**: fantasy and nature Places
  make it whimsical and storybook (berry and gold, mint curls, sparkles and little
  stars on an enchanted meadow), sci-fi Places futuristic (dark metal, neon LEDs,
  a dot grid, HUD brackets), both together "magitech" (wooden frames with neon inlays,
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
| Dramatic (the Drama tier, arranged in arcs) | Battle at the Castle Gate, Chase Through the Neon City, The Dragon in the Deep, Siege of the Desert Fortress, Space Battle over the Station, Escape from the Hive, The Final Stand, Tragic Loss, Triumphant Return, Pirate Battle in the Storm, Mining Colony Uprising, Ambush in the Dark Woods, Awakening of the Guardian, Factory Breakdown, Last Light of the Temple, Victory Parade, Warp Jump Pursuit |

- Loading a scene switches Play Mode to **Arrangement** (unless it is Free), so pressing
  play in the host plays its sections and travels between its Locations. In Transport
  mode you hear its first Location as it stands.
- It keeps your session settings: Power, Master Level, Quality and Place Outputs.
- **INSPIRE** keeps an arrangement you have built (its sections then play the new
  Places); with none, it brings its own. Loading a factory scene replaces it with the
  scene's.
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

**INSPIRE's mood** (the menu beside it): **Any**, **Calm**, **Adventure**, **Mystery**,
**Tension**, **Battle**, **Epic**, **Tragic**, **Triumphant**. The world it builds is made
to sound like it: Battle chooses fighting Places in their fiercest Characters, a minor or
Phrygian key, high Danger and Activity, Score Intensity at Action or Climax with driving
drums and ostinatos, and a Battle, Chase, Final Stand or Siege arrangement in arcs; Epic
is big and slow (Scale high, choir and brass, half time); Tragic slow and minor (strings,
piano, a Lament); Triumphant major with a march and fanfares; Tension a pulse and a
heartbeat at the Tension tier; Calm, Mystery and Adventure stay at or near the calm
tiers. **Any** picks the mood from the seed: a dramatic one (Tension, Battle, Epic,
Tragic or Triumphant) about three times in five.

## Drama: Calm, Tension, Action, Climax

A game score layers up as the action does. With **Drama** above 0 % (100 % for a new
instance), the top half of Score Intensity moves every Place through four tiers, and at
each tier the music changes gear on the next bar line:

| Tier | Score Intensity (Tension as set) | What joins the Place |
|---|---|---|
| **Calm** | up to 55 % | nothing: the Place exactly as before |
| **Tension** | about 55-72 % | a pedal-point pulse on the tonic, distant drums (a heartbeat), swells; the harmony holds the tonic |
| **Action** | about 72-89 % | a driving ostinato (sixteenths with 3-3-2 accents), war drums, snares and toms, stabs on the downbeats; the chords move twice as fast |
| **Climax** | from about 89 % | double-time drums, tom fills, impacts and crashes every four bars, cymbal swells into each phrase, choir and brass stabs, a brass ostinato, low piano strikes; a gear change lands on the chromatic mediant |

A Place's own **Tension** (with Danger) pushes it further once Score Intensity is above
the middle; at the very top of Tension the fifths rub a semitone and the stabs cluster.

Each Place has its own dramatic colour, from the developer's recordings: the **Castle**
war drums (the large djembe pitched down), spiccato strings, a brass ostinato and choir
stabs (the voice recordings); the **Spaceport** hybrid pulses, braams and synth stabs
(the Minilogue), hybrid percussion; the **Forest** tribal drums and driving strings; the
**Factory** metal percussion and pistons as the rhythm, a grinding bass; the **Ruins**
low piano strikes, dissonant strings and distant drums; and so on for all 26. The first
Place leads (drums, ostinato, hits), the second adds its ostinato and hits, the others
their hits only, so a world of four never stacks four drum kits.

- **Drama Style** (footer): Auto (each Place's own), Driving (battle, chase), Epic (big,
  half time, choir and brass), Tragic (slow strings, piano, almost no drums), Triumphant
  (a march, fanfare brass), Suspense (a heartbeat, ticking, pulses).
- **Drama 0 %** is the Classic score: the world exactly as before. Projects and the 24
  scenes from before the Drama tier open at 0 % and sound bit for bit as they did.
- **Arrangement**: each section has a **DRAMA** menu in ARRANGE (as its intensity says,
  or Calm, Tension, Action, Climax). Transitions get their dramatic sounds: a drum roll
  and a cymbal swell into a Riser or Drum fill, an impact, crash and stab on the
  downbeat after a Riser, Drum fill, Drop or Reverse swell. With Drama above 0, **Auto**
  builds arcs: intros and breaks Calm, build-ups at Tension, the first fight at Action and
  the later ones at Climax, a Riser into a fight, a Drop out of it, a Drum fill back in.
  New templates: **Chase**, **Siege**, **Final Stand**, **Lament**.
- The Drama layer costs no CPU while the world is Calm.

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
  active Place and a cell under each section. **Click a cell to open its clip** in the
  piano roll (below); **Cmd/Ctrl-click** switches the instrument on or off there;
  Shift-click steps its pattern (A, B, C); right-click for **enter halfway**, **fade
  in**, **fade out**, **Fill** (the instrument plays its Fill pattern in the last bar
  before the section changes) and the clip tools.
- **Section clips** (a mini DAW timeline): every section has its own notes for every
  instrument, as long as the section. A clip starts **linked** to the instrument's
  pattern variant (A, B or C): it plays that pattern repeated over the section, and
  edits to the variant show up in every section linked to it. Edit a linked clip and the
  piano roll asks once: **MAKE UNIQUE** (the edit changes this section only) or **EDIT
  VARIANT** (the variant opens, and edits there change every linked section). A unique
  clip shows its notes in miniature in its cell, with a bright corner.
- **The clip piano roll** is the Pattern Editor with the section's real bar numbers (a
  16-bar Build at bars 9-24 is numbered 9-24) and every tool: draw, select, velocity,
  chance, quantise (KEYS), Randomness and Evolve (shared with the instrument's
  patterns), MIDI import (fills the clip, keeping the section's length) and export
  (SAVE MIDI / DRAG MIDI: the clip from its first downbeat). Its PLAY, STOP, LOOP and
  playhead work inside the clip and play the arrangement there; **REC** records the
  keyboard into the clip at the playhead (a linked clip becomes unique first). The
  instrument tabs switch to the same section's other instruments. **UNIQUE** and
  **RE-LINK** in the toolbar, **BACK** returns to ARRANGE.
- **SONG** (in the clip piano roll, or right-click a cell > Song view) shows one
  instrument's clips across the whole arrangement as one piano roll: one pass of every
  section, end to end, numbered as in the song, the section boundaries marked and the
  linked sections dimmed. Draw across sections like a DAW track; an edit lands in the
  section it is drawn in (a linked one asks to become unique first). SAVE MIDI writes the
  instrument's whole part at its places in the song.
- **Clip tools:** right-click a cell for Open, Song view, Make unique, Re-link, Clear the
  clip (unique and empty: silent there), Copy and Paste the clip (into any section and
  instrument). Right-click a section for Copy its clips, Paste clips here (fitted to its
  length), Make every clip unique and Re-link every clip. **DUPLICATE** copies a
  section with its clips. Resizing a section fits its clips (longer: they repeat into
  the new bars). Every clip edit undoes.
- **Pattern variants:** in the Pattern Editor every instrument has tabs **A, B, C** and
  **FILL**. Until you edit them, B is a busier take on A, C a sparser one and the Fill
  the busiest, building over its bar.
- **Your arrangement is saved and played.** Building the first sections switches Play
  Mode to **Arrangement**, so pressing play in the host plays them. The top bar shows
  **ARRANGEMENT: ON** with the section playing (or **OFF**); click it to switch. If you
  choose Free or Transport yourself, the arrangement stays saved in the project and plays
  again when you switch back. Closing ARRANGE or the plugin window, changing Places,
  INSPIRE and saving or reopening the project keep it. PREVIEW and the host's playback
  sound exactly the same.
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
  **AUTO-ARRANGE and TEMPLATE also write section clips** from each instrument's variant:
  intros sparse and an octave lower, build-ups rising (thin at first, then more notes,
  long notes tightening into eighths and sixteenths, climbing a scale step every quarter
  of the section, and a run up into a climax in their last bar), climaxes full and louder
  (the melody doubled an octave up), wind-downs and outros thinning out and softening,
  breaks sparse. Loops and bridges stay linked. Everything stays editable.
- Automating Score Intensity still works: it adds on top of the sections' ramps (50 %
  adds nothing).
- Bars are 4/4. The arrangement, with its clips, is saved with the project and in
  `.world` files. Arrangement playback, PREVIEW, renders, game files (loops and stems)
  and the scene MIDI export all play the clips.

## Recorded instruments

Since Milestone 15 these instruments play the developer's own recordings instead of a
synthesised model (the same patterns, effects, Characters, Score Intensity, Arrangement
and KEYS as every other instrument):

| Place | Instrument | Recording | Fitted to the Place |
|---|---|---|---|
| Tavern | Lute Strum | classical guitar, fingers (3 velocity layers) | paired courses, a lute's lighter body, shorter and brighter |
| Tavern | Fiddle | violin, bowed open strings (looped while held, its own release) | vibrato, quick bowing for reels, sweeter in the Quiet Corner |
| Desert | Oud | classical guitar | paired courses, the oud's bowl body, darker (with SCOOP/GLIDE on, a fretless slide into notes) |
| Forest | Kalimba | kalimba, 17 tines (3 velocity layers) | as played; darker in the Dark Woods, short in the Morning Glade |
| Forest | Strings | violin, bowed open strings (looped while held, its own release) | a soft pad on the chords an octave above the old section, two players a note (another take, a few cents apart); darker and trembling in the Dark Woods |
| Biodome | Water Plucks | kalimba | softened and short (with SCOOP/GLIDE on, each tine slides quickly up into its note like a drop) |
| Ruins | Violin Textures (new, in place of the Ghost Pad) | violin: bowed, pizzicato, col legno | slow swells and bowed tremolo; short notes are plucked, the shortest struck with the bow's wood |
| Cyberpunk Megacity | Grit Bass | Korg Minilogue, static-y bassy synth | eighths and sixteenths as short hits, the Night Market's held roots as long hits; the Place's drive on top |
| Cyberpunk Megacity | Glitch Stabs | Minilogue, cyberpunk synth | short hits ringing out; a held stab (a beat) closes the Night Market's phrase and opens the Corporate Tower's |
| Space Station | Reactor Pulse | Minilogue, static-y bassy synth | short hits, darker and longer than in the Megacity, cut shorter in an emergency |
| Spaceport | Pulses | Minilogue, pulsating synth | short hits an octave down; the launch opens the filter through the build; a detuned second player on the dock |
| Starship Bridge | Warp Core | Minilogue, pulsating synth (held, its own pulse looping) | a slow swell, a second player 9 cents up; the warp build opens it and spins a tremolo up over the pulse |
| Biodome | Pads | Minilogue, soft pad (held) | a swell, a detuned second player (wider as the dome fails), the filter and level sagging with the power |
| Nebula | Nebula Pads | Minilogue, soft pad (held) | a slow swell, a detuned, late second player (widest in the Void), the filter drifting open and shut |
| Spaceport | Data Blips | Minilogue, harsh square (two oscillators) | three octaves down, so the high oscillator sounds the written note and the low one an octave and a fourth under it; shortened to blips |
| Asteroid Mining Colony | Drill | Minilogue, harsh square | two octaves down: the high oscillator whines an octave over the root; short bites with a fast wobble |

Since Milestone 17 the developer's piano, voice, djembes and banjo play these:

| Place | Instrument | Recording | Fitted to the Place |
|---|---|---|---|
| Suburbs | E-Piano (Evening) | acoustic piano, 88 keys (3 velocity layers, held notes) | felt-soft tone, gentle dampers through the warm tape; Sunny Afternoon and Uncanny keep the electric piano |
| Mall | E-Piano (Empty Night, Dead Mall) | acoustic piano | a lone piano in the atrium; in the dead mall slowed, detuned and wobbling; Busy Weekend keeps the electric piano |
| Ruins | Piano Shards | acoustic piano | most notes played backwards (the decay swelling into the strike), the rest held fragments the damper stops; lo-fi and reverse echoes |
| Ruins | Wind | voice: ghostly falsetto "ooh", and whispering (Haunted) or breaths and air ("shh", "fff") elsewhere (a layer under the wind) | far off and dark, closer when Haunted |
| Ruins | Low Toms | large djembe (tone and bass, pitched down) | dark and distant |
| Castle | Choir | voice: vowels, choir stacks | a full male section (six singers), dark hall tone; rougher when Besieged |
| Palace | Choir | voice | a small, bright court choir (four singers); in the Fallen Palace a ghostly, drifting "ooh" |
| Temple | Choir | voice | monks: dark, slow, humming voices among the singers |
| Temple | Chant Drone | voice: the gravelly low chant, a hummed, "ooo" or "aah" drone under it (by Character) | nearly still, two takes apart |
| Temple | Zither | banjo (3 velocity layers, round-robins on the open fifth string; palm-muted for the quickest notes) | a zither's box under it, the open strings ringing in sympathy, a second string a hair off, in the long shimmer |
| Alien Ruins | Alien Choir | voice | ring-modulated against a sub-octave, every singer drifting, slow swells (an octave down for the Guardian) |
| Alien Ruins | Whispers | voice: unintelligible whispering (normal, higher, lower) | three murmuring at once, slowed a little and drifting |
| Alien Ruins | Ritual Drums | djembes | pitched down five and seven semitones, darkened (the rattle stays synthesised) |
| Asteroid Mining Colony | Worker Hymn | voice, mostly hummed; a grungy "eeeh" in a Cave-in; gentle hummed notes on the Night Shift | a few tired singers, dark and gritty |
| Cavern | Sub Drone | the hummed low drone (a layer under the sub drone) | an octave above the sub, quiet and dark |
| Nebula | Deep Hum | the "ooo" drone | three takes detuned and spread, drifting, its top rolled off |
| Alien Hive | Formants | voice: vowels | the vowel racing between "ooh" and "ee", an octave up, wing-flutter tremolo, ring modulation, a band-pass |
| Starship Bridge | Crew Chatter | voice: unintelligible whispering | four murmurs through a comm channel (band-pass, grit, the channel cutting in and out) |
| Village | Hand Drums | djembes (3 velocity layers, 3 round-robins) | large djembe's bass (a tone now and then), small djembe's tone (a slap on some hits); the shaker stays synthesised |
| Tavern | Frame Drum, Stomps | djembes' low strokes | the frame drum the large djembe's bass and muted bass; the stomps its muted bass pitched down and dulled (the clap stays synthesised) |
| Desert | Frame Drums | djembes | the doum the large djembe's bass, the tek the small one's slap (the riq stays synthesised) |

The **piano** is played like a piano: each key its own recording (88 keys, A0 to C8, at
the piano's own stretch tuning, the whole instrument on A440), three velocity layers
crossfaded, held notes that ring with their natural decay (no loop: past what was
recorded the decay goes on as resynthesis of the note's own partials), each key's damper
when it is let go (the top octaves have none, as on a piano), and the sustain pedal
holding everything until it lifts. A note up to an eighth is a short hit, longer is a
held note; from a keyboard every note starts as the short hit and becomes the held one
if the key stays down. In Instrument mode the factory preset **Piano** is the Mall's
E-Piano on an Empty Night (in KEYS and SINGLE: the Mall's or the Suburbs' E-Piano in
those Characters).

The **voice** is a sampler of the developer's sung vowels ("ooh", "oh", "aah", "ee" and a
hummed "mm"; soft and full takes crossfaded by velocity, sustained by loops, its own
release when a note ends), morphing between vowels as a choir's vowel drifts, and an
ensemble built from his choir stacks (five or six takes of one note): from one voice to
eight, each a few cents off, a little late, elsewhere in the stereo field. Above the
highest note he sang it is shifted up a little and the synthesised choir takes over. Each
Place colours it its own way (see the table), so no two sound alike.

In the Ruins' Violin Textures a note's length in the Pattern Editor chooses how it is
played: longer than an eighth is bowed, an eighth is pizzicato, a sixteenth is col legno.

The Minilogue instruments have two articulations, also chosen by a note's length: up to
an eighth note plays the patch's **short hit** (it rings out as recorded, release and
all), longer plays its **long hit**, held for as long as the note lasts (it loops) and
let go with its recorded release. Every key was recorded, so every note plays its own
recording at its own pitch; only below the lowest and above the highest recorded key are
notes stretched (up to a fifth), and further out they fold by octaves.

The recordings are inside the plugin bundle (`Contents/Resources/Samples`, about 78 MB),
where GarageBand's sandbox lets the plugin read them, and load once in the background
(decoded on several threads) when the first WORLDBUILDER opens (about 210 MB of memory,
shared by every instance). The installer reports how many recorded notes each bundle
holds. A build without them (see [Build](#build)) plays the synthesised models these
instruments had before.

## Play and record with a MIDI keyboard (KEYS)

In GarageBand and Logic, a MIDI keyboard plays the instrument plugin on the selected
track, so WORLDBUILDER receives your keyboard. The **instrument tab you are looking at**
in the Pattern Editor is what it plays and what REC records into (the KEYS row says
"PLAYS ..."); open another tab to play another instrument. Until you open a tab, an
older project keeps the target it had (none: notes do nothing).

**Playing**
- The keyboard plays that instrument live, through its own effects: **velocity** (level
  and brightness), **note length** (released keys stop; plucked strings are damped, struck
  bells and drums ring on), the **sustain pedal** (CC64), **pitch bend** (a tone; an octave
  for sirens, risers and gliding leads; none for drums) and the **mod wheel** (vibrato, or
  brightness on instruments without vibrato). Bend and mod act on the whole instrument,
  as on a synth.
- Drum instruments: each key plays a drum of the kit (C the first, C# the second...).
- **CHROMATIC** plays exactly what you press; **SNAP TO SCALE** moves wrong notes to the
  nearest note of the Root and Mood. **OCT** shifts -2 to +2 octaves. **MONITOR** off:
  not heard (still recorded).
- The instrument's pattern keeps playing; mute it (MUTE) to play over silence. In Play
  Mode Transport with the song stopped the world is silent, and the keyboard alone sounds.
- The small keyboard on the right lights up the notes arriving (dim: held by the pedal).

**The transport: Play, Stop, Loop**
The Pattern Editor has its own transport, so you never have to start and stop the host
to record. It plays at the host's tempo while the host is stopped; when the host plays,
the editor follows the host instead.
- The **bar ruler** over the grid: click or drag its top to set the **playhead** (where
  PLAY and REC start). While it plays, dropping the playhead somewhere plays on from
  there.
- **PLAY** (the triangle) plays from the playhead, **STOP** (the square) stops and goes
  back to where Play started (stop again: to the loop's start), **LOOP** plays the
  **loop region** round and round: the brass bar in the ruler's middle lane, the whole
  pattern until you drag a new one there (double-click it: the whole pattern again).
- Scroll the grid with the mouse wheel or a two-finger swipe (sideways, or with Shift,
  across the steps); **zoom** with Cmd/Ctrl + wheel or a pinch (around the pointer), and
  the rows with Alt + wheel. While it plays, the grid scrolls to keep the playhead in view.

**Recording (REC)**
- **REC** with the host stopped starts the transport from the playhead and records. With
  **LOOP** on, **each pass is a new take**: with REPLACE the last pass you played on is
  the pattern (a pass with no notes changes nothing); with OVERDUB every pass adds to the
  last. **UNDO TAKE** goes back one pass at a time. REC again stops recording (the
  transport plays on); STOP stops both.
- **COUNT-IN**: a bar of clicks before the take, then it starts on the playhead.
- **PUNCH**: only notes inside the **punch region** are recorded (the red bar in the
  ruler's lowest lane; dragging there switches PUNCH on). REPLACE then replaces only the
  notes inside it and keeps the rest of the pattern.
- While the host plays, REC records along with it, on its bars and tempo (COUNT-IN: from
  the next bar). With the host stopped and REC armed but the transport stopped, just
  play: a **free take** starts on your first note, with a metronome.
- **REPLACE** (a new pattern, from your first note; a take with no notes changes
  nothing), **OVERDUB** (added to the notes there) or **INTO VARIANT** (a new pattern in A,
  B, C or the Fill, for the Arrangement).
- **Quantise** on input (OFF, 1/4, 1/8, 1/16, 1/32, 1/8T, 1/16T) with a **Strength** (less
  than 100 % keeps some of your feel); **QUANTISE** applies it afterwards. New patterns
  use the 1/32 grid (1/32T with a triplet quantise), the finest the editor has.
- **Length**: the pattern's own, or **AUTO** (whole bars covering the take, up to 8). A
  note keeps its place in the song: what you played at bar 9 plays at bar 9.
- While armed, that instrument's generator and randomness pause, so you hear the pattern
  exactly. Recorded notes appear in the grid at once; afterwards the pattern counts as
  edited (Generate no longer overwrites it). **UNDO TAKE** removes the whole take.

**Don't record it twice.** GarageBand and Logic also record your keyboard as a MIDI
region on the track, and when that region plays back it plays the instrument again on
top of the pattern. Either record into WORLDBUILDER's pattern (recommended: the part then
lives in the scene and the Arrangement), **or** record a normal MIDI region in the DAW that
plays the target, not both. **HOST MIDI** ("host regions play the target instrument") is on
until you record a take with the transport; then it switches off by itself (the KEYS line
says so) and notes arriving while the song plays no longer play the target, unless REC is
armed. WORLDBUILDER cannot tell a region from your hands, so this includes playing along
live while the song plays; with the song stopped the keyboard always plays. Delete the region, or switch HOST MIDI back on if you want the region instead. A
tip says this the first time you arm REC.

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
   pattern DRAG MIDI in the Pattern Editor still works too. To hear each track as its
   WORLDBUILDER instrument in GarageBand, see
   [Exporting parts to separate GarageBand tracks](#exporting-parts-to-separate-garageband-tracks).

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

## Instrument mode

Any one of WORLDBUILDER's 156 instruments, played like a normal software instrument from
the host's MIDI: Logic's or GarageBand's piano roll, a recorded region, or a MIDI keyboard.

**Switch it on:** the top bar's **INSTRUMENT** (under **WORLD**). It is the **Mode**
host parameter (World / Instrument), so it is saved with the project and a GarageBand
patch, and it can be automated (switching while notes are held fades the old sound out
over a few milliseconds and nothing hangs). The world you built stays as it was: back in
WORLD mode it is all there.

**What plays:** only the chosen instrument, from the notes the host sends. No generated
patterns, no other instruments, no environment, no arrangement, and the transport does
not matter: it plays with the song stopped or playing, in any Play Mode, and a host
starting, stopping or jumping never cuts a note. It is polyphonic where the instrument
can be (up to six or eight voices; a wind or a hurdy-gurdy fewer), and responds to:

- **velocity**: level and brightness (and a harder strike on mallets, drums and strings);
- **note length**: held instruments (pads, brass, bows, voices, winds) last as long as the
  key, plucked and struck ones ring and are damped when the key is released (bells, drums
  and gongs ring on);
- **the sustain pedal** (CC64);
- **pitch bend**: a whole tone either way (an octave on sirens, risers and gliding leads;
  kits don't bend);
- **the mod wheel** (CC1): vibrato on instruments that have it, otherwise brightness;
- **recorded instruments** (the developer's recordings) play their articulations as in the
  patterns: a short note is the short hit; a note held past the short notes' length
  becomes the long hit (the long recording takes over where the note has got to).
- **kits** (drums, risers, clanks...): each key plays one of the kit's sounds.

**The instrument screen** (it replaces the world's panels):

- **INSTRUMENTS** (left): every instrument **BY PLACE** ("Tavern" > Lute Strum, Frame
  Drum...) or **BY TYPE** (Plucks, Pads, Bass, Leads, Keys, Percussion, Bells & Mallets,
  Strings, Choirs & Voices, Textures, FX; listed as "SPACEPORT > DATA BLIPS"). Type in
  **SEARCH** to narrow it (all the words must match a Place, a name or a type: "pad",
  "tavern lute", "bells"). Click to play it.
- The instrument (top): **<** and **>** step through the list as it is shown (so with
  "pad" in the search box they step through the pads while you play), and the name opens
  every instrument as a menu. Its Place's icon and description, **CHARACTER** (the Place's
  three Characters: they change the instrument's sound and playing), **PLACE AMBIENCE**
  and **OCTAVE** (-2 to +2).
- **PLACE AMBIENCE** (off by default, so it is just the instrument): brings the world in
  under it: the environment layers as the World Controls set them, the World Space reverb,
  the Planet's colour and the Place's own room.
- **SOUND**: **TONE** (dark to bright), **ATTACK** (the middle is as the instrument was
  designed; up: a slower swell, down: quicker; on plucked and struck instruments it is
  **STRIKE**: softer or harder), **RELEASE** (how notes end; **RING** on plucked and struck
  instruments: how long they ring), **DRIVE** (a warm saturation) and **LEVEL**.
- **EFFECTS**: the instrument's own reverb send (Room, Hall, Cathedral, Plate, Cave),
  delay send (Tape, Ping-Pong, Dotted, Reverse, in time with the host), character effect
  (Saturation, Chorus, Phaser, Filter, Lo-Fi) with its amount, and width. Choosing an
  instrument sets its own defaults (the Temple choir in the Cathedral, the Tavern lute in
  a Room...).
- **FACTORY PRESETS** and **PRESET**: instrument presets save the instrument with its
  Character, effects and controls, **separately from world presets** (a preset never
  changes the world; loading a world, a .world file or a scene never changes the
  instrument). 34 factory presets of the best-sounding instruments (the recorded lute,
  fiddle, oud, kalimba and violin, the piano, voice choir, monk chant, alien voices and
  djembes, the Minilogue pads, basses and leads, choirs, bells,
  bowls, harpsichord, strings, keys, mallets and drums) are one click each. **SAVE**
  writes your own as a .wbinst file (in Documents/WORLDBUILDER/Instrument Presets, where
  the PRESET menu lists them); **LOAD** opens one from anywhere.
- **KEYBOARD**: lights up the notes arriving (bright while held, dimmer while the pedal
  holds them), with the pitch bend, mod wheel and pedal beside it. Click a key to hear the
  instrument (lower on the key: louder).

The instruments are levelled for this mode (each has a measured trim), so stepping
through the browser keeps a similar loudness. Every parameter of the screen is a host
parameter ("Instrument Tone", "Instrument Reverb"...), so it can be automated.

### One instrument per track in Logic Pro

1. Create a software-instrument track (Track > New Software Instrument Track) and choose
   **AU Instruments > Veil Audio > WORLDBUILDER** in its Instrument slot.
2. Click **INSTRUMENT** in WORLDBUILDER's top bar and choose an instrument (or a factory
   preset).
3. Play it from your keyboard, record a region, or draw notes in the piano roll (select
   the region, press P). Pitch bend, mod wheel and sustain can be drawn in the region's
   MIDI Draw lanes.
4. For more parts, add more tracks the same way, each with its own WORLDBUILDER in
   Instrument mode and its own instrument. Each track has its own mixer channel, plug-ins
   and automation. Save a setup in Logic's Library ("Save Channel Strip...") to call up
   an instrument in one click.

### One instrument per track in GarageBand

1. Create a Software Instrument track and choose **Plug-ins > Instrument slot > AU
   Instruments > Veil Audio > WORLDBUILDER** in Smart Controls (B).
2. Click **INSTRUMENT** in WORLDBUILDER's top bar and choose an instrument.
3. Play it, record a region, or edit the notes in the Piano Roll editor (E).
4. Repeat on more tracks. **Save each as a patch** (Library (Y) > **Save...**): the patch
   keeps WORLDBUILDER in Instrument mode with its instrument, effects and controls.

Several instances in Instrument mode are light: four, each holding a four-note chord with
their effects, measured about 6 % of one core on the Linux test machine (the CI and the
Mac will differ; see the DEVLOG).

## Exporting parts to separate GarageBand tracks

(With Instrument mode, the quickest way is: drag the scene MIDI's tracks into GarageBand,
put a WORLDBUILDER in **Instrument mode** on each track and choose the instrument the
track is named after: "Spaceport - Pads" is **Spaceport > Pads** in the browser. The
single-instrument mode below is still there: it plays one instrument *of your scene*,
with the scene's Root, Characters and effects.)

A scene's **MIDI** (footer, CAPTURE) has one track per instrument, named
"Place - Instrument" (e.g. "Spaceport - Pads"). Dragged into GarageBand it becomes one
software-instrument track per MIDI track, but a MIDI file cannot choose the instrument of
a new GarageBand track: each gets GarageBand's default sound. To hear each part as its
WORLDBUILDER instrument, give every track its own WORLDBUILDER in **single-instrument
mode**: it plays only that one instrument of the scene, from the track's MIDI, with all
its effects (its own, its Place's and the shared reverbs and delays), and the rest of
the scene stays silent.

1. In the WORLDBUILDER with your scene, click **MIDI** (CAPTURE) and save the file (or
   drag it out). Drag the file onto GarageBand's tracks area: one track per instrument,
   each named "Place - Instrument".
2. On the first new track, open Smart Controls (B) → Plug-ins → Instrument slot → AU
   Instruments → Veil Audio → WORLDBUILDER. Load the same scene
   (the scene browser, or copy the world with SAVE .WORLD / LOAD .WORLD).
3. Open the Place's **EDIT > PATTERNS**, click the tab of the instrument the track is
   named after (e.g. Spaceport > Pads) and switch **SINGLE** on in the KEYS row. The top
   bar now says **SINGLE INSTRUMENT - SPACEPORT - PADS**; the track's region plays it
   (HOST MIDI is ignored in this mode: the region always plays it). REC is off.
4. **Save it as a GarageBand patch** for one-click reuse: in the Library (Y), click
   **Save...** at the bottom and name it after the instrument ("WB Spaceport Pads"). The
   patch keeps WORLDBUILDER, its scene and its SINGLE instrument. Next time, select a
   track and click the patch in the Library.
5. Repeat for each track (each patch loads its own instrument). Mute or remove the
   original whole-scene WORLDBUILDER track if you don't want the scene twice.

Notes: every instance plays its instrument with the scene's Root, Mood, Characters and
effects as they were when you saved the patch; change the scene in one instance and the
others don't follow. A single-instrument instance ignores the arrangement and plays in
any Play Mode, with the song stopped too (so you can play it from the keyboard). Click
the top bar's SINGLE INSTRUMENT to get back to its tab; SINGLE off plays the whole scene
again.

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
tone, width), **SCOOP/GLIDE** (whether its notes slide in pitch: Off, every note starts
on its pitch and held notes move to the next chord at once; On, notes scoop into their
pitch and held notes glide, with the amount in semitones and the time; on by default only
for drums, birds and gulls, cave drips, the Temple gongs and the City sirens), MIDI import
(drop a file) and DRAG MIDI / save.

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
- **Instrument mode is silent.** It plays only the notes the host sends: select the
  track (record-enable it in Logic) so the keyboard reaches it, or click the on-screen
  keyboard. A level of -inf or Power off silences it too. Nothing else plays in this
  mode: for the world, click **WORLD**.
- **Instrument mode sounds too dry or too wet.** Each instrument's reverb and delay are in
  EFFECTS; **PLACE AMBIENCE** adds the world's environment and space under it.
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

The recorded instruments come from the private `AdrianAbbas/Samples` repository and are
never committed here. To build with them, prepare them and point CMake at the result:

```
pip install -r tools/sample-prep/requirements.txt
python3 tools/sample-prep/prepare.py --source ../samples --out build/samples
cmake -S . -B build -DWORLDBUILDER_SAMPLES_DIR=build/samples ...
```

(`tools/sample-prep/README.md` explains the preparation.) CI fetches them with the
`SAMPLES_TOKEN` repository secret. Without them the build works and those instruments play
their synthesised models.

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
