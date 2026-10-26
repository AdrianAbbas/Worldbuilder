#include "Style.h"
#include "WorldLookAndFeel.h"

namespace wb::ui {

void drawGlow(juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour colour, float size) {
    // A few widening outlines, fading out: cheap and soft enough at these sizes.
    for (int k = 3; k >= 1; --k) {
        const float grow = size * float(k) / 3.0f;
        g.setColour(colour.withMultipliedAlpha(0.22f / float(k)));
        g.drawRoundedRectangle(r.expanded(grow * 0.5f), corner + grow * 0.5f, grow);
    }
}

void drawSurface(juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour base, juce::Colour accent, const SurfaceState& state) {
    const float alpha = state.enabled ? 1.0f : 0.45f;
    // Drop shadow under a raised surface (none when pressed in).
    if (!state.down) {
        g.setColour(juce::Colours::black.withAlpha(0.35f * alpha));
        g.fillRoundedRectangle(r.translated(0.0f, 1.2f), corner);
    }
    if ((state.hover || state.on) && state.enabled) drawGlow(g, r, corner, accent.withAlpha(state.on ? 0.9f : 0.6f), state.on ? 5.0f : 4.0f);
    auto body = state.on ? accent : base;
    if (state.hover && state.enabled) body = body.brighter(0.12f);
    const auto top = state.down ? body.darker(0.25f) : body.brighter(state.on ? 0.35f : 0.22f);
    const auto bottom = state.down ? body.brighter(0.05f) : body.darker(state.on ? 0.35f : 0.45f);
    g.setGradientFill(juce::ColourGradient(top.withMultipliedAlpha(alpha), 0.0f, r.getY(), bottom.withMultipliedAlpha(alpha), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle(r, corner);
    // The lit top edge (a bevel), or an inner shadow when pressed.
    {
        juce::Graphics::ScopedSaveState s(g);
        g.reduceClipRegion(r.withHeight(std::max(2.0f, r.getHeight() * 0.5f)).toNearestIntEdges());
        g.setColour((state.down ? juce::Colours::black.withAlpha(0.35f) : juce::Colours::white.withAlpha(state.on ? 0.45f : 0.16f)).withMultipliedAlpha(alpha));
        g.drawRoundedRectangle(r.reduced(1.0f).translated(0.0f, state.down ? 0.5f : 0.0f), std::max(0.0f, corner - 1.0f), 1.0f);
    }
    // A gloss band over the upper half of an active surface.
    if (state.on && !state.down) {
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.22f * alpha), 0.0f, r.getY(), juce::Colours::white.withAlpha(0.0f), 0.0f,
                                               r.getCentreY(), false));
        g.fillRoundedRectangle(r.reduced(1.5f).withHeight(r.getHeight() * 0.48f), std::max(0.0f, corner - 1.5f));
    }
    g.setColour((state.on ? accent.brighter(0.4f) : accent.withAlpha(state.hover ? 0.85f : 0.45f)).withMultipliedAlpha(alpha));
    g.drawRoundedRectangle(r, corner, state.on || state.hover ? 1.2f : 1.0f);
}

void drawWell(juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour edge, bool hover) {
    g.setGradientFill(juce::ColourGradient(colours::background.darker(0.45f), 0.0f, r.getY(), colours::panel.darker(0.1f), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle(r, corner);
    {
        // Inner shadow along the top edge; a faint light on the bottom lip.
        juce::Graphics::ScopedSaveState s(g);
        g.reduceClipRegion(r.toNearestIntEdges());
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.drawRoundedRectangle(r.translated(0.0f, 1.0f).expanded(0.5f), corner, 1.5f);
    }
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.drawHorizontalLine(int(r.getBottom()) - 1, r.getX() + corner, r.getRight() - corner);
    g.setColour(edge.withAlpha(hover ? 0.85f : 0.5f));
    g.drawRoundedRectangle(r, corner, 1.0f);
}

juce::Colour inkFor(const SurfaceState& state, juce::Colour accent) {
    if (!state.enabled) return colours::dim;
    if (state.on) return accent.getPerceivedBrightness() > 0.45f ? colours::background : colours::parchment;
    return colours::parchment.withAlpha(state.hover ? 1.0f : 0.88f);
}

void setIcon(juce::Component& c, Icon icon) { c.getProperties().set("wbIcon", int(icon)); }
Icon iconOf(const juce::Component& c) { return Icon(int(c.getProperties().getWithDefault("wbIcon", 0))); }

void drawIcon(juce::Graphics& g, Icon icon, juce::Rectangle<float> area, juce::Colour colour) {
    if (icon == Icon::None) return;
    // Drawn on a 24-unit grid, scaled into the area.
    const float size = std::min(area.getWidth(), area.getHeight());
    const auto box = juce::Rectangle<float>(size, size).withCentre(area.getCentre());
    const float k = size / 24.0f;
    const auto P = [&](float x, float y) { return juce::Point<float>(box.getX() + x * k, box.getY() + y * k); };
    juce::Path p, solid;
    const auto line = [&](float x0, float y0, float x1, float y1) {
        p.startNewSubPath(P(x0, y0));
        p.lineTo(P(x1, y1));
    };
    const auto rect = [&](float x, float y, float w, float h, float c = 1.5f) { p.addRoundedRectangle(box.getX() + x * k, box.getY() + y * k, w * k, h * k, c * k); };
    const auto dot = [&](float x, float y, float r) { solid.addEllipse(box.getX() + (x - r) * k, box.getY() + (y - r) * k, 2 * r * k, 2 * r * k); };
    const auto arc = [&](float cx, float cy, float r, float from, float to) {
        p.addCentredArc(box.getX() + cx * k, box.getY() + cy * k, r * k, r * k, 0.0f, juce::degreesToRadians(from), juce::degreesToRadians(to), true);
    };
    const auto arrowHead = [&](float x, float y, float dx, float dy) {   // an open head at (x, y) pointing along (dx, dy)
        const float n = std::sqrt(dx * dx + dy * dy);
        dx /= n;
        dy /= n;
        line(x - 4.0f * dx + 3.0f * dy, y - 4.0f * dy - 3.0f * dx, x, y);
        p.lineTo(P(x - 4.0f * dx - 3.0f * dy, y - 4.0f * dy + 3.0f * dx));
    };
    switch (icon) {
        case Icon::Power: arc(12, 13, 7.5f, 35, 325); line(12, 3, 12, 12); break;
        case Icon::Dice:
            rect(4, 4, 16, 16, 3.5f);
            dot(8.5f, 8.5f, 1.5f); dot(15.5f, 15.5f, 1.5f); dot(12, 12, 1.5f); dot(15.5f, 8.5f, 1.5f); dot(8.5f, 15.5f, 1.5f);
            break;
        case Icon::Edit:   // three sliders
            line(4, 7, 20, 7); line(4, 12, 20, 12); line(4, 17, 20, 17);
            dot(14, 7, 2.2f); dot(8, 12, 2.2f); dot(16, 17, 2.2f);
            break;
        case Icon::Close: line(6, 6, 18, 18); line(18, 6, 6, 18); break;
        case Icon::Patterns:   // a piano roll
            rect(3, 4, 18, 16, 2.0f); line(7, 4, 7, 20);
            solid.addRoundedRectangle(P(9, 7).x, P(9, 7).y, 4 * k, 2.4f * k, 1.0f * k);
            solid.addRoundedRectangle(P(14, 11).x, P(14, 11).y, 5 * k, 2.4f * k, 1.0f * k);
            solid.addRoundedRectangle(P(10, 15).x, P(10, 15).y, 6 * k, 2.4f * k, 1.0f * k);
            break;
        case Icon::Save:   // a tray with an arrow down
            line(12, 3, 12, 14); arrowHead(12, 14, 0, 1);
            line(4, 14, 4, 20); p.lineTo(P(20, 20)); p.lineTo(P(20, 14));
            break;
        case Icon::Load:   // a folder
            p.startNewSubPath(P(3, 7)); p.lineTo(P(3, 19)); p.lineTo(P(21, 19)); p.lineTo(P(21, 9)); p.lineTo(P(11, 9)); p.lineTo(P(9, 6)); p.lineTo(P(3, 6));
            p.closeSubPath();
            break;
        case Icon::Render:   // a waveform
            line(3, 12, 5, 12); p.lineTo(P(7, 6)); p.lineTo(P(9, 18)); p.lineTo(P(11, 4)); p.lineTo(P(13, 20)); p.lineTo(P(15, 8)); p.lineTo(P(17, 15));
            p.lineTo(P(19, 12)); p.lineTo(P(21, 12));
            break;
        case Icon::Midi:   // a five-pin socket
            p.addEllipse(box.getX() + 3 * k, box.getY() + 3 * k, 18 * k, 18 * k);
            dot(7.5f, 12, 1.3f); dot(16.5f, 12, 1.3f); dot(9, 8.2f, 1.3f); dot(15, 8.2f, 1.3f); dot(12, 7, 1.3f);
            line(10, 17, 14, 17);
            break;
        case Icon::DragOut: line(5, 12, 18, 12); arrowHead(19, 12, 1, 0); line(5, 6, 5, 18); break;
        case Icon::Import: line(19, 12, 7, 12); arrowHead(6, 12, -1, 0); line(19, 6, 19, 18); break;
        case Icon::Path:   // stops along a winding route
            p.startNewSubPath(P(4, 18)); p.cubicTo(P(4, 10), P(12, 16), P(12, 9)); p.cubicTo(P(12, 4), P(20, 8), P(20, 4));
            dot(4, 18, 2.2f); dot(12, 10, 2.2f); dot(20, 4, 2.2f);
            break;
        case Icon::Journey:   // from one place to another
            dot(5, 12, 2.4f); line(8, 12, 18, 12); arrowHead(19.5f, 12, 1, 0); p.addEllipse(box.getX() + 16 * k, box.getY() + 8 * k, 8 * k, 8 * k);
            break;
        case Icon::Undo: arc(13, 13, 7, -90, 120); arrowHead(6.5f, 9.5f, -0.4f, 1); break;
        case Icon::Redo: arc(11, 13, 7, -120, 90); arrowHead(17.5f, 9.5f, 0.4f, 1); break;
        case Icon::Copy: rect(8, 8, 12, 12, 2); p.startNewSubPath(P(5, 15)); p.lineTo(P(4, 15)); p.lineTo(P(4, 4)); p.lineTo(P(15, 4)); p.lineTo(P(15, 5)); break;
        case Icon::Paste: rect(5, 5, 14, 16, 2); rect(9, 3, 6, 4, 1); line(8.5f, 12, 15.5f, 12); line(8.5f, 16, 13.5f, 16); break;
        case Icon::Clear: p.addEllipse(box.getX() + 4 * k, box.getY() + 4 * k, 16 * k, 16 * k); line(7, 17, 17, 7); break;
        case Icon::Generate:   // a four-pointed spark and a small one
            p.startNewSubPath(P(10, 3)); p.quadraticTo(P(10, 10), P(17, 10.5f)); p.quadraticTo(P(10, 11), P(10, 18)); p.quadraticTo(P(10, 11), P(3, 10.5f));
            p.quadraticTo(P(10, 10), P(10, 3)); p.closeSubPath();
            line(18, 15, 18, 21); line(15, 18, 21, 18);
            break;
        case Icon::Up: line(12, 19, 12, 5); arrowHead(12, 5, 0, -1); break;
        case Icon::Down: line(12, 5, 12, 19); arrowHead(12, 19, 0, 1); break;
        case Icon::Lock: rect(5, 11, 14, 10, 2); arc(12, 11, 4.5f, -90, 90); line(7.5f, 11, 7.5f, 8.5f); line(16.5f, 11, 16.5f, 8.5f); dot(12, 16, 1.4f); break;
        case Icon::Draw:   // a pencil
            p.startNewSubPath(P(5, 19)); p.lineTo(P(6, 15)); p.lineTo(P(16, 5)); p.lineTo(P(19, 8)); p.lineTo(P(9, 18)); p.closeSubPath(); line(14, 7, 17, 10);
            break;
        case Icon::Select:   // a pointer
            p.startNewSubPath(P(6, 4)); p.lineTo(P(6, 19)); p.lineTo(P(10, 15)); p.lineTo(P(13, 21)); p.lineTo(P(15.5f, 20)); p.lineTo(P(12.5f, 14)); p.lineTo(P(18, 14));
            p.closeSubPath();
            break;
        case Icon::Back: line(15, 5, 8, 12); p.lineTo(P(15, 19)); break;
        case Icon::Snap:   // a magnet
            p.startNewSubPath(P(6, 4)); p.lineTo(P(6, 13)); p.cubicTo(P(6, 21), P(18, 21), P(18, 13)); p.lineTo(P(18, 4));
            line(10, 4, 10, 13); line(14, 4, 14, 13); line(6, 8, 10, 8); line(14, 8, 18, 8);
            break;
        case Icon::Note: dot(8, 17, 3.0f); line(11, 17, 11, 4); p.lineTo(P(18, 6)); p.lineTo(P(18, 9)); p.lineTo(P(11, 7)); break;
        case Icon::Sync: arc(12, 12, 7, 20, 160); arrowHead(5.4f, 14.4f, -0.5f, 1); arc(12, 12, 7, 200, 340); arrowHead(18.6f, 9.6f, 0.5f, -1); break;
        case Icon::Double: rect(3, 7, 8, 10, 1.5f); rect(13, 7, 8, 10, 1.5f); break;
        case Icon::Half: rect(3, 7, 8, 10, 1.5f); p.addRoundedRectangle(box.getX() + 13 * k, box.getY() + 7 * k, 8 * k, 10 * k, 1.5f * k); line(15, 9, 19, 15); break;
        case Icon::Chords: dot(8, 18, 2.4f); dot(8, 12, 2.4f); dot(8, 6, 2.4f); line(11, 18, 11, 4); line(14, 20, 20, 20); line(14, 14, 20, 14); break;
        case Icon::Tone12:   // a keyboard octave
            rect(3, 5, 18, 14, 1.5f); for (float x : { 6.0f, 9.0f, 12.0f, 15.0f, 18.0f }) line(x, 5, x, 19);
            for (float x : { 7.5f, 10.5f, 16.5f }) solid.addRectangle(box.getX() + (x - 0.9f) * k, box.getY() + 5 * k, 1.8f * k, 8 * k);
            break;
        case Icon::None:
        case Icon::Count: break;
    }
    g.setColour(colour);
    g.strokePath(p, juce::PathStrokeType(std::max(1.1f, 1.7f * k), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillPath(solid);
}

} // namespace wb::ui
