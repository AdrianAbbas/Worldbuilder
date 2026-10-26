#pragma once
// The 8-bit control style (Milestone 13, Direction Update 7): pixel-art controls and
// Place icons in three moods. Fantasy is whimsical (wood, gold trim, sparkles), sci-fi
// is futuristic (dark metal, neon, scanlines, HUD brackets), and the blend between them
// is "magitech" (wooden frames with neon inlays, runes that glow like circuitry). A mood
// is one number, 0 (fantasy) to 1 (sci-fi), 0.5 the blend; values between move
// smoothly from one look to the next.
//
// Everything is drawn into a Canvas at source resolution (one art pixel per image
// pixel, no smoothing) and shown at an integer scale, so no art pixel is ever split.
// 13a, the style prototype: one knob, slider, button, toggle, tab strip and dropdown,
// and six Place icons. 13b applies the approved style to the whole editor.
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <map>

namespace wb::ui::pixel {

// ---- Moods -------------------------------------------------------------------------

inline constexpr float fantasyMood = 0.0f, blendMood = 0.5f, sciFiMood = 1.0f;

// The mood of a mix of Places, weighted like the world theme (`presence` per
// wb::PlaceType, 0..1): sci-fi Places pull towards 1, fantasy towards 0, modern Places
// sit in the middle and nature a little towards fantasy. With none present, the blend.
float moodOf(const float* presence, int count);

// The materials of a mood: the colours every control is drawn with, and how much of
// each mood's decoration shows (sparkles, scanlines, runes; 0..1, they cross-fade).
struct Palette {
    juce::Colour outline;                     // the darkest line around everything
    juce::Colour rim, rimLight, rimDark;      // frames and bezels (gold / metal / wood)
    juce::Colour inlay;                       // the thin line inside a frame (gold / neon)
    juce::Colour face, faceLight, faceDark;   // the body of buttons, knobs, tabs
    juce::Colour well, wellLight;             // recesses: slider tracks, fields
    juce::Colour accent, accentDim, glow;     // lit values, LEDs (on / off), highlights
    juce::Colour ink, inkDim, inkOn;          // text on a face; dimmed; on a lit (accent) face
    juce::Colour panel, panelLight, panelDark;   // the background the controls sit on
    float sparkle = 0, scanline = 0, runes = 0;
};
Palette palette(float mood);

// ---- Canvas ------------------------------------------------------------------------

// A sprite being drawn at source resolution. Positions are whole art pixels.
class Canvas {
public:
    Canvas() = default;
    Canvas(int width, int height);
    int width() const { return w; }
    int height() const { return h; }
    bool isValid() const { return image.isValid(); }
    const juce::Image& source() const { return image; }

    juce::Colour get(int x, int y) const;
    void set(int x, int y, juce::Colour c);           // replaces the pixel
    void blend(int x, int y, juce::Colour c);         // composites over it (decorations)
    void fill(int x, int y, int width, int height, juce::Colour c);
    void hline(int x0, int x1, int y, juce::Colour c);
    void vline(int x, int y0, int y1, juce::Colour c);
    void line(int x0, int y0, int x1, int y1, juce::Colour c);   // Bresenham
    // A box with its corner pixels cut (the pixel-art rounded rectangle), outlined.
    void box(int x, int y, int width, int height, juce::Colour fillColour, juce::Colour outlineColour);
    void outlineBox(int x, int y, int width, int height, juce::Colour outlineColour);   // the outline alone
    void paste(const Canvas& other, int x, int y);    // composites another sprite
    void paste(const juce::Image& image, int x, int y);

    // The pixel font: 5 x 7 capitals, digits and signs on a 6-pixel advance.
    static int textWidth(const juce::String& text);
    static constexpr int textHeight = 7;
    void text(int x, int y, const juce::String& text, juce::Colour c);
    // Text with a one-pixel drop shadow (down-right), the 8-bit way of keeping it legible.
    void shadowText(int x, int y, const juce::String& text, juce::Colour c, juce::Colour shadow);
    void centredText(int y, const juce::String& text, juce::Colour c, juce::Colour shadow);

    // The sprite at `scale` (an integer), nearest neighbour: every art pixel becomes a
    // scale x scale block of one colour.
    juce::Image scaled(int scale) const;

private:
    juce::Image image;
    int w = 0, h = 0;
};

// Draws a canvas at an integer scale with its top-left corner snapped to whole pixels.
void draw(juce::Graphics& g, const Canvas& canvas, juce::Point<float> topLeft, int scale);

// A small cache of scaled sprites by key (a control's look for one mood, value and
// state). Pixel sprites are cheap to make but cheaper to reuse.
class SpriteCache {
public:
    explicit SpriteCache(int capacity = 256) : limit(capacity) {}
    const juce::Image& get(const juce::String& key, const std::function<juce::Image()>& make);
    void clear() { sprites.clear(); }
    int size() const { return int(sprites.size()); }
    int made = 0;   // how many sprites were drawn (not reused)

private:
    std::map<juce::String, juce::Image> sprites;
    int limit;
};

// ---- Controls ----------------------------------------------------------------------

struct State {
    bool hover = false, down = false, on = false, enabled = true;
};

// Every control sprite has a 3-pixel margin for its decorations (sparkles, brackets,
// glow) and a drop shadow; `margin` is where the control itself starts.
inline constexpr int margin = 3;

// A pixel dial: a segmented value ring (gold gems / LEDs / glowing runes), a bevelled
// body and a chunky pointer; label above, value below. `frame` animates the decoration.
Canvas knob(const Palette& p, float value, const juce::String& label, const juce::String& valueText, const State& s, int frame = 0);
inline constexpr int knobDial = 34;   // the dial's size in art pixels
// A horizontal slider `width` art pixels wide: a notched track and a sprite handle.
Canvas slider(const Palette& p, int width, float value, const juce::String& label, const juce::String& valueText, const State& s, int frame = 0);
// A chunky bevelled button (pressed: it sinks one pixel); `width` 0 fits the text.
Canvas button(const Palette& p, const juce::String& text, const State& s, int width = 0, int frame = 0);
// A switch with an LED, its label to the right.
Canvas toggle(const Palette& p, const juce::String& label, const State& s, int frame = 0);
// A strip of tabs over a panel edge; the active tab joins the panel.
Canvas tabs(const Palette& p, const juce::StringArray& names, int active, int hovered = -1, int frame = 0);
// A dropdown field with its arrow; with `items`, the open menu under it.
Canvas dropdown(const Palette& p, const juce::String& text, int width, const State& s, const juce::StringArray* items = nullptr, int highlighted = -1,
                int frame = 0);
// A Place card: the icon in a framed window, its name on a plaque below.
Canvas placeCard(const Palette& p, int placeType, const juce::String& name, const State& s, int frame = 0);
// A panel background `width` x `height` in the mood's material.
Canvas panel(const Palette& p, int width, int height, int frame = 0);

// The tab strip's hit areas, for a LookAndFeel or a test (art pixels, in the sprite).
juce::Array<juce::Rectangle<int>> tabAreas(const juce::StringArray& names);

// ---- Place icons -------------------------------------------------------------------

inline constexpr int iconSize = 32;
// True when a Place has a pixel icon (13a: Castle, Tavern, Forest, Spaceport,
// Cyberpunk Megacity, Nebula; 13b: all of them).
bool hasPlaceIcon(int placeType);
// How many idle-animation frames its icon has (2-4).
int placeIconFrames(int placeType);
// The icon's sprite (32 x 32, transparent background) for an animation frame, drawn
// with the mood's outline colour. A bundled `assets/pixel/place-<key>.png` (frames
// side by side, each 32 x 32) replaces the code-drawn art.
Canvas placeIcon(int placeType, int frame, juce::Colour outline);

// A bundled replacement sprite (`assets/pixel/<name>.png`), or an invalid image.
juce::Image bundledSprite(const juce::String& name);

// ---- The prototype sheet (13a) -----------------------------------------------------

// Every prototype element in one mood, on that mood's panel, at source resolution:
// the knob, the slider, the button (normal, hover, pressed), the toggle (off, on),
// the tabs, the dropdown (closed and open) and the six Place cards.
Canvas prototypeSheet(float mood, int frame = 0);
// The three moods side by side, titled.
Canvas prototypeComparison(int frame = 0);
// One button, knob and card at mood 0, 0.25, 0.5, 0.75 and 1: the smooth change.
Canvas moodSweep(int frame = 0);
// The six icons, every animation frame, on each mood's card backing.
Canvas iconStrips();

// WCAG contrast ratio between two colours (for the readability tests).
float contrast(juce::Colour a, juce::Colour b);

} // namespace wb::ui::pixel
