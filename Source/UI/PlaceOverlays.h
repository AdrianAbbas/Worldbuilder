#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Places/Place.h"

namespace wb::ui {

// How much the Place overlays decorate the interface (saved with the project).
enum OverlayMode { OverlaysFull, OverlaysSubtle, OverlaysOff, OverlayModeCount };
inline constexpr std::array<const char*, OverlayModeCount> overlayModeNames { "Full", "Subtle", "Off" };

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
// draws that image and a few dozen moving shapes, at 20 fps (10 at Subtle), and only
// the places where something moved are repainted.
class PlaceOverlays final : public juce::Component {
public:
    PlaceOverlays();

    // The panel frames to decorate, open space they may also use (empty parts of the
    // header), and everything never to draw on.
    void setFrames(std::vector<juce::Rectangle<int>> panels, std::vector<juce::Rectangle<int>> openSpace, const juce::RectangleList<int>& keepClear);
    void setAmount(int type, float amount);
    void setMode(int mode);
    int mode() const { return overlayMode; }
    // Moves the animation on and repaints what moved: where the moving parts were and
    // are now, merged into a few rectangles (20 times a second at Full, 10 at Subtle,
    // called 30 times a second). While an overlay's strength changes, everything repaints.
    void advance(float seconds);
    // How strongly a Place's overlay shows: its amount, scaled by the mode.
    float strength(int type) const;
    bool showing() const;
    // The area overlays may draw on: the gaps between panels, open space, a narrow band
    // inside each frame and its corners, minus everything kept clear.
    const juce::RectangleList<int>& drawableArea() const { return drawable; }
    // What the last frame repainted (empty when it repainted everything, or nothing).
    const std::vector<juce::Rectangle<int>>& lastRepaint() const { return repainted; }

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int frameBand = 6;   // how far a decoration reaches inside a frame

    // What a Place draws: its still parts once per layout, its moving parts each frame.
    struct Shape {
        juce::Path path;
        juce::Colour colour;
        float stroke = 0;   // 0: filled
    };
    struct Layout {
        juce::Rectangle<float> bounds;
        std::vector<juce::Rectangle<float>> panels, gutters, open;
        float top = 0;   // the top of the highest panels
    };

private:
    struct Still {
        std::vector<Shape> shapes;
        std::vector<std::vector<juce::Point<float>>> tracks;   // points along each panel's frame
        std::vector<juce::Point<float>> spots;                 // where moving parts live
        std::vector<juce::Rectangle<float>> vents;             // smoke sources: top centre, how far it rises (height)
        bool built = false;
        juce::Image image;                                     // the shapes, drawn and clipped
        float imageScale = 0;
        int imageArea = -1;                                    // the drawable area it was clipped to
    };
    void rebuildArea();
    void build(int type);
    // Draws a Place's moving parts (with no Graphics, only notes where they are).
    void drawMoving(juce::Graphics*, int type, const Still&, float strength, std::vector<juce::Rectangle<int>>& marks) const;
    std::vector<juce::Rectangle<int>> movingParts() const;
    // Glows drawn once at their exact sizes and stamped where they move, unscaled (much
    // cheaper than a gradient, or a scaled image, under the overlays' clip).
    struct Sprites {
        juce::Image firefly, lantern, cometGlow;
        std::array<juce::Image, 8> smoke, cometDot;   // growing sizes
    };
    void stamp(juce::Graphics*, const juce::Image&, juce::Point<float> centre, float alpha, std::vector<juce::Rectangle<int>>& marks) const;
    Sprites sprites;

    Layout layout;
    juce::RectangleList<int> clear, decorated, drawable;
    std::array<float, PlaceTypeCount> amounts {}, paintedStrength {};
    std::vector<juce::Rectangle<int>> moved;       // where the moving parts were last painted
    std::vector<juce::Rectangle<int>> repainted;
    std::array<Still, PlaceTypeCount> still;
    std::vector<juce::Rectangle<int>> panelList, openList;
    int overlayMode = OverlaysFull, frame = 0, areaVersion = 0;
    float time = 0;
    bool shownLastFrame = false;
};

} // namespace wb::ui
