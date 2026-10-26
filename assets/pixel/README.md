# Pixel sprites (optional)

The 8-bit interface (Milestone 13) draws its sprites in code: palette-indexed pixel
maps in `Source/UI/PixelArt.cpp` and the control sprites in `Source/UI/Pixel.cpp`.
Any of them can be replaced by hand-drawn pixel art: put a PNG with the sprite's name
in this folder and rebuild. CMake bundles every `*.png` here (BinaryData) and the
plugin uses it instead of the code-drawn sprite.

| File | What it replaces |
|---|---|
| `place-<key>.png` | a Place icon (cards, strips, the tiny planet, the detail panel). Frames side by side, each square (32 x 32 is the native size): a 4-frame idle animation is 128 x 32. `<key>` is the Place's key (`empty`, `castle`, `village`, `spaceport`, ...; the full list is `placeKeys` in `Source/Places/Place.h`). |
| `icon-<name>.png` | a tool icon on buttons (`power`, `dice`, `edit`, `close`, `patterns`, `save`, `load`, `render`, `midi`, `dragout`, `path`, `journey`, `undo`, `redo`, `copy`, `paste`, `clear`, `generate`, `up`, `down`, `lock`, `draw`, `select`, `back`, `snap`, `note`, `sync`, `import`, `double`, `half`, `chords`, `tone12`). White on transparent, any square size (it is scaled to the button by whole pixels and tinted). |
| `instrument-<name>.png` | an instrument icon (`drone`, `bell`, `horn`, `timpani`, `harp`, `choir`, `lute`, `marimba`, `flute`, `jingle`, `pulse`, `pad`, `riser`, `pa`, `engine`, `blip`, `handdrum`, `anvil`, `piston`, `steam`, `gear`, `piano`, `crowd`, `siren`, `bird`, `wave`, `gong`, `bowl`, `drip`, `crystal`, `wind`, `musicbox`, `alarm`, `strings`, `glass`, `shaker`, `snare`, `radio`, `bass`, `swarm`). White on transparent. |

Rules for replacements:
- draw at 1 art pixel per image pixel, with a transparent background; the plugin
  scales it by whole numbers (no smoothing);
- original art only (nothing taken from existing games);
- keep each file small; together they count toward the installed plugin's size.

The controls themselves (knobs, sliders, buttons, panels) are drawn from the mood's
palette at any size, so they are changed in `Source/UI/PixelControls.cpp`, not here.
