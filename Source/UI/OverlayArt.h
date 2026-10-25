#pragma once
// The art of the Place overlays from Milestone 5b on, and the drawing helpers every
// overlay shares. Each overlay is still shapes (drawn once per layout into a cached
// image by PlaceOverlays) and a few dozen small sprites, drawn once here, that move
// (pulse, blink, flicker, spin, rise, fall or drift; see PlaceOverlays::Mover).
#include "PlaceOverlays.h"

namespace wb::ui::art {

using Point = juce::Point<float>;
using Shape = PlaceOverlays::Shape;
using Layout = PlaceOverlays::Layout;
using Mover = PlaceOverlays::Mover;

// What a Place's overlay is made of.
struct OverlayArt {
    std::vector<Shape> shapes;          // still, drawn once
    std::vector<juce::Image> sprites;   // drawn once, stamped unscaled where they move
    std::vector<Mover> movers;
    bool light = false;                 // glowing art (neon) is drawn before the rest
};

// True for the Places whose overlay is built here.
bool hasArt(int type);
// Builds a Place's overlay for a layout (PlaceOverlays clips it to where it may draw).
OverlayArt build(int type, const Layout& layout);

// --- Shared helpers.
float between(juce::Random& r, float a, float b);
// A curved blade of grass rising from its base.
void blade(juce::Path& p, float x, float base, float height, float lean, float width);
// A pointed leaf from its stalk, pointing along `angle`.
void leaf(juce::Path& p, Point at, float size, float angle);
// Ivy growing from a to b along a frame: a wavy stem, leaves on alternate sides that
// get smaller towards the tip.
void vine(juce::Path& stem, juce::Path& leavesA, juce::Path& leavesB, Point a, Point b, juce::Random& r, float leafSize = 1.0f);
// Evenly spaced points around a rounded frame (lights run along them).
std::vector<Point> trackAround(juce::Rectangle<float> frame, float corner);
// Random places in an area, chosen by size.
std::vector<Point> spotsIn(const juce::RectangleList<int>& area, int count, int seed);
void glowDot(juce::Graphics& g, Point p, float radius, juce::Colour glow, juce::Colour core, float coreRadius);
// A glow drawn once, to be stamped unscaled.
juce::Image glowSprite(float radius, juce::Colour glow, juce::Colour core, float coreRadius);
// A filled path drawn once into its own small image (centred on the path's centre), with
// an optional soft glow around it.
juce::Image pathSprite(const juce::Path& path, juce::Colour colour, float glow = 0, juce::Colour glowColour = {});

} // namespace wb::ui::art
