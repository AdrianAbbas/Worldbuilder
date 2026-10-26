#pragma once
// The 8-bit interface (Milestone 13, Direction Update 7): pixel-art controls, icons and
// text in moods that follow the world. Fantasy is whimsical and storybook (berry and
// gold, mint curls, sparkles and little stars), sci-fi is futuristic (dark metal, neon,
// scanlines, HUD brackets), the blend between them is "magitech" (wooden frames with
// neon inlays, runes that glow like circuitry), and modern Places pull everything
// toward a clean, neutral graphite. Moods change smoothly as Places come and go.
//
// Everything is drawn into a Canvas at source resolution (one art pixel per image
// pixel, no smoothing) and shown with each art pixel as a whole block of screen pixels:
// `unit` logical pixels at 100 % (2), so 3 at 150 % and 4 at 200 %, and positions
// snapped to the art grid. No art pixel is ever split or smoothed.
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <unordered_map>

namespace wb::ui::pixel {

// Logical pixels per art pixel at 100 % UI size.
inline constexpr int unit = 2;

// ---- Moods -------------------------------------------------------------------------

inline constexpr float fantasyMood = 0.0f, blendMood = 0.5f, sciFiMood = 1.0f;

// Where the world sits: `sciFi` on the fantasy (0) - blend (0.5) - sci-fi (1) axis,
// and how `neutral` (0..1) modern Places make it.
struct Mood {
    float sciFi = blendMood, neutral = 0;
    bool operator==(const Mood& o) const { return sciFi == o.sciFi && neutral == o.neutral; }
};
inline constexpr Mood neutralMood { blendMood, 1.0f };

// The mood of a mix of Places (`presence` per wb::PlaceType, 0..1, weighted like the
// world theme): fantasy Places pull the axis to 0, sci-fi to 1, nature a little toward
// fantasy; modern Places add neutrality by their share. With none present, the blend.
Mood moodOf(const float* presence, int count);

// The materials of a mood: the colours every control is drawn with, and how much of
// each mood's decoration shows (0..1, cross-fading).
struct Palette {
    juce::Colour outline;                     // the darkest line around everything
    juce::Colour rim, rimLight, rimDark;      // frames and bezels (gold / metal / wood / aluminium)
    juce::Colour inlay;                       // the thin line inside a frame (mint / neon / white)
    juce::Colour face, faceLight, faceDark;   // the body of buttons, knobs, tabs
    juce::Colour well, wellLight;             // recesses: slider tracks, fields
    juce::Colour accent, accentDim, glow;     // lit values, LEDs (on / off), highlights
    juce::Colour ink, inkDim, inkOn;          // text on a face; dimmed; on a lit (accent) face
    juce::Colour panel, panelLight, panelDark;   // the background the controls sit on
    juce::Colour warn, peak;                  // meters: loud, too loud
    float sparkle = 0, scanline = 0, runes = 0;   // fantasy, sci-fi, blend decoration
};
Palette palette(float sciFi);
Palette palette(const Mood& mood);

// WCAG contrast ratio between two colours (for the readability tests).
float contrast(juce::Colour a, juce::Colour b);

// The look the interface is drawn in now (set by the editor every animation frame).
struct Theme {
    Mood mood { -1.0f, 0 };   // none yet
    Palette colours = palette(blendMood);
    int frame = 0;      // the decoration animation frame (twinkles, flicker), ~8 per second
    int delights = 2;   // 0 off, 1 subtle, 2 full: the Overlays setting
    int version = 0;    // names the mood and delights: part of every cached sprite's key
                        // (animated sprites add the frame to their own key)
};
const Theme& theme();
// Moods are kept to steps of 1/64 (smooth to the eye, a bounded number of sprites).
void setTheme(const Mood& mood, int frame, int delights);

// ---- Canvas and fonts --------------------------------------------------------------

// The two original pixel fonts (capitals, digits, signs; proportional): Large is 5 x 7
// for headings and button labels, Small is 3 x 5 for labels and numbers.
enum class Font { Small, Large };

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
    void blend(int x, int y, juce::Colour c);         // composites over it
    void fill(int x, int y, int width, int height, juce::Colour c);
    void blendFill(int x, int y, int width, int height, juce::Colour c);
    void hline(int x0, int x1, int y, juce::Colour c);
    void vline(int x, int y0, int y1, juce::Colour c);
    void line(int x0, int y0, int x1, int y1, juce::Colour c);   // Bresenham
    // A box with its corner pixels cut (the pixel-art rounded rectangle), outlined.
    void box(int x, int y, int width, int height, juce::Colour fillColour, juce::Colour outlineColour);
    void outlineBox(int x, int y, int width, int height, juce::Colour outlineColour);   // the outline alone
    void paste(const Canvas& other, int x, int y);    // composites another sprite
    void paste(const juce::Image& image, int x, int y);

    static int textWidth(const juce::String& text, Font font = Font::Large);
    static int textHeight(Font font = Font::Large) { return font == Font::Large ? 7 : 5; }
    void text(int x, int y, const juce::String& text, juce::Colour c, Font font = Font::Large);
    // Text with a one-pixel drop shadow (down-right), the 8-bit way of keeping it legible.
    void shadowText(int x, int y, const juce::String& text, juce::Colour c, juce::Colour shadow, Font font = Font::Large);
    void centredText(int y, const juce::String& text, juce::Colour c, juce::Colour shadow, Font font = Font::Large);

    // The sprite at `scale` (an integer), nearest neighbour: every art pixel becomes a
    // scale x scale block of one colour.
    juce::Image scaled(int scale) const;

private:
    juce::Image image;
    int w = 0, h = 0;
};

// `text` cut to fit `width` art pixels (ending in ".." when it is cut).
juce::String fitText(const juce::String& text, int width, Font font);

// ---- Drawing -----------------------------------------------------------------------

// The art-pixel block, in device pixels, at this Graphics context's scale (2 at 100 %
// on a standard display, 3 at 150 %, 4 on Retina...).
int blockFor(juce::Graphics& g);
// A logical coordinate snapped down to the art grid.
inline int snap(int v) { return v >= 0 ? v / unit * unit : -((-v + unit - 1) / unit * unit); }
inline juce::Rectangle<int> snap(juce::Rectangle<int> r) { return { snap(r.getX()), snap(r.getY()), snap(r.getWidth() + 1), snap(r.getHeight() + 1) }; }
// Moves and sizes `parent`'s children onto the art grid (whole art pixels), so their
// sprites land on whole device pixels at every UI size. Call at the end of resized().
void snapChildren(juce::Component& parent);
// Draws a canvas at `topLeft` (logical, snapped to the art grid), each art pixel a
// whole block of device pixels.
void draw(juce::Graphics& g, const Canvas& canvas, juce::Point<int> topLeft);
// Draws a cached sprite (made by `make` once per key, look and device scale).
void drawCached(juce::Graphics& g, const juce::String& key, juce::Point<int> topLeft, const std::function<Canvas()>& make);
// The same at half the art grid: one logical pixel per art pixel (small icons in tight
// rows). Whole device pixels at every size (it rounds down at 150 %).
void drawCachedSmall(juce::Graphics& g, const juce::String& key, juce::Point<int> topLeft, const std::function<Canvas()>& make);
// The title: Large text at twice the art grid, lit from the top.
void drawTitle(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, juce::Colour colour, juce::Colour light, juce::Colour shadow);
// The size in logical pixels of a sprite once drawn.
inline juce::Point<int> logicalSize(const Canvas& c) { return { c.width() * unit, c.height() * unit }; }

// Direct drawing on the art grid, for large areas that change all the time (the piano
// roll, the timeline): rectangles snapped to whole art pixels, so they stay exact at
// every UI size and cost no sprite.
juce::Rectangle<int> snapArt(juce::Rectangle<float> r);
void fillArt(juce::Graphics& g, juce::Rectangle<int> r, juce::Colour c);
// A one-art-pixel outline with cut corners.
void frameArt(juce::Graphics& g, juce::Rectangle<int> r, juce::Colour c);
// A raised block: outline with cut corners, a lit top row, a shaded bottom row.
void blockArt(juce::Graphics& g, juce::Rectangle<int> r, juce::Colour body, juce::Colour light, juce::Colour shade, juce::Colour outline);

// Pixel text in `area` (logical), with a drop shadow in the mood's outline colour (or
// `shadow`; transparent for none), cut to fit.
void drawText(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, juce::Colour colour,
              juce::Justification justification = juce::Justification::centredLeft, Font font = Font::Small,
              juce::Colour shadow = juce::Colours::black.withAlpha(0.0f), bool shadowFromTheme = true);
// Its width in logical pixels.
int textWidthLogical(const juce::String& text, Font font = Font::Small);

// A small cache of sprites by key. Pixel sprites are cheap to make but cheaper to reuse.
class SpriteCache {
public:
    explicit SpriteCache(int capacity = 768, int64_t byteBudget = 48 << 20) : limit(capacity), budget(byteBudget) {}
    const juce::Image& get(const juce::String& key, const std::function<juce::Image()>& make);
    void clear() {
        sprites.clear();
        bytes = 0;
    }
    int size() const { return int(sprites.size()); }
    int64_t memory() const { return bytes; }
    int made = 0;   // how many sprites were drawn (not reused)

private:
    std::unordered_map<juce::String, juce::Image> sprites;
    int limit;
    int64_t budget, bytes = 0;
};
SpriteCache& sharedCache();

// ---- Controls (sizes in art pixels) ------------------------------------------------

struct State {
    bool hover = false, down = false, on = false, enabled = true;
};

// The space around a control sprite for its decorations (sparkles, brackets, glow).
inline constexpr int margin = 3;

// A pixel dial `dial` art pixels across: a segmented value ring (gems / runes / LEDs), a
// bevelled body and a chunky pointer. Label above and value below when given (Small).
Canvas knob(const Palette& p, int dial, float value, const juce::String& label, const juce::String& valueText, const State& s, int frame = 0,
            juce::Colour accent = {});
// The Score Intensity dial: larger, a full ring that heats from cool to the accent,
// a core that glows with the value.
Canvas intensityKnob(const Palette& p, int dial, float value, const State& s, int frame = 0);
// A slider track `length` long with a sprite handle `thickness` across (5-11: slimmer
// handles and tracks for tight rows); vertical runs bottom to top.
Canvas slider(const Palette& p, int length, float value, bool vertical, const State& s, int frame = 0, juce::Colour accent = {}, int thickness = 11);
// The 13a slider with its label and value above it.
Canvas labelledSlider(const Palette& p, int width, float value, const juce::String& label, const juce::String& valueText, const State& s, int frame = 0);
// A value bar (a field filled to the value, the value written on it).
Canvas bar(const Palette& p, int width, int height, float value, const juce::String& text, const State& s);
// A chunky bevelled button `width` x `height` (0: fit the text). Its icon (an
// IconSprite canvas) sits left of the text. On: lit in `accent` (or the mood's).
Canvas button(const Palette& p, const juce::String& text, const State& s, int width = 0, int height = 15, int frame = 0, const Canvas* icon = nullptr,
              juce::Colour accent = {});
// A switch with an LED, its label to the right.
Canvas toggle(const Palette& p, const juce::String& label, const State& s, int frame = 0);
// A strip of tabs over a panel edge; the active tab joins the panel.
Canvas tabs(const Palette& p, const juce::StringArray& names, int active, int hovered = -1, int frame = 0);
// One tab `width` x `height`, active (joined to the panel below) or not.
Canvas tab(const Palette& p, const juce::String& text, int width, int height, bool active, bool hover, const Canvas* icon = nullptr, juce::Colour accent = {});
// A dropdown field with its arrow; with `items`, the open menu under it.
Canvas dropdown(const Palette& p, const juce::String& text, int width, const State& s, const juce::StringArray* items = nullptr, int highlighted = -1,
                int frame = 0, int height = 15);
// A recessed field (text boxes, empty slots).
Canvas field(const Palette& p, int width, int height, bool hover = false);
// A segmented LED meter `length` x `thickness`, level 0..1 (peak segments at the top).
Canvas meter(const Palette& p, int length, int thickness, float level, bool vertical);
// A Place card `width` x `height`: the icon in a framed window, its name on a band.
Canvas placeCard(const Palette& p, int placeType, const juce::String& name, const State& s, int frame = 0, int width = 0, int height = 0);
// A panel background `width` x `height` in the mood's material, framed.
Canvas panel(const Palette& p, int width, int height, int frame = 0, juce::Colour tint = {}, float tintAmount = 0);
// A small label chip (category chips, Location chips); `accent` colours it when on.
Canvas chip(const Palette& p, const juce::String& text, int width, int height, const State& s, juce::Colour accent = {});

// A Place strip `width` x `height`: a raised face with the Place's colour along its
// left edge, outlined in it when `selected`.
Canvas strip(const Palette& p, int width, int height, juce::Colour accent, bool selected);
// A small round badge with a number (how many of a Place are in use).
Canvas badge(const Palette& p, const juce::String& text, juce::Colour accent);
// A Location marker on the globe's orbit: a gem in its colour (an empty ring when unused).
Canvas marker(const Palette& p, juce::Colour colour, bool used, bool here);

// The tab strip's hit areas, for a test (art pixels, in the sprite).
juce::Array<juce::Rectangle<int>> tabAreas(const juce::StringArray& names);

// ---- Icons -------------------------------------------------------------------------

inline constexpr int iconSize = 32;
// True when a Place has a pixel icon (all of them, and Empty: a signpost).
bool hasPlaceIcon(int placeType);
// How many idle-animation frames its icon has (2-4).
int placeIconFrames(int placeType);
// The icon's sprite (32 x 32, transparent background) for an animation frame, its
// outline in `outline`. A bundled `assets/pixel/place-<key>.png` (frames side by side,
// each square) replaces the code-drawn art.
Canvas placeIcon(int placeType, int frame, juce::Colour outline);
// Draws a Place icon (cached) at `topLeft`: on the art grid, or at half of it (`half`:
// one logical pixel per art pixel, for tight rows), showing only its top `rows`.
void drawPlaceIcon(juce::Graphics& g, int placeType, juce::Point<int> topLeft, bool half, int frame = 0, int rows = iconSize);
// The icon animation frame for now, by the theme's delights (Full: always moving,
// Subtle: only when `hovered`, Off: still).
int iconFrame(int placeType, bool hovered);
// A Place's short name for small spaces (cards, chips).
juce::String placeShortName(int placeType);

// A tool icon (wb::ui::Icon) or instrument icon (wb::InstrumentIcon) as a `size` x
// `size` pixel sprite in `colour`, with a one-pixel shadow. Bundled
// `assets/pixel/icon-<name>.png` / `instrument-<name>.png` replace them (white on
// transparent; tinted to `colour`).
Canvas toolIcon(int icon, int size, juce::Colour colour, juce::Colour shadow);
Canvas instrumentIcon(int icon, int size, juce::Colour colour, juce::Colour shadow);
// Their file names (without .png).
juce::String toolIconName(int icon);
juce::String instrumentIconName(int icon);

// A bundled replacement sprite (`assets/pixel/<name>.png`), or an invalid image.
juce::Image bundledSprite(const juce::String& name);

// ---- Sheets (snapshots for review) -------------------------------------------------

// Every control in one mood, on that mood's panel.
Canvas prototypeSheet(const Mood& mood, int frame = 0);
inline Canvas prototypeSheet(float sciFi, int frame = 0) { return prototypeSheet(Mood { sciFi, 0 }, frame); }
// Fantasy, blend, sci-fi and neutral side by side, titled.
Canvas prototypeComparison(int frame = 0);
// One button, knob and card at 0, 25, 50, 75 and 100 % sci-fi, and neutral.
Canvas moodSweep(int frame = 0);
// All 27 Place icons, every animation frame (in the mood their world belongs to).
Canvas iconStrips();
// Every tool and instrument icon.
Canvas iconSheet();

} // namespace wb::ui::pixel
