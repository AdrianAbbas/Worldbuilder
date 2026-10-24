# Place artwork (optional)

Drop finished artwork for a Place into its own folder here and rebuild; it is bundled
into the plug-in and used on the Place's card, its detail-panel header and its globe
landmark. Anything missing falls back to the built-in procedural art.

    assets/places/<key>/back.png     sky and far background (moves least)
    assets/places/<key>/mid.png      middle ground
    assets/places/<key>/front.png    the landmark itself (also the globe landmark)
    assets/places/<key>/lights.png   windows, fires, lamps (flickers)

Keys: castle, village, spaceport, factory, city, forest, hive, temple, cavern, ruins,
spaceship, biodome, seaship, suburbs, mall, military, beach, desert, palace, tavern.

Layers are drawn scaled to fill the card (about 3:2, e.g. 300 x 200 px), so use
transparent PNGs for mid, front and lights. Keep the whole folder small (the
installed plug-in should stay under about 60 MB).
