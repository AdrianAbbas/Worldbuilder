#include "PlaceOverlays.h"
#include "OverlayArt.h"

namespace wb::ui {

namespace {
using Shape = PlaceOverlays::Shape;
using Layout = PlaceOverlays::Layout;
using Point = juce::Point<float>;
using art::between, art::blade, art::vine, art::trackAround, art::spotsIn, art::glowSprite;

// The Spaceport's hologram: a slight shimmer and, every few seconds, a brief dropout.
float hologramFlicker(float t) {
    if (std::fmod(t, 3.7f) < 0.09f || std::fmod(t + 1.3f, 5.9f) < 0.05f) return 0.45f;
    return 0.9f + 0.1f * std::sin(t * 31.0f) * std::sin(t * 17.3f);
}

// --- Castle: grass along the bottom and on the ledges, ivy up the corners, banners,
// a stone trim, fireflies.
void buildCastle(const Layout& L, std::vector<Shape>& shapes) {
    juce::Random r(101);
    const float width = L.bounds.getWidth(), bottom = L.bounds.getBottom();
    juce::Path stone;
    for (float x = -6.0f; x < width; x += 16.0f) stone.addRoundedRectangle(x, bottom - 6.0f, 15.0f, 6.0f, 1.5f);
    shapes.push_back({ stone, juce::Colour(0xff7a7380) });

    // Ivy: up both sides from the bottom corners (twice, for a thicker growth), along
    // the bottom edges, hanging from the top corners.
    juce::Path stems, leavesDark, leavesLight;
    for (auto p : L.panels) {
        const float c = 4.0f, left = p.getX() - 0.5f, right = p.getRight() + 0.5f;
        for (int k = 0; k < 2; ++k) {
            const float reach = k == 0 ? between(r, 0.45f, 0.8f) : between(r, 0.2f, 0.4f);
            vine(stems, leavesDark, leavesLight, { left, p.getBottom() - c }, { left, p.getBottom() - p.getHeight() * reach }, r);
            vine(stems, leavesDark, leavesLight, { right, p.getBottom() - c }, { right, p.getBottom() - p.getHeight() * reach }, r);
        }
        vine(stems, leavesDark, leavesLight, { p.getX() + c, p.getBottom() + 0.5f }, { p.getX() + p.getWidth() * between(r, 0.2f, 0.4f), p.getBottom() + 0.5f }, r);
        vine(stems, leavesDark, leavesLight, { p.getRight() - c, p.getBottom() + 0.5f }, { p.getRight() - p.getWidth() * between(r, 0.2f, 0.4f), p.getBottom() + 0.5f }, r);
        vine(stems, leavesDark, leavesLight, { left, p.getY() + c }, { left, p.getY() + between(r, 40.0f, 90.0f) }, r);
        vine(stems, leavesDark, leavesLight, { right, p.getY() + c }, { right, p.getY() + between(r, 40.0f, 90.0f) }, r);
        vine(stems, leavesDark, leavesLight, { p.getX() + c, p.getY() - 0.5f }, { p.getX() + between(r, 50.0f, 110.0f), p.getY() - 0.5f }, r);
        vine(stems, leavesDark, leavesLight, { p.getRight() - c, p.getY() - 0.5f }, { p.getRight() - between(r, 40.0f, 90.0f), p.getY() - 0.5f }, r);
    }
    shapes.push_back({ stems, juce::Colour(0xff35602a), 1.6f });
    shapes.push_back({ leavesDark, juce::Colour(0xff3f8a34) });
    shapes.push_back({ leavesLight, juce::Colour(0xff78c052) });

    juce::Path dark, light;
    for (float x = 0.0f; x < width; x += between(r, 1.5f, 3.2f)) {
        const float clump = 0.5f + 0.5f * std::sin(x * 0.045f) * std::sin(x * 0.013f + 1.0f);
        blade(r.nextBool() ? dark : light, x, bottom + 1.0f, between(r, 5.0f, 9.0f) + 12.0f * clump * r.nextFloat(), between(r, -4.0f, 4.0f), between(r, 1.8f, 3.0f));
    }
    for (auto p : L.panels)   // tufts on the panels' top edges, as on a ledge
        for (float x = p.getX() + between(r, 14.0f, 40.0f); x < p.getRight() - 14.0f; x += between(r, 34.0f, 80.0f))
            for (int k = 0; k < 7; ++k)
                blade(r.nextBool() ? dark : light, x + between(r, -6.0f, 6.0f), p.getY() + 1.5f, between(r, 3.0f, 8.5f), between(r, -3.0f, 3.0f), 1.8f);
    shapes.push_back({ dark, juce::Colour(0xff3a6e2c) });
    shapes.push_back({ light, juce::Colour(0xff70ad4a) });

    // Banners: in the gaps between the top panels and in the header's open space.
    juce::Path cloth, emblem, rods;
    const auto banner = [&](float cx, float top, float w, float h) {
        rods.addRoundedRectangle(cx - w * 0.5f - 2.5f, top, w + 5.0f, 2.0f, 1.0f);
        juce::Path b;
        b.startNewSubPath(cx - w * 0.5f, top + 2.0f);
        b.lineTo(cx + w * 0.5f, top + 2.0f);
        b.lineTo(cx + w * 0.5f, top + h);
        b.lineTo(cx, top + h - w * 0.45f);
        b.lineTo(cx - w * 0.5f, top + h);
        b.closeSubPath();
        cloth.addPath(b);
        const float e = w * 0.28f, ey = top + h * 0.38f;
        emblem.addTriangle(cx, ey - e, cx + e, ey, cx - e, ey);
        emblem.addTriangle(cx - e, ey, cx + e, ey, cx, ey + e);
    };
    for (auto gap : L.gutters)
        if (std::abs(gap.getY() - L.top) < 2.0f) banner(gap.getCentreX(), gap.getY() + 3.0f, gap.getWidth() - 2.0f, 44.0f);
    for (auto o : L.open)
        for (float f : { 0.2f, 0.5f, 0.8f }) banner(o.getX() + o.getWidth() * f, o.getY() + 2.0f, 15.0f, std::min(o.getHeight() - 4.0f, 34.0f));
    shapes.push_back({ cloth, juce::Colour(0xff962436) });
    shapes.push_back({ cloth, juce::Colour(0xffe0bb60), 1.1f });
    shapes.push_back({ emblem, juce::Colour(0xffeccb6c) });
    shapes.push_back({ rods, juce::Colour(0xffa5844a) });
}

// --- Spaceport: neon frames and corner brackets, a scale along the bottom, a hologram
// in the open space (it flickers); lights run along the frames, scan lines sweep.
void buildSpaceport(const Layout& L, std::vector<Shape>& shapes, std::vector<std::vector<Point>>& tracks) {
    const juce::Colour neon(0xff4ff4ff), white(0xffd0fdff);
    juce::Path frames, brackets, ticks;
    for (auto p : L.panels) {
        frames.addRoundedRectangle(p.reduced(0.5f), 10.0f);
        tracks.push_back(trackAround(p.reduced(0.5f), 10.0f));
        const float o = 3.0f, len = 18.0f;
        const auto corner = [&](float x, float y, float dx, float dy) {
            brackets.startNewSubPath(x, y + dy * len);
            brackets.lineTo(x, y);
            brackets.lineTo(x + dx * len, y);
        };
        corner(p.getX() - o, p.getY() - o, 1.0f, 1.0f);
        corner(p.getRight() + o, p.getY() - o, -1.0f, 1.0f);
        corner(p.getX() - o, p.getBottom() + o, 1.0f, -1.0f);
        corner(p.getRight() + o, p.getBottom() + o, -1.0f, -1.0f);
    }
    const float bottom = L.bounds.getBottom();
    int i = 0;
    for (float x = 4.0f; x < L.bounds.getRight(); x += 9.0f, ++i) ticks.addRectangle(x, bottom - (i % 5 == 0 ? 7.0f : 4.0f), 1.2f, i % 5 == 0 ? 7.0f : 4.0f);
    shapes.push_back({ frames, neon.withAlpha(0.10f), 12.0f });
    shapes.push_back({ frames, neon.withAlpha(0.25f), 5.0f });
    shapes.push_back({ frames, neon, 1.6f });
    shapes.push_back({ brackets, white, 2.0f });
    shapes.push_back({ ticks, neon.withAlpha(0.7f) });
}

// --- Village: wooden beams and braces on the frames, posts in the gaps, a plank floor,
// rooftops with chimneys and a string of lanterns in the open space.
void buildVillage(const Layout& L, std::vector<Shape>& shapes, std::vector<Point>& lanterns, std::vector<juce::Rectangle<float>>& chimneys) {
    juce::Random r(303);
    juce::Path wood, grain, pegs, roofs, walls, windows, strings, lanternFrames;
    const auto beam = [&](juce::Rectangle<float> b, bool horizontal) {
        wood.addRoundedRectangle(b, 1.5f);
        for (int k = 1; k <= 2; ++k) {
            if (horizontal) {
                const float y = b.getY() + b.getHeight() * float(k) / 3.0f;
                grain.startNewSubPath(b.getX() + 2.0f, y);
                for (float x = b.getX() + 8.0f; x < b.getRight() - 2.0f; x += 8.0f) grain.lineTo(x, y + 0.6f * std::sin(x * 0.07f + float(k)));
            } else {
                const float x = b.getX() + b.getWidth() * float(k) / 3.0f;
                grain.startNewSubPath(x, b.getY() + 2.0f);
                for (float y = b.getY() + 8.0f; y < b.getBottom() - 2.0f; y += 8.0f) grain.lineTo(x + 0.6f * std::sin(y * 0.07f + float(k)), y);
            }
        }
    };
    const float top = L.top;
    for (auto p : L.panels) {
        beam({ p.getX() - 3.0f, p.getY() - 3.0f, p.getWidth() + 6.0f, 6.0f }, true);
        for (float x = p.getX() + 20.0f; x < p.getRight() - 10.0f; x += 46.0f) pegs.addEllipse(x - 1.2f, p.getY() - 1.2f, 2.4f, 2.4f);
        // Corner braces.
        juce::Path brace;
        brace.startNewSubPath(p.getX() + 1.0f, p.getY() + 13.0f);
        brace.lineTo(p.getX() + 13.0f, p.getY() + 1.0f);
        brace.startNewSubPath(p.getRight() - 1.0f, p.getY() + 13.0f);
        brace.lineTo(p.getRight() - 13.0f, p.getY() + 1.0f);
        juce::PathStrokeType(3.6f).createStrokedPath(brace, brace);
        wood.addPath(brace);
        // A chimney on the lower panels' beams.
        if (p.getY() > top + 2.0f) {
            const float x = p.getX() + p.getWidth() * between(r, 0.6f, 0.85f);
            walls.addRectangle(x, p.getY() - 10.0f, 6.0f, 8.0f);
            chimneys.push_back({ x + 3.0f, p.getY() - 11.0f, 0.0f, 14.0f });
        }
    }
    for (auto gap : L.gutters) beam({ gap.getCentreX() - 3.0f, gap.getY() + 4.0f, 6.0f, gap.getHeight() - 8.0f }, false);
    const float bottom = L.bounds.getBottom();
    for (float x = -10.0f, w = 0.0f; x < L.bounds.getRight(); x += w) {
        w = between(r, 50.0f, 110.0f);
        beam({ x, bottom - 6.0f, w - 1.0f, 6.0f }, true);
    }
    // Lanterns hanging in the gaps between the top panels.
    for (auto gap : L.gutters)
        if (std::abs(gap.getY() - top) < 2.0f) {
            const Point at { gap.getCentreX(), gap.getY() + 30.0f };
            strings.addRectangle(at.x - 0.4f, gap.getY() + 3.0f, 0.8f, 22.0f);
            lanternFrames.addRoundedRectangle(at.x - 4.5f, at.y - 5.5f, 9.0f, 11.0f, 2.0f);
            lanterns.push_back(at);
        }
    // Rooftops and a string of lanterns in the open space.
    for (auto o : L.open) {
        float x = o.getX() + 6.0f;
        while (x < o.getRight() - 30.0f) {
            const float w = between(r, 22.0f, 34.0f), h = between(r, 10.0f, 16.0f), base = o.getBottom();
            walls.addRectangle(x, base - h, w, h);
            roofs.addTriangle(x - 3.0f, base - h, x + w + 3.0f, base - h, x + w * 0.5f, base - h - w * 0.45f);
            const float cx = x + w * 0.72f;
            walls.addRectangle(cx, base - h - w * 0.4f, 4.0f, w * 0.3f);
            chimneys.push_back({ cx + 2.0f, base - h - w * 0.4f - 1.0f, 0.0f, o.getHeight() * 0.8f });
            windows.addRectangle(x + w * 0.25f, base - h * 0.62f, 4.0f, 4.0f);
            x += w + between(r, 10.0f, 26.0f);
        }
        juce::Path rope;
        const float y0 = o.getY() + 6.0f;
        rope.startNewSubPath(o.getX() + 4.0f, y0);
        rope.quadraticTo(o.getCentreX(), y0 + 14.0f, o.getRight() - 4.0f, y0);
        juce::PathStrokeType(0.9f).createStrokedPath(strings, rope);
        for (int k = 1; k < 7; ++k) {
            const float t = float(k) / 7.0f;
            const Point at { o.getX() + 4.0f + t * (o.getWidth() - 8.0f), y0 + 28.0f * t * (1.0f - t) + 4.0f };   // below the rope
            lanternFrames.addRoundedRectangle(at.x - 3.0f, at.y - 3.5f, 6.0f, 7.0f, 1.5f);
            lanterns.push_back(at);
        }
    }
    shapes.push_back({ wood, juce::Colour(0xff7e522e) });
    shapes.push_back({ grain, juce::Colour(0xff4a2e18), 0.7f });
    shapes.push_back({ pegs, juce::Colour(0xff2c1a0c) });
    shapes.push_back({ walls, juce::Colour(0xff5a3a28) });
    shapes.push_back({ roofs, juce::Colour(0xff8a3e26) });
    shapes.push_back({ windows, juce::Colour(0xffffc86a) });
    shapes.push_back({ strings, juce::Colour(0xff3a2616) });
    shapes.push_back({ lanternFrames, juce::Colour(0xff2a1a0e) });
}
} // namespace

bool hasOverlay(int type) { return type == Castle || type == Village || type == Spaceport || art::hasArt(type); }

PlaceOverlays::PlaceOverlays() {
    setInterceptsMouseClicks(false, false);
    setOpaque(false);
    sprites.firefly = glowSprite(7.0f, juce::Colour(0x80e8ff8a), juce::Colour(0xfffbffd2), 1.4f);
    sprites.lantern = glowSprite(18.0f, juce::Colour(0x73ffb050), juce::Colour(0xffffe2a0), 2.6f);
    sprites.cometGlow = glowSprite(12.0f, juce::Colour(0x994ff4ff), juce::Colours::white, 2.2f);
    for (std::size_t i = 0; i < sprites.smoke.size(); ++i) {
        sprites.smoke[i] = glowSprite(2.0f + float(i), juce::Colour(0x73c0b8b0), juce::Colours::transparentBlack, 0.0f);
        sprites.cometDot[i] = glowSprite(1.2f + 0.2f * float(i), juce::Colour(0xffe8ffff), juce::Colour(0xffe8ffff), 0.8f + 0.15f * float(i));
    }
}

void PlaceOverlays::stamp(juce::Graphics* g, const juce::Image& image, juce::Point<float> centre, float alpha, std::vector<juce::Rectangle<int>>& marks) const {
    const int x = juce::roundToInt(centre.x - float(image.getWidth()) * 0.5f), y = juce::roundToInt(centre.y - float(image.getHeight()) * 0.5f);
    const juce::Rectangle<int> area { x, y, image.getWidth(), image.getHeight() };
    marks.push_back(area);
    if (g == nullptr || alpha < 0.01f || !g->clipRegionIntersects(area)) return;
    g->setOpacity(std::min(1.0f, alpha));
    g->drawImageAt(image, x, y);
}

void PlaceOverlays::setFrames(std::vector<juce::Rectangle<int>> panels, std::vector<juce::Rectangle<int>> openSpace, const juce::RectangleList<int>& keepClear) {
    if (panels != panelList || openSpace != openList) {
        panelList = std::move(panels);
        openList = std::move(openSpace);
        for (auto& s : still) s = {};
    }
    clear = keepClear;
    rebuildArea();
    repaint();
}

void PlaceOverlays::resized() {
    for (auto& s : still) s = {};
    rebuildArea();
}

void PlaceOverlays::rebuildArea() {
    decorated.clear();
    decorated.add(getLocalBounds());
    for (auto p : panelList) decorated.subtract(p.reduced(frameBand));
    for (auto p : panelList) {
        const int c = 2 * frameBand;
        for (auto corner : { p.withSize(c, c), p.withSize(c, c).withRightX(p.getRight()), p.withSize(c, c).withBottomY(p.getBottom()),
                             p.withSize(c, c).withRightX(p.getRight()).withBottomY(p.getBottom()) })
            decorated.add(corner);
    }
    drawable = decorated;
    drawable.subtract(clear);
    roaming.clear();
    roaming.add(getLocalBounds());
    for (auto p : panelList) roaming.subtract(p.reduced(frameBand));
    roamingDrawable = roaming;
    roamingDrawable.subtract(clear);
    ++areaVersion;

    layout.bounds = getLocalBounds().toFloat();
    layout.panels.clear();
    layout.gutters.clear();
    layout.ledges.clear();   // (once missing: every layout change added the ledges again, and the Hive's honeycomb on them grew without end)
    layout.open.clear();
    layout.top = panelList.empty() ? 0.0f : float(panelList.front().getY());
    for (auto p : panelList) {
        layout.panels.push_back(p.toFloat());
        layout.top = std::min(layout.top, float(p.getY()));
    }
    for (auto a : panelList)
        for (auto b : panelList) {
            const int gap = b.getX() - a.getRight(), top = std::max(a.getY(), b.getY()), bottom = std::min(a.getBottom(), b.getBottom());
            if (gap >= 4 && gap <= 40 && bottom - top > 30) layout.gutters.push_back(juce::Rectangle<int>(a.getRight(), top, gap, bottom - top).toFloat());
        }
    for (auto a : panelList)
        for (auto b : panelList) {
            const int gap = b.getY() - a.getBottom(), left = std::max(a.getX(), b.getX()), right = std::min(a.getRight(), b.getRight());
            if (gap >= 4 && gap <= 40 && right - left > 30) layout.ledges.push_back(juce::Rectangle<int>(left, a.getBottom(), right - left, gap).toFloat());
        }
    for (auto o : openList) layout.open.push_back(o.toFloat());
}

void PlaceOverlays::setAmount(int type, float amount) {
    if (type > NoPlace && type < PlaceTypeCount) amounts[std::size_t(type)] = juce::jlimit(0.0f, 1.0f, amount);
}

void PlaceOverlays::setMode(int m) {
    m = juce::jlimit(0, OverlayModeCount - 1, m);
    if (m == overlayMode) return;
    overlayMode = m;
    repaint();
}

float PlaceOverlays::strength(int type) const {
    if (type <= NoPlace || type >= PlaceTypeCount || !hasOverlay(type)) return 0.0f;
    const float a = amounts[std::size_t(type)];
    const float eased = a * a * (3.0f - 2.0f * a);
    return eased * (overlayMode == OverlaysFull ? 1.0f : overlayMode == OverlaysSubtle ? 0.45f : 0.0f);
}

bool PlaceOverlays::showing() const {
    for (int type = NoPlace + 1; type < PlaceTypeCount; ++type)
        if (strength(type) > 0.004f) return true;
    return false;
}

bool PlaceOverlays::isLegacy(int type) { return type == Castle || type == Village || type == Spaceport; }

int PlaceOverlays::framesPerSecond(int type, int mode) {
    if (mode == OverlaysOff) return 0;
    if (mode == OverlaysSubtle) return 10;
    return isLegacy(type) ? 20 : 15;
}

bool PlaceOverlays::movesOn(int type, int tick) const {
    // On the editor's 30 Hz timer: 20 fps is two ticks of three, 15 every other, 10 every third.
    switch (framesPerSecond(type, overlayMode)) {
        case 20: return tick % 3 != 2;
        case 15: return tick % 2 == 0;
        case 10: return tick % 3 == 0;
        default: return false;
    }
}

void PlaceOverlays::advance(float seconds) {
    time += seconds;
    if (time > 3600.0f) time -= 3600.0f;
    const bool show = showing();
    bool fading = show != shownLastFrame;
    for (int type = NoPlace + 1; type < PlaceTypeCount; ++type) {
        const float s = strength(type);
        fading |= std::abs(s - paintedStrength[std::size_t(type)]) > 0.001f;
        paintedStrength[std::size_t(type)] = s;
        if (amounts[std::size_t(type)] <= 0.0f && still[std::size_t(type)].image.isValid()) still[std::size_t(type)].image = {};   // a Place gone
        if (s > 0.0f && !still[std::size_t(type)].built) build(type);
    }
    shownLastFrame = show;
    fadingNow = fading;
    if (!show) combined = {};
    repainted.clear();
    if (fading) {
        repaint();
        for (int type = NoPlace + 1; type < PlaceTypeCount; ++type) {
            animTime[std::size_t(type)] = time;
            moved[std::size_t(type)] = movingParts(type);
        }
        return;
    }
    ++frame;
    if (!show) return;
    // Each overlay moves on its own frames: repaint where its parts were and where they
    // are now, only within the decorated area, joining pieces while that adds little
    // area (each rectangle is a paint call in some hosts, but a large one would repaint
    // the panels' insides). An overlay between its frames stays exactly as it was.
    std::vector<juce::Rectangle<int>> dirty;
    const auto area = [](juce::Rectangle<int> r) { return int64_t(r.getWidth()) * int64_t(r.getHeight()); };
    const auto add = [&](const std::vector<juce::Rectangle<int>>& list, const juce::RectangleList<int>& within) {
        for (auto r : list)
            for (auto d : within) {
                const auto piece = r.getIntersection(d);
                if (piece.isEmpty()) continue;
                // Into the first rectangle it joins cheaply, else a new one.
                auto joined = std::find_if(dirty.begin(), dirty.end(),
                                           [&](juce::Rectangle<int> x) { return area(x.getUnion(piece)) - area(x) - area(piece) < 4000; });
                if (joined != dirty.end()) *joined = joined->getUnion(piece);
                else dirty.push_back(piece);
            }
    };
    for (int type = NoPlace + 1; type < PlaceTypeCount; ++type) {
        const auto k = std::size_t(type);
        if (strength(type) < 0.004f || !still[k].built || !movesOn(type, frame)) continue;
        animTime[k] = time;
        auto now = movingParts(type);
        const auto& within = isLegacy(type) ? decorated : roaming;
        add(moved[k], within);
        add(now, within);
        moved[k] = std::move(now);
    }
    while (dirty.size() > maxRepaintRects) {   // rarely: join the cheapest pair
        std::size_t bestA = 0, bestB = 1;
        int64_t bestCost = std::numeric_limits<int64_t>::max();
        for (std::size_t i = 0; i < dirty.size(); ++i)
            for (std::size_t j = i + 1; j < dirty.size(); ++j)
                if (const auto cost = area(dirty[i].getUnion(dirty[j])) - area(dirty[i]) - area(dirty[j]); cost < bestCost) {
                    bestCost = cost;
                    bestA = i;
                    bestB = j;
                }
        dirty[bestA] = dirty[bestA].getUnion(dirty[bestB]);
        dirty.erase(dirty.begin() + std::ptrdiff_t(bestB));
    }
    for (auto r : dirty) repaint(r);
    repainted = dirty;
}

std::vector<juce::Rectangle<int>> PlaceOverlays::movingParts(int type) const {
    std::vector<juce::Rectangle<int>> marks;
    if (const float s = strength(type); s >= 0.004f && still[std::size_t(type)].built) {
        marks.reserve(128);
        drawMoving(nullptr, type, still[std::size_t(type)], s, marks);
    }
    return marks;
}

const PlaceOverlays::Layout& PlaceOverlays::currentLayout() const { return layout; }

std::size_t PlaceOverlays::stillSegments(int type) const {
    if (type <= NoPlace || type >= PlaceTypeCount) return 0;
    std::size_t n = 0;
    for (const auto& shape : still[std::size_t(type)].shapes)
        for (juce::Path::Iterator it(shape.path); it.next();) ++n;
    return n;
}

void PlaceOverlays::build(int type) {
    ++builds;
    auto& s = still[std::size_t(type)];
    s = {};
    std::vector<Point> lanterns;
    std::vector<juce::Rectangle<float>> chimneys;
    switch (type) {
        case Castle: buildCastle(layout, s.shapes); break;
        case Spaceport: buildSpaceport(layout, s.shapes, s.tracks); break;
        case Village:
            buildVillage(layout, s.shapes, lanterns, chimneys);
            s.spots = lanterns;
            s.vents = chimneys;
            break;
        default:
            if (art::hasArt(type)) {
                auto a = art::build(type, layout);
                s.shapes = std::move(a.shapes);
                s.sprite = std::move(a.sprites);
                s.movers = std::move(a.movers);
                s.light = a.light;
            }
            break;
    }
    // Strokes become fills once, so a frame only fills paths.
    for (auto& shape : s.shapes)
        if (shape.stroke > 0.0f) {
            juce::Path filled;
            juce::PathStrokeType(shape.stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded).createStrokedPath(filled, shape.path);
            shape.path = std::move(filled);
            shape.stroke = 0.0f;
        }
    if (type == Castle) s.spots = spotsIn(decorated, 40, 7);
    s.built = true;
}

void PlaceOverlays::paint(juce::Graphics& g) {
    if (!showing()) return;
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor() > 1.01f ? 2.0f : 1.0f;
    // Light (neon) first, so growing and built things combine on top of it.
    std::array<int, PlaceTypeCount> order {};
    int count = 0;
    for (int pass = 0; pass < 2; ++pass)
        for (int type = NoPlace + 1; type < PlaceTypeCount; ++type)
            if (strength(type) >= 0.004f && (type == Spaceport || still[std::size_t(type)].light) == (pass == 0)) order[std::size_t(count++)] = type;
    // The still parts: their image, drawn once per layout and already clipped.
    for (int i = 0; i < count; ++i) {
        const int type = order[std::size_t(i)];
        auto& st = still[std::size_t(type)];
        if (!st.built) build(type);
        if (!st.image.isValid() || st.imageScale != scale || st.imageArea != areaVersion) {
            st.image = juce::Image(juce::Image::ARGB, juce::roundToInt(float(getWidth()) * scale), juce::roundToInt(float(getHeight()) * scale), true);
            juce::Graphics ig(st.image);
            ig.addTransform(juce::AffineTransform::scale(scale));
            ig.reduceClipRegion(drawable);
            for (const auto& shape : st.shapes) {
                ig.setColour(shape.colour);
                ig.fillPath(shape.path);
            }
            st.imageScale = scale;
            st.imageArea = areaVersion;
        }
    }
    // Steady (nothing fading): the still images are combined once into one, so a frame
    // draws one image however many overlays are showing.
    bool same = combined.isValid() && combinedScale == scale && combinedArea == areaVersion;
    for (int type = NoPlace + 1; type < PlaceTypeCount && same; ++type) same = combinedStrength[std::size_t(type)] == strength(type);
    if (!same && !fadingNow) {
        combined = juce::Image(juce::Image::ARGB, juce::roundToInt(float(getWidth()) * scale), juce::roundToInt(float(getHeight()) * scale), true);
        juce::Graphics cg(combined);
        for (int i = 0; i < count; ++i) {
            const int type = order[std::size_t(i)];
            cg.setOpacity(strength(type));
            cg.drawImageAt(still[std::size_t(type)].image, 0, 0);
        }
        for (int type = NoPlace + 1; type < PlaceTypeCount; ++type) combinedStrength[std::size_t(type)] = strength(type);
        combinedScale = scale;
        combinedArea = areaVersion;
        same = true;
    }
    if (same) {
        g.setOpacity(1.0f);
        if (scale == 1.0f) g.drawImageAt(combined, 0, 0);
        else g.drawImageTransformed(combined, juce::AffineTransform::scale(1.0f / scale));
    } else {
        for (int i = 0; i < count; ++i) {
            const int type = order[std::size_t(i)];
            g.setOpacity(strength(type));
            if (scale == 1.0f) g.drawImageAt(still[std::size_t(type)].image, 0, 0);
            else g.drawImageTransformed(still[std::size_t(type)].image, juce::AffineTransform::scale(1.0f / scale));
        }
    }
    // The moving parts, clipped to the drawable area (the part of it this paint covers).
    auto local = drawable;
    local.clipTo(g.getClipBounds());
    if (local.isEmpty()) return;
    g.reduceClipRegion(local);
    std::vector<juce::Rectangle<int>> marks;
    marks.reserve(256);
    for (int i = 0; i < count; ++i) {
        const int type = order[std::size_t(i)];
        if (!isLegacy(type)) continue;
        g.setOpacity(1.0f);
        drawMoving(&g, type, still[std::size_t(type)], strength(type), marks);
    }
    // The later overlays' sprites, only where they may move.
    auto roam = roamingDrawable;
    roam.clipTo(g.getClipBounds());
    if (roam.isEmpty()) return;
    g.reduceClipRegion(roam);
    for (int i = 0; i < count; ++i) {
        const int type = order[std::size_t(i)];
        if (isLegacy(type)) continue;
        g.setOpacity(1.0f);
        drawMoving(&g, type, still[std::size_t(type)], strength(type), marks);
    }
}

void PlaceOverlays::drawMoving(juce::Graphics* g, int type, const Still& st, float s, std::vector<juce::Rectangle<int>>& marks) const {
    const bool subtle = overlayMode == OverlaysSubtle;
    const float t = animTime[std::size_t(type)];
    if (!isLegacy(type)) {
        drawMovers(g, st, s, t, marks);
        return;
    }
    // Notes an area drawn on; true when it is to be drawn now (inside the clip).
    const auto mark = [&](juce::Rectangle<float> r) {
        const auto area = r.getSmallestIntegerContainer().expanded(2);
        marks.push_back(area);
        return g != nullptr && g->clipRegionIntersects(area);
    };
    switch (type) {
        case Castle: {   // fireflies wander and glow on and off
            const int count = std::min(int(st.spots.size()), subtle ? 14 : 34);
            for (int i = 0; i < count; ++i) {
                const float speed = 0.6f + 0.15f * float(i % 5), phase = float(i) * 1.7f;
                const auto p = st.spots[std::size_t(i)] + Point { 10.0f * std::sin(t * 0.45f * speed + phase), 7.0f * std::cos(t * 0.37f * speed + phase * 1.3f) };
                stamp(g, sprites.firefly, p, s * std::pow(std::max(0.0f, std::sin(t * 0.9f * speed + phase)), 1.5f), marks);
            }
            break;
        }
        case Spaceport: {
            const juce::Colour neon(0xff4ff4ff);
            const float flicker = hologramFlicker(t);
            // Lights running along each frame (each one noted as a whole).
            for (std::size_t k = 0; k < st.tracks.size(); ++k) {
                const auto& track = st.tracks[k];
                const int n = int(track.size());
                const int comets = subtle ? 1 : 2;
                for (int c = 0; c < comets; ++c) {
                    const float position = std::fmod(t * 90.0f / (3.0f * float(n)) + float(c) / float(comets) + float(k) * 0.37f, 1.0f);
                    const int head = int(position * float(n));
                    const auto first = marks.size();
                    for (int j = 12; j > 0; j -= 2) {
                        const float fade = 1.0f - float(j) / 13.0f;
                        stamp(g, sprites.cometDot[std::size_t(fade * 7.0f)], track[std::size_t((head - j + n * 2) % n)], s * flicker * fade, marks);
                    }
                    stamp(g, sprites.cometGlow, track[std::size_t(head)], s * flicker, marks);
                    auto bounds = marks[first];   // the comet noted as a whole
                    for (auto i = first; i < marks.size(); ++i) bounds = bounds.getUnion(marks[i]);
                    marks.resize(first);
                    marks.push_back(bounds);
                }
            }
            if (g != nullptr) g->setOpacity(1.0f);
            // Scan lines: one sweeping down, one across.
            const auto b = getLocalBounds().toFloat();
            const float y = std::fmod(t * 70.0f, b.getHeight() + 60.0f) - 30.0f;
            if (mark(b.withY(y - 26.0f).withHeight(28.0f))) {
                g->setGradientFill(juce::ColourGradient(neon.withAlpha(0.0f), 0.0f, y - 26.0f, neon.withAlpha(0.3f * s), 0.0f, y, false));
                g->fillRect(b.withY(y - 26.0f).withHeight(26.0f));
                g->setColour(neon.withAlpha(0.85f * s));
                g->fillRect(b.withY(y).withHeight(1.2f));
            }
            const float x = std::fmod(t * 110.0f + 300.0f, b.getWidth() + 60.0f) - 30.0f;
            if (mark(b.withX(x).withWidth(1.0f))) {
                g->setColour(neon.withAlpha(0.45f * s));
                g->fillRect(b.withX(x).withWidth(1.0f));
            }
            // Hologram glitch: a few bright lines for a moment.
            if (flicker < 0.5f && !subtle) {
                juce::Random glitch(int(t * 20.0f));
                for (int k = 0; k < 4; ++k) {
                    const auto line = b.withY(glitch.nextFloat() * b.getHeight()).withHeight(1.0f + 2.0f * glitch.nextFloat());
                    if (mark(line)) {
                        g->setColour(neon.withAlpha(0.5f * s));
                        g->fillRect(line);
                    }
                }
            }
            // Status lights blinking down the gaps.
            for (auto gap : layout.gutters)
                if (mark(gap.withSizeKeepingCentre(4.0f, gap.getHeight())))
                    for (float yy = gap.getY() + 10.0f; yy < gap.getBottom() - 6.0f; yy += 14.0f) {
                        const float on = std::sin(t * 3.0f + yy * 0.37f + gap.getX());
                        g->setColour(neon.withAlpha(s * (on > 0.5f ? 0.95f : 0.25f)));
                        g->fillRect(juce::Rectangle<float>(3.0f, 3.0f).withCentre({ gap.getCentreX(), yy }));
                    }
            // A flickering hologram with a radar sweep in the open space.
            for (auto o : layout.open) {
                if (!mark(o)) continue;
                const auto h = o.reduced(4.0f, 3.0f);
                const float a = s * flicker;
                g->setColour(neon.withAlpha(0.16f * a));
                for (float gx = h.getX() + 8.0f; gx < h.getRight(); gx += 8.0f) g->fillRect(gx, h.getY(), 0.6f, h.getHeight());
                for (float gy = h.getY() + 8.0f; gy < h.getBottom(); gy += 8.0f) g->fillRect(h.getX(), gy, h.getWidth(), 0.6f);
                g->setColour(neon.withAlpha(0.6f * a));
                g->drawRoundedRectangle(h, 4.0f, 1.2f);
                const auto centre = Point { o.getRight() - o.getHeight() * 0.6f, o.getCentreY() };
                const float radius = o.getHeight() * 0.42f, angle = t * 2.0f;
                g->setColour(neon.withAlpha(0.5f * a));
                g->drawEllipse(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre), 1.0f);
                juce::Path sweep;
                sweep.addPieSegment(juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre), angle, angle + 0.8f, 0.0f);
                g->setColour(neon.withAlpha(0.35f * a));
                g->fillPath(sweep);
            }
            break;
        }
        case Village: {
            // Lanterns glow and flicker warmly.
            for (std::size_t i = 0; i < st.spots.size(); ++i) {
                const float f = float(i);
                const float flick = 0.85f + 0.15f * std::sin(t * 7.0f + f) * std::sin(t * 3.1f + f * 2.0f);
                stamp(g, sprites.lantern, st.spots[i], s * flick, marks);
            }
            // Chimney smoke drifting up (each chimney's column noted as a whole).
            const int puffs = subtle ? 3 : 5;
            for (std::size_t i = 0; i < st.vents.size(); ++i) {
                const auto top = st.vents[i].getPosition();
                const float rise = st.vents[i].getHeight(), scale = juce::jlimit(0.4f, 1.0f, rise / 30.0f);
                const bool draw = mark({ top.x - 12.0f, top.y - rise - 12.0f, 32.0f * scale + 16.0f, rise + 20.0f });
                const auto noted = marks.size();
                for (int k = 0; k < puffs && draw; ++k) {
                    const float age = std::fmod(t * 0.3f + float(k) / float(puffs) + float(i) * 0.137f, 1.0f);
                    const auto p = top + Point { (std::sin(age * 5.0f + float(i)) * 3.0f + age * 8.0f) * scale, -age * rise };
                    const auto size = std::size_t(juce::jlimit(0, 7, int(age * 7.0f * scale + 0.5f)));
                    stamp(g, sprites.smoke[size], p, s * (1.0f - age) * std::min(1.0f, age * 6.0f), marks);
                }
                marks.resize(noted);   // the column already covers the puffs
            }
            break;
        }
        default: break;
    }
}

void PlaceOverlays::drawMovers(juce::Graphics* g, const Still& st, float s, float t, std::vector<juce::Rectangle<int>>& marks) const {
    if (st.sprite.empty()) return;
    const bool subtle = overlayMode == OverlaysSubtle;
    for (std::size_t i = 0; i < st.movers.size(); ++i) {
        if (subtle && (i & 1) != 0) continue;   // Subtle: half of them
        const auto& m = st.movers[i];
        const float u = std::fmod(t / m.period + m.phase, 1.0f);
        // Travellers fade in and out at the ends of their path.
        const float ends = std::min(1.0f, u * 5.0f) * std::min(1.0f, (1.0f - u) * 3.0f);
        auto p = m.at;
        float a = m.alpha;
        int frame = 0;
        switch (m.kind) {
            case MovePulse: a *= 0.3f + 0.7f * (0.5f + 0.5f * std::sin(juce::MathConstants<float>::twoPi * u)); break;
            case MoveBlink: a *= std::sin(juce::MathConstants<float>::twoPi * u) > -0.2f ? 1.0f : 0.1f; break;
            case MoveFlicker: a *= 0.7f + 0.3f * std::sin(t * 12.7f + float(i) * 1.9f) * std::sin(t * 5.3f + float(i)); break;
            case MoveSpin: frame = int(u * float(m.frames)); break;
            case MoveRise:
                p += { m.sway * std::sin(juce::MathConstants<float>::twoPi * (1.3f * u + m.phase)) + 0.3f * m.sway * u, -m.travel * u };
                a *= ends;
                frame = int(u * float(m.frames));
                break;
            case MoveFall:
                p += { m.sway * std::sin(juce::MathConstants<float>::twoPi * (1.7f * u + m.phase)), m.travel * u };
                a *= ends;
                frame = int(u * float(m.frames) * 3.0f) % std::max<int>(1, m.frames);
                break;
            default:   // drift
                p += { m.travel * u, m.sway * std::sin(juce::MathConstants<float>::twoPi * (2.0f * u + m.phase)) };
                a *= ends;
                break;
        }
        const auto index = std::min(st.sprite.size() - 1, std::size_t(m.sprite) + std::size_t(std::clamp(frame, 0, std::max(0, m.frames - 1))));
        stamp(g, st.sprite[index], p, s * a, marks);
    }
}

} // namespace wb::ui
