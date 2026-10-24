#include "PlaceSlot.h"

namespace wb {

using namespace dsp;

void PlaceSlot::prepare(double sampleRate, int slotIndex) {
    rate = float(sampleRate);
    slot = slotIndex;
    const float controlRate = rate / float(chunk);
    castle.prepare(sampleRate);
    village.prepare(sampleRate);
    spaceport.prepare(sampleRate);
    for (auto& s : smooth) s.setTime(0.15f, controlRate);
    for (auto& s : elementSmooth) s.setTime(0.08f, controlRate);
    gain.setTime(0.05f, rate);
    distance.setTime(0.2f, controlRate);
    pan.setTime(0.2f, controlRate);
    fadeStep = 1.0f / (0.3f * rate);
    for (auto& f : dcBlock) f.setCutoff(12.0f, rate);   // a Place fades out and in over 0.3 s when the slot changes type
    reset();
}

void PlaceSlot::reset() {
    castle.reset();
    village.reset();
    spaceport.reset();
    current = nullptr;
    currentType = NoPlace;
    fade = 0;
    freeBeat = 0;
    seeded = false;
    gain.snap(0);
    for (auto& f : distanceLp) f.reset();
    for (auto& f : dcBlock) f.reset();
    fresh = true;
}

Place* PlaceSlot::engineFor(int type) {
    switch (type) {
        case Castle: return &castle;
        case Village: return &village;
        case Spaceport: return &spaceport;
        default: return nullptr;
    }
}

float PlaceSlot::effectiveDistance(const WorldFrame& world, float d) {
    // Colossal worlds and storms push every Place further away.
    return clamp01(d + 0.45f * (world[Scale] - 0.5f) + 0.1f * world[Weather]);
}

PlaceSettings PlaceSlot::settingsFor(const WorldFrame& world, const PlaceSlotControls& c) {
    PlaceSettings s;
    s.character = std::clamp(c.character, 0, placeCharacters - 1);
    const float population = world[Population], machinery = world[Machinery];
    // Population: how busy every Place is, and how deep its effect character goes.
    s.activity = clamp01(c.activity * lerp(0.5f, 1.5f, population));
    s.fxDepth = clamp01(0.15f + 0.85f * s.activity * lerp(0.6f, 1.3f, population));
    // Danger darkens harmony and tightens rhythm, on top of the Place's own Tension.
    s.tension = clamp01(c.tension + 0.6f * (world[Danger] - 0.2f));
    s.density = clamp01(c.density + 0.25f * machinery);
    // Machines play straight.
    s.swing = clamp01(c.swing * (1.0f - 0.8f * machinery));
    // Futuristic and cold worlds are brighter, storms and hostility darker.
    s.tone = clamp01(c.tone + 0.35f * (world[Era] - 0.3f) + 0.2f * (0.5f - world[Temperature]) - 0.12f * world[Weather] - 0.1f * world[Danger]);
    s.size = clamp01(c.size + 0.35f * (world[Scale] - 0.5f));
    s.variation = clamp01(c.variation * lerp(0.6f, 1.4f, world[Activity]));
    s.life = world[Life];
    s.machinery = machinery;
    s.magic = world[Magic];
    s.era = world[Era];
    s.sync = c.sync;
    for (int e = 0; e < placeElements; ++e) s.element[std::size_t(e)] = c.elementOn[std::size_t(e)] ? clamp01(c.element[std::size_t(e)]) : 0.0f;
    return s;
}

bool PlaceSlot::render(const WorldFrame& world, const PlaceSlotControls& c, bool audible, uint64_t clock, double hostBeat, float hostBpm,
                       float* left, float* right, int n) {
    const int wanted = placeImplemented[std::size_t(std::clamp(c.type, 0, PlaceTypeCount - 1))] ? c.type : NoPlace;
    if (wanted != currentType && fade <= 0.0f) {
        // The old Place has faded out (or nothing played): switch engines.
        currentType = wanted;
        current = engineFor(wanted);
        if (current != nullptr) current->reset();
        freeBeat = hostBeat;
        seeded = false;
        fresh = true;
    }
    const bool switching = wanted != currentType;
    if (current == nullptr || (!audible && gain.value < 1.0e-5f && !switching)) {
        if (current == nullptr) fade = 0;
        gain.snap(audible ? gain.value : 0.0f);
        return false;
    }

    const uint32_t key = uint32_t(mix64((uint64_t(world.seed) << 32) ^ (uint64_t(c.seed) << 8) ^ uint64_t(slot)));
    if (!seeded || key != seedKey) {
        seedKey = key;
        seeded = true;
        current->setSeed(key);
    }

    // Settings, smoothed at control rate.
    const auto target = settingsFor(world, c);
    const std::array<float, 8> targets { target.activity, target.tension, target.density, target.swing, target.tone, target.size, target.variation, target.fxDepth };
    if (fresh) {
        for (std::size_t i = 0; i < smooth.size(); ++i) smooth[i].snap(targets[i]);
        for (int e = 0; e < placeElements; ++e) elementSmooth[std::size_t(e)].snap(target.element[std::size_t(e)]);
        distance.snap(effectiveDistance(world, c.distance));
        pan.snap(std::clamp(c.pan, -1.0f, 1.0f));
        fresh = false;
    }
    settings = target;
    settings.activity = smooth[0].next(targets[0]);
    settings.tension = smooth[1].next(targets[1]);
    settings.density = smooth[2].next(targets[2]);
    settings.swing = smooth[3].next(targets[3]);
    settings.tone = smooth[4].next(targets[4]);
    settings.size = smooth[5].next(targets[5]);
    settings.variation = smooth[6].next(targets[6]);
    settings.fxDepth = smooth[7].next(targets[7]);
    for (int e = 0; e < placeElements; ++e) settings.element[std::size_t(e)] = elementSmooth[std::size_t(e)].next(target.element[std::size_t(e)]);

    // The beat: the host's (or the world's default tempo), or the Place's own free tempo.
    PlaceContext context;
    context.world = &world;
    context.clock = clock;
    context.s = settings;
    if (c.sync) {
        context.beat = hostBeat;
        context.bpm = hostBpm;
        freeBeat = hostBeat;
    } else {
        context.beat = freeBeat;
        context.bpm = current->freeTempo() * lerp(0.85f, 1.15f, settings.activity);
    }
    context.beatsPerSample = double(context.bpm) / 60.0 / double(rate);
    if (!c.sync) freeBeat += context.beatsPerSample * double(n);
    // Harmony always follows the shared clock, so free and synced Places agree.
    context.chord = music::chordDegree(hostBeat, world[Danger]);

    current->render(context, left, right, n);

    // Distance and pan.
    const float d = distance.next(effectiveDistance(world, c.distance));
    const float p = pan.next(std::clamp(c.pan, -1.0f, 1.0f));
    for (auto& f : distanceLp) f.setCutoff(lerpExp(18000.0f, 2000.0f, d), rate);
    const float near = lerp(1.0f, 0.3f, std::pow(d, 0.8f)), width = lerp(1.0f, 0.45f, d);
    const float right_ = std::max(p, 0.0f), left_ = std::max(-p, 0.0f);
    send = current->spaceSend() * lerp(0.4f, 1.6f, d);
    const float target_ = audible ? dbToGain(std::clamp(c.levelDb, -60.0f, 6.0f)) * near : 0.0f;
    for (int i = 0; i < n; ++i) {
        fade = switching ? std::max(0.0f, fade - fadeStep) : std::min(1.0f, fade + fadeStep);
        const float g = gain.next(target_) * fade * fade;
        float l = finiteOr(left[i]), r = finiteOr(right[i]);
        const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * width;
        l = distanceLp[0].lp(dcBlock[0].hp(mid + side));
        r = distanceLp[1].lp(dcBlock[1].hp(mid - side));
        left[i] = (l * (1.0f - right_) + r * left_ * 0.7f) * g;
        right[i] = (r * (1.0f - left_) + l * right_ * 0.7f) * g;
    }
    return true;
}

} // namespace wb
