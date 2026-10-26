#pragma once
#include "Intensity.h"
#include "Places.h"
#include <atomic>
#include <memory>

namespace wb {

constexpr int placeSlots = 4;

// One slot's parameter values, as read from the host.
struct PlaceSlotControls {
    int type = NoPlace, character = 0;
    float levelDb = 0, distance = 0.35f, pan = 0;
    bool mute = false, solo = false, sync = true;
    float activity = 0.4f, tension = 0.3f, density = 0.45f, swing = 0.08f, tone = 0.5f, size = 0.5f, variation = 0.35f;
    uint32_t seed = 1;
    std::array<float, placeElements> element { 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f };
    std::array<bool, placeElements> elementOn { true, true, true, true, true, true };
    // Each instrument's effects.
    std::array<dsp::ElementFxControls, placeElements> fx {};
    // A journey between Locations sends the Place away (0: where the slot puts it, 1: at
    // the horizon, nearly silent) as it leaves or arrives.
    float travelFar = 0;
    // The Arrangement (Milestone 11): each instrument's gain in the current section (1
    // without an arrangement) and the pattern variant it plays (A, B, C or Fill).
    std::array<float, placeElements> arrange { 1, 1, 1, 1, 1, 1 };
    std::array<uint8_t, placeElements> variant {};
    // A stinger's ring-out (renders): what rings keeps ringing, no new notes start.
    bool ringOut = false;
};

// Hosts one Place: switches between Place engines with a short fade, folds the
// World Controls into the Place's settings, keeps the Place's beat clock (the host's
// or its own free tempo) and applies level, distance (quieter, darker, narrower,
// more reverberant) and pan.
//
// Patterns: each patterned element plays the composer's edited pattern, or else the
// one the Place's generator writes from its seed and settings (regenerated whenever
// those change). Generated patterns are reported back to the editor.
class PlaceSlot {
public:
    PlaceSlot();
    void prepare(double sampleRate, int slotIndex);
    void reset();
    // Renders n <= dsp::chunk samples (writes). Returns false, writing nothing, when the
    // slot is silent.
    bool render(const WorldFrame& world, const PlaceSlotControls& controls, const pat::SlotPatterns& patterns, bool audible, uint64_t clock,
                double hostBeat, float hostBpm, float* left, float* right, int n);
    // Where generated patterns are reported (the editor reads them).
    void setFeedback(pat::TripleBuffer<pat::Generated>* buffer) { feedback = buffer; }
    // While this flag is set (the Pattern Editor shows variants), every variant is
    // generated, not only the ones playing.
    void setVariantFlag(const std::atomic<bool>* flag) { allVariants = flag; }
    // The Place's beat at the last chunk (for the editor's playhead).
    double beat() const { return lastBeat; }
    // The pattern element e is playing now (nullptr when none), for the tests.
    const pat::Pattern* playingPattern(int e) const { return e >= 0 && e < placeElements ? playing[std::size_t(e)] : nullptr; }
    const pat::Pattern& generatedPattern(int e, int variant = 0) const {
        return generated[std::size_t(pat::Element::clampVariant(variant))][std::size_t(std::clamp(e, 0, placeElements - 1))];
    }
    // The arrangement's gain on element e now (after its entrance and exit ramps).
    float arrangeGain(int e) const { return arranged[std::size_t(std::clamp(e, 0, placeElements - 1))]; }
    // World Space send for the last rendered chunk.
    float spaceSend() const { return send; }
    // The instruments' sends to the shared reverbs and delays for the last rendered
    // chunk, before the slot's gain; sendGainL/R are that gain per sample (level, fade,
    // distance and pan).
    const dsp::SendBuses& sends() const { return fx.sends; }
    const float* sendGainL() const { return sendGain[0].data(); }
    const float* sendGainR() const { return sendGain[1].data(); }
    int playingType() const { return currentType; }
    // The Place playing in this slot (nullptr when empty), for the tests.
    const Place* place() const { return current; }

    // The settings the Place sees for a slot's controls in a world. Public for the tests.
    static PlaceSettings settingsFor(const WorldFrame& world, const PlaceSlotControls& controls);
    static float effectiveDistance(const WorldFrame& world, float distance);
    // The distance and extra gain of a Place sent `far` away by a journey.
    static float travelDistance(float distance, float far) { return distance + (1.0f - distance) * dsp::clamp01(far); }
    static float travelGain(float far) { return std::pow(1.0f - 0.92f * dsp::clamp01(far), 1.5f); }

private:
    Place* engineFor(int type);

    float rate = 48000;
    int slot = 0;
    // One engine of every built Place, made and prepared off the audio thread. Only the
    // one playing is ever reset (when it starts), so a slot's reset stays cheap.
    std::array<std::unique_ptr<Place>, PlaceTypeCount> engines;
    Place* current = nullptr;
    int currentType = NoPlace;
    float fade = 0, fadeStep = 0.001f;
    double freeBeat = 0;
    uint32_t seedKey = 0;
    bool seeded = false;
    PlaceSettings settings;
    std::array<dsp::Smoother, 8> smooth;   // activity, tension, density, swing, tone, size, variation, fxDepth
    std::array<dsp::Smoother, placeElements> elementSmooth;
    dsp::Smoother gain, distance, pan;
    std::array<dsp::OnePole, 4> distanceLp;
    std::array<dsp::OnePole, 2> dcBlock;
    std::array<dsp::OnePole, 2> fullness;   // a gentle low shelf: weight in the low mids and bass
    float send = 0;
    bool fresh = true;
    dsp::InstrumentFx fx;
    // The Place's Tone on what its instruments send (the Place's own tone filters only
    // reach its dry sound): a tilt per shared bus.
    std::array<std::array<dsp::OnePole, 2>, ReverbTypeCount + DelayTypeCount> sendTilt;
    void tiltSends(float tone, float far, int n);
    std::array<std::array<float, dsp::chunk>, 2> sendGain {};

    void updatePatterns(const PlaceSettings& target, int mood, const pat::SlotPatterns& patterns, const std::array<uint8_t, placeElements>& variants);
    // Generated patterns per variant (A, B, C, Fill) and element.
    using VariantPatterns = std::array<std::array<pat::Pattern, placeElements>, pat::variantCount>;
    VariantPatterns generated {};
    std::array<std::array<GenInputs, placeElements>, pat::variantCount> generatedFor {};
    std::array<std::array<bool, placeElements>, pat::variantCount> generatedValid {};
    void generateVariant(int variant, int e, GenInputs in);
    // The arrangement's gain per element: an entrance from silence starts at once, a
    // re-entrance while ringing rises over 20 ms, an exit fades over 250 ms.
    std::array<float, placeElements> arranged {};
    float arrangeUp = 0.001f, arrangeDown = 0.0001f;
    std::array<const pat::Pattern*, placeElements> playing {};
    std::array<pat::Controls, placeElements> controlsNow {};
    pat::TripleBuffer<pat::Generated>* feedback = nullptr;
    const std::atomic<bool>* allVariants = nullptr;
    uint32_t feedbackSerial = 0;
    double lastBeat = 0;
};

} // namespace wb
