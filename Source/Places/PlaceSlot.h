#pragma once
#include "Intensity.h"
#include "Drama.h"
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
    // The six instruments of the Place, then the three Drama instruments (elements 6-8).
    std::array<float, slotElements> element { 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f };
    std::array<bool, slotElements> elementOn { true, true, true, true, true, true, true, true, true };
    // Each instrument's effects, and its Scoop / Glide.
    std::array<dsp::ElementFxControls, slotElements> fx {};
    std::array<Slide, slotElements> slide {};
    // A journey between Locations sends the Place away (0: where the slot puts it, 1: at
    // the horizon, nearly silent) as it leaves or arrives.
    float travelFar = 0;
    // The Arrangement (Milestone 11): each instrument's gain in the current section (1
    // without an arrangement) and the pattern variant it plays (A, B, C or Fill).
    std::array<float, slotElements> arrange { 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    std::array<uint8_t, slotElements> variant {};
    // The Drama tier rework: a part doubled an octave up (+1) or down (-1) as the drama
    // rises (0: as written).
    std::array<int8_t, slotElements> doubling {};
    // The Drama loudness pass: the arrangement's reshaping as drama plays it (instruments
    // entering, busier, doubled; or thinned out at Calm), balanced back (dB; 0 at Drama 0).
    float dramaShapeDb = 0;
    // The orchestration pass: the same, taken from the Drama layer instead (dB, never above 0):
    // the Place's own instruments keep their level.
    float dramaLayerDb = 0;
    // The level and drums pass: free play (not an arrangement section): the Place's own
    // instruments are level-matched at each tier (their busier variants and the harmony's
    // gear measured against Drama 0), so Drama never changes their level.
    bool freePlay = true;
    // The orchestration pass: the variants as written (before drama changed their gear;
    // 0xff: as `variant`). A part drama moved to a busier variant never plays quieter than
    // its A (its level made up, at most 4 dB).
    std::array<uint8_t, slotElements> writtenVariant { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    // The orchestration pass: how much of the slot's environment (its World Space send and
    // its instruments' reverb and delay sends) plays: 1, or less for a Place that does not
    // lead an orchestrated section (several Places do not stack their spaces into mud).
    float spaceFocus = 1;
    // Milestone 16: the section's own clip for each instrument (nullptr: linked, it plays
    // its variant), laid from `clipOrigin` (song beats: the section's first downbeat)
    // and playing for `clipSpan` beats (<= 0: endlessly).
    std::array<const pat::Clip*, slotElements> clip {};
    double clipOrigin = 0, clipSpan = 0;
    // A stinger's ring-out (renders): what rings keeps ringing, no new notes start.
    bool ringOut = false;
    // KEYS (Milestone 14): the instrument the keyboard plays in this slot (-1: none) and
    // its live notes this chunk; REC armed on it (its pattern plays exactly, the generator
    // and randomness paused, in the variant being recorded); keys alone (the host stopped
    // in Transport: only the live notes sound, no patterns).
    int keysElement = -1;
    const keys::Live* keys = nullptr;
    bool keysArmed = false, keysOnly = false;
    int keysVariant = 0;
    // Instrument mode: the slot plays one instrument like a normal software instrument.
    // The World Controls do not shape it (a neutral world), it sits close (no distance),
    // sends nothing to the World Space unless `ambience`, and its output is multiplied
    // by `duck` (from duckFrom to duckTo across the chunk: the engine's fade when the
    // instrument changes). attack and release (0..1, 0.5 as designed) scale its times.
    bool instrument = false, ambience = false;
    float trimDb = 0;   // the instrument's level trim (Instruments/Trims.h)
    float duckFrom = 1, duckTo = 1;
    float attack = 0.5f, release = 0.5f;
    int drone = 1;   // Instrument mode's Drone (Off, Root, Root + Fifth)
    // The Drama tier: what the slot's Drama layer plays (drama 0: no layer at all).
    drama::Cue drama;
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
    // `everywhere`: set while the ARRANGE view is open (its clips are made from them).
    void setVariantFlag(const std::atomic<bool>* flag, const std::atomic<bool>* everywhere = nullptr) {
        allVariants = flag;
        allVariantsToo = everywhere;
    }
    // The Place's beat at the last chunk (for the editor's playhead), and where the next
    // chunk starts.
    double beat() const { return lastBeat; }
    double upcomingBeat() const { return nextBeat; }
    // KEYS: the host stopped and the world silent, the keyboard wakes the slot: what was
    // frozen mid-note is cleared and only the instrument played comes in.
    void wake();
    // Silent at once (another slot while the keyboard plays alone).
    void silence() { gain.snap(0); }
    // Instrument mode: plays `type` from the next chunk on, at once (no crossfade: the
    // engine has faded the slot out), from a fresh start (whatever rang is gone), at its
    // level from the first sample.
    void cutTo(int type);
    // The pattern element e is playing now (nullptr when none), for the tests.
    // (nullptr too while a clip plays it: playingClip.)
    const pat::Pattern* playingPattern(int e) const { return e >= 0 && e < slotElements && clipNow[std::size_t(e)] == nullptr ? playing[std::size_t(e)] : nullptr; }
    const pat::Clip* playingClip(int e) const { return e >= 0 && e < slotElements ? clipNow[std::size_t(e)] : nullptr; }
    const pat::Pattern& generatedPattern(int e, int variant = 0) const {
        return generated[std::size_t(pat::Element::clampVariant(variant))][std::size_t(std::clamp(e, 0, slotElements - 1))];
    }
    // The arrangement's gain on element e now (after its entrance and exit ramps).
    float arrangeGain(int e) const { return arranged[std::size_t(std::clamp(e, 0, slotElements - 1))]; }
    // World Space send for the last rendered chunk.
    float spaceSend() const { return send; }
    // The Drama percussion of the last render (the orchestration pass), for the dry mix
    // only (no World Space): nullptr when there was none.
    const float* percussionLeft() const { return percussionNow ? percL.data() : nullptr; }
    const float* percussionRight() const { return percussionNow ? percR.data() : nullptr; }
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
    std::array<dsp::Smoother, slotElements> elementSmooth;
    dsp::Smoother gain, distance, pan;
    std::array<dsp::OnePole, 4> distanceLp, percLp;
    // The orchestration pass: the Drama percussion's dry bus (after level, distance and pan).
    std::array<float, dsp::chunk> percL {}, percR {};
    bool percussionNow = false;
    dsp::Smoother focus;   // the orchestration pass: the environment's share (spaceFocus), over 0.4 s
    std::array<dsp::Smoother, placeElements> gearMakeUp {};   // the orchestration pass: a busier variant's level made up
    static float patternEnergy(const pat::Pattern& p);
public:
    static inline bool noDoubling = false;   // Tests only (the calibration measures the doubling)
    static inline bool noMakeUp = false;     // Tests only (the calibration measures the gear change)
private:
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

    // `paused`: an element whose generated patterns hold as they are (REC armed on it).
    void updatePatterns(const PlaceSettings& target, int mood, const pat::SlotPatterns& patterns, const std::array<uint8_t, slotElements>& variants,
                        int paused = -1);
    // Generated patterns per variant (A, B, C, Fill) and element.
    using VariantPatterns = std::array<std::array<pat::Pattern, slotElements>, pat::variantCount>;
    VariantPatterns generated {};
    std::array<std::array<GenInputs, slotElements>, pat::variantCount> generatedFor {};
    std::array<std::array<bool, slotElements>, pat::variantCount> generatedValid {};
    void generateVariant(int variant, int e, GenInputs in);
    // The arrangement's gain per element: an entrance from silence starts at once, a
    // re-entrance while ringing rises over 20 ms, an exit fades over 250 ms.
    std::array<float, slotElements> arranged {};
    float arrangeUp = 0.001f, arrangeDown = 0.0001f;
    std::array<const pat::Pattern*, slotElements> playing {};
    std::array<const pat::Clip*, slotElements> clipNow {};
    std::array<pat::Controls, slotElements> controlsNow {};
    pat::TripleBuffer<pat::Generated>* feedback = nullptr;
    const std::atomic<bool>* allVariants = nullptr;
    const std::atomic<bool>* allVariantsToo = nullptr;
    uint32_t feedbackSerial = 0;
    double lastBeat = 0, nextBeat = 0;
    dsp::Smoother keysLevel;
    bool snapGain = false;
    // The Drama tier's layer, themed by the Place playing: the slot's three Drama
    // instruments (elements 6-8), the style and rank their patterns are written for, and
    // their gains at the last chunk (< 0: none yet).
    drama::Layer dramaLayer;
    int dramaStyleNow = 0, dramaRankNow = 0;
    std::array<float, dramaElements> dramaGainNow { -1.0f, -1.0f, -1.0f };
    // The Drama loudness pass: the slot's balance as its tier changes (the Place making
    // room, the Drama layer at its share of the Calm level): gains, smoothed.
    dsp::Smoother placeComp, layerComp, percComp;
    float compRelease = 1;
    float heldLayer = 1, heldPerc = 1;   // the level and drums pass: the layer's last tier level (it rings out at it)
public:
    const drama::Layer& drama() const { return dramaLayer; }
    void clearReductions() { dramaLayer.clearReduction(); }
    // The balance in dB now (the Place, the Drama layer against it): for the tests.
    float placeCompDb() const { return dsp::gainToDb(placeComp.value); }
    float placeCompGain() const { return placeComp.value; }
    // An instrument's own transposition (a live note sounds as pressed; the recorder takes
    // it off): the Place's, or a Drama instrument's.
    float partTranspose(int e) const {
        if (isDramaElement(e)) return dramaLayer.transposeSemis(e);
        return current != nullptr && e >= 0 ? current->partTranspose(e) : 0.0f;
    }
};

} // namespace wb
