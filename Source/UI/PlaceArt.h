#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace wb::ui {

// Original vector illustrations of each Place (wb::PlaceType), drawn in `area`:
// filled with a soft `fill` and outlined in `ink`.
void drawPlaceIcon(juce::Graphics& g, int type, juce::Rectangle<float> area, juce::Colour ink, juce::Colour fill);

// Each Place's accent colour (cards, strips, landmarks, the Pattern Editor).
juce::Colour placeAccent(int type);

// A small glyph for an instrument (wb::InstrumentIcon).
void drawInstrumentIcon(juce::Graphics& g, int icon, juce::Rectangle<float> area, juce::Colour ink);

// Ambient particles that drift through a Place's air.
enum ParticleKind { ParticleMotes, ParticleEmbers, ParticleSparks, ParticleDust, ParticleFireflies, ParticleSmoke, ParticleSpores, ParticleMist };
// What a Place adds to the Living Globe's surface and atmosphere.
enum GlobeFeature { GlobeFields, GlobeOrbit, GlobeSmoke, GlobeCityLights, GlobeForest, GlobeHive, GlobeGlow, GlobeCrystals, GlobeRuins, GlobeOcean, GlobeDunes,
                    GlobeFeatureCount };

// Each Place's look: the sky and horizon of its card, its ground, the colour of its
// light, the tint it gives the interface's panels, the interface's background, its
// particles and its mark on the globe.
struct PlaceTheme {
    juce::Colour sky, horizon, ground, glow, tint, backdrop;
    int particles = ParticleMotes;
    int globe = GlobeFields;
};
const PlaceTheme& placeTheme(int type);

// A Place's illustrated scene: its own artwork layers when bundled (see
// assets/places/README.md), else procedural art: sky, light, hills in depth and the
// landmark. `hover` (0..1) brings the scene to life: parallax, lights, particles.
void drawPlaceScene(juce::Graphics& g, int type, juce::Rectangle<float> area, float hover, float time, bool enabled);
// Ambient particles of one kind in `area` (amount 0..1), animated by `time` (seconds).
void drawParticles(juce::Graphics& g, int kind, juce::Rectangle<float> area, float time, float amount, juce::Colour colour, int seed = 1);
// True when bundled artwork exists for this Place.
bool hasArtwork(int type);
// The front layer of a Place's artwork (invalid when none is bundled).
juce::Image artworkLayer(int type, const char* layer);

} // namespace wb::ui
