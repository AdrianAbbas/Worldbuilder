#include "Pixel.h"
#include "PlaceArt.h"
#include "../Places/Place.h"
#include <cmath>

namespace wb::ui::pixel {

// ---- Moods -------------------------------------------------------------------------

float moodOf(const float* presence, int count) {
    float sum = 0, weight = 0;
    for (int type = NoPlace + 1; type < std::min(count, int(PlaceTypeCount)); ++type) {
        const float w = presence[type];
        if (w <= 0) continue;
        const int category = placeInfo(type).category;
        const float mood = category == SciFi ? 1.0f : category == Modern ? 0.5f : category == Nature ? 0.25f : 0.0f;
        sum += w * mood;
        weight += w;
    }
    return weight > 0 ? sum / weight : blendMood;
}

namespace {
juce::Colour c(uint32_t rgb) { return juce::Colour(0xff000000 | rgb); }

// The three keyframes. Faces stay dark and inks light in all of them, so text keeps
// its contrast at every point between them.
Palette fantasy() {
    Palette p;
    p.outline = c(0x24140c);
    p.rim = c(0xd29a3c); p.rimLight = c(0xffd772); p.rimDark = c(0x8c5a1e);              // gold trim
    p.inlay = c(0xffe7a0);
    p.face = c(0x7a4826); p.faceLight = c(0x93592c); p.faceDark = c(0x56301a);          // warm wood
    p.well = c(0x2e1a0e); p.wellLight = c(0x48301c);
    p.accent = c(0xffc23a); p.accentDim = c(0x5c3c1c); p.glow = c(0xfff0a8);             // amber gems
    p.ink = c(0xfff0cc); p.inkDim = c(0xc8a87c); p.inkOn = c(0x2a160a);
    p.panel = c(0x3a2416); p.panelLight = c(0x4c3020); p.panelDark = c(0x2a180e);       // planks
    p.sparkle = 1;
    return p;
}
Palette sciFi() {
    Palette p;
    p.outline = c(0x04060c);
    p.rim = c(0x3c485c); p.rimLight = c(0x64748e); p.rimDark = c(0x222a38);              // gunmetal
    p.inlay = c(0x38eaff);
    p.face = c(0x172030); p.faceLight = c(0x24324a); p.faceDark = c(0x0d121c);
    p.well = c(0x05080e); p.wellLight = c(0x111a28);
    p.accent = c(0x3ef0ff); p.accentDim = c(0x103444); p.glow = c(0xb4faff);             // cyan LEDs
    p.ink = c(0xc4f6ff); p.inkDim = c(0x6a8aa2); p.inkOn = c(0x03141c);
    p.panel = c(0x0b1019); p.panelLight = c(0x162234); p.panelDark = c(0x060910);
    p.scanline = 1;
    return p;
}
Palette magitech() {
    Palette p;
    p.outline = c(0x160a12);
    p.rim = c(0x84502c); p.rimLight = c(0xb0743e); p.rimDark = c(0x4e2a18);              // dark wood frames
    p.inlay = c(0x46f2d2);                                                               // neon inlays
    p.face = c(0x3a2236); p.faceLight = c(0x56344e); p.faceDark = c(0x24121f);          // plum lacquer
    p.well = c(0x140810); p.wellLight = c(0x281424);
    p.accent = c(0xc77bff); p.accentDim = c(0x3c1e4c); p.glow = c(0xecc8ff);             // glowing runes
    p.ink = c(0xffe8f2); p.inkDim = c(0xb491aa); p.inkOn = c(0x1c0826);
    p.panel = c(0x1f1220); p.panelLight = c(0x2e1c2e); p.panelDark = c(0x140a14);
    p.runes = 1;
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

// ---- The pixel font ----------------------------------------------------------------

namespace {
// Original 5 x 7 glyphs ('#' ink), drawn for this plugin.
struct Glyph {
    char c;
    const char* rows;   // seven rows of five, separated by '|'
};
constexpr Glyph glyphs[] {
    { 'A', ".###.|#...#|#...#|#####|#...#|#...#|#...#" }, { 'B', "####.|#...#|#...#|####.|#...#|#...#|####." },
    { 'C', ".###.|#...#|#....|#....|#....|#...#|.###." }, { 'D', "####.|#...#|#...#|#...#|#...#|#...#|####." },
    { 'E', "#####|#....|#....|####.|#....|#....|#####" }, { 'F', "#####|#....|#....|####.|#....|#....|#...." },
    { 'G', ".###.|#...#|#....|#.###|#...#|#...#|.####" }, { 'H', "#...#|#...#|#...#|#####|#...#|#...#|#...#" },
    { 'I', ".###.|..#..|..#..|..#..|..#..|..#..|.###." }, { 'J', "..###|...#.|...#.|...#.|#..#.|#..#.|.##.." },
    { 'K', "#...#|#..#.|#.#..|##...|#.#..|#..#.|#...#" }, { 'L', "#....|#....|#....|#....|#....|#....|#####" },
    { 'M', "#...#|##.##|#.#.#|#.#.#|#...#|#...#|#...#" }, { 'N', "#...#|##..#|#.#.#|#..##|#...#|#...#|#...#" },
    { 'O', ".###.|#...#|#...#|#...#|#...#|#...#|.###." }, { 'P', "####.|#...#|#...#|####.|#....|#....|#...." },
    { 'Q', ".###.|#...#|#...#|#...#|#.#.#|#..#.|.##.#" }, { 'R', "####.|#...#|#...#|####.|#.#..|#..#.|#...#" },
    { 'S', ".####|#....|#....|.###.|....#|....#|####." }, { 'T', "#####|..#..|..#..|..#..|..#..|..#..|..#.." },
    { 'U', "#...#|#...#|#...#|#...#|#...#|#...#|.###." }, { 'V', "#...#|#...#|#...#|#...#|#...#|.#.#.|..#.." },
    { 'W', "#...#|#...#|#...#|#.#.#|#.#.#|#.#.#|.#.#." }, { 'X', "#...#|#...#|.#.#.|..#..|.#.#.|#...#|#...#" },
    { 'Y', "#...#|#...#|.#.#.|..#..|..#..|..#..|..#.." }, { 'Z', "#####|....#|...#.|..#..|.#...|#....|#####" },
    { '0', ".###.|#...#|#..##|#.#.#|##..#|#...#|.###." }, { '1', "..#..|.##..|..#..|..#..|..#..|..#..|.###." },
    { '2', ".###.|#...#|....#|...#.|..#..|.#...|#####" }, { '3', "####.|....#|....#|.###.|....#|....#|####." },
    { '4', "...#.|..##.|.#.#.|#..#.|#####|...#.|...#." }, { '5', "#####|#....|####.|....#|....#|#...#|.###." },
    { '6', ".###.|#....|#....|####.|#...#|#...#|.###." }, { '7', "#####|....#|...#.|..#..|.#...|.#...|.#..." },
    { '8', ".###.|#...#|#...#|.###.|#...#|#...#|.###." }, { '9', ".###.|#...#|#...#|.####|....#|....#|.###." },
    { '.', ".....|.....|.....|.....|.....|.##..|.##.." }, { ',', ".....|.....|.....|.....|.##..|..#..|.#..." },
    { ':', ".....|.##..|.##..|.....|.##..|.##..|....." }, { '-', ".....|.....|.....|.###.|.....|.....|....." },
    { '+', ".....|..#..|..#..|#####|..#..|..#..|....." }, { '/', "....#|....#|...#.|..#..|.#...|#....|#...." },
    { '%', "##...|##..#|...#.|..#..|.#...|#..##|...##" }, { '<', "...#.|..#..|.#...|#....|.#...|..#..|...#." },
    { '>', ".#...|..#..|...#.|....#|...#.|..#..|.#..." }, { '!', "..#..|..#..|..#..|..#..|..#..|.....|..#.." },
    { '?', ".###.|#...#|....#|...#.|..#..|.....|..#.." }, { '\'', "..#..|..#..|.#...|.....|.....|.....|....." },
    { '&', ".##..|#..#.|#.#..|.#...|#.#.#|#..#.|.##.#" }, { '#', ".#.#.|.#.#.|#####|.#.#.|#####|.#.#.|.#.#." },
    { '(', "...#.|..#..|.#...|.#...|.#...|..#..|...#." }, { ')', ".#...|..#..|...#.|...#.|...#.|..#..|.#..." },
};
const Glyph* glyphFor(juce::juce_wchar ch) {
    ch = juce::CharacterFunctions::toUpperCase(ch);
    for (const auto& g : glyphs)
        if (juce::juce_wchar(g.c) == ch) return &g;
    return nullptr;
}
constexpr int advance = 6;
} // namespace

// ---- Canvas ------------------------------------------------------------------------

Canvas::Canvas(int width, int height) : image(juce::Image::ARGB, std::max(1, width), std::max(1, height), true), w(width), h(height) {}

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
    for (int j = y; j < y + height; ++j)
        for (int i = x; i < x + width; ++i) set(i, j, colour);
}
void Canvas::hline(int x0, int x1, int y, juce::Colour colour) {
    for (int i = std::min(x0, x1); i <= std::max(x0, x1); ++i) set(i, y, colour);
}
void Canvas::vline(int x, int y0, int y1, juce::Colour colour) {
    for (int j = std::min(y0, y1); j <= std::max(y0, y1); ++j) set(x, j, colour);
}
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
    for (int j = 0; j < other.getHeight(); ++j)
        for (int i = 0; i < other.getWidth(); ++i) blend(x + i, y + j, other.getPixelAt(i, j));
}

int Canvas::textWidth(const juce::String& text) { return text.isEmpty() ? 0 : text.length() * advance - 1; }

void Canvas::text(int x, int y, const juce::String& text, juce::Colour colour) {
    for (auto it = text.getCharPointer(); !it.isEmpty(); ++x) {
        if (const auto* g = glyphFor(it.getAndAdvance()))
            for (int row = 0, k = 0; row < 7; ++row, ++k)
                for (int col = 0; col < 5; ++col, ++k)
                    if (g->rows[k] == '#') blend(x + col, y + row, colour);
        x += advance - 1;
    }
}
void Canvas::shadowText(int x, int y, const juce::String& s, juce::Colour colour, juce::Colour shadow) {
    text(x + 1, y + 1, s, shadow);
    text(x, y, s, colour);
}
void Canvas::centredText(int y, const juce::String& s, juce::Colour colour, juce::Colour shadow) {
    shadowText((w - textWidth(s)) / 2, y, s, colour, shadow);
}

juce::Image Canvas::scaled(int scale) const {
    scale = std::max(1, scale);
    juce::Image out(juce::Image::ARGB, w * scale, h * scale, true);
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

void draw(juce::Graphics& g, const Canvas& canvas, juce::Point<float> topLeft, int scale) {
    g.drawImageAt(canvas.scaled(scale), juce::roundToInt(topLeft.x), juce::roundToInt(topLeft.y));
}

const juce::Image& SpriteCache::get(const juce::String& key, const std::function<juce::Image()>& make) {
    if (auto it = sprites.find(key); it != sprites.end()) return it->second;
    if (int(sprites.size()) >= limit) sprites.clear();   // simple and bounded: sprites are cheap to remake
    ++made;
    return sprites[key] = make();
}

// ---- Shared pieces -----------------------------------------------------------------

namespace {
juce::Colour alpha(juce::Colour colour, float a) { return colour.withMultipliedAlpha(juce::jlimit(0.0f, 1.0f, a)); }

// A raised, bevelled box: outline with cut corners, a lit top-left edge, a two-pixel
// shade along the bottom and right. Pressed boxes swap light and shade.
void bevel(Canvas& k, int x, int y, int w, int h, juce::Colour light, juce::Colour body, juce::Colour shade, juce::Colour outline, bool pressed) {
    k.box(x, y, w, h, body, outline);
    const auto top = pressed ? shade : light, bottom = pressed ? body : shade;
    k.hline(x + 1, x + w - 2, y + 1, top);
    k.vline(x + 1, y + 1, y + h - 2, top);
    k.hline(x + 1, x + w - 2, y + h - 2, bottom);
    if (!pressed && h > 8) k.hline(x + 2, x + w - 2, y + h - 3, bottom);
    k.vline(x + w - 2, y + 2, y + h - 2, bottom);
    k.set(x + 1, y + h - 2, pressed ? body : shade);
}

// A soft one-pixel glow around a cut-corner box (hover, active).
void glowAround(Canvas& k, int x, int y, int w, int h, juce::Colour colour) {
    for (int i = x + 1; i < x + w - 1; ++i) {
        k.blend(i, y - 1, colour);
        k.blend(i, y + h, colour);
    }
    for (int j = y + 1; j < y + h - 1; ++j) {
        k.blend(x - 1, j, colour);
        k.blend(x + w, j, colour);
    }
    k.blend(x, y, colour); k.blend(x + w - 1, y, colour); k.blend(x, y + h - 1, colour); k.blend(x + w - 1, y + h - 1, colour);
}

// A four-point sparkle (fantasy); `frame` makes it twinkle between a plus and a dot.
void sparkle(Canvas& k, int x, int y, juce::Colour colour, int frame) {
    k.blend(x, y, colour);
    if (frame % 3 != 2) {
        k.blend(x - 1, y, alpha(colour, 0.7f)); k.blend(x + 1, y, alpha(colour, 0.7f));
        k.blend(x, y - 1, alpha(colour, 0.7f)); k.blend(x, y + 1, alpha(colour, 0.7f));
    }
    if (frame % 3 == 0) {
        k.blend(x - 2, y, alpha(colour, 0.35f)); k.blend(x + 2, y, alpha(colour, 0.35f));
        k.blend(x, y - 2, alpha(colour, 0.35f)); k.blend(x, y + 2, alpha(colour, 0.35f));
    }
}

// HUD corner brackets just outside a box (sci-fi).
void brackets(Canvas& k, int x, int y, int w, int h, juce::Colour colour, int gap = 2) {
    const int l = x - gap, t = y - gap, r = x + w - 1 + gap, b = y + h - 1 + gap;
    for (int d = 0; d < 3; ++d) {
        k.blend(l + d, t, colour); k.blend(l, t + d, colour);
        k.blend(r - d, t, colour); k.blend(r, t + d, colour);
        k.blend(l + d, b, colour); k.blend(l, b - d, colour);
        k.blend(r - d, b, colour); k.blend(r, b - d, colour);
    }
}

// A small rune (blend): one of four 3 x 3 marks, like a circuit node.
void rune(Canvas& k, int x, int y, juce::Colour colour, int which) {
    static const char* marks[] { "#.#.#.#.#", ".#.###.#.", "##..#..##", "#..###..#" };
    const char* m = marks[which & 3];
    for (int i = 0; i < 9; ++i)
        if (m[i] == '#') k.blend(x + i % 3, y + i / 3, colour);
}

// Scanlines over an area: every other row a little darker (sci-fi).
void scanlines(Canvas& k, int x, int y, int w, int h, float amount, int frame) {
    if (amount <= 0.01f) return;
    for (int j = y + (frame & 1); j < y + h; j += 2)
        for (int i = x; i < x + w; ++i)
            if (k.get(i, j).getAlpha() > 0) k.blend(i, j, juce::Colours::black.withAlpha(0.22f * amount));
}

// Decorations a hovered or active control shows, in proportion to the mood: sparkles
// (fantasy), brackets (sci-fi), a glowing rune at a corner (blend).
void hoverDecor(Canvas& k, const Palette& p, int x, int y, int w, int h, int frame) {
    glowAround(k, x, y, w, h, alpha(p.glow, 0.45f));
    if (p.sparkle > 0.05f) {
        sparkle(k, x + w, y - 1, alpha(p.glow, p.sparkle), frame);
        sparkle(k, x - 1, y + h - 1, alpha(p.glow, p.sparkle * 0.8f), frame + 1);
    }
    if (p.scanline > 0.05f) brackets(k, x, y, w, h, alpha(p.accent, p.scanline));
    if (p.runes > 0.05f) {
        k.blend(x + w, y - 1, alpha(p.inlay, p.runes));
        k.blend(x - 1, y + h, alpha(p.inlay, p.runes));
    }
}

int clampWidth(int w) { return std::max(8, w); }
} // namespace

// ---- Controls ----------------------------------------------------------------------

Canvas knob(const Palette& p, float value, const juce::String& label, const juce::String& valueText, const State& s, int frame) {
    value = juce::jlimit(0.0f, 1.0f, value);
    const int dial = knobDial;
    const int w = std::max({ dial, Canvas::textWidth(label) + 1, Canvas::textWidth(valueText) + 1 }) + 2 * margin;
    const int h = margin + Canvas::textHeight + 3 + dial + 2 + Canvas::textHeight + 1 + margin;
    Canvas k(w, h);
    const int dx = (w - dial) / 2, dy = margin + Canvas::textHeight + 3;
    const float r = float(dial) * 0.5f;
    const float body = 10.5f, rimInner = 7.5f;
    const float pointerAngle = juce::degreesToRadians(-135.0f + 270.0f * value);
    const float px = std::sin(pointerAngle), py = -std::cos(pointerAngle);
    const auto face = s.hover ? p.faceLight : p.face;
    for (int j = 0; j < dial; ++j)
        for (int i = 0; i < dial; ++i) {
            const float fx = float(i) + 0.5f - r, fy = float(j) + 0.5f - r, d = std::hypot(fx, fy);
            if (d > body + 1.0f) continue;
            const float light = d > 0 ? (-fx - fy) / (d * 1.4142f) : 0.0f;
            juce::Colour colour;
            if (d > body) colour = p.outline;
            else if (d > rimInner) colour = light > 0.35f ? p.rimLight : light < -0.35f ? p.rimDark : p.rim;
            else {
                colour = light > 0.55f && d > 3.5f ? p.faceLight : light < -0.5f && d > 3.5f ? p.faceDark : face;
                // The pointer: a chunky two-pixel line from the centre to the rim.
                const float along = fx * px + fy * py, across = std::abs(fx * py - fy * px);
                if (along > -0.5f && along < rimInner + 0.5f && across < 1.25f) colour = along > rimInner - 1.5f ? p.glow : p.ink;
                if (d < 2.0f) colour = p.rimLight;   // the centre boss
            }
            k.set(dx + i, dy + j, colour);
        }
    scanlines(k, dx + 2, dy + 2, dial - 4, dial - 4, p.scanline * 0.6f, frame);
    // The value ring: nine segments around 270 degrees (gems, LEDs or runes by mood).
    constexpr int segments = 11;
    const int lit = juce::roundToInt(value * float(segments));
    for (int n = 0; n < segments; ++n) {
        const float a = juce::degreesToRadians(-135.0f + 270.0f * float(n) / float(segments - 1));
        const int sx = dx + juce::roundToInt(r + 14.4f * std::sin(a) - 1.0f), sy = dy + juce::roundToInt(r - 14.4f * std::cos(a) - 1.0f);
        const bool on = n < lit;
        const auto colour = on ? (n == lit - 1 ? p.glow : p.accent) : p.accentDim;
        k.fill(sx, sy, 2, 2, colour);
        if (on) k.blend(sx, sy, alpha(juce::Colours::white, 0.35f * (p.sparkle + 0.4f)));   // a gem's glint
    }
    if (s.hover) {
        // A ring of glow just outside the body, then the mood's decoration.
        for (int j = 0; j < dial; ++j)
            for (int i = 0; i < dial; ++i) {
                const float d = std::hypot(float(i) + 0.5f - r, float(j) + 0.5f - r);
                if (d > body + 1.0f && d <= body + 2.0f && k.get(dx + i, dy + j).getAlpha() == 0) k.blend(dx + i, dy + j, alpha(p.glow, 0.35f));
            }
        if (p.sparkle > 0.05f) sparkle(k, dx + dial - 2, dy + 1, alpha(p.glow, p.sparkle), frame);
        if (p.scanline > 0.05f) brackets(k, dx, dy, dial, dial, alpha(p.accent, p.scanline), 0);
        if (p.runes > 0.05f) rune(k, dx + dial - 3, dy, alpha(p.inlay, p.runes), frame);
    }
    k.centredText(margin, label, s.enabled ? p.ink : p.inkDim, p.outline);
    k.centredText(dy + dial + 2, valueText, p.accent, p.outline);
    return k;
}

Canvas slider(const Palette& p, int width, float value, const juce::String& label, const juce::String& valueText, const State& s, int frame) {
    value = juce::jlimit(0.0f, 1.0f, value);
    width = clampWidth(width);
    const int handleW = 7, handleH = 11;
    const int w = width + 2 * margin, top = margin + Canvas::textHeight + 3, h = top + handleH + 1 + margin;
    Canvas k(w, h);
    k.shadowText(margin, margin, label, s.enabled ? p.ink : p.inkDim, p.outline);
    k.shadowText(margin + width - Canvas::textWidth(valueText) - 1, margin, valueText, p.accent, p.outline);
    // The track: a recessed well with notches; the filled part lit up to the handle.
    const int tx = margin, ty = top + 3, tw = width, th = 5;
    const int hx = margin + juce::roundToInt(value * float(width - handleW));
    k.box(tx, ty, tw, th, p.well, p.outline);
    k.hline(tx + 1, tx + tw - 2, ty + 1, p.well.darker(0.4f));   // inner shadow
    for (int i = tx + 4; i < tx + tw - 2; i += 6) k.set(i, ty + 2, p.wellLight);
    const int fillEnd = hx + handleW / 2;
    if (fillEnd > tx + 1) {
        k.fill(tx + 1, ty + 1, fillEnd - tx - 1, th - 2, p.accent);
        k.hline(tx + 1, fillEnd - 1, ty + 1, p.glow);
        k.hline(tx + 1, fillEnd - 1, ty + th - 2, p.accent.darker(0.35f));
        scanlines(k, tx + 1, ty + 1, fillEnd - tx - 1, th - 2, p.scanline * 0.5f, frame);
    }
    // The handle sprite: a bevelled block with an inlay stripe (gold cap / LED / rune).
    const int hy = top;
    k.fill(hx + 1, hy + handleH, handleW - 1, 1, alpha(juce::Colours::black, 0.5f));   // its shadow
    bevel(k, hx, hy, handleW, handleH, s.hover ? p.rimLight.brighter(0.1f) : p.rimLight, s.hover ? p.rimLight : p.rim, p.rimDark, p.outline, s.down);
    k.vline(hx + 3, hy + 3, hy + handleH - 4, s.hover || s.down ? p.glow : p.inlay);
    k.set(hx + 3, hy + 2, p.rimLight);
    if (s.hover) hoverDecor(k, p, hx, hy, handleW, handleH, frame);
    return k;
}

Canvas button(const Palette& p, const juce::String& text, const State& s, int width, int frame) {
    const int bw = width > 0 ? width : Canvas::textWidth(text) + 14, bh = 15;
    Canvas k(bw + 2 * margin, bh + 1 + 2 * margin);
    const int x = margin, y = margin + (s.down ? 1 : 0);
    if (!s.down) k.hline(x + 1, x + bw - 2, margin + bh, alpha(juce::Colours::black, 0.55f));   // raised: a shadow under it
    const bool lit = s.on;
    auto light = lit ? p.glow : p.faceLight, body = lit ? p.accent : p.face, shade = lit ? p.accent.darker(0.4f) : p.faceDark;
    if (s.hover && !s.down) {
        light = light.brighter(0.15f);
        body = body.interpolatedWith(light, 0.35f);
    }
    if (!s.enabled) {
        light = light.withSaturation(0.1f).darker(0.3f);
        body = body.withSaturation(0.1f).darker(0.3f);
        shade = shade.withSaturation(0.1f);
    }
    bevel(k, x, y, bw, bh, light, body, shade, p.outline, s.down);
    // Corner studs (fantasy and blend), an LED (sci-fi and blend).
    const float studs = std::min(1.0f, p.sparkle + p.runes), led = std::min(1.0f, p.scanline + p.runes * 0.6f);
    if (studs > 0.05f)
        for (auto [sx, sy] : { std::pair { x + 2, y + 2 }, { x + bw - 3, y + 2 }, { x + 2, y + bh - 4 }, { x + bw - 3, y + bh - 4 } })
            k.blend(sx, sy, alpha(p.inlay, studs));
    if (led > 0.05f) {
        const auto colour = s.hover || s.on || s.down ? p.glow : p.accentDim;
        k.fill(x + 3, y + bh / 2 - 1, 1, 2, alpha(colour, led));
    }
    scanlines(k, x + 1, y + 1, bw - 2, bh - 2, p.scanline * 0.35f, frame);
    const auto ink = lit ? p.inkOn : s.enabled ? p.ink : p.inkDim;
    const auto shadow = lit ? alpha(p.glow, 0.6f) : p.outline;
    k.shadowText(x + (bw - Canvas::textWidth(text)) / 2 + (led > 0.05f ? 1 : 0), y + (bh - Canvas::textHeight) / 2 - 1, text, ink, shadow);
    if (s.hover && !s.down) hoverDecor(k, p, x, y, bw, bh, frame);
    return k;
}

Canvas toggle(const Palette& p, const juce::String& label, const State& s, int frame) {
    const int tw = 19, th = 11;
    const int w = tw + 5 + Canvas::textWidth(label) + 1 + 2 * margin, h = th + 1 + 2 * margin;
    Canvas k(w, h);
    const int x = margin, y = margin;
    k.box(x, y, tw, th, s.on ? p.accent.darker(0.25f) : p.well, p.outline);
    k.hline(x + 1, x + tw - 2, y + 1, s.on ? p.glow : p.well.darker(0.4f));
    if (s.on) scanlines(k, x + 1, y + 1, tw - 2, th - 2, p.scanline * 0.5f, frame);
    // The LED in the track's free half: lit when on.
    const int ledX = s.on ? x + 4 : x + tw - 6;
    k.fill(ledX, y + 4, 2, 3, s.on ? p.glow : p.accentDim);
    // The knob: a bevelled block that slides from left (off) to right (on).
    const int kw = 9, kx = s.on ? x + tw - kw : x;
    k.hline(kx + 1, kx + kw - 2, y + th, alpha(juce::Colours::black, 0.5f));
    bevel(k, kx, y, kw, th, s.hover ? p.rimLight.brighter(0.1f) : p.rimLight, s.hover ? p.rimLight : p.rim, p.rimDark, p.outline, s.down);
    k.vline(kx + 3, y + 3, y + th - 4, p.rimDark);
    k.vline(kx + 5, y + 3, y + th - 4, p.rimDark);
    if (s.hover) hoverDecor(k, p, x, y, tw, th, frame);
    k.shadowText(x + tw + 5, y + (th - Canvas::textHeight) / 2, label, s.on ? p.ink : p.inkDim, p.outline);
    return k;
}

juce::Array<juce::Rectangle<int>> tabAreas(const juce::StringArray& names) {
    juce::Array<juce::Rectangle<int>> areas;
    int x = margin;
    for (const auto& name : names) {
        const int tw = Canvas::textWidth(name) + 12;
        areas.add({ x, margin, tw, 14 });
        x += tw + 1;
    }
    return areas;
}

Canvas tabs(const Palette& p, const juce::StringArray& names, int active, int hovered, int frame) {
    const auto areas = tabAreas(names);
    const int stripY = margin + 13, w = (areas.isEmpty() ? 20 : areas.getLast().getRight() + 6) + margin, h = stripY + 5 + margin;
    Canvas k(w, h);
    // The panel edge the tabs sit on.
    k.fill(margin, stripY, w - 2 * margin, 4, p.face);
    k.hline(margin, w - margin - 1, stripY, p.outline);
    k.hline(margin, w - margin - 1, stripY + 4, p.outline);
    k.hline(margin, w - margin - 1, stripY + 1, p.faceLight);
    for (int i = 0; i < areas.size(); ++i) {
        if (i == active) continue;
        const auto a = areas[i];
        const bool hover = i == hovered;
        bevel(k, a.getX(), a.getY() + 2, a.getWidth(), 12, hover ? p.face : p.faceDark.brighter(0.05f), hover ? p.faceLight.darker(0.1f) : p.faceDark,
              p.faceDark.darker(0.3f), p.outline, false);
        k.hline(a.getX(), a.getRight() - 1, stripY, p.outline);
        k.shadowText(a.getX() + 6, a.getY() + 5, names[i], hover ? p.ink : p.inkDim, p.outline);
        if (hover) {
            if (p.sparkle > 0.05f) sparkle(k, a.getRight() - 1, a.getY() + 1, alpha(p.glow, p.sparkle), frame);
            if (p.scanline > 0.05f) k.hline(a.getX() + 2, a.getRight() - 3, a.getY() + 3, alpha(p.accent, p.scanline * 0.8f));
            if (p.runes > 0.05f) k.blend(a.getRight() - 3, a.getY() + 4, alpha(p.inlay, p.runes));
        }
    }
    if (juce::isPositiveAndBelow(active, areas.size())) {
        // The active tab stands taller and opens into the panel edge (no line between).
        const auto a = areas[active];
        k.box(a.getX(), a.getY(), a.getWidth(), 15, p.face, p.outline);
        k.hline(a.getX() + 1, a.getRight() - 2, a.getY() + 1, p.faceLight);
        k.vline(a.getX() + 1, a.getY() + 1, stripY + 1, p.faceLight);
        k.vline(a.getRight() - 2, a.getY() + 2, stripY, p.faceDark);
        k.fill(a.getX() + 1, stripY, a.getWidth() - 2, 1, p.face);
        k.hline(a.getX() + 1, a.getRight() - 2, stripY + 1, p.faceLight);
        k.set(a.getX(), stripY, p.outline);
        k.set(a.getRight() - 1, stripY, p.outline);
        // An inlay along its top: gold, neon or a glowing rune line.
        k.hline(a.getX() + 2, a.getRight() - 3, a.getY() + 2, p.inlay);
        if (p.scanline > 0.05f) k.blend(a.getX() + 2, a.getY() + 2, alpha(p.glow, p.scanline));
        k.shadowText(a.getX() + 6, a.getY() + 5, names[active], p.ink, p.outline);
    }
    return k;
}

Canvas dropdown(const Palette& p, const juce::String& text, int width, const State& s, const juce::StringArray* items, int highlighted, int frame) {
    width = clampWidth(width);
    const int fh = 15, itemH = 10;
    const bool open = items != nullptr && !items->isEmpty();
    const int menuH = open ? items->size() * itemH + 4 : 0;
    Canvas k(width + 2 * margin + 1, fh + (open ? menuH - 1 : 0) + 2 * margin + 1);
    const int x = margin, y = margin;
    // The field: a recessed well with the current choice.
    k.hline(x + 1, x + width - 2, y + fh, alpha(juce::Colours::black, 0.5f));
    k.box(x, y, width, fh, p.well, s.hover ? p.inlay : p.outline);
    k.hline(x + 1, x + width - 2, y + 1, p.well.darker(0.5f));
    k.hline(x + 1, x + width - 2, y + fh - 2, p.wellLight);
    k.shadowText(x + 4, y + 4, text, s.enabled ? p.ink : p.inkDim, p.outline);
    // The arrow button at its right end.
    const int bw = 13, bx = x + width - bw;
    bevel(k, bx, y, bw, fh, s.hover ? p.rimLight.brighter(0.1f) : p.rimLight, s.hover ? p.rimLight : p.rim, p.rimDark, p.outline, open || s.down);
    const int ax = bx + 4, ay = y + 6 + (open || s.down ? 1 : 0);
    for (int row = 0; row < 3; ++row) k.hline(ax + row, ax + 4 - row, open ? ay + 2 - row : ay + row, p.outline);
    if (s.hover) hoverDecor(k, p, x, y, width, fh, frame);
    if (open) {
        // The open menu: a panel of choices, the highlighted one lit.
        const int my = y + fh - 1;
        k.fill(x + 2, my + menuH, width - 2, 1, alpha(juce::Colours::black, 0.5f));
        k.box(x, my, width, menuH, p.face, p.outline);
        k.hline(x + 1, x + width - 2, my + 1, p.faceDark);
        for (int i = 0; i < items->size(); ++i) {
            const int iy = my + 2 + i * itemH;
            const bool hi = i == highlighted, current = (*items)[i] == text;
            if (hi) {
                k.fill(x + 2, iy, width - 4, itemH, p.accent);
                k.hline(x + 2, x + width - 3, iy, p.glow);
                scanlines(k, x + 2, iy, width - 4, itemH, p.scanline * 0.4f, frame);
            }
            if (current) {   // a small diamond marks the current choice
                const auto mark = hi ? p.inkOn : p.inlay;
                k.set(x + 5, iy + 3, mark); k.hline(x + 4, x + 6, iy + 4, mark); k.set(x + 5, iy + 5, mark);
            }
            k.shadowText(x + 9, iy + 2, (*items)[i], hi ? p.inkOn : p.ink, hi ? alpha(p.glow, 0.5f) : p.outline);
        }
    }
    return k;
}

Canvas placeCard(const Palette& p, int placeType, const juce::String& name, const State& s, int frame) {
    const int cw = iconSize + 8, ch = iconSize + 8;
    const int plaqueW = Canvas::textWidth(name) + 10, plaqueH = 11;
    const int w = std::max(cw, plaqueW) + 2 * margin, h = 2 + ch + 2 + plaqueH + 1 + margin + margin;
    Canvas k(w, h);
    const int x = (w - cw) / 2, y = margin + 2 - (s.hover ? 1 + (frame & 1) : 0);   // hovered cards bounce
    // Shadow, then the frame (gold / metal / wood), then the window.
    k.fill(x + 2, margin + 2 + ch, cw - 2, 1, alpha(juce::Colours::black, s.hover ? 0.35f : 0.55f));
    bevel(k, x, y, cw, ch, p.rimLight, p.rim, p.rimDark, p.outline, false);
    const int wx = x + 3, wy = y + 3, ww = cw - 6, wh = ch - 6;
    // The window: the Place's sky in three dithered bands, darkened toward the mood.
    const auto& theme = placeTheme(placeType);
    const auto sky = theme.sky.interpolatedWith(p.well, 0.35f), horizon = theme.horizon.interpolatedWith(p.well, 0.25f);
    for (int j = 0; j < wh; ++j)
        for (int i = 0; i < ww; ++i) {
            const float t = float(j) / float(wh - 1) * 2.0f;
            int band = int(t);
            const float frac = t - float(band);
            if (frac > 0.75f && ((i + j) & 1)) ++band;   // dither into the next band
            const auto colour = band <= 0 ? sky : band == 1 ? sky.interpolatedWith(horizon, 0.5f) : horizon;
            k.set(wx + i, wy + j, colour);
        }
    k.outlineBox(wx - 1, wy - 1, ww + 2, wh + 2, p.outline);
    scanlines(k, wx, wy, ww, wh, p.scanline * 0.5f, frame);
    k.paste(placeIcon(placeType, frame, p.outline), wx + (ww - iconSize) / 2, wy + wh - iconSize);
    // Frame decoration: gold studs (fantasy), corner LEDs (sci-fi), neon inlay (blend).
    if (p.sparkle > 0.05f)
        for (auto [sx, sy] : { std::pair { x + 1, y + 1 }, { x + cw - 2, y + 1 }, { x + 1, y + ch - 3 }, { x + cw - 2, y + ch - 3 } })
            k.blend(sx, sy, alpha(p.inlay, p.sparkle));
    if (p.scanline > 0.05f) {
        k.blend(x + cw - 3, y + ch - 2, alpha(s.on || (frame & 2) ? p.glow : p.accentDim, p.scanline));
        k.blend(x + cw - 5, y + ch - 2, alpha(p.accentDim, p.scanline));
    }
    if (p.runes > 0.05f) {
        k.hline(wx, wx + ww - 1, wy - 2, alpha(p.inlay, p.runes * 0.8f));
        k.hline(wx, wx + ww - 1, wy + wh + 1, alpha(p.inlay, p.runes * 0.5f));
    }
    if (s.on) {
        glowAround(k, x, y, cw, ch, p.accent);
        glowAround(k, x - 1, y - 1, cw + 2, ch + 2, alpha(p.glow, 0.4f));
        if (p.scanline > 0.05f) brackets(k, x, y, cw, ch, alpha(p.accent, p.scanline), 3);
    } else if (s.hover) {
        hoverDecor(k, p, x, y, cw, ch, frame);
    }
    // The name plaque.
    const int px = (w - plaqueW) / 2, py = margin + 2 + ch + 2;
    k.hline(px + 1, px + plaqueW - 2, py + plaqueH, alpha(juce::Colours::black, 0.5f));
    bevel(k, px, py, plaqueW, plaqueH, s.on ? p.glow : p.faceLight, s.on ? p.accent : p.face, s.on ? p.accent.darker(0.4f) : p.faceDark, p.outline, false);
    k.shadowText(px + 5, py + 2, name, s.on ? p.inkOn : p.ink, s.on ? alpha(p.glow, 0.5f) : p.outline);
    return k;
}

Canvas panel(const Palette& p, int width, int height, int frame) {
    Canvas k(width, height);
    k.fill(0, 0, width, height, p.panel);
    // Fantasy: wooden planks with grain. Sci-fi: a dot grid with scanlines. Blend:
    // circuit traces between runes, in the neon inlay.
    const float planks = p.sparkle, grid = p.scanline, traces = p.runes;
    if (planks > 0.05f) {
        for (int y = 11; y < height; y += 12) {
            k.hline(0, width - 1, y, alpha(p.panelDark, planks));
            k.hline(0, width - 1, y + 1, alpha(p.panelLight, planks * 0.6f));
            const int seam = ((y / 12) * 37) % std::max(1, width - 20) + 10;
            k.vline(seam, y - 10, y - 1, alpha(p.panelDark, planks * 0.8f));
        }
        juce::Random rnd(7);
        for (int n = 0; n < width * height / 90; ++n) {
            const int gx = rnd.nextInt(width - 6), gy = rnd.nextInt(height);
            k.hline(gx, gx + 2 + rnd.nextInt(5), gy, alpha(p.panelLight, planks * 0.45f));
        }
    }
    if (grid > 0.05f) {
        for (int y = 3; y < height; y += 8)
            for (int x = 3; x < width; x += 8) k.blend(x, y, alpha(p.panelLight.brighter(0.4f), grid));
        for (int y = frame & 1; y < height; y += 2) k.hline(0, width - 1, y, alpha(juce::Colours::black, grid * 0.18f));
    }
    if (traces > 0.05f) {
        juce::Random rnd(11);
        for (int n = 0; n < width * height / 3200; ++n) {
            int x = 6 + rnd.nextInt(std::max(1, width - 30)), y = 6 + rnd.nextInt(std::max(1, height - 30));
            const auto colour = alpha(p.inlay, traces * 0.16f);
            const int run = 6 + rnd.nextInt(14), drop = 4 + rnd.nextInt(10);
            k.hline(x, x + run, y, colour);
            k.vline(x + run, y, y + drop, colour);
            rune(k, x - 3, y - 1, alpha(p.inlay, traces * 0.35f), n);
            k.blend(x + run, y + drop + 1, alpha(p.accent, traces * 0.5f));
        }
    }
    // The frame: outline, a rim in the mood's material, and corner decoration.
    k.outlineBox(0, 0, width, height, p.outline);
    k.hline(1, width - 2, 1, p.rimLight);
    k.vline(1, 1, height - 2, p.rimLight);
    k.hline(1, width - 2, height - 2, p.rimDark);
    k.vline(width - 2, 2, height - 2, p.rimDark);
    k.hline(2, width - 3, 2, p.rim);
    k.vline(2, 2, height - 3, p.rim);
    k.hline(2, width - 3, height - 3, p.rim);
    k.vline(width - 3, 2, height - 3, p.rim);
    k.hline(3, width - 4, 3, p.outline);
    k.vline(3, 3, height - 4, p.outline);
    k.hline(3, width - 4, height - 4, p.outline);
    k.vline(width - 4, 3, height - 4, p.outline);
    for (auto [cx, cy] : { std::pair { 2, 2 }, { width - 4, 2 }, { 2, height - 4 }, { width - 4, height - 4 } }) {
        if (planks > 0.05f) { k.fill(cx, cy, 2, 2, alpha(p.inlay, planks)); k.blend(cx, cy, alpha(juce::Colours::white, planks * 0.5f)); }
        if (grid > 0.05f) k.fill(cx, cy, 2, 2, alpha(p.accent, grid));
        if (traces > 0.05f) k.fill(cx, cy, 2, 2, alpha(p.inlay, traces));
    }
    return k;
}

// ---- The prototype sheets ----------------------------------------------------------

namespace {
juce::String moodName(float mood) { return mood < 0.25f ? "FANTASY" : mood > 0.75f ? "SCI-FI" : "BLEND"; }
struct Demo {
    int type;
    const char* name;
};
const Demo demoPlaces[] { { Castle, "CASTLE" }, { Tavern, "TAVERN" }, { Forest, "FOREST" }, { Spaceport, "SPACEPORT" }, { Megacity, "MEGACITY" }, { Nebula, "NEBULA" } };
} // namespace

Canvas prototypeSheet(float mood, int frame) {
    const auto p = palette(mood);
    constexpr int width = 212, height = 380;
    auto k = panel(p, width, height, frame);
    const juce::String title = moodName(mood);
    // A heading banner.
    const int bw = Canvas::textWidth(title) + 16;
    bevel(k, (width - bw) / 2, 8, bw, 13, p.faceLight, p.face, p.faceDark, p.outline, false);
    k.hline((width - bw) / 2 + 2, (width + bw) / 2 - 3, 10, p.inlay);
    k.centredText(12, title, p.accent, p.outline);
    if (p.sparkle > 0.05f) { sparkle(k, (width - bw) / 2 - 4, 13, alpha(p.glow, p.sparkle), frame); sparkle(k, (width + bw) / 2 + 3, 13, alpha(p.glow, p.sparkle), frame + 1); }
    if (p.scanline > 0.05f) brackets(k, (width - bw) / 2, 8, bw, 13, alpha(p.accent, p.scanline), 3);
    if (p.runes > 0.05f) { rune(k, (width - bw) / 2 - 7, 11, alpha(p.inlay, p.runes), 1); rune(k, (width + bw) / 2 + 4, 11, alpha(p.inlay, p.runes), 3); }

    int y = 26;
    // Knobs: at rest and hovered.
    const auto k1 = knob(p, 0.62f, "TEMP", "62%", {}, frame);
    const auto k2 = knob(p, 0.25f, "DANGER", "25%", { true }, frame);
    k.paste(k1, 8, y);
    k.paste(k2, 8 + k1.width() + 2, y);
    // Toggles: off and on (with hover).
    const int tx = 8 + k1.width() + k2.width() + 10;
    k.paste(toggle(p, "SYNC", {}, frame), tx, y + 14);
    k.paste(toggle(p, "LOOP", { false, false, true }, frame), tx, y + 34);
    y += k1.height() + 2;
    // The slider.
    k.paste(slider(p, 186, 0.7f, "TONE", "+3.5 DB", {}, frame), 10, y);
    y += 30;
    // The button: normal, hover, pressed.
    int x = 8;
    for (auto s : { State {}, State { true }, State { true, true } }) {
        const auto b = button(p, "RENDER", s, 0, frame);
        k.paste(b, x, y);
        x += b.width() + 3;
    }
    y += 26;
    // The tabs (hover on the third).
    k.paste(tabs(p, { "BELLS", "HORNS", "DRONE" }, 0, 2, frame), 8, y);
    y += 28;
    // The dropdown: closed (hovered) and open.
    const juce::StringArray moods { "MINOR", "PHRYGIAN", "LYDIAN", "DORIAN" };
    k.paste(dropdown(p, "PHRYGIAN", 90, { true }, nullptr, -1, frame), 8, y);
    k.paste(dropdown(p, "LYDIAN", 90, {}, &moods, 1, frame), 108, y);
    y += 62;
    // The six Place cards: Spaceport selected, Tavern hovered.
    for (int i = 0; i < 6; ++i) {
        State s;
        s.on = demoPlaces[i].type == Spaceport;
        s.hover = demoPlaces[i].type == Tavern;
        const auto card = placeCard(p, demoPlaces[i].type, demoPlaces[i].name, s, frame);
        k.paste(card, 8 + (i % 3) * 66 + (66 - card.width()) / 2, y + (i / 3) * 66);
    }
    return k;
}

Canvas prototypeComparison(int frame) {
    const auto f = prototypeSheet(fantasyMood, frame), b = prototypeSheet(blendMood, frame), s = prototypeSheet(sciFiMood, frame);
    const int gap = 6, top = 18;
    Canvas k(f.width() * 3 + gap * 4, f.height() + top + gap);
    k.fill(0, 0, k.width(), k.height(), juce::Colour(0xff0c0a12));
    k.shadowText(gap, 6, "WORLDBUILDER  8-BIT STYLE PROTOTYPE  (13A)", juce::Colour(0xffe8e0f0), juce::Colours::black);
    k.paste(f, gap, top);
    k.paste(b, gap * 2 + f.width(), top);
    k.paste(s, gap * 3 + f.width() * 2, top);
    return k;
}

Canvas moodSweep(int frame) {
    const float moods[] { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    const int column = 74, width = column * 5 + 4, height = 212;
    Canvas k(width, height);
    k.fill(0, 0, width, height, juce::Colour(0xff0c0a12));
    for (int i = 0; i < 5; ++i) {
        const auto p = palette(moods[i]);
        const int x = 2 + i * column;
        k.paste(panel(p, column - 2, height - 4, frame), x, 2);
        const juce::String label = juce::String(juce::roundToInt(moods[i] * 100)) + "% SCI-FI";
        k.shadowText(x + (column - 2 - Canvas::textWidth(label)) / 2, 8, label, p.ink, p.outline);
        const auto b = button(p, "PLAY", { true }, 0, frame);
        k.paste(b, x + (column - 2 - b.width()) / 2, 20);
        const auto kn = knob(p, 0.5f, "MIX", "50%", {}, frame);
        k.paste(kn, x + (column - 2 - kn.width()) / 2, 44);
        const auto card = placeCard(p, i < 2 ? Castle : i == 2 ? Tavern : Nebula, i < 2 ? "CASTLE" : i == 2 ? "TAVERN" : "NEBULA", {}, frame);
        k.paste(card, x + (column - 2 - card.width()) / 2, 112);
        const auto t = toggle(p, "ON", { false, false, true }, frame);
        k.paste(t, x + (column - 2 - t.width()) / 2, 180);
    }
    return k;
}

Canvas iconStrips() {
    const float moods[] { fantasyMood, blendMood, sciFiMood };
    const int cell = iconSize + 4, labelW = 62, maxFrames = 4;
    const int groupW = cell * maxFrames + 8;
    Canvas k(labelW + groupW * 3 + 4, 14 + 6 * cell + 4);
    k.fill(0, 0, k.width(), k.height(), juce::Colour(0xff0c0a12));
    for (int m = 0; m < 3; ++m) {
        const auto p = palette(moods[m]);
        const int gx = labelW + m * groupW;
        k.fill(gx, 12, groupW - 6, 6 * cell + 2, p.panel);
        k.shadowText(gx + 2, 3, moodName(moods[m]), p.accent, juce::Colours::black);
        for (int i = 0; i < 6; ++i) {
            const int type = demoPlaces[i].type;
            if (m == 0) k.shadowText(2, 14 + i * cell + 13, demoPlaces[i].name, juce::Colour(0xffe8e0f0), juce::Colours::black);
            for (int f = 0; f < placeIconFrames(type); ++f) k.paste(placeIcon(type, f, p.outline), gx + 2 + f * cell, 14 + i * cell);
        }
    }
    return k;
}

} // namespace wb::ui::pixel
