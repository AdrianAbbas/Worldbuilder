#pragma once
// The Living Globe's planet (Milestone 10): a textured, lit sphere drawn from cached
// layers. Each planet type has its own surface map (made once, procedurally, the first
// time it is shown), a cloud map, city lights for the night side and glowing cracks
// where the ground is hot. A frame only turns the maps under a per-pixel table of the
// sphere (latitude, longitude, light) that is made once per size, so a 290 px globe
// costs one pass over its pixels: no paths, no gradients.
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>
#include <vector>

namespace wb::ui {

// What a frame of the planet shows besides its own surface.
struct GlobeLook {
    float spin = 0;            // the surface's turn (radians)
    float time = 0;            // seconds, for the clouds' own drift
    float clouds = 0.5f;       // cloud cover 0..1 (Weather adds to the planet's own)
    float lights = 0;          // city lights on the night side 0..1 (populated Places)
    float heat = 0;            // extra glow in the cracks (Temperature)
    float cold = 0, warm = 0;  // colour grading from Temperature (0..1 each)
    float danger = 0;          // a darker, redder grade
    float alpha = 1;           // Power
    juce::Colour accent { 0xff5fd4e6 };   // the Places' light, mixed into the atmosphere
};

class GlobeRenderer {
public:
    static constexpr int planetCount = 7;   // dsp::PlanetType
    // Each planet's atmosphere colour and whether it has rings.
    static juce::Colour atmosphere(int planet);
    static bool hasRings(int planet);

    // Draws planet `planet` in `area` (a square). `previous` and `blend` crossfade from
    // the planet shown before (blend 1: only `planet`).
    void draw(juce::Graphics& g, juce::Rectangle<float> area, int planet, int previous, float blend, const GlobeLook& look);

    // For the tests: the surface map of a planet (made on first use) and the last frame.
    const juce::Image& lastFrame() const { return frame; }
    // The sky-facing tilt of the planet's axis (radians), for the rings.
    static constexpr float tilt = -0.32f;

private:
    struct Maps {
        static constexpr int width = 512, height = 256;
        std::vector<juce::uint32> surface;   // RGB
        std::vector<juce::uint8> cloud;      // cloud density 0..255
        std::vector<juce::uint8> lights;     // city lights 0..255
        std::vector<juce::uint8> glow;       // emissive cracks 0..255
        std::vector<juce::uint8> shine;      // specular (water, ice) 0..255
        float cloudCover = 0;                // the planet's own cloud cover
        juce::Colour cloudColour { 0xfff4f6fa };
    };
    struct Pixel {
        int offset;      // position in the frame
        float u;         // longitude in map columns, before the spin
        int row;         // latitude row in the maps
        float light;     // 0..1 lit, after the terminator's softness
        float rim;       // 0..1 towards the limb
        float spec;      // specular highlight (Blinn) for a shiny surface
        float edge;      // anti-aliased coverage at the limb
    };
    const Maps& maps(int planet);
    void buildTable(int diameter);
    void renderPlanet(int planet, const GlobeLook& look, float weight, bool first);
    void drawRings(juce::Graphics& g, juce::Rectangle<float> area, int planet, float alpha, bool front) const;

    std::array<std::unique_ptr<Maps>, planetCount> cache;
    std::vector<Pixel> table;
    int tableSize = 0;
    juce::Image frame;
    std::vector<float> accum;   // RGBA, for the crossfade
};

} // namespace wb::ui
