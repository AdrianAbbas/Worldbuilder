# WORLDBUILDER

A film-scoring soundscape instrument for Logic Pro and GarageBand on macOS (Apple
Silicon and Intel, macOS 10.15+). It scores a place rather than recording it:
musical, tonal and rhythmic sound in your song's key and tempo that evokes a castle,
a village or a spaceport, over a living environment of wind, water, fire, ice and
thunder. Everything is synthesised in real time. It uses no samples and no
recordings.

**Status: Milestone 3 (musical retune and the first Places).** These work now:
- **Places**, the centrepiece. Up to four play at once: Castle, Village and Spaceport.
  - Each has three Characters that change its patterns, harmony and sound.
  - Each has five elements you can switch off or turn down, and its own effect
    character (stone hall and tape echo; tape warmth; ping-pong delay and hangar).
  - Controls: Activity, Tension, Tone, Size, Variation, Density, Swing, Sync (to the
    host tempo, or the Place's own free tempo) and a Seed.
  - Each slot also has level, distance and pan.
- **Key and tempo.** Everything tonal follows Root and Mood, and all Places share one
  slow chord progression. Rhythmic parts follow the host tempo.
- **The environment**, retuned toward music. Air whistles on scale notes. Water drips
  ring as tuned drops, and rain carries pitched plucks and an in-key shimmer. Fire and
  ice sing in the key. Thunder lands as a tuned impact with a swell. The Tonal Bed
  holds it together.
- **World Controls** shape everything. Population sets how busy the Places are,
  Machinery makes them straighter and more metallic, Life more human, and Danger
  darker.
- the shared World Space reverb; Power, Master Level, Width, Seed and Quality;
- **The interface**: Place cards to add Places, Place strips, a detail panel for each
  Place, and landmarks on a placeholder globe that you can drag to move a Place.

The other seven Places and MIDI events come in Milestone 4, Locations and Journeys in
Milestone 5, and the full interface in Milestone 6. See
[docs/DEVLOG.md](docs/DEVLOG.md) for progress and known limits. The plan is in
[docs/PROJECT_BRIEF.md](docs/PROJECT_BRIEF.md) and
[docs/DIRECTION_UPDATE.md](docs/DIRECTION_UPDATE.md); the update overrides the brief.

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

`WorldbuilderTests --snapshot editor.png` writes an image of the interface (and of a
Place's detail panel); `WorldbuilderTests --probe` prints per-layer measurements for
each World Control; `WorldbuilderTests --probe-places` prints each Place's level per
character and element;
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
