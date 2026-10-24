#include "PlaceArt.h"
#include "../Places/Place.h"

namespace wb::ui {

namespace {
// Shapes are drawn in a 100 x 100 box, ground at y = 88.
void rect(juce::Path& p, float x, float y, float w, float h) { p.addRectangle(x, y, w, h); }

void crenellations(juce::Path& p, float x, float y, float w, int count) {
    const float step = w / float(count * 2 - 1);
    for (int i = 0; i < count; ++i) p.addRectangle(x + float(i * 2) * step, y - 5.0f, step, 5.0f);
}

void triangle(juce::Path& p, float x1, float y1, float x2, float y2, float x3, float y3) { p.addTriangle(x1, y1, x2, y2, x3, y3); }

juce::Path castle() {
    juce::Path p;
    rect(p, 30, 40, 40, 48);          // keep
    crenellations(p, 30, 40, 40, 5);
    rect(p, 12, 50, 16, 38);          // towers
    rect(p, 72, 50, 16, 38);
    triangle(p, 10, 50, 20, 30, 30, 50);
    triangle(p, 70, 50, 80, 30, 90, 50);
    rect(p, 49, 18, 1.6f, 22);        // flag
    triangle(p, 50.6f, 18, 62, 22, 50.6f, 26);
    return p;
}
juce::Path castleDetail() {
    juce::Path p;
    p.addRoundedRectangle(43, 66, 14, 22, 7, 7, true, true, false, false);   // gate
    rect(p, 37, 50, 4, 7);
    rect(p, 59, 50, 4, 7);
    rect(p, 18, 60, 4, 7);
    rect(p, 78, 60, 4, 7);
    return p;
}

juce::Path house(float x, float w, float h) {
    juce::Path p;
    rect(p, x, 88 - h, w, h);
    triangle(p, x - 4, 88 - h, x + w * 0.5f, 88 - h - w * 0.55f, x + w + 4, 88 - h);
    return p;
}
juce::Path village() {
    juce::Path p = house(8, 26, 24);
    p.addPath(house(40, 32, 32));
    p.addPath(house(76, 18, 18));
    rect(p, 60, 36, 6, 14);   // chimney
    p.addEllipse(83, 46, 12, 12);   // tree
    return p;
}
juce::Path villageDetail() {
    juce::Path p;
    rect(p, 52, 70, 8, 18);
    rect(p, 15, 70, 6, 6);
    rect(p, 45, 64, 6, 6);
    p.startNewSubPath(63, 34);
    p.cubicTo(58, 28, 70, 24, 64, 16);   // smoke
    p.startNewSubPath(89, 58);
    p.lineTo(89, 70);
    return p;
}

juce::Path spaceport() {
    juce::Path p;
    // Rocket
    p.startNewSubPath(56, 78);
    p.lineTo(56, 34);
    p.quadraticTo(62, 14, 68, 34);
    p.lineTo(68, 78);
    p.closeSubPath();
    triangle(p, 56, 64, 49, 80, 56, 78);
    triangle(p, 68, 64, 75, 80, 68, 78);
    // Tower
    rect(p, 78, 26, 5, 62);
    rect(p, 68, 40, 10, 2.5f);
    // Dish
    p.addCentredArc(26, 56, 16, 9, -0.5f, 0.0f, juce::MathConstants<float>::pi, true);
    rect(p, 25, 62, 3, 20);
    // Pad
    p.addEllipse(10, 82, 80, 8);
    return p;
}
juce::Path spaceportDetail() {
    juce::Path p;
    p.addEllipse(59, 40, 6, 6);
    p.startNewSubPath(26, 56);
    p.lineTo(36, 44);
    return p;
}

juce::Path factory() {
    juce::Path p;
    p.startNewSubPath(8, 88);
    p.lineTo(8, 56);
    for (int i = 0; i < 3; ++i) {
        const float x = 8.0f + float(i) * 18.0f;
        p.lineTo(x + 18.0f, 46);
        p.lineTo(x + 18.0f, 56);
    }
    p.lineTo(62, 56);
    p.lineTo(92, 56);
    p.lineTo(92, 88);
    p.closeSubPath();
    rect(p, 66, 20, 8, 36);
    rect(p, 80, 30, 7, 26);
    return p;
}
juce::Path factoryDetail() {
    juce::Path p;
    for (int i = 0; i < 4; ++i) rect(p, 14.0f + float(i) * 12.0f, 66, 7, 8);
    p.startNewSubPath(70, 18);
    p.cubicTo(64, 10, 78, 6, 72, 0);
    return p;
}

juce::Path city() {
    juce::Path p;
    rect(p, 6, 52, 14, 36);
    rect(p, 22, 30, 16, 58);
    rect(p, 40, 44, 12, 44);
    rect(p, 54, 18, 14, 70);
    triangle(p, 54, 18, 61, 6, 68, 18);
    rect(p, 70, 40, 12, 48);
    rect(p, 84, 56, 10, 32);
    return p;
}
juce::Path cityDetail() {
    juce::Path p;
    for (int r = 0; r < 6; ++r) {
        rect(p, 26, 36.0f + float(r) * 8.0f, 3, 4);
        rect(p, 32, 36.0f + float(r) * 8.0f, 3, 4);
        rect(p, 58, 26.0f + float(r) * 9.0f, 3, 4);
        rect(p, 63, 26.0f + float(r) * 9.0f, 3, 4);
    }
    return p;
}

juce::Path temple() {
    juce::Path p;
    triangle(p, 10, 38, 50, 14, 90, 38);
    rect(p, 14, 38, 72, 6);
    for (int i = 0; i < 5; ++i) rect(p, 18.0f + float(i) * 14.5f, 46, 6, 34);
    rect(p, 8, 80, 84, 4);
    rect(p, 4, 84, 92, 4);
    return p;
}
juce::Path templeDetail() {
    juce::Path p;
    p.addEllipse(46, 24, 8, 8);
    return p;
}

juce::Path tree(float x, float h) {
    juce::Path p;
    triangle(p, x - 14, 88 - h * 0.35f, x, 88 - h, x + 14, 88 - h * 0.35f);
    triangle(p, x - 17, 88 - h * 0.08f, x, 88 - h * 0.7f, x + 17, 88 - h * 0.08f);
    return p;
}
juce::Path forest() {
    juce::Path p = tree(26, 60);
    p.addPath(tree(52, 78));
    p.addPath(tree(76, 54));
    return p;
}
juce::Path forestDetail() {
    juce::Path p;
    rect(p, 50.5f, 82, 3, 6);
    p.startNewSubPath(34, 30);
    p.quadraticTo(38, 26, 42, 30);
    return p;
}

juce::Path ruins() {
    juce::Path p;
    rect(p, 14, 40, 10, 48);
    p.startNewSubPath(34, 88);
    p.lineTo(34, 54);
    p.lineTo(39, 50);
    p.lineTo(44, 58);
    p.lineTo(44, 88);
    p.closeSubPath();
    rect(p, 58, 30, 10, 58);
    rect(p, 10, 34, 62, 6);
    p.addEllipse(76, 78, 14, 10);
    p.addEllipse(84, 72, 8, 7);
    return p;
}
juce::Path ruinsDetail() {
    juce::Path p;
    p.startNewSubPath(62, 50);
    p.lineTo(65, 58);
    p.lineTo(61, 66);
    return p;
}

juce::Path cavern() {
    juce::Path p;
    p.startNewSubPath(4, 88);
    p.lineTo(26, 40);
    p.lineTo(40, 50);
    p.lineTo(56, 22);
    p.lineTo(74, 46);
    p.lineTo(96, 88);
    p.closeSubPath();
    return p;
}
juce::Path cavernDetail() {
    juce::Path p;
    p.startNewSubPath(32, 88);
    p.quadraticTo(34, 54, 52, 54);
    p.quadraticTo(70, 54, 72, 88);
    triangle(p, 42, 58, 45, 68, 48, 56);
    triangle(p, 54, 56, 57, 70, 60, 57);
    return p;
}

juce::Path hive() {
    juce::Path p;
    const auto hex = [&p](float cx, float cy, float r) {
        juce::Path h;
        for (int i = 0; i < 6; ++i) {
            const float a = juce::MathConstants<float>::pi / 3.0f * float(i) + juce::MathConstants<float>::pi / 6.0f;
            const float x = cx + r * std::cos(a), y = cy + r * std::sin(a);
            if (i == 0) h.startNewSubPath(x, y);
            else h.lineTo(x, y);
        }
        h.closeSubPath();
        p.addPath(h);
    };
    hex(50, 52, 14);
    hex(26, 66, 12);
    hex(74, 66, 12);
    hex(38, 34, 10);
    hex(64, 32, 9);
    hex(50, 78, 10);
    return p;
}
juce::Path hiveDetail() {
    juce::Path p;
    p.addEllipse(46, 48, 8, 8);
    p.startNewSubPath(64, 22);
    p.quadraticTo(72, 10, 82, 14);
    return p;
}
} // namespace

void drawPlaceIcon(juce::Graphics& g, int type, juce::Rectangle<float> area, juce::Colour ink, juce::Colour fill) {
    juce::Path shape, detail;
    switch (type) {
        case Castle: shape = castle(); detail = castleDetail(); break;
        case Village: shape = village(); detail = villageDetail(); break;
        case Spaceport: shape = spaceport(); detail = spaceportDetail(); break;
        case Factory: shape = factory(); detail = factoryDetail(); break;
        case City: shape = city(); detail = cityDetail(); break;
        case Temple: shape = temple(); detail = templeDetail(); break;
        case Forest: shape = forest(); detail = forestDetail(); break;
        case Ruins: shape = ruins(); detail = ruinsDetail(); break;
        case Cavern: shape = cavern(); detail = cavernDetail(); break;
        case Hive: shape = hive(); detail = hiveDetail(); break;
        default: return;
    }
    const float side = std::min(area.getWidth(), area.getHeight());
    const auto box = area.withSizeKeepingCentre(side, side);
    const auto transform = juce::AffineTransform::scale(side / 100.0f).translated(box.getX(), box.getY());
    shape.applyTransform(transform);
    detail.applyTransform(transform);
    shape.setUsingNonZeroWinding(true);
    g.setGradientFill(juce::ColourGradient(fill.brighter(0.3f), box.getCentreX(), box.getY(), fill.darker(0.5f), box.getCentreX(),
                                           box.getBottom(), false));
    g.fillPath(shape);
    const float stroke = std::max(1.0f, side / 70.0f);
    g.setColour(ink);
    g.strokePath(shape, juce::PathStrokeType(stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(ink.withMultipliedAlpha(0.8f));
    g.strokePath(detail, juce::PathStrokeType(stroke * 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine(box.getX() + side * 0.04f, box.getY() + side * 0.885f, box.getRight() - side * 0.04f, box.getY() + side * 0.885f, stroke * 0.7f);
}

} // namespace wb::ui
