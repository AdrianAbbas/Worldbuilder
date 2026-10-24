# Manual test in GarageBand (Milestone 1 gate)

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
