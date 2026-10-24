# WORLDBUILDER

A procedural soundscape instrument for Logic Pro and GarageBand on macOS (Apple
Silicon and Intel, macOS 10.15+). Describe a world with a handful of high-level
controls (temperature, weather, machinery, magic, scale and more) and WORLDBUILDER
synthesises it in real time. It uses no samples and no recordings.

**Status: Milestone 2 (climate layers).** These work now:
- six of the ten layers: Air (wind), Water (rain, drips, streams, waves), Fire &
  Heat (crackle, roar, lava), Ice & Earth (creaks, singing ice, rumble,
  rockfalls), Thunder and the Tonal Bed;
- Temperature, Weather, Wetness, Scale, Era, Magic, Danger and Activity shape them;
- the shared World Space reverb;
- Power/Play, Master Level, Width, Seed, Quality, Root and Mood;
- a placeholder interface whose globe shows the climate (rain, snow, lava, lightning).

The remaining layers, Structures, Locations and the full interface arrive in later
milestones. See [docs/DEVLOG.md](docs/DEVLOG.md) for progress and known limits, and
[docs/PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md) for the plan.

## Install (macOS)

1. Download `WORLDBUILDER-macOS-Universal` from the latest green
   [CI run](../../actions) and unzip it.
2. In Terminal: `bash install-mac.sh` from the unzipped folder. It removes the
   download quarantine, re-signs the plug-in for your Mac, refreshes the Audio Unit
   list and runs Apple's validator. See `INSTALL.txt` inside the package for why
   this is needed.
3. GarageBand: on a Software Instrument track, open Smart Controls (B) → Plug-ins →
   Instrument slot → AU Instruments → Veil Audio → WORLDBUILDER. It plays
   straight away; no MIDI is needed.

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

`WorldbuilderTests --snapshot editor.png` writes an image of the interface;
`WorldbuilderTests --probe` prints per-layer measurements for each World Control;
`WorldbuilderTests --render storm.wav 30 weather=1 wetness=0.8` renders any world
(parameter IDs from `Source/Parameters.h`) to a WAV file.

CI (`.github/workflows/build.yml`) runs on every push:
- universal macOS build and the tests on both architectures;
- bundle and Info.plist checks, then signing and packaging;
- install of a quarantined copy with `install-mac.sh`;
- `auval` and the AU host harness (`tools/au-harness`) on arm64 and x86_64;
- the Linux build and tests.

The installable zip is uploaded only when every check passes.
