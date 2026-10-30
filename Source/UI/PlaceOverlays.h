#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Places/Place.h"

namespace wb::ui {

// How much the Place overlays decorate the interface (saved with the project).
enum OverlayMode { OverlaysFull, OverlaysSubtle, OverlaysOff, OverlayModeCount };
inline constexpr std::array<const char*, OverlayModeCount> overlayModeNames { "Full", "Subtle", "Off" };
// Overlays are off until the composer turns them on (they cost CPU in the host).
inline constexpr int defaultOverlayMode = OverlaysOff;

// True when a Place has its own overlay. Every built Place must have one.
bool hasOverlay(int type);

// Place overlays: each active Place decorates the interface with its own animated
// overlay along the panel frames and in the empty space between them (Castle: grass,
// ivy, banners, fireflies; Spaceport: neon edges, scan lines, hologram flicker;
// Village: wooden beams, lanterns, chimney smoke). Several Places combine, each at its
// own amount (its level, fading in and out with the Place). Overlays never draw on
// the areas kept clear (controls, text, meters) and never take the mouse.
//
// The still parts are drawn once per layout into an image at the display's pixel scale,
// already clipped to the decorated area (released when the Place is gone); a frame
// draws that image and the moving parts, and only the places where something moved
// are repainted. Each overlay keeps its own frame clock: Castle, Village and Spaceport
// move at 20 fps (10 at Subtle); every later overlay moves only small pre-rendered
// sprites, at 15 fps (10 at Subtle), and repaints only on its own frames.
class PlaceOverlays final : public juce::Component {
public:
    PlaceOverlays();

    // The panel frames to decorate, open space they may also use (empty parts of the
    // header), and everything never to draw on.
    void setFrames(std::vector<juce::Rectangle<int>> panels, std::vector<juce::Rectangle<int>> openSpace, const juce::RectangleList<int>& keepClear);
    void setAmount(int type, float amount);
    void setMode(int mode);
    int mode() const { return overlayMode; }
    // Moves the animation on and repaints what moved: for each overlay on its own
    // frames (see framesPerSecond; called 30 times a second), where its moving parts were
    // and are now, merged into a few rectangles. While an overlay's strength changes,
    // everything repaints.
    void advance(float seconds);
    // How strongly a Place's overlay shows: its amount, scaled by the mode.
    float strength(int type) const;
    // How often a Place's overlay moves, in frames a second (the editor's timer runs at 30).
    static int framesPerSecond(int type, int mode);
    bool showing() const;
    // The area overlays may draw on: the gaps between panels, open space, a narrow band
    // inside each frame and its corners, minus everything kept clear.
    const juce::RectangleList<int>& drawableArea() const { return drawable; }
    // Where the later overlays' sprites may move: the drawable area without the panels'
    // corners, so a frame never repaints inside a panel (the globe, the cards).
    const juce::RectangleList<int>& roamingArea() const { return roamingDrawable; }
    // What the last frame repainted (empty when it repainted everything, or nothing).
    const std::vector<juce::Rectangle<int>>& lastRepaint() const { return repainted; }
    // For the tests: the layout the overlays are built from, how many times overlays were
    // built (each is costly), and how many path segments a Place's still parts hold.
    struct Layout;
    const Layout& currentLayout() const;
    int buildCount() const { return builds; }
    std::size_t stillSegments(int type) const;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int frameBand = 6;
    // At most this many rectangles repainted per frame (each can be a paint call).
    static constexpr std::size_t maxRepaintRects = 12;   // how far a decoration reaches inside a frame

    // What a Place draws: its still parts once per layout, its moving parts each frame.
    struct Shape {
        juce::Path path;
        juce::Colour colour;
        float stroke = 0;   // 0: filled
    };
    struct Layout {
        juce::Rectangle<float> bounds;
        std::vector<juce::Rectangle<float>> panels, gutters, open;
        std::vector<juce::Rectangle<float>> ledges;   // the gaps between panels stacked one above the other
        float top = 0;   // the top of the highest panels
    };
    // A small pre-rendered sprite that moves (the later overlays move nothing else): it
    // pulses, blinks or flickers where it is, or rises, falls or drifts `travel` pixels
    // over its period, swaying `sway` pixels, fading in and out. Rising sprites step
    // through `frames` growing images; falling ones turn through them.
    // Spinning sprites step through their `frames` in place (gears, flames).
    enum MoverKind : uint8_t { MovePulse, MoveBlink, MoveFlicker, MoveSpin, MoveRise, MoveFall, MoveDrift };
    struct Mover {
        juce::Point<float> at;
        float travel = 0, sway = 0, period = 4, phase = 0, alpha = 1;
        uint8_t kind = MovePulse, sprite = 0, frames = 1;
    };

private:
    struct Still {
        std::vector<Shape> shapes;
        std::vector<std::vector<juce::Point<float>>> tracks;   // points along each panel's frame
        std::vector<juce::Point<float>> spots;                 // where moving parts live
        std::vector<juce::Rectangle<float>> vents;             // smoke sources: top centre, how far it rises (height)
        std::vector<Mover> movers;                             // the later overlays' moving sprites
        std::vector<juce::Image> sprite;                       // and their images, drawn once
        bool light = false;                                    // drawn in the first pass (neon, glows)
        bool built = false;
        juce::Image image;                                     // the shapes, drawn and clipped
        float imageScale = 0;
        int imageArea = -1;                                    // the drawable area it was clipped to
    };
    // Castle, Village and Spaceport: drawn shapes at 20 fps (their optimisation waits for
    // the visual polish milestone). Every later overlay moves sprites only, at 15 fps.
    static bool isLegacy(int type);
    void rebuildArea();
    void build(int type);
    // Draws a Place's moving parts (with no Graphics, only notes where they are).
    void drawMoving(juce::Graphics*, int type, const Still&, float strength, std::vector<juce::Rectangle<int>>& marks) const;
    void drawMovers(juce::Graphics*, const Still&, float strength, float t, std::vector<juce::Rectangle<int>>& marks) const;
    std::vector<juce::Rectangle<int>> movingParts(int type) const;
    bool movesOn(int type, int tick) const;
    // Glows drawn once at their exact sizes and stamped where they move, unscaled (much
    // cheaper than a gradient, or a scaled image, under the overlays' clip).
    struct Sprites {
        juce::Image firefly, lantern, cometGlow;
        std::array<juce::Image, 8> smoke, cometDot;   // growing sizes
    };
    void stamp(juce::Graphics*, const juce::Image&, juce::Point<float> centre, float alpha, std::vector<juce::Rectangle<int>>& marks) const;
    Sprites sprites;

    Layout layout;
    juce::RectangleList<int> clear, decorated, drawable, roaming, roamingDrawable;
    std::array<float, PlaceTypeCount> amounts {}, paintedStrength {};
    std::array<float, PlaceTypeCount> animTime {};   // each overlay's time at its last frame
    std::array<std::vector<juce::Rectangle<int>>, PlaceTypeCount> moved;   // where its moving parts were last painted
    std::vector<juce::Rectangle<int>> repainted;
    std::array<Still, PlaceTypeCount> still;
    std::vector<juce::Rectangle<int>> panelList, openList;
    int overlayMode = OverlaysFull, frame = 0, areaVersion = 0, builds = 0;
    float time = 0;
    bool shownLastFrame = false, fadingNow = false;
    // The still images combined at their strengths (when nothing fades).
    juce::Image combined;
    std::array<float, PlaceTypeCount> combinedStrength {};
    float combinedScale = 0;
    int combinedArea = -1;
};

} // namespace wb::ui
