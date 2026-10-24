# WORLDBUILDER — Direction Update 3 (Milestone 5: Places expansion + visual update)

Read after WORLDBUILDER_PROMPT.md, DIRECTION_UPDATE.md and DIRECTION_UPDATE_2.md. This file overrides them where they conflict.

Milestone 4 (Pattern Editor) is tested and works great in GarageBand.

## 0. Scope and order

- **Milestone 5 = 20 Places, each with at least 5 instruments, plus a visual update.** MIDI mode and keyboard zones (Section 1B of Update 2) move to **Milestone 6**. Locations & Journeys, the full visual overhaul and presets shift down accordingly.
- This is too big for one session. Work in **sub-milestones**, pushing and updating the DEVLOG handoff after each one:
  - **5a** Shared upgrades: instrument framework for 5+ instruments per Place, the Place card/selector UI for 20 Places (scrollable or categorised), the visual update (Section 3), and upgrading **Castle, Village, Spaceport** to the new standard.
  - **5b** Places 4–9.
  - **5c** Places 10–14.
  - **5d** Places 15–20.
- Stop after each sub-milestone. Don't wait for CI.

## 1. Instrument standard (applies to every Place)

- **Minimum 5 instruments per Place**, ideally 6. Each instrument is a distinct voice with its own role: e.g. drone/bed, bass, rhythm, melody/lead, accent/hits, texture.
- Every instrument is an **element** in the Pattern Editor (its own tab), with its own pattern, randomness settings, level, mute and on/off toggle.
- Instruments follow the global **Root/Mood**, host **tempo** and **Transport**. Each Place may suggest a default Mood (e.g. Desert → Phrygian dominant), but never overrides the user's choice.
- **Musical, not literal** (from Update 1): instruments evoke the place through tone, rhythm and harmony. Realistic noise is a seasoning only.
- Each Place has **2–4 Characters** (variations that change patterns, voicings and DSP, not just levels) and its own **effect character**.
- Synthesis only, no samples. Reuse and extend shared synthesis building blocks (plucked string / Karplus-Strong, modal mallets and bells, FM, additive pads, formant choir, noise shapers, drum synthesis) so 20 Places stay maintainable and CPU-efficient.
- Keep CPU in budget: 4 active Places × 6 instruments must stay under the brief's CPU target in Standard quality. Use voice limits and the Quality setting.

## 2. The 20 Places

Existing three, upgraded to 5+ instruments (5a):

| # | Place | Instruments (minimum 5) | Characters | Effect character |
|---|---|---|---|---|
| 1 | **Castle** | brass-like drone, tuned bells, horn calls, processional timpani pulse, harp-like arpeggios, choir pad | Fortress, Royal Court, Besieged | stone-hall reverb, slow tape echo |
| 2 | **Village** | lute-like plucks, hand drums, wooden marimba-like tones, hurdy-gurdy-like drone, recorder-like flute, small bell jingles | Market Day, Harvest Night, Quiet Dawn | warm tape saturation, wow & flutter |
| 3 | **Spaceport** | arpeggiated pulses, wide pads, thruster risers, PA tones, engine sub drone, data blips | Busy Hub, Abandoned Dock, Launch | wide ping-pong delay, sci-fi shimmer |

New Places:

| # | Place | Instruments (minimum 5) | Characters | Effect character |
|---|---|---|---|---|
| 4 | **Factory / Forge** | tuned anvil hits, piston ostinato, steam swells, bowed-metal drone, gear-click rhythm, furnace bass | Steam Mill, Iron Forge, Assembly Line | gritty saturation, metallic comb resonance |
| 5 | **City** | muted bell chimes, pulsing synth bass, electric-piano chords, crowd texture in key, tuned siren glides, traffic-hum drone | Rush Hour, Night City, Rain-soaked | dense short reverb, subtle chorus |
| 6 | **Forest** | breathy flute, melodic bird calls, wood percussion, kalimba-like plucks, string pad, rustle texture | Enchanted, Dark Woods, Morning Glade | soft diffusion, damped space |
| 7 | **Alien Hive** | modulated organic pulses, glassy FM calls, chittering rhythm, throbbing bass, formant drones, swarm texture | Dormant, Awakening, Swarm | ring-mod shimmer, warped delay |
| 8 | **Temple** | formant choir pad, gongs, singing bowls, low chant drone, temple bells, zither-like plucks | Sacred Dawn, Ancient Rite, Forsaken | long shimmer reverb |
| 9 | **Cavern / Mine** | tuned drips, sub drone, echoing metal taps, bowed low bass, crystal resonances, distant rumble pulse | Crystal Cave, Deep Mine, Flooded | long dark reverb, slapback echo |
| 10 | **Ruins** | reversed piano-like fragments, lo-fi music box, broken bells, cold wind harmonics, ghostly pad, sparse low toms | Overgrown, Haunted, Desolate | lo-fi decay, reverse echoes |
| 11 | **Spaceship Interior** | engine drone, life-support pulse, computer tone arps, airlock swells, hull resonances, soft alarm motif | Cruising, Red Alert, Derelict | confined metallic room, filtered delay |
| 12 | **Biodome** | lush pads, bright mallet arpeggios, synthetic birdsong, water-like plucks, ventilation hum, gentle shaker rhythm | Thriving, Night Cycle, Failing | bright airy reverb, gentle chorus |
| 13 | **Sea Ship** | creaking wood percussion, concertina-like chords, rope-pull swells, ship bell, wave drone, gull-like calls | Calm Seas, Storm, Ghost Ship | rolling modulation, wide wet reverb |
| 14 | **Suburbs** | warm electric-piano chords, soft pop pulse, wind chimes, sprinkler-like hi-hat rhythm, music-box motif, mellow bass | Sunny Afternoon, Evening, Uncanny | warm tape, short room |
| 15 | **Mall** | smooth electric piano, vibraphone melodies, PA chime dings, escalator-hum drone, fountain shimmer, soft drum groove | Busy Weekend, Empty Night, Dead Mall | glossy reverb, slight detune (vaporwave-style option in Dead Mall) |
| 16 | **Military Camp** | snare march patterns, bugle-like calls, low drum pulse, tuned radio blips, metallic clanks, tense string ostinato | Morning Drill, On Alert, Night Watch | tight room, gritty radio filter |
| 17 | **Beach** | warm guitar-like plucks, steel-drum-like tones, wave swells, shaker, breezy pad, gull calls | Tropical Day, Sunset, Stormy Shore | open airy space, gentle chorus |
| 18 | **Desert** | oud-like plucks, frame drums, ney-like flute, tanpura-like drone, wind whistles, sparse metal chimes | Oasis, Endless Dunes, Sandstorm | wide dry space, long sparse echo |
| 19 | **Palace** | harpsichord-like arpeggios, string-ensemble pads, fanfare brass, glockenspiel, courtly timpani, choir swells | Grand Ball, Throne Room, Fallen Palace | grand hall reverb |
| 20 | **Tavern** | lute-like strumming, frame-drum pulse, fiddle-like drone and melodies, tuned glass clinks, squeezebox chords, stomping rhythm | Lively Night, Quiet Corner, Brawl | warm wooden room, tape warmth |

For culturally specific sounds (Desert, Beach, etc.), aim for evocative film-score colour rather than caricature.

## 3. Visual update (5a, refined as Places are added)

- **Place selector for 20 Places:** a scrollable grid or categorised tabs (e.g. Medieval & Fantasy / Modern / Sci-Fi / Nature). Cards keep the current style but with richer illustrations: lighting, depth, small animations on hover.
- **Theme follows the world:** the background, panel tint, glow colour and particle effects shift with the active Places (e.g. warm amber for Tavern and Village, cold cyan for Spaceship and Spaceport, sandy gold for Desert, deep green for Forest, smoky red for Military Camp). With several Places active, blend their palettes. Transitions are smooth (~1 s).
- **Living Globe:** each Place appears as its own illustrated landmark; the globe's surface and atmosphere respond to the active Places (ocean for Sea Ship and Beach, dunes for Desert, city lights for City and Suburbs).
- **Detail panel:** per-Place header art and colour; the Pattern Editor's element tabs show an icon for each instrument.
- Keep the artwork asset pipeline from Update 2 (drop-in PNG layers per Place, procedural fallback). Keep animation ≤ 30 fps and CPU-light.

## 4. Tests (lean)

For each Place: every instrument audible on solo, finite output, follows Root/Mood (pitch-class check), follows tempo, every Character measurably different, CPU within budget with 4 Places active. State round trip with 4 active Places and edited patterns.

## 5. Revised milestones

5. Places expansion + visual update (5a–5d above).
6. MIDI control: MIDI Play Mode and keyboard zones (Update 2 Section 1B), with every Place playable.
7. Locations & Journeys.
8. Full visual overhaul / polish.
9. Presets & release.

Same working rules: push to main, stop after pushing, don't wait for CI, short logs, max 2 fix attempts, update the DEVLOG handoff.
