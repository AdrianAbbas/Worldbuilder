# Pixel-art authoring scripts (Milestone 13)

The Place icons in `Source/UI/PixelArt.cpp` were composed with these scripts: pixel
primitives on a 32 x 32 grid of palette letters (`icons.py`), the icons
(`designs.py`: the 13a prototype's Tavern, Forest, Spaceport, Megacity and Nebula;
`designs2.py`: all 27, using those), previewed as PNGs and printed as the C++ maps
(frame 0 in full, later frames as "row column pixels" patches).

To change an icon: edit its function in `designs2.py`, run `python3 designs2.py` to
check `preview2.png`, then regenerate the maps the same way (see the commit that added
this folder). Or skip all this and drop a hand-drawn PNG in `assets/pixel/`
(see its README): it replaces the code-drawn icon.
