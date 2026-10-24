# WORLDBUILDER — Direction Update (after Milestone 2)

This file overrides WORLDBUILDER_PROMPT.md wherever they conflict. Read it after the brief and DEVLOG.

## 1. The new core idea: musical, not literal

WORLDBUILDER is a **film-scoring instrument**, not a field-recording simulator. It should not try to sound exactly like a castle, a factory or rain. It should produce **musical sound that evokes the vibe** of a place, the way a film composer would score it.

- Every sound should be **tonal or rhythmic where possible**, tuned to the plugin's Root and Mood, and sit naturally under a score.
- Realism is a seasoning, not the goal. A factory is a pulsing metallic ostinato with anvil-like tuned hits and steam-like swells, not a recording of machines. A castle is low brass-like drones, distant tuned bells, a slow processional pulse and stone-hall space, not horses and banners.
- Environmental layers (Air, Water, Fire, Ice & Earth, Thunder) stay, but are retuned in the same spirit: rain becomes pitched, randomised plucks and shimmering textures in key; wind carries harmonic whistles in the current scale; thunder lands as a tuned low impact plus a swell.
- **Tempo sync:** rhythmic parts follow the host tempo (with a free option), so a scene can sit under a cue.
- **Key sync:** everything tonal follows Root and Mood.
- Keep levels mix-ready: nothing should overpower dialogue-level material at default; each place should work as a bed under a score.

## 2. PLACES — the new centrepiece

Replace the "Structures" concept and the Machinery / Life / Population layers with **Places**. A Place is a self-contained, deep musical sound engine representing one setting. This becomes the main thing the user interacts with.

### Place selector (UI)

- A clear **Place selector** next to the globe: a row or grid of illustrated cards (original vector art) for each Place. Click to add it to the world; the globe shows it as a landmark.
- Up to **4 Places active at once**, each with its own strip: level, distance (near → far), pan, mute/solo, and an **Edit** button that opens that Place's **detail panel** with its own controls (below).
- Places can still be placed on the spinning globe for panning/distance, as in the original Structures design, but the selector and detail panels are the primary way to work.

### Places to build (each a distinct musical engine)

| Place | Musical character (vibe, not realism) |
|---|---|
| **Castle** | slow processional pulse, low brass-like drones, distant tuned bells, horn-like calls, stone-hall reverb |
| **Village** | warm plucked ostinatos, folk-like drones, gentle hand-percussion pulse, wooden tones, tape warmth |
| **City** | layered rhythmic motion, muted bell-like tones, bustling granular textures in key, distant hum |
| **Factory / Forge** | metallic tuned anvil hits, pulsing ostinato, steam-like noise swells, gritty saturation |
| **Spaceport** | arpeggiated synth pulses, wide pads, sweeping thruster-like risers, PA-like tones, ping-pong delay |
| **Temple** | choir-like formant pads (no words), gongs, singing-bowl tones, long shimmer reverb |
| **Forest** | breathy pads, bird-like melodic chirps in scale, soft wood percussion, rustling textures |
| **Ruins** | decayed, reversed and lo-fi fragments, sparse haunted tones, cold wind harmonics |
| **Cavern / Mine** | resonant drips as tuned plucks, deep sub drones, echoing metallic taps |
| **Alien Hive / Biome** | organic modulated textures, microtonal-sounding (but still key-anchored) calls, pulsing |

Build them in priority order: Castle, Village, Spaceport, Factory, City first; the rest in a later milestone.

### Detail panel: controls every Place has

- **Character** (a few named variations per Place, e.g. Castle: *Fortress, Royal Court, Besieged*; Spaceport: *Busy Hub, Abandoned Dock, Launch*). These must change the DSP and patterns, not just levels.
- **Activity** (sparse → busy), **Tension** (calm → menacing: harmony darkens, rhythm tightens), **Rhythm** (free / synced; density; swing), **Tone** (dark → bright), **Distance**, **Size of space**, **Variation** (how much it evolves and repeats), and a **Seed**.
- 3–5 **element toggles/levels** specific to that Place (e.g. Castle: Drones, Bells, Horns, Pulse, Hall; Spaceport: Pulses, Pads, Risers, Announcements, Engines), so users can strip a Place down or emphasise parts.
- Each Place has its own effect character (tape delay, shimmer, saturation, ping-pong, etc.). **Population/Activity drives that effect's depth**, as in the original Structures design.

The global World Controls (Temperature, Weather, Era, Danger, Scale, etc.) still apply to all Places, shaping them together.

## 3. Immediate fixes from Milestone 2 testing

- **Water drips sound too fake.** Rework them in the new musical spirit: tuned, softly resonant drops in the current key, with natural randomised timing, velocity, decay and stereo position, plus some room. They should sound beautiful rather than synthetic.
- Do a quick retune of the other Milestone 2 layers toward the musical approach where it is cheap to do so.

## 4. Revised milestones (replacing 3–7 in the brief)

3. **Musical retune + Place engine + first Places:** fix the drips; retune the environment layers to key and tempo; build the shared Place framework (strips, detail panel, element toggles, Character, Activity, Tension, Rhythm, Seed); implement **Castle, Village and Spaceport**; add the Place selector UI. Machinery / Life / Population layers are removed or folded into Places.
4. **More Places:** Factory, City, Temple, Forest, Ruins, Cavern, Alien Hive; MIDI events per Place (notes trigger Place-specific hits and phrases in key).
5. **Locations & Journeys** (as in the brief, now including active Places and their settings).
6. **Interface polish:** globe landmarks for Places, animations, detail-panel art.
7. **Presets & release:** 15+ factory worlds built as film-scoring scenes, each combining 2–4 Places with 2–4 Locations to travel between.

Same working rules as before: after pushing, stop and don't wait for CI; short logs; max 2 fix attempts; update the DEVLOG handoff.
