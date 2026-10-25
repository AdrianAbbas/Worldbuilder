#include "PlaceArt.h"
#include "../Places/Place.h"
#if WB_HAS_PLACE_ART
#include "WorldbuilderArt.h"
#endif

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

juce::Path spaceship() {
    juce::Path p;
    // A long hull in orbit with engines at the back.
    p.startNewSubPath(10, 60);
    p.quadraticTo(20, 44, 52, 42);
    p.lineTo(80, 46);
    p.quadraticTo(94, 52, 80, 60);
    p.lineTo(52, 64);
    p.quadraticTo(20, 66, 10, 60);
    p.closeSubPath();
    triangle(p, 30, 46, 22, 30, 44, 44);
    triangle(p, 30, 62, 22, 78, 44, 64);
    rect(p, 4, 54, 8, 4);
    return p;
}
juce::Path spaceshipDetail() {
    juce::Path p;
    for (int i = 0; i < 4; ++i) p.addEllipse(50.0f + float(i) * 7.0f, 50, 3.5f, 3.5f);
    p.startNewSubPath(4, 56);
    p.lineTo(0, 56);
    return p;
}

juce::Path biodome() {
    juce::Path p;
    p.addCentredArc(50, 84, 40, 40, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
    p.closeSubPath();
    rect(p, 6, 84, 88, 4);
    return p;
}
juce::Path biodomeDetail() {
    juce::Path p;
    p.startNewSubPath(50, 44);
    p.lineTo(50, 84);
    p.startNewSubPath(22, 56);
    p.quadraticTo(50, 64, 78, 56);
    p.addEllipse(28, 64, 14, 12);
    p.addEllipse(56, 58, 16, 16);
    rect(p, 34, 74, 2, 10);
    rect(p, 63, 72, 2, 12);
    return p;
}

juce::Path seaship() {
    juce::Path p;
    p.startNewSubPath(12, 70);
    p.lineTo(88, 70);
    p.lineTo(78, 84);
    p.lineTo(22, 84);
    p.closeSubPath();
    rect(p, 48, 16, 3, 54);
    p.startNewSubPath(52, 20);
    p.quadraticTo(74, 38, 52, 62);
    p.closeSubPath();
    p.startNewSubPath(47, 26);
    p.quadraticTo(30, 44, 47, 62);
    p.closeSubPath();
    return p;
}
juce::Path seashipDetail() {
    juce::Path p;
    p.startNewSubPath(4, 90);
    p.quadraticTo(14, 86, 24, 90);
    p.quadraticTo(34, 94, 44, 90);
    p.quadraticTo(56, 86, 66, 90);
    p.quadraticTo(78, 94, 96, 90);
    triangle(p, 51, 12, 60, 15, 51, 18);
    return p;
}

juce::Path suburbs() {
    juce::Path p = house(10, 30, 26);
    p.addPath(house(54, 26, 22));
    p.addEllipse(84, 52, 12, 14);
    rect(p, 89, 64, 2, 24);
    return p;
}
juce::Path suburbsDetail() {
    juce::Path p;
    for (int i = 0; i < 11; ++i) {
        p.startNewSubPath(4.0f + float(i) * 8.5f, 88);
        p.lineTo(4.0f + float(i) * 8.5f, 80);
    }
    p.startNewSubPath(2, 83);
    p.lineTo(96, 83);
    rect(p, 20, 70, 7, 8);
    rect(p, 62, 72, 6, 6);
    return p;
}

juce::Path mall() {
    juce::Path p;
    rect(p, 6, 46, 88, 42);
    rect(p, 24, 34, 52, 12);
    return p;
}
juce::Path mallDetail() {
    juce::Path p;
    p.addRoundedRectangle(40, 62, 20, 26, 10, 10, true, true, false, false);
    for (int i = 0; i < 3; ++i) {
        rect(p, 12.0f + float(i) * 8.0f, 56, 5, 18);
        rect(p, 66.0f + float(i) * 8.0f, 56, 5, 18);
    }
    rect(p, 32, 38, 36, 4);
    return p;
}

juce::Path military() {
    juce::Path p;
    triangle(p, 6, 88, 26, 58, 46, 88);
    triangle(p, 42, 88, 62, 54, 82, 88);
    rect(p, 86, 22, 2.5f, 66);
    rect(p, 88.5f, 22, 10, 7);
    return p;
}
juce::Path militaryDetail() {
    juce::Path p;
    triangle(p, 22, 88, 26, 74, 30, 88);
    triangle(p, 58, 88, 62, 72, 66, 88);
    rect(p, 2, 86, 96, 2);
    return p;
}

juce::Path beach() {
    juce::Path p;
    // A palm tree on a little island.
    p.startNewSubPath(44, 86);
    p.quadraticTo(40, 60, 52, 34);
    p.lineTo(56, 35);
    p.quadraticTo(46, 60, 50, 86);
    p.closeSubPath();
    for (float a : { -2.6f, -1.9f, -1.2f, -0.5f, 0.2f }) {
        juce::Path leaf;
        leaf.startNewSubPath(54, 34);
        leaf.quadraticTo(54 + 18 * std::cos(a - 0.3f), 34 + 18 * std::sin(a - 0.3f), 54 + 26 * std::cos(a), 40 + 26 * std::sin(a) * 0.6f);
        leaf.quadraticTo(54 + 14 * std::cos(a + 0.2f), 36 + 14 * std::sin(a + 0.2f), 54, 34);
        p.addPath(leaf);
    }
    p.addEllipse(20, 80, 60, 12);
    return p;
}
juce::Path beachDetail() {
    juce::Path p;
    p.addEllipse(76, 12, 14, 14);
    p.startNewSubPath(2, 92);
    p.quadraticTo(10, 88, 18, 92);
    p.startNewSubPath(82, 92);
    p.quadraticTo(90, 88, 98, 92);
    return p;
}

juce::Path desert() {
    juce::Path p;
    p.startNewSubPath(0, 88);
    p.quadraticTo(22, 62, 46, 76);
    p.quadraticTo(70, 56, 100, 74);
    p.lineTo(100, 88);
    p.closeSubPath();
    triangle(p, 54, 64, 70, 36, 86, 64);
    return p;
}
juce::Path desertDetail() {
    juce::Path p;
    p.addEllipse(14, 16, 16, 16);
    p.startNewSubPath(70, 36);
    p.lineTo(66, 64);
    p.startNewSubPath(24, 78);
    p.quadraticTo(34, 74, 44, 80);
    return p;
}

juce::Path palace() {
    juce::Path p;
    rect(p, 20, 50, 60, 38);
    p.addEllipse(34, 26, 32, 30);
    rect(p, 49, 12, 2, 16);
    rect(p, 8, 40, 10, 48);
    rect(p, 82, 40, 10, 48);
    p.addEllipse(7, 30, 12, 14);
    p.addEllipse(81, 30, 12, 14);
    return p;
}
juce::Path palaceDetail() {
    juce::Path p;
    p.addRoundedRectangle(43, 66, 14, 22, 7, 7, true, true, false, false);
    for (int i = 0; i < 4; ++i) p.addRoundedRectangle(24.0f + float(i) * 15.0f, 54, 6, 9, 3, 3, true, true, false, false);
    return p;
}

juce::Path tavern() {
    juce::Path p = house(14, 50, 36);
    rect(p, 70, 30, 6, 16);   // chimney
    rect(p, 80, 46, 2, 30);   // sign post
    rect(p, 80, 46, 14, 2);
    return p;
}
juce::Path tavernDetail() {
    juce::Path p;
    p.addRoundedRectangle(84, 50, 10, 9, 2);
    rect(p, 32, 68, 10, 20);
    rect(p, 50, 62, 10, 8);
    rect(p, 20, 62, 8, 8);
    p.startNewSubPath(73, 28);
    p.cubicTo(68, 22, 80, 18, 74, 10);
    return p;
}

// Milestone 7: the sci-fi Places.
juce::Path station() {
    juce::Path p;
    // A ring station: the wheel, its hub and spokes, solar wings.
    p.addEllipse(20, 22, 60, 60);
    rect(p, 4, 49, 16, 6);
    rect(p, 80, 49, 16, 6);
    return p;
}
juce::Path stationDetail() {
    juce::Path p;
    p.addEllipse(28, 30, 44, 44);
    p.addEllipse(42, 44, 16, 16);
    for (int i = 0; i < 4; ++i) {
        const float a = juce::MathConstants<float>::halfPi * float(i) + 0.4f;
        p.startNewSubPath(50 + 8 * std::cos(a), 52 + 8 * std::sin(a));
        p.lineTo(50 + 22 * std::cos(a), 52 + 22 * std::sin(a));
    }
    for (int i = 0; i < 3; ++i) {
        p.startNewSubPath(8.0f + 4.0f * float(i), 49);
        p.lineTo(8.0f + 4.0f * float(i), 55);
        p.startNewSubPath(84.0f + 4.0f * float(i), 49);
        p.lineTo(84.0f + 4.0f * float(i), 55);
    }
    return p;
}

juce::Path megacity() {
    juce::Path p;
    // Stacked towers, one megastructure spire in the middle.
    rect(p, 6, 46, 14, 42);
    rect(p, 20, 34, 12, 54);
    triangle(p, 40, 88, 50, 6, 60, 88);
    rect(p, 62, 28, 14, 60);
    rect(p, 76, 50, 18, 38);
    return p;
}
juce::Path megacityDetail() {
    juce::Path p;
    for (float y = 40; y < 84; y += 8) {
        p.startNewSubPath(64, y);
        p.lineTo(74, y);
    }
    for (float y = 52; y < 84; y += 7) {
        p.startNewSubPath(8, y);
        p.lineTo(18, y);
    }
    p.addRoundedRectangle(78, 56, 14, 6, 2);   // a neon sign
    p.startNewSubPath(50, 6);
    p.lineTo(50, 0);
    for (int i = 0; i < 5; ++i) {   // rain
        p.startNewSubPath(12.0f + 18.0f * float(i), 10);
        p.lineTo(9.0f + 18.0f * float(i), 22);
    }
    return p;
}

juce::Path bridge() {
    juce::Path p;
    // The bridge seen from behind the captain's chair: a wide viewport over consoles.
    p.startNewSubPath(6, 64);
    p.quadraticTo(50, 8, 94, 64);
    p.lineTo(94, 70);
    p.lineTo(6, 70);
    p.closeSubPath();
    rect(p, 10, 72, 28, 10);
    rect(p, 62, 72, 28, 10);
    rect(p, 42, 66, 16, 22);
    return p;
}
juce::Path bridgeDetail() {
    juce::Path p;
    // Target brackets and stars in the viewport.
    for (auto [x, y] : { std::pair { 30.0f, 46.0f }, std::pair { 66.0f, 40.0f }, std::pair { 50.0f, 30.0f } }) p.addEllipse(x, y, 2.0f, 2.0f);
    p.startNewSubPath(56, 34);
    p.lineTo(56, 30);
    p.lineTo(60, 30);
    p.startNewSubPath(76, 34);
    p.lineTo(76, 30);
    p.lineTo(72, 30);
    p.startNewSubPath(56, 46);
    p.lineTo(56, 50);
    p.lineTo(60, 50);
    p.startNewSubPath(76, 46);
    p.lineTo(76, 50);
    p.lineTo(72, 50);
    return p;
}

juce::Path mining() {
    juce::Path p;
    // An asteroid with a drilling rig on top.
    p.startNewSubPath(8, 88);
    p.quadraticTo(10, 56, 34, 52);
    p.quadraticTo(56, 40, 76, 54);
    p.quadraticTo(96, 64, 92, 88);
    p.closeSubPath();
    rect(p, 46, 18, 6, 36);
    triangle(p, 36, 54, 49, 14, 62, 54);
    return p;
}
juce::Path miningDetail() {
    juce::Path p;
    p.addEllipse(22, 66, 10, 7);
    p.addEllipse(66, 70, 12, 8);
    p.startNewSubPath(40, 40);
    p.lineTo(58, 40);
    p.startNewSubPath(42, 30);
    p.lineTo(56, 30);
    p.startNewSubPath(54, 16);   // the drill light
    p.lineTo(80, 4);
    p.startNewSubPath(54, 18);
    p.lineTo(84, 14);
    return p;
}

juce::Path alienRuins() {
    juce::Path p;
    // A broken alien arch and a leaning monolith.
    p.startNewSubPath(10, 88);
    p.lineTo(14, 40);
    p.quadraticTo(36, 10, 58, 40);
    p.lineTo(60, 88);
    p.lineTo(50, 88);
    p.lineTo(48, 46);
    p.quadraticTo(36, 28, 24, 46);
    p.lineTo(20, 88);
    p.closeSubPath();
    p.startNewSubPath(70, 88);
    p.lineTo(74, 30);
    p.lineTo(88, 34);
    p.lineTo(90, 88);
    p.closeSubPath();
    return p;
}
juce::Path alienRuinsDetail() {
    juce::Path p;
    // Glyphs and a crack.
    p.addEllipse(77, 44, 6, 6);
    p.startNewSubPath(76, 58);
    p.lineTo(86, 58);
    p.startNewSubPath(80, 62);
    p.lineTo(80, 72);
    p.startNewSubPath(32, 22);
    p.lineTo(36, 30);
    p.lineTo(33, 36);
    p.addEllipse(14, 56, 4, 4);
    p.addEllipse(52, 60, 4, 4);
    return p;
}

juce::Path nebula() {
    juce::Path p;
    // Billowing gas clouds.
    p.addEllipse(8, 40, 44, 30);
    p.addEllipse(30, 22, 44, 36);
    p.addEllipse(50, 44, 42, 30);
    p.addEllipse(26, 56, 40, 24);
    return p;
}
juce::Path nebulaDetail() {
    juce::Path p;
    for (auto [x, y, r] : { std::tuple { 20.0f, 14.0f, 2.0f }, std::tuple { 84.0f, 20.0f, 2.5f }, std::tuple { 52.0f, 40.0f, 4.0f },
                            std::tuple { 12.0f, 82.0f, 1.5f }, std::tuple { 88.0f, 84.0f, 2.0f } })
        p.addStar({ x, y }, 4, r * 0.3f, r * 1.6f, 0.0f);
    p.startNewSubPath(30, 48);
    p.quadraticTo(50, 56, 72, 46);
    return p;
}

// Instrument glyphs, in a 20 x 20 box.
juce::Path glyph(int icon) {
    juce::Path p;
    switch (icon) {
        case IconDrone:   // three long held lines
            for (int i = 0; i < 3; ++i) rect(p, 2, 5.0f + float(i) * 5.0f, 16, 1.6f);
            break;
        case IconBell:
        case IconPA:
            p.startNewSubPath(4, 15);
            p.quadraticTo(5, 4, 10, 3);
            p.quadraticTo(15, 4, 16, 15);
            p.closeSubPath();
            p.addEllipse(8.5f, 15, 3, 3);
            if (icon == IconPA) {
                p.startNewSubPath(17, 5);
                p.quadraticTo(20, 9, 17, 13);
            }
            break;
        case IconHorn:
            p.startNewSubPath(2, 9);
            p.lineTo(11, 8);
            p.quadraticTo(15, 4, 18, 3);
            p.lineTo(18, 17);
            p.quadraticTo(15, 16, 11, 12);
            p.lineTo(2, 11);
            p.closeSubPath();
            break;
        case IconTimpani:
        case IconHandDrum:
        case IconSnare:
            p.addEllipse(3, 5, 14, 5);
            p.startNewSubPath(3, 7.5f);
            if (icon == IconTimpani) {
                p.quadraticTo(4, 17, 10, 17);
                p.quadraticTo(16, 17, 17, 7.5f);
            } else {
                p.lineTo(3, 15);
                p.lineTo(17, 15);
                p.lineTo(17, 7.5f);
            }
            if (icon == IconSnare) {
                p.startNewSubPath(1, 2);
                p.lineTo(8, 6);
            }
            break;
        case IconHarp:
            p.startNewSubPath(4, 18);
            p.lineTo(4, 2);
            p.quadraticTo(12, 4, 16, 18);
            p.closeSubPath();
            for (int i = 0; i < 3; ++i) {
                p.startNewSubPath(6.5f + float(i) * 2.5f, 4.0f + float(i) * 1.5f);
                p.lineTo(6.5f + float(i) * 2.5f, 17);
            }
            break;
        case IconChoir:   // two singing mouths
            p.addEllipse(2, 4, 8, 10);
            p.addEllipse(10, 6, 8, 10);
            p.addEllipse(4.5f, 10, 3, 2.5f);
            p.addEllipse(12.5f, 12, 3, 2.5f);
            break;
        case IconLute:
            p.addEllipse(2, 8, 10, 10);
            p.startNewSubPath(10, 10);
            p.lineTo(18, 2);
            p.addEllipse(5.5f, 11.5f, 3, 3);
            break;
        case IconMarimba:
            for (int i = 0; i < 4; ++i) rect(p, 2.0f + float(i) * 4.5f, 3.0f + float(i) * 1.5f, 3, 14.0f - float(i) * 3.0f);
            break;
        case IconFlute:
            rect(p, 1, 9, 18, 2.5f);
            for (int i = 0; i < 4; ++i) p.addEllipse(5.0f + float(i) * 3.2f, 9.5f, 1.4f, 1.4f);
            break;
        case IconJingle:
            for (int i = 0; i < 3; ++i) p.addEllipse(2.0f + float(i) * 5.5f, 8.0f + float(i % 2) * 4.0f, 5, 5);
            p.startNewSubPath(2, 5);
            p.quadraticTo(10, 1, 18, 5);
            break;
        case IconPulse:
            p.startNewSubPath(1, 15);
            for (int i = 0; i < 4; ++i) {
                p.lineTo(1.0f + float(i) * 4.5f, 5);
                p.lineTo(3.0f + float(i) * 4.5f, 5);
                p.lineTo(3.0f + float(i) * 4.5f, 15);
                p.lineTo(5.5f + float(i) * 4.5f, 15);
            }
            break;
        case IconPad:
        case IconWave:
            for (int k = 0; k < 2; ++k) {
                p.startNewSubPath(1, 8.0f + float(k) * 5.0f);
                p.quadraticTo(5.5f, 4.0f + float(k) * 5.0f, 10, 8.0f + float(k) * 5.0f);
                p.quadraticTo(14.5f, 12.0f + float(k) * 5.0f, 19, 8.0f + float(k) * 5.0f);
            }
            break;
        case IconRiser:
            p.startNewSubPath(1, 18);
            p.quadraticTo(12, 17, 18, 2);
            triangle(p, 15, 3, 19, 1, 18, 6);
            break;
        case IconEngine:
        case IconGear:
            p.addEllipse(3, 3, 14, 14);
            p.addEllipse(7.5f, 7.5f, 5, 5);
            for (int i = 0; i < 8; ++i) {
                const float a = float(i) * juce::MathConstants<float>::pi / 4.0f;
                p.startNewSubPath(10 + 7 * std::cos(a), 10 + 7 * std::sin(a));
                p.lineTo(10 + 9.5f * std::cos(a), 10 + 9.5f * std::sin(a));
            }
            break;
        case IconBlip:
        case IconRadio:
            for (int i = 0; i < 4; ++i) rect(p, 2.0f + float(i) * 4.5f, float((i * 7) % 11) + 3.0f, 2.5f, 2.5f);
            if (icon == IconRadio) {
                p.startNewSubPath(2, 18);
                p.lineTo(18, 18);
            }
            break;
        case IconAnvil:
            p.startNewSubPath(2, 6);
            p.lineTo(18, 6);
            p.lineTo(15, 10);
            p.lineTo(12, 10);
            p.lineTo(13, 16);
            p.lineTo(7, 16);
            p.lineTo(8, 10);
            p.quadraticTo(3, 10, 2, 6);
            p.closeSubPath();
            break;
        case IconPiston:
            rect(p, 6, 2, 8, 8);
            rect(p, 9, 10, 2, 8);
            break;
        case IconSteam:
        case IconWind:
            for (int k = 0; k < 3; ++k) {
                p.startNewSubPath(1, 5.0f + float(k) * 5.0f);
                p.quadraticTo(12, 3.0f + float(k) * 5.0f, 15, 5.0f + float(k) * 5.0f);
                p.quadraticTo(19, 8.0f + float(k) * 5.0f, 15, 9.0f + float(k) * 5.0f);
            }
            break;
        case IconPiano:
            rect(p, 1, 4, 18, 12);
            for (int i = 1; i < 6; ++i) {
                p.startNewSubPath(1.0f + float(i) * 3.0f, 4);
                p.lineTo(1.0f + float(i) * 3.0f, 16);
            }
            break;
        case IconCrowd:
        case IconSwarm:
            for (int i = 0; i < 5; ++i) p.addEllipse(1.0f + float(i) * 3.6f, 6.0f + float((i * 5) % 7), 3.4f, 3.4f);
            break;
        case IconSiren:
        case IconAlarm:
            p.addCentredArc(10, 12, 6, 6, 0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
            rect(p, 3, 12, 14, 3);
            p.startNewSubPath(2, 4);
            p.lineTo(5, 6);
            p.startNewSubPath(18, 4);
            p.lineTo(15, 6);
            break;
        case IconBird:
            p.startNewSubPath(1, 9);
            p.quadraticTo(5, 4, 10, 10);
            p.quadraticTo(15, 4, 19, 9);
            break;
        case IconGong:
        case IconBowl:
            if (icon == IconGong) {
                p.addEllipse(3, 2, 14, 14);
                p.addEllipse(8, 7, 4, 4);
            } else {
                p.startNewSubPath(2, 7);
                p.quadraticTo(10, 22, 18, 7);
                p.closeSubPath();
            }
            break;
        case IconDrip:
            p.startNewSubPath(10, 2);
            p.quadraticTo(17, 12, 10, 17);
            p.quadraticTo(3, 12, 10, 2);
            break;
        case IconCrystal:
            p.startNewSubPath(10, 1);
            p.lineTo(16, 7);
            p.lineTo(10, 19);
            p.lineTo(4, 7);
            p.closeSubPath();
            p.startNewSubPath(4, 7);
            p.lineTo(16, 7);
            break;
        case IconMusicBox:
            rect(p, 2, 8, 16, 10);
            p.startNewSubPath(2, 8);
            p.lineTo(6, 3);
            p.lineTo(20, 3);
            p.lineTo(18, 8);
            break;
        case IconStrings:
            p.addEllipse(5, 6, 10, 12);
            p.startNewSubPath(10, 6);
            p.lineTo(10, 1);
            p.startNewSubPath(1, 18);
            p.lineTo(19, 4);
            break;
        case IconGlass:
            p.startNewSubPath(5, 2);
            p.lineTo(15, 2);
            p.quadraticTo(15, 10, 10, 11);
            p.quadraticTo(5, 10, 5, 2);
            p.startNewSubPath(10, 11);
            p.lineTo(10, 17);
            p.startNewSubPath(6, 18);
            p.lineTo(14, 18);
            break;
        case IconShaker:
            p.addEllipse(2, 4, 10, 9);
            p.startNewSubPath(11, 11);
            p.lineTo(18, 18);
            break;
        case IconBass:
            p.addEllipse(2, 9, 9, 9);
            p.startNewSubPath(10, 12);
            p.lineTo(18, 2);
            p.startNewSubPath(13, 16);
            p.quadraticTo(17, 12, 15, 8);
            break;
        default: p.addEllipse(5, 5, 10, 10); break;
    }
    return p;
}

// Hand-made artwork (Direction Update 2): assets/places/<key>/{back,mid,front,lights}.png,
// bundled at build time. Missing layers fall back to the procedural art.
juce::Image layerImage(int type, const char* layer) {
#if WB_HAS_PLACE_ART
    const auto name = juce::String(placeKeys[std::size_t(std::clamp(type, 0, PlaceTypeCount - 1))]) + "_" + layer + "_png";
    int size = 0;
    if (const char* data = WorldbuilderArt::getNamedResource(name.toRawUTF8(), size); data != nullptr && size > 0)
        return juce::ImageCache::getFromMemory(data, size);
#else
    juce::ignoreUnused(type, layer);
#endif
    return {};
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
        case Spaceship: shape = spaceship(); detail = spaceshipDetail(); break;
        case Biodome: shape = biodome(); detail = biodomeDetail(); break;
        case SeaShip: shape = seaship(); detail = seashipDetail(); break;
        case Suburbs: shape = suburbs(); detail = suburbsDetail(); break;
        case Mall: shape = mall(); detail = mallDetail(); break;
        case Military: shape = military(); detail = militaryDetail(); break;
        case Beach: shape = beach(); detail = beachDetail(); break;
        case Desert: shape = desert(); detail = desertDetail(); break;
        case Palace: shape = palace(); detail = palaceDetail(); break;
        case Tavern: shape = tavern(); detail = tavernDetail(); break;
        case SpaceStation: shape = station(); detail = stationDetail(); break;
        case Megacity: shape = megacity(); detail = megacityDetail(); break;
        case Bridge: shape = bridge(); detail = bridgeDetail(); break;
        case Mining: shape = mining(); detail = miningDetail(); break;
        case AlienRuins: shape = alienRuins(); detail = alienRuinsDetail(); break;
        case Nebula: shape = nebula(); detail = nebulaDetail(); break;
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

namespace wb::ui {

juce::Colour placeAccent(int type) {
    static constexpr std::array<uint32_t, PlaceTypeCount> table { 0xff6c7a8f, 0xffc9a45c, 0xffd08a4e, 0xff5fd4e6, 0xffb36a4a, 0xff8fa6c9, 0xff6fb37a,
                                                                  0xffb78cff, 0xffd8c27a, 0xff7a8fb3, 0xff9a8f86, 0xff7fc8f0, 0xff7ad8a8, 0xff5f9ad8,
                                                                  0xffe0a87a, 0xffe07ab8, 0xffc0584a, 0xff6fd0c8, 0xffe0b060, 0xfff0d890, 0xffd8843e,
                                                                  0xffa8c8e8, 0xffff4fa8, 0xff4f9fff, 0xffd09a50, 0xff5fe8b0, 0xffc890ff };
    return juce::Colour(table[std::size_t(std::clamp(type, 0, PlaceTypeCount - 1))]);
}

const PlaceTheme& placeTheme(int type) {
    const auto t = [](uint32_t sky, uint32_t horizon, uint32_t ground, uint32_t glow, uint32_t tint, uint32_t backdrop, int particles, int globe) {
        return PlaceTheme { juce::Colour(sky), juce::Colour(horizon), juce::Colour(ground), juce::Colour(glow), juce::Colour(tint), juce::Colour(backdrop),
                            particles, globe };
    };
    static const std::array<PlaceTheme, PlaceTypeCount> table {
        t(0xff101828, 0xff26364d, 0xff152031, 0xff5fd4e6, 0xff152031, 0xff0d1420, ParticleMotes, GlobeFields),          // empty
        t(0xff2a2340, 0xffb07a4a, 0xff2b2a33, 0xffe0b060, 0xff34243a, 0xff2a1838, ParticleMotes, GlobeFields),          // castle: torchlit stone at dusk
        t(0xff3a2a2a, 0xffd89a52, 0xff3d3424, 0xffffb35c, 0xff3a281a, 0xff2e1a10, ParticleEmbers, GlobeFields),         // village: warm amber
        t(0xff071426, 0xff1f6f8f, 0xff10202e, 0xff5fe0ff, 0xff06303f, 0xff021e2e, ParticleSparks, GlobeOrbit),          // spaceport: cold cyan
        t(0xff201a18, 0xffa0502a, 0xff2a2220, 0xffff8a3c, 0xff3a2212, 0xff28140a, ParticleEmbers, GlobeSmoke),          // factory: furnace orange
        t(0xff100c24, 0xff6a3a8a, 0xff171630, 0xffff5fb0, 0xff241842, 0xff160c30, ParticleSparks, GlobeCityLights),     // city: neon night
        t(0xff0c1c14, 0xff3f7a4a, 0xff12261a, 0xffc8ff8a, 0xff12301c, 0xff081e10, ParticleFireflies, GlobeForest),      // forest: deep green
        t(0xff1a0c24, 0xff7a3aa0, 0xff220f2a, 0xffd07aff, 0xff2e1238, 0xff1e0828, ParticleSpores, GlobeHive),           // alien hive
        t(0xff1f1830, 0xffd8b070, 0xff2a2230, 0xffffe0a0, 0xff342c1c, 0xff241c10, ParticleMotes, GlobeGlow),            // temple: gold light
        t(0xff060c14, 0xff1d4a5a, 0xff0b141c, 0xff7ae8e0, 0xff0a2830, 0xff04161c, ParticleDust, GlobeCrystals),         // cavern
        t(0xff1a1c22, 0xff6f7a6a, 0xff22261f, 0xffb0c8a0, 0xff262c22, 0xff161a14, ParticleDust, GlobeRuins),            // ruins
        t(0xff050a14, 0xff1c3c5c, 0xff0c1624, 0xff8ad8ff, 0xff0a2240, 0xff04122a, ParticleSparks, GlobeOrbit),          // spaceship
        t(0xff0c2026, 0xff4aa88a, 0xff123026, 0xffb0ffd0, 0xff0e3229, 0xff06201a, ParticleSpores, GlobeForest),         // biodome
        t(0xff0c1a2c, 0xff3f7aa0, 0xff102838, 0xffbfe6ff, 0xff0c263e, 0xff06182c, ParticleMist, GlobeOcean),            // sea ship
        t(0xff22283a, 0xffe0a07a, 0xff26302a, 0xffffd0a0, 0xff2e2c3a, 0xff1e1c2c, ParticleMotes, GlobeCityLights),      // suburbs
        t(0xff1c1430, 0xffd77ab0, 0xff241a34, 0xff7af0e0, 0xff301c3e, 0xff1e0e2e, ParticleSparks, GlobeCityLights),     // mall: pastel neon
        t(0xff1c1414, 0xff8a3a2a, 0xff241c18, 0xffff6a4a, 0xff341a14, 0xff22100c, ParticleSmoke, GlobeSmoke),           // military camp: smoky red
        t(0xff123a5a, 0xfff0c080, 0xffc8a868, 0xffffe6a0, 0xff143a4c, 0xff0a2838, ParticleMist, GlobeOcean),            // beach
        t(0xff3a2414, 0xffe8b060, 0xffb08040, 0xffffd27a, 0xff3e2c16, 0xff2c1c0c, ParticleDust, GlobeDunes),            // desert: sandy gold
        t(0xff241a34, 0xffe8c890, 0xff2c2438, 0xfffff0c0, 0xff32244a, 0xff201436, ParticleSparks, GlobeGlow),           // palace
        t(0xff2a1a12, 0xffc8783a, 0xff33241a, 0xffffa040, 0xff3e2410, 0xff2a1608, ParticleEmbers, GlobeFields),         // tavern: warm amber
        t(0xff060c18, 0xff3a5a80, 0xff101a28, 0xffa8d8ff, 0xff0e2238, 0xff050e1c, ParticleSparks, GlobeOrbit),          // space station: steel blue
        t(0xff120820, 0xffb02a80, 0xff1a1028, 0xffff3fa0, 0xff2a0c38, 0xff12061e, ParticleMist, GlobeCityLights),       // megacity: neon magenta rain
        t(0xff040814, 0xff1f4a9a, 0xff0a1428, 0xff5fb0ff, 0xff0a1e44, 0xff030a20, ParticleSparks, GlobeOrbit),          // starship bridge: command blue
        t(0xff141010, 0xff8a6030, 0xff221a14, 0xffffb040, 0xff2e2014, 0xff1a120a, ParticleDust, GlobeCrystals),         // mining colony: drill-light amber
        t(0xff081414, 0xff2a8a70, 0xff10201c, 0xff5fffc0, 0xff0c2e28, 0xff061a16, ParticleSpores, GlobeRuins),          // alien ruins: glyph green
        t(0xff0c0618, 0xff6a3aa0, 0xff140c24, 0xffe0a0ff, 0xff241040, 0xff0e0620, ParticleMotes, GlobeGlow),            // nebula: violet gas
    };
    return table[std::size_t(std::clamp(type, 0, PlaceTypeCount - 1))];
}

void drawInstrumentIcon(juce::Graphics& g, int icon, juce::Rectangle<float> area, juce::Colour ink) {
    auto p = glyph(icon);
    const float side = std::min(area.getWidth(), area.getHeight());
    const auto box = area.withSizeKeepingCentre(side, side);
    p.applyTransform(juce::AffineTransform::scale(side / 20.0f).translated(box.getX(), box.getY()));
    g.setColour(ink);
    g.strokePath(p, juce::PathStrokeType(std::max(1.0f, side / 14.0f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Image artworkLayer(int type, const char* layer) { return layerImage(type, layer); }
bool hasArtwork(int type) { return layerImage(type, "front").isValid() || layerImage(type, "back").isValid(); }

void drawParticles(juce::Graphics& g, int kind, juce::Rectangle<float> area, float time, float amount, juce::Colour colour, int seed) {
    if (amount <= 0.01f || area.isEmpty()) return;
    juce::Random random(seed * 977 + kind);
    const int count = int(std::lround(amount * (kind == ParticleMist || kind == ParticleSmoke ? 7.0f : 22.0f)));
    for (int i = 0; i < count; ++i) {
        const float x0 = random.nextFloat(), y0 = random.nextFloat(), speed = 0.5f + random.nextFloat(), phase = random.nextFloat() * 6.28f;
        float x = x0, y = y0, size = 2.0f, alpha = 0.5f;
        switch (kind) {
            case ParticleEmbers:   // rising, flickering
                y = 1.0f - std::fmod(y0 + time * 0.08f * speed, 1.0f);
                x = x0 + 0.03f * std::sin(time * 1.3f + phase);
                size = 1.6f + speed;
                alpha = 0.35f + 0.45f * (0.5f + 0.5f * std::sin(time * 9.0f * speed + phase)) * y;
                break;
            case ParticleSparks:   // twinkling points
                size = 1.2f + speed;
                alpha = 0.6f * std::pow(0.5f + 0.5f * std::sin(time * 2.2f * speed + phase), 3.0f);
                break;
            case ParticleDust:   // drifting sideways
                x = std::fmod(x0 + time * 0.03f * speed, 1.0f);
                y = y0 + 0.02f * std::sin(time + phase);
                size = 1.4f + 0.8f * speed;
                alpha = 0.3f;
                break;
            case ParticleFireflies:   // wandering, glowing on and off
                x = x0 + 0.05f * std::sin(time * 0.5f * speed + phase);
                y = y0 + 0.05f * std::cos(time * 0.4f * speed + phase * 1.3f);
                size = 2.4f;
                alpha = 0.7f * std::max(0.0f, std::sin(time * 1.1f * speed + phase));
                break;
            case ParticleSmoke:   // big soft plumes rising
                y = 1.0f - std::fmod(y0 + time * 0.02f * speed, 1.0f);
                x = x0 + 0.08f * std::sin(time * 0.3f + phase);
                size = 18.0f + 30.0f * (1.0f - y);
                alpha = 0.12f * y;
                break;
            case ParticleSpores:   // slow floating motes, pulsing
                y = 1.0f - std::fmod(y0 + time * 0.015f * speed, 1.0f);
                x = x0 + 0.04f * std::sin(time * 0.7f + phase);
                size = 2.0f + 1.5f * (0.5f + 0.5f * std::sin(time * 2.0f + phase));
                alpha = 0.45f;
                break;
            case ParticleMist:   // low drifting banks
                x = std::fmod(x0 + time * 0.012f * speed, 1.3f) - 0.15f;
                y = 0.55f + 0.4f * y0;
                size = 40.0f + 40.0f * speed;
                alpha = 0.08f;
                break;
            default:   // motes floating in a beam of light
                x = x0 + 0.02f * std::sin(time * 0.6f * speed + phase);
                y = y0 + 0.03f * std::cos(time * 0.5f * speed + phase);
                size = 1.3f + 0.6f * speed;
                alpha = 0.25f + 0.25f * std::sin(time * 1.5f + phase);
                break;
        }
        const juce::Point<float> p { area.getX() + x * area.getWidth(), area.getY() + y * area.getHeight() };
        const auto c = colour.withAlpha(juce::jlimit(0.0f, 1.0f, alpha * std::min(1.0f, amount * 1.5f)));
        if (size > 6.0f) {
            g.setGradientFill(juce::ColourGradient(c, p.x, p.y, c.withAlpha(0.0f), p.x + size * 0.5f, p.y, true));
            g.fillEllipse(juce::Rectangle<float>(size, size * 0.7f).withCentre(p));
        } else {
            g.setColour(c);
            g.fillEllipse(juce::Rectangle<float>(size, size).withCentre(p));
        }
    }
}

namespace {
// A silhouette of rolling ground across `area`, at `height` (0..1 from the bottom).
juce::Path hills(juce::Rectangle<float> area, float height, float roughness, int seed, float shift) {
    juce::Path p;
    juce::Random random(seed);
    const float a = random.nextFloat() * 6.28f, b = random.nextFloat() * 6.28f;
    p.startNewSubPath(area.getX() - 10.0f, area.getBottom());
    for (int i = 0; i <= 24; ++i) {
        const float u = float(i) / 24.0f;
        const float x = area.getX() - 10.0f + u * (area.getWidth() + 20.0f);
        const float y = area.getBottom() - area.getHeight() * (height + roughness * (0.5f * std::sin(u * 7.0f + a + shift) + 0.3f * std::sin(u * 13.0f + b - shift)));
        p.lineTo(x, y);
    }
    p.lineTo(area.getRight() + 10.0f, area.getBottom());
    p.closeSubPath();
    return p;
}

void drawLayer(juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> area, float dx, float alpha = 1.0f) {
    if (!image.isValid()) return;
    g.setOpacity(alpha);
    g.drawImage(image, area.translated(dx, 0).expanded(4.0f, 0), juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
    g.setOpacity(1.0f);
}
} // namespace

void drawPlaceScene(juce::Graphics& g, int type, juce::Rectangle<float> area, float hover, float time, bool enabled) {
    const auto& theme = placeTheme(type);
    juce::Graphics::ScopedSaveState state(g);
    juce::Path clip;
    clip.addRoundedRectangle(area, 6.0f);
    g.reduceClipRegion(clip);
    const float sway = hover * std::sin(time * 0.9f);
    const auto back = layerImage(type, "back"), mid = layerImage(type, "mid"), front = layerImage(type, "front"), lights = layerImage(type, "lights");

    // Sky (or the artwork's back layer), and the Place's light low in the sky.
    if (back.isValid()) {
        drawLayer(g, back, area, sway * 1.0f);
    } else {
        g.setGradientFill(juce::ColourGradient(theme.sky, area.getX(), area.getY(), theme.horizon.withMultipliedSaturation(0.8f), area.getX(),
                                               area.getBottom() - area.getHeight() * 0.2f, false));
        g.fillRect(area);
    }
    const juce::Point<float> light { area.getX() + area.getWidth() * 0.72f + sway * 2.0f, area.getY() + area.getHeight() * 0.38f };
    const float pulse = 0.85f + 0.15f * std::sin(time * 2.0f);
    const auto glow = theme.glow.withAlpha((0.22f + 0.25f * hover * pulse) * (back.isValid() ? 0.5f : 1.0f));
    g.setGradientFill(juce::ColourGradient(glow, light.x, light.y, glow.withAlpha(0.0f), light.x + area.getWidth() * 0.45f, light.y, true));
    g.fillRect(area);

    // Ground in depth (or the mid layer): far hills drift less than near ones.
    if (mid.isValid()) {
        drawLayer(g, mid, area, sway * 2.5f);
    } else {
        g.setColour(theme.ground.interpolatedWith(theme.horizon, 0.35f).withAlpha(0.9f));
        g.fillPath(hills(area, 0.28f, 0.06f, type * 7 + 1, sway * 0.05f));
        g.setColour(theme.ground.darker(0.3f));
        g.fillPath(hills(area, 0.14f, 0.05f, type * 7 + 2, sway * 0.12f));
    }

    // The landmark (or the front layer), lifted off the ground by a soft shadow.
    const auto iconArea = area.reduced(area.getWidth() * 0.16f, area.getHeight() * 0.1f).translated(sway * 4.0f, -area.getHeight() * 0.04f - 2.0f * hover);
    if (front.isValid()) {
        drawLayer(g, front, area, sway * 4.0f);
    } else {
        drawPlaceIcon(g, type, iconArea.translated(2.0f, 3.0f), juce::Colours::black.withAlpha(0.0f), juce::Colours::black.withAlpha(0.35f));
        drawPlaceIcon(g, type, iconArea, juce::Colour(0xffe8dcc0).withAlpha(0.95f), theme.glow.interpolatedWith(theme.ground, 0.45f).withAlpha(0.85f));
    }
    // Lights: the artwork's light layer flickers; procedural art gets a few warm windows.
    if (lights.isValid()) {
        drawLayer(g, lights, area, sway * 4.0f, 0.6f + 0.4f * (0.5f + 0.5f * std::sin(time * 7.0f) * std::sin(time * 3.1f)));
    } else {
        juce::Random r(type * 31);
        for (int i = 0; i < 4; ++i) {
            const juce::Point<float> p { iconArea.getX() + iconArea.getWidth() * (0.3f + 0.4f * r.nextFloat()),
                                         iconArea.getY() + iconArea.getHeight() * (0.55f + 0.25f * r.nextFloat()) };
            const float flicker = 0.5f + 0.5f * std::sin(time * (4.0f + 3.0f * float(i)) + float(i));
            const auto c = theme.glow.withAlpha((0.25f + 0.55f * hover * flicker));
            g.setGradientFill(juce::ColourGradient(c, p.x, p.y, c.withAlpha(0.0f), p.x + 5.0f + 3.0f * hover, p.y, true));
            g.fillEllipse(juce::Rectangle<float>(12.0f, 12.0f).withCentre(p));
        }
    }
    drawParticles(g, theme.particles, area, time, 0.25f + 0.75f * hover, theme.glow, type);

    // A dark foot for the name, and a dim veil on Places that are still to come.
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, area.getX(), area.getBottom() - area.getHeight() * 0.35f,
                                           juce::Colours::black.withAlpha(0.6f), area.getX(), area.getBottom(), false));
    g.fillRect(area);
    if (!enabled) {
        g.setColour(juce::Colour(0xff0d1420).withAlpha(0.55f));
        g.fillRect(area);
    }
}

} // namespace wb::ui
