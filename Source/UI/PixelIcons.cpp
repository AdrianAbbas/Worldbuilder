#include "Pixel.h"
#include "PlaceArt.h"
#include "Style.h"
#include "../Places/Place.h"

// The pixel tool and instrument icons. Each is made from the plugin's own vector icon
// (Style.cpp, PlaceArt.cpp): drawn four times larger, reduced to one bit per art pixel
// (a pixel is ink when at least 40 % covered), then given a one-pixel shadow. One
// consistent set, made once per size and colour. A bundled PNG of the same name
// (white on transparent) replaces any of them.

namespace wb::ui::pixel {

namespace {
constexpr const char* toolNames[] { "none", "power", "dice", "edit", "close", "patterns", "save", "load", "render", "midi", "dragout",
                                    "path", "journey", "undo", "redo", "copy", "paste", "clear", "generate", "up", "down", "lock",
                                    "draw", "select", "back", "snap", "note", "sync", "import", "double", "half", "chords", "tone12",
                                    "play", "stop", "loop", "record" };
static_assert(std::size(toolNames) == std::size_t(Icon::Count), "a file name for every tool icon");
constexpr const char* instrumentNames[] { "drone", "bell", "horn", "timpani", "harp", "choir", "lute", "marimba", "flute", "jingle",
                                          "pulse", "pad", "riser", "pa", "engine", "blip", "handdrum", "anvil", "piston", "steam",
                                          "gear", "piano", "crowd", "siren", "bird", "wave", "gong", "bowl", "drip", "crystal",
                                          "wind", "musicbox", "alarm", "strings", "glass", "shaker", "snare", "radio", "bass", "swarm" };
static_assert(std::size(instrumentNames) == std::size_t(InstrumentIconCount), "a file name for every instrument icon");

// One bit per art pixel from a vector drawing, then ink plus shadow.
Canvas rasterise(int size, juce::Colour colour, juce::Colour shadow, const std::function<void(juce::Graphics&, juce::Rectangle<float>)>& paint,
                 const juce::String& bundled) {
    Canvas k(size + 1, size + 1);
    juce::Image mask;
    if (const auto art = bundledSprite(bundled); art.isValid()) {
        mask = art.rescaled(size, size, juce::Graphics::lowResamplingQuality);
    } else {
        constexpr int over = 4;
        juce::Image big(juce::Image::ARGB, size * over, size * over, true);
        {
            juce::Graphics g(big);
            paint(g, juce::Rectangle<float>(float(size * over), float(size * over)).reduced(float(over) * 0.5f));
        }
        mask = juce::Image(juce::Image::ARGB, size, size, true);
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x) {
                float cover = 0;
                for (int j = 0; j < over; ++j)
                    for (int i = 0; i < over; ++i) cover += big.getPixelAt(x * over + i, y * over + j).getFloatAlpha();
                if (cover / float(over * over) >= 0.4f) mask.setPixelAt(x, y, juce::Colours::white);
            }
    }
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            if (mask.getPixelAt(x, y).getAlpha() > 127) {
                if (shadow.getAlpha() > 0 && k.get(x + 1, y + 1).getAlpha() == 0) k.set(x + 1, y + 1, shadow);
            }
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x)
            if (mask.getPixelAt(x, y).getAlpha() > 127) k.set(x, y, colour);
    return k;
}
} // namespace

juce::String toolIconName(int icon) { return juce::isPositiveAndBelow(icon, int(Icon::Count)) ? juce::String("icon-") + toolNames[icon] : juce::String(); }
juce::String instrumentIconName(int icon) {
    return juce::isPositiveAndBelow(icon, int(InstrumentIconCount)) ? juce::String("instrument-") + instrumentNames[icon] : juce::String();
}

Canvas toolIcon(int icon, int size, juce::Colour colour, juce::Colour shadow) {
    if (icon <= int(Icon::None) || icon >= int(Icon::Count)) return Canvas(size + 1, size + 1);
    return rasterise(size, colour, shadow, [icon](juce::Graphics& g, juce::Rectangle<float> area) { drawIcon(g, Icon(icon), area, juce::Colours::white); },
                     toolIconName(icon));
}

Canvas instrumentIcon(int icon, int size, juce::Colour colour, juce::Colour shadow) {
    if (!juce::isPositiveAndBelow(icon, int(InstrumentIconCount))) return Canvas(size + 1, size + 1);
    return rasterise(size, colour, shadow, [icon](juce::Graphics& g, juce::Rectangle<float> area) { drawInstrumentIcon(g, icon, area, juce::Colours::white); },
                     instrumentIconName(icon));
}

} // namespace wb::ui::pixel
