# Manual tests in GarageBand

## Milestone 1 gate (passed)

1. Download `WORLDBUILDER-macOS-Universal` from the latest green CI run (Actions →
   Native build → Artifacts) and unzip it, then unzip the zip inside it.
2. Quit GarageBand. In Terminal type `bash ` (with a space), drag `install-mac.sh`
   from the folder into the window and press Return. Wait for
   "WORLDBUILDER is installed and validated."
3. Open GarageBand, make a new empty project and add a **Software Instrument** track.
4. Press **B** (Smart Controls), click **Plug-ins**, then click the **Instrument**
   slot (the top slot): **AU Instruments → Veil Audio → WORLDBUILDER**.
5. Check, and report each one:
   - [ ] Within about 2 seconds you hear wind and a soft drone, **without playing
     any notes**.
   - [ ] The WORLDBUILDER window appears with the dark navy interface: knobs on the
     left, a globe in the middle, a layer list on the right. (If you see plain
     sliders, switch the plug-in window's view menu from "Controls" to "Editor".)
   - [ ] Turning **Weather** up makes the wind stronger and gustier (howling near
     the top). **Temperature** down makes it thin and icy; up makes it a low rush.
   - [ ] **Root** and **Mood** change the drone's key smoothly (a glide, no click).
   - [ ] **POWER** off fades everything out over about 2 seconds; on fades it back in.
   - [ ] Press play: it keeps sounding while the song plays and after you stop.
6. If anything fails, run `bash diagnose-mac.sh` from the
   [repository](../scripts/diagnose-mac.sh) and send `~/Desktop/WORLDBUILDER-diagnostics.txt`.

Note: GarageBand only processes a software-instrument track while it is selected
or playing. If you select another track, WORLDBUILDER may go quiet until you
select its track again.

## Milestone 2 check: climate layers

Install the new build the same way (steps 1-4 above; the install script replaces
the old copy). Start from a fresh WORLDBUILDER instance so every control is at its
default. Use **S** (solo) on a layer in the mixer to hear it alone, and click it
again to return to the full world. Report anything that sounds synthetic, harsh,
too loud or too quiet; the levels between layers are the first thing to tune.

- [ ] **Default world:** wind and drone as before, plus a few quiet drips, a
  faint crackle now and then, and a low rumble underneath. Nothing should jump out.
- [ ] **Water:** turn **Wetness** up slowly. Drips first, then rain and a
  babbling stream, and near the top a waterfall and lapping waves. **Weather** up
  turns drizzle into a downpour and the waves into surf. **Temperature** fully down
  turns the rain into a soft snow hush.
- [ ] **Fire & Heat:** turn **Temperature** up. A small campfire crackle above
  the middle, a roaring blaze near the top, and at the very top lava bubbling with
  low "bloops". Turn **Wetness** up to douse it.
- [ ] **Ice & Earth:** **Temperature** fully down: the ice creaks and now and then
  cracks with a falling "pew". **Danger** up: the ground rumbles and groans, and
  distant rockfalls happen more often (bigger **Scale** puts them further away).
- [ ] **Thunder:** **Weather** fully up and wait up to half a minute. The globe
  flashes, then the thunder follows (later when the strike is far away). Close
  strikes crack, distant ones roll. **Scale** up moves the storm further away.
- [ ] The globe shows rain or snow, glowing lava when scorching, and a lightning
  bolt when a strike flashes.
- [ ] With everything turned up (Weather, Wetness, Temperature, Danger, Activity
  all at 100 %) it stays clean: no crackling distortion, no clipping on the
  master meter.
- [ ] **Quality** set to Eco still sounds like the same world, a little thinner.

## Milestone 3 check: Places, key and tempo

Install the new build the same way (steps 1-4 above). Start from a **fresh**
WORLDBUILDER instance (it opens with a Castle in slot 1). Mostly listening: report
anything that sounds synthetic, harsh, out of tune, too loud or too quiet.

- [ ] **Default world:** the environment as before, with a distant Castle under it: a
  low brass drone, a slow timpani pulse, now and then a bell or a horn call. Nothing
  should jump out over where dialogue would sit.
- [ ] **Drips (the fix):** solo **Water** (S) with **Wetness** around 30-40 % and
  **Weather** down. The drips should be soft, tuned, ringing drops in the key with
  natural timing, spread left to right, with a little room. Turn **Scale** up: lower
  and longer, like a cavern.
- [ ] **Add Places:** click the **Village** and **Spaceport** cards (below the globe).
  Each appears in the PLACES strips on the right and as a landmark on the globe. Drag a
  landmark to move it: left and right is pan, up is further away.
- [ ] **Detail panel:** click **EDIT** on a strip. Try the three **Character** buttons
  (each should clearly change the music, not just the level). Try **Activity**,
  **Tension** (darker and tighter), **Tone**, **Size** and **Variation**, and switch
  the five **elements** on and off.
- [ ] **Key:** change **Root** and **Mood** in the footer. Everything tonal follows:
  the bed, the drips, the wind's whistles and every Place.
- [ ] **Tempo:** set the song tempo (for example 80, then 140 BPM) and press play. The
  Places' pulses, arps and plucks follow it (the footer shows the BPM). Turn a Place's
  **SYNC TO HOST** off: it keeps its own tempo.
- [ ] **World Controls:** **Population** up makes every Place busier with more echo;
  **Danger** up makes them darker; **Machinery** up makes them straighter and more
  metallic; **Scale** up pushes them further away.
- [ ] **Four Places plus a storm** (Weather and Wetness high): clean, no distortion,
  no clipping on the master meter.
- [ ] **Remove** a Place with **X** and add another one: it fades out and in, no clicks.
- [ ] Save the project, close and reopen it: the Places and their settings come back.

## Milestone 4 check: Pattern Editor and Play Mode

Install the new build the same way (steps 1-4 above). Start from a **fresh**
WORLDBUILDER instance. A new instance starts in **Play Mode: Transport**, so it is
silent until you press play. Projects saved with Milestone 3 load in Free mode and
play as before.

**Play Mode** (footer, next to Quality):
- [ ] **Transport:** silent while stopped. Press play: the world starts at once (no
  slow fade) and stops with a short fade when you stop.
- [ ] Play from bar 9 twice: it sounds the same both times. Export or bounce the song
  (Share → Export Song to Disk): WORLDBUILDER is in the file.
- [ ] **Free:** it plays all the time, as before. (GarageBand exports only up to the
  last region, so for exports in Free mode put a region over the whole section.)

**Pattern Editor:** click **EDIT** on the Castle strip, then **PATTERNS** (top right of
the detail panel). It covers the left and centre of the window.
- [ ] Tabs for **Bells**, **Horns** and **Pulse**. Each shows the pattern the Castle is
  playing now; with the host playing, a cyan playhead runs across it in time with
  the song.
- [ ] Turn the Place's **Activity** (close the editor with BACK, change it, reopen):
  a generated pattern follows the knob.
- [ ] **Draw:** click an empty cell to add a note, drag to lengthen it. Drag a note to
  move it. **Alt-click** or **right-click** deletes it. **Double-click** locks it (a
  small padlock). The status line changes to "Edited": from now on the pattern stays
  as written whatever the knobs do.
- [ ] **Select:** drag a box, then **COPY**, click a step, **PASTE**; **UP**/**DOWN**
  transpose; **x2** and **/2** change the length; **UNDO**/**REDO**; **CLEAR**;
  **GENERATE** gives a fresh pattern (and follows the knobs again).
- [ ] **Velocity** lane: drag bars up and down. **Chance** lane: lower a note's chance;
  it fades in the grid and plays only on some passes.
- [ ] **12 TONE** shows all semitones; **CHORDS** off makes the rows the key's notes
  instead of following the chords. **LENGTH**, **GRID** (try 1/8T) and **SWING**.
- [ ] **RANDOMNESS** at 0 %: the pattern repeats exactly. Raise it and switch the
  **T V P S +** buttons (timing, velocity, pitch, skip, extra notes) one at a time:
  only that aspect changes. Locked notes never change.
- [ ] **EVOLVE** up: the pattern slowly drifts over a minute or two; **RETURN** every 4
  bars brings it back to the original at each return.
- [ ] Try the Village (Plucks, Hand Drums, Wood) and Spaceport (Pulses, Risers,
  Announce) tabs too. The Spaceport risers show a lower opacity: their chance.
- [ ] Change **Root** and **Mood**: edited patterns follow the key.
- [ ] Save the project, close and reopen: edited patterns, their randomness settings
  and the Play Mode come back.

**MIDI** (please report exactly what works in each host):
- [ ] **DRAG MIDI** (right of the MIDI buttons): drag it onto a track in **Logic** and
  in **GarageBand**. Does a MIDI region appear? If not, does the host refuse the drop,
  or nothing happen?
- [ ] **SAVE MIDI** saves a .mid file; import it into the host and check it plays the
  pattern.
- [ ] Drag a .mid file from Finder onto the editor (or onto a tab): the notes become
  that element's pattern (snapped to the scale; turn **SNAP** off to keep exact
  pitches). **IMPORT** does the same through a file dialog.

## Milestone 5a check: six instruments per Place, the new selector and look

Install the new build the same way. Start from a **fresh** instance (press play: the
default Play Mode is Transport). Projects from Milestone 4 load, but the Castle,
Village and Spaceport now have six instruments, so they sound fuller and different.

**The selector** (below the globe):
- [ ] 20 cards. **ALL** scrolls with the mouse wheel or trackpad (a thin bar on the
  right shows where you are); **FANTASY**, **MODERN**, **SCI-FI** and **NATURE** show
  one group each.
- [ ] Hover a card: it lifts, its light glows, windows flicker and particles drift.
  The 17 Places still to come are dimmed, marked "IN 5B/5C/5D", and cannot be added.
- [ ] Is the animation smooth, and does GarageBand stay responsive with the window
  open? (The interface animates at 30 frames per second.)

**The theme follows the world:**
- [ ] With only a Castle, the interface turns towards dusk purple and amber; add a
  Spaceport and it shifts to cold cyan; remove the Castle and it settles there. Each
  change takes about a second. Muting a Place or turning it down weakens its colour.
- [ ] The globe shows the Places: fields for Castle and Village, an orbit ring with
  small ships for the Spaceport; drifting embers, motes or sparks in the globe panel.

**Six instruments each** (EDIT on a strip):
- [ ] The detail panel has header art and colour for the Place, the effect character
  under ELEMENTS, and six instrument rows, each with an icon, a switch and a level.
- [ ] **Castle:** Brass Drone, Bells, Horns, Timpani, Harp, Choir. **Village:** Lute,
  Hurdy-Gurdy, Hand Drums, Marimba, Recorder, Bells (Characters: Market Day, Harvest
  Night, Quiet Dawn). **Spaceport:** Pulses, Pads, Risers, PA Tones, Engine, Data Blips.
  The Village shows **USE DORIAN** (its suggested Mood): it changes the Mood only when
  clicked.
- [ ] **PATTERNS**: six tabs with icons. Above the toolbar, the instrument's strip:
  LEVEL, ON, **MUTE** and **SOLO**. Solo one instrument: you hear only it (within that
  Place); its tab shows S, a muted one M. Mute and solo are saved with the project.
- [ ] Listen to each instrument alone in each Character. Tell me which ones sound
  weak, too loud, or not like their name: this is the tuning that matters most before
  the other 17 Places use the same instruments.
- [ ] The drones, pads and choir now play from patterns too (one long note per
  chord). Press play in the middle of a song: they sound at once, not at the next
  chord.
- [ ] Root, Mood and the host tempo: every instrument follows them.
- [ ] Four Places at once: does GarageBand's CPU meter stay comfortable?

## Milestone 5a.1 check: world theme and Place overlays

Install the new build the same way. The header should read **MILESTONE 5A.1 PREVIEW**;
if it still says 5A, GarageBand is using the old build. Note: a fresh instance
starts with a Castle in slot 1.

**The theme** (set OVERLAYS to Off first, to see only the colours):
- [ ] Castle only: background and panels plum-purple, with an amber glow on the panel
  frames and behind the globe. Remove it (X): it goes back to navy in about a second.
- [ ] Spaceport only: deep teal with cyan frames. Village only: warm brown and amber.
- [ ] Castle + Spaceport: a mix between the two. Turning one Place's LEVEL down or
  muting it moves the colours towards the other Place, smoothly.
- [ ] The globe: patchwork fields for Castle and Village, two tilted orbit rings with
  ships and trails for the Spaceport.
- [ ] EDIT: the Place's scene is clearly visible across the top of the detail panel,
  the name and description readable. PATTERNS: every tab has its instrument's icon.

**The overlays** (OVERLAYS: Full):
- [ ] Castle: grass along the bottom and on the panels' top edges, ivy climbing the
  panel corners, red banners at the top, fireflies drifting in the gaps.
- [ ] Spaceport: neon frames and corner brackets, lights running along the frames, a
  scan line sweeping down, a flickering hologram with a radar in the header.
- [ ] Village: wooden beams and posts, a plank floor, rooftops with chimney smoke and
  a string of lanterns in the header, lanterns glowing in the gaps.
- [ ] Castle + Spaceport: grass and ivy with neon lining at the same time.
- [ ] Add or remove a Place: its overlay grows in or fades out over about a second.
  Turn its LEVEL down: the overlay gets fainter.
- [ ] No overlay covers a knob, a slider, text, a meter or the Pattern Editor, and
  every control still works where it is.
- [ ] Subtle is lighter and calmer; Off removes them. The setting is saved with the
  project.
- [ ] With overlays on, does GarageBand stay responsive and its CPU meter comfortable?
  If not, tell me whether Subtle helps.

## Milestone 5b check: six new Places

Install the new build the same way. The header should read **MILESTONE 5B PREVIEW**.
**Overlays now start Off**, also in projects saved with 5a.1 (they stored Full, the
old default); switch OVERLAYS to Full or Subtle in the footer to see them.

**The new Places** (their cards are no longer dimmed; add each one alone first):
- [ ] **Factory / Forge**: Anvil, Pistons, Steam, Bowed Metal, Gears, Furnace Bass.
  Steam Mill (a lilting chug, a ringing anvil marking the bar, steam swells), Iron
  Forge (hammer strikes with bounces, slow bellows, a dark drone), Assembly Line (a
  driving sixteenth-note machine). Gritty saturation and a metallic ring tuned to the
  chord.
- [ ] **City**: Chimes, Synth Bass, E-Piano, Crowd, Sirens, Traffic Hum. Rush Hour
  (pulsing bass, syncopated chords, a loud crowd, sirens), Night City (slow rich
  chords, a walking bass, a distant siren, a lone chime), Rain-soaked (soft broken
  chords, wet and blurred, rain). The crowd is a murmur in the key, never words.
- [ ] **Forest** (USE LYDIAN): Flute, Birds, Wood, Kalimba, Strings, Rustle. Enchanted
  (a floating flute, glittering kalimba, deep and blurred), Dark Woods (low and
  sparse, a knocking log, an owl), Morning Glade (a dawn chorus, a dancing flute, wood
  clicks, close and open).
- [ ] **Alien Hive** (USE WHOLE-TONE): Pulses, Glass Calls, Chitter, Throb Bass,
  Formants, Swarm. Dormant (slow breathing), Awakening (calls answering each other),
  Swarm (a buzzing mass). Raising Tension or Danger wakes the hive. A fluttering,
  ring-modulated shimmer and a warped delay.
- [ ] **Temple** (USE PENTATONIC): Choir, Gongs, Bowls, Chant Drone, Temple Bells,
  Zither. Sacred Dawn, Ancient Rite, Forsaken. A long shimmer reverb.
- [ ] **Cavern / Mine**: Drips, Sub Drone, Metal Taps, Bowed Bass, Crystals, Rumble.
  Crystal Cave (ringing crystals, bright drips), Deep Mine (pick-axe taps echoing, a
  rumble), Flooded (dense dripping, everything muffled under water). A long dark
  reverb and a slapback.
- [ ] Listen to each instrument alone (SOLO in PATTERNS) in each Character, as for
  5a. The new voices to judge: the bowed metal and bowed bass, the drips, the crowd,
  the sirens, the swarm, the gongs and singing bowls, the rustle. Tell me which sound
  weak, harsh, too loud or not like their name.
- [ ] Root, Mood and the host tempo: every instrument follows them.
- [ ] Four of the new Places at once (for example Factory, City, Hive, Cavern): does
  GarageBand's CPU meter stay comfortable?

**Their overlays** (OVERLAYS: Full):
- [ ] Factory: riveted girders on the panel tops, pipes with valves down the gaps,
  hazard stripes, a skyline with smokestacks; steam rising, embers, turning gears, a
  blinking lamp.
- [ ] City: a night skyline with an antenna and a neon OPEN sign in the header, neon
  strips on the frames, a street along the bottom; windows switch on and off, the sign
  buzzes, car lights pass.
- [ ] Forest: thick vines, ferns, moss and mushrooms, a tree line in the header;
  leaves fall down the gaps, pollen drifts, pale mushrooms glow.
- [ ] Alien Hive: tendrils curling along the frames, honeycomb in the gaps, egg pods
  that pulse, spores rising.
- [ ] Temple: gold trim and corner filigree, stone pillars in the gaps, a facade with
  incense burners in the header; incense smoke, flickering lamps, gold motes.
- [ ] Cavern: rock, stalactites and crystal clusters; water drips from the
  stalactites, the crystals glint.
- [ ] Nothing covers a control, and they combine with each other and with the first
  three.
- [ ] These six move only small sprites, at 15 frames a second (10 at Subtle). Compare
  GarageBand's CPU meter with one of them on Full against the Castle or Spaceport on
  Full: they should cost clearly less. (Making the first three as light is part of the
  visual polish milestone.)

## Milestone 5c check: five new Places

Install the new build the same way. The header should read **MILESTONE 5C PREVIEW**.
Overlays still start Off; switch OVERLAYS to Full or Subtle in the footer to see them.

**The new Places** (add each one alone first):
- [ ] **Ruins**: Piano Shards, Music Box, Broken Bells, Wind, Ghost Pad, Low Toms.
  Overgrown (worn and warm, the music box still turning, a soft heartbeat), Haunted
  (a ghostly choir, long reverse swells, a slow uneven march), Desolate (wind
  whistling through the stones, tolling cracked bells, a lone tom). The piano notes
  swell in and stop, as if played backwards; the old instruments sound like a worn
  recording (lo-fi decay) and echo backwards.
- [ ] **Spaceship Interior**: Engine, Life Support, Computer, Airlock, Hull, Alarm.
  Cruising (a calm hum, gentle arpeggios, the pumps breathing, a soft notification),
  Red Alert (a throbbing engine, racing computers, a two-tone alarm every bar),
  Derelict (a sputtering engine, a groaning hull, a stuttering computer, a dying
  alarm). A small metal room that rings on the chord, and a delay whose repeats sweep
  through a band.
- [ ] **Biodome** (USE LYDIAN): Pads, Mallets, Birdsong, Water Plucks, Vents, Shaker.
  Thriving (running mallets, birds everywhere), Night Cycle (dim and slow, crickets),
  Failing (the power sags, the pads waver, the vents sputter). A bright airy space and
  a gentle chorus.
- [ ] **Sea Ship** (USE DORIAN): Creaks, Concertina, Rope Swells, Ship Bell, Waves,
  Gulls. Calm Seas (a sailors' oom-pah, the bell telling the time in pairs, gulls),
  Storm (thrown about, ropes straining, the bell clanging), Ghost Ship (a slow lament
  in the fog, a tolling bell, groaning timbers). Everything but the gulls sways with
  the ship's roll (a slow side-to-side sway and a slight pitch bend) in a wide, wet
  space.
- [ ] **Suburbs** (USE MAJOR): E-Piano, Pop Pulse, Wind Chimes, Sprinkler, Music Box,
  Bass. Sunny Afternoon (an easy off-beat comp, the sprinkler ticking round), Evening
  (long soft sevenths, chimes on the porch), Uncanny (the same chord held too long, a
  slowed music box, everything wavering like stretched tape). Warm tape in a short
  room.
- [ ] Listen to each instrument alone (SOLO in PATTERNS) in each Character. The new
  voices to judge: the reversed piano, the music boxes, the cracked bells, the wind,
  the airlock and rope swells, the hull, the concertina, the creaks, the waves, the
  gulls, the sprinkler. Tell me which sound weak, harsh, too loud or not like their
  name.
- [ ] Root, Mood and the host tempo: every instrument follows them.
- [ ] Four of the new Places at once (for example Ruins, Spaceship, Sea Ship,
  Suburbs): does GarageBand's CPU meter stay comfortable?

**Their overlays** (OVERLAYS: Full):
- [ ] Ruins: cracks along the frames, moss and ivy, broken stonework along the bottom,
  crumbling pillars in the gaps, a broken colonnade in the header; dust drifts,
  pebbles fall, pale wisps glow.
- [ ] Spaceship: riveted plating and corner brackets, conduits down the gaps, hazard
  stripes, a deck grating, consoles in the header; status lights blink, data pulses
  run down the conduits, red lamps glow.
- [ ] Biodome: a geodesic dome in the header, ferns and leaves, irrigation pipes on
  the frames; water drips, pollen rises, grow lights glow.
- [ ] Sea Ship: rope along the frames with knots, masts in the gaps, planks and waves
  along the bottom, portholes and a ship's wheel in the header; light glints on the
  water, portholes shimmer, gulls drift past.
- [ ] Suburbs: a picket fence and lawn, hedges, lamp posts in the gaps, houses with a
  power line in the header; windows glow, a sprinkler sprays, butterflies flutter.
- [ ] Nothing covers a control; they combine with each other and with the others.
- [ ] Like the 5b ones they move only small sprites at 15 frames a second: compare
  GarageBand's CPU meter with one of them on Full against a 5b overlay.

## Milestone 5d check: the last six Places

Install the new build the same way. The header should read **MILESTONE 5D PREVIEW**.
Every card in the selector can now be added. Overlays still start Off; switch OVERLAYS
to Full or Subtle in the footer to see them.

**The new Places** (add each one alone first):
- [ ] **Mall** (USE LYDIAN): E-Piano, Vibraphone, PA Chimes, Escalator, Fountain,
  Drums. Busy Weekend (a light syncopated comp, a running vibraphone with its motor
  tremolo, frequent three-note PA dings), Empty Night (long ninth chords, a lone
  vibraphone, the escalator hum and the fountain), Dead Mall (the vaporwave option:
  slowed, half-time, everything detuned flat and wobbling, a dim long hall). A glossy
  reverb with a slight detune on the other two.
- [ ] **Military Camp**: Snare March, Bugle, Low Drums, Radio Blips, Clanks, Strings.
  Morning Drill (a brisk march, reveille), On Alert (driving sixteenths, a call to
  arms, urgent radio traffic, a racing string ostinato), Night Watch (rim clicks, a
  slow heartbeat, taps heard over the radio). The radio blips always go through a
  gritty radio filter; a tight room and a slapback.
- [ ] **Beach** (USE MIXOLYDIAN): Guitar, Steel Drum, Waves, Shaker, Breeze Pad,
  Gulls. Tropical Day (a lilting strum, a steel-drum tune, a son clave), Sunset (slow
  fingerpicking, the pad glowing), Stormy Shore (a nervous tremolo, the surf rising,
  rain on the shaker). An open, airy space; a chorus on the guitar, steel drum and pad.
- [ ] **Desert** (USE PHRYGIAN): Oud, Frame Drums, Ney Flute, Tanpura, Wind, Chimes.
  Oasis (a maqsum rhythm, ornamented oud phrases), Endless Dunes (a slow taqsim over
  the drone, long echoes), Sandstorm (driving drums, tremolo oud, the wind howling).
  A wide dry space and a long sparse echo on the oud, ney and chimes.
- [ ] **Palace** (USE MAJOR): Harpsichord, Strings, Fanfare, Glockenspiel, Timpani,
  Choir. Grand Ball (a waltz: oom-pah-pah, four waltz bars to three of the song's),
  Throne Room (rolled chords, fanfares answered, timpani rolls, a full choir), Fallen
  Palace (a broken, slow harpsichord, thin strings, a distant call, a ghostly choir).
  A grand hall.
- [ ] **Tavern** (USE DORIAN): Lute Strum, Frame Drum, Fiddle, Glass Clinks,
  Squeezebox, Stomps. Lively Night (a reel, everyone stamping and clapping), Quiet
  Corner (slow arpeggios, a fiddle air by the fire), Brawl (everything too fast, the
  fiddler sawing, glasses flying). A warm wooden room with tape warmth.
- [ ] Listen to each instrument alone (SOLO in PATTERNS) in each Character. The new
  voices to judge: the vibraphone's tremolo, the PA chimes, the fountain, the bugle,
  the radio blips, the clanks, the steel drum, the oud, the ney, the tanpura, the
  harpsichord, the fanfare, the fiddle, the squeezebox, the glass clinks, the stomps.
  Tell me which sound weak, harsh, too loud or not like their name.
- [ ] Root, Mood and the host tempo: every instrument follows them. The Grand Ball's
  waltz still lands on the host's grid (it repeats every three bars).
- [ ] Four of the new Places at once (for example Mall, Military Camp, Desert,
  Tavern): does GarageBand's CPU meter stay comfortable?

**Their overlays** (OVERLAYS: Full):
- [ ] Mall: pastel neon tubes bent round the panels' corners, storefronts with
  glowing signs and awnings in the header, potted palms in the gaps, a polished floor;
  the floor glints, signs flicker, a fountain sprays, light sweeps the floor.
- [ ] Military Camp: barbed wire along the panel tops, camouflage netting on the
  corners, tents, a flag and a radio mast in the header, ammunition crates in the gaps,
  sandbags; embers and smoke from fires, the flag flutters, lamps and the mast blink,
  a searchlight sweeps the sandbags.
- [ ] Beach: sand with shells and starfish, surf and foam, palm trunks in the gaps,
  fronds at the corners, a low sun, an umbrella and the sea in the header; the water
  glints, foam bubbles, gulls drift.
- [ ] Desert: dunes with sand drifting across the bottom and heaped on the frames,
  carved sandstone along the panel tops, brass lanterns in the gaps, a caravan and an
  oasis palm under a pale sun; the lanterns flicker, the sun glows.
- [ ] Palace: gilded moulding with scrolls at the corners, red velvet drapes, marble
  columns, chandeliers with candles, a chequered floor; candles flicker, crystals
  glint, gold motes fall.
- [ ] Tavern: oak beams with nails, candles at the panels' corners, barrels in the
  gaps, floorboards and a hearth, a shelf of tankards and bottles and an INN sign; the
  candles and the hearth flicker, embers and smoke rise.
- [ ] Nothing covers a control; they combine with each other and with the others.
- [ ] Like the 5b and 5c ones they move only small sprites at 15 frames a second.

## Milestone 6 check: the sound overhaul and each instrument's effects

Install the new build as before (CI's package). Nothing in this milestone has been
heard by a person: these checks are the first listen.

**Effects per instrument** (PATTERNS, the EFFECTS row under each instrument's level):
- [ ] Each reverb type on one instrument (Room, Hall, Cathedral, Plate, Cave): clearly
  different spaces; the Cave has slapping early echoes.
- [ ] Each delay type in time with GarageBand's tempo (Tape a quarter that wobbles and
  darkens, Ping-Pong eighths left-right, Dotted a dotted eighth, Reverse each beat
  backwards). Change the tempo: the delays follow.
- [ ] Each character effect (Saturation, Chorus, Phaser, Filter, Lo-Fi) and its amount;
  Tone dark to bright; Width mono to extra wide (above 100 % the instrument spreads,
  in mono nothing changes).
- [ ] Switch types while playing: no clicks. Double-click a control: back to that
  instrument's own default.
- [ ] Add a Place from its card: its instruments start with their own effects (the
  Temple choir in the Cathedral, the Tavern lute in the Room).
- [ ] Automate a send in GarageBand; save, reopen the project: everything comes back.
  Open a project saved before this milestone: it plays, each instrument with its
  Place's default effects.
- [ ] Four Places at once with many effects: GarageBand's CPU meter stays comfortable.

**The new instruments** (solo them in PATTERNS; tell me which sound wrong, weak, harsh
or synthetic where they should be natural):
- [ ] Plucked strings: the Tavern lute's strum (the stroke crossing the strings), the
  Village lute, the Desert oud (paired strings, a quill), the Castle harp, the Beach
  guitar, the Temple zither, the Palace harpsichord (two strings per note, the damper
  as each note ends).
- [ ] Bowed strings: the Tavern fiddle, the Military and Palace sections, the Forest
  violas, the Cavern double basses, the Village hurdy-gurdy (a wheel, nasal, grinding).
- [ ] Brass: the Castle brass drone and French horns, the Palace trumpets, the Military
  bugle: louder notes brighter, the lip "scoop" into each note.
- [ ] Reeds: the Sea Ship concertina and the Tavern squeezebox (beating reed pairs, the
  squeezebox's bass).
- [ ] Winds: the Village recorder (a clear chiff), the Forest flute (airy), the Desert
  ney (breathy, wandering).
- [ ] Bells and bars: the Castle church bells (long, beating), the Temple bells and
  gongs (the gong's pitch sags, its shimmer blooms), the Palace glockenspiel, the
  Village marimba, the Mall vibraphone, the Tavern glasses, the Ruins' worn music box
  against the Suburbs' clean one, the Forest kalimba.
- [ ] Skins: the Castle and Palace timpani, the frame drums (Village, Desert, Tavern),
  the Military snare (wires) and bass drum, the Ruins toms, the Mall's brushed snare.
- [ ] Voices: the choirs (Castle, Temple, Palace) and the Temple monks' chant.
- [ ] Keys: the City's tine piano (barking when played hard) against the Suburbs'
  mellow one and the Mall's FM piano.

**Weight**:
- [ ] The Places feel heavier, slower and more sustained than in 5d: longer rings and
  releases, less bounce (Swing 8 % by default), fuller low end. Too heavy anywhere?
  The weights are one table in `Source/Places/Ensemble.h` (`weight::`).
- [ ] The Characters sound like different spaces now (e.g. the Desert's open dunes
  against the dry oasis, the dead Mall's atrium, the forsaken Temple).

## Milestone 7 check: six sci-fi Places and Score Intensity

Load the CI build in GarageBand (Transport mode, press Play). Turn Overlays to Full to
see each new overlay.

**The SCI-FI category** lists ten Places now; the six new cards are at the end (Space
Station, Cyberpunk Megacity, Starship Bridge, Asteroid Mining Colony, Alien Ruins,
Nebula). For each, add it alone, then try its three Characters:
- [ ] Space Station: the hull drone slowly turning left and right around you (one turn
  every 16 beats), clamps, comms beeps, the vent air, the reactor bass, the gliding
  lead. Emergency: clamps slamming, the reactor racing. Abandoned: a breach hissing.
- [ ] Cyberpunk Megacity: the driven bass, the neon FM arp bouncing left and right,
  the rain pad (a faint hiss on it), the drum machine (half-time, four on the floor,
  a breakbeat in a Riot), crushed glitch stabs, the far megastructure.
- [ ] Starship Bridge: console arps, the warp core throbbing, the crew murmuring over
  the intercom (no words), sensor pings echoing. Warp Jump: an eight-bar build, the
  core spinning up under a riser, the brass swelling to the jump.
- [ ] Asteroid Mining Colony: the whining drill, the grinding bass, ringing clanks,
  the dust hiss, the low hymn, the lonely beacon call with its slapback.
- [ ] Alien Ruins: beating crystals, the deep machine throb, the choir swelling in
  backwards, the sub drone, ritual drums for the Guardian, the whispers.
- [ ] Nebula: slow pads opening and closing, the cosmic wind, star bells and stardust
  rising into an endless shimmer, solar flares.
- [ ] Each one sounds rich and cinematic rather than thin; each instrument stands
  apart (solo them in the Pattern Editor).
- [ ] Overlays: hull panels with status lights; neon tubes, signs and rain; a heads-up
  display with brackets and a radar; rubble and drill-light beams; glowing glyphs on
  cracked stone; gas clouds and stars. None covers a control.

**Score Intensity** (the knob in the header, left of POWER):
- [ ] With two to four Places, sweep it from 0 % to 100 %: a quiet, sparse bed at 0 %,
  the Places as before at 50 %, busier, tenser, brighter and louder at 100 %.
- [ ] Jump it from 0 % to 100 % (or draw a step in its automation lane): the climax
  arrives over about a second, with no jump or click.
- [ ] Automate it in GarageBand over a few bars; the cue builds smoothly, and a
  bounce matches playback.
- [ ] Double-click resets it to 50 %. An older project opens at 50 % and sounds as it did.

## Milestone 8 check: capture and export

Load the CI build in GarageBand with two or three Places (Transport mode). The new
**CAPTURE** section is in the footer, between OVERLAYS and the output knobs.

**RENDER**
- [ ] Choose 8 bars and Mix, click RENDER: it shows its progress and turns into
  CANCEL while it renders; the plugin keeps playing; then "Rendered" and WAV lights up.
- [ ] Drag **WAV** onto an empty part of the tracks area (or an audio track at bar 1).
  What happens? (JUCE's external file drag; untested in GarageBand and Logic.)
- [ ] Click **WAV** instead: a save dialog. Save, then drag the file in from Finder at
  bar 1. Mute WORLDBUILDER's track and play: the audio file sounds the same as the
  plugin did from bar 1, in time with the song, and ends exactly at bar 9.
- [ ] Stems: choose Stems, RENDER; clicking WAV asks for a folder and writes one file
  per Place plus "Environment". Together they sound like the mix.
- [ ] Try another tempo (e.g. 90 bpm) and a 44.1 kHz and a 48 kHz project: the render
  follows the tempo and rate.
- [ ] Click RENDER again while it runs: it cancels.

**MIDI**
- [ ] Click **MIDI**: a save dialog; save the .mid file and drag it onto the tracks
  area: one track per instrument, named "Place - Instrument", starting at bar 1 and
  ending at the chosen length, in the song's tempo.
- [ ] Drag **MIDI** straight out of the plugin onto the tracks area: what happens?
- [ ] Put one of those regions on a GarageBand instrument: the notes are the
  instrument's part, in key.
- [ ] Drop the saved .mid file (or one track exported from GarageBand) onto a Pattern
  Editor tab: the notes come back.

**Host sync and automation**
- [ ] Play the same 4 bars several times from the same position, and export the song:
  the export matches playback (Transport mode).
- [ ] Automate a few controls over a cue, with steps as well as ramps: Score
  Intensity, a Place's level and mute, a Character switch, Activity or Tension, Root.
  Nothing clicks or jumps; the export matches playback.

## Milestone 9 check: Locations and Journeys

Load the CI build in GarageBand. The new **LOCATIONS** row sits between the panels and
the footer; the globe has eight markers on an orbit around it.

**Saving and travelling**
- [ ] Build a world (two or three Places, some World Controls), click chip 1: it fills
  ("Location 1"), and marker 1 lights up on the orbit. Double-click it and rename it.
- [ ] Change the world a lot: another Place in one slot, a Place added to an empty slot,
  one removed, other Characters, Temperature, Weather, Root and Mood. Click chip 2.
- [ ] Click chip 1 (Travel Time 4 s): does it sound like moving through a world rather
  than a crossfade? The changed Place should recede into the distance and the old one
  approach; the removed one should come back from far away; Root and Mood should change
  in the middle without a clash. The globe turns faster, a comet runs along the orbit,
  chip 1 fills; on arrival the knobs show Location 1.
- [ ] Clicks, level jumps or a moment of silence anywhere on the way? (Tested by
  measurement only.)
- [ ] Travel Time 30 s, and 2 bars with Travel Sync. Click another chip half way: it
  turns back smoothly.
- [ ] Right-click a chip: Save here (replace), Rename, Colour, Clear.

**Automation**
- [ ] Show automation, choose **Destination**, draw 1 at bar 1, 2 at bar 9, 1 at bar 17
  (Transport mode). Play from bar 1: the world travels at bars 9 and 17. Start playing
  at bar 12: the world is at Location 2 straight away. Export the song: it matches.
- [ ] Switch on **JOURNEY**, From 1, To 2, and draw the **Journey** automation from 0 to
  100 % over 16 bars: the listener walks from one place to the other. Switch it off:
  the world stays where the Journey stood.

**Saving**
- [ ] Save the project, close it, reopen: the Locations (names, colours, worlds) are
  back and no journey starts on its own.
- [ ] SAVE .world, change everything, LOAD .world: the world and its Locations come
  back; Master Level and Play Mode stay as they were. Do the file dialogs open inside
  sandboxed GarageBand?

## Milestone 10 check: planet types and the new look

Load the CI build in GarageBand.

**Planet sound**
- [ ] A world with two or three Places. PLANET menu (top right of the globe): step
  through the seven planets at INFLUENCE 35 %. Is each a subtle but real colour, and
  does each change crossfade smoothly (no click, no jump)?
- [ ] INFLUENCE 100 %: Rocky dry and close; Ocean watery and rolling; Ice Giant bright
  and shimmering; Gas Giant vast, swirling, with deep sub and a slow swell; Desert dusty
  and wide; Lava hot, saturated, rumbling; Forest soft and damped. Is any too much, too
  little, or unpleasant? (Tested by measurement only: each within about 1 dB of the
  others in level.)
- [ ] Automate Planet Influence and the Planet: smooth?
- [ ] Open a project saved with Milestone 9: it should sound exactly as before
  (Influence 0 %).
- [ ] Save two Locations on different planets and travel: the planet changes half way.

**The look** (compare with `docs/snapshots/before-*` and `after-*`)
- [ ] The globe on each planet: rich enough? Rings on Ice and Gas Giants, clouds, city
  lights on the night side (add City or Village and raise Population), lava glowing.
- [ ] Buttons, toggles, menus, tabs and cards: raised, glowing on hover, lit when on;
  icons readable at 100 %; nothing cut off. The Place card hover lift and glow.
- [ ] CPU: the globe now draws a textured planet every frame (30 fps, about 66 000
  pixels). Is the editor still light on the MacBook Air with the window open (Activity
  Monitor, GarageBand's CPU meter), compared with Milestone 9?


## Milestone 11a check: the Arrangement

Load the CI build in GarageBand. Add three or four Places first.

**ARRANGE view**
- [ ] ARRANGE (top bar) opens the timeline over the globe; BACK closes it. Readable at 100 %?
- [ ] + SECTION five times (Intro, Build, Climax, Wind-down, Outro). Rename one by typing;
  change colours, lengths (slider, and dragging a section's right edge), Plays; drag a
  section to reorder. UNDO / REDO (and Cmd-Z) step back through all of it.
- [ ] The grid: click cells off in the Intro, set some to Enter halfway / Fade in in the
  Build (right-click), Shift-click variants to B in the Climax, a Fill in the Build.
- [ ] PREVIEW with GarageBand stopped: the arrangement plays from the selected section;
  the playhead moves and the section glows; clicking another section jumps there.

**Playback**
- [ ] Play Mode: Arrangement. Play from bar 1: do instruments enter and leave exactly at
  the boundaries, with no clicks? Does the Fill land in the last bar before a change?
- [ ] Start playback in the middle of a section (e.g. bar 37): it plays that section
  straight away. Play the same spot twice: identical?
- [ ] Intensity ramps: a Build from 40 % to 75 % audibly grows; automation of Score
  Intensity on top still works.
- [ ] End: stop (fades out over two bars) and End: loop to a section.
- [ ] Bounce / export the project in Arrangement mode: the bounce sounds like playback.

**Variants**
- [ ] Pattern Editor: tabs A, B, C, FILL. Are the generated B (busier), C (sparser) and
  Fill useful starting points? Edit B: A stays as it was.
- [ ] Save and reopen the project: arrangement, variants and grid come back.

## Milestone 11b check: transitions, templates, Auto-arrange

Load the CI build in GarageBand with two or three Places, Play Mode Arrangement.

**Templates and Auto-arrange**
- [ ] ARRANGE → TEMPLATE → Classic Arc: five sections appear, already arranged. Play from
  bar 1: a sparse intro, instruments entering through the Build, everything in the
  Climax, thinning out after. Musical? UNDO brings back what was there.
- [ ] Try Battle, Boss Fight, Town Theme (loops back to A) and Cinematic Reveal.
- [ ] Rename your own sections (e.g. "Combat") and press AUTO-ARRANGE: sensible?
  Change the Seed (footer) and AUTO-ARRANGE again: a different take?

**Transitions** (INTO NEXT on a section; listen across its boundary)
- [ ] Crossfade 2 bars: instruments leaving fade out, entering ones fade in, no bump.
- [ ] Riser and Reverse swell: in key? Too loud or too quiet against the Places?
- [ ] Drum fill: the rhythm instruments fill the last bar.
- [ ] Drop 1 and 2 beats: silence, then the sub hit on the downbeat. Clicks?
- [ ] Filter sweep: closes down, opens on the next section. Clicks?

**Locations and overrides**
- [ ] Save two contrasting Locations. Give the Build Location 1 and the Climax Location 2:
  the world travels and arrives on the Climax's downbeat (Travel Time sets how long).
  Your knobs are unchanged afterwards; leaving Arrangement mode returns to them.
- [ ] A section with PLANET Ocean and WORLD Danger 100 %: only while it plays.
- [ ] Start playback inside a transition or a journey: same every time? A bounce matches?
- [ ] Save and reopen the project: transitions, Locations and overrides come back.

## Milestone 11c check: game-audio export

Use an arrangement with a looping section (TEMPLATE → Exploration Loop is quick).

- [ ] CAPTURE: BARS → Arrangement, Mix, RENDER. Drag the WAV to bar 1: does it match
  GarageBand playing the project in Play Mode Arrangement (including a section Location)?
- [ ] Stems from the arrangement: one file per Place plus the environment.
- [ ] Sections: three files (`<Place>_Intro`, `_Loop`, `_Outro`). Put them back to back on
  a track: do they join? Loop `_Loop.wav` in GarageBand (cycle): seamless, no click or
  level bump at the loop point?
- [ ] Stingers: a file per section ending, ringing out to silence. Useful as cues?
- [ ] MIDI with Arrangement selected: drag it onto the tracks area. Do the tracks enter
  and leave with the sections?


## Milestone 12 check: separate outputs, scenes, INSPIRE

First, the risk: WORLDBUILDER now has five output buses (main plus Place 1-4).

- [ ] GarageBand still loads it on a software instrument track and plays (main output,
  every Place audible), the interface appears, and an older project opens unchanged.
- [ ] Scenes: **>** in the header loads "Ambush at the Tavern". Play Mode becomes
  Arrangement; press play: do the sections build, and does it travel between its
  Locations ("Lively Night" → "Brawl")? Try three or four more (Storm at Sea, Neon Rain
  Chase, Into the Nebula, Market at Dawn). Do they sound like their names? Any too
  loud or too quiet?
- [ ] The scene menu (the name between < and >): every scene listed by world.
- [ ] **UNDO** after a scene: is your world back (Places, arrangement, Locations, Play
  Mode)?
- [ ] **INSPIRE** a few times: are the worlds musical and different? Do the Places belong
  together? UNDO after INSPIRE.
- [ ] Shift-drag a knob: finer than a plain drag?

**Logic Pro only** (separate outputs):
- [ ] Insert WORLDBUILDER as **Multi Output (5xStereo)** and add the four aux channels.
  With two Places, does each aux play its own Place?
- [ ] PLACE OUTPUTS **Copy**: the main channel plays the full mix. **Split**: the Places
  with their own outputs leave the main (their reverb stays there).
- [ ] Bounce an aux (or record it): is it the Place alone?

## Milestone 13 check: the 8-bit interface and the tiny planet

Nothing in the sound changed; this is about the look and that every control still works.

- [ ] GarageBand loads it and the interface appears in the pixel style (no blurry or
  half pixels on your screen).
- [ ] The mood follows the Places: add Castle and Tavern (whimsical, berry and gold),
  then swap to Spaceport and Megacity (neon sci-fi), then Castle with Spaceport (the
  "magitech" blend), then City and Mall (neutral graphite). Does it change smoothly?
- [ ] Every control works as before: drag knobs and sliders (Shift for fine), double-click
  to reset, menus, M and S, EDIT, X, the Pattern Editor and ARRANGE, tooltips.
- [ ] Readable at 100 %: the labels, values and knob names. Anything too small?
- [ ] UI SIZE 150 % and 200 %: the window grows and stays sharp. (Untested on a Mac:
  if the window does not resize in GarageBand, say so.)
- [ ] OVERLAYS Full: sparkles, stars, scanlines and brackets on hovered controls, icons
  animate (flag, tavern sign, thruster). Subtle: only the hovered card moves. Off: still.
- [ ] The tiny planet: switch PLANET through all seven. Horizon and sky right for each?
  Do the Places stand on the horizon and turn with it (ships and stations in orbit, the
  Nebula in the sky)? Weather (rain, snow, lightning), day and night, an aurora with
  Magic. Drag a Place sideways (pan) and up and down (distance).
- [ ] Locations: save two and travel: a comet crosses the sky between their stars/moons.
- [ ] CPU: about the same or lower than Milestone 12 with the interface open?
