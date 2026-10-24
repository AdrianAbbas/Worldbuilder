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
