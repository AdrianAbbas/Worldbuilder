#include "Pixel.h"
#include "../Places/Place.h"
#include <cmath>

namespace wb::ui::pixel {

// ---- Moods -------------------------------------------------------------------------

Mood moodOf(const float* presence, int count) {
    float axis = 0, axisWeight = 0, modern = 0, total = 0;
    for (int type = NoPlace + 1; type < std::min(count, int(PlaceTypeCount)); ++type) {
        const float w = presence[type];
        if (w <= 0) continue;
        total += w;
        const int category = placeInfo(type).category;
        if (category == Modern) {
            modern += w;
            continue;
        }
        axis += w * (category == SciFi ? 1.0f : category == Nature ? 0.15f : 0.0f);
        axisWeight += w;
    }
    Mood m;
    m.sciFi = axisWeight > 0 ? axis / axisWeight : blendMood;
    m.neutral = total > 0 ? modern / total : 0.0f;
    return m;
}

namespace {
juce::Colour c(uint32_t rgb) { return juce::Colour(0xff000000 | rgb); }

// The keyframes. Faces stay dark and inks light in all of them, so text keeps its
// contrast at every point between them.
Palette fantasy() {
    // Storybook whimsy: berry-painted faces, sunny gold trim, mint curls, an enchanted
    // teal-green ground with little stars and flowers.
    Palette p;
    p.outline = c(0x2a1430);
    p.rim = c(0xefb54a); p.rimLight = c(0xffe48a); p.rimDark = c(0xae6c2c);
    p.inlay = c(0xa6f2cf);
    p.face = c(0x7a3a66); p.faceLight = c(0x8e4a7a); p.faceDark = c(0x56284a);
    p.well = c(0x2a1430); p.wellLight = c(0x46244c);
    p.accent = c(0xffd34e); p.accentDim = c(0x5a3a52); p.glow = c(0xfff6b8);
    p.ink = c(0xfff3dc); p.inkDim = c(0xd9b9cb); p.inkOn = c(0x3a1a22);
    p.panel = c(0x1e3832); p.panelLight = c(0x2c4c44); p.panelDark = c(0x142824);
    p.warn = c(0xff9f5a); p.peak = c(0xff5c7a);
    p.heading = c(0xf6c04e);
    p.sparkle = 1;
    return p;
}
Palette sciFi() {
    Palette p;
    p.outline = c(0x04060c);
    p.rim = c(0x3c485c); p.rimLight = c(0x64748e); p.rimDark = c(0x222a38);
    p.inlay = c(0x38eaff);
    p.face = c(0x172030); p.faceLight = c(0x24324a); p.faceDark = c(0x0d121c);
    p.well = c(0x05080e); p.wellLight = c(0x111a28);
    p.accent = c(0x3ef0ff); p.accentDim = c(0x103444); p.glow = c(0xb4faff);
    p.ink = c(0xc4f6ff); p.inkDim = c(0x6a8aa2); p.inkOn = c(0x03141c);
    p.panel = c(0x0b1019); p.panelLight = c(0x162234); p.panelDark = c(0x060910);
    p.warn = c(0xffd23e); p.peak = c(0xff3e7a);
    p.heading = c(0x5ee8ff);
    p.scanline = 1;
    return p;
}
Palette magitech() {
    Palette p;
    p.outline = c(0x160a12);
    p.rim = c(0x84502c); p.rimLight = c(0xb0743e); p.rimDark = c(0x4e2a18);
    p.inlay = c(0x46f2d2);
    p.face = c(0x3a2236); p.faceLight = c(0x56344e); p.faceDark = c(0x24121f);
    p.well = c(0x140810); p.wellLight = c(0x281424);
    p.accent = c(0xc77bff); p.accentDim = c(0x3c1e4c); p.glow = c(0xecc8ff);
    p.ink = c(0xffe8f2); p.inkDim = c(0xb491aa); p.inkOn = c(0x1c0826);
    p.panel = c(0x1f1220); p.panelLight = c(0x2e1c2e); p.panelDark = c(0x140a14);
    p.warn = c(0xffc85a); p.peak = c(0xff4a6a);
    p.heading = c(0xe8a868);
    p.runes = 1;
    return p;
}
Palette neutralKeyframe() {
    // Modern: clean graphite and brushed aluminium, a coral signal light, no decoration.
    Palette p;
    p.outline = c(0x101318);
    p.rim = c(0x8c949e); p.rimLight = c(0xc4ccd4); p.rimDark = c(0x565e68);
    p.inlay = c(0xe4eaf0);
    p.face = c(0x33383f); p.faceLight = c(0x424851); p.faceDark = c(0x23272c);
    p.well = c(0x121519); p.wellLight = c(0x1e2227);
    p.accent = c(0xff8f6b); p.accentDim = c(0x4a3330); p.glow = c(0xffd2c2);
    p.ink = c(0xf2f4f6); p.inkDim = c(0xa3abb4); p.inkOn = c(0x2a120c);
    p.panel = c(0x1a1d22); p.panelLight = c(0x252930); p.panelDark = c(0x121418);
    p.warn = c(0xffd25a); p.peak = c(0xff4a4a);
    p.heading = c(0xc8d0d8);
    return p;
}

Palette mix(const Palette& a, const Palette& b, float t) {
    const auto m = [t](juce::Colour x, juce::Colour y) { return x.interpolatedWith(y, t); };
    Palette p;
    p.outline = m(a.outline, b.outline);
    p.rim = m(a.rim, b.rim); p.rimLight = m(a.rimLight, b.rimLight); p.rimDark = m(a.rimDark, b.rimDark);
    p.inlay = m(a.inlay, b.inlay);
    p.face = m(a.face, b.face); p.faceLight = m(a.faceLight, b.faceLight); p.faceDark = m(a.faceDark, b.faceDark);
    p.well = m(a.well, b.well); p.wellLight = m(a.wellLight, b.wellLight);
    p.accent = m(a.accent, b.accent); p.accentDim = m(a.accentDim, b.accentDim); p.glow = m(a.glow, b.glow);
    p.ink = m(a.ink, b.ink); p.inkDim = m(a.inkDim, b.inkDim); p.inkOn = m(a.inkOn, b.inkOn);
    p.panel = m(a.panel, b.panel); p.panelLight = m(a.panelLight, b.panelLight); p.panelDark = m(a.panelDark, b.panelDark);
    p.warn = m(a.warn, b.warn); p.peak = m(a.peak, b.peak);
    p.heading = m(a.heading, b.heading);
    p.sparkle = a.sparkle + (b.sparkle - a.sparkle) * t;
    p.scanline = a.scanline + (b.scanline - a.scanline) * t;
    p.runes = a.runes + (b.runes - a.runes) * t;
    return p;
}
} // namespace

Palette palette(float mood) {
    static const Palette f = fantasy(), b = magitech(), s = sciFi();
    mood = juce::jlimit(0.0f, 1.0f, mood);
    return mood <= 0.5f ? mix(f, b, mood * 2.0f) : mix(b, s, mood * 2.0f - 1.0f);
}

Palette palette(const Mood& mood) {
    static const Palette n = neutralKeyframe();
    const auto axis = palette(mood.sciFi);
    return mood.neutral <= 0 ? axis : mix(axis, n, juce::jlimit(0.0f, 1.0f, mood.neutral));
}

float contrast(juce::Colour a, juce::Colour b) {
    const auto luminance = [](juce::Colour x) {
        const auto channel = [](juce::uint8 v) {
            const float f = float(v) / 255.0f;
            return f <= 0.03928f ? f / 12.92f : std::pow((f + 0.055f) / 1.055f, 2.4f);
        };
        return 0.2126f * channel(x.getRed()) + 0.7152f * channel(x.getGreen()) + 0.0722f * channel(x.getBlue());
    };
    const float la = luminance(a), lb = luminance(b);
    return (std::max(la, lb) + 0.05f) / (std::min(la, lb) + 0.05f);
}

namespace {
Theme& mutableTheme() {
    static Theme t;
    return t;
}
} // namespace

const Theme& theme() { return mutableTheme(); }

void setTheme(const Mood& mood, int frame, int delights) {
    auto& t = mutableTheme();
    Mood stepped { std::round(juce::jlimit(0.0f, 1.0f, mood.sciFi) * 64.0f) / 64.0f, std::round(juce::jlimit(0.0f, 1.0f, mood.neutral) * 64.0f) / 64.0f };
    // Without delights the decoration stands still.
    if (delights == 0) frame = 0;
    if (!(stepped == t.mood) || delights != t.delights) {
        t.mood = stepped;
        t.colours = palette(stepped);
        t.delights = delights;
        // The version names the look (not a counter): two editors in different moods
        // each keep their own sprites in the cache.
        t.version = (juce::roundToInt(stepped.sciFi * 64.0f) * 65 + juce::roundToInt(stepped.neutral * 64.0f)) * 3 + delights;
    }
    t.frame = frame;
}

// ---- The pixel fonts ---------------------------------------------------------------

namespace {
// Original glyphs drawn for this plugin ('#' ink), rows separated by '|'. Glyphs are
// proportional: each is as wide as its widest row.
struct Glyph {
    char c;
    const char* rows;
};
constexpr Glyph large[] {
    { 'A', ".###.|#...#|#...#|#####|#...#|#...#|#...#" }, { 'B', "####.|#...#|#...#|####.|#...#|#...#|####." },
    { 'C', ".###.|#...#|#....|#....|#....|#...#|.###." }, { 'D', "####.|#...#|#...#|#...#|#...#|#...#|####." },
    { 'E', "#####|#....|#....|####.|#....|#....|#####" }, { 'F', "#####|#....|#....|####.|#....|#....|#...." },
    { 'G', ".###.|#...#|#....|#.###|#...#|#...#|.####" }, { 'H', "#...#|#...#|#...#|#####|#...#|#...#|#...#" },
    { 'I', "###|.#.|.#.|.#.|.#.|.#.|###" },              { 'J', "..###|...#.|...#.|...#.|#..#.|#..#.|.##.." },
    { 'K', "#...#|#..#.|#.#..|##...|#.#..|#..#.|#...#" }, { 'L', "#....|#....|#....|#....|#....|#....|#####" },
    { 'M', "#...#|##.##|#.#.#|#.#.#|#...#|#...#|#...#" }, { 'N', "#...#|##..#|#.#.#|#..##|#...#|#...#|#...#" },
    { 'O', ".###.|#...#|#...#|#...#|#...#|#...#|.###." }, { 'P', "####.|#...#|#...#|####.|#....|#....|#...." },
    { 'Q', ".###.|#...#|#...#|#...#|#.#.#|#..#.|.##.#" }, { 'R', "####.|#...#|#...#|####.|#.#..|#..#.|#...#" },
    { 'S', ".####|#....|#....|.###.|....#|....#|####." }, { 'T', "#####|..#..|..#..|..#..|..#..|..#..|..#.." },
    { 'U', "#...#|#...#|#...#|#...#|#...#|#...#|.###." }, { 'V', "#...#|#...#|#...#|#...#|#...#|.#.#.|..#.." },
    { 'W', "#...#|#...#|#...#|#.#.#|#.#.#|#.#.#|.#.#." }, { 'X', "#...#|#...#|.#.#.|..#..|.#.#.|#...#|#...#" },
    { 'Y', "#...#|#...#|.#.#.|..#..|..#..|..#..|..#.." }, { 'Z', "#####|....#|...#.|..#..|.#...|#....|#####" },
    { '0', ".###.|#...#|#..##|#.#.#|##..#|#...#|.###." }, { '1', ".#.|##.|.#.|.#.|.#.|.#.|###" },
    { '2', ".###.|#...#|....#|...#.|..#..|.#...|#####" }, { '3', "####.|....#|....#|.###.|....#|....#|####." },
    { '4', "...#.|..##.|.#.#.|#..#.|#####|...#.|...#." }, { '5', "#####|#....|####.|....#|....#|#...#|.###." },
    { '6', ".###.|#....|#....|####.|#...#|#...#|.###." }, { '7', "#####|....#|...#.|..#..|.#...|.#...|.#..." },
    { '8', ".###.|#...#|#...#|.###.|#...#|#...#|.###." }, { '9', ".###.|#...#|#...#|.####|....#|....#|.###." },
    { '.', "..|..|..|..|..|##|##" },                      { ',', "..|..|..|..|##|.#|#." },
    { ':', "..|##|##|..|##|##|.." },                      { '-', "....|....|....|####|....|....|...." },
    { '+', ".....|..#..|..#..|#####|..#..|..#..|....." }, { '/', "....#|....#|...#.|..#..|.#...|#....|#...." },
    { '%', "##...|##..#|...#.|..#..|.#...|#..##|...##" }, { '<', "...#|..#.|.#..|#...|.#..|..#.|...#" },
    { '>', "#...|.#..|..#.|...#|..#.|.#..|#..." },       { '!', "#|#|#|#|#|.|#" },
    { '?', ".###.|#...#|....#|...#.|..#..|.....|..#.." }, { '\'', "#|#|.|.|.|.|." },
    { '&', ".##..|#..#.|#.#..|.#...|#.#.#|#..#.|.##.#" }, { '#', ".#.#.|.#.#.|#####|.#.#.|#####|.#.#.|.#.#." },
    { '(', ".#|#.|#.|#.|#.|#.|.#" },                      { ')', "#.|.#|.#|.#|.#|.#|#." },
    { '=', "....|....|####|....|####|....|...." },        { '_', "....|....|....|....|....|....|####" },
    { '"', "#.#|#.#|...|...|...|...|..." },               { '*', ".....|#.#.#|.###.|#####|.###.|#.#.#|....." },
};
constexpr Glyph small[] {
    { 'A', ".#.|#.#|###|#.#|#.#" }, { 'B', "##.|#.#|##.|#.#|##." }, { 'C', ".##|#..|#..|#..|.##" }, { 'D', "##.|#.#|#.#|#.#|##." },
    { 'E', "###|#..|##.|#..|###" }, { 'F', "###|#..|##.|#..|#.." }, { 'G', ".##|#..|#.#|#.#|.##" }, { 'H', "#.#|#.#|###|#.#|#.#" },
    { 'I', "###|.#.|.#.|.#.|###" }, { 'J', "..#|..#|..#|#.#|.#." }, { 'K', "#.#|#.#|##.|#.#|#.#" }, { 'L', "#..|#..|#..|#..|###" },
    { 'M', "#...#|##.##|#.#.#|#...#|#...#" }, { 'N', "#..#|##.#|#.##|#..#|#..#" }, { 'O', ".#.|#.#|#.#|#.#|.#." },
    { 'P', "##.|#.#|##.|#..|#.." }, { 'Q', ".#.|#.#|#.#|##.|.##" }, { 'R', "##.|#.#|##.|#.#|#.#" }, { 'S', ".##|#..|.#.|..#|##." },
    { 'T', "###|.#.|.#.|.#.|.#." }, { 'U', "#.#|#.#|#.#|#.#|###" }, { 'V', "#.#|#.#|#.#|#.#|.#." },
    { 'W', "#...#|#...#|#.#.#|##.##|#...#" }, { 'X', "#.#|#.#|.#.|#.#|#.#" }, { 'Y', "#.#|#.#|.#.|.#.|.#." }, { 'Z', "###|..#|.#.|#..|###" },
    { '0', "###|#.#|#.#|#.#|###" }, { '1', ".#.|##.|.#.|.#.|###" }, { '2', "##.|..#|.#.|#..|###" }, { '3', "##.|..#|.#.|..#|##." },
    { '4', "#.#|#.#|###|..#|..#" }, { '5', "###|#..|##.|..#|##." }, { '6', ".##|#..|###|#.#|###" }, { '7', "###|..#|.#.|.#.|.#." },
    { '8', "###|#.#|###|#.#|###" }, { '9', "###|#.#|###|..#|##." }, { '.', ".|.|.|.|#" }, { ',', "..|..|..|.#|#." },
    { ':', ".|#|.|#|." }, { '-', "...|...|###|...|..." }, { '+', "...|.#.|###|.#.|..." }, { '/', "..#|..#|.#.|#..|#.." },
    { '%', "#.#|..#|.#.|#..|#.#" }, { '<', "..#|.#.|#..|.#.|..#" }, { '>', "#..|.#.|..#|.#.|#.." }, { '!', "#|#|#|.|#" },
    { '?', "##.|..#|.#.|...|.#." }, { '\'', "#|#|.|.|." }, { '(', ".#|#.|#.|#.|.#" }, { ')', "#.|.#|.#|.#|#." },
    { '&', ".#.|#.#|.#.|#.#|.##" }, { '=', "...|###|...|###|..." }, { '_', "...|...|...|...|###" }, { '"', "#.#|#.#|...|...|..." },
    { '#', "#.#|###|#.#|###|#.#" }, { '*', "#.#|.#.|###|.#.|#.#" },
};

struct Face {
    // A glyph's rows as bit masks and its width; index by character.
    struct G {
        uint8_t rows[7] {};
        uint8_t width = 0;
        bool present = false;
    };
    std::array<G, 128> glyphs {};
    int height = 7, space = 3;
};
Face build(const Glyph* table, int count, int height, int space) {
    Face f;
    f.height = height;
    f.space = space;
    for (int i = 0; i < count; ++i) {
        auto& g = f.glyphs[std::size_t(table[i].c)];
        g.present = true;
        int row = 0, col = 0;
        for (const char* r = table[i].rows; *r != 0; ++r) {
            if (*r == '|') { ++row; col = 0; continue; }
            if (*r == '#') g.rows[row] |= uint8_t(1 << col);
            g.width = uint8_t(std::max<int>(g.width, ++col));
        }
    }
    return f;
}
const Face& faceFor(Font font) {
    static const Face l = build(large, int(std::size(large)), 7, 3), s = build(small, int(std::size(small)), 5, 2);
    return font == Font::Large ? l : s;
}
const Face::G* glyphFor(const Face& f, juce::juce_wchar ch) {
    ch = juce::CharacterFunctions::toUpperCase(ch);
    if (ch == 0x2026) ch = '.';   // an ellipsis
    if (ch < 0 || ch >= 128) return nullptr;
    const auto& g = f.glyphs[std::size_t(ch)];
    return g.present ? &g : nullptr;
}
int advanceOf(const Face& f, juce::juce_wchar ch) {
    if (const auto* g = glyphFor(f, ch)) return g->width + 1;
    return f.space + 1;
}
} // namespace

// ---- Canvas ------------------------------------------------------------------------

Canvas::Canvas(int width, int height) : image(juce::Image::ARGB, std::max(1, width), std::max(1, height), true), w(std::max(1, width)), h(std::max(1, height)) {}

juce::Colour Canvas::get(int x, int y) const {
    if (x < 0 || y < 0 || x >= w || y >= h) return juce::Colours::transparentBlack;
    return image.getPixelAt(x, y);
}
void Canvas::set(int x, int y, juce::Colour colour) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    image.setPixelAt(x, y, colour);
}
void Canvas::blend(int x, int y, juce::Colour colour) {
    if (x < 0 || y < 0 || x >= w || y >= h || colour.getAlpha() == 0) return;
    auto under = image.getPixelAt(x, y);
    under = under.getAlpha() == 0 ? colour : under.overlaidWith(colour);
    image.setPixelAt(x, y, under);
}
void Canvas::fill(int x, int y, int width, int height, juce::Colour colour) {
    const int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(w, x + width), y1 = std::min(h, y + height);
    if (x0 >= x1 || y0 >= y1) return;
    juce::Image::BitmapData data(image, x0, y0, x1 - x0, y1 - y0, juce::Image::BitmapData::writeOnly);
    for (int j = 0; j < y1 - y0; ++j)
        for (int i = 0; i < x1 - x0; ++i) data.setPixelColour(i, j, colour);
}
void Canvas::blendFill(int x, int y, int width, int height, juce::Colour colour) {
    for (int j = y; j < y + height; ++j)
        for (int i = x; i < x + width; ++i) blend(i, j, colour);
}
void Canvas::hline(int x0, int x1, int y, juce::Colour colour) { fill(std::min(x0, x1), y, std::abs(x1 - x0) + 1, 1, colour); }
void Canvas::vline(int x, int y0, int y1, juce::Colour colour) { fill(x, std::min(y0, y1), 1, std::abs(y1 - y0) + 1, colour); }
void Canvas::line(int x0, int y0, int x1, int y1, juce::Colour colour) {
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        set(x0, y0, colour);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
void Canvas::box(int x, int y, int width, int height, juce::Colour fillColour, juce::Colour outlineColour) {
    fill(x + 1, y + 1, width - 2, height - 2, fillColour);
    outlineBox(x, y, width, height, outlineColour);
}
void Canvas::outlineBox(int x, int y, int width, int height, juce::Colour outlineColour) {
    hline(x + 1, x + width - 2, y, outlineColour);
    hline(x + 1, x + width - 2, y + height - 1, outlineColour);
    vline(x, y + 1, y + height - 2, outlineColour);
    vline(x + width - 1, y + 1, y + height - 2, outlineColour);
}
void Canvas::paste(const Canvas& other, int x, int y) { paste(other.source(), x, y); }
void Canvas::paste(const juce::Image& other, int x, int y) {
    if (!other.isValid()) return;
    juce::Image::BitmapData src(other, juce::Image::BitmapData::readOnly);
    for (int j = 0; j < other.getHeight(); ++j)
        for (int i = 0; i < other.getWidth(); ++i) {
            const auto colour = src.getPixelColour(i, j);
            if (colour.getAlpha() == 255) set(x + i, y + j, colour);
            else blend(x + i, y + j, colour);
        }
}

int Canvas::textWidth(const juce::String& text, Font font) {
    const auto& f = faceFor(font);
    int width = 0;
    for (auto it = text.getCharPointer(); !it.isEmpty();) width += advanceOf(f, it.getAndAdvance());
    return std::max(0, width - 1);
}

void Canvas::text(int x, int y, const juce::String& text, juce::Colour colour, Font font) {
    const auto& f = faceFor(font);
    for (auto it = text.getCharPointer(); !it.isEmpty();) {
        const auto ch = it.getAndAdvance();
        if (const auto* g = glyphFor(f, ch)) {
            for (int row = 0; row < f.height; ++row)
                for (int col = 0; col < g->width; ++col)
                    if (g->rows[row] & (1 << col)) blend(x + col, y + row, colour);
        }
        x += advanceOf(f, ch);
    }
}
void Canvas::shadowText(int x, int y, const juce::String& s, juce::Colour colour, juce::Colour shadow, Font font) {
    if (shadow.getAlpha() > 0) text(x + 1, y + 1, s, shadow, font);
    text(x, y, s, colour, font);
}
void Canvas::centredText(int y, const juce::String& s, juce::Colour colour, juce::Colour shadow, Font font) {
    shadowText((w - textWidth(s, font)) / 2, y, s, colour, shadow, font);
}

juce::Image Canvas::scaled(int scale) const {
    scale = std::max(1, scale);
    if (scale == 1) return image.createCopy();
    juce::Image out(juce::Image::ARGB, w * scale, h * scale, false);
    juce::Image::BitmapData src(image, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dst(out, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const auto colour = src.getPixelColour(x, y);
            for (int j = 0; j < scale; ++j)
                for (int i = 0; i < scale; ++i) dst.setPixelColour(x * scale + i, y * scale + j, colour);
        }
    return out;
}

juce::String fitText(const juce::String& text, int width, Font font) {
    if (Canvas::textWidth(text, font) <= width) return text;
    auto cut = text;
    while (cut.isNotEmpty() && Canvas::textWidth(cut.trimEnd() + "..", font) > width) cut = cut.dropLastCharacters(1);
    return cut.isEmpty() ? juce::String() : cut.trimEnd() + "..";
}

// ---- Drawing -----------------------------------------------------------------------

int blockFor(juce::Graphics& g) {
    const float physical = g.getInternalContext().getPhysicalPixelScaleFactor();
    return std::max(1, juce::roundToInt(float(unit) * physical));
}

namespace {
// Draws an image already scaled to `block` device pixels per art pixel at a logical
// position, snapped to whole device pixels.
void drawScaled(juce::Graphics& g, const juce::Image& image, juce::Point<int> topLeft, int block) {
    const float physical = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (block == unit && std::abs(physical - 1.0f) < 1.0e-3f) {
        g.drawImageAt(image, topLeft.x, topLeft.y);
        return;
    }
    juce::Graphics::ScopedSaveState save(g);
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    const float px = std::round(float(topLeft.x) * physical), py = std::round(float(topLeft.y) * physical);
    g.drawImageTransformed(image, juce::AffineTransform::translation(px, py).scaled(1.0f / physical));
}
} // namespace

void draw(juce::Graphics& g, const Canvas& canvas, juce::Point<int> topLeft) {
    const int block = blockFor(g);
    drawScaled(g, canvas.scaled(block), { snap(topLeft.x), snap(topLeft.y) }, block);
}

void drawCached(juce::Graphics& g, const juce::String& key, juce::Point<int> topLeft, const std::function<Canvas()>& make) {
    const int block = blockFor(g);
    const auto& image = sharedCache().get(key + "@" + juce::String(block) + "/" + juce::String(theme().version), [&] { return make().scaled(block); });
    drawScaled(g, image, { snap(topLeft.x), snap(topLeft.y) }, block);
}

juce::Rectangle<int> snapArt(juce::Rectangle<float> r) {
    const int x0 = snap(juce::roundToInt(r.getX())), y0 = snap(juce::roundToInt(r.getY()));
    const int x1 = snap(juce::roundToInt(r.getRight()) + unit / 2), y1 = snap(juce::roundToInt(r.getBottom()) + unit / 2);
    return { x0, y0, std::max(unit, x1 - x0), std::max(unit, y1 - y0) };
}

void fillArt(juce::Graphics& g, juce::Rectangle<int> r, juce::Colour c) {
    if (r.isEmpty() || c.getAlpha() == 0) return;
    g.setColour(c);
    g.fillRect(snap(r.getX()), snap(r.getY()), snap(r.getWidth() + unit / 2), snap(r.getHeight() + unit / 2));
}

void frameArt(juce::Graphics& g, juce::Rectangle<int> r, juce::Colour c) {
    r = { snap(r.getX()), snap(r.getY()), snap(r.getWidth() + unit / 2), snap(r.getHeight() + unit / 2) };
    if (r.getWidth() < 3 * unit || r.getHeight() < 3 * unit) {
        fillArt(g, r, c);
        return;
    }
    g.setColour(c);
    g.fillRect(r.getX() + unit, r.getY(), r.getWidth() - 2 * unit, unit);
    g.fillRect(r.getX() + unit, r.getBottom() - unit, r.getWidth() - 2 * unit, unit);
    g.fillRect(r.getX(), r.getY() + unit, unit, r.getHeight() - 2 * unit);
    g.fillRect(r.getRight() - unit, r.getY() + unit, unit, r.getHeight() - 2 * unit);
}

void blockArt(juce::Graphics& g, juce::Rectangle<int> r, juce::Colour body, juce::Colour light, juce::Colour shade, juce::Colour outline) {
    r = { snap(r.getX()), snap(r.getY()), snap(r.getWidth() + unit / 2), snap(r.getHeight() + unit / 2) };
    if (r.getWidth() < 3 * unit || r.getHeight() < 3 * unit) {
        fillArt(g, r, body);
        return;
    }
    fillArt(g, r.reduced(unit), body);
    if (r.getHeight() >= 5 * unit) {
        fillArt(g, { r.getX() + unit, r.getY() + unit, r.getWidth() - 2 * unit, unit }, light);
        fillArt(g, { r.getX() + unit, r.getBottom() - 2 * unit, r.getWidth() - 2 * unit, unit }, shade);
    }
    frameArt(g, r, outline);
}

void snapChildren(juce::Component& parent) {
    for (auto* child : parent.getChildren())
        if (const auto snapped = snap(child->getBounds()); snapped != child->getBounds()) child->setBounds(snapped);
}

void drawCachedSmall(juce::Graphics& g, const juce::String& key, juce::Point<int> topLeft, const std::function<Canvas()>& make) {
    const float physical = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int block = std::max(1, int(std::floor(physical + 1.0e-3f)));
    const auto& image = sharedCache().get(key + "@s" + juce::String(block) + "/" + juce::String(theme().version), [&] { return make().scaled(block); });
    if (std::abs(physical - 1.0f) < 1.0e-3f) {
        g.drawImageAt(image, topLeft.x, topLeft.y);
        return;
    }
    juce::Graphics::ScopedSaveState save(g);
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImageTransformed(image, juce::AffineTransform::translation(std::round(float(topLeft.x) * physical), std::round(float(topLeft.y) * physical)).scaled(1.0f / physical));
}

void drawTitle(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, juce::Colour colour, juce::Colour light, juce::Colour shadow) {
    const int w = Canvas::textWidth(text) + 2, h = Canvas::textHeight() + 2;
    if (w * unit * 2 > area.getWidth()) {
        // Too long for the double size: Large text at the art grid.
        drawText(g, text, area, colour, juce::Justification::centredLeft, Font::Large, shadow);
        return;
    }
    const float physical = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int block = std::max(1, juce::roundToInt(float(unit * 2) * physical));
    const auto key = "title/" + text + colour.toString() + light.toString() + shadow.toString();
    const auto& image = sharedCache().get(key + "@" + juce::String(block), [&] {
        Canvas k(w, h);
        // A shadow, then the letters: lit in the top rows, a darker base.
        k.text(1, 1, text, shadow);
        Canvas letters(w, h);
        letters.text(0, 0, text, colour);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                if (letters.get(x, y).getAlpha() > 0) k.set(x, y, y < 2 ? light : y > 4 ? colour.darker(0.25f) : colour);
        return k.scaled(block);
    });
    const int x = snap(area.getX()), y = snap(area.getCentreY() - h * unit);
    juce::Graphics::ScopedSaveState save(g);
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImageTransformed(image, juce::AffineTransform::translation(std::round(float(x) * physical), std::round(float(y) * physical)).scaled(1.0f / physical));
}

int textWidthLogical(const juce::String& text, Font font) { return Canvas::textWidth(text, font) * unit; }

void drawText(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, juce::Colour colour, juce::Justification justification, Font font,
              juce::Colour shadow, bool shadowFromTheme) {
    if (text.isEmpty() || colour.getAlpha() == 0) return;
    const auto shown = fitText(text, std::max(0, area.getWidth() / unit), font);
    if (shown.isEmpty()) return;
    if (shadowFromTheme && shadow.getAlpha() == 0) shadow = theme().colours.outline.withMultipliedAlpha(colour.getFloatAlpha());
    const int w = Canvas::textWidth(shown, font) + 1, h = Canvas::textHeight(font) + 1;
    int x = area.getX(), y = area.getY();
    if (justification.testFlags(juce::Justification::horizontallyCentred)) x += (area.getWidth() - w * unit) / 2;
    else if (justification.testFlags(juce::Justification::right)) x = area.getRight() - w * unit;
    if (justification.testFlags(juce::Justification::verticallyCentred)) y += (area.getHeight() - h * unit) / 2;
    else if (justification.testFlags(juce::Justification::bottom)) y = area.getBottom() - h * unit;
    const juce::String key = "text/" + juce::String(int(font)) + "/" + juce::String::toHexString(int(colour.getARGB())) + "/"
                             + juce::String::toHexString(int(shadow.getARGB())) + "/" + shown;
    drawCached(g, key, { x, y }, [&] {
        Canvas k(w, h);
        k.shadowText(0, 0, shown, colour, shadow, font);
        return k;
    });
}

void drawTinyText(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area, juce::Colour colour, juce::Justification justification) {
    if (text.isEmpty() || colour.getAlpha() == 0) return;
    const auto shown = fitText(text, std::max(0, area.getWidth()), Font::Small);
    if (shown.isEmpty()) return;
    const int w = Canvas::textWidth(shown, Font::Small), h = Canvas::textHeight(Font::Small);
    int x = area.getX(), y = area.getY();
    if (justification.testFlags(juce::Justification::horizontallyCentred)) x += (area.getWidth() - w) / 2;
    else if (justification.testFlags(juce::Justification::right)) x = area.getRight() - w;
    if (justification.testFlags(juce::Justification::verticallyCentred)) y += (area.getHeight() - h) / 2;
    else if (justification.testFlags(juce::Justification::bottom)) y = area.getBottom() - h;
    drawCachedSmall(g, "tiny/" + juce::String::toHexString(int(colour.getARGB())) + "/" + shown, { x, y }, [&] {
        Canvas k(w, h);
        k.text(0, 0, shown, colour, Font::Small);
        return k;
    });
}

const juce::Image& SpriteCache::get(const juce::String& key, const std::function<juce::Image()>& make) {
    if (auto it = sprites.find(key); it != sprites.end()) return it->second;
    if (int(sprites.size()) >= limit || bytes > budget) clear();   // bounded: sprites are cheap to remake
    ++made;
    auto image = make();
    bytes += int64_t(image.getWidth()) * image.getHeight() * 4;
    return sprites[key] = std::move(image);
}

SpriteCache& sharedCache() {
    static SpriteCache cache(4096);
    return cache;
}

} // namespace wb::ui::pixel
