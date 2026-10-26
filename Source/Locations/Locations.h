#pragma once
// Locations: saved snapshots of the whole world (World Controls, environment levels,
// Root, Mood, seeds, the four Place slots with their Characters, settings, instrument
// effects and patterns, Score Intensity), and the rules for moving between two of them.
//
// A world here is a set of parameter values plus each slot's patterns. Moving between
// two worlds is a blend at a position e (0..1):
// - continuous values (World Controls, Place settings, effect sends...) glide;
// - levels blend as gains, a muted layer, Place or instrument counting as silent;
// - choices (Root, Mood, seeds, Characters, effect types, Sync) change at the far
//   point, the middle of the way, when the Places that change are furthest away;
// - a slot holding the same Place at both ends morphs its settings; a slot whose Place
//   changes sends the old Place away into the distance (quieter, darker, more
//   reverberant) over the first half and brings the new one in from there over the
//   second; a Place arriving in an empty slot approaches over the whole way, and one
//   leaving recedes over the whole way.
// The distance a Place has been sent away is `far` (0: where the slot puts it, 1: at
// the horizon); PlaceSlot applies it.
#include "../Parameters.h"
#include "../Places/Place.h"
#include "../Patterns/Pattern.h"
#include "../DSP/Core.h"

namespace wb {

constexpr int locationCount = 8;
constexpr int locationSlots = 4;   // Place slots in a Location (the engine's placeSlots)

using WorldValues = std::array<float, PCount>;

// How a parameter moves between two Locations.
enum class Move {
    Live,    // not part of a Location: Power, Master, Quality, Play Mode, solos, the Location controls
    Glide,   // continuous
    Gain,    // a level, blended as a gain together with its mute or switch
    Paired,  // a mute or switch, blended with its level
    Switch,  // a choice: changes at the far point
};

inline Move moveOf(int p) {
    // The World Seed stays too: reseeding the environment layers cannot glide (their
    // noise and gusts jump to other curves), so a journey would dip them out and back.
    if (p == PPower || p == PMaster || p == PQuality || p == PPlayMode || p == PPlaceOuts || p == PSeed || (p >= PDestination && p <= PPathEnd)) return Move::Live;
    if (p >= PLayerFirst && p < PPlaceFirst) {
        const int field = (p - PLayerFirst) % 3;
        return field == 0 ? Move::Gain : field == 1 ? Move::Paired : Move::Live;
    }
    if (p >= PPlaceFirst && p < PPlayMode) {
        const int field = (p - PPlaceFirst) % PlaceParamCount;
        if (field == PlSolo) return Move::Live;
        if (field == PlLevel || (field >= PlElement && field < PlElementOn)) return Move::Gain;
        if (field == PlMute || (field >= PlElementOn && field < PlFx)) return Move::Paired;
    }
    const auto kind = specs[std::size_t(p)].kind;
    return kind == Kind::Float || kind == Kind::Decibels || kind == Kind::Seconds ? Move::Glide : Move::Switch;
}

inline int slotTypeIn(const WorldValues& v, int slot) {
    return std::clamp(int(std::lround(v[std::size_t(placeParam(slot, PlType))])), 0, PlaceTypeCount - 1);
}

// A world as it sounds: its values, how far each slot's Place has been sent away, and
// each slot's patterns (nullptr: the live patterns the editor publishes).
struct Sounding {
    WorldValues v {};
    std::array<float, locationSlots> far {};
    std::array<const pat::SlotPatterns*, locationSlots> patterns {};
};

// A Location as the audio thread holds it.
struct LocationData {
    bool used = false;
    WorldValues values {};
    std::array<pat::SlotPatterns, locationSlots> patterns {};
};

namespace locdetail {
// A level and its mute (layers, Places: on = !mute) or switch (instruments: on = switch)
// as one gain.
inline float levelGain(float value, bool decibels, bool on) {
    if (!on) return 0.0f;
    return decibels ? (value <= -59.95f ? 0.0f : dsp::dbToGain(value)) : value;
}

// Blends parameter p and its pair (level p, mute or switch q) from a and b into out.
inline void blendLevel(const WorldValues& a, const WorldValues& b, float e, int p, int q, bool muteIsOff, WorldValues& out) {
    const bool decibels = specs[std::size_t(p)].kind == Kind::Decibels;
    const auto isOn = [&](const WorldValues& v) { return muteIsOff ? v[std::size_t(q)] < 0.5f : v[std::size_t(q)] > 0.5f; };
    const bool onA = isOn(a), onB = isOn(b);
    if (!onA && !onB) {
        out[std::size_t(p)] = e < 0.5f ? a[std::size_t(p)] : b[std::size_t(p)];
        out[std::size_t(q)] = b[std::size_t(q)];
        return;
    }
    if (e <= 0.0f || e >= 1.0f || (a[std::size_t(p)] == b[std::size_t(p)] && a[std::size_t(q)] == b[std::size_t(q)])) {
        const auto& end = e <= 0.0f ? a : b;
        out[std::size_t(p)] = end[std::size_t(p)];
        out[std::size_t(q)] = end[std::size_t(q)];
        return;
    }
    const float g = dsp::lerp(levelGain(a[std::size_t(p)], decibels, onA), levelGain(b[std::size_t(p)], decibels, onB), e);
    const auto& spec = specs[std::size_t(p)];
    out[std::size_t(p)] = decibels ? (g <= 0.001f ? spec.min : std::clamp(20.0f * std::log10(g), spec.min, spec.max)) : g;
    out[std::size_t(q)] = muteIsOff ? 0.0f : 1.0f;
}

// Blends parameters [first, last) by their Move; Live ones are left as they are in out.
inline void blendRange(const WorldValues& a, const WorldValues& b, float e, int first, int last, WorldValues& out) {
    for (int p = first; p < last; ++p) {
        const auto k = std::size_t(p);
        switch (moveOf(p)) {
            case Move::Live:
            case Move::Paired: break;   // a pair is written with its level
            case Move::Glide: out[k] = a[k] == b[k] ? a[k] : a[k] * (1.0f - e) + b[k] * e; break;   // exact at both ends
            case Move::Switch: out[k] = e < 0.5f ? a[k] : b[k]; break;
            case Move::Gain: {
                int pair = p + 1;   // a layer's or a Place's mute follows its level
                bool muteIsOff = true;
                if (p >= PPlaceFirst) {
                    const int slotBase = PPlaceFirst + (p - PPlaceFirst) / PlaceParamCount * PlaceParamCount;
                    const int field = p - slotBase;
                    if (field == PlLevel) pair = slotBase + PlMute;
                    else {
                        pair = slotBase + PlElementOn + (field - PlElement);
                        muteIsOff = false;
                    }
                }
                blendLevel(a, b, e, p, pair, muteIsOff, out);
                break;
            }
        }
    }
}
} // namespace locdetail

// The world at position e (0..1) of the way from a to b. Live values in out are left
// untouched (the caller fills them from the host).
inline void blendWorlds(const Sounding& a, const Sounding& b, float e, Sounding& out) {
    e = std::clamp(e, 0.0f, 1.0f);
    if (e <= 0.0f || e >= 1.0f) {
        // At either end the world is exactly that end.
        const auto& end = e <= 0.0f ? a : b;
        for (int p = 0; p < PCount; ++p)
            if (moveOf(p) != Move::Live) out.v[std::size_t(p)] = end.v[std::size_t(p)];
        out.far = end.far;
        out.patterns = end.patterns;
        return;
    }
    // Everything outside the Place slots.
    locdetail::blendRange(a.v, b.v, e, 0, PPlaceFirst, out.v);
    locdetail::blendRange(a.v, b.v, e, PPlayMode, PCount, out.v);
    for (int slot = 0; slot < locationSlots; ++slot) {
        const auto k = std::size_t(slot);
        const int first = placeParam(slot, 0), last = first + PlaceParamCount;
        const int ta = slotTypeIn(a.v, slot), tb = slotTypeIn(b.v, slot);
        const auto copyFrom = [&](const Sounding& s) {
            for (int p = first; p < last; ++p)
                if (moveOf(p) != Move::Live) out.v[std::size_t(p)] = s.v[std::size_t(p)];
            out.patterns[k] = s.patterns[k];
        };
        if (ta == tb) {
            // The same Place (or none) at both ends: its settings morph.
            locdetail::blendRange(a.v, b.v, e, first, last, out.v);
            out.far[k] = a.far[k] == b.far[k] ? a.far[k] : dsp::lerp(a.far[k], b.far[k], e);
            out.patterns[k] = e < 0.5f ? a.patterns[k] : b.patterns[k];
        } else if (ta == NoPlace) {
            // A Place arrives in an empty slot: it approaches from the horizon.
            copyFrom(b);
            out.far[k] = 1.0f - e * (1.0f - b.far[k]);
        } else if (tb == NoPlace) {
            // A Place leaves: it recedes to the horizon (the slot empties on arrival).
            copyFrom(a);
            out.far[k] = a.far[k] + e * (1.0f - a.far[k]);
        } else if (e < 0.5f) {
            // Another Place takes the slot: the old one recedes, then the new one approaches.
            copyFrom(a);
            out.far[k] = a.far[k] + 2.0f * e * (1.0f - a.far[k]);
        } else {
            copyFrom(b);
            out.far[k] = 1.0f - 2.0f * (e - 0.5f) * (1.0f - b.far[k]);
        }
    }
}

// Ease in and out: a journey starts and arrives gently.
inline float easeTravel(float t) { return 0.5f - 0.5f * std::cos(dsp::pi * std::clamp(t, 0.0f, 1.0f)); }

// The palette of Location colours (ARGB), one per slot by default.
inline constexpr std::array<uint32_t, 8> locationColours { 0xFFE2B45Au, 0xFF4FD6E6u, 0xFFE0708Au, 0xFF7FD27Au,
                                                           0xFFA98BF0u, 0xFFF08A3Cu, 0xFF3CC9A6u, 0xFFD8D2C4u };

} // namespace wb
