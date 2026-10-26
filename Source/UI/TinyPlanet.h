#pragma once
// The Living Globe as a pixel-art "tiny planet" (Milestone 13b): a large planet,
// cropped by the bottom of the panel, its curved horizon across the middle; a pixel sky
// above (stars, sun, clouds, rain, snow, lightning, an aurora with Magic) tinted from day
// to night as the planet turns. The Places stand upright on the horizon, tilted with the
// curve; they turn with the planet, shrink and fade toward the edges and go over the
// horizon. Each Planet type has its own terrain and sky (Ocean waves and islands, Desert
// dunes, Ice Giant snow and ice spikes, Lava glowing cracks, Forest trees, Rocky
// craters, Gas Giant swirling bands with rings across the sky). The Locations are stars
// and moons on an arc in the sky; a journey is a comet between them.
//
// Drawn at art resolution (one art pixel per image pixel) into a Canvas, so it shows at
// whole pixels like every other sprite. The sky is cached; the rest is cheap per frame.
#include "Pixel.h"
#include <map>

namespace wb::ui {

struct TinyPlanetScene {
    int width = 0, height = 0;   // art pixels
    int planet = 0, previous = 0;
    float blend = 1;             // the crossfade from `previous` to `planet` (dithered)
    float turn = 0;              // how far the planet has turned (radians)
    float time = 0;              // seconds, for waves, cracks, clouds, weather
    int frame = 0;               // the decoration frame (twinkles, idle animations)
    float clouds = 0, rain = 0, snow = 0, flash = 0, magic = 0, danger = 0;
    int boltSeed = 1;
    float day = 1;               // 1 day, 0 night

    struct Landmark {
        int type = 0;
        int kind = 0;            // TinyPlanet::Ground, Orbit or Space (see kindOf)
        float longitude = 0;     // where it stands on the planet (radians; it turns with it)
        float near = 1;          // 1 near (full size), 0 far (smaller, darker)
        int frame = 0;
        bool highlight = false;
    };
    std::vector<Landmark> landmarks;

    struct Marker {
        juce::Colour colour;
        bool used = false, here = false;
    };
    std::vector<Marker> markers;
    // A journey: a comet along the sky arc from marker position `from` to `to` (0..n-1,
    // fractional), `progress` 0..1.
    bool comet = false;
    float cometFrom = 0, cometTo = 0, cometProgress = 0;
};

class TinyPlanet {
public:
    // The geometry, in art pixels of a scene `width` x `height`: the planet's centre and
    // radius, where a longitude shows on the horizon (its angle from the top, after the
    // turn), and the sky arc's point for a marker position (0..count-1).
    // The planet is small (0.4 x the scene's width), so its curve is tight and the arc
    // in view (angles within visibleArc of the top) is about 43 % of its circumference.
    static constexpr float visibleArc = 1.3f;
    static juce::Point<float> centre(int width, int height);
    static float radius(int width, int height);
    static float angleOf(float longitude, float turn);
    static bool visible(float angle);
    static juce::Point<float> horizonPoint(int width, int height, float angle, float lift = 0);
    static juce::Point<float> markerPoint(int width, int height, float position, int count);

    // Where a Place belongs: standing on the ground, in orbit (ships, stations, an
    // asteroid colony: circling above the horizon, passing behind the planet), or out in
    // space (the Nebula: high in the sky, drifting slowly).
    enum Kind { Ground, Orbit, Space };
    static int kindOf(int placeType);
    // A Place's middle (art pixels) and whether it shows now; `angle` gets its tilt.
    static bool placePoint(int width, int height, int kind, float longitude, float turn, float time, juce::Point<float>& middle, float& angle);

    // Draws the scene in the palette's colours; the frame stays until the next render.
    const pixel::Canvas& render(const TinyPlanetScene& scene, const pixel::Palette& palette);
    const pixel::Canvas& lastFrame() const { return frame; }

private:
    const pixel::Canvas& sky(const TinyPlanetScene& scene, int planet, const pixel::Palette& palette);
    const pixel::Canvas& placeSprite(int type, int iconFrame, int angleStep, int scaleStep, int dimStep, bool highlight, const pixel::Palette& palette);

    pixel::Canvas frame;
    std::map<juce::String, pixel::Canvas> skies, sprites;
};

} // namespace wb::ui
