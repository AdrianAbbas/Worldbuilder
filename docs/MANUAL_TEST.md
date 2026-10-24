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
