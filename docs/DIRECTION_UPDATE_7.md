# WORLDBUILDER — Direction Update 7 (Milestone 13: 8-bit interface overhaul)

Read after docs/PROJECT_BRIEF.md and Direction Updates 1–6. This file overrides them where they conflict and adds **Milestone 13**, the final milestone (after Milestone 12).

## 0. The idea

The developer likes the **planet** and the **overlays**, but finds the **buttons, sliders, knobs and especially the Place/scene icons** ugly. Pivot the whole control layer to a **pixel-art / 8-bit style** that fits both fantasy and space:

- **Fantasy worlds → whimsical:** warm, playful, storybook pixel art (wood, parchment, gold trim, little sparkles, bouncy hover animations), like a classic fantasy RPG.
- **Sci-fi worlds → futuristic:** crisp neon pixel art (dark metal panels, cyan/magenta glow, scanlines, blinking LEDs, HUD brackets), like a classic space game.
- **Both active → a blend:** "magitech" — e.g. wooden frames with neon inlays, runes that glow like circuitry. The blend follows the mix of active Places (same weighting as the world theme), changing smoothly.

The planet and overlays stay as they are (optionally offer a "pixelate planet & overlays" toggle, default off).

## 1. Work in two stages — prototype first

**13a. Style prototype (stop for approval).** Build the pixel style for a small set of elements: one knob, one slider, one button, one toggle, one tab, one dropdown, and **six Place icons** (Castle, Tavern, Forest, Spaceport, Cyberpunk Megacity, Nebula), in all three moods (fantasy, sci-fi, blend). Render snapshots to `docs/snapshots/pixel-prototype-*` and a side-by-side sheet. Push, update the handoff, and **stop** so the developer can approve or redirect the look.

**13b. Full overhaul (after approval).** Apply the approved style everywhere (Section 2).

## 2. Scope of the full overhaul

- **Place icons:** all 27 Places get new pixel-art icons (e.g. 32×32 or 48×48 source pixels, drawn at integer scale), each instantly recognisable, with a small idle animation (2–4 frames: a flag waving, tavern sign swinging, rocket thruster flicker, neon flicker). Fantasy Places drawn whimsical, sci-fi Places futuristic, modern Places in a neutral style that sits between.
- **Controls:** knobs (pixel dials with a chunky pointer and segmented value ring), sliders (pixel tracks with a sprite handle), buttons (chunky bevelled pixel borders with pressed state), toggles, tabs, dropdowns/menus, the Place cards, Location chips, the Arrange timeline sections and transitions, the Pattern Editor grid, notes and lanes, meters (segmented LED-style), the Score Intensity knob (larger, special).
- **Icons:** one consistent pixel icon set for every tool/button (draw, select, undo, render, export, etc.).
- **Typography:** a pixel font for headings and labels, **readable** at 100%; keep a clean, readable font (or a highly legible pixel font) for numbers and small text. Use only SIL Open Font License or CC0 fonts, embedded via BinaryData, licences included. Verify each font's licence.
- **Mood theming:** controls take their palette, frame material and decoration from the active mood (fantasy / sci-fi / blend), smoothly transitioning when Places change. Modern Places lean neutral.
- **Small delights (cheap):** hover bounce or glow, click "press" animations, tiny particle sparkles (fantasy) or scanline flicker (sci-fi) on key buttons — all subtle and optional (follow the Overlays Full/Subtle/Off setting).

## 3. Technical rules

- **Pixel-perfect rendering:** draw sprites at integer scale with nearest-neighbour scaling (no blurry smoothing); snap positions to the pixel grid. Offer UI scale 100% / 150% / 200% (integer-friendly) and keep the window aspect ratio.
- **How art is made:** pixel art can be authored as code (palette-indexed pixel maps in source) or as PNG sprite sheets in `assets/pixel/` loaded from BinaryData. Extend the existing asset pipeline so the developer can later replace any sprite with hand-drawn pixel art by dropping a PNG with the same name.
- **Performance:** pixel sprites must be cached; the overhaul should make the editor *cheaper* to draw, not more expensive. Measure before/after (report-only).
- **Usability first:** every control keeps its function, hit area, tooltip, double-click reset and automation linkage. Text and values stay readable; contrast stays good in all three moods.
- All original art. No sprites, fonts or icons copied from existing games.

## 4. Tests (lean)

- Every control still works (existing editor tests pass).
- Snapshot sheets for fantasy, sci-fi and blend moods; a check that sprites render at integer scale (no partial pixels).
- Editor draw time before/after (report-only).

## 5. Milestones

12. Pro features, presets & release (as in Update 6).
13. **8-bit interface overhaul**: 13a prototype → approval → 13b full overhaul.

Same working rules: push after each part with `[skip ci]`, the final push of a session without it; DEVLOG checklist; don't wait for CI. **Exception: 13a must stop for approval.**
