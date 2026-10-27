#pragma once
// The tactile style shared by every button, toggle, tab, chip, menu and card
// (Milestone 10): raised, bevelled surfaces with a soft gradient, a lit top edge and a
// shadow below; a glow on hover and when active; pressed surfaces sink. And one set of
// original vector icons, drawn as strokes in the same weight everywhere.
#include <juce_gui_basics/juce_gui_basics.h>

namespace wb::ui {

struct SurfaceState {
    bool hover = false, down = false, on = false, enabled = true;
};

// A raised control surface in `r` (corner radius `corner`). `base` is its body colour,
// `accent` the colour it lights in when on, glows in on hover and is outlined with.
void drawSurface(juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour base, juce::Colour accent, const SurfaceState& state);
// A recessed field (menus, value bars, slider grooves): darker, shaded from the top.
void drawWell(juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour edge, bool hover = false);
// A soft glow around a rounded rectangle.
void drawGlow(juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour colour, float size);
// The ink on a surface: dark on a lit (on) surface, parchment otherwise.
juce::Colour inkFor(const SurfaceState& state, juce::Colour accent);

enum class Icon {
    None, Power, Dice, Edit, Close, Patterns, Save, Load, Render, Midi, DragOut, Path, Journey, Undo, Redo, Copy, Paste, Clear,
    Generate, Up, Down, Lock, Draw, Select, Back, Snap, Note, Sync, Import, Double, Half, Chords, Tone12, Play, Stop, Loop, Record, Count
};
// One icon in `area` (kept square, centred), stroked in `colour`.
void drawIcon(juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour);
// A component's icon, as set with setIcon (Icon::None when none).
void setIcon(juce::Component& c, Icon icon);
Icon iconOf(const juce::Component& c);

} // namespace wb::ui
