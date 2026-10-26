#include "Pixel.h"
#include "PlaceArt.h"
#include "Style.h"
#include "../Places/Place.h"
#include <cmath>

// The control sprites of the 16-bit style, drawn at any size (in art pixels), and the
// review sheets. Faces and panels are shaded with ordered dithering (lit and a little
// warm at the top, shaded and a little cool at the bottom), as SNES-era menus were.
// Each mood shows its own decoration in proportion: fantasy sparkles, little stars and
// mint curls; blend runes and neon; sci-fi LEDs and HUD brackets; neutral nothing but
// clean lines. Nothing is drawn over text (no scanlines).

namespace wb::ui::pixel {

namespace {
juce::Colour alpha(juce::Colour colour, float a) { return colour.withMultipliedAlpha(juce::jlimit(0.0f, 1.0f, a)); }
juce::Colour or_(juce::Colour colour, juce::Colour fallback) { return colour.isTransparent() ? fallback : colour; }

// The 4 x 4 ordered-dither threshold (0..1) at a pixel.
float bayer4(int x, int y) {
    static constexpr int m[16] { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
    return (float(m[(y & 3) * 4 + (x & 3)]) + 0.5f) / 16.0f;
}

// A vertical gradient over the opaque pixels of an area, from `top` through `middle` to
// `bottom`, in five dithered bands (the 16-bit way to shade without banding).
void ditherShade(Canvas& k, int x, int y, int w, int h, juce::Colour top, juce::Colour middle, juce::Colour bottom) {
    if (w <= 0 || h <= 1) return;
    for (int j = 0; j < h; ++j) {
        const float t = float(j) / float(h - 1) * 4.0f;   // 0..4: five bands
        for (int i = 0; i < w; ++i) {
            if (k.get(x + i, y + j).getAlpha() < 255) continue;
            int band = int(t);
            if (t - float(band) > bayer4(x + i, y + j)) ++band;
            band = std::min(band, 4);
            const auto c = band < 2 ? top.interpolatedWith(middle, float(band) / 2.0f) : middle.interpolatedWith(bottom, float(band - 2) / 2.0f);
            k.set(x + i, y + j, c);
        }
    }
}

// A face's highlight and shade, hue-shifted as 16-bit palettes were: lights a touch
// warmer, shades a touch cooler.
juce::Colour warmLight(juce::Colour body, juce::Colour light) { return body.interpolatedWith(light, 0.45f).interpolatedWith(juce::Colour(0xfffff0c0), 0.06f); }
juce::Colour coolShade(juce::Colour body, juce::Colour shade) { return body.interpolatedWith(shade, 0.45f).interpolatedWith(juce::Colour(0xff203060), 0.08f); }

// A raised, bevelled box: outline with cut corners, a dithered face (lit at the top,
// shaded at the bottom), a lit top-left edge, a shade along the bottom and right.
// Pressed boxes swap light and shade.
void bevel(Canvas& k, int x, int y, int w, int h, juce::Colour light, juce::Colour body, juce::Colour shade, juce::Colour outline, bool pressed) {
    if (w < 3 || h < 3) {
        k.fill(x, y, w, h, body);
        return;
    }
    k.box(x, y, w, h, body, outline);
    if (h > 5) {
        if (pressed) ditherShade(k, x + 1, y + 1, w - 2, h - 2, coolShade(body, shade), body, body);
        else ditherShade(k, x + 1, y + 1, w - 2, h - 2, warmLight(body, light), body, coolShade(body, shade));
    }
    const auto top = pressed ? shade : light, bottom = pressed ? body : shade;
    k.hline(x + 1, x + w - 2, y + 1, top);
    k.vline(x + 1, y + 1, y + h - 2, top);
    k.hline(x + 1, x + w - 2, y + h - 2, bottom);
    if (!pressed && h > 10) k.hline(x + 2, x + w - 2, y + h - 3, bottom);
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

// A four-point sparkle; `frame` makes it twinkle between a big plus, a plus and a dot.
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

// A little five-pixel star (fantasy whimsy).
void star(Canvas& k, int x, int y, juce::Colour colour) {
    k.blend(x, y - 1, colour);
    k.blend(x - 1, y, colour); k.blend(x, y, colour); k.blend(x + 1, y, colour);
    k.blend(x - 1, y + 1, alpha(colour, 0.8f)); k.blend(x + 1, y + 1, alpha(colour, 0.8f));
}

// A mint curl at a frame's corner (fantasy): a tiny spiral, mirrored per corner.
void curl(Canvas& k, int x, int y, int dx, int dy, juce::Colour colour) {
    static const char* m = "###|..#|#.#";   // read with the corner's mirroring
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < 3; ++i)
            if (m[j * 4 + i] == '#') k.blend(x + i * dx, y + j * dy, colour);
}

// A tiny flower (fantasy panels): petals around a sunny centre.
void flower(Canvas& k, int x, int y, juce::Colour petal, juce::Colour centre) {
    k.blend(x, y - 1, petal); k.blend(x - 1, y, petal); k.blend(x + 1, y, petal); k.blend(x, y + 1, petal);
    k.blend(x, y, centre);
}

// HUD corner brackets just outside a box (sci-fi).
void brackets(Canvas& k, int x, int y, int w, int h, juce::Colour colour, int gap = 2, int arm = 3) {
    const int l = x - gap, t = y - gap, r = x + w - 1 + gap, b = y + h - 1 + gap;
    for (int d = 0; d < arm; ++d) {
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

// Scanlines are gone with the 16-bit style (they lay over text and blurred small
// labels); faces are shaded by dithering instead (bevel). Kept as a no-op so the sprites
// that called it read the same.
void scanlines(Canvas&, int, int, int, int, float, int) {}

// What a hovered control shows, in proportion to the mood: a glow; sparkles and a star
// (fantasy), brackets (sci-fi), glowing rune points (blend).
// The small delights follow the Overlays setting (pixel::theme().delights): Off keeps
// only the glow.
bool delights() { return theme().delights > 0; }

void hoverDecor(Canvas& k, const Palette& p, int x, int y, int w, int h, int frame) {
    glowAround(k, x, y, w, h, alpha(p.glow, 0.45f));
    if (!delights()) return;
    if (p.sparkle > 0.05f) {
        sparkle(k, x + w, y - 1, alpha(p.glow, p.sparkle), frame);
        if (h > 8) star(k, x - 1, y + h - 1, alpha(p.inlay, p.sparkle * 0.9f));
    }
    if (p.scanline > 0.05f) brackets(k, x, y, w, h, alpha(p.accent, p.scanline), 2, h > 8 ? 3 : 2);
    if (p.runes > 0.05f) {
        k.blend(x + w, y - 1, alpha(p.inlay, p.runes));
        k.blend(x - 1, y + h, alpha(p.inlay, p.runes));
    }
}

int clampWidth(int w) { return std::max(8, w); }
Font fontFor(int height) { return height >= 13 ? Font::Large : Font::Small; }
// Large text when it fits the room and the height, else the Small font.
Font fontFitting(const juce::String& text, int room, int height) {
    return height >= 13 && Canvas::textWidth(text, Font::Large) <= room ? Font::Large : Font::Small;
}
} // namespace

// ---- Knobs -------------------------------------------------------------------------

Canvas knob(const Palette& p, int dial, float value, const juce::String& label, const juce::String& valueText, const State& s, int frame, juce::Colour accent) {
    value = juce::jlimit(0.0f, 1.0f, value);
    dial = std::max(10, dial);
    const auto lit = or_(accent, p.accent), dim = accent.isTransparent() ? p.accentDim : accent.interpolatedWith(p.well, 0.7f);
    const auto small = Font::Small;
    const int labelH = label.isEmpty() ? 0 : Canvas::textHeight(small) + 3, valueH = valueText.isEmpty() ? 0 : Canvas::textHeight(small) + 3;
    const int w = std::max({ dial, Canvas::textWidth(label, small) + 1, Canvas::textWidth(valueText, small) + 1 }) + 2 * margin;
    const int h = margin + labelH + dial + valueH + margin;
    Canvas k(w, h);
    const int dx = (w - dial) / 2, dy = margin + labelH;
    const float r = float(dial) * 0.5f;
    const bool big = dial >= 24;
    const float body = r - (big ? 6.5f : 4.0f), rimInner = body - (big ? 3.0f : 2.0f);
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
                colour = light > 0.55f && d > rimInner * 0.45f ? p.faceLight : light < -0.5f && d > rimInner * 0.45f ? p.faceDark : face;
                // The pointer: a chunky line from the centre to the rim, its tip lit.
                const float along = fx * px + fy * py, across = std::abs(fx * py - fy * px);
                if (along > -0.5f && along < rimInner + 0.5f && across < (big ? 1.25f : 0.75f)) colour = along > rimInner - 1.5f ? p.glow : p.ink;
                if (d < (big ? 2.0f : 1.2f)) colour = p.rimLight;   // the centre boss
            }
            if (!s.enabled) colour = colour.withSaturation(colour.getSaturation() * 0.2f).darker(0.25f);
            k.set(dx + i, dy + j, colour);
        }
    scanlines(k, dx + 2, dy + 2, dial - 4, dial - 4, p.scanline * 0.6f, frame);
    // The value ring: segments around 270 degrees (gems, runes or LEDs by mood).
    const int segments = dial >= 30 ? 11 : dial >= 20 ? 9 : 7, size = big ? 2 : 1;
    const float ringR = r - (big ? 2.4f : 1.5f);
    const int count = juce::roundToInt(value * float(segments));
    for (int n = 0; n < segments; ++n) {
        const float a = juce::degreesToRadians(-135.0f + 270.0f * float(n) / float(segments - 1));
        const int sx = dx + juce::roundToInt(r + ringR * std::sin(a) - float(size) * 0.5f), sy = dy + juce::roundToInt(r - ringR * std::cos(a) - float(size) * 0.5f);
        const bool on = n < count && s.enabled;
        k.fill(sx, sy, size, size, on ? (n == count - 1 ? p.glow : lit) : dim);
        if (on && size > 1) k.blend(sx, sy, alpha(juce::Colours::white, 0.35f * (p.sparkle + 0.4f)));   // a gem's glint
    }
    if (s.hover) {
        for (int j = 0; j < dial; ++j)
            for (int i = 0; i < dial; ++i) {
                const float d = std::hypot(float(i) + 0.5f - r, float(j) + 0.5f - r);
                if (d > body + 1.0f && d <= body + 2.0f && k.get(dx + i, dy + j).getAlpha() == 0) k.blend(dx + i, dy + j, alpha(p.glow, 0.35f));
            }
    }
    if (s.hover && delights()) {
        if (p.sparkle > 0.05f) {
            sparkle(k, dx + dial - 2, dy + 1, alpha(p.glow, p.sparkle), frame);
            if (big) star(k, dx + 1, dy + dial - 2, alpha(p.inlay, p.sparkle));
        }
        if (p.scanline > 0.05f) brackets(k, dx, dy, dial, dial, alpha(lit, p.scanline), 0, big ? 3 : 2);
        if (p.runes > 0.05f) rune(k, dx + dial - 3, dy, alpha(p.inlay, p.runes), frame);
    }
    if (labelH > 0) k.centredText(margin, label, s.enabled ? p.ink : p.inkDim, p.outline, small);
    if (valueH > 0) k.centredText(dy + dial + 2, valueText, s.enabled ? lit : p.inkDim, p.outline, small);
    return k;
}

Canvas intensityKnob(const Palette& p, int dial, float value, const State& s, int frame) {
    value = juce::jlimit(0.0f, 1.0f, value);
    dial = std::max(16, dial);
    Canvas k(dial + 2 * margin, dial + 2 * margin);
    const int dx = margin, dy = margin;
    const float r = float(dial) * 0.5f, body = r - 4.5f, rimInner = body - 2.5f;
    // A halo that warms with the value: the cue heating up.
    for (int j = -margin; j < dial + margin; ++j)
        for (int i = -margin; i < dial + margin; ++i) {
            const float d = std::hypot(float(i) + 0.5f - r, float(j) + 0.5f - r);
            if (d > r - 0.5f && d < r + 2.5f) k.blend(dx + i, dy + j, alpha(p.accent, (0.08f + 0.3f * value) * (1.0f - (d - r + 0.5f) / 3.0f)));
        }
    // The ring: every segment lit up to the value, from cool (the inlay) through the
    // accent to the peak colour.
    constexpr int segments = 16;
    const int count = juce::roundToInt(value * float(segments));
    for (int n = 0; n < segments; ++n) {
        const float t = float(n) / float(segments - 1);
        const float a = juce::degreesToRadians(-150.0f + 300.0f * t);
        const int sx = dx + juce::roundToInt(r + (r - 2.2f) * std::sin(a) - 1.0f), sy = dy + juce::roundToInt(r - (r - 2.2f) * std::cos(a) - 1.0f);
        const auto heat = t < 0.5f ? p.inlay.interpolatedWith(p.accent, t * 2.0f) : p.accent.interpolatedWith(p.peak, (t - 0.5f) * 2.0f);
        k.fill(sx, sy, 2, 2, n < count ? (n == count - 1 ? p.glow : heat) : p.accentDim);
    }
    // The body: rim, face glowing toward the accent with the value, pointer.
    const float pa = juce::degreesToRadians(-150.0f + 300.0f * value), px = std::sin(pa), py = -std::cos(pa);
    const auto face = p.face.interpolatedWith(p.accent, 0.12f + 0.35f * value);
    for (int j = 0; j < dial; ++j)
        for (int i = 0; i < dial; ++i) {
            const float fx = float(i) + 0.5f - r, fy = float(j) + 0.5f - r, d = std::hypot(fx, fy);
            if (d > body + 1.0f) continue;
            const float light = d > 0 ? (-fx - fy) / (d * 1.4142f) : 0.0f;
            juce::Colour colour;
            if (d > body) colour = p.outline;
            else if (d > rimInner) colour = light > 0.35f ? p.rimLight : light < -0.35f ? p.rimDark : p.rim;
            else {
                colour = light > 0.5f && d > 4 ? face.brighter(0.25f) : light < -0.5f && d > 4 ? face.darker(0.35f) : face;
                const float along = fx * px + fy * py, across = std::abs(fx * py - fy * px);
                if (along > 3.0f && along < rimInner + 0.5f && across < 1.25f) colour = along > rimInner - 1.5f ? p.glow : p.ink;
            }
            k.set(dx + i, dy + j, colour);
        }
    // The core emblem by mood: a star (fantasy), a rune (blend), a reactor ring (sci-fi),
    // a dot (neutral); brighter with the value.
    const int cx = dx + int(r), cy = dy + int(r);
    const auto core = p.glow.interpolatedWith(juce::Colours::white, value * 0.5f);
    k.fill(cx - 1, cy - 1, 2, 2, alpha(core, 0.6f + 0.4f * value));
    if (p.sparkle > 0.05f) { star(k, cx, cy, alpha(core, p.sparkle)); sparkle(k, cx - 1, cy - 1, alpha(core, p.sparkle * value), frame); }
    if (p.runes > 0.05f) rune(k, cx - 2, cy - 2, alpha(p.inlay, p.runes), 1);
    if (p.scanline > 0.05f) {
        k.outlineBox(cx - 3, cy - 3, 6, 6, alpha(p.accent, p.scanline * (0.5f + 0.5f * value)));
        scanlines(k, dx + 3, dy + 3, dial - 6, dial - 6, p.scanline * 0.5f, frame);
    }
    if (s.hover && delights()) {
        if (p.sparkle > 0.05f) sparkle(k, dx + dial - 1, dy + 1, alpha(p.glow, p.sparkle), frame);
        if (p.scanline > 0.05f) brackets(k, dx, dy, dial, dial, alpha(p.accent, p.scanline), 1);
    }
    return k;
}

// ---- Sliders, bars, fields, meters --------------------------------------------------

Canvas slider(const Palette& p, int length, float value, bool vertical, const State& s, int frame, juce::Colour accent, int thickness) {
    value = juce::jlimit(0.0f, 1.0f, value);
    length = clampWidth(length);
    const auto lit = or_(accent, p.accent);
    const int handleLong = juce::jlimit(5, 11, thickness), handleShort = handleLong >= 9 ? 7 : 5, track = handleLong >= 9 ? 5 : 3;
    const int across = handleLong + 1;   // with the handle's shadow
    Canvas k(vertical ? across + 2 * margin : length + 2 * margin, vertical ? length + 2 * margin : across + 2 * margin);
    const int travel = length - handleShort;
    const int pos = juce::roundToInt(value * float(travel));
    if (!vertical) {
        const int tx = margin, ty = margin + (handleLong - track) / 2;
        k.box(tx, ty, length, track, p.well, p.outline);
        k.hline(tx + 1, tx + length - 2, ty + 1, p.well.darker(0.4f));
        for (int i = tx + 4; i < tx + length - 2; i += 6) k.set(i, ty + track / 2, p.wellLight);
        const int end = tx + pos + handleShort / 2;
        if (end > tx + 1 && s.enabled) {
            k.fill(tx + 1, ty + 1, end - tx - 1, track - 2, lit);
            k.hline(tx + 1, end - 1, ty + 1, p.glow.interpolatedWith(lit, accent.isTransparent() ? 0.0f : 0.5f));
            k.hline(tx + 1, end - 1, ty + track - 2, lit.darker(0.35f));
            scanlines(k, tx + 1, ty + 1, end - tx - 1, track - 2, p.scanline * 0.5f, frame);
        }
        const int hx = tx + pos, hy = margin;
        k.fill(hx + 1, hy + handleLong, handleShort - 1, 1, alpha(juce::Colours::black, 0.5f));
        bevel(k, hx, hy, handleShort, handleLong, s.hover ? p.rimLight.brighter(0.1f) : p.rimLight, s.hover ? p.rimLight : p.rim, p.rimDark, p.outline, s.down);
        k.vline(hx + handleShort / 2, hy + 3, hy + handleLong - 4, s.hover || s.down ? p.glow : p.inlay);
        k.set(hx + handleShort / 2, hy + 2, p.rimLight);
        if (s.hover) hoverDecor(k, p, hx, hy, handleShort, handleLong, frame);
    } else {
        const int tx = margin + (handleLong - track) / 2, ty = margin;
        k.box(tx, ty, track, length, p.well, p.outline);
        k.vline(tx + 1, ty + 1, ty + length - 2, p.well.darker(0.4f));
        for (int j = ty + 4; j < ty + length - 2; j += 6) k.set(tx + track / 2, j, p.wellLight);
        const int top = ty + length - 1 - pos - handleShort / 2;
        if (top < ty + length - 2 && s.enabled) {
            k.fill(tx + 1, top, track - 2, ty + length - 1 - top, lit);
            k.vline(tx + 1, top, ty + length - 2, p.glow.interpolatedWith(lit, accent.isTransparent() ? 0.0f : 0.5f));
            k.vline(tx + track - 2, top, ty + length - 2, lit.darker(0.35f));
        }
        const int hx = margin, hy = ty + length - handleShort - pos;
        k.fill(hx + 1, hy + handleShort, handleLong - 1, 1, alpha(juce::Colours::black, 0.5f));
        bevel(k, hx, hy, handleLong, handleShort, s.hover ? p.rimLight.brighter(0.1f) : p.rimLight, s.hover ? p.rimLight : p.rim, p.rimDark, p.outline, s.down);
        k.hline(hx + 3, hx + handleLong - 4, hy + handleShort / 2, s.hover || s.down ? p.glow : p.inlay);
        k.set(hx + 2, hy + handleShort / 2, p.rimLight);
        if (s.hover) hoverDecor(k, p, hx, hy, handleLong, handleShort, frame);
    }
    return k;
}

Canvas labelledSlider(const Palette& p, int width, float value, const juce::String& label, const juce::String& valueText, const State& s, int frame) {
    const auto track = slider(p, width, value, false, s, frame);
    const int top = Canvas::textHeight(Font::Small) + 3;
    Canvas k(track.width(), track.height() + top);
    k.shadowText(margin, margin, label, s.enabled ? p.ink : p.inkDim, p.outline, Font::Small);
    k.shadowText(margin + width - Canvas::textWidth(valueText, Font::Small) - 1, margin, valueText, p.accent, p.outline, Font::Small);
    k.paste(track, 0, top);
    return k;
}

Canvas field(const Palette& p, int width, int height, bool hover) {
    width = std::max(4, width);
    height = std::max(4, height);
    Canvas k(width, height);
    k.box(0, 0, width, height, p.well, hover ? p.inlay : p.outline);
    k.hline(1, width - 2, 1, p.well.darker(0.5f));
    if (height > 5) k.hline(1, width - 2, height - 2, p.wellLight);
    return k;
}

Canvas bar(const Palette& p, int width, int height, float value, const juce::String& text, const State& s) {
    auto k = field(p, width, height, s.hover);
    const int fillW = juce::roundToInt(juce::jlimit(0.0f, 1.0f, value) * float(width - 2));
    if (fillW > 0) {
        k.fill(1, 2, fillW, height - 3, p.accentDim.interpolatedWith(p.accent, 0.35f));
        k.hline(1, fillW, 2, p.accentDim.interpolatedWith(p.accent, 0.6f));
    }
    const auto font = fontFor(height);
    const auto shown = fitText(text, width - 4, font);
    k.shadowText((width - Canvas::textWidth(shown, font)) / 2, (height - Canvas::textHeight(font)) / 2, shown, s.enabled ? p.ink : p.inkDim, p.outline, font);
    return k;
}

Canvas meter(const Palette& p, int length, int thickness, float level, bool vertical) {
    length = std::max(6, length);
    thickness = std::max(3, thickness);
    Canvas k(vertical ? thickness : length, vertical ? length : thickness);
    k.box(0, 0, k.width(), k.height(), p.well, p.outline);
    // Two-pixel LED segments with one-pixel gaps: the accent, then warm, then the peak.
    const int segments = (length - 2) / 3;
    const int lit = juce::roundToInt(juce::jlimit(0.0f, 1.0f, level) * float(segments));
    for (int n = 0; n < segments; ++n) {
        const float t = float(n + 1) / float(segments);
        const auto on = t > 0.9f ? p.peak : t > 0.7f ? p.warn : p.accent;
        const auto colour = n < lit ? on : on.interpolatedWith(p.well, 0.82f);
        const int a = 1 + n * 3 + 1;
        if (vertical) k.fill(1, length - 1 - a - 1, thickness - 2, 2, colour);
        else k.fill(a, 1, 2, thickness - 2, colour);
        if (n < lit && n == lit - 1) {
            if (vertical) k.blend(1, length - a - 2, alpha(juce::Colours::white, 0.35f));
            else k.blend(a, 1, alpha(juce::Colours::white, 0.35f));
        }
    }
    return k;
}

// ---- Buttons, toggles, tabs, chips, dropdowns ---------------------------------------

Canvas button(const Palette& p, const juce::String& text, const State& s, int width, int height, int frame, const Canvas* icon, juce::Colour accent) {
    height = std::max(5, height);
    const int iconW = icon != nullptr ? icon->width() + (text.isEmpty() ? 0 : 3) : 0;
    const auto font = width > 0 ? fontFitting(text, width - 6 - iconW, height) : fontFor(height);
    const int bw = width > 0 ? width : Canvas::textWidth(text, font) + iconW + 12, bh = height;
    Canvas k(bw + 2 * margin, bh + 1 + 2 * margin);
    const int x = margin, y = margin + (s.down ? 1 : 0);
    if (!s.down) k.hline(x + 1, x + bw - 2, margin + bh, alpha(juce::Colours::black, 0.55f));   // raised: a shadow under it
    const auto lit = or_(accent, p.accent);
    auto light = s.on ? lit.interpolatedWith(p.glow, 0.5f) : p.faceLight, body = s.on ? lit : p.face, shade = s.on ? lit.darker(0.4f) : p.faceDark;
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
    // Corner studs (fantasy and blend), an LED (sci-fi and blend), on roomy buttons.
    const bool roomy = bw >= 20 && bh >= 11;
    const float studs = std::min(1.0f, p.sparkle + p.runes), led = std::min(1.0f, p.scanline + p.runes * 0.6f);
    if (roomy && studs > 0.05f)
        for (auto [sx, sy] : { std::pair { x + 2, y + 2 }, { x + bw - 3, y + 2 }, { x + 2, y + bh - 4 }, { x + bw - 3, y + bh - 4 } })
            k.blend(sx, sy, alpha(p.inlay, studs));
    const bool lightFace = s.on && lit.getPerceivedBrightness() > 0.45f;
    const auto ink = !s.enabled ? p.inkDim : s.on ? (lightFace ? p.inkOn : p.ink) : p.ink;
    const auto shadow = s.on && lightFace ? alpha(p.glow, 0.5f) : p.outline;
    // Text first: the icon goes when there is no room for both, the LED when tight.
    const int textW = Canvas::textWidth(text, font);
    const bool withIcon = icon != nullptr && (text.isEmpty() || textW + iconW + 4 <= bw);
    const int contentNeed = (withIcon ? iconW : 0) + textW;
    const bool showLed = roomy && led > 0.05f && contentNeed + 8 <= bw;
    if (showLed) k.fill(x + 3, y + bh / 2 - 1, 1, 2, alpha(s.hover || s.on || s.down ? p.glow : p.accentDim, led));
    scanlines(k, x + 1, y + 1, bw - 2, bh - 2, p.scanline * 0.35f, frame);
    const int room = bw - 3 - (showLed ? 2 : 0) - (withIcon ? iconW : 0);
    const auto shown = fitText(text, std::max(0, room), font);
    const int contentW = (withIcon ? iconW : 0) + (shown.isEmpty() ? 0 : Canvas::textWidth(shown, font));
    int cx = x + (bw - contentW) / 2 + (showLed ? 1 : 0);
    if (withIcon) {
        // An icon sprite drawn white is tinted to the ink.
        Canvas tinted(icon->width(), icon->height());
        for (int j = 0; j < icon->height(); ++j)
            for (int i = 0; i < icon->width(); ++i) {
                const auto c = icon->get(i, j);
                if (c.getAlpha() == 0) continue;
                tinted.set(i, j, c.getBrightness() > 0.5f ? ink.withMultipliedAlpha(c.getFloatAlpha()) : shadow.withMultipliedAlpha(c.getFloatAlpha()));
            }
        k.paste(tinted, cx, y + (bh - icon->height()) / 2);
        cx += iconW;
    }
    if (shown.isNotEmpty()) k.shadowText(cx, y + (bh - Canvas::textHeight(font)) / 2 - (font == Font::Large ? 1 : 0), shown, ink, shadow, font);
    if (s.hover && !s.down) hoverDecor(k, p, x, y, bw, bh, frame);
    return k;
}

Canvas toggle(const Palette& p, const juce::String& label, const State& s, int frame) {
    const int tw = 19, th = 11;
    const auto font = Font::Small;
    const int w = tw + 5 + Canvas::textWidth(label, font) + 1 + 2 * margin, h = th + 1 + 2 * margin;
    Canvas k(w, h);
    const int x = margin, y = margin;
    k.box(x, y, tw, th, s.on ? p.accent.darker(0.25f) : p.well, p.outline);
    k.hline(x + 1, x + tw - 2, y + 1, s.on ? p.glow : p.well.darker(0.4f));
    if (s.on) scanlines(k, x + 1, y + 1, tw - 2, th - 2, p.scanline * 0.5f, frame);
    const int ledX = s.on ? x + 4 : x + tw - 6;
    k.fill(ledX, y + 4, 2, 3, s.on ? p.glow : p.accentDim);
    const int kw = 9, kx = s.on ? x + tw - kw : x;
    k.hline(kx + 1, kx + kw - 2, y + th, alpha(juce::Colours::black, 0.5f));
    bevel(k, kx, y, kw, th, s.hover ? p.rimLight.brighter(0.1f) : p.rimLight, s.hover ? p.rimLight : p.rim, p.rimDark, p.outline, s.down);
    k.vline(kx + 3, y + 3, y + th - 4, p.rimDark);
    k.vline(kx + 5, y + 3, y + th - 4, p.rimDark);
    if (s.hover) hoverDecor(k, p, x, y, tw, th, frame);
    k.shadowText(x + tw + 5, y + (th - Canvas::textHeight(font)) / 2, label, s.enabled ? (s.on ? p.ink : p.inkDim) : p.inkDim.darker(0.3f), p.outline, font);
    return k;
}

Canvas tab(const Palette& p, const juce::String& text, int width, int height, bool active, bool hover, const Canvas* icon, juce::Colour accent) {
    width = std::max(8, width);
    height = std::max(8, height);
    Canvas k(width, height);
    const int iconW = icon != nullptr ? icon->width() + 2 : 0;
    // Tabs keep one size of text across a strip: Large only on tall tabs.
    const auto font = height >= 18 ? fontFitting(text, width - 8 - iconW, height - 2) : Font::Small;
    const auto line = or_(accent, p.inlay);
    const auto shown = fitText(text, width - 8 - iconW, font);
    const int contentW = iconW + Canvas::textWidth(shown, font);
    const int top = active ? 0 : 2;
    const int th = height - top;
    if (active) {
        // The active tab is taller, opens into the panel below (no bottom line).
        k.fill(1, 1, width - 2, height - 1, p.face);
        k.hline(1, width - 2, 0, p.outline);
        k.vline(0, 1, height - 1, p.outline);
        k.vline(width - 1, 1, height - 1, p.outline);
        k.hline(1, width - 2, 1, p.faceLight);
        k.vline(1, 1, height - 1, p.faceLight);
        k.vline(width - 2, 2, height - 1, p.faceDark);
        k.hline(2, width - 3, 2, line);
    } else {
        bevel(k, 0, top, width, th + 1, hover ? p.face : p.faceDark.brighter(0.05f), hover ? p.faceLight.darker(0.1f) : p.faceDark, p.faceDark.darker(0.3f), p.outline, false);
        k.hline(0, width - 1, height - 1, p.outline);
        k.hline(2, width - 3, top + 1, alpha(line, 0.35f));
        if (hover && delights() && p.sparkle > 0.05f) k.blend(width - 3, top + 2, alpha(p.glow, p.sparkle));
        if (hover && delights() && p.scanline > 0.05f) k.hline(2, width - 3, top + 2, alpha(p.accent, p.scanline * 0.8f));
    }
    const auto ink = active || hover ? p.ink : p.inkDim;
    int cx = (width - contentW) / 2;
    const int cy = top + (th - Canvas::textHeight(font)) / 2 + (active ? 1 : 0);
    if (icon != nullptr) {
        Canvas tinted(icon->width(), icon->height());
        for (int j = 0; j < icon->height(); ++j)
            for (int i = 0; i < icon->width(); ++i)
                if (const auto c = icon->get(i, j); c.getAlpha() > 0) tinted.set(i, j, c.getBrightness() > 0.5f ? (active ? or_(accent, ink) : ink) : p.outline);
        k.paste(tinted, cx, top + (th - icon->height()) / 2 + (active ? 1 : 0));
        cx += iconW;
    }
    k.shadowText(cx, cy, shown, ink, p.outline, font);
    return k;
}

juce::Array<juce::Rectangle<int>> tabAreas(const juce::StringArray& names) {
    juce::Array<juce::Rectangle<int>> areas;
    int x = margin;
    for (const auto& name : names) {
        const int tw = Canvas::textWidth(name, Font::Large) + 12;
        areas.add({ x, margin, tw, 14 });
        x += tw + 1;
    }
    return areas;
}

Canvas tabs(const Palette& p, const juce::StringArray& names, int active, int hovered, int frame) {
    const auto areas = tabAreas(names);
    const int stripY = margin + 13, w = (areas.isEmpty() ? 20 : areas.getLast().getRight() + 6) + margin, h = stripY + 5 + margin;
    Canvas k(w, h);
    k.fill(margin, stripY, w - 2 * margin, 4, p.face);
    k.hline(margin, w - margin - 1, stripY, p.outline);
    k.hline(margin, w - margin - 1, stripY + 4, p.outline);
    k.hline(margin, w - margin - 1, stripY + 1, p.faceLight);
    for (int i = 0; i < areas.size(); ++i) {
        const auto a = areas[i];
        const auto t = tab(p, names[i], a.getWidth(), 15, i == active, i == hovered);
        k.paste(t, a.getX(), a.getY());
        if (i == hovered && p.sparkle > 0.05f) sparkle(k, a.getRight() - 1, a.getY() + 2, alpha(p.glow, p.sparkle), frame);
    }
    if (juce::isPositiveAndBelow(active, areas.size())) {
        const auto a = areas[active];
        k.fill(a.getX() + 1, stripY, a.getWidth() - 2, 1, p.face);
        k.hline(a.getX() + 1, a.getRight() - 2, stripY + 1, p.faceLight);
    }
    return k;
}

Canvas chip(const Palette& p, const juce::String& text, int width, int height, const State& s, juce::Colour accent) {
    width = std::max(6, width);
    height = std::max(5, height);
    Canvas k(width, height);
    const auto lit = or_(accent, p.accent);
    const auto body = s.on ? lit.interpolatedWith(p.face, 0.25f) : s.hover ? p.faceLight : p.face;
    k.box(0, 0, width, height, body, s.hover ? p.glow.interpolatedWith(p.outline, 0.4f) : p.outline);
    k.hline(1, width - 2, 1, s.on ? lit.brighter(0.3f) : body.brighter(0.15f));
    if (!s.on) k.vline(1, 1, height - 2, alpha(lit, 0.9f));   // a stripe of the chip's colour
    const bool lightFace = s.on && body.getPerceivedBrightness() > 0.45f;
    const auto font = fontFitting(text, width - 4, height - 2);
    const auto shown = fitText(text, width - 3, font);
    k.shadowText((width - Canvas::textWidth(shown, font)) / 2 + (s.on ? 0 : 1), (height - Canvas::textHeight(font)) / 2, shown,
                 !s.enabled ? p.inkDim : lightFace ? p.inkOn : p.ink, lightFace ? alpha(p.glow, 0.5f) : p.outline, font);
    return k;
}

Canvas dropdown(const Palette& p, const juce::String& text, int width, const State& s, const juce::StringArray* items, int highlighted, int frame, int height) {
    width = clampWidth(width);
    const int fh = std::max(7, height), itemH = 10;
    const auto font = fontFitting(text, width - std::min(13, fh - 2) - 5, fh - 2);
    const bool open = items != nullptr && !items->isEmpty();
    const int menuH = open ? items->size() * itemH + 4 : 0;
    Canvas k(width + 2 * margin + 1, fh + (open ? menuH - 1 : 0) + 2 * margin + 1);
    const int x = margin, y = margin;
    k.hline(x + 1, x + width - 2, y + fh, alpha(juce::Colours::black, 0.5f));
    k.paste(field(p, width, fh, s.hover), x, y);
    const int bw = std::min(13, fh - 2), bx = x + width - bw;
    k.shadowText(x + 3, y + (fh - Canvas::textHeight(font)) / 2, fitText(text, width - bw - 5, font), s.enabled ? p.ink : p.inkDim, p.outline, font);
    bevel(k, bx, y, bw, fh, s.hover ? p.rimLight.brighter(0.1f) : p.rimLight, s.hover ? p.rimLight : p.rim, p.rimDark, p.outline, open || s.down);
    const int aw = bw >= 9 ? 5 : 3, ax = bx + (bw - aw) / 2, ay = y + (fh - (aw + 1) / 2) / 2 + (open || s.down ? 1 : 0);
    for (int row = 0; row < (aw + 1) / 2; ++row) k.hline(ax + row, ax + aw - 1 - row, open ? ay + (aw + 1) / 2 - 1 - row : ay + row, p.outline);
    if (s.hover) hoverDecor(k, p, x, y, width, fh, frame);
    if (open) {
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
            if (current) {
                const auto mark = hi ? p.inkOn : p.inlay;
                k.set(x + 5, iy + 3, mark); k.hline(x + 4, x + 6, iy + 4, mark); k.set(x + 5, iy + 5, mark);
            }
            k.shadowText(x + 9, iy + 2, (*items)[i], hi ? p.inkOn : p.ink, hi ? alpha(p.glow, 0.5f) : p.outline, Font::Large);
        }
    }
    return k;
}

Canvas strip(const Palette& p, int width, int height, juce::Colour accent, bool selected) {
    width = std::max(8, width);
    height = std::max(8, height);
    Canvas k(width, height + 1);
    const auto face = p.face.interpolatedWith(accent, selected ? 0.2f : 0.1f);
    k.hline(1, width - 2, height, alpha(juce::Colours::black, 0.45f));
    bevel(k, 0, 0, width, height, face.brighter(0.18f), face, face.darker(0.35f), selected ? accent : p.outline, false);
    k.fill(1, 1, 2, height - 2, accent);
    k.vline(1, 1, height - 2, accent.brighter(0.35f));
    if (selected) glowAround(k, 0, 0, width, height, alpha(accent, 0.6f));
    if (p.sparkle > 0.05f) curl(k, width - 2, 1, -1, 1, alpha(p.inlay, p.sparkle * 0.8f));
    if (p.scanline > 0.05f) k.blend(width - 3, height - 3, alpha(accent, p.scanline));
    return k;
}

Canvas badge(const Palette& p, const juce::String& text, juce::Colour accent) {
    const int w = std::max(7, Canvas::textWidth(text, Font::Small) + 4), h = 7;
    Canvas k(w + 1, h + 1);
    k.fill(1, h, w - 1, 1, alpha(juce::Colours::black, 0.5f));
    k.box(0, 0, w, h, or_(accent, p.accent), p.outline);
    k.hline(1, w - 2, 1, or_(accent, p.accent).brighter(0.4f));
    k.text((w - Canvas::textWidth(text, Font::Small) + 1) / 2, 1, text, p.inkOn, Font::Small);
    return k;
}

Canvas marker(const Palette& p, juce::Colour colour, bool used, bool here) {
    Canvas k(9, 9);
    static const char* gem = "..###..|.#####.|#######|#######|#######|.#####.|..###..";
    int row = 0, col = 0;
    for (const char* c = gem; *c != 0; ++c) {
        if (*c == '|') { ++row; col = 0; continue; }
        if (*c == '#') {
            const bool edge = row == 0 || row == 6 || col == 0 || col == 6 || (row == 1 && (col == 1 || col == 5)) || (row == 5 && (col == 1 || col == 5));
            if (used) k.set(col + 1, row + 1, edge ? p.outline : (row <= 2 && col <= 3 ? colour.brighter(0.4f) : colour));
            else if (edge) k.set(col + 1, row + 1, p.inkDim);
        }
        ++col;
    }
    if (here) {
        k.blend(0, 4, alpha(colour, 0.7f)); k.blend(8, 4, alpha(colour, 0.7f)); k.blend(4, 0, alpha(colour, 0.7f)); k.blend(4, 8, alpha(colour, 0.7f));
    }
    return k;
}

// ---- Cards and panels --------------------------------------------------------------

Canvas placeCard(const Palette& p, int placeType, const juce::String& name, const State& s, int frame, int width, int height) {
    if (width <= 0 || height <= 0) {
        // The roomy card (review sheets): the icon framed, its name on a plaque below.
        const int cw = iconSize + 8, ch = iconSize + 8;
        const int plaqueW = Canvas::textWidth(name, Font::Small) + 10, plaqueH = 9;
        const int w = std::max(cw, plaqueW) + 2 * margin, h = 2 + ch + 2 + plaqueH + 1 + margin + margin;
        Canvas k(w, h);
        const int x = (w - cw) / 2;
        k.paste(placeCard(p, placeType, {}, s, frame, cw, ch), x - margin, 2);
        const int px = (w - plaqueW) / 2, py = margin + 2 + ch + 2;
        k.hline(px + 1, px + plaqueW - 2, py + plaqueH, alpha(juce::Colours::black, 0.5f));
        bevel(k, px, py, plaqueW, plaqueH, s.on ? p.glow : p.faceLight, s.on ? p.accent : p.face, s.on ? p.accent.darker(0.4f) : p.faceDark, p.outline, false);
        k.shadowText(px + 5, py + 2, name, s.on ? p.inkOn : p.ink, s.on ? alpha(p.glow, 0.5f) : p.outline, Font::Small);
        return k;
    }
    width = std::max(16, width);
    height = std::max(16, height);
    Canvas k(width + 2 * margin, height + 2 * margin + 1);
    const int x = margin, y = margin - (s.hover && s.enabled ? 1 : 0);   // a hovered card hops up
    k.fill(x + 2, margin + height, width - 2, 1, alpha(juce::Colours::black, s.hover ? 0.35f : 0.55f));
    bevel(k, x, y, width, height, p.rimLight, p.rim, p.rimDark, p.outline, false);
    const int wx = x + 3, wy = y + 3, ww = width - 6, wh = height - 6;
    // The window: the Place's sky in three dithered bands, tempered toward the mood.
    const auto& theme = placeTheme(placeType);
    const auto sky = theme.sky.interpolatedWith(p.well, 0.35f), horizon = theme.horizon.interpolatedWith(p.well, 0.25f);
    for (int j = 0; j < wh; ++j)
        for (int i = 0; i < ww; ++i) {
            const float t = float(j) / float(std::max(1, wh - 1)) * 2.0f;
            int band = int(t);
            if (t - float(band) > 0.75f && ((i + j) & 1)) ++band;
            k.set(wx + i, wy + j, band <= 0 ? sky : band == 1 ? sky.interpolatedWith(horizon, 0.5f) : horizon);
        }
    k.outlineBox(wx - 1, wy - 1, ww + 2, wh + 2, p.outline);
    scanlines(k, wx, wy, ww, wh, p.scanline * 0.5f, frame);
    const int nameH = name.isEmpty() ? 0 : Canvas::textHeight(Font::Small) + 3;
    {
        // The icon stands on the name band (cropped to the window when small).
        const auto icon = placeIcon(placeType, frame, p.outline);
        Canvas window(ww, wh);
        window.paste(icon, (ww - iconSize) / 2, std::min(wh - iconSize, wh - nameH - iconSize + 3));
        k.paste(window, wx, wy);
    }
    if (nameH > 0) {
        k.blendFill(wx, wy + wh - nameH, ww, nameH, alpha(p.well, 0.82f));
        k.hline(wx, wx + ww - 1, wy + wh - nameH, alpha(s.on ? p.accent : p.rim, 0.8f));
        const auto shown = fitText(name, ww - 2, Font::Small);
        k.shadowText(wx + (ww - Canvas::textWidth(shown, Font::Small)) / 2, wy + wh - nameH + 2, shown, s.on ? p.glow : p.ink, p.outline, Font::Small);
    }
    // Frame decoration: mint curls (fantasy), corner LEDs (sci-fi), neon inlay (blend).
    if (p.sparkle > 0.05f) {
        const auto colour = alpha(p.inlay, p.sparkle);
        curl(k, x + 1, y + 1, 1, 1, colour);
        curl(k, x + width - 2, y + 1, -1, 1, colour);
    }
    if (p.scanline > 0.05f) {
        k.blend(x + width - 3, y + height - 2, alpha(s.on || (frame & 2) ? p.glow : p.accentDim, p.scanline));
        k.blend(x + width - 5, y + height - 2, alpha(p.accentDim, p.scanline));
    }
    if (p.runes > 0.05f) k.hline(wx, wx + ww - 1, wy - 2, alpha(p.inlay, p.runes * 0.8f));
    if (!s.enabled) k.blendFill(x, y, width, height, alpha(p.well, 0.55f));
    if (s.on) {
        glowAround(k, x, y, width, height, p.accent);
        glowAround(k, x - 1, y - 1, width + 2, height + 2, alpha(p.glow, 0.4f));
        if (p.scanline > 0.05f) brackets(k, x, y, width, height, alpha(p.accent, p.scanline), 2);
    } else if (s.hover) {
        hoverDecor(k, p, x, y, width, height, frame);
    }
    return k;
}

Canvas panel(const Palette& p, int width, int height, int frame, juce::Colour tint, float tintAmount) {
    width = std::max(12, width);
    height = std::max(12, height);
    Canvas k(width, height);
    const auto base = tint.isTransparent() ? p.panel : p.panel.interpolatedWith(tint, juce::jlimit(0.0f, 1.0f, tintAmount));
    const auto light = tint.isTransparent() ? p.panelLight : p.panelLight.interpolatedWith(tint.brighter(0.2f), juce::jlimit(0.0f, 1.0f, tintAmount));
    const auto dark = p.panelDark;
    k.fill(0, 0, width, height, base);
    // A soft dithered light from the top, as on a 16-bit menu window.
    ditherShade(k, 0, 0, width, height, base.interpolatedWith(light, 0.35f).interpolatedWith(juce::Colour(0xfffff0c0), 0.03f), base,
                base.interpolatedWith(dark, 0.4f).interpolatedWith(juce::Colour(0xff203060), 0.05f));
    // Fantasy: a mossy, speckled ground with little flowers and stars. Sci-fi: a dot
    // grid. Blend: circuit traces between runes, in the neon inlay.
    const float meadow = p.sparkle, grid = p.scanline, traces = p.runes;
    if (meadow > 0.05f) {
        juce::Random rnd(7);
        for (int n = 0; n < width * height / 18; ++n) {
            const int gx = rnd.nextInt(width), gy = rnd.nextInt(height);
            k.blend(gx, gy, alpha(rnd.nextBool() ? light : dark, meadow * 0.7f));
        }
        for (int n = 0; n < width * height / 900; ++n) {
            const int fx = 6 + rnd.nextInt(std::max(1, width - 12)), fy = 6 + rnd.nextInt(std::max(1, height - 12));
            if (rnd.nextInt(3) == 0) flower(k, fx, fy, alpha(juce::Colour(0xffff9ccf), meadow * 0.55f), alpha(p.accent, meadow * 0.6f));
            else star(k, fx, fy, alpha(p.glow, meadow * (0.18f + 0.12f * float((n + frame) % 3))));
        }
    }
    if (grid > 0.05f) {
        for (int y = 3; y < height; y += 8)
            for (int x = 3; x < width; x += 8) k.blend(x, y, alpha(light.brighter(0.4f), grid));
    }
    if (traces > 0.05f) {
        juce::Random rnd(11);
        for (int n = 0; n < width * height / 3200; ++n) {
            int x = 6 + rnd.nextInt(std::max(1, width - 30)), y = 6 + rnd.nextInt(std::max(1, height - 30));
            const auto colour = alpha(p.inlay, traces * 0.16f);
            const int run = 6 + rnd.nextInt(14), drop = 4 + rnd.nextInt(10);
            k.hline(x, std::min(width - 6, x + run), y, colour);
            k.vline(std::min(width - 6, x + run), y, std::min(height - 6, y + drop), colour);
            rune(k, x - 3, y - 1, alpha(p.inlay, traces * 0.35f), n);
        }
    }
    // The frame: outline, a rim in the mood's material, an inner outline, corner marks.
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
    if (meadow > 0.05f) {
        const auto colour = alpha(p.inlay, meadow);
        curl(k, 4, 4, 1, 1, colour);
        curl(k, width - 5, 4, -1, 1, colour);
        curl(k, 4, height - 5, 1, -1, colour);
        curl(k, width - 5, height - 5, -1, -1, colour);
    }
    for (auto [cx, cy] : { std::pair { 2, 2 }, { width - 4, 2 }, { 2, height - 4 }, { width - 4, height - 4 } }) {
        if (meadow > 0.05f) { k.blendFill(cx, cy, 2, 2, alpha(p.accent, meadow)); k.blend(cx, cy, alpha(juce::Colours::white, meadow * 0.5f)); }
        if (grid > 0.05f) k.blendFill(cx, cy, 2, 2, alpha(p.accent, grid));
        if (traces > 0.05f) k.blendFill(cx, cy, 2, 2, alpha(p.inlay, traces));
    }
    return k;
}

// ---- The review sheets -------------------------------------------------------------

namespace {
juce::String moodName(const Mood& m) {
    if (m.neutral > 0.5f) return "NEUTRAL";
    return m.sciFi < 0.25f ? "FANTASY" : m.sciFi > 0.75f ? "SCI-FI" : "BLEND";
}
struct Demo {
    int type;
};
const Demo demoPlaces[] { { Castle }, { Tavern }, { Forest }, { Spaceport }, { Megacity }, { Nebula } };
const Demo modernPlaces[] { { City }, { Suburbs }, { Mall }, { Factory }, { Beach }, { Military } };
} // namespace

Canvas prototypeSheet(const Mood& mood, int frame) {
    const auto p = palette(mood);
    constexpr int width = 212, height = 412;
    auto k = panel(p, width, height, frame);
    const juce::String title = moodName(mood);
    const int bw = Canvas::textWidth(title) + 16;
    bevel(k, (width - bw) / 2, 8, bw, 13, p.faceLight, p.face, p.faceDark, p.outline, false);
    k.hline((width - bw) / 2 + 2, (width + bw) / 2 - 3, 10, p.inlay);
    k.centredText(12, title, p.accent, p.outline);
    if (p.sparkle > 0.05f) { sparkle(k, (width - bw) / 2 - 4, 13, alpha(p.glow, p.sparkle), frame); star(k, (width + bw) / 2 + 4, 14, alpha(p.glow, p.sparkle)); }
    if (p.scanline > 0.05f) brackets(k, (width - bw) / 2, 8, bw, 13, alpha(p.accent, p.scanline), 3);
    if (p.runes > 0.05f) { rune(k, (width - bw) / 2 - 7, 11, alpha(p.inlay, p.runes), 1); rune(k, (width + bw) / 2 + 4, 11, alpha(p.inlay, p.runes), 3); }

    int y = 26;
    const auto k1 = knob(p, 34, 0.62f, "TEMPERATURE", "62%", {}, frame);
    const auto k2 = knob(p, 34, 0.25f, "DANGER", "25%", { true }, frame);
    k.paste(k1, 6, y);
    k.paste(k2, 6 + k1.width(), y);
    const auto big = intensityKnob(p, 40, 0.7f, {}, frame);
    k.paste(big, width - big.width() - 8, y + 2);
    k.shadowText(width - big.width() - 4, y + big.height() + 2, "INTENSITY", p.ink, p.outline, Font::Small);
    y += k1.height() + 2;
    k.paste(toggle(p, "SYNC", {}, frame), 8, y);
    k.paste(toggle(p, "LOOP", { false, false, true }, frame), 68, y);
    y += 20;
    k.paste(labelledSlider(p, 150, 0.7f, "TONE", "+3.5 DB", {}, frame), 6, y);
    // A vertical slider and two meters beside it.
    k.paste(slider(p, 48, 0.4f, true, {}, frame), width - 40, y - 4);
    k.paste(meter(p, 46, 5, 0.78f, true), width - 18, y - 1);
    k.paste(meter(p, 46, 5, 0.95f, true), width - 12, y - 1);
    y += 28;
    k.paste(meter(p, 150, 5, 0.6f, false), 9, y);
    y += 10;
    int x = 6;
    for (auto s : { State {}, State { true }, State { true, true }, State { false, false, true } }) {
        const auto b = button(p, s.on ? "ON" : "RENDER", s, 0, 15, frame);
        k.paste(b, x, y);
        x += b.width() - 1;
    }
    y += 24;
    k.paste(tabs(p, { "BELLS", "HORNS", "DRONE" }, 0, 2, frame), 6, y);
    y += 28;
    const juce::StringArray moods { "MINOR", "PHRYGIAN", "LYDIAN", "DORIAN" };
    k.paste(dropdown(p, "PHRYGIAN", 90, { true }, nullptr, -1, frame), 6, y);
    k.paste(dropdown(p, "LYDIAN", 90, {}, &moods, 1, frame), 108, y);
    y += 24;
    k.paste(chip(p, "FANTASY", 40, 9, {}, {}), 9, y);
    k.paste(chip(p, "SCI-FI", 36, 9, { false, false, true }, {}), 52, y);
    y += 38;
    const auto& demo = mood.neutral > 0.5f ? modernPlaces : demoPlaces;
    for (int i = 0; i < 6; ++i) {
        State s;
        s.on = i == 3;
        s.hover = i == 1;
        const auto card = placeCard(p, demo[i].type, placeShortName(demo[i].type), s, frame, 46, 44);
        k.paste(card, 10 + (i % 3) * 66 + (66 - card.width()) / 2, y + (i / 3) * 54);
    }
    return k;
}

Canvas prototypeComparison(int frame) {
    const Mood moods[] { { fantasyMood, 0 }, { blendMood, 0 }, { sciFiMood, 0 }, neutralMood };
    const int gap = 6, top = 18;
    std::vector<Canvas> sheets;
    for (const auto& m : moods) sheets.push_back(prototypeSheet(m, frame));
    Canvas k(sheets[0].width() * 4 + gap * 5, sheets[0].height() + top + gap);
    k.fill(0, 0, k.width(), k.height(), juce::Colour(0xff0c0a12));
    k.shadowText(gap, 6, "WORLDBUILDER  8-BIT STYLE  (13B)", juce::Colour(0xffe8e0f0), juce::Colours::black);
    for (int i = 0; i < 4; ++i) k.paste(sheets[std::size_t(i)], gap + i * (sheets[0].width() + gap), top);
    return k;
}

Canvas moodSweep(int frame) {
    const Mood moods[] { { 0.0f, 0 }, { 0.25f, 0 }, { 0.5f, 0 }, { 0.75f, 0 }, { 1.0f, 0 }, { 0.5f, 0.5f }, neutralMood };
    const int column = 74, count = int(std::size(moods)), width = column * count + 4, height = 214;
    Canvas k(width, height);
    k.fill(0, 0, width, height, juce::Colour(0xff0c0a12));
    for (int i = 0; i < count; ++i) {
        const auto p = palette(moods[i]);
        const int x = 2 + i * column;
        k.paste(panel(p, column - 2, height - 4, frame), x, 2);
        const juce::String label = moods[i].neutral >= 1.0f ? "MODERN" : moods[i].neutral > 0 ? "HALF MODERN" : juce::String(juce::roundToInt(moods[i].sciFi * 100)) + "% SCI-FI";
        k.shadowText(x + (column - 2 - Canvas::textWidth(label, Font::Small)) / 2, 8, label, p.ink, p.outline, Font::Small);
        const auto b = button(p, "PLAY", { true }, 0, 15, frame);
        k.paste(b, x + (column - 2 - b.width()) / 2, 18);
        const auto kn = knob(p, 30, 0.5f, "MIX", "50%", {}, frame);
        k.paste(kn, x + (column - 2 - kn.width()) / 2, 42);
        const int type = moods[i].neutral > 0.5f ? City : moods[i].sciFi < 0.3f ? Castle : moods[i].sciFi < 0.6f ? Tavern : Nebula;
        const auto card = placeCard(p, type, placeShortName(type), {}, frame, 46, 44);
        k.paste(card, x + (column - 2 - card.width()) / 2, 96);
        k.paste(meter(p, 56, 5, 0.75f, false), x + 7, 152);
        const auto t = toggle(p, "ON", { false, false, true }, frame);
        k.paste(t, x + (column - 2 - t.width()) / 2, 164);
        const auto sl = slider(p, 56, 0.6f, false, {}, frame);
        k.paste(sl, x + (column - 2 - sl.width()) / 2, 186);
    }
    return k;
}

Canvas iconStrips() {
    const int cell = iconSize + 4, labelW = 70, maxFrames = 4, rows = PlaceTypeCount;
    Canvas k(labelW + cell * maxFrames + 8, 6 + rows * cell);
    k.fill(0, 0, k.width(), k.height(), juce::Colour(0xff0c0a12));
    for (int type = 0; type < rows; ++type) {
        const int category = type == NoPlace ? MedievalFantasy : placeInfo(type).category;
        const Mood m = category == Modern ? neutralMood : Mood { category == SciFi ? sciFiMood : fantasyMood, 0 };
        const auto p = palette(m);
        const int y = 4 + type * cell;
        k.fill(labelW - 2, y - 1, cell * maxFrames + 6, cell - 1, p.panel);
        k.shadowText(2, y + 14, type == NoPlace ? "EMPTY" : placeShortName(type), juce::Colour(0xffe8e0f0), juce::Colours::black, Font::Small);
        for (int f = 0; f < placeIconFrames(type); ++f) k.paste(placeIcon(type, f, p.outline), labelW + f * cell, y);
    }
    return k;
}

Canvas iconSheet() {
    const int cols = 12, cell = 18;
    const int tools = int(Icon::Count) - 1, instruments = InstrumentIconCount;
    const int rowsTools = (tools + cols - 1) / cols, rowsInst = (instruments + cols - 1) / cols;
    Canvas k(cols * cell + 8, (rowsTools + rowsInst) * cell + 30);
    const auto p = palette(blendMood);
    k.fill(0, 0, k.width(), k.height(), p.panel);
    k.shadowText(4, 3, "TOOLS", p.accent, p.outline, Font::Small);
    for (int i = 0; i < tools; ++i) k.paste(toolIcon(i + 1, 14, p.ink, p.outline), 4 + (i % cols) * cell, 11 + (i / cols) * cell);
    const int top = 11 + rowsTools * cell + 4;
    k.shadowText(4, top, "INSTRUMENTS", p.accent, p.outline, Font::Small);
    for (int i = 0; i < instruments; ++i) k.paste(instrumentIcon(i, 14, p.ink, p.outline), 4 + (i % cols) * cell, top + 8 + (i / cols) * cell);
    return k;
}

} // namespace wb::ui::pixel
