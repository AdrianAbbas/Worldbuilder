#include "TinyPlanet.h"
#include "../DSP/Planet.h"
#include "../Places/Place.h"
#include <cmath>

namespace wb::ui {
using namespace wb::dsp;

namespace {
constexpr float pi = juce::MathConstants<float>::pi, twoPi = juce::MathConstants<float>::twoPi;
juce::Colour c(uint32_t rgb) { return juce::Colour(0xff000000 | rgb); }
juce::Colour mixc(juce::Colour a, juce::Colour b, float t) { return a.interpolatedWith(b, juce::jlimit(0.0f, 1.0f, t)); }

// A 4 x 4 ordered dither: thresholds 0..1 for smooth steps in flat pixel colours.
float bayer(int x, int y) {
    static constexpr int m[16] { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
    return (float(m[(y & 3) * 4 + (x & 3)]) + 0.5f) / 16.0f;
}
// Integer hash noise, 0..1.
float hash(int x, int y, int seed = 0) {
    uint32_t h = uint32_t(x) * 374761393u + uint32_t(y) * 668265263u + uint32_t(seed) * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return float((h ^ (h >> 16)) & 0xffffu) / 65535.0f;
}
// Smooth 1D value noise along the planet's surface.
float noise1(float x, int seed) {
    const int i = int(std::floor(x));
    const float f = x - float(i), t = f * f * (3.0f - 2.0f * f);
    return hash(i, 0, seed) * (1.0f - t) + hash(i + 1, 0, seed) * t;
}

// Each Planet's ground (four depth bands, the horizon's light first) and its sky (top,
// horizon) by day; night skies come from the interface's palette.
struct Look {
    std::array<juce::Colour, 4> ground;
    juce::Colour skyTop, skyLow, accent;
};
Look lookOf(int planet) {
    switch (planet) {
        case Ocean: return { { c(0x5cc8f0), c(0x2e8ad0), c(0x1f62a8), c(0x15427a) }, c(0x5aa8e8), c(0xbfe8ff), c(0xf2e2a0) };
        case IceGiant: return { { c(0xf2fbff), c(0xc4ecf8), c(0x8ccfe6), c(0x5aa4c8) }, c(0x7ab8e0), c(0xe0f6ff), c(0xffffff) };
        case GasGiant: return { { c(0xf0c48a), c(0xd08a5c), c(0xa45a6a), c(0x6a3c78) }, c(0x3c2a6e), c(0xc89ad8), c(0xffe2a8) };
        case DesertWorld: return { { c(0xf6d690), c(0xe2b06a), c(0xc48a4e), c(0x9a6438) }, c(0x7ab0e0), c(0xffd8a0), c(0xffe8b0) };
        case LavaWorld: return { { c(0x5a3a3a), c(0x3e2626), c(0x2c1a1c), c(0x1c1012) }, c(0x4a1422), c(0xe0603a), c(0xffa040) };
        case ForestWorld: return { { c(0x8ad06a), c(0x5aa64e), c(0x3c7a40), c(0x26543a) }, c(0x6ab0e0), c(0xd6f0d0), c(0xfff0a0) };
        default: return { { c(0xc8b8a4), c(0x9c8c7c), c(0x766a62), c(0x544c4a) }, c(0x6a80a8), c(0xe8c8a8), c(0xffe0b0) };   // Rocky
    }
}

// Forest World's open ground, at a surface point (`u` along the surface, `depth` below
// the horizon): rivers meandering down from the horizon, lakes, meadow clearings. `rim`
// gets how close the point is to the feature's edge (0 inside .. 1 at the edge).
enum ForestGround { Woods, River, Lake, Clearing };
ForestGround forestGround(float u, float depth, float& rim) {
    rim = 0;
    // A river every so often: its course wanders as it runs down the slope, widening.
    {
        const int cell = int(std::floor(u / 150.0f));
        if (hash(cell, 0, 51) > 0.35f) {
            const float course = (float(cell) + 0.3f + 0.4f * hash(cell, 1, 51)) * 150.0f + 7.0f * std::sin(depth / 9.0f + hash(cell, 2, 51) * 6.0f)
                                 + 3.0f * std::sin(depth / 3.7f);
            const float half = 1.2f + depth * 0.035f;
            const float off = std::abs(u - course);
            if (off < half + 1.0f) {
                rim = juce::jlimit(0.0f, 1.0f, off / (half + 1.0f));
                return River;
            }
        }
    }
    // Lakes: flattened toward the horizon.
    {
        const int cell = int(std::floor(u / 95.0f));
        if (hash(cell, 0, 53) > 0.2f) {
            const float cu = (float(cell) + 0.25f + 0.5f * hash(cell, 1, 53)) * 95.0f, cd = 12.0f + 40.0f * hash(cell, 2, 53);
            const float rx = 12.0f + 10.0f * hash(cell, 3, 53), ry = rx * (0.3f + std::min(0.4f, cd / 110.0f));
            const float e = ((u - cu) * (u - cu)) / (rx * rx) + ((depth - cd) * (depth - cd)) / (ry * ry);
            if (e < 1.25f) {
                rim = juce::jlimit(0.0f, 1.0f, e / 1.25f);
                return Lake;
            }
        }
    }
    // Clearings: meadows open in the woods.
    {
        const int cell = int(std::floor(u / 80.0f));
        if (hash(cell, 0, 55) > 0.35f) {
            const float cu = (float(cell) + 0.2f + 0.6f * hash(cell, 1, 55)) * 80.0f, cd = 6.0f + 50.0f * hash(cell, 2, 55);
            const float rx = 10.0f + 9.0f * hash(cell, 3, 55), ry = rx * (0.35f + std::min(0.4f, cd / 100.0f));
            const float e = ((u - cu) * (u - cu)) / (rx * rx) + ((depth - cd) * (depth - cd)) / (ry * ry);
            if (e < 1.0f) {
                rim = e;
                return Clearing;
            }
        }
    }
    return Woods;
}

// The ground at a surface point: `u` (art pixels along the surface, turning with the
// planet), `depth` (art pixels below the horizon), `edge` (0 centre .. 1 the side).
juce::Colour groundAt(int planet, const Look& look, float u, float depth, int px, int py, float time, int frame) {
    const int ui = int(std::floor(u));
    // Depth bands, dithered into each other; nearer the horizon the bands are thinner
    // (the curve foreshortens them).
    const float bandSize = 7.0f + depth * 0.18f;
    const float b = depth / bandSize;
    int band = int(b);
    if (b - float(band) > bayer(px, py)) ++band;
    auto col = look.ground[std::size_t(std::min(band, 3))];
    switch (planet) {
        case Ocean: {
            // Wave crests drifting along, islands here and there.
            const float row = std::fmod(depth + 1000.0f, 6.0f);
            if (row < 1.0f && hash(int(std::floor((u + time * 6.0f) / 3.0f)), int(depth / 6.0f), 3) > 0.55f) col = look.ground[0].brighter(0.35f);
            // Round islands: a sandy ring around a green middle, flattened toward the horizon.
            const int cell = int(std::floor(u / 70.0f));
            if (hash(cell, 0, 11) > 0.45f) {
                const float cu = (float(cell) + 0.25f + 0.5f * hash(cell, 1, 11)) * 70.0f, cd = 14.0f + 40.0f * hash(cell, 2, 11);
                const float rx = 6.0f + 6.0f * hash(cell, 3, 11), ry = rx * (0.3f + std::min(0.4f, cd / 100.0f));
                const float e = ((u - cu) * (u - cu)) / (rx * rx) + ((depth - cd) * (depth - cd)) / (ry * ry);
                if (e < 1.0f) col = e < 0.55f ? c(0x5aa64e) : c(0xf2e2a0);
                else if (e < 1.35f && bayer(px, py) < 0.5f) col = look.ground[0];
            }
            break;
        }
        case DesertWorld: {
            // Ripples: diagonal streaks.
            if (int(u + depth * 1.6f) % 9 == 0 && depth > 3) col = col.brighter(0.18f);
            break;
        }
        case IceGiant: {
            if (int(u * 0.7f - depth * 2.3f) % 23 == 0 && depth > 4) col = c(0x9adff0);   // cracks in the ice
            if (hash(ui, int(depth), 5) > 0.985f && ((frame + ui) % 4) == 0) col = c(0xffffff);   // sparkling snow
            break;
        }
        case LavaWorld: {
            // Glowing cracks: a network where two noises meet, pulsing.
            const float n = noise1(u / 9.0f + depth * 0.13f, 21) - noise1(depth / 7.0f - u * 0.05f, 22);
            if (std::abs(n) < 0.05f + 0.01f * depth / 20.0f) {
                const float pulse = 0.5f + 0.5f * std::sin(time * 2.2f + u * 0.07f);
                col = mixc(c(0xe0402a), c(0xffc050), pulse);
            }
            break;
        }
        case ForestWorld: {
            // Woods: tree tops dotted over the ground, broken by rivers, lakes and
            // clearings.
            float rim = 0;
            switch (forestGround(u, depth, rim)) {
                case River: {
                    // Water with a dark bank, glints drifting downstream.
                    if (rim > 0.75f) col = c(0x2e5a34);
                    else {
                        col = mixc(c(0x4a9ad8), c(0x2f6ab8), juce::jlimit(0.0f, 1.0f, depth / 60.0f));
                        if (hash(int(std::floor(depth - time * 4.0f)), ui / 2, 57) > 0.86f) col = c(0xbfe6ff);
                    }
                    break;
                }
                case Lake: {
                    // A reedy edge, a sandy shore by day, still water with a few ripples.
                    if (rim > 0.82f) col = bayer(px, py) < 0.5f ? c(0x3c7a40) : c(0xd8d090);
                    else {
                        col = rim > 0.6f ? c(0x3c88c8) : c(0x2a66a8);
                        const float ripple = std::fmod(depth * 1.3f + u * 0.15f + time * 1.5f, 5.0f);
                        if (ripple < 0.6f && bayer(px, py) < 0.5f) col = c(0x8ccaf0);
                    }
                    break;
                }
                case Clearing: {
                    // A sunny meadow with a dithered edge into the woods, and flowers.
                    if (rim > 0.7f && bayer(px, py) < (rim - 0.7f) * 3.3f) break;
                    col = rim < 0.5f ? c(0xb4dc78) : c(0x9ccc68);
                    if (hash(ui, int(depth), 59) > 0.94f) col = hash(ui, int(depth), 61) > 0.5f ? c(0xfff0a0) : c(0xff9ccf);
                    break;
                }
                case Woods:
                    if (hash(ui / 3, int(depth / 3.0f), 7) > 0.7f && depth > 4) col = look.ground[std::size_t(std::min(band + 1, 3))].darker(0.15f);
                    else if (hash(ui / 2, int(depth / 2.0f), 9) > 0.85f) col = col.brighter(0.12f);
                    break;
            }
            break;
        }
        case GasGiant: {
            // Swirling bands: no ground, only weather.
            const float swirl = depth + 3.0f * std::sin(u / 22.0f + depth / 9.0f + time * 0.25f);
            int s = int(swirl / 6.0f);
            if (swirl / 6.0f - float(s) > bayer(px, py) * 0.7f + 0.3f) ++s;
            col = look.ground[std::size_t((s % 4 + 4) % 4)];
            // A great storm.
            const float spot = std::fmod(u + 4000.0f, 260.0f) - 130.0f, sy = depth - 34.0f;
            if ((spot * spot) / 400.0f + (sy * sy) / 49.0f < 1.0f) col = (spot * spot) / 400.0f + (sy * sy) / 49.0f < 0.4f ? c(0xe0806a) : c(0xb85a50);
            break;
        }
        default: {   // Rocky: craters, foreshortened near the horizon
            const int cell = int(std::floor(u / 34.0f));
            const float cx = (float(cell) + 0.2f + 0.6f * hash(cell, 0, 31)) * 34.0f, cy = 8.0f + 52.0f * hash(cell, 1, 31);
            const float rx = 5.0f + 6.0f * hash(cell, 2, 31), ry = rx * (0.25f + std::min(0.5f, cy / 90.0f));
            const float dx = (u - cx) / rx, dy = (depth - cy) / ry, d = dx * dx + dy * dy;
            if (d < 1.0f) col = dy < -0.2f ? look.ground[3] : look.ground[std::size_t(std::min(band + 1, 3))].darker(0.1f);
            else if (d < 1.5f && dy > 0.0f) col = col.brighter(0.15f);
            break;
        }
    }
    return col;
}

// How tall the horizon's skyline stands at a surface point (art pixels above the
// curve): dunes, islands, ice spikes, trees, boulders, a volcano; 0: none.
float skylineAt(int planet, float u, float time) {
    switch (planet) {
        case DesertWorld: return std::max(0.0f, 3.0f + 3.0f * std::sin(u / 15.0f) + 2.0f * std::sin(u / 5.7f + 1.0f));
        case Ocean: {
            const float isle = noise1(u / 38.0f, 13);
            const float waves = (std::sin(u / 2.0f + time * 3.0f) > 0.6f) ? 1.0f : 0.0f;
            return std::max(isle > 0.7f ? (isle - 0.7f) * 32.0f : 0.0f, waves);
        }
        case IceGiant: {
            const int cell = int(std::floor(u / 13.0f));
            const float h = 3.0f + 7.0f * hash(cell, 0, 41), centre = (float(cell) + 0.5f) * 13.0f;
            return std::max(0.0f, h * (1.0f - std::abs(u - centre) / 4.0f));
        }
        case ForestWorld: {
            // A treeline, open where a river, lake or clearing meets the horizon.
            float rim = 0;
            if (forestGround(u, 0.5f, rim) != Woods) return 0.0f;
            const int cell = int(std::floor(u / 6.0f));
            const float h = 5.0f + 7.0f * hash(cell, 0, 43), centre = (float(cell) + 0.5f) * 6.0f;
            return hash(cell, 1, 43) > 0.2f ? std::max(0.0f, h * (1.0f - std::abs(u - centre) / 3.4f)) : 0.0f;
        }
        case LavaWorld: {
            const float spot = std::fmod(u + 4000.0f, 300.0f) - 150.0f;
            return std::max(0.0f, 14.0f - std::abs(spot) * 0.45f);   // a volcano cone
        }
        case Rocky: {
            const int cell = int(std::floor(u / 9.0f));
            return hash(cell, 0, 47) > 0.75f ? std::max(0.0f, 3.0f * hash(cell, 1, 47) * (1.0f - std::abs(u - (float(cell) + 0.5f) * 9.0f) / 3.0f)) : 0.0f;
        }
        default: return 0.0f;
    }
}

juce::Colour skylineColour(int planet, const Look& look, float height, float above) {
    switch (planet) {
        case ForestWorld: return above > height - 1.0f ? look.ground[1] : c(0x2e6a3c);
        case IceGiant: return above > height - 1.5f ? c(0xffffff) : c(0xb8e6f4);
        case Ocean: return height > 1.5f ? (above < 2.0f ? c(0xf2e2a0) : c(0x5aa64e)) : c(0xe8f8ff);
        case LavaWorld: return above > height - 2.0f ? c(0xff8a3a) : look.ground[1];
        default: return above > height - 1.0f ? look.ground[0].brighter(0.15f) : look.ground[1];
    }
}
} // namespace

// ---- Geometry ------------------------------------------------------------------------

juce::Point<float> TinyPlanet::centre(int width, int height) { return { float(width) * 0.5f, float(height) * 0.5f + radius(width, height) }; }
float TinyPlanet::radius(int width, int) { return float(width) * 0.40f; }
float TinyPlanet::angleOf(float longitude, float turn) {
    float a = std::fmod(longitude + turn + pi, twoPi);
    if (a < 0) a += twoPi;
    return a - pi;
}
bool TinyPlanet::visible(float angle) { return std::abs(angle) < visibleArc; }
juce::Point<float> TinyPlanet::horizonPoint(int width, int height, float angle, float lift) {
    const auto o = centre(width, height);
    const float r = radius(width, height) + lift;
    return { o.x + r * std::sin(angle), o.y - r * std::cos(angle) };
}
juce::Point<float> TinyPlanet::markerPoint(int width, int height, float position, int count) {
    // An arc in the sky, concentric with the horizon, from left to right.
    const float spread = 0.36f;
    const float a = count > 1 ? -spread + 2.0f * spread * position / float(count - 1) : 0.0f;
    return horizonPoint(width, height, a, float(height) * 0.25f);
}

int TinyPlanet::kindOf(int placeType) {
    switch (placeType) {
        case Spaceship: case SpaceStation: case Bridge: case Mining: return Orbit;
        case Nebula: return Space;
        default: return Ground;
    }
}

bool TinyPlanet::placePoint(int width, int height, int kind, float longitude, float turn, float time, juce::Point<float>& middle, float& angle) {
    if (kind == Space) {
        // High in the sky, spread by its longitude, drifting slowly across.
        const float span = float(width) + 40.0f;
        const float x = std::fmod(float(width) * (0.5f + 0.4f * std::sin(longitude)) + 20.0f + time * 0.6f, span) - 20.0f;
        middle = { x, float(height) * 0.16f };
        angle = 0;
        return true;
    }
    if (kind == Orbit) {
        // A lower orbit than the sky arc, turning faster than the planet; behind it when
        // the orbit takes it past the sides.
        angle = angleOf(longitude, turn * 1.8f);
        if (std::abs(angle) > visibleArc - 0.1f) return false;
        middle = horizonPoint(width, height, angle, float(height) * 0.14f + 1.5f * std::sin(time * 1.3f + longitude * 5.0f));
        angle *= 0.25f;   // a little lean along the orbit, mostly upright
        return true;
    }
    angle = angleOf(longitude, turn);
    if (!visible(angle)) return false;
    middle = horizonPoint(width, height, angle, 12.0f);
    return true;
}

// ---- The sky (cached) ----------------------------------------------------------------

const pixel::Canvas& TinyPlanet::sky(const TinyPlanetScene& scene, int planet, const pixel::Palette& p) {
    const int dayStep = juce::roundToInt(scene.day * 8.0f), dangerStep = juce::roundToInt(scene.danger * 4.0f);
    const auto key = juce::String(planet) + "/" + juce::String(scene.width) + "x" + juce::String(scene.height) + "/" + juce::String(dayStep) + "/" + juce::String(dangerStep)
                     + "/" + p.panelDark.toString() + p.accentDim.toString();
    if (auto it = skies.find(key); it != skies.end()) return it->second;
    if (skies.size() > 64) skies.clear();
    const auto look = lookOf(planet);
    const float day = float(dayStep) / 8.0f, danger = float(dangerStep) / 4.0f;
    // Night: the interface's own darks; day: the planet's sky; both a little in the mood.
    const auto top = mixc(mixc(p.panelDark, p.accentDim, 0.35f), look.skyTop.interpolatedWith(p.panel, 0.2f), day);
    const auto low = mixc(mixc(p.panel, p.accentDim, 0.6f), look.skyLow.interpolatedWith(p.panel, 0.15f), day);
    const auto threat = c(0x5a1020);
    pixel::Canvas k(scene.width, scene.height);
    const float horizon = float(scene.height) * 0.5f;
    for (int y = 0; y < scene.height; ++y) {
        // Six bands from the top to the horizon, dithered.
        const float t = juce::jlimit(0.0f, 1.0f, float(y) / horizon) * 5.0f;
        for (int x = 0; x < scene.width; ++x) {
            int band = int(t);
            if (t - float(band) > bayer(x, y)) ++band;
            auto col = mixc(top, low, float(std::min(band, 5)) / 5.0f);
            if (danger > 0) col = mixc(col, threat, danger * 0.35f);
            k.set(x, y, col);
        }
    }
    // The sun by day: a round, stepped disc with a soft ring.
    if (day > 0.3f) {
        const float sx = float(scene.width) * 0.16f, sy = float(scene.height) * 0.2f, r = 6.0f;
        for (int y = int(sy - r - 3); y <= int(sy + r + 3); ++y)
            for (int x = int(sx - r - 3); x <= int(sx + r + 3); ++x) {
                const float d = std::hypot(float(x) + 0.5f - sx, float(y) + 0.5f - sy);
                if (d < r) k.set(x, y, d < r - 2.0f ? look.accent.brighter(0.2f) : look.accent);
                else if (d < r + 2.5f && bayer(x, y) < 0.5f * day) k.blend(x, y, look.accent.withAlpha(0.5f));
            }
    }
    return skies[key] = std::move(k);
}

// A Place, rotated by an angle step (2 degrees) and scaled (steps of 1/16) in art pixels:
// nearest-neighbour, so it stays crisp pixel art; dimmed toward the haze.
const pixel::Canvas& TinyPlanet::placeSprite(int type, int iconFrame, int angleStep, int scaleStep, int dimStep, bool highlight, const pixel::Palette& p) {
    const auto key = juce::String(type) + "/" + juce::String(iconFrame) + "/" + juce::String(angleStep) + "/" + juce::String(scaleStep) + "/" + juce::String(dimStep) + "/"
                     + juce::String(int(highlight)) + p.outline.toString();
    if (auto it = sprites.find(key); it != sprites.end()) return it->second;
    if (sprites.size() > 512) sprites.clear();
    const auto icon = pixel::placeIcon(type, iconFrame, p.outline);
    const float angle = juce::degreesToRadians(float(angleStep) * 2.0f), scale = float(scaleStep) / 16.0f, dim = float(dimStep) / 8.0f;
    const int size = 48;
    pixel::Canvas k(size, size);
    // The icon's foot (bottom centre) sits at the canvas centre; rotate about it.
    const float ca = std::cos(angle), sa = std::sin(angle);
    const auto haze = p.panelDark;
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const float dx = float(x) + 0.5f - float(size) * 0.5f, dy = float(y) + 0.5f - float(size) * 0.5f;
            // Into the icon's frame: unrotate, unscale.
            const float ix = (ca * dx + sa * dy) / scale + 16.0f, iy = (-sa * dx + ca * dy) / scale + 31.0f;
            if (ix < 0 || iy < 0 || ix >= 32 || iy >= 32) continue;
            auto col = icon.get(int(ix), int(iy));
            if (col.getAlpha() == 0) continue;
            if (dim > 0) col = mixc(col, haze, dim * 0.7f);
            k.set(x, y, col);
        }
    if (highlight) {
        // A glow around its silhouette.
        pixel::Canvas glow(size, size);
        for (int y = 1; y < size - 1; ++y)
            for (int x = 1; x < size - 1; ++x)
                if (k.get(x, y).getAlpha() == 0 && (k.get(x - 1, y).getAlpha() || k.get(x + 1, y).getAlpha() || k.get(x, y - 1).getAlpha() || k.get(x, y + 1).getAlpha()))
                    glow.set(x, y, p.glow);
        k.paste(glow, 0, 0);
    }
    return sprites[key] = std::move(k);
}

// ---- The frame -----------------------------------------------------------------------

const pixel::Canvas& TinyPlanet::render(const TinyPlanetScene& s, const pixel::Palette& p) {
    const int w = std::max(16, s.width), h = std::max(16, s.height);
    if (frame.width() != w || frame.height() != h) frame = pixel::Canvas(w, h);
    const int planet = juce::jlimit(0, int(PlanetTypeCount) - 1, s.planet), previous = juce::jlimit(0, int(PlanetTypeCount) - 1, s.previous);
    const bool fading = s.blend < 1.0f && previous != planet;
    // Which planet a pixel shows during a crossfade: an ordered dither by the blend.
    const auto pick = [&](int x, int y) { return !fading || bayer(x, y) < s.blend ? planet : previous; };

    // The sky.
    {
        const auto& a = sky(s, planet, p);
        if (!fading) frame.paste(a, 0, 0);
        else {
            const auto& b = sky(s, previous, p);
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x) frame.set(x, y, (pick(x, y) == planet ? a : b).get(x, y));
        }
    }
    const float night = 1.0f - s.day;
    // Stars at night, twinkling.
    if (night > 0.15f) {
        for (int i = 0; i < w * h / 160; ++i) {
            const int x = int(hash(i, 0, 61) * float(w)), y = int(hash(i, 1, 61) * float(h) * 0.5f);
            if (hash(i, 2, 61) > night) continue;
            const bool bright = (i + s.frame / 2) % 7 == 0;
            frame.blend(x, y, juce::Colours::white.withAlpha(bright ? 0.95f : 0.55f));
        }
    }
    // An aurora with Magic at night: faint curtains of vertical rays, each fading in at
    // its foot and out toward the top by ordered dither, its brightness wandering slowly
    // along the sky. Never a solid band (that read as a glitch across the panel's top).
    if (s.magic > 0.35f && night > 0.4f) {
        const float strength = juce::jlimit(0.0f, 1.0f, (s.magic - 0.35f) * 1.6f) * juce::jlimit(0.0f, 1.0f, (night - 0.4f) * 2.0f);
        const float foot = float(h) * 0.34f;
        for (int x = 0; x < w; ++x) {
            const float ray = noise1(float(x) * 0.09f + s.time * 0.15f, 91) * noise1(float(x) * 0.023f - s.time * 0.05f, 93);
            const float amount = strength * juce::jlimit(0.0f, 1.0f, (ray - 0.2f) * 2.2f) * 0.55f;
            if (amount <= 0.01f) continue;
            const float length = float(h) * (0.12f + 0.12f * noise1(float(x) * 0.05f, 95));
            const float base = foot + 3.0f * noise1(float(x) * 0.04f + s.time * 0.1f, 97);
            for (int k = 0; k < int(length); ++k) {
                const int y = int(base) - k;
                const float t = float(k) / length;   // 0 at the foot, 1 at the top
                const float fade = std::min(t * 6.0f, 1.0f) * (1.0f - t);
                if (bayer(x, y) < amount * fade) frame.blend(x, y, (t < 0.45f ? c(0x7af0b8) : c(0xa890f0)).withAlpha(0.5f));
            }
        }
    }
    // Places out in space (the Nebula): far off in the sky, behind the clouds.
    for (const auto& lm : s.landmarks) {
        if (lm.kind != Space) continue;
        juce::Point<float> middle;
        float tilt = 0;
        placePoint(w, h, Space, lm.longitude, s.turn, s.time, middle, tilt);
        const auto& sprite = placeSprite(lm.type, lm.frame, 0, juce::roundToInt((0.6f + 0.3f * lm.near) * 16.0f), juce::roundToInt((0.3f + night * 0.2f) * 8.0f), lm.highlight, p);
        const float up = 16.0f * (0.6f + 0.3f * lm.near);
        frame.paste(sprite, int(std::round(middle.x)) - sprite.width() / 2, int(std::round(middle.y + up)) - sprite.height() / 2);
    }
    const auto o = centre(w, h);
    const float r = radius(w, h);
    // Gas Giant rings: an arc across the sky (drawn before the planet hides its foot).
    if (planet == GasGiant || (fading && previous == GasGiant)) {
        for (int band = 0; band < 4; ++band) {
            const float rx = r * 1.32f + float(band) * 2.0f, ry = r * 0.5f + float(band) * 1.0f;
            const auto ringColour = band == 1 ? c(0xe8c8a0) : band == 2 ? c(0xb08ab8) : c(0xd0a07a);
            for (int x = 0; x < w; ++x) {
                const float dx = (float(x) + 0.5f - o.x) / rx;
                if (std::abs(dx) >= 1.0f) continue;
                const int y = int(o.y - r * 0.62f - ry * std::sqrt(1.0f - dx * dx));
                if (y >= 0 && y < h && pick(x, y) == GasGiant) frame.set(x, y, ringColour);
            }
        }
    }
    // Clouds drifting across the sky.
    if (s.clouds > 0.05f) {
        const int count = 1 + int(s.clouds * 6.0f);
        const auto cloud = mixc(c(0xffffff), c(0x8090a0), s.rain + s.flash * 0.5f).interpolatedWith(p.panelDark, night * 0.6f);
        for (int i = 0; i < count; ++i) {
            const float speed = 1.5f + 2.5f * hash(i, 0, 71);
            const float cx = std::fmod(hash(i, 1, 71) * float(w + 60) + s.time * speed, float(w + 60)) - 30.0f;
            const float cy = 6.0f + hash(i, 2, 71) * float(h) * 0.3f, size = 5.0f + 6.0f * hash(i, 3, 71);
            for (int y = int(cy - size); y < int(cy + size * 0.6f); ++y)
                for (int x = int(cx - size * 2.0f); x < int(cx + size * 2.0f); ++x) {
                    const float dx = (float(x) - cx) / (size * 2.0f), dy = (float(y) - cy) / size;
                    const float bump = 0.2f * std::sin(float(x) * 0.6f + float(i));
                    if (dx * dx + dy * dy < 0.8f + bump) frame.set(x, y, y > int(cy + size * 0.2f) ? cloud.darker(0.2f) : cloud);
                }
        }
    }
    // Lightning: the sky flashes, a jagged bolt to the horizon.
    if (s.flash > 0.02f) {
        frame.blendFill(0, 0, w, int(o.y - r) + 4, juce::Colours::white.withAlpha(0.25f * s.flash));
        juce::Random bolt(s.boltSeed);
        int x = int(float(w) * (0.25f + 0.5f * bolt.nextFloat())), y = 4;
        const int bottom = int(o.y - r);
        while (y < bottom) {
            const int nx = x + bolt.nextInt({ -3, 4 }), ny = y + 2 + bolt.nextInt(4);
            frame.line(x, y, nx, ny, c(0xf0faff).withAlpha(s.flash));
            frame.line(x + 1, y, nx + 1, ny, c(0x9ad8ff).withAlpha(0.6f * s.flash));
            x = nx;
            y = ny;
        }
    }

    // The planet: each pixel samples the surface at its longitude (turning) and depth.
    std::array<Look, PlanetTypeCount> looks;
    for (int t = 0; t < PlanetTypeCount; ++t) looks[std::size_t(t)] = lookOf(t);
    const int top = std::max(0, int(o.y - r - 18.0f));
    for (int y = top; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float dx = float(x) + 0.5f - o.x, dy = float(y) + 0.5f - o.y;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float angle = std::atan2(dx, -dy);
            const float u = (angle - s.turn) * r;   // art pixels along the surface
            const int type = pick(x, y);
            const auto& look = looks[std::size_t(type)];
            if (d <= r) {
                const float depth = r - d;
                auto col = groundAt(type, look, u, depth, x, y, s.time, s.frame);
                // Limb darkening toward the sides, night shade.
                const float edge = std::abs(angle) / (visibleArc * 1.05f);
                if (edge > 0.55f && bayer(x, y) < (edge - 0.55f) * 1.6f) col = col.darker(0.35f);
                if (night > 0.0f) col = mixc(col, p.panelDark, night * 0.45f);
                frame.set(x, y, col);
            } else if (d < r + 16.0f) {
                // The skyline above the curve; and the atmosphere's glow along it.
                const float above = d - r, height = skylineAt(type, u, s.time);
                if (above < height) {
                    auto col = skylineColour(type, look, height, above);
                    if (night > 0.0f) col = mixc(col, p.panelDark, night * 0.45f);
                    frame.set(x, y, col);
                } else if (above < 3.0f && bayer(x, y) < 0.45f - above * 0.13f) {
                    frame.blend(x, y, look.skyLow.withAlpha(0.6f));
                }
            }
        }

    // Rain and snow over everything in the sky.
    if (s.rain > 0.02f) {
        const int count = 10 + int(160.0f * s.rain);
        const auto drop = mixc(c(0xa8d8ff), c(0xffffff), 0.3f).withAlpha(0.75f);
        for (int i = 0; i < count; ++i) {
            const float speed = 70.0f + 40.0f * hash(i, 0, 81);
            const int x0 = int(hash(i, 1, 81) * float(w + 40)) - 20;
            const int y0 = int(std::fmod(hash(i, 2, 81) * float(h) + s.time * speed, float(h)));
            for (int k = 0; k < 3; ++k) {
                const int x = x0 - (y0 + k) / 3, y = y0 + k;
                const float dx = float(x) - o.x, dy = float(y) - o.y;
                if (dx * dx + dy * dy > r * r) frame.blend(x, y, drop);
            }
        }
    }
    if (s.snow > 0.02f) {
        const int count = 10 + int(110.0f * s.snow);
        for (int i = 0; i < count; ++i) {
            const float speed = 8.0f + 8.0f * hash(i, 0, 83);
            const int y = int(std::fmod(hash(i, 2, 83) * float(h) + s.time * speed, float(h)));
            const int x = int(hash(i, 1, 83) * float(w) + 2.0f * std::sin(s.time * 1.3f + float(i)));
            const float dx = float(x) - o.x, dy = float(y) - o.y;
            if (dx * dx + dy * dy > r * r) frame.set(x, y, c(0xffffff));
        }
    }

    // The Places on the horizon: upright, tilted with the curve; smaller and hazier
    // toward the edges and when far; gone over the horizon. Orbiting Places float above
    // it (drawn after the planet only while in front of it).
    for (const auto& lm : s.landmarks) {
        if (lm.kind == Space) continue;
        juce::Point<float> middle;
        float tilt = 0;
        if (!placePoint(w, h, lm.kind, lm.longitude, s.turn, s.time, middle, tilt)) continue;
        const float edge = juce::jlimit(0.0f, 1.0f, std::abs(lm.kind == Orbit ? tilt * 4.0f : tilt) / visibleArc);
        const float scale = juce::jlimit(0.35f, 1.0f, (1.0f - 0.55f * edge * edge) * (0.62f + 0.38f * lm.near) * (lm.kind == Orbit ? 0.85f : 1.0f));
        const float dim = juce::jlimit(0.0f, 1.0f, edge * edge * 0.9f + (1.0f - lm.near) * 0.35f + night * 0.25f);
        const auto& sprite = placeSprite(lm.type, lm.frame, juce::roundToInt(juce::radiansToDegrees(tilt) / 2.0f), juce::roundToInt(scale * 16.0f),
                                         juce::roundToInt(dim * 8.0f), lm.highlight, p);
        // The sprite's centre is the icon's foot: on the horizon for ground Places, half an
        // icon below the middle (along its tilt) for orbiting ones.
        const float up = 16.0f * scale;
        const auto foot = lm.kind == Ground ? horizonPoint(w, h, tilt, -1.0f) : middle.translated(-std::sin(tilt) * up, std::cos(tilt) * up);
        frame.paste(sprite, int(std::round(foot.x)) - sprite.width() / 2, int(std::round(foot.y)) - sprite.height() / 2);
    }

    // The Locations: stars and moons on the sky arc; a comet on a journey.
    const int count = int(s.markers.size());
    for (int i = 0; i < count; ++i) {
        const auto& m = s.markers[std::size_t(i)];
        const auto at = markerPoint(w, h, float(i), count);
        const int mx = int(at.x), my = int(at.y);
        if (!m.used) {
            frame.blend(mx, my, p.inkDim.withAlpha(0.8f));
            frame.blend(mx - 1, my, p.inkDim.withAlpha(0.3f));
            frame.blend(mx + 1, my, p.inkDim.withAlpha(0.3f));
            continue;
        }
        const auto col = m.colour;
        if (i % 2 == 0) {
            // A star: a bright cross with a centre.
            const int arm = m.here ? 4 : 3;
            for (int k = -arm; k <= arm; ++k) {
                const float fade = 1.0f - float(std::abs(k)) / float(arm + 1);
                frame.blend(mx + k, my, col.withAlpha(fade));
                frame.blend(mx, my + k, col.withAlpha(fade));
            }
            frame.set(mx, my, juce::Colours::white);
            frame.set(mx - 1, my - 1, col.brighter(0.3f).withAlpha(0.7f));
            frame.set(mx + 1, my + 1, col.brighter(0.3f).withAlpha(0.7f));
        } else {
            // A moon: a crescent.
            for (int y = -4; y <= 4; ++y)
                for (int x = -4; x <= 4; ++x) {
                    const float d1 = std::hypot(float(x), float(y)), d2 = std::hypot(float(x) - 2.0f, float(y) + 1.0f);
                    if (d1 <= 3.6f && d2 > 3.0f) frame.set(mx + x, my + y, d1 > 2.8f ? col.darker(0.25f) : col.brighter(0.2f));
                }
        }
        if (m.here)
            for (int k = 0; k < 16; ++k) {
                const float a = float(k) / 16.0f * twoPi;
                frame.blend(mx + int(std::round(6.0f * std::cos(a))), my + int(std::round(6.0f * std::sin(a))), col.withAlpha(0.5f));
            }
    }
    if (s.comet && count > 0) {
        const float t = juce::jlimit(0.0f, 1.0f, s.cometProgress);
        for (int k = 14; k >= 0; --k) {
            const float u = t - float(k) * 0.02f;
            if (u < 0.0f) continue;
            const auto at = markerPoint(w, h, s.cometFrom + (s.cometTo - s.cometFrom) * u, count);
            const auto lift = at.translated(0.0f, -3.0f - 5.0f * std::sin(pi * u));   // it arcs a little above the markers
            const float fade = 1.0f - float(k) / 15.0f;
            frame.blend(int(lift.x), int(lift.y), (k < 3 ? c(0xffffff) : p.inlay).withAlpha(fade));
            if (k == 0) {
                frame.set(int(lift.x) + 1, int(lift.y), c(0xffffff));
                frame.set(int(lift.x), int(lift.y) + 1, c(0xffffff));
                frame.set(int(lift.x) + 1, int(lift.y) + 1, p.glow);
            }
        }
    }
    return frame;
}

} // namespace wb::ui
