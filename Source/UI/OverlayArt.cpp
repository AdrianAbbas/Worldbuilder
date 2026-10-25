#include "OverlayArt.h"
#include "../Places/Place.h"

namespace wb::ui::art {

namespace {
constexpr float twoPi = juce::MathConstants<float>::twoPi;
using Rect = juce::Rectangle<float>;

// Adds a mover to an overlay.
void move(OverlayArt& a, int kind, Point at, int sprite, float period, float phase, float alpha = 1.0f, float travel = 0, float sway = 0, int frames = 1) {
    Mover m;
    m.kind = uint8_t(kind);
    m.at = at;
    m.sprite = uint8_t(sprite);
    m.period = period;
    m.phase = phase;
    m.alpha = alpha;
    m.travel = travel;
    m.sway = sway;
    m.frames = uint8_t(frames);
    a.movers.push_back(m);
}

// A neon tube: a wide faint glow, a narrower brighter one and a pale core.
void neon(OverlayArt& a, const juce::Path& tube, juce::Colour colour, float width = 1.4f) {
    a.shapes.push_back({ tube, colour.withAlpha(0.12f), width * 6.0f });
    a.shapes.push_back({ tube, colour.withAlpha(0.32f), width * 2.8f });
    a.shapes.push_back({ tube, colour.interpolatedWith(juce::Colours::white, 0.55f), width });
}

// Soft puffs of growing size (smoke, steam, incense), sprites first..first+7.
int puffSprites(OverlayArt& a, juce::Colour colour, float from = 2.0f) {
    const int first = int(a.sprites.size());
    for (int i = 0; i < 8; ++i) a.sprites.push_back(glowSprite(from + float(i) * 1.1f, colour, juce::Colours::transparentBlack, 0.0f));
    return first;
}

// A four-pointed glint with a soft glow.
juce::Image sparkleSprite(float radius, juce::Colour colour) {
    juce::Path star;
    star.addStar({ 0.0f, 0.0f }, 4, radius * 0.18f, radius, 0.0f);
    return pathSprite(star, colour.interpolatedWith(juce::Colours::white, 0.5f), radius * 0.45f, colour);
}

// A gear outline (teeth around a rim, a hole in the middle) turned by `angle`.
juce::Path gear(float radius, int teeth, float angle) {
    juce::Path p;
    const int points = teeth * 4;
    for (int i = 0; i < points; ++i) {
        const float a = angle + twoPi * float(i) / float(points);
        const float r = (i % 4 == 0 || i % 4 == 1) ? radius : radius * 0.78f;
        const Point at { r * std::cos(a), r * std::sin(a) };
        if (i == 0) p.startNewSubPath(at);
        else p.lineTo(at);
    }
    p.closeSubPath();
    p.addEllipse(-radius * 0.3f, -radius * 0.3f, radius * 0.6f, radius * 0.6f);
    p.setUsingNonZeroWinding(false);
    return p;
}

// Frames of a spinning gear (one tooth's turn, so the cycle loops seamlessly).
int gearSprites(OverlayArt& a, float radius, int teeth, juce::Colour colour, int frames) {
    const int first = int(a.sprites.size());
    for (int f = 0; f < frames; ++f) {
        auto image = pathSprite(gear(radius, teeth, twoPi / float(teeth) * float(f) / float(frames)), colour);
        // A fixed size for every frame, so each one is stamped at the same place.
        const int size = int(std::ceil(radius * 2.0f)) + 4;
        juce::Image framed(juce::Image::ARGB, size, size, true);
        juce::Graphics g(framed);
        g.drawImageAt(image, (size - image.getWidth()) / 2, (size - image.getHeight()) / 2);
        a.sprites.push_back(framed);
    }
    return first;
}

// A tapering tentacle from `base` along a curve, `width` at the base.
void tendril(juce::Path& body, juce::Path& dots, Point base, float angle, float length, float width, float curl, juce::Random& r) {
    const int steps = 18;
    std::vector<Point> spine;
    float a = angle;
    Point p = base;
    for (int i = 0; i <= steps; ++i) {
        spine.push_back(p);
        a += curl * (0.4f + 0.8f * float(i) / float(steps)) / float(steps) * 3.0f;
        p += Point { std::cos(a), std::sin(a) } * (length / float(steps));
    }
    juce::Path t;
    std::vector<Point> left, right;
    for (int i = 0; i <= steps; ++i) {
        const auto& q = spine[std::size_t(i)];
        const auto& next = spine[std::size_t(std::min(i + 1, steps))];
        const auto& prev = spine[std::size_t(std::max(i - 1, 0))];
        auto d = next - prev;
        const float len = std::max(0.001f, d.getDistanceFromOrigin());
        const Point n { -d.y / len, d.x / len };
        const float w = width * 0.5f * (1.0f - 0.92f * float(i) / float(steps));
        left.push_back(q + n * w);
        right.push_back(q - n * w);
        if (i % 3 == 1 && i < steps - 2) dots.addEllipse(juce::Rectangle<float>(w * 0.7f, w * 0.7f).withCentre(q + n * (w * 0.2f)));
    }
    t.startNewSubPath(left.front());
    for (std::size_t i = 1; i < left.size(); ++i) t.lineTo(left[i]);
    for (std::size_t i = right.size(); i-- > 0;) t.lineTo(right[i]);
    t.closeSubPath();
    body.addPath(t);
    juce::ignoreUnused(r);
}

// A faceted crystal growing from `base` along `angle`: its lit and shaded halves.
void crystal(juce::Path& lit, juce::Path& shade, Point base, float angle, float length, float width) {
    const Point u { std::cos(angle), std::sin(angle) }, n { -u.y, u.x };
    const auto at = [&](float along, float across) { return base + u * along + n * across; };
    lit.addTriangle(at(0, -width * 0.5f), at(length * 0.72f, -width * 0.5f), at(length, 0));
    lit.addTriangle(at(0, -width * 0.5f), at(length, 0), at(0, 0));
    shade.addTriangle(at(0, width * 0.5f), at(length * 0.72f, width * 0.5f), at(length, 0));
    shade.addTriangle(at(0, width * 0.5f), at(length, 0), at(0, 0));
}

//==============================================================================
// Factory: riveted girders on the frames, gusset plates, pipes along the bottom and
// down the gaps with valves, hazard stripes on the ledges, a skyline of sawtooth
// roofs and smokestacks; steam puffs, embers, warning lamps and turning gears.
OverlayArt factory(const Layout& L) {
    OverlayArt a;
    juce::Random r(404);
    juce::Path girders, girderLight, rivets, plates, pipes, pipeLight, flanges, wheels, stripes, stripeBase, skyline, windows, stacks;
    std::vector<Point> vents, lamps;
    for (auto p : L.panels) {
        girders.addRectangle(p.getX() - 2.0f, p.getY() - 3.0f, p.getWidth() + 4.0f, 6.0f);
        girderLight.addRectangle(p.getX() - 2.0f, p.getY() - 3.0f, p.getWidth() + 4.0f, 1.2f);
        for (float x = p.getX() + 7.0f; x < p.getRight() - 5.0f; x += 15.0f) rivets.addEllipse(x - 1.1f, p.getY() - 1.1f, 2.2f, 2.2f);
        // Gusset plates in the corners, and a bolted plate at the bottom ones.
        for (auto [cx, dx] : { std::pair<float, float> { p.getX(), 1.0f }, { p.getRight(), -1.0f } }) {
            plates.addTriangle(cx, p.getY(), cx + dx * 13.0f, p.getY(), cx, p.getY() + 13.0f);
            rivets.addEllipse(cx + dx * 3.5f - 1.0f, p.getY() + 3.5f - 1.0f, 2.0f, 2.0f);
            plates.addRectangle(dx > 0 ? cx : cx - 10.0f, p.getBottom() - 10.0f, 10.0f, 10.0f);
            rivets.addEllipse(cx + dx * 3.0f - 1.0f, p.getBottom() - 4.0f, 2.0f, 2.0f);
            rivets.addEllipse(cx + dx * 7.0f - 1.0f, p.getBottom() - 8.0f, 2.0f, 2.0f);
        }
    }
    // A pipe along the bottom with flanged joints.
    const float bottom = L.bounds.getBottom();
    pipes.addRectangle(0.0f, bottom - 7.0f, L.bounds.getWidth(), 6.0f);
    pipeLight.addRectangle(0.0f, bottom - 6.0f, L.bounds.getWidth(), 1.3f);
    for (float x = between(r, 20.0f, 60.0f); x < L.bounds.getRight(); x += between(r, 90.0f, 150.0f)) flanges.addRectangle(x, bottom - 9.0f, 4.0f, 9.0f);
    // Pipes down the gaps, with valves; a warning lamp at the top of each.
    for (auto gap : L.gutters) {
        const float cx = gap.getCentreX(), w = std::min(6.0f, gap.getWidth() - 4.0f);
        pipes.addRectangle(cx - w * 0.5f, gap.getY() + 14.0f, w, gap.getHeight() - 20.0f);
        pipeLight.addRectangle(cx - w * 0.5f + 1.0f, gap.getY() + 14.0f, 1.2f, gap.getHeight() - 20.0f);
        for (float y = gap.getY() + 40.0f; y < gap.getBottom() - 20.0f; y += between(r, 60.0f, 90.0f)) flanges.addRectangle(cx - w * 0.5f - 2.0f, y, w + 4.0f, 3.0f);
        const Point valve { cx, gap.getY() + gap.getHeight() * between(r, 0.35f, 0.6f) };
        juce::Path wheel;
        wheel.addEllipse(juce::Rectangle<float>(10.0f, 10.0f).withCentre(valve));
        for (int k = 0; k < 2; ++k) {
            const float angle = 0.4f + float(k) * 1.5708f;
            wheel.startNewSubPath(valve + Point { std::cos(angle), std::sin(angle) } * 5.0f);
            wheel.lineTo(valve - Point { std::cos(angle), std::sin(angle) } * 5.0f);
        }
        juce::PathStrokeType(1.6f).createStrokedPath(wheel, wheel);
        wheels.addPath(wheel);
        vents.push_back(valve.translated(0.0f, -6.0f));
        lamps.push_back({ cx, gap.getY() + 8.0f });
    }
    // Hazard stripes on the ledges.
    for (auto ledge : L.ledges) {
        const auto band = ledge.withSizeKeepingCentre(ledge.getWidth() - 16.0f, std::min(5.0f, ledge.getHeight() - 2.0f));
        stripeBase.addRectangle(band);
        for (float x = band.getX() - band.getHeight(); x < band.getRight(); x += 9.0f) {
            juce::Path s;
            s.addQuadrilateral(x, band.getBottom(), x + 4.0f, band.getBottom(), x + 4.0f + band.getHeight(), band.getY(), x + band.getHeight(), band.getY());
            stripes.addPath(s);
        }
    }
    // The skyline in the open space: sawtooth roofs, smokestacks, lit windows.
    std::vector<Point> stackTops, gears;
    for (auto o : L.open) {
        const float base = o.getBottom();
        float x = o.getX() + 34.0f;   // room for a gear at the left
        gears.push_back({ o.getX() + 15.0f, base - 13.0f });
        while (x < o.getRight() - 24.0f) {
            const float w = between(r, 30.0f, 52.0f), h = between(r, 9.0f, 15.0f);
            skyline.addRectangle(x, base - h, w, h);
            for (float tx = x; tx < x + w - 6.0f; tx += 9.0f) skyline.addTriangle(tx, base - h, tx + 9.0f, base - h, tx + 9.0f, base - h - 6.0f);
            for (float wx = x + 3.0f; wx < x + w - 4.0f; wx += 6.0f)
                if (r.nextFloat() < 0.6f) windows.addRectangle(wx, base - h * 0.55f, 3.0f, 3.0f);
            if (r.nextFloat() < 0.6f) {
                const float sx = x + w * between(r, 0.3f, 0.8f), sh = between(r, 22.0f, std::max(23.0f, o.getHeight() - 8.0f));
                stacks.addRectangle(sx, base - sh, 4.0f, sh);
                stackTops.push_back({ sx + 2.0f, base - sh - 2.0f });
            }
            x += w + between(r, 3.0f, 10.0f);
        }
    }
    a.shapes.push_back({ pipes, juce::Colour(0xff7a4426) });
    a.shapes.push_back({ pipeLight, juce::Colour(0xffc07a46) });
    a.shapes.push_back({ flanges, juce::Colour(0xff4c2a18) });
    a.shapes.push_back({ wheels, juce::Colour(0xffc0503a) });
    a.shapes.push_back({ stripeBase, juce::Colour(0xff1c1c1c) });
    a.shapes.push_back({ stripes, juce::Colour(0xffe0b030) });
    a.shapes.push_back({ girders, juce::Colour(0xff4a4f57) });
    a.shapes.push_back({ girderLight, juce::Colour(0xff8a929c) });
    a.shapes.push_back({ plates, juce::Colour(0xff5a606a) });
    a.shapes.push_back({ rivets, juce::Colour(0xffb5bcc4) });
    a.shapes.push_back({ stacks, juce::Colour(0xff2e2a28) });
    a.shapes.push_back({ skyline, juce::Colour(0xff26201c) });
    a.shapes.push_back({ windows, juce::Colour(0xffffa040) });

    const int steam = puffSprites(a, juce::Colour(0x70d8d4d0));
    const int ember = int(a.sprites.size());
    a.sprites.push_back(glowSprite(3.5f, juce::Colour(0xb0ff8a3c), juce::Colour(0xffffe0a0), 0.9f));
    const int lamp = int(a.sprites.size());
    a.sprites.push_back(glowSprite(7.0f, juce::Colour(0xa0ff9a30), juce::Colour(0xffffe8b0), 1.8f));
    const int bigGear = gearSprites(a, 12.0f, 10, juce::Colour(0xff6a707a), 6);
    const int smallGear = gearSprites(a, 6.0f, 8, juce::Colour(0xff8a7060), 6);
    for (auto top : stackTops)
        for (int k = 0; k < 3; ++k) move(a, PlaceOverlays::MoveRise, top, steam, 3.4f, float(k) / 3.0f + r.nextFloat() * 0.1f, 0.85f, 26.0f, 3.0f, 8);
    for (auto v : vents)
        for (int k = 0; k < 2; ++k) move(a, PlaceOverlays::MoveRise, v, steam, 2.6f, float(k) * 0.5f, 0.75f, 44.0f, 2.0f, 8);
    for (int k = 0; k < 10; ++k)
        move(a, PlaceOverlays::MoveRise, { between(r, 10.0f, L.bounds.getRight() - 10.0f), bottom - 6.0f }, ember, between(r, 1.6f, 2.6f), r.nextFloat(), 1.0f, 12.0f, 4.0f);
    for (auto l : lamps) move(a, PlaceOverlays::MoveBlink, l, lamp, 1.4f, r.nextFloat());
    for (auto g : gears) move(a, PlaceOverlays::MoveSpin, g, bigGear, 1.2f, 0.0f, 1.0f, 0, 0, 6);
    for (auto gap : L.gutters) move(a, PlaceOverlays::MoveSpin, { gap.getCentreX(), gap.getBottom() - 12.0f }, smallGear, 0.8f, r.nextFloat(), 1.0f, 0, 0, 6);
    return a;
}

//==============================================================================
// City: a night skyline with lit windows and an antenna in the open space, a neon
// sign, neon strips along the frames and down the gaps, a street along the bottom;
// windows switch on and off, the sign buzzes, cars pass, the antenna blinks.
OverlayArt city(const Layout& L) {
    OverlayArt a;
    a.light = true;
    juce::Random r(505);
    const juce::Colour pink(0xffff5fb0), cyan(0xff5ff0ff), violet(0xffb07aff);
    juce::Path buildings, edges, dimWindows, litWindows, road, lanes;
    std::vector<Point> windowSpots, antennas;
    juce::Path signTube, signText;
    for (auto o : L.open) {
        const float base = o.getBottom();
        const float signWidth = 64.0f;
        float x = o.getX() + 4.0f, tallest = 0;
        Point antenna;
        while (x < o.getRight() - signWidth - 12.0f) {
            const float w = between(r, 14.0f, 26.0f), h = between(r, 14.0f, std::max(15.0f, o.getHeight() - 4.0f));
            buildings.addRectangle(x, base - h, w, h);
            edges.addRectangle(x, base - h, w, 1.0f);
            for (float wy = base - h + 4.0f; wy < base - 3.0f; wy += 5.0f)
                for (float wx = x + 3.0f; wx < x + w - 3.0f; wx += 5.0f) {
                    if (r.nextFloat() < 0.3f) {
                        litWindows.addRectangle(wx, wy, 2.0f, 2.5f);
                        if (r.nextFloat() < 0.25f) windowSpots.push_back({ wx + 1.0f, wy + 1.25f });
                    } else {
                        dimWindows.addRectangle(wx, wy, 2.0f, 2.5f);
                    }
                }
            if (h > tallest) {
                tallest = h;
                antenna = { x + w * 0.5f, base - h };
            }
            x += w + between(r, 1.0f, 4.0f);
        }
        if (tallest > 0) {
            buildings.addRectangle(antenna.x - 0.6f, antenna.y - 8.0f, 1.2f, 8.0f);
            antennas.push_back(antenna.translated(0.0f, -8.0f));
        }
        // A neon sign at the right of the open space.
        const Rect sign { o.getRight() - signWidth - 4.0f, o.getCentreY() - 10.0f, signWidth, 20.0f };
        signTube.addRoundedRectangle(sign, 5.0f);
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)), "OPEN", 0.0f, 0.0f);
        juce::Path text;
        glyphs.createPath(text);
        const auto tb = text.getBounds();
        text.applyTransform(juce::AffineTransform::translation(sign.getCentreX() - tb.getCentreX(), sign.getCentreY() - tb.getCentreY()));
        signText.addPath(text);
    }
    // Neon strips along the lower frames and down the gaps.
    juce::Path pinkTubes, cyanTubes;
    int k = 0;
    for (auto p : L.panels) {
        auto& tube = (k++ % 2 == 0) ? pinkTubes : cyanTubes;
        const float length = p.getWidth() * between(r, 0.25f, 0.45f);
        tube.startNewSubPath(p.getX() + 12.0f, p.getBottom() + 0.5f);
        tube.lineTo(p.getX() + 12.0f + length, p.getBottom() + 0.5f);
        tube.startNewSubPath(p.getRight() - 12.0f, p.getY() - 0.5f);
        tube.lineTo(p.getRight() - 12.0f - length * 0.6f, p.getY() - 0.5f);
    }
    for (auto gap : L.gutters) {
        pinkTubes.startNewSubPath(gap.getCentreX(), gap.getY() + 20.0f);
        pinkTubes.lineTo(gap.getCentreX(), gap.getY() + 20.0f + gap.getHeight() * 0.3f);
        cyanTubes.startNewSubPath(gap.getCentreX(), gap.getBottom() - 20.0f);
        cyanTubes.lineTo(gap.getCentreX(), gap.getBottom() - 20.0f - gap.getHeight() * 0.25f);
    }
    // The street along the bottom: tarmac and lane dashes.
    const float bottom = L.bounds.getBottom();
    road.addRectangle(0.0f, bottom - 7.0f, L.bounds.getWidth(), 7.0f);
    for (float x = 6.0f; x < L.bounds.getRight(); x += 22.0f) lanes.addRectangle(x, bottom - 4.0f, 11.0f, 1.0f);
    a.shapes.push_back({ road, juce::Colour(0xff17151e) });
    a.shapes.push_back({ lanes, juce::Colour(0xb0d8c890) });
    neon(a, pinkTubes, pink);
    neon(a, cyanTubes, cyan);
    a.shapes.push_back({ buildings, juce::Colour(0xff120f22) });
    a.shapes.push_back({ edges, juce::Colour(0xff3a3460) });
    a.shapes.push_back({ dimWindows, juce::Colour(0xff2c2848) });
    a.shapes.push_back({ litWindows, juce::Colour(0xffffd27a) });
    neon(a, signTube, violet, 1.2f);
    a.shapes.push_back({ signText, pink.withAlpha(0.25f), 4.0f });
    a.shapes.push_back({ signText, pink.interpolatedWith(juce::Colours::white, 0.4f) });

    const int window = int(a.sprites.size());
    a.sprites.push_back(glowSprite(3.5f, juce::Colour(0x90ffd27a), juce::Colour(0xffffe8b0), 1.2f));
    const int buzz = int(a.sprites.size());
    a.sprites.push_back(pathSprite(signText, pink.withAlpha(0.0f), 3.0f, pink));
    const int red = int(a.sprites.size());
    a.sprites.push_back(glowSprite(4.5f, juce::Colour(0xb0ff3040), juce::Colour(0xffffa0a0), 1.2f));
    const int head = int(a.sprites.size());
    a.sprites.push_back(glowSprite(5.0f, juce::Colour(0x90fff0c0), juce::Colours::white, 1.2f));
    const int tail = int(a.sprites.size());
    a.sprites.push_back(glowSprite(4.0f, juce::Colour(0xa0ff3030), juce::Colour(0xffffb0a0), 1.0f));
    for (std::size_t i = 0; i < windowSpots.size() && i < 14; ++i) move(a, PlaceOverlays::MoveBlink, windowSpots[i], window, between(r, 5.0f, 11.0f), r.nextFloat());
    if (!signText.isEmpty()) move(a, PlaceOverlays::MoveFlicker, signText.getBounds().getCentre(), buzz, 1.0f, 0.0f, 0.9f);
    for (auto at : antennas) move(a, PlaceOverlays::MoveBlink, at, red, 1.6f, 0.0f);
    const float width = L.bounds.getWidth();
    for (int c = 0; c < 4; ++c) {
        move(a, PlaceOverlays::MoveDrift, { -10.0f, bottom - 5.0f }, head, between(r, 6.0f, 9.0f), float(c) / 4.0f, 0.9f, width + 20.0f, 0.0f);
        move(a, PlaceOverlays::MoveDrift, { width + 10.0f, bottom - 2.5f }, tail, between(r, 7.0f, 10.0f), float(c) / 4.0f + 0.1f, 0.9f, -(width + 20.0f), 0.0f);
    }
    return a;
}

//==============================================================================
// Forest: dense vines up the frames and hanging from them, ferns in the corners,
// moss and mushrooms on the ledges, roots along the bottom, a tree line in the open
// space; leaves fall, pollen drifts, pale mushrooms glow.
OverlayArt forest(const Layout& L) {
    OverlayArt a;
    juce::Random r(606);
    juce::Path stems, leavesDark, leavesLight, ferns, moss, mossLight, stalks, capsRed, capsPale, spots, roots, farTrees, nearTrees;
    std::vector<Point> glowCaps;
    for (auto p : L.panels) {
        const float left = p.getX() - 0.5f, right = p.getRight() + 0.5f;
        for (int k = 0; k < 2; ++k) {   // twice, for a thick growth
            const float reach = k == 0 ? between(r, 0.55f, 0.95f) : between(r, 0.25f, 0.5f);
            vine(stems, leavesDark, leavesLight, { left, p.getBottom() - 4.0f }, { left, p.getBottom() - p.getHeight() * reach }, r, 1.35f);
            vine(stems, leavesDark, leavesLight, { right, p.getBottom() - 4.0f }, { right, p.getBottom() - p.getHeight() * reach * 0.9f }, r, 1.35f);
        }
        vine(stems, leavesDark, leavesLight, { left, p.getY() + 4.0f }, { left, p.getY() + between(r, 50.0f, 110.0f) }, r, 1.2f);
        vine(stems, leavesDark, leavesLight, { right, p.getY() + 4.0f }, { right, p.getY() + between(r, 40.0f, 90.0f) }, r, 1.2f);
        vine(stems, leavesDark, leavesLight, { p.getX() + 4.0f, p.getY() - 0.5f }, { p.getX() + p.getWidth() * between(r, 0.25f, 0.5f), p.getY() - 0.5f }, r, 1.1f);
        vine(stems, leavesDark, leavesLight, { p.getRight() - 4.0f, p.getBottom() + 0.5f }, { p.getRight() - p.getWidth() * between(r, 0.2f, 0.4f), p.getBottom() + 0.5f }, r, 1.1f);
        // Hanging strands from the top edge.
        for (float x = p.getX() + between(r, 30.0f, 60.0f); x < p.getRight() - 30.0f; x += between(r, 70.0f, 140.0f))
            vine(stems, leavesDark, leavesLight, { x, p.getY() }, { x + between(r, -3.0f, 3.0f), p.getY() + between(r, 10.0f, 18.0f) }, r, 0.8f);
        // Ferns in the bottom corners: fronds of small leaflets curving out.
        for (float dir : { 1.0f, -1.0f }) {
            const Point root { dir > 0 ? p.getX() + 2.0f : p.getRight() - 2.0f, p.getBottom() - 1.0f };
            for (int f = 0; f < 3; ++f) {
                const float angle = (dir > 0 ? -1.2f : -1.94f) + dir * (0.35f * float(f) - 0.2f);
                float a0 = angle;
                Point q = root;
                for (int s = 0; s < 9; ++s) {
                    a0 += dir * 0.06f;
                    q += Point { std::cos(a0), std::sin(a0) } * 2.2f;
                    const float size = 4.5f * (1.0f - float(s) / 11.0f);
                    leaf(ferns, q, size, a0 - 1.2f);
                    leaf(ferns, q, size, a0 + 1.2f);
                }
            }
        }
        // Moss along the top edge, and a mushroom or two on it.
        for (float x = p.getX() + 6.0f; x < p.getRight() - 6.0f; x += between(r, 2.0f, 5.0f)) {
            const float clump = 0.5f + 0.5f * std::sin(x * 0.03f + p.getX()) * std::sin(x * 0.011f + 0.7f);
            if (clump < 0.3f) continue;
            const float size = 2.0f + 4.0f * clump * r.nextFloat();
            (r.nextBool() ? moss : mossLight).addEllipse(x - size * 0.5f, p.getY() - size * 0.6f, size, size * 0.9f);
        }
        if (p.getY() > L.top + 2.0f || p.getWidth() > 400.0f) {
            for (int m = 0; m < 2; ++m) {
                const float x = p.getX() + p.getWidth() * between(r, 0.15f, 0.85f), h = between(r, 4.0f, 7.0f), w = between(r, 6.0f, 9.0f);
                stalks.addRectangle(x - 0.8f, p.getY() - h, 1.6f, h);
                const bool pale = r.nextFloat() < 0.5f;
                (pale ? capsPale : capsRed).addPieSegment(x - w * 0.5f, p.getY() - h - w * 0.4f, w, w * 0.8f, -1.5708f, 1.5708f, 0.0f);
                if (pale) glowCaps.push_back({ x, p.getY() - h - 1.0f });
                else spots.addEllipse(x - 1.0f, p.getY() - h - w * 0.25f, 1.6f, 1.6f);
            }
        }
    }
    // Roots along the bottom.
    const float bottom = L.bounds.getBottom();
    for (float x = 0.0f; x < L.bounds.getRight(); x += between(r, 40.0f, 80.0f)) {
        juce::Path root;
        root.startNewSubPath(x, bottom - between(r, 1.0f, 6.0f));
        root.cubicTo(x + 15.0f, bottom - between(r, 3.0f, 8.0f), x + 30.0f, bottom - between(r, 0.0f, 5.0f), x + between(r, 45.0f, 70.0f), bottom - between(r, 1.0f, 7.0f));
        juce::PathStrokeType(between(r, 1.6f, 3.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(root, root);
        roots.addPath(root);
    }
    // A tree line in the open space: a far row and a near one.
    for (auto o : L.open) {
        for (int row = 0; row < 2; ++row) {
            auto& trees = row == 0 ? farTrees : nearTrees;
            for (float x = o.getX() + between(r, 0.0f, 8.0f); x < o.getRight() - 6.0f; x += between(r, 8.0f, 16.0f)) {
                const float h = between(r, 12.0f, o.getHeight() - 2.0f) * (row == 0 ? 1.0f : 0.75f), w = h * between(r, 0.35f, 0.5f), base = o.getBottom();
                if (r.nextFloat() < 0.7f) {   // a pine of three tiers
                    for (int t = 0; t < 3; ++t) {
                        const float top = base - h + float(t) * h * 0.25f, tierW = w * (0.55f + 0.25f * float(t));
                        trees.addTriangle(x, top, x - tierW * 0.5f, top + h * 0.38f, x + tierW * 0.5f, top + h * 0.38f);
                    }
                    trees.addRectangle(x - 0.8f, base - h * 0.15f, 1.6f, h * 0.15f);
                } else {   // a round tree
                    trees.addEllipse(x - w * 0.6f, base - h, w * 1.2f, h * 0.75f);
                    trees.addRectangle(x - 1.0f, base - h * 0.3f, 2.0f, h * 0.3f);
                }
            }
        }
    }
    // Leaves fall down the gaps, the window's edges and through the open space (never
    // inside a panel): where each starts and how far it falls.
    std::vector<std::pair<Point, float>> lanes;
    for (auto gap : L.gutters)
        for (int k = 0; k < 3; ++k) lanes.push_back({ { gap.getCentreX(), gap.getY() + gap.getHeight() * between(r, 0.0f, 0.5f) }, between(r, 70.0f, 140.0f) });
    for (float x : { L.bounds.getX() + 5.0f, L.bounds.getRight() - 5.0f })
        for (int k = 0; k < 2; ++k) lanes.push_back({ { x, L.top + between(r, 20.0f, 300.0f) }, between(r, 90.0f, 180.0f) });
    for (auto o : L.open)
        for (int k = 0; k < 3; ++k) lanes.push_back({ { o.getX() + o.getWidth() * between(r, 0.1f, 0.9f), o.getY() }, o.getHeight() });
    a.shapes.push_back({ farTrees, juce::Colour(0xff2c4c32) });
    a.shapes.push_back({ nearTrees, juce::Colour(0xff15291a) });
    a.shapes.push_back({ roots, juce::Colour(0xff6a4a2c) });
    a.shapes.push_back({ stems, juce::Colour(0xff2c4a22), 1.8f });
    a.shapes.push_back({ leavesDark, juce::Colour(0xff2f6a2a) });
    a.shapes.push_back({ leavesLight, juce::Colour(0xff5fa848) });
    a.shapes.push_back({ ferns, juce::Colour(0xff4f8a3a) });
    a.shapes.push_back({ moss, juce::Colour(0xff3d6b2e) });
    a.shapes.push_back({ mossLight, juce::Colour(0xff6a9a40) });
    a.shapes.push_back({ stalks, juce::Colour(0xffe0d6c0) });
    a.shapes.push_back({ capsRed, juce::Colour(0xffb03a2a) });
    a.shapes.push_back({ spots, juce::Colour(0xfff0e8d8) });
    a.shapes.push_back({ capsPale, juce::Colour(0xffc8f0d8) });

    // Falling leaves: six turns of a green and an amber leaf.
    const auto leafSprites = [&](juce::Colour colour) {
        const int first = int(a.sprites.size());
        for (int f = 0; f < 6; ++f) {
            juce::Path l;
            leaf(l, { 0.0f, 0.0f }, 8.0f, twoPi * float(f) / 6.0f);
            // Turning in the air: foreshortened on alternate frames.
            l.applyTransform(juce::AffineTransform::scale(1.0f, 0.55f + 0.45f * std::abs(std::cos(twoPi * float(f) / 6.0f))));
            auto image = pathSprite(l, colour);
            juce::Image framed(juce::Image::ARGB, 14, 14, true);
            juce::Graphics g(framed);
            g.drawImageAt(image, (14 - image.getWidth()) / 2, (14 - image.getHeight()) / 2);
            a.sprites.push_back(framed);
        }
        return first;
    };
    const int green = leafSprites(juce::Colour(0xff6aa84a)), amber = leafSprites(juce::Colour(0xffc89a3a));
    const int pollen = int(a.sprites.size());
    a.sprites.push_back(glowSprite(3.5f, juce::Colour(0x80e8ff9a), juce::Colour(0xfff8ffd8), 1.0f));
    const int capGlow = int(a.sprites.size());
    a.sprites.push_back(glowSprite(8.0f, juce::Colour(0x709ff0c0), juce::Colours::transparentBlack, 0.0f));
    for (std::size_t k = 0; k < lanes.size(); ++k)
        move(a, PlaceOverlays::MoveFall, lanes[k].first, k % 3 == 0 ? amber : green, between(r, 5.0f, 8.0f), r.nextFloat(), 0.95f, lanes[k].second, between(r, 3.0f, 5.0f), 6);
    const auto drift = spotsIn([&] {
        juce::RectangleList<int> area;
        for (auto gap : L.gutters) area.add(gap.toNearestInt());
        for (auto o : L.open) area.add(o.toNearestInt());
        for (auto l : L.ledges) area.add(l.toNearestInt());
        return area;
    }(), 10, 66);
    for (auto at : drift) move(a, PlaceOverlays::MoveDrift, at, pollen, between(r, 5.0f, 8.0f), r.nextFloat(), 0.9f, between(r, -14.0f, 14.0f), 5.0f);
    for (auto at : glowCaps) move(a, PlaceOverlays::MovePulse, at, capGlow, between(r, 2.5f, 4.0f), r.nextFloat());
    return a;
}

//==============================================================================
// Alien Hive: tendrils curling from the corners along the frames, a honeycomb of
// cells in the gaps and the open space, veins along the bottoms, clusters of egg
// pods; the pods pulse, spores rise, the tendril tips glow.
OverlayArt hive(const Layout& L) {
    OverlayArt a;
    juce::Random r(707);
    juce::Path bodies, suckers, comb, cells, veins, pods, podLight;
    std::vector<Point> podSpots, tips;
    const auto hexes = [&](Rect area, float size) {
        const float h = size * 1.732f;
        int row = 0;
        for (float y = area.getY(); y < area.getBottom() + h; y += h * 0.5f, ++row)
            for (float x = area.getX() + ((row & 1) != 0 ? size * 1.5f : 0.0f); x < area.getRight() + size; x += size * 3.0f) {
                juce::Path hex;
                hex.addPolygon({ x, y }, 6, size * 0.92f, 0.5236f);
                comb.addPath(hex);
                if (r.nextFloat() < 0.18f) cells.addPath(hex);
            }
    };
    for (auto gap : L.gutters) hexes(gap, 4.0f);
    for (auto ledge : L.ledges) hexes(ledge, 3.0f);
    for (auto o : L.open) hexes(o.reduced(2.0f), 5.0f);
    for (auto p : L.panels) {
        // Tendrils from the corners, along the frames.
        const float len = std::min(90.0f, p.getHeight() * 0.4f);
        tendril(bodies, suckers, { p.getX(), p.getBottom() }, -1.5708f, len * between(r, 0.8f, 1.2f), 6.0f, 0.5f, r);
        tendril(bodies, suckers, { p.getRight(), p.getBottom() }, -1.5708f, len * between(r, 0.7f, 1.1f), 6.0f, -0.5f, r);
        tendril(bodies, suckers, { p.getX(), p.getY() }, 0.0f, std::min(110.0f, p.getWidth() * 0.3f), 5.0f, 0.35f, r);
        tendril(bodies, suckers, { p.getRight(), p.getY() }, 3.1416f, std::min(80.0f, p.getWidth() * 0.25f), 5.0f, -0.35f, r);
        tips.push_back({ p.getX(), p.getBottom() - len * 0.95f });
        tips.push_back({ p.getRight(), p.getBottom() - len * 0.85f });
        // Veins along the bottom edge.
        juce::Path v;
        float y = p.getBottom() + 0.5f;
        v.startNewSubPath(p.getX() + 10.0f, y);
        for (float x = p.getX() + 16.0f; x < p.getRight() - 10.0f; x += 6.0f) {
            v.lineTo(x, y + 1.5f * std::sin(x * 0.09f + p.getY()));
            if (r.nextFloat() < 0.08f) {
                juce::Path branch;
                branch.startNewSubPath(x, y);
                branch.quadraticTo(x + 5.0f, y - 3.0f, x + between(r, 8.0f, 14.0f), y - between(r, 2.0f, 5.0f));
                juce::PathStrokeType(0.8f).createStrokedPath(branch, branch);
                veins.addPath(branch);
            }
        }
        juce::PathStrokeType(1.2f).createStrokedPath(v, v);
        veins.addPath(v);
        // Egg pods in the bottom corners.
        for (float dir : { 1.0f, -1.0f }) {
            const float cx = dir > 0 ? p.getX() + 6.0f : p.getRight() - 6.0f;
            for (int e = 0; e < 3; ++e) {
                const Point at { cx + dir * float(e) * 5.0f, p.getBottom() - 4.0f - float(e % 2) * 4.0f };
                pods.addEllipse(juce::Rectangle<float>(5.5f, 7.0f).withCentre(at));
                podLight.addEllipse(juce::Rectangle<float>(1.6f, 2.4f).withCentre(at.translated(-1.0f, -1.5f)));
            }
        }
    }
    // Pods that glow: at the foot of each gap and in the open space.
    for (auto gap : L.gutters) {
        const Point at { gap.getCentreX(), gap.getBottom() - 10.0f };
        for (int e = 0; e < 3; ++e) {
            const Point egg = at.translated(0.0f, -float(e) * 6.0f);
            pods.addEllipse(juce::Rectangle<float>(std::min(6.0f, gap.getWidth() - 4.0f), 7.0f).withCentre(egg));
            podLight.addEllipse(juce::Rectangle<float>(1.6f, 2.4f).withCentre(egg.translated(-1.0f, -1.5f)));
        }
        podSpots.push_back(at.translated(0.0f, -6.0f));
    }
    for (auto o : L.open)
        for (int c = 0; c < 3; ++c) {
            const Point at { o.getX() + o.getWidth() * (0.2f + 0.3f * float(c)), o.getBottom() - 8.0f };
            for (int e = 0; e < 4; ++e) {
                const Point egg = at.translated(float(e - 2) * 6.0f + 3.0f, -float(e % 2) * 5.0f);
                pods.addEllipse(juce::Rectangle<float>(7.0f, 9.0f).withCentre(egg));
                podLight.addEllipse(juce::Rectangle<float>(2.0f, 3.0f).withCentre(egg.translated(-1.3f, -2.0f)));
            }
            podSpots.push_back(at.translated(0.0f, -2.0f));
        }
    a.shapes.push_back({ comb, juce::Colour(0xff5a2470), 0.9f });
    a.shapes.push_back({ cells, juce::Colour(0xff2a1236) });
    a.shapes.push_back({ veins, juce::Colour(0xffa04ab8) });
    a.shapes.push_back({ bodies, juce::Colour(0xff6a2a7a) });
    a.shapes.push_back({ bodies, juce::Colour(0xffb060c8), 0.8f });
    a.shapes.push_back({ suckers, juce::Colour(0xffe0a0f0) });
    a.shapes.push_back({ pods, juce::Colour(0xff3a6a4a) });
    a.shapes.push_back({ pods, juce::Colour(0xff9affc8), 0.7f });
    a.shapes.push_back({ podLight, juce::Colour(0xffe0fff0) });

    const int magenta = int(a.sprites.size());
    a.sprites.push_back(glowSprite(9.0f, juce::Colour(0x80d07aff), juce::Colour(0xfff0c8ff), 1.6f));
    const int green = int(a.sprites.size());
    a.sprites.push_back(glowSprite(9.0f, juce::Colour(0x807affc0), juce::Colour(0xffd8fff0), 1.6f));
    const int spore = int(a.sprites.size());
    a.sprites.push_back(glowSprite(3.0f, juce::Colour(0x907affc0), juce::Colour(0xffe8fff4), 0.9f));
    const int tip = int(a.sprites.size());
    a.sprites.push_back(glowSprite(5.0f, juce::Colour(0x90ff7ae0), juce::Colour(0xffffd0f4), 1.2f));
    for (std::size_t i = 0; i < podSpots.size(); ++i) move(a, PlaceOverlays::MovePulse, podSpots[i], i % 2 == 0 ? green : magenta, between(r, 2.2f, 3.8f), r.nextFloat());
    for (int k = 0; k < 10 && !podSpots.empty(); ++k)
        move(a, PlaceOverlays::MoveRise, podSpots[std::size_t(r.nextInt(int(podSpots.size())))], spore, between(r, 4.0f, 6.0f), r.nextFloat(), 1.0f, between(r, 24.0f, 40.0f), 5.0f);
    for (auto t : tips) move(a, PlaceOverlays::MovePulse, t, tip, between(r, 2.0f, 3.5f), r.nextFloat());
    return a;
}

//==============================================================================
// Temple: gold trim along the frames with diamond studs, filigree in the corners,
// stone pillars in the gaps, a facade with a pediment and incense burners in the open
// space, stone steps along the bottom; incense smoke rises, lamps flicker, gold motes
// drift.
OverlayArt temple(const Layout& L) {
    OverlayArt a;
    juce::Random r(808);
    juce::Path gold, goldDark, studs, filigree, stone, stoneDark, pillarLines, red;
    std::vector<Point> burners, lamps;
    for (auto p : L.panels) {
        gold.addRoundedRectangle(p.getX() + 8.0f, p.getY() - 2.5f, p.getWidth() - 16.0f, 5.0f, 1.5f);
        for (float x = p.getX() + 14.0f; x < p.getRight() - 12.0f; x += 11.0f) {
            juce::Path d;
            d.addQuadrilateral(x, p.getY() - 2.0f, x + 2.0f, p.getY(), x, p.getY() + 2.0f, x - 2.0f, p.getY());
            studs.addPath(d);
        }
        goldDark.addRectangle(p.getX() + 14.0f, p.getBottom() - 0.6f, p.getWidth() - 28.0f, 1.2f);
        // Filigree in the corners: a diamond and two curls.
        for (auto corner : { p.getTopLeft(), p.getTopRight(), p.getBottomLeft(), p.getBottomRight() }) {
            const float dx = corner.x < p.getCentreX() ? 1.0f : -1.0f, dy = corner.y < p.getCentreY() ? 1.0f : -1.0f;
            const Point c = corner + Point { dx * 5.0f, dy * 5.0f };
            filigree.addQuadrilateral(c.x, c.y - 3.5f, c.x + 3.5f, c.y, c.x, c.y + 3.5f, c.x - 3.5f, c.y);
            juce::Path curl;
            curl.addCentredArc(c.x + dx * 6.0f, c.y, 2.5f, 2.5f, 0.0f, 0.0f, 4.7f, true);
            curl.addCentredArc(c.x, c.y + dy * 6.0f, 2.5f, 2.5f, 0.0f, 1.57f, 6.2f, true);
            juce::PathStrokeType(1.0f).createStrokedPath(curl, curl);
            filigree.addPath(curl);
        }
    }
    // Pillars in the gaps: a fluted shaft between a capital and a base.
    for (auto gap : L.gutters) {
        const float w = std::min(9.0f, gap.getWidth() - 3.0f), cx = gap.getCentreX();
        stone.addRectangle(cx - w * 0.5f, gap.getY() + 10.0f, w, gap.getHeight() - 20.0f);
        stoneDark.addRectangle(cx - w * 0.5f - 1.5f, gap.getY() + 6.0f, w + 3.0f, 4.0f);
        stoneDark.addRectangle(cx - w * 0.5f - 1.5f, gap.getBottom() - 10.0f, w + 3.0f, 4.0f);
        for (float fx = cx - w * 0.5f + 2.0f; fx < cx + w * 0.5f - 1.0f; fx += 2.5f) pillarLines.addRectangle(fx, gap.getY() + 12.0f, 0.7f, gap.getHeight() - 24.0f);
        gold.addRectangle(cx - w * 0.5f - 1.5f, gap.getY() + 10.0f, w + 3.0f, 1.2f);
        lamps.push_back({ cx, gap.getY() + 2.0f });
        burners.push_back({ cx, gap.getBottom() - 12.0f });
    }
    // The facade in the open space: steps, columns and a pediment, a burner each side.
    for (auto o : L.open) {
        const float cx = o.getCentreX(), base = o.getBottom(), w = std::min(110.0f, o.getWidth() * 0.45f), h = o.getHeight() - 4.0f;
        stone.addRectangle(cx - w * 0.5f - 6.0f, base - 3.0f, w + 12.0f, 3.0f);
        stone.addRectangle(cx - w * 0.5f - 3.0f, base - 6.0f, w + 6.0f, 3.0f);
        const int columns = 5;
        for (int c = 0; c < columns; ++c) {
            const float x = cx - w * 0.5f + 4.0f + (w - 8.0f) * float(c) / float(columns - 1);
            stone.addRectangle(x - 2.5f, base - h * 0.62f, 5.0f, h * 0.62f - 6.0f);
            pillarLines.addRectangle(x - 0.4f, base - h * 0.6f, 0.8f, h * 0.6f - 8.0f);
        }
        stoneDark.addRectangle(cx - w * 0.5f, base - h * 0.62f - 3.0f, w, 3.0f);
        juce::Path pediment;
        pediment.addTriangle(cx - w * 0.5f - 3.0f, base - h * 0.62f - 3.0f, cx + w * 0.5f + 3.0f, base - h * 0.62f - 3.0f, cx, base - h);
        stone.addPath(pediment);
        gold.addPath([&] {
            juce::Path edge;
            juce::PathStrokeType(1.2f).createStrokedPath(edge, pediment);
            return edge;
        }());
        red.addEllipse(juce::Rectangle<float>(4.0f, 4.0f).withCentre({ cx, base - h * 0.62f - 7.0f }));
        for (float side : { -1.0f, 1.0f }) {
            const float bx = cx + side * (w * 0.5f + 26.0f);
            if (bx < o.getX() + 6.0f || bx > o.getRight() - 6.0f) continue;
            juce::Path bowl;
            bowl.addPieSegment(bx - 6.0f, base - 12.0f, 12.0f, 9.0f, 1.5708f, 4.7124f, 0.0f);
            gold.addPath(bowl);
            goldDark.addRectangle(bx - 0.8f, base - 7.5f, 1.6f, 7.5f);
            goldDark.addRectangle(bx - 4.0f, base - 1.5f, 8.0f, 1.5f);
            burners.push_back({ bx, base - 9.0f });
        }
    }
    // Stone steps along the bottom.
    const float bottom = L.bounds.getBottom();
    for (float x = -8.0f, w = 0.0f; x < L.bounds.getRight(); x += w) {
        w = between(r, 26.0f, 44.0f);
        stone.addRectangle(x, bottom - 7.0f, w - 1.0f, 7.0f);
    }
    a.shapes.push_back({ stone, juce::Colour(0xffb8a888) });
    a.shapes.push_back({ stoneDark, juce::Colour(0xff7a6c52) });
    a.shapes.push_back({ pillarLines, juce::Colour(0xff8a7c60) });
    a.shapes.push_back({ gold, juce::Colour(0xffd8b060) });
    a.shapes.push_back({ goldDark, juce::Colour(0xff9a7834) });
    a.shapes.push_back({ studs, juce::Colour(0xfff4e0a0) });
    a.shapes.push_back({ filigree, juce::Colour(0xffe8c878) });
    a.shapes.push_back({ red, juce::Colour(0xffa83228) });

    const int smoke = puffSprites(a, juce::Colour(0x60e8dcc8), 1.6f);
    const int flame = int(a.sprites.size());
    {
        juce::Path f;
        f.startNewSubPath(0.0f, -6.0f);
        f.quadraticTo(3.2f, -1.0f, 0.0f, 2.0f);
        f.quadraticTo(-3.2f, -1.0f, 0.0f, -6.0f);
        f.closeSubPath();
        a.sprites.push_back(pathSprite(f, juce::Colour(0xffffe0a0), 4.0f, juce::Colour(0xffffa040)));
    }
    const int mote = int(a.sprites.size());
    a.sprites.push_back(glowSprite(2.5f, juce::Colour(0x90ffe0a0), juce::Colour(0xfffff4d0), 0.8f));
    for (auto b : burners)
        for (int k = 0; k < 3; ++k) move(a, PlaceOverlays::MoveRise, b, smoke, 4.5f, float(k) / 3.0f, 0.85f, 36.0f, 4.0f, 8);
    for (auto l : lamps) move(a, PlaceOverlays::MoveFlicker, l.translated(0.0f, 0.0f), flame, 1.0f, 0.0f);
    for (auto p : L.panels)
        if (p.getY() > L.top + 2.0f) move(a, PlaceOverlays::MoveFlicker, { p.getCentreX(), p.getY() - 6.0f }, flame, 1.0f, 0.0f);
    juce::RectangleList<int> air;
    for (auto o : L.open) air.add(o.toNearestInt());
    for (auto l : L.ledges) air.add(l.toNearestInt());
    for (auto gap : L.gutters) air.add(gap.toNearestInt());
    for (auto at : spotsIn(air, 10, 88)) move(a, PlaceOverlays::MoveDrift, at, mote, between(r, 6.0f, 9.0f), r.nextFloat(), 1.0f, between(r, -20.0f, 20.0f), 6.0f);
    return a;
}

//==============================================================================
// Cavern: jagged rock along the bottom and over the frames, stalactites hanging from
// the frames and the top, crystal clusters in the corners and the open space; water
// drips from the stalactites, crystals glint and glow.
OverlayArt cavern(const Layout& L) {
    OverlayArt a;
    juce::Random r(909);
    juce::Path rock, rockLight, drips, cyanLit, cyanShade, violetLit, violetShade;
    std::vector<std::pair<Point, float>> dripTips;   // a tip and how far a drop falls
    std::vector<Point> glints, glows;
    const auto stalactite = [&](float x, float y, float length, float width, float fall) {
        juce::Path s;
        s.startNewSubPath(x - width * 0.5f, y);
        s.quadraticTo(x - width * 0.2f, y + length * 0.6f, x, y + length);
        s.quadraticTo(x + width * 0.2f, y + length * 0.6f, x + width * 0.5f, y);
        s.closeSubPath();
        rock.addPath(s);
        rockLight.addRectangle(x - width * 0.35f, y, 0.9f, length * 0.4f);
        if (fall > 6.0f) dripTips.push_back({ { x, y + length }, fall });
    };
    const auto cluster = [&](Point base, float up, float size, bool violet, bool lively) {
        const int count = 3 + r.nextInt(2);
        for (int c = 0; c < count; ++c) {
            const float angle = up + between(r, -0.7f, 0.7f), length = size * between(r, 0.55f, 1.0f);
            crystal(violet ? violetLit : cyanLit, violet ? violetShade : cyanShade, base.translated(between(r, -3.0f, 3.0f), 0.0f), angle, length, length * 0.32f);
            if (c == 0 && lively) glints.push_back(base + Point { std::cos(angle), std::sin(angle) } * length);
        }
        if (lively) glows.push_back(base + Point { std::cos(up), std::sin(up) } * size * 0.4f);
    };
    // Rock along the bottom of the window, and stalagmites on it.
    const float bottom = L.bounds.getBottom(), width = L.bounds.getWidth();
    juce::Path ground;
    ground.startNewSubPath(0.0f, bottom);
    for (float x = 0.0f; x <= width; x += between(r, 6.0f, 14.0f)) ground.lineTo(x, bottom - between(r, 2.0f, 8.0f));
    ground.lineTo(width, bottom);
    ground.closeSubPath();
    rock.addPath(ground);
    for (float x = between(r, 20.0f, 60.0f); x < width; x += between(r, 60.0f, 130.0f)) {
        const float h = between(r, 6.0f, 12.0f), w = between(r, 5.0f, 8.0f);
        rock.addTriangle(x - w * 0.5f, bottom - 3.0f, x + w * 0.5f, bottom - 3.0f, x, bottom - 3.0f - h);
    }
    // Overhangs on the top frames, stalactites hanging from the frames into the gaps.
    for (auto p : L.panels) {
        juce::Path ledge;
        ledge.startNewSubPath(p.getX() + 8.0f, p.getY() - 3.0f);
        for (float x = p.getX() + 8.0f; x <= p.getRight() - 8.0f; x += between(r, 5.0f, 10.0f)) ledge.lineTo(x, p.getY() + between(r, 0.5f, 5.0f));
        ledge.lineTo(p.getRight() - 8.0f, p.getY() - 3.0f);
        ledge.closeSubPath();
        rock.addPath(ledge);
        rockLight.addRectangle(p.getX() + 8.0f, p.getY() - 3.0f, p.getWidth() - 16.0f, 1.2f);
        // Small crystals on the top frame.
        for (float x = p.getX() + between(r, 40.0f, 90.0f); x < p.getRight() - 40.0f; x += between(r, 90.0f, 180.0f))
            for (int c = 0; c < 3; ++c) {
                const float angle = -1.5708f + between(r, -0.6f, 0.6f), length = between(r, 6.0f, 10.0f);
                crystal(r.nextBool() ? cyanLit : violetLit, r.nextBool() ? cyanShade : violetShade, { x + between(r, -4.0f, 4.0f), p.getY() - 2.0f }, angle, length, length * 0.35f);
            }
        for (float x = p.getX() + between(r, 14.0f, 30.0f); x < p.getRight() - 14.0f; x += between(r, 22.0f, 48.0f)) {
            // Below a panel: hang into the gap beneath it (and let the drop fall to the next frame).
            float gapBelow = 0;
            for (auto l : L.ledges)
                if (std::abs(l.getY() - p.getBottom()) < 1.5f && x > l.getX() && x < l.getRight()) gapBelow = l.getHeight();
            if (gapBelow <= 0 && p.getBottom() + 12.0f < bottom - 4.0f) continue;
            const float length = std::max(3.0f, std::min(gapBelow > 0 ? gapBelow - 2.0f : 8.0f, between(r, 4.0f, 9.0f)));
            stalactite(x, p.getBottom(), length, between(r, 3.0f, 5.0f), (gapBelow > 0 ? gapBelow : bottom - p.getBottom()) - length);
        }
        // Crystal clusters in the bottom corners, growing out and up.
        if (r.nextFloat() < 0.8f) cluster({ p.getX() + 3.0f, p.getBottom() + 1.0f }, -1.1f, between(r, 13.0f, 18.0f), r.nextBool(), false);
        if (r.nextFloat() < 0.8f) cluster({ p.getRight() - 3.0f, p.getBottom() + 1.0f }, -2.0f, between(r, 13.0f, 18.0f), r.nextBool(), false);
    }
    // In the gaps: stalactites from the top, a cluster at the bottom.
    for (auto gap : L.gutters) {
        stalactite(gap.getCentreX(), gap.getY(), between(r, 14.0f, 24.0f), 5.0f, gap.getHeight() * 0.6f);
        cluster({ gap.getCentreX(), gap.getBottom() - 2.0f }, -1.5708f, std::min(gap.getWidth() * 1.4f, 18.0f), r.nextBool(), true);
    }
    // The open space: stalactites from its top, a large cluster at each end.
    for (auto o : L.open) {
        for (float x = o.getX() + between(r, 10.0f, 20.0f); x < o.getRight() - 10.0f; x += between(r, 16.0f, 30.0f))
            stalactite(x, o.getY(), between(r, 6.0f, o.getHeight() * 0.45f), between(r, 4.0f, 7.0f), o.getHeight() * 0.5f);
        cluster({ o.getX() + 22.0f, o.getBottom() }, -1.4f, std::min(o.getHeight() * 0.8f, 30.0f), false, true);
        cluster({ o.getRight() - 22.0f, o.getBottom() }, -1.75f, std::min(o.getHeight() * 0.7f, 26.0f), true, true);
    }
    // Up the window's sides: rough rock walls.
    for (float x : { 0.0f, width }) {
        juce::Path wall;
        const float dir = x < 1.0f ? 1.0f : -1.0f;
        wall.startNewSubPath(x, L.top);
        for (float y = L.top; y < bottom; y += between(r, 8.0f, 18.0f)) wall.lineTo(x + dir * between(r, 2.0f, 9.0f), y);
        wall.lineTo(x, bottom);
        wall.closeSubPath();
        rock.addPath(wall);
    }
    a.shapes.push_back({ rock, juce::Colour(0xff34404c) });
    a.shapes.push_back({ rock, juce::Colour(0xff58687a), 0.8f });
    a.shapes.push_back({ rockLight, juce::Colour(0xff7a8ca0) });
    a.shapes.push_back({ cyanShade, juce::Colour(0xff1f6f7a) });
    a.shapes.push_back({ cyanLit, juce::Colour(0xff7ae8e0) });
    a.shapes.push_back({ violetShade, juce::Colour(0xff4a3a8a) });
    a.shapes.push_back({ violetLit, juce::Colour(0xffa898ff) });

    const int drop = int(a.sprites.size());
    {
        juce::Path d;
        d.startNewSubPath(0.0f, -3.0f);
        d.quadraticTo(1.8f, 0.5f, 0.0f, 1.8f);
        d.quadraticTo(-1.8f, 0.5f, 0.0f, -3.0f);
        d.closeSubPath();
        a.sprites.push_back(pathSprite(d, juce::Colour(0xffc8f4ff), 1.5f, juce::Colour(0xff7ad8ff)));
    }
    const int glint = int(a.sprites.size());
    a.sprites.push_back(sparkleSprite(5.0f, juce::Colour(0xffa8fff8)));
    const int glow = int(a.sprites.size());
    a.sprites.push_back(glowSprite(10.0f, juce::Colour(0x507ae8e0), juce::Colours::transparentBlack, 0.0f));
    for (int k = 0; k < 12 && !dripTips.empty(); ++k) {
        const auto& [tip, fall] = dripTips[std::size_t(r.nextInt(int(dripTips.size())))];
        move(a, PlaceOverlays::MoveFall, tip, drop, between(r, 2.5f, 5.0f), r.nextFloat(), 1.0f, std::min(fall, 60.0f), 0.0f);
    }
    for (auto g : glints) move(a, PlaceOverlays::MovePulse, g, glint, between(r, 1.8f, 3.6f), r.nextFloat());
    for (auto g : glows) move(a, PlaceOverlays::MovePulse, g, glow, between(r, 3.0f, 5.0f), r.nextFloat(), 0.8f);
    return a;
}

//==============================================================================
// Ruins: cracks running along the frames, moss over the corners and the broken
// stonework along the bottom, a broken colonnade in the open space; dust drifts,
// pebbles fall from the cracks, pale wisps glow and fade.
OverlayArt ruins(const Layout& L) {
    OverlayArt a;
    juce::Random r(1010);
    juce::Path stone, stoneDark, cracks, moss, mossLight, ivy;
    std::vector<Point> crumbs, wisps;
    // A crack: a jagged line wandering along `u` from `start`, with a branch or two.
    const auto crack = [&](Point start, Point u, float length) {
        const Point n { -u.y, u.x };
        juce::Path c;
        c.startNewSubPath(start);
        Point p = start;
        for (float s = 0.0f; s < length; s += between(r, 4.0f, 8.0f)) {
            p = start + u * s + n * between(r, -2.2f, 2.2f);
            c.lineTo(p);
            if (r.nextFloat() < 0.12f) {
                juce::Path b;
                b.startNewSubPath(p);
                b.lineTo(p + u * between(r, 3.0f, 7.0f) + n * between(r, -6.0f, 6.0f));
                juce::PathStrokeType(0.8f).createStrokedPath(b, b);
                cracks.addPath(b);
            }
        }
        juce::PathStrokeType(1.1f).createStrokedPath(c, c);
        cracks.addPath(c);
        crumbs.push_back(p);
    };
    // Moss: a cushion of small round tufts.
    const auto cushion = [&](Point at, float w, float h) {
        for (int k = 0; k < int(w * 0.6f); ++k) {
            const float x = at.x + between(r, -w * 0.5f, w * 0.5f), y = at.y + between(r, -h * 0.5f, h * 0.5f), d = between(r, 2.0f, 4.5f);
            (r.nextFloat() < 0.35f ? mossLight : moss).addEllipse(x - d * 0.5f, y - d * 0.5f, d, d);
        }
    };
    for (auto p : L.panels) {
        // Cracks along the top and down one side.
        crack({ p.getX() + between(r, 20.0f, 80.0f), p.getY() + 0.5f }, { 1.0f, 0.0f }, between(r, 60.0f, 140.0f));
        if (r.nextBool()) crack({ p.getRight() - between(r, 30.0f, 90.0f), p.getBottom() - 0.5f }, { -1.0f, 0.0f }, between(r, 40.0f, 100.0f));
        crack({ r.nextBool() ? p.getX() + 0.5f : p.getRight() - 0.5f, p.getY() + between(r, 10.0f, 30.0f) }, { 0.0f, 1.0f }, between(r, 30.0f, std::max(31.0f, p.getHeight() * 0.5f)));
        // Moss over the corners and in tufts along the bottom edge; ivy hanging from the top.
        cushion({ p.getX() + 6.0f, p.getY() + 1.0f }, 20.0f, 6.0f);
        cushion({ p.getRight() - 6.0f, p.getBottom() - 1.0f }, 22.0f, 6.0f);
        for (float x = p.getX() + between(r, 40.0f, 90.0f); x < p.getRight() - 30.0f; x += between(r, 80.0f, 160.0f)) cushion({ x, p.getBottom() + 0.5f }, between(r, 10.0f, 22.0f), 4.0f);
        juce::Path leavesB;
        vine(ivy, moss, leavesB, { p.getX() + between(r, 30.0f, 60.0f), p.getY() }, { p.getX() + between(r, 30.0f, 60.0f), p.getY() + between(r, 18.0f, 30.0f) }, r, 0.7f);
        mossLight.addPath(leavesB);
    }
    // Broken stonework along the bottom: blocks of uneven height, some fallen.
    const float bottom = L.bounds.getBottom();
    for (float x = -6.0f, w = 0.0f; x < L.bounds.getRight(); x += w + between(r, 1.0f, 3.0f)) {
        w = between(r, 18.0f, 34.0f);
        const float h = between(r, 4.0f, 9.0f);
        stone.addRectangle(x, bottom - h, w, h);
        stoneDark.addRectangle(x, bottom - h, w, 1.2f);
        if (r.nextFloat() < 0.3f) cushion({ x + w * 0.5f, bottom - h }, w * 0.6f, 3.0f);
    }
    // In the gaps: a crumbling pillar, broken off at different heights.
    for (auto gap : L.gutters) {
        const float w = std::min(9.0f, gap.getWidth() - 3.0f), cx = gap.getCentreX(), top = gap.getY() + gap.getHeight() * between(r, 0.25f, 0.55f);
        juce::Path shaft;
        shaft.startNewSubPath(cx - w * 0.5f, gap.getBottom() - 6.0f);
        shaft.lineTo(cx - w * 0.5f, top + 3.0f);
        shaft.lineTo(cx - w * 0.1f, top);
        shaft.lineTo(cx + w * 0.2f, top + 4.0f);
        shaft.lineTo(cx + w * 0.5f, top + 1.0f);
        shaft.lineTo(cx + w * 0.5f, gap.getBottom() - 6.0f);
        shaft.closeSubPath();
        stone.addPath(shaft);
        stoneDark.addRectangle(cx - w * 0.5f - 1.5f, gap.getBottom() - 8.0f, w + 3.0f, 3.0f);
        cushion({ cx, top + 2.0f }, w, 3.0f);
        crumbs.push_back({ cx, top + 2.0f });
        wisps.push_back({ cx, top - 14.0f });
    }
    // The open space: a broken colonnade under an arch that has fallen in.
    for (auto o : L.open) {
        const float base = o.getBottom(), h = o.getHeight() - 4.0f;
        const int columns = std::max(3, int(o.getWidth() / 60.0f));
        for (int c = 0; c < columns; ++c) {
            const float x = o.getX() + 20.0f + (o.getWidth() - 40.0f) * float(c) / float(columns - 1);
            const float height = h * (c % 3 == 1 ? between(r, 0.3f, 0.5f) : between(r, 0.6f, 0.95f));
            stone.addRectangle(x - 3.0f, base - height, 6.0f, height);
            stoneDark.addRectangle(x - 4.5f, base - 3.0f, 9.0f, 3.0f);
            stoneDark.addRectangle(x - 0.4f, base - height + 2.0f, 0.8f, height - 5.0f);
            cushion({ x, base - height }, 8.0f, 3.0f);
            if (c % 2 == 0) wisps.push_back({ x + between(r, 10.0f, 25.0f), base - h * 0.6f });
        }
        // A fallen lintel at the foot of the colonnade.
        juce::Path lintel;
        lintel.addRectangle(0.0f, 0.0f, 34.0f, 6.0f);
        stone.addPath(lintel, juce::AffineTransform::rotation(0.12f).translated(o.getX() + o.getWidth() * 0.45f, base - 7.0f));
    }
    a.shapes.push_back({ stone, juce::Colour(0xff7c7a70) });
    a.shapes.push_back({ stoneDark, juce::Colour(0xff4e4c46) });
    a.shapes.push_back({ cracks, juce::Colour(0xff2a2826) });
    a.shapes.push_back({ ivy, juce::Colour(0xff3c5a2c), 1.2f });
    a.shapes.push_back({ moss, juce::Colour(0xff4a7a34) });
    a.shapes.push_back({ mossLight, juce::Colour(0xff86b450) });

    const int pebble = int(a.sprites.size());
    {
        juce::Path p;
        p.addEllipse(-1.5f, -1.2f, 3.0f, 2.4f);
        a.sprites.push_back(pathSprite(p, juce::Colour(0xff9a968a)));
    }
    const int dust = int(a.sprites.size());
    a.sprites.push_back(glowSprite(2.2f, juce::Colour(0x70d8d0b8), juce::Colour(0xb0f0e8d0), 0.7f));
    const int wisp = int(a.sprites.size());
    a.sprites.push_back(glowSprite(9.0f, juce::Colour(0x40c8e0ff), juce::Colour(0x60e8f4ff), 2.0f));
    for (int k = 0; k < 10 && !crumbs.empty(); ++k)
        move(a, PlaceOverlays::MoveFall, crumbs[std::size_t(r.nextInt(int(crumbs.size())))], pebble, between(r, 3.5f, 7.0f), r.nextFloat(), 1.0f, between(r, 8.0f, 22.0f), 1.0f);
    for (auto w : wisps) move(a, PlaceOverlays::MovePulse, w, wisp, between(r, 4.0f, 7.0f), r.nextFloat(), 0.9f);
    juce::RectangleList<int> air;
    for (auto o : L.open) air.add(o.toNearestInt());
    for (auto l : L.ledges) air.add(l.toNearestInt());
    for (auto gap : L.gutters) air.add(gap.toNearestInt());
    for (auto at : spotsIn(air, 12, 1011)) move(a, PlaceOverlays::MoveDrift, at, dust, between(r, 7.0f, 11.0f), r.nextFloat(), 0.9f, between(r, -24.0f, 24.0f), 5.0f);
    return a;
}

//==============================================================================
// Spaceship Interior: riveted hull plating with seams on the frames, conduits down
// the gaps, a deck grating along the bottom, consoles with screens in the open space;
// status lights blink, data pulses run down the conduits, warning lamps glow.
OverlayArt spaceship(const Layout& L) {
    OverlayArt a;
    juce::Random r(1111);
    juce::Path plate, plateLight, seams, rivets, conduit, conduitLight, grating, console, screen, stripe;
    std::vector<Point> leds, lamps;
    std::vector<std::pair<Point, float>> runs;   // conduit tops and lengths
    for (auto p : L.panels) {
        plate.addRectangle(p.getX() + 6.0f, p.getY() - 2.5f, p.getWidth() - 12.0f, 5.0f);
        plateLight.addRectangle(p.getX() + 6.0f, p.getY() - 2.5f, p.getWidth() - 12.0f, 1.0f);
        for (float x = p.getX() + 30.0f; x < p.getRight() - 20.0f; x += between(r, 50.0f, 90.0f)) seams.addRectangle(x, p.getY() - 2.5f, 1.0f, 5.0f);
        for (float x = p.getX() + 10.0f; x < p.getRight() - 8.0f; x += 12.0f) rivets.addEllipse(x - 0.8f, p.getY() - 0.8f, 1.6f, 1.6f);
        // Bracket plates on the corners, a status light on each.
        for (auto corner : { p.getTopLeft(), p.getTopRight(), p.getBottomLeft(), p.getBottomRight() }) {
            const float dx = corner.x < p.getCentreX() ? 1.0f : -1.0f, dy = corner.y < p.getCentreY() ? 1.0f : -1.0f;
            juce::Path b;
            b.startNewSubPath(corner.x - dx * 2.0f, corner.y + dy * 16.0f);
            b.lineTo(corner.x - dx * 2.0f, corner.y - dy * 2.0f);
            b.lineTo(corner.x + dx * 16.0f, corner.y - dy * 2.0f);
            juce::PathStrokeType(3.0f).createStrokedPath(b, b);
            plate.addPath(b);
            if (dy > 0) leds.push_back({ corner.x + dx * 9.0f, corner.y - 1.0f });
        }
        stripe.addRectangle(p.getX() + p.getWidth() * 0.4f, p.getBottom() - 1.5f, p.getWidth() * 0.2f, 3.0f);
    }
    // Conduits down the gaps, with clamps; data runs down them.
    for (auto gap : L.gutters) {
        const float cx = gap.getCentreX();
        for (float off : { -2.5f, 2.5f }) {
            conduit.addRectangle(cx + off - 1.5f, gap.getY() + 8.0f, 3.0f, gap.getHeight() - 16.0f);
            conduitLight.addRectangle(cx + off - 1.0f, gap.getY() + 8.0f, 0.8f, gap.getHeight() - 16.0f);
        }
        for (float y = gap.getY() + 24.0f; y < gap.getBottom() - 16.0f; y += between(r, 50.0f, 80.0f)) plate.addRectangle(cx - 5.0f, y, 10.0f, 3.0f);
        runs.push_back({ { cx - 2.5f, gap.getY() + 10.0f }, gap.getHeight() - 24.0f });
        lamps.push_back({ cx, gap.getY() + 4.0f });
    }
    // Hazard stripes on the ledges.
    for (auto ledge : L.ledges) {
        const auto band = ledge.withSizeKeepingCentre(ledge.getWidth() - 20.0f, std::min(4.0f, ledge.getHeight() - 2.0f));
        for (float x = band.getX(); x < band.getRight() - 6.0f; x += 10.0f) {
            juce::Path s;
            s.addQuadrilateral(x, band.getBottom(), x + 5.0f, band.getBottom(), x + 5.0f + band.getHeight(), band.getY(), x + band.getHeight(), band.getY());
            stripe.addPath(s);
        }
    }
    // A deck grating along the bottom.
    const float bottom = L.bounds.getBottom();
    grating.addRectangle(0.0f, bottom - 6.0f, L.bounds.getWidth(), 6.0f);
    for (float x = 2.0f; x < L.bounds.getRight(); x += 6.0f) seams.addRectangle(x, bottom - 5.0f, 1.2f, 4.0f);
    // Consoles in the open space: a desk with a screen and a row of lights.
    for (auto o : L.open) {
        const int count = std::max(2, int(o.getWidth() / 120.0f));
        for (int c = 0; c < count; ++c) {
            const float cx = o.getX() + o.getWidth() * (float(c) + 0.5f) / float(count), base = o.getBottom();
            const float w = std::min(56.0f, o.getWidth() / float(count) - 12.0f), h = std::min(o.getHeight() - 6.0f, 26.0f);
            console.addRectangle(cx - w * 0.5f, base - h * 0.45f, w, h * 0.45f);
            console.addRectangle(cx - w * 0.35f, base - h, w * 0.7f, h * 0.5f);
            screen.addRectangle(cx - w * 0.3f, base - h + 2.0f, w * 0.6f, h * 0.5f - 4.0f);
            for (int k = 0; k < 4; ++k) leds.push_back({ cx - w * 0.3f + w * 0.2f * float(k), base - h * 0.25f });
        }
    }
    a.shapes.push_back({ plate, juce::Colour(0xff5a6470) });
    a.shapes.push_back({ plateLight, juce::Colour(0xff9aa8b8) });
    a.shapes.push_back({ seams, juce::Colour(0xff2c323a) });
    a.shapes.push_back({ rivets, juce::Colour(0xffb0bccc) });
    a.shapes.push_back({ conduit, juce::Colour(0xff3a4450) });
    a.shapes.push_back({ conduitLight, juce::Colour(0xff7a90a8) });
    a.shapes.push_back({ grating, juce::Colour(0xff3c444e) });
    a.shapes.push_back({ console, juce::Colour(0xff46505c) });
    a.shapes.push_back({ screen, juce::Colour(0xff0e3a44) });
    a.shapes.push_back({ stripe, juce::Colour(0xffd0a030) });

    const int green = int(a.sprites.size());
    a.sprites.push_back(glowSprite(3.5f, juce::Colour(0x8060ffa0), juce::Colour(0xffc0ffd8), 1.0f));
    const int amber = int(a.sprites.size());
    a.sprites.push_back(glowSprite(3.5f, juce::Colour(0x80ffb040), juce::Colour(0xffffe0b0), 1.0f));
    const int red = int(a.sprites.size());
    a.sprites.push_back(glowSprite(7.0f, juce::Colour(0x70ff4040), juce::Colour(0xffffb0a0), 2.0f));
    const int data = int(a.sprites.size());
    a.sprites.push_back(glowSprite(3.0f, juce::Colour(0x9060e0ff), juce::Colour(0xffd0f8ff), 1.0f));
    for (std::size_t i = 0; i < leds.size(); ++i)
        move(a, i % 3 == 0 ? PlaceOverlays::MoveBlink : PlaceOverlays::MovePulse, leds[i], i % 4 == 1 ? amber : green, between(r, 0.8f, 2.6f), r.nextFloat());
    for (auto l : lamps) move(a, PlaceOverlays::MovePulse, l, red, 2.0f, r.nextFloat(), 0.9f);
    for (const auto& [top, length] : runs)
        for (int k = 0; k < 2; ++k) move(a, PlaceOverlays::MoveFall, top.translated(k == 0 ? 0.0f : 5.0f, 0.0f), data, between(r, 2.0f, 3.5f), r.nextFloat(), 1.0f, length, 0.0f);
    return a;
}

//==============================================================================
// Biodome: a geodesic lattice in the open space, ferns and broad leaves along the
// bottom and up the corners, irrigation pipes on the frames; pollen rises, water
// drops fall from the pipes, grow lights glow.
OverlayArt biodome(const Layout& L) {
    OverlayArt a;
    juce::Random r(1212);
    juce::Path lattice, frond, frondLight, stems, pipe, pipeLight, soil;
    std::vector<Point> drops, lights;
    const auto fern = [&](Point base, float up, float length) {
        // A frond: a curved stem with leaflets on both sides.
        const int steps = 9;
        juce::Path stem;
        stem.startNewSubPath(base);
        float angle = up;
        Point p = base;
        for (int i = 1; i <= steps; ++i) {
            angle += 0.06f * (up < -1.5708f ? -1.0f : 1.0f);
            p += Point { std::cos(angle), std::sin(angle) } * (length / float(steps));
            stem.lineTo(p);
            const float size = length * 0.28f * (1.0f - float(i) / float(steps + 2));
            leaf(r.nextBool() ? frond : frondLight, p, size, angle - 0.9f);
            leaf(r.nextBool() ? frond : frondLight, p, size, angle + 0.9f);
        }
        stems.addPath(stem);
    };
    // The dome's lattice: triangles under an arc in each open space.
    for (auto o : L.open) {
        const float cx = o.getCentreX(), base = o.getBottom(), rx = std::min(o.getWidth() * 0.45f, 160.0f), ry = o.getHeight() - 3.0f;
        juce::Path arc;
        arc.addCentredArc(cx, base, rx, ry, 0.0f, -1.5708f, 1.5708f, true);
        lattice.addPath(arc);
        const int ribs = 9;
        for (int k = 1; k < ribs; ++k) {
            const float t = -1.5708f + 3.1416f * float(k) / float(ribs);
            lattice.startNewSubPath(cx, base);
            lattice.lineTo(cx + rx * std::sin(t), base - ry * std::cos(t));
        }
        for (float f : { 0.45f, 0.75f }) {
            juce::Path ring;
            ring.addCentredArc(cx, base, rx * f, ry * f, 0.0f, -1.5708f, 1.5708f, true);
            lattice.addPath(ring);
        }
        lights.push_back({ cx, base - ry * 0.85f });
        fern({ o.getX() + 14.0f, base }, -1.2f, std::min(o.getHeight() * 0.8f, 28.0f));
        fern({ o.getRight() - 14.0f, base }, -1.9f, std::min(o.getHeight() * 0.8f, 28.0f));
    }
    juce::PathStrokeType(1.0f).createStrokedPath(lattice, lattice);
    // Irrigation pipes along the top frames, dripping; ferns from the bottom corners.
    for (auto p : L.panels) {
        pipe.addRoundedRectangle(p.getX() + 10.0f, p.getY() - 2.0f, p.getWidth() - 20.0f, 4.0f, 2.0f);
        pipeLight.addRectangle(p.getX() + 10.0f, p.getY() - 1.5f, p.getWidth() - 20.0f, 0.9f);
        for (float x = p.getX() + between(r, 30.0f, 70.0f); x < p.getRight() - 20.0f; x += between(r, 70.0f, 140.0f)) {
            pipe.addRectangle(x - 2.0f, p.getY() - 3.0f, 4.0f, 6.0f);
            drops.push_back({ x, p.getY() + 3.0f });
        }
        fern({ p.getX() + 2.0f, p.getBottom() + 1.0f }, -1.25f, between(r, 16.0f, 24.0f));
        fern({ p.getRight() - 2.0f, p.getBottom() + 1.0f }, -1.9f, between(r, 16.0f, 24.0f));
    }
    // Planters along the bottom, full of leaves.
    const float bottom = L.bounds.getBottom();
    soil.addRectangle(0.0f, bottom - 5.0f, L.bounds.getWidth(), 5.0f);
    for (float x = between(r, 4.0f, 12.0f); x < L.bounds.getRight(); x += between(r, 8.0f, 16.0f))
        leaf(r.nextBool() ? frond : frondLight, { x, bottom - 4.0f }, between(r, 7.0f, 13.0f), -1.5708f + between(r, -0.8f, 0.8f));
    // Vines up the gaps.
    for (auto gap : L.gutters) {
        vine(stems, frond, frondLight, { gap.getCentreX(), gap.getBottom() - 4.0f }, { gap.getCentreX(), gap.getY() + gap.getHeight() * between(r, 0.3f, 0.6f) }, r, 0.8f);
        lights.push_back({ gap.getCentreX(), gap.getY() + 5.0f });
    }
    a.shapes.push_back({ lattice, juce::Colour(0x90b8e8f0) });
    a.shapes.push_back({ soil, juce::Colour(0xff3a2c20) });
    a.shapes.push_back({ pipe, juce::Colour(0xffa8b8c0) });
    a.shapes.push_back({ pipeLight, juce::Colour(0xffe8f4f8) });
    a.shapes.push_back({ stems, juce::Colour(0xff2e6a30), 1.2f });
    a.shapes.push_back({ frond, juce::Colour(0xff38a048) });
    a.shapes.push_back({ frondLight, juce::Colour(0xff8ade6a) });

    const int drop = int(a.sprites.size());
    {
        juce::Path d;
        d.startNewSubPath(0.0f, -2.5f);
        d.quadraticTo(1.6f, 0.5f, 0.0f, 1.6f);
        d.quadraticTo(-1.6f, 0.5f, 0.0f, -2.5f);
        d.closeSubPath();
        a.sprites.push_back(pathSprite(d, juce::Colour(0xffc8f0ff), 1.2f, juce::Colour(0xff70d0ff)));
    }
    const int pollen = int(a.sprites.size());
    a.sprites.push_back(glowSprite(2.4f, juce::Colour(0x90f8f080), juce::Colour(0xfffffff0), 0.8f));
    const int grow = int(a.sprites.size());
    a.sprites.push_back(glowSprite(9.0f, juce::Colour(0x50ff80f0), juce::Colour(0x80ffe0ff), 2.0f));
    for (auto d : drops) move(a, PlaceOverlays::MoveFall, d, drop, between(r, 2.5f, 4.5f), r.nextFloat(), 1.0f, between(r, 14.0f, 30.0f), 0.0f);
    for (auto l : lights) move(a, PlaceOverlays::MovePulse, l, grow, between(r, 3.0f, 5.0f), r.nextFloat(), 0.9f);
    juce::RectangleList<int> air;
    for (auto o : L.open) air.add(o.toNearestInt());
    for (auto l : L.ledges) air.add(l.toNearestInt());
    for (auto gap : L.gutters) air.add(gap.toNearestInt());
    for (auto at : spotsIn(air, 12, 1213)) move(a, PlaceOverlays::MoveRise, at, pollen, between(r, 6.0f, 10.0f), r.nextFloat(), 0.9f, between(r, 14.0f, 30.0f), 5.0f);
    return a;
}

//==============================================================================
// Sea Ship: twisted rope along the frames with knots in the corners, planking and
// waves along the bottom, portholes, a ship's wheel and rigging in the open space;
// light glints on the water, gulls drift past, the portholes shimmer.
OverlayArt seaship(const Layout& L) {
    OverlayArt a;
    juce::Random r(1313);
    juce::Path rope, ropeDark, planks, plankLines, water, foam, brass, glass, rigging, wood;
    std::vector<Point> glints, shimmer, knots;
    // Rope: a thick line with diagonal twists.
    const auto ropeAlong = [&](Point from, Point to) {
        const auto d = to - from;
        const float length = d.getDistanceFromOrigin();
        if (length < 6.0f) return;
        const Point u { d.x / length, d.y / length }, n { -u.y, u.x };
        juce::Path line;
        line.startNewSubPath(from);
        line.lineTo(to);
        juce::PathStrokeType(3.6f).createStrokedPath(line, line);
        rope.addPath(line);
        for (float s = 2.0f; s < length - 2.0f; s += 3.2f) {
            const Point c = from + u * s;
            ropeDark.startNewSubPath(c - n * 1.6f - u * 1.0f);
            ropeDark.lineTo(c + n * 1.6f + u * 1.0f);
        }
    };
    for (auto p : L.panels) {
        ropeAlong({ p.getX() + 10.0f, p.getY() }, { p.getRight() - 10.0f, p.getY() });
        ropeAlong({ p.getX(), p.getY() + 10.0f }, { p.getX(), p.getY() + std::min(60.0f, p.getHeight() * 0.4f) });
        ropeAlong({ p.getRight(), p.getY() + 10.0f }, { p.getRight(), p.getY() + std::min(60.0f, p.getHeight() * 0.4f) });
        for (auto corner : { p.getTopLeft(), p.getTopRight() }) knots.push_back(corner);
    }
    juce::PathStrokeType(0.9f).createStrokedPath(ropeDark, ropeDark);
    for (auto k : knots) {
        rope.addEllipse(juce::Rectangle<float>(9.0f, 9.0f).withCentre(k));
        ropeDark.addEllipse(juce::Rectangle<float>(3.0f, 3.0f).withCentre(k));
    }
    // Masts down the gaps, with rigging.
    for (auto gap : L.gutters) {
        const float cx = gap.getCentreX(), w = std::min(6.0f, gap.getWidth() - 4.0f);
        wood.addRectangle(cx - w * 0.5f, gap.getY() + 6.0f, w, gap.getHeight() - 12.0f);
        for (float y = gap.getY() + 30.0f; y < gap.getBottom() - 20.0f; y += between(r, 60.0f, 100.0f)) {
            brass.addRectangle(cx - w * 0.5f - 1.0f, y, w + 2.0f, 2.5f);
            shimmer.push_back({ cx, y + 1.0f });
        }
    }
    // The deck and the sea along the bottom: planks over a band of waves.
    const float bottom = L.bounds.getBottom(), width = L.bounds.getWidth();
    planks.addRectangle(0.0f, bottom - 11.0f, width, 5.0f);
    for (float x = between(r, 10.0f, 40.0f); x < width; x += between(r, 40.0f, 70.0f)) plankLines.addRectangle(x, bottom - 11.0f, 1.0f, 5.0f);
    water.startNewSubPath(0.0f, bottom);
    for (float x = 0.0f; x <= width; x += 4.0f) water.lineTo(x, bottom - 4.5f + 1.5f * std::sin(x * 0.09f));
    water.lineTo(width, bottom);
    water.closeSubPath();
    for (float x = between(r, 10.0f, 30.0f); x < width; x += between(r, 30.0f, 60.0f)) {
        foam.addRectangle(x, bottom - 5.0f + 1.5f * std::sin(x * 0.09f), between(r, 4.0f, 10.0f), 1.0f);
        if (glints.size() < 14) glints.push_back({ x + 3.0f, bottom - 3.0f });
    }
    // The open space: portholes, a ship's wheel, rigging lines.
    for (auto o : L.open) {
        const float cy = o.getCentreY(), radius = std::min(o.getHeight() * 0.32f, 11.0f);
        const int holes = std::max(2, int(o.getWidth() / 110.0f));
        for (int h = 0; h < holes; ++h) {
            const float cx = o.getX() + o.getWidth() * (float(h) + 0.5f) / float(holes);
            if (h == holes / 2) {
                // The wheel: a rim, spokes and handles.
                juce::Path wheel;
                wheel.addEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre({ cx, cy }));
                for (int s = 0; s < 8; ++s) {
                    const float angle = 0.3927f * float(2 * s);
                    wheel.startNewSubPath(cx, cy);
                    wheel.lineTo(cx + std::cos(angle) * radius * 1.35f, cy + std::sin(angle) * radius * 1.35f);
                }
                juce::PathStrokeType(2.0f).createStrokedPath(wheel, wheel);
                wood.addPath(wheel);
                brass.addEllipse(juce::Rectangle<float>(4.0f, 4.0f).withCentre({ cx, cy }));
                continue;
            }
            brass.addEllipse(juce::Rectangle<float>(radius * 2.0f + 4.0f, radius * 2.0f + 4.0f).withCentre({ cx, cy }));
            glass.addEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre({ cx, cy }));
            shimmer.push_back({ cx - radius * 0.3f, cy - radius * 0.3f });
        }
        rigging.startNewSubPath(o.getX() + 4.0f, o.getBottom());
        rigging.lineTo(o.getX() + o.getWidth() * 0.3f, o.getY() + 2.0f);
        rigging.startNewSubPath(o.getRight() - 4.0f, o.getBottom());
        rigging.lineTo(o.getRight() - o.getWidth() * 0.3f, o.getY() + 2.0f);
    }
    juce::PathStrokeType(1.0f).createStrokedPath(rigging, rigging);
    a.shapes.push_back({ water, juce::Colour(0xff1e4a6a) });
    a.shapes.push_back({ foam, juce::Colour(0xffc8e4f0) });
    a.shapes.push_back({ planks, juce::Colour(0xff7a5434) });
    a.shapes.push_back({ plankLines, juce::Colour(0xff4a3020) });
    a.shapes.push_back({ wood, juce::Colour(0xff8a5e38) });
    a.shapes.push_back({ rigging, juce::Colour(0xffb8a078) });
    a.shapes.push_back({ brass, juce::Colour(0xffc89a48) });
    a.shapes.push_back({ glass, juce::Colour(0xff2a6a8a) });
    a.shapes.push_back({ rope, juce::Colour(0xffc8a870) });
    a.shapes.push_back({ ropeDark, juce::Colour(0xff8a6a40) });

    const int glint = int(a.sprites.size());
    a.sprites.push_back(sparkleSprite(4.0f, juce::Colour(0xffc8f0ff)));
    const int sheen = int(a.sprites.size());
    a.sprites.push_back(glowSprite(5.0f, juce::Colour(0x60a8e8ff), juce::Colour(0xa0e8f8ff), 1.2f));
    const int gull = int(a.sprites.size());
    {
        juce::Path g;
        g.startNewSubPath(-5.0f, 0.0f);
        g.quadraticTo(-2.5f, -2.5f, 0.0f, 0.0f);
        g.quadraticTo(2.5f, -2.5f, 5.0f, 0.0f);
        juce::PathStrokeType(1.3f).createStrokedPath(g, g);
        a.sprites.push_back(pathSprite(g, juce::Colour(0xfff0f0e8)));
    }
    for (auto g : glints) move(a, PlaceOverlays::MovePulse, g, glint, between(r, 1.5f, 3.0f), r.nextFloat());
    for (auto s : shimmer) move(a, PlaceOverlays::MovePulse, s, sheen, between(r, 2.5f, 4.0f), r.nextFloat(), 0.9f);
    juce::RectangleList<int> air;
    for (auto o : L.open) air.add(o.toNearestInt());
    for (auto l : L.ledges) air.add(l.toNearestInt());
    for (auto at : spotsIn(air, 6, 1314)) move(a, PlaceOverlays::MoveDrift, at, gull, between(r, 8.0f, 12.0f), r.nextFloat(), 0.9f, between(r, -40.0f, 40.0f), 4.0f);
    return a;
}

//==============================================================================
// Suburbs: a picket fence and lawn along the bottom, hedges in the corners, rooftops
// with lit windows and a power line in the open space, a rain gutter on the frames;
// sprinkler drops fall, butterflies drift, windows glow.
OverlayArt suburbs(const Layout& L) {
    OverlayArt a;
    juce::Random r(1414);
    juce::Path fence, fenceShade, grassDark, grassLight, hedge, hedgeLight, roofs, walls, windows, wire, gutter;
    std::vector<Point> windowSpots, sprays;
    const float bottom = L.bounds.getBottom(), width = L.bounds.getWidth();
    // The fence: pickets with pointed tops on two rails.
    for (float x = 2.0f; x < width; x += 9.0f) {
        juce::Path picket;
        picket.startNewSubPath(x, bottom);
        picket.lineTo(x, bottom - 12.0f);
        picket.lineTo(x + 2.5f, bottom - 15.0f);
        picket.lineTo(x + 5.0f, bottom - 12.0f);
        picket.lineTo(x + 5.0f, bottom);
        picket.closeSubPath();
        fence.addPath(picket);
        fenceShade.addRectangle(x + 4.0f, bottom - 12.0f, 1.0f, 12.0f);
    }
    fence.addRectangle(0.0f, bottom - 10.0f, width, 2.0f);
    fence.addRectangle(0.0f, bottom - 5.0f, width, 2.0f);
    // Lawn in front of it.
    for (float x = 0.0f; x < width; x += between(r, 2.0f, 3.5f)) blade(r.nextBool() ? grassDark : grassLight, x, bottom + 1.0f, between(r, 3.0f, 6.5f), between(r, -2.0f, 2.0f), 2.0f);
    for (float x = between(r, 60.0f, 140.0f); x < width; x += between(r, 160.0f, 260.0f)) sprays.push_back({ x, bottom - 6.0f });
    // Hedges on the bottom corners of the panels, a gutter along their tops.
    const auto shrub = [&](Point at, float w) {
        for (int k = 0; k < int(w * 0.5f); ++k) {
            const float d = between(r, 4.0f, 7.0f);
            (r.nextFloat() < 0.3f ? hedgeLight : hedge).addEllipse(at.x + between(r, -w * 0.5f, w * 0.5f) - d * 0.5f, at.y - between(r, 0.0f, 6.0f) - d * 0.5f, d, d);
        }
    };
    for (auto p : L.panels) {
        shrub({ p.getX() + 8.0f, p.getBottom() + 1.0f }, 18.0f);
        shrub({ p.getRight() - 8.0f, p.getBottom() + 1.0f }, 18.0f);
        gutter.addRoundedRectangle(p.getX() + 12.0f, p.getY() - 2.0f, p.getWidth() - 24.0f, 3.5f, 1.5f);
    }
    // Lamp posts in the gaps.
    for (auto gap : L.gutters) {
        const float cx = gap.getCentreX();
        walls.addRectangle(cx - 1.2f, gap.getY() + 14.0f, 2.4f, gap.getHeight() - 20.0f);
        roofs.addRectangle(cx - 4.0f, gap.getY() + 10.0f, 8.0f, 4.0f);
        windowSpots.push_back({ cx, gap.getY() + 16.0f });
    }
    // Houses in the open space: walls, pitched roofs, windows, a power line over them.
    for (auto o : L.open) {
        const float base = o.getBottom();
        float x = o.getX() + 10.0f;
        while (x < o.getRight() - 40.0f) {
            const float w = between(r, 34.0f, 50.0f), h = std::min(o.getHeight() * 0.5f, between(r, 11.0f, 16.0f));
            walls.addRectangle(x, base - h, w, h);
            roofs.addTriangle(x - 3.0f, base - h, x + w + 3.0f, base - h, x + w * 0.5f, base - h - std::min(o.getHeight() * 0.35f, 12.0f));
            for (float wx = x + 5.0f; wx < x + w - 7.0f; wx += 11.0f) {
                windows.addRectangle(wx, base - h * 0.7f, 5.0f, 4.0f);
                if (r.nextFloat() < 0.5f) windowSpots.push_back({ wx + 2.5f, base - h * 0.7f + 2.0f });
            }
            x += w + between(r, 14.0f, 30.0f);
        }
        wire.startNewSubPath(o.getX(), o.getY() + 4.0f);
        wire.quadraticTo(o.getCentreX(), o.getY() + 12.0f, o.getRight(), o.getY() + 4.0f);
    }
    juce::PathStrokeType(0.8f).createStrokedPath(wire, wire);
    a.shapes.push_back({ walls, juce::Colour(0xffd8c8b0) });
    a.shapes.push_back({ roofs, juce::Colour(0xff8a4a3a) });
    a.shapes.push_back({ windows, juce::Colour(0xff3a4a5a) });
    a.shapes.push_back({ wire, juce::Colour(0xff2a2a2a) });
    a.shapes.push_back({ gutter, juce::Colour(0xffe8e4dc) });
    a.shapes.push_back({ hedge, juce::Colour(0xff3c7a3a) });
    a.shapes.push_back({ hedgeLight, juce::Colour(0xff6aaa50) });
    a.shapes.push_back({ fence, juce::Colour(0xfff4f0e8) });
    a.shapes.push_back({ fenceShade, juce::Colour(0xffb8b4ac) });
    a.shapes.push_back({ grassDark, juce::Colour(0xff4a8a38) });
    a.shapes.push_back({ grassLight, juce::Colour(0xff86c058) });

    const int glow = int(a.sprites.size());
    a.sprites.push_back(glowSprite(6.0f, juce::Colour(0x60ffd080), juce::Colour(0xc0fff0c0), 1.6f));
    const int drop = int(a.sprites.size());
    a.sprites.push_back(glowSprite(2.0f, juce::Colour(0x8090d8ff), juce::Colour(0xffe0f8ff), 0.8f));
    const int butterfly = int(a.sprites.size());
    for (float open : { 1.0f, 0.4f }) {
        juce::Path b;
        b.addEllipse(-3.2f * open - 0.3f, -2.4f, 3.2f * open, 3.2f);
        b.addEllipse(0.3f, -2.4f, 3.2f * open, 3.2f);
        b.addEllipse(-2.4f * open - 0.3f, 0.2f, 2.4f * open, 2.2f);
        b.addEllipse(0.3f, 0.2f, 2.4f * open, 2.2f);
        a.sprites.push_back(pathSprite(b, juce::Colour(0xfff8c040)));
    }
    for (auto w : windowSpots) move(a, PlaceOverlays::MovePulse, w, glow, between(r, 3.5f, 6.0f), r.nextFloat(), 0.8f);
    for (auto s : sprays)
        for (int k = 0; k < 4; ++k) move(a, PlaceOverlays::MoveRise, s.translated(between(r, -6.0f, 6.0f), 0.0f), drop, between(r, 1.5f, 2.5f), float(k) / 4.0f, 1.0f, between(r, 10.0f, 18.0f), 5.0f);
    juce::RectangleList<int> air;
    for (auto o : L.open) air.add(o.toNearestInt());
    for (auto l : L.ledges) air.add(l.toNearestInt());
    for (auto gap : L.gutters) air.add(gap.toNearestInt());
    for (auto at : spotsIn(air, 6, 1415)) move(a, PlaceOverlays::MoveFall, at, butterfly, between(r, 6.0f, 9.0f), r.nextFloat(), 1.0f, between(r, 6.0f, 14.0f), 8.0f, 2);
    return a;
}
//==============================================================================
// Mall: a polished floor along the bottom with reflections, pastel neon tubes on the
// panel corners, storefronts with glowing signs in the open space, potted palms in the
// gaps; the floor glints, the signs flicker, a fountain sprays, light sweeps the floor.
OverlayArt mall(const Layout& L) {
    OverlayArt a;
    a.light = true;
    juce::Random r(1515);
    const juce::Colour pink(0xffff7ad0), teal(0xff58f0e0), lilac(0xffc8a0ff);
    juce::Path floor, floorShine, pinkTubes, tealTubes, fronts, glass, awnings, signTube, signText, pots, fronds, trunks;
    std::vector<Point> glints, signs, sprays;
    const float bottom = L.bounds.getBottom(), width = L.bounds.getWidth();
    // The polished floor: tiles with a glossy streak.
    floor.addRectangle(0.0f, bottom - 8.0f, width, 8.0f);
    for (float x = 0.0f; x < width; x += 26.0f) floorShine.addRectangle(x, bottom - 8.0f, 0.8f, 8.0f);
    floorShine.addRectangle(0.0f, bottom - 6.0f, width, 0.8f);
    for (float x = between(r, 10.0f, 40.0f); x < width; x += between(r, 50.0f, 90.0f)) glints.push_back({ x, bottom - between(r, 2.0f, 6.0f) });
    // Neon tubes bent round the panels' corners, pink and teal by turns.
    int k = 0;
    for (auto p : L.panels) {
        auto& tube = (k++ % 2 == 0) ? pinkTubes : tealTubes;
        const float arm = std::min(60.0f, p.getWidth() * 0.25f);
        tube.startNewSubPath(p.getX() + arm, p.getY() - 0.5f);
        tube.lineTo(p.getX() + 10.0f, p.getY() - 0.5f);
        tube.quadraticTo(p.getX() - 0.5f, p.getY() - 0.5f, p.getX() - 0.5f, p.getY() + 10.0f);
        tube.lineTo(p.getX() - 0.5f, p.getY() + std::min(40.0f, p.getHeight() * 0.25f));
        tube.startNewSubPath(p.getRight() - arm, p.getBottom() + 0.5f);
        tube.lineTo(p.getRight() - 10.0f, p.getBottom() + 0.5f);
        tube.quadraticTo(p.getRight() + 0.5f, p.getBottom() + 0.5f, p.getRight() + 0.5f, p.getBottom() - 10.0f);
        tube.lineTo(p.getRight() + 0.5f, p.getBottom() - std::min(40.0f, p.getHeight() * 0.25f));
    }
    // Potted palms in the gaps.
    for (auto gap : L.gutters) {
        const float cx = gap.getCentreX(), base = gap.getBottom() - 4.0f;
        if (gap.getHeight() < 60.0f) continue;
        pots.addRoundedRectangle(cx - 5.0f, base - 9.0f, 10.0f, 9.0f, 2.0f);
        const float top = base - std::min(70.0f, gap.getHeight() * 0.45f);
        juce::Path trunk;
        trunk.startNewSubPath(cx, base - 9.0f);
        trunk.quadraticTo(cx + 3.0f, (base + top) * 0.5f, cx, top);
        juce::PathStrokeType(2.2f).createStrokedPath(trunk, trunk);
        trunks.addPath(trunk);
        for (int f = 0; f < 6; ++f) {
            const float angle = -2.6f + float(f) * 0.52f + between(r, -0.1f, 0.1f);
            juce::Path frond;
            const Point tip { cx + std::cos(angle) * 14.0f, top + std::sin(angle) * 9.0f + 6.0f };
            frond.startNewSubPath(cx, top);
            frond.quadraticTo((cx + tip.x) * 0.5f, top + std::sin(angle) * 9.0f - 3.0f, tip.x, tip.y);
            juce::PathStrokeType(1.8f).createStrokedPath(frond, frond);
            fronds.addPath(frond);
        }
        sprays.push_back({ cx, gap.getY() + 30.0f });
    }
    // Storefronts in the open space: glass fronts under striped awnings, a glowing sign over each.
    static constexpr std::array<const char*, 5> names { "SALE", "MALL", "FOOD", "OPEN", "SHOP" };
    int name = 0;
    for (auto o : L.open) {
        const float base = o.getBottom();
        float x = o.getX() + 8.0f;
        while (x < o.getRight() - 60.0f) {
            const float w = between(r, 52.0f, 70.0f), h = std::min(o.getHeight() - 2.0f, between(r, 18.0f, 24.0f));
            fronts.addRectangle(x, base - h, w, h);
            glass.addRectangle(x + 4.0f, base - h * 0.55f, w - 8.0f, h * 0.55f - 1.0f);
            for (float ax = x; ax < x + w - 1.0f; ax += 8.0f) awnings.addRectangle(ax, base - h * 0.55f - 4.0f, 4.0f, 4.0f);
            const Rect sign { x + 6.0f, base - h + 2.0f, w - 12.0f, h * 0.45f - 4.0f };
            if (sign.getHeight() > 6.0f) {
                signTube.addRoundedRectangle(sign, 3.0f);
                juce::GlyphArrangement glyphs;
                glyphs.addLineOfText(juce::Font(juce::FontOptions(std::min(9.0f, sign.getHeight() - 1.0f), juce::Font::bold)), names[std::size_t(name++ % 5)], 0.0f, 0.0f);
                juce::Path text;
                glyphs.createPath(text);
                const auto tb = text.getBounds();
                text.applyTransform(juce::AffineTransform::translation(sign.getCentreX() - tb.getCentreX(), sign.getCentreY() - tb.getCentreY()));
                signText.addPath(text);
                signs.push_back(sign.getCentre());
            }
            x += w + between(r, 10.0f, 24.0f);
        }
    }
    a.shapes.push_back({ floor, juce::Colour(0xffe8dcef) });
    a.shapes.push_back({ floorShine, juce::Colour(0xffffffff) });
    a.shapes.push_back({ fronts, juce::Colour(0xff2a2440) });
    a.shapes.push_back({ glass, juce::Colour(0xff4a6a88) });
    a.shapes.push_back({ awnings, pink.withAlpha(0.85f) });
    a.shapes.push_back({ pots, juce::Colour(0xfff0e8f8) });
    a.shapes.push_back({ trunks, juce::Colour(0xff8a6a50) });
    a.shapes.push_back({ fronds, juce::Colour(0xff3aa878) });
    neon(a, pinkTubes, pink);
    neon(a, tealTubes, teal);
    neon(a, signTube, lilac, 1.0f);
    a.shapes.push_back({ signText, teal.interpolatedWith(juce::Colours::white, 0.4f) });

    const int glint = int(a.sprites.size());
    a.sprites.push_back(sparkleSprite(4.0f, juce::Colour(0xffffe0ff)));
    const int glow = int(a.sprites.size());
    a.sprites.push_back(glowSprite(7.0f, lilac.withAlpha(0.45f), juce::Colour(0x00ffffff), 0.0f));
    const int drop = int(a.sprites.size());
    a.sprites.push_back(glowSprite(2.0f, juce::Colour(0x8080e8ff), juce::Colour(0xffe8ffff), 0.8f));
    const int sweep = int(a.sprites.size());
    a.sprites.push_back(glowSprite(6.0f, juce::Colour(0x50ffffff), juce::Colour(0x90ffffff), 1.5f));
    for (auto g : glints) move(a, PlaceOverlays::MovePulse, g, glint, between(r, 2.5f, 5.0f), r.nextFloat(), 0.9f);
    for (std::size_t i = 0; i < signs.size() && i < 8; ++i) move(a, PlaceOverlays::MoveFlicker, signs[i], glow, between(r, 1.0f, 3.0f), r.nextFloat(), 0.8f);
    for (auto at : sprays)
        for (int d = 0; d < 4; ++d) move(a, PlaceOverlays::MoveRise, at.translated(between(r, -4.0f, 4.0f), 0.0f), drop, between(r, 1.4f, 2.2f), float(d) / 4.0f, 1.0f, between(r, 10.0f, 16.0f), 4.0f);
    for (int c = 0; c < 2; ++c) move(a, PlaceOverlays::MoveDrift, { -10.0f, bottom - 4.0f }, sweep, between(r, 9.0f, 12.0f), float(c) / 2.0f, 0.8f, width + 20.0f, 0.0f);
    return a;
}
//==============================================================================
// Military Camp: sandbags along the bottom, barbed wire along the panel tops, camouflage
// netting over the corners, a row of tents with a flag pole and a radio mast in the open
// space, ammunition crates in the gaps; campfire embers rise, the flag flutters, lamps
// and the mast blink, a searchlight sweeps the sandbags.
OverlayArt military(const Layout& L) {
    OverlayArt a;
    juce::Random r(1616);
    juce::Path bags, bagShade, wire, barbs, netDark, netLight, tents, tentShade, poles, crates, crateEdge;
    std::vector<Point> fires, lamps, masts, flags;
    const float bottom = L.bounds.getBottom(), width = L.bounds.getWidth();
    // Sandbags in two staggered rows along the bottom.
    for (int row = 0; row < 2; ++row)
        for (float x = row == 0 ? 0.0f : 7.0f; x < width; x += 14.0f) {
            const float y = bottom - 6.0f - float(row) * 5.0f;
            bags.addRoundedRectangle(x, y, 13.0f, 6.0f, 2.8f);
            bagShade.addRectangle(x + 2.0f, y + 4.2f, 9.0f, 1.0f);
        }
    for (float x = between(r, 80.0f, 160.0f); x < width; x += between(r, 220.0f, 340.0f)) fires.push_back({ x, bottom - 12.0f });
    // Barbed wire: a twisted strand along each panel's top, barbs every few pixels;
    // camouflage netting over the corners.
    for (auto p : L.panels) {
        juce::Path strand;
        strand.startNewSubPath(p.getX() + 14.0f, p.getY() - 1.0f);
        for (float x = p.getX() + 14.0f; x < p.getRight() - 14.0f; x += 6.0f) strand.lineTo(x + 3.0f, p.getY() - 1.0f + ((int(x) / 6) % 2 == 0 ? 1.2f : -1.2f));
        juce::PathStrokeType(0.9f).createStrokedPath(strand, strand);
        wire.addPath(strand);
        for (float x = p.getX() + 18.0f; x < p.getRight() - 16.0f; x += 11.0f) {
            juce::Path barb;
            barb.startNewSubPath(x - 2.0f, p.getY() - 3.0f);
            barb.lineTo(x + 2.0f, p.getY() + 1.0f);
            barb.startNewSubPath(x + 2.0f, p.getY() - 3.0f);
            barb.lineTo(x - 2.0f, p.getY() + 1.0f);
            juce::PathStrokeType(0.8f).createStrokedPath(barb, barb);
            barbs.addPath(barb);
        }
        for (auto corner : { Point { p.getX(), p.getY() }, Point { p.getRight(), p.getBottom() } })
            for (int k = 0; k < 16; ++k) {
                const float d = between(r, 4.0f, 8.0f), ang = between(r, 0.0f, twoPi), rad = between(r, 0.0f, 12.0f);
                (r.nextFloat() < 0.4f ? netLight : netDark).addEllipse(corner.x + std::cos(ang) * rad - d * 0.5f, corner.y + std::sin(ang) * rad - d * 0.5f, d, d * 0.7f);
            }
    }
    // Ammunition crates stacked in the gaps, a lamp above them.
    for (auto gap : L.gutters) {
        const float cx = gap.getCentreX(), w = std::min(12.0f, gap.getWidth() - 2.0f), base = gap.getBottom() - 14.0f;
        if (gap.getHeight() < 50.0f) continue;
        for (int k = 0; k < 2; ++k) {
            const Rect crate { cx - w * 0.5f, base - float(k + 1) * 8.0f, w, 7.5f };
            crates.addRectangle(crate);
            crateEdge.addRectangle(crate.getX(), crate.getCentreY() - 0.4f, crate.getWidth(), 0.8f);
        }
        lamps.push_back({ cx, gap.getY() + 10.0f });
    }
    // Tents in the open space, a flag pole and a radio mast.
    for (auto o : L.open) {
        const float base = o.getBottom();
        float x = o.getX() + 10.0f;
        const float tall = std::min(o.getHeight() - 4.0f, 20.0f);
        poles.addRectangle(x, base - tall - 6.0f, 1.4f, tall + 6.0f);
        flags.push_back({ x + 7.0f, base - tall - 3.0f });
        x += 22.0f;
        while (x < o.getRight() - 50.0f) {
            const float w = between(r, 28.0f, 38.0f), h = std::min(o.getHeight() - 4.0f, between(r, 12.0f, 16.0f));
            tents.addTriangle(x, base, x + w, base, x + w * 0.5f, base - h);
            tentShade.addTriangle(x + w * 0.5f, base, x + w, base, x + w * 0.5f, base - h);
            tentShade.addTriangle(x + w * 0.42f, base, x + w * 0.58f, base, x + w * 0.5f, base - h * 0.45f);
            x += w + between(r, 6.0f, 16.0f);
        }
        const float mx = o.getRight() - 24.0f, mastTop = base - std::min(o.getHeight() - 2.0f, 30.0f);
        juce::Path mast;
        mast.startNewSubPath(mx - 6.0f, base);
        mast.lineTo(mx, mastTop);
        mast.lineTo(mx + 6.0f, base);
        for (float y = base - 6.0f; y > mastTop + 4.0f; y -= 6.0f) {
            const float half = 6.0f * (y - mastTop) / (base - mastTop);
            mast.startNewSubPath(mx - half, y);
            mast.lineTo(mx + half, y - 3.0f);
        }
        juce::PathStrokeType(1.0f).createStrokedPath(mast, mast);
        poles.addPath(mast);
        masts.push_back({ mx, mastTop });
    }
    a.shapes.push_back({ tents, juce::Colour(0xff5e6a3e) });
    a.shapes.push_back({ tentShade, juce::Colour(0xff3c4628) });
    a.shapes.push_back({ poles, juce::Colour(0xff3a3a36) });
    a.shapes.push_back({ crates, juce::Colour(0xff4e5a34) });
    a.shapes.push_back({ crateEdge, juce::Colour(0xffc8b060) });
    a.shapes.push_back({ netDark, juce::Colour(0xd0384628) });
    a.shapes.push_back({ netLight, juce::Colour(0xd0788a4a) });
    a.shapes.push_back({ wire, juce::Colour(0xff9a9a96) });
    a.shapes.push_back({ barbs, juce::Colour(0xffb8b8b0) });
    a.shapes.push_back({ bags, juce::Colour(0xffb09a6c) });
    a.shapes.push_back({ bagShade, juce::Colour(0xff7a6a48) });

    const int ember = int(a.sprites.size());
    a.sprites.push_back(glowSprite(3.5f, juce::Colour(0xb0ff6a2c), juce::Colour(0xffffd090), 0.9f));
    const int smoke = puffSprites(a, juce::Colour(0x60b09088));
    const int red = int(a.sprites.size());
    a.sprites.push_back(glowSprite(4.5f, juce::Colour(0xb0ff3030), juce::Colour(0xffffa0a0), 1.2f));
    const int lamp = int(a.sprites.size());
    a.sprites.push_back(glowSprite(6.0f, juce::Colour(0x90ffc060), juce::Colour(0xffffe8b0), 1.6f));
    const int beam = int(a.sprites.size());
    a.sprites.push_back(glowSprite(12.0f, juce::Colour(0x38fff8d8), juce::Colour(0x60ffffff), 4.0f));
    const int flag = int(a.sprites.size());
    for (int f = 0; f < 4; ++f) {
        juce::Path cloth;
        const float wave = std::sin(float(f) * twoPi / 4.0f) * 1.5f;
        cloth.startNewSubPath(-6.0f, -3.5f);
        cloth.quadraticTo(-1.0f, -3.5f + wave, 5.0f, -3.0f);
        cloth.lineTo(5.0f, 2.5f);
        cloth.quadraticTo(-1.0f, 3.0f + wave, -6.0f, 3.0f);
        cloth.closeSubPath();
        a.sprites.push_back(pathSprite(cloth, juce::Colour(0xffb03a30)));
    }
    for (auto f : fires) {
        for (int k = 0; k < 3; ++k) move(a, PlaceOverlays::MoveRise, f.translated(between(r, -4.0f, 4.0f), 4.0f), ember, between(r, 1.4f, 2.2f), float(k) / 3.0f, 1.0f, 14.0f, 3.0f);
        move(a, PlaceOverlays::MoveRise, f, smoke, 3.2f, r.nextFloat(), 0.7f, 18.0f, 3.0f, 8);
    }
    for (auto l : lamps) move(a, PlaceOverlays::MovePulse, l, lamp, between(r, 3.0f, 5.0f), r.nextFloat(), 0.9f);
    for (auto m : masts) move(a, PlaceOverlays::MoveBlink, m, red, 1.8f, 0.0f);
    for (auto f : flags) move(a, PlaceOverlays::MoveSpin, f, flag, 0.8f, 0.0f, 1.0f, 0, 0, 4);
    move(a, PlaceOverlays::MoveDrift, { -12.0f, bottom - 9.0f }, beam, 11.0f, 0.0f, 0.8f, width + 24.0f, 0.0f);
    move(a, PlaceOverlays::MoveDrift, { width + 12.0f, bottom - 9.0f }, beam, 13.0f, 0.5f, 0.6f, -(width + 24.0f), 0.0f);
    return a;
}
//==============================================================================
// Beach: sand along the bottom with shells and starfish, turquoise surf with foam above
// it, palm fronds hanging over the panels' top corners, palm trunks in the gaps, a low
// sun over the sea with a striped umbrella in the open space; the water glints, foam
// bubbles, the sun glows, gulls drift.
OverlayArt beach(const Layout& L) {
    OverlayArt a;
    juce::Random r(1717);
    juce::Path sand, sandDots, sea, foam, shells, stars, fronds, frondDark, trunks, trunkRings, umbrella, umbrellaStripes, sun;
    std::vector<Point> glints, bubbles, suns;
    const float bottom = L.bounds.getBottom(), width = L.bounds.getWidth();
    // The surf, then the sand in front of it.
    sea.addRectangle(0.0f, bottom - 10.0f, width, 4.0f);
    for (float x = 0.0f; x < width; x += 12.0f) {
        foam.addEllipse(x, bottom - 8.0f - between(r, 0.0f, 1.5f), between(r, 7.0f, 11.0f), 2.2f);
        if (r.nextFloat() < 0.3f) bubbles.push_back({ x + 5.0f, bottom - 7.5f });
    }
    juce::Path dune;
    dune.startNewSubPath(0.0f, bottom);
    dune.lineTo(0.0f, bottom - 6.0f);
    for (float x = 0.0f; x <= width; x += 40.0f) dune.quadraticTo(x + 20.0f, bottom - between(r, 5.0f, 8.0f), x + 40.0f, bottom - 6.0f);
    dune.lineTo(width, bottom);
    dune.closeSubPath();
    sand.addPath(dune);
    for (int k = 0; k < int(width / 6.0f); ++k) sandDots.addEllipse(between(r, 0.0f, width), bottom - between(r, 0.5f, 5.0f), 0.9f, 0.9f);
    for (float x = between(r, 30.0f, 90.0f); x < width; x += between(r, 110.0f, 200.0f)) {
        if (r.nextBool()) {
            shells.addPieSegment(x - 3.0f, bottom - 6.0f, 6.0f, 5.0f, -1.5708f, 1.5708f, 0.0f);
        } else {
            stars.addStar({ x, bottom - 3.5f }, 5, 1.2f, 3.4f, r.nextFloat());
        }
        glints.push_back({ x + between(r, 20.0f, 50.0f), bottom - 8.5f });
    }
    // Palm fronds over the panels' top corners.
    const auto frond = [&](Point base, float angle, float length) {
        juce::Path stem;
        const Point tip = base + Point { std::cos(angle), std::sin(angle) } * length;
        const Point mid = base + Point { std::cos(angle), std::sin(angle) } * (length * 0.5f) + Point { 0.0f, length * 0.15f };
        stem.startNewSubPath(base);
        stem.quadraticTo(mid, tip);
        juce::PathStrokeType(1.2f).createStrokedPath(stem, stem);
        frondDark.addPath(stem);
        for (float f = 0.15f; f < 0.95f; f += 0.12f) {
            const Point at = base + (mid - base) * (2.0f * f * (1.0f - f)) * 2.0f * 0.5f + (tip - base) * (f * f) + (mid - base) * (f * (1.0f - f));
            for (float side : { -1.0f, 1.0f }) leaf(fronds, at, 6.0f * (1.1f - f), angle + side * 1.1f + 0.4f);
        }
    };
    for (auto p : L.panels) {
        frond({ p.getX() - 2.0f, p.getY() - 2.0f }, 0.35f, 34.0f);
        frond({ p.getX() - 2.0f, p.getY() - 2.0f }, 1.25f, 26.0f);
        frond({ p.getRight() + 2.0f, p.getY() - 2.0f }, twoPi * 0.5f - 0.35f, 34.0f);
        frond({ p.getRight() + 2.0f, p.getY() - 2.0f }, twoPi * 0.5f - 1.25f, 26.0f);
    }
    // Palm trunks up the gaps.
    for (auto gap : L.gutters) {
        if (gap.getHeight() < 60.0f) continue;
        const float cx = gap.getCentreX();
        for (float y = gap.getBottom() - 10.0f; y > gap.getY() + gap.getHeight() * 0.35f; y -= 6.0f) {
            const float sway = 2.0f * std::sin((gap.getBottom() - y) * 0.02f);
            trunks.addEllipse(cx + sway - 3.0f, y - 3.0f, 6.0f, 6.5f);
            trunkRings.addRectangle(cx + sway - 2.5f, y + 1.5f, 5.0f, 0.8f);
        }
    }
    // A low sun over the sea, and a striped umbrella, in the open space.
    for (auto o : L.open) {
        const float radius = std::min(o.getHeight() * 0.45f, 14.0f);
        const Point centre { o.getX() + o.getWidth() * 0.3f, o.getBottom() - radius * 0.4f };
        sun.addPieSegment(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, -1.5708f, 1.5708f, 0.0f);
        suns.push_back(centre.translated(0.0f, -radius * 0.4f));
        for (float x = o.getX(); x < o.getRight(); x += 9.0f) {
            sea.addRectangle(x, o.getBottom() - 2.0f, 6.0f, 1.0f);
            if (r.nextFloat() < 0.2f) glints.push_back({ x + 3.0f, o.getBottom() - 1.5f });
        }
        const float ux = o.getRight() - 40.0f, top = o.getBottom() - std::min(o.getHeight() - 2.0f, 22.0f);
        umbrella.addRectangle(ux - 0.6f, top + 4.0f, 1.2f, o.getBottom() - top - 4.0f);
        juce::Path canopy;
        canopy.addPieSegment(ux - 16.0f, top, 32.0f, 16.0f, -1.5708f, 1.5708f, 0.0f);
        umbrella.addPath(canopy);
        for (int k = 0; k < 4; k += 2) {
            juce::Path stripe;
            const float a0 = -1.5708f + float(k) * 0.785f;
            stripe.addPieSegment(ux - 16.0f, top, 32.0f, 16.0f, a0, a0 + 0.785f, 0.0f);
            umbrellaStripes.addPath(stripe);
        }
    }
    a.shapes.push_back({ sun, juce::Colour(0xffffb050) });
    a.shapes.push_back({ sea, juce::Colour(0xff28b8c0) });
    a.shapes.push_back({ foam, juce::Colour(0xfff0fbff) });
    a.shapes.push_back({ sand, juce::Colour(0xffe8cc8a) });
    a.shapes.push_back({ sandDots, juce::Colour(0xffb89a60) });
    a.shapes.push_back({ shells, juce::Colour(0xfff8e0d0) });
    a.shapes.push_back({ stars, juce::Colour(0xffe87a50) });
    a.shapes.push_back({ trunks, juce::Colour(0xff8a6440) });
    a.shapes.push_back({ trunkRings, juce::Colour(0xff5a4028) });
    a.shapes.push_back({ umbrella, juce::Colour(0xfff4f0e8) });
    a.shapes.push_back({ umbrellaStripes, juce::Colour(0xffe8505a) });
    a.shapes.push_back({ frondDark, juce::Colour(0xff2e6a30) });
    a.shapes.push_back({ fronds, juce::Colour(0xff48a040) });

    const int glint = int(a.sprites.size());
    a.sprites.push_back(sparkleSprite(4.0f, juce::Colour(0xffd0fbff)));
    const int bubble = int(a.sprites.size());
    a.sprites.push_back(glowSprite(2.5f, juce::Colour(0x80ffffff), juce::Colour(0xffffffff), 0.8f));
    const int glow = int(a.sprites.size());
    a.sprites.push_back(glowSprite(12.0f, juce::Colour(0x50ffc060), juce::Colour(0x00ffffff), 0.0f));
    const int gull = int(a.sprites.size());
    {
        juce::Path g;
        g.startNewSubPath(-5.0f, 0.0f);
        g.quadraticTo(-2.5f, -2.5f, 0.0f, 0.0f);
        g.quadraticTo(2.5f, -2.5f, 5.0f, 0.0f);
        juce::PathStrokeType(1.3f).createStrokedPath(g, g);
        a.sprites.push_back(pathSprite(g, juce::Colour(0xfff8f8f0)));
    }
    for (auto g : glints) move(a, PlaceOverlays::MovePulse, g, glint, between(r, 1.5f, 3.0f), r.nextFloat());
    for (std::size_t i = 0; i < bubbles.size() && i < 16; ++i) move(a, PlaceOverlays::MovePulse, bubbles[i], bubble, between(r, 2.0f, 3.5f), r.nextFloat(), 0.8f);
    for (auto s : suns) move(a, PlaceOverlays::MovePulse, s, glow, 6.0f, 0.0f, 0.8f);
    juce::RectangleList<int> sky;
    for (auto o : L.open) sky.add(o.toNearestInt());
    for (auto l : L.ledges) sky.add(l.toNearestInt());
    for (auto at : spotsIn(sky, 6, 1718)) move(a, PlaceOverlays::MoveDrift, at, gull, between(r, 8.0f, 12.0f), r.nextFloat(), 0.9f, between(r, -40.0f, 40.0f), 4.0f);
    return a;
}
//==============================================================================
// Desert: dunes along the bottom with sand blown over the frames' lower edges, carved
// sandstone trim with a zigzag band along the panel tops, brass lanterns hanging in the
// gaps, a caravan of camels by an oasis under a pale sun in the open space; sand drifts
// across the bottom, the lanterns flicker, heat glints, the sun glows.
OverlayArt desert(const Layout& L) {
    OverlayArt a;
    juce::Random r(1818);
    juce::Path dunesBack, dunesFront, ripples, trim, zigzag, chains, lanterns, lanternGlass, camels, palms, sun, drifts;
    std::vector<Point> lamps, glints, suns;
    const float bottom = L.bounds.getBottom(), width = L.bounds.getWidth();
    // Two rows of dunes with ripples.
    for (auto* row : { &dunesBack, &dunesFront }) {
        const bool front = row == &dunesFront;
        juce::Path d;
        d.startNewSubPath(0.0f, bottom);
        d.lineTo(0.0f, bottom - (front ? 5.0f : 9.0f));
        for (float x = 0.0f; x < width; x += front ? 70.0f : 110.0f) {
            const float peak = front ? between(r, 7.0f, 10.0f) : between(r, 11.0f, 15.0f);
            d.quadraticTo(x + (front ? 35.0f : 55.0f), bottom - peak, x + (front ? 70.0f : 110.0f), bottom - (front ? 5.0f : 9.0f));
        }
        d.lineTo(width, bottom);
        d.closeSubPath();
        row->addPath(d);
    }
    for (float x = 6.0f; x < width; x += between(r, 10.0f, 18.0f)) {
        juce::Path rp;
        rp.startNewSubPath(x, bottom - 2.5f);
        rp.quadraticTo(x + 3.0f, bottom - 3.6f, x + 6.0f, bottom - 2.5f);
        juce::PathStrokeType(0.7f).createStrokedPath(rp, rp);
        ripples.addPath(rp);
        if (r.nextFloat() < 0.15f) glints.push_back({ x, bottom - between(r, 3.0f, 8.0f) });
    }
    // Carved sandstone along the panel tops, a zigzag band on it; sand heaped at the bottom corners.
    for (auto p : L.panels) {
        trim.addRectangle(p.getX() + 10.0f, p.getY() - 2.5f, p.getWidth() - 20.0f, 5.0f);
        for (float x = p.getX() + 12.0f; x < p.getRight() - 16.0f; x += 6.0f) zigzag.addTriangle(x, p.getY() + 1.5f, x + 3.0f, p.getY() - 1.5f, x + 6.0f, p.getY() + 1.5f);
        for (auto [cx, dx] : { std::pair<float, float> { p.getX(), 1.0f }, { p.getRight(), -1.0f } }) {
            juce::Path heap;
            heap.startNewSubPath(cx - dx * 4.0f, p.getBottom() + 2.0f);
            heap.quadraticTo(cx + dx * 6.0f, p.getBottom() - 12.0f, cx + dx * 26.0f, p.getBottom() + 2.0f);
            heap.closeSubPath();
            drifts.addPath(heap);
        }
    }
    // Brass lanterns on chains in the gaps.
    for (auto gap : L.gutters) {
        if (gap.getHeight() < 60.0f) continue;
        const float cx = gap.getCentreX(), y = gap.getY() + std::min(60.0f, gap.getHeight() * 0.3f);
        chains.addRectangle(cx - 0.4f, gap.getY() + 4.0f, 0.8f, y - gap.getY() - 4.0f);
        juce::Path lantern;
        lantern.addTriangle(cx - 5.0f, y + 2.0f, cx + 5.0f, y + 2.0f, cx, y - 3.0f);
        lantern.addRectangle(cx - 4.0f, y + 2.0f, 8.0f, 8.0f);
        lantern.addTriangle(cx - 4.0f, y + 10.0f, cx + 4.0f, y + 10.0f, cx, y + 14.0f);
        lanterns.addPath(lantern);
        lanternGlass.addRectangle(cx - 2.5f, y + 3.5f, 5.0f, 5.0f);
        lamps.push_back({ cx, y + 6.0f });
    }
    // A caravan by an oasis under a pale sun, in the open space.
    for (auto o : L.open) {
        const float base = o.getBottom() - 1.0f, scale = std::min(1.0f, o.getHeight() / 26.0f);
        const float radius = std::min(o.getHeight() * 0.35f, 11.0f);
        const Point sunAt { o.getX() + o.getWidth() * 0.18f, o.getY() + radius + 1.0f };
        sun.addEllipse(sunAt.x - radius, sunAt.y - radius, radius * 2.0f, radius * 2.0f);
        suns.push_back(sunAt);
        // Distant dunes in the header.
        juce::Path far;
        far.startNewSubPath(o.getX(), base);
        for (float x = o.getX(); x < o.getRight(); x += 60.0f) far.quadraticTo(x + 30.0f, base - 7.0f * scale, x + 60.0f, base);
        far.closeSubPath();
        dunesBack.addPath(far);
        // Camels walking in a line.
        for (int k = 0; k < 3; ++k) {
            const float x = o.getX() + o.getWidth() * 0.45f + float(k) * 22.0f * scale, y = base - 3.0f * scale;
            juce::Path camel;
            camel.addEllipse(x - 7.0f * scale, y - 11.0f * scale, 12.0f * scale, 6.0f * scale);       // body
            camel.addEllipse(x - 3.5f * scale, y - 14.0f * scale, 5.0f * scale, 5.0f * scale);       // hump
            camel.addRectangle(x + 3.5f * scale, y - 16.0f * scale, 1.6f * scale, 8.0f * scale);     // neck
            camel.addEllipse(x + 3.5f * scale, y - 17.5f * scale, 4.0f * scale, 2.6f * scale);       // head
            for (float leg : { -5.5f, -3.0f, 1.5f, 3.5f }) camel.addRectangle(x + leg * scale, y - 6.0f * scale, 1.1f * scale, 6.0f * scale);
            camels.addPath(camel);
        }
        // An oasis palm at the right.
        const float px = o.getRight() - 30.0f, top = base - std::min(o.getHeight() - 3.0f, 22.0f);
        juce::Path trunk;
        trunk.startNewSubPath(px, base);
        trunk.quadraticTo(px + 4.0f, (base + top) * 0.5f, px + 1.0f, top);
        juce::PathStrokeType(1.6f).createStrokedPath(trunk, trunk);
        palms.addPath(trunk);
        for (int f = 0; f < 5; ++f) leaf(palms, { px + 1.0f, top }, 7.0f, -2.8f + float(f) * 0.6f);
    }
    a.shapes.push_back({ sun, juce::Colour(0xfffff0c8) });
    a.shapes.push_back({ dunesBack, juce::Colour(0xffc89a58) });
    a.shapes.push_back({ camels, juce::Colour(0xff5a3e26) });
    a.shapes.push_back({ palms, juce::Colour(0xff3a6a34) });
    a.shapes.push_back({ dunesFront, juce::Colour(0xffe8c07a) });
    a.shapes.push_back({ ripples, juce::Colour(0xffb08850) });
    a.shapes.push_back({ drifts, juce::Colour(0xe0e2b878) });
    a.shapes.push_back({ trim, juce::Colour(0xffc8a46a) });
    a.shapes.push_back({ zigzag, juce::Colour(0xff8a5a30) });
    a.shapes.push_back({ chains, juce::Colour(0xff8a7040) });
    a.shapes.push_back({ lanterns, juce::Colour(0xffb08838) });
    a.shapes.push_back({ lanternGlass, juce::Colour(0xffffc860) });

    const int grain = int(a.sprites.size());
    a.sprites.push_back(glowSprite(1.8f, juce::Colour(0x90f0d090), juce::Colour(0xfff8e8c0), 0.7f));
    const int lamp = int(a.sprites.size());
    a.sprites.push_back(glowSprite(8.0f, juce::Colour(0x90ffb040), juce::Colour(0xffffe0a0), 2.0f));
    const int glint = int(a.sprites.size());
    a.sprites.push_back(sparkleSprite(3.5f, juce::Colour(0xfffff0c0)));
    const int halo = int(a.sprites.size());
    a.sprites.push_back(glowSprite(16.0f, juce::Colour(0x40fff0c0), juce::Colour(0x00ffffff), 0.0f));
    for (int k = 0; k < 14; ++k)
        move(a, PlaceOverlays::MoveDrift, { between(r, -20.0f, width * 0.6f), bottom - between(r, 3.0f, 11.0f) }, grain, between(r, 4.0f, 7.0f), r.nextFloat(), 0.9f,
             between(r, 120.0f, 260.0f), 2.0f);
    for (auto l : lamps) move(a, PlaceOverlays::MoveFlicker, l, lamp, between(r, 1.0f, 2.0f), r.nextFloat(), 0.9f);
    for (std::size_t i = 0; i < glints.size() && i < 10; ++i) move(a, PlaceOverlays::MovePulse, glints[i], glint, between(r, 2.0f, 4.0f), r.nextFloat(), 0.8f);
    for (auto s : suns) move(a, PlaceOverlays::MovePulse, s, halo, 7.0f, 0.0f, 0.8f);
    return a;
}
//==============================================================================
// Palace: gilded moulding along the frames with scrolls at the corners, red velvet drapes
// tied back at the panels' top corners, marble columns in the gaps, a chequered marble
// floor along the bottom, crystal chandeliers with candles in the open space; candles
// flicker, crystals glint, gold motes drift down.
OverlayArt palace(const Layout& L) {
    OverlayArt a;
    juce::Random r(1919);
    juce::Path gold, goldDark, scrolls, drape, drapeShade, tassels, columns, columnLines, floorLight, floorDark, chandelier, candles;
    std::vector<Point> flames, crystals;
    const float bottom = L.bounds.getBottom(), width = L.bounds.getWidth();
    // The marble floor: a chequer of light and dark tiles.
    for (float x = 0.0f; x < width; x += 10.0f)
        for (int row = 0; row < 2; ++row) ((int(x / 10.0f) + row) % 2 == 0 ? floorLight : floorDark).addRectangle(x, bottom - 8.0f + float(row) * 4.0f, 10.0f, 4.0f);
    // Gilded moulding on every frame, with a scroll at each corner.
    for (auto p : L.panels) {
        gold.addRectangle(p.getX() + 8.0f, p.getY() - 2.0f, p.getWidth() - 16.0f, 3.5f);
        gold.addRectangle(p.getX() + 8.0f, p.getBottom() - 1.5f, p.getWidth() - 16.0f, 3.5f);
        goldDark.addRectangle(p.getX() + 8.0f, p.getY() + 0.6f, p.getWidth() - 16.0f, 0.8f);
        for (float x = p.getX() + 16.0f; x < p.getRight() - 16.0f; x += 14.0f) goldDark.addEllipse(x - 1.0f, p.getY() - 1.3f, 2.0f, 2.0f);
        for (auto corner : { Point { p.getX(), p.getY() }, Point { p.getRight(), p.getY() }, Point { p.getX(), p.getBottom() }, Point { p.getRight(), p.getBottom() } }) {
            const float dx = corner.x < p.getCentreX() ? 1.0f : -1.0f, dy = corner.y < p.getCentreY() ? 1.0f : -1.0f;
            juce::Path curl;
            curl.startNewSubPath(corner.x + dx * 12.0f, corner.y);
            curl.cubicTo(corner.x + dx * 2.0f, corner.y - dy * 3.0f, corner.x - dx * 3.0f, corner.y + dy * 4.0f, corner.x + dx * 2.0f, corner.y + dy * 8.0f);
            curl.cubicTo(corner.x + dx * 6.0f, corner.y + dy * 11.0f, corner.x + dx * 8.0f, corner.y + dy * 5.0f, corner.x + dx * 4.0f, corner.y + dy * 4.5f);
            juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(curl, curl);
            scrolls.addPath(curl);
        }
        // Velvet drapes swagged from the top corners.
        for (auto [cx, dx] : { std::pair<float, float> { p.getX(), 1.0f }, { p.getRight(), -1.0f } }) {
            juce::Path d;
            const float reach = std::min(42.0f, p.getWidth() * 0.18f);
            d.startNewSubPath(cx - dx * 2.0f, p.getY() - 2.0f);
            d.lineTo(cx + dx * reach, p.getY() - 2.0f);
            d.quadraticTo(cx + dx * reach * 0.5f, p.getY() + 9.0f, cx + dx * 4.0f, p.getY() + 22.0f);
            d.lineTo(cx - dx * 2.0f, p.getY() + 30.0f);
            d.closeSubPath();
            drape.addPath(d);
            juce::Path fold;
            fold.startNewSubPath(cx + dx * reach * 0.6f, p.getY() - 1.0f);
            fold.quadraticTo(cx + dx * reach * 0.3f, p.getY() + 6.0f, cx + dx * 2.0f, p.getY() + 20.0f);
            juce::PathStrokeType(1.2f).createStrokedPath(fold, fold);
            drapeShade.addPath(fold);
            tassels.addEllipse(cx - 1.5f, p.getY() + 29.0f, 3.0f, 5.0f);
        }
    }
    // Marble columns with fluting in the gaps.
    for (auto gap : L.gutters) {
        if (gap.getHeight() < 50.0f) continue;
        const float cx = gap.getCentreX(), w = std::min(10.0f, gap.getWidth() - 2.0f);
        columns.addRectangle(cx - w * 0.5f, gap.getY() + 8.0f, w, gap.getHeight() - 16.0f);
        gold.addRectangle(cx - w * 0.5f - 1.5f, gap.getY() + 6.0f, w + 3.0f, 3.0f);
        gold.addRectangle(cx - w * 0.5f - 1.5f, gap.getBottom() - 9.0f, w + 3.0f, 3.0f);
        for (float fx = cx - w * 0.5f + 2.0f; fx < cx + w * 0.5f - 1.0f; fx += 2.5f) columnLines.addRectangle(fx, gap.getY() + 10.0f, 0.6f, gap.getHeight() - 20.0f);
    }
    // Chandeliers in the open space.
    for (auto o : L.open) {
        const int count = std::max(1, int(o.getWidth() / 140.0f));
        for (int k = 0; k < count; ++k) {
            const float cx = o.getX() + o.getWidth() * (float(k) + 0.5f) / float(count), top = o.getY();
            const float h = std::min(o.getHeight() - 2.0f, 26.0f);
            chandelier.addRectangle(cx - 0.5f, top, 1.0f, h * 0.35f);
            juce::Path arms;
            arms.startNewSubPath(cx - 14.0f, top + h * 0.5f);
            arms.quadraticTo(cx, top + h * 0.95f, cx + 14.0f, top + h * 0.5f);
            arms.startNewSubPath(cx - 8.0f, top + h * 0.45f);
            arms.quadraticTo(cx, top + h * 0.75f, cx + 8.0f, top + h * 0.45f);
            juce::PathStrokeType(1.2f).createStrokedPath(arms, arms);
            chandelier.addPath(arms);
            chandelier.addEllipse(cx - 2.5f, top + h * 0.35f, 5.0f, 5.0f);
            for (float dx : { -14.0f, -8.0f, 8.0f, 14.0f }) {
                const float y = top + h * (std::abs(dx) > 10.0f ? 0.5f : 0.45f);
                candles.addRectangle(cx + dx - 0.8f, y - 4.0f, 1.6f, 4.0f);
                flames.push_back({ cx + dx, y - 5.5f });
            }
            for (int d = 0; d < 5; ++d) crystals.push_back({ cx + between(r, -12.0f, 12.0f), top + h * between(r, 0.55f, 0.85f) });
        }
    }
    a.shapes.push_back({ floorLight, juce::Colour(0xfff2ece4) });
    a.shapes.push_back({ floorDark, juce::Colour(0xff3a3036) });
    a.shapes.push_back({ columns, juce::Colour(0xffe8e2da) });
    a.shapes.push_back({ columnLines, juce::Colour(0xffb8b0a8) });
    a.shapes.push_back({ drape, juce::Colour(0xff8a1a2a) });
    a.shapes.push_back({ drapeShade, juce::Colour(0xff5a0e1a) });
    a.shapes.push_back({ gold, juce::Colour(0xffd8b050) });
    a.shapes.push_back({ goldDark, juce::Colour(0xff9a7a30) });
    a.shapes.push_back({ scrolls, juce::Colour(0xffe8c060) });
    a.shapes.push_back({ tassels, juce::Colour(0xffe8c060) });
    a.shapes.push_back({ chandelier, juce::Colour(0xffd8b050) });
    a.shapes.push_back({ candles, juce::Colour(0xfff8f0e0) });

    const int flame = int(a.sprites.size());
    a.sprites.push_back(glowSprite(4.5f, juce::Colour(0xb0ffb040), juce::Colour(0xfffff0c0), 1.2f));
    const int glint = int(a.sprites.size());
    a.sprites.push_back(sparkleSprite(3.5f, juce::Colour(0xffe8f0ff)));
    const int mote = int(a.sprites.size());
    a.sprites.push_back(glowSprite(2.0f, juce::Colour(0x90ffd870), juce::Colour(0xfffff0b0), 0.7f));
    for (auto f : flames) move(a, PlaceOverlays::MoveFlicker, f, flame, between(r, 0.8f, 1.6f), r.nextFloat(), 0.95f);
    for (auto c : crystals) move(a, PlaceOverlays::MovePulse, c, glint, between(r, 1.5f, 3.0f), r.nextFloat(), 0.9f);
    juce::RectangleList<int> air;
    for (auto o : L.open) air.add(o.toNearestInt());
    for (auto l : L.ledges) air.add(l.toNearestInt());
    for (auto gap : L.gutters) air.add(gap.toNearestInt());
    for (auto at : spotsIn(air, 8, 1920)) move(a, PlaceOverlays::MoveFall, at, mote, between(r, 5.0f, 8.0f), r.nextFloat(), 0.9f, between(r, 14.0f, 26.0f), 5.0f);
    return a;
}
//==============================================================================
// Tavern: dark oak beams with iron nails along the frames, barrels stacked in the gaps,
// floorboards along the bottom with a hearth, a shelf of tankards and bottles and a
// hanging inn sign in the open space, candles in iron holders at the panels' top
// corners; candles flicker, embers rise from the hearth, smoke curls up.
OverlayArt tavern(const Layout& L) {
    OverlayArt a;
    juce::Random r(2020);
    juce::Path beams, beamLight, nails, boards, boardLines, barrels, hoops, hearth, logs, shelf, mugs, foam, bottles, sign, signText, holders, candles;
    std::vector<Point> flames, hearths;
    const float bottom = L.bounds.getBottom(), width = L.bounds.getWidth();
    // Floorboards, and a stone hearth here and there.
    boards.addRectangle(0.0f, bottom - 7.0f, width, 7.0f);
    for (float x = 0.0f; x < width; x += between(r, 30.0f, 60.0f)) boardLines.addRectangle(x, bottom - 7.0f, 0.8f, 7.0f);
    boardLines.addRectangle(0.0f, bottom - 3.5f, width, 0.6f);
    for (float x = between(r, 120.0f, 220.0f); x < width - 40.0f; x += between(r, 360.0f, 480.0f)) {
        hearth.addRoundedRectangle(x - 18.0f, bottom - 15.0f, 36.0f, 10.0f, 3.0f);
        logs.addRoundedRectangle(x - 10.0f, bottom - 11.0f, 20.0f, 3.0f, 1.5f);
        logs.addRoundedRectangle(x - 8.0f, bottom - 13.0f, 16.0f, 2.5f, 1.2f);
        hearths.push_back({ x, bottom - 14.0f });
    }
    // Oak beams across the panel tops and bottoms, nailed; candles at the top corners.
    for (auto p : L.panels) {
        for (float y : { p.getY() - 3.0f, p.getBottom() - 2.0f }) {
            beams.addRectangle(p.getX() + 6.0f, y, p.getWidth() - 12.0f, 5.0f);
            beamLight.addRectangle(p.getX() + 6.0f, y, p.getWidth() - 12.0f, 1.0f);
            for (float x = p.getX() + 14.0f; x < p.getRight() - 10.0f; x += between(r, 40.0f, 70.0f)) nails.addEllipse(x - 1.0f, y + 1.5f, 2.0f, 2.0f);
        }
        for (float cx : { p.getX() + 3.0f, p.getRight() - 3.0f }) {
            holders.addRectangle(cx - 3.0f, p.getY() + 12.0f, 6.0f, 1.5f);
            holders.addRectangle(cx - 0.6f, p.getY() + 12.0f, 1.2f, 6.0f);
            candles.addRectangle(cx - 1.3f, p.getY() + 6.0f, 2.6f, 6.0f);
            flames.push_back({ cx, p.getY() + 4.5f });
        }
    }
    // Barrels stacked in the gaps.
    for (auto gap : L.gutters) {
        if (gap.getHeight() < 50.0f) continue;
        const float cx = gap.getCentreX(), w = std::min(12.0f, gap.getWidth() - 2.0f);
        for (int k = 0; k < 2; ++k) {
            const Rect barrel { cx - w * 0.5f, gap.getBottom() - 10.0f - float(k + 1) * 15.0f, w, 14.0f };
            barrels.addRoundedRectangle(barrel, w * 0.35f);
            hoops.addRectangle(barrel.getX() + 0.5f, barrel.getY() + 3.0f, barrel.getWidth() - 1.0f, 1.0f);
            hoops.addRectangle(barrel.getX() + 0.5f, barrel.getBottom() - 4.0f, barrel.getWidth() - 1.0f, 1.0f);
        }
    }
    // A shelf of tankards and bottles, and a hanging inn sign, in the open space.
    for (auto o : L.open) {
        const float base = o.getBottom() - 2.0f;
        shelf.addRectangle(o.getX() + 6.0f, base, o.getWidth() * 0.6f, 2.5f);
        for (float x = o.getX() + 12.0f; x < o.getX() + o.getWidth() * 0.6f - 6.0f; x += between(r, 12.0f, 18.0f)) {
            if (r.nextFloat() < 0.55f) {
                mugs.addRoundedRectangle(x, base - 9.0f, 7.0f, 9.0f, 1.2f);
                juce::Path handle;
                handle.addArc(x + 5.5f, base - 7.5f, 4.0f, 5.0f, 0.0f, juce::MathConstants<float>::pi);
                juce::PathStrokeType(1.0f).createStrokedPath(handle, handle);
                mugs.addPath(handle);
                foam.addRoundedRectangle(x - 0.5f, base - 10.5f, 8.0f, 2.5f, 1.2f);
            } else {
                const float h = std::min(o.getHeight() - 6.0f, between(r, 12.0f, 16.0f));
                bottles.addRoundedRectangle(x + 1.0f, base - h * 0.65f, 5.0f, h * 0.65f, 1.5f);
                bottles.addRectangle(x + 2.5f, base - h, 2.0f, h * 0.4f);
            }
        }
        const float sx = o.getRight() - 50.0f, sy = o.getY() + 2.0f;
        const float sh = std::min(o.getHeight() - 8.0f, 18.0f);
        sign.addRectangle(sx - 26.0f, sy, 52.0f, 1.5f);
        sign.addRectangle(sx - 18.0f, sy, 1.0f, 4.0f);
        sign.addRectangle(sx + 17.0f, sy, 1.0f, 4.0f);
        const Rect board { sx - 22.0f, sy + 4.0f, 44.0f, sh };
        sign.addRoundedRectangle(board, 2.0f);
        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(juce::Font(juce::FontOptions(std::min(10.0f, sh - 4.0f), juce::Font::bold)), "INN", 0.0f, 0.0f);
        juce::Path text;
        glyphs.createPath(text);
        const auto tb = text.getBounds();
        text.applyTransform(juce::AffineTransform::translation(board.getCentreX() - tb.getCentreX(), board.getCentreY() - tb.getCentreY()));
        signText.addPath(text);
    }
    a.shapes.push_back({ boards, juce::Colour(0xff5a3a22) });
    a.shapes.push_back({ boardLines, juce::Colour(0xff3a2414) });
    a.shapes.push_back({ hearth, juce::Colour(0xff6a6260) });
    a.shapes.push_back({ logs, juce::Colour(0xff4a2c18) });
    a.shapes.push_back({ barrels, juce::Colour(0xff8a5a30) });
    a.shapes.push_back({ hoops, juce::Colour(0xff3a3634) });
    a.shapes.push_back({ beams, juce::Colour(0xff4e321e) });
    a.shapes.push_back({ beamLight, juce::Colour(0xff7a5434) });
    a.shapes.push_back({ nails, juce::Colour(0xff2a2624) });
    a.shapes.push_back({ holders, juce::Colour(0xff2e2a28) });
    a.shapes.push_back({ candles, juce::Colour(0xfff0e4c8) });
    a.shapes.push_back({ shelf, juce::Colour(0xff5a3a22) });
    a.shapes.push_back({ mugs, juce::Colour(0xffa08060) });
    a.shapes.push_back({ foam, juce::Colour(0xfff8f0dc) });
    a.shapes.push_back({ bottles, juce::Colour(0xff2a5a3a) });
    a.shapes.push_back({ sign, juce::Colour(0xff6a4428) });
    a.shapes.push_back({ signText, juce::Colour(0xffe8c070) });

    const int flame = int(a.sprites.size());
    a.sprites.push_back(glowSprite(5.0f, juce::Colour(0xb0ffa040), juce::Colour(0xfffff0c0), 1.3f));
    const int ember = int(a.sprites.size());
    a.sprites.push_back(glowSprite(3.0f, juce::Colour(0xb0ff7030), juce::Colour(0xffffd090), 0.8f));
    const int fire = int(a.sprites.size());
    a.sprites.push_back(glowSprite(10.0f, juce::Colour(0x80ff8030), juce::Colour(0xc0ffd080), 2.5f));
    const int smoke = puffSprites(a, juce::Colour(0x50a09080));
    for (auto f : flames) move(a, PlaceOverlays::MoveFlicker, f, flame, between(r, 0.8f, 1.6f), r.nextFloat(), 0.95f);
    for (auto h : hearths) {
        move(a, PlaceOverlays::MoveFlicker, h, fire, 1.2f, r.nextFloat(), 0.9f);
        for (int k = 0; k < 4; ++k) move(a, PlaceOverlays::MoveRise, h.translated(between(r, -8.0f, 8.0f), 0.0f), ember, between(r, 1.5f, 2.5f), float(k) / 4.0f, 1.0f, 16.0f, 4.0f);
        move(a, PlaceOverlays::MoveRise, h.translated(0.0f, -4.0f), smoke, 3.5f, r.nextFloat(), 0.6f, 20.0f, 3.0f, 8);
    }
    return a;
}
} // namespace

//==============================================================================
float between(juce::Random& r, float a, float b) { return a + (b - a) * r.nextFloat(); }

void blade(juce::Path& p, float x, float base, float height, float lean, float width) {
    p.startNewSubPath(x - width * 0.5f, base);
    p.quadraticTo(x - width * 0.2f + lean * 0.3f, base - height * 0.55f, x + lean, base - height);
    p.quadraticTo(x + width * 0.2f + lean * 0.3f, base - height * 0.55f, x + width * 0.5f, base);
    p.closeSubPath();
}

void leaf(juce::Path& p, Point at, float size, float angle) {
    juce::Path l;
    l.startNewSubPath(0.0f, 0.0f);
    l.quadraticTo(size * 0.5f, -size * 0.45f, size, 0.0f);
    l.quadraticTo(size * 0.5f, size * 0.45f, 0.0f, 0.0f);
    l.closeSubPath();
    p.addPath(l, juce::AffineTransform::rotation(angle).translated(at));
}

void vine(juce::Path& stem, juce::Path& leavesA, juce::Path& leavesB, Point a, Point b, juce::Random& r, float leafSize) {
    const auto d = b - a;
    const float length = d.getDistanceFromOrigin();
    if (length < 6.0f) return;
    const Point u { d.x / length, d.y / length }, n { -u.y, u.x };
    const float phase = r.nextFloat() * 6.28f;
    const auto at = [&](float s) { return a + u * s + n * (2.0f * std::sin(s * 0.12f + phase)); };
    stem.startNewSubPath(at(0.0f));
    for (float s = 3.0f; s < length; s += 3.0f) stem.lineTo(at(s));
    const float direction = std::atan2(u.y, u.x);
    float side = 1.0f;
    for (float s = 3.0f; s < length; s += between(r, 4.0f, 7.0f)) {
        const float size = between(r, 6.5f, 10.0f) * (1.0f - 0.4f * s / length) * leafSize;
        leaf(r.nextBool() ? leavesA : leavesB, at(s), size, direction + side * between(r, 0.6f, 1.15f));
        side = -side;
    }
}

std::vector<Point> trackAround(juce::Rectangle<float> frame, float corner) {
    juce::Path p;
    p.addRoundedRectangle(frame, corner);
    const float length = p.getLength();
    const int count = std::max(8, int(length / 3.0f));
    std::vector<Point> points;
    points.reserve(std::size_t(count));
    for (int i = 0; i < count; ++i) points.push_back(p.getPointAlongPath(length * float(i) / float(count)));
    return points;
}

std::vector<Point> spotsIn(const juce::RectangleList<int>& area, int count, int seed) {
    std::vector<Point> spots;
    float total = 0;
    for (const auto& r : area) total += float(r.getWidth() * r.getHeight());
    if (total <= 0.0f) return spots;
    juce::Random random(seed);
    for (int i = 0; i < count; ++i) {
        float pick = random.nextFloat() * total;
        for (const auto& r : area) {
            pick -= float(r.getWidth() * r.getHeight());
            if (pick > 0.0f) continue;
            spots.push_back({ float(r.getX()) + random.nextFloat() * float(r.getWidth()), float(r.getY()) + random.nextFloat() * float(r.getHeight()) });
            break;
        }
    }
    return spots;
}

void glowDot(juce::Graphics& g, Point p, float radius, juce::Colour glow, juce::Colour core, float coreRadius) {
    g.setGradientFill(juce::ColourGradient(glow, p.x, p.y, glow.withAlpha(0.0f), p.x + radius, p.y, true));
    g.fillEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(p));
    g.setColour(core);
    g.fillEllipse(juce::Rectangle<float>(coreRadius * 2.0f, coreRadius * 2.0f).withCentre(p));
}

juce::Image glowSprite(float radius, juce::Colour glow, juce::Colour core, float coreRadius) {
    const int size = int(std::ceil(radius * 2.0f)) + 2;
    juce::Image image(juce::Image::ARGB, size, size, true);
    juce::Graphics g(image);
    glowDot(g, { float(size) * 0.5f, float(size) * 0.5f }, radius, glow, core, coreRadius);
    return image;
}

juce::Image pathSprite(const juce::Path& path, juce::Colour colour, float glow, juce::Colour glowColour) {
    const auto b = path.getBounds();
    const float margin = 2.0f + 2.0f * glow;
    const int w = int(std::ceil(b.getWidth() + 2.0f * margin)), h = int(std::ceil(b.getHeight() + 2.0f * margin));
    juce::Image image(juce::Image::ARGB, std::max(w, 1), std::max(h, 1), true);
    juce::Graphics g(image);
    const auto shift = juce::AffineTransform::translation(float(w) * 0.5f - b.getCentreX(), float(h) * 0.5f - b.getCentreY());
    for (int k = 3; k >= 1 && glow > 0.0f; --k) {
        g.setColour(glowColour.withAlpha(0.16f));
        g.strokePath(path, juce::PathStrokeType(glow * float(k), juce::PathStrokeType::curved, juce::PathStrokeType::rounded), shift);
    }
    g.setColour(colour);
    g.fillPath(path, shift);
    return image;
}

bool hasArt(int type) {
    return type == Factory || type == City || type == Forest || type == Hive || type == Temple || type == Cavern || type == Ruins || type == Spaceship
           || type == Biodome || type == SeaShip || type == Suburbs || type == Mall || type == Military || type == Beach || type == Desert || type == Palace || type == Tavern;
}

OverlayArt build(int type, const Layout& layout) {
    switch (type) {
        case Factory: return factory(layout);
        case City: return city(layout);
        case Forest: return forest(layout);
        case Hive: return hive(layout);
        case Temple: return temple(layout);
        case Cavern: return cavern(layout);
        case Ruins: return ruins(layout);
        case Spaceship: return spaceship(layout);
        case Biodome: return biodome(layout);
        case SeaShip: return seaship(layout);
        case Suburbs: return suburbs(layout);
        case Mall: return mall(layout);
        case Military: return military(layout);
        case Beach: return beach(layout);
        case Desert: return desert(layout);
        case Palace: return palace(layout);
        case Tavern: return tavern(layout);
        default: return {};
    }
}

} // namespace wb::ui::art
