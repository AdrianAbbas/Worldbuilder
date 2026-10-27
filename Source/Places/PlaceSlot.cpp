#include "PlaceSlot.h"

namespace wb {

using namespace dsp;

PlaceSlot::PlaceSlot() {
    for (int type = NoPlace + 1; type < PlaceTypeCount; ++type) engines[std::size_t(type)] = makePlace(type);
}

void PlaceSlot::prepare(double sampleRate, int slotIndex) {
    rate = float(sampleRate);
    slot = slotIndex;
    const float controlRate = rate / float(chunk);
    for (auto& e : engines)
        if (e != nullptr) e->prepare(sampleRate);
    for (auto& s : smooth) s.setTime(0.15f, controlRate);
    for (auto& s : elementSmooth) s.setTime(0.08f, controlRate);
    gain.setTime(0.05f, rate);
    distance.setTime(0.2f, controlRate);
    pan.setTime(0.2f, controlRate);
    fadeStep = 1.0f / (0.3f * rate);
    arrangeUp = 1.0f / (0.02f * rate);
    keysLevel.setTime(0.08f, controlRate);
    arrangeDown = 1.0f / (0.25f * rate);
    for (auto& f : dcBlock) f.setCutoff(12.0f, rate);   // a Place fades out and in over 0.3 s when the slot changes type
    fx.prepare(rate);
    for (auto& f : fullness) f.setCutoff(260.0f, rate);
    for (auto& bus : sendTilt)
        for (auto& f : bus) f.setCutoff(900.0f, rate);
    reset();
}

void PlaceSlot::reset() {
    current = nullptr;
    currentType = NoPlace;
    fade = 0;
    freeBeat = 0;
    seeded = false;
    gain.snap(0);
    for (auto& f : distanceLp) f.reset();
    for (auto& f : dcBlock) f.reset();
    for (auto& f : fullness) f.reset();
    fx.reset();
    for (auto& bus : sendTilt)
        for (auto& f : bus) f.reset();
    fresh = true;
    for (auto& v : generatedValid) v.fill(false);
    playing.fill(nullptr);
    arranged.fill(0.0f);
    lastBeat = nextBeat = 0;
    keysLevel.snap(0);
}

void PlaceSlot::wake() {
    if (current != nullptr) current->reset();
    fx.reset();
    for (auto& s : elementSmooth) s.snap(0);
    arranged.fill(0.0f);
    for (auto& bus : sendTilt)
        for (auto& f : bus) f.reset();
}

std::unique_ptr<Place> makePlace(int type) {
    switch (type) {
        case Castle: return std::make_unique<CastlePlace>();
        case Village: return std::make_unique<VillagePlace>();
        case Spaceport: return std::make_unique<SpaceportPlace>();
        case Factory: return std::make_unique<FactoryPlace>();
        case City: return std::make_unique<CityPlace>();
        case Forest: return std::make_unique<ForestPlace>();
        case Hive: return std::make_unique<HivePlace>();
        case Temple: return std::make_unique<TemplePlace>();
        case Cavern: return std::make_unique<CavernPlace>();
        case Ruins: return std::make_unique<RuinsPlace>();
        case Spaceship: return std::make_unique<SpaceshipPlace>();
        case Biodome: return std::make_unique<BiodomePlace>();
        case SeaShip: return std::make_unique<SeaShipPlace>();
        case Suburbs: return std::make_unique<SuburbsPlace>();
        case Mall: return std::make_unique<MallPlace>();
        case Military: return std::make_unique<MilitaryPlace>();
        case Beach: return std::make_unique<BeachPlace>();
        case Desert: return std::make_unique<DesertPlace>();
        case Palace: return std::make_unique<PalacePlace>();
        case Tavern: return std::make_unique<TavernPlace>();
        case SpaceStation: return std::make_unique<StationPlace>();
        case Megacity: return std::make_unique<MegacityPlace>();
        case Bridge: return std::make_unique<BridgePlace>();
        case Mining: return std::make_unique<MiningPlace>();
        case AlienRuins: return std::make_unique<AlienRuinsPlace>();
        case Nebula: return std::make_unique<NebulaPlace>();
        default: return nullptr;
    }
}

void PlaceSlot::tiltSends(float tone, float far, int n) {
    // Dark to bright, as the Places' own Tone filters go (unity at the middle); far
    // away the highs fade (the lows are left alone).
    const float t = tone - 0.5f;
    if (std::abs(t) < 1.0e-3f && far < 1.0e-3f) return;
    const float low = dbToGain(-8.0f * t), high = dbToGain(36.0f * t - 16.0f * far * far);
    const auto tilt = [&](std::array<float, chunk>& l, std::array<float, chunk>& r, std::array<OnePole, 2>& f) {
        for (int i = 0; i < n; ++i) {
            const float a = f[0].lp(l[std::size_t(i)]), b = f[1].lp(r[std::size_t(i)]);
            l[std::size_t(i)] = a * low + (l[std::size_t(i)] - a) * high;
            r[std::size_t(i)] = b * low + (r[std::size_t(i)] - b) * high;
        }
    };
    auto& s = fx.sends;
    for (int b = 0; b < ReverbTypeCount; ++b)
        if (s.revFed[std::size_t(b)]) tilt(s.revL[std::size_t(b)], s.revR[std::size_t(b)], sendTilt[std::size_t(b)]);
    for (int b = 0; b < DelayTypeCount; ++b)
        if (s.dlyFed[std::size_t(b)]) tilt(s.dlyL[std::size_t(b)], s.dlyR[std::size_t(b)], sendTilt[std::size_t(ReverbTypeCount + b)]);
}

Place* PlaceSlot::engineFor(int type) { return type > NoPlace && type < PlaceTypeCount ? engines[std::size_t(type)].get() : nullptr; }

float PlaceSlot::effectiveDistance(const WorldFrame& world, float d) {
    // Colossal worlds and storms push every Place further away.
    return clamp01(d + 0.45f * (world[Scale] - 0.5f) + 0.1f * world[Weather]);
}

PlaceSettings PlaceSlot::settingsFor(const WorldFrame& world, const PlaceSlotControls& c) {
    PlaceSettings s;
    s.character = std::clamp(c.character, 0, placeCharacters - 1);
    const float population = world[Population], machinery = world[Machinery];
    // Population: how busy every Place is, and how deep its effect character goes.
    s.activity = clamp01(c.activity * lerp(0.4f, 1.6f, population));
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
    // Score Intensity: calm below the middle, a climax above it, each Place on its own curve.
    const auto& curve = intensityCurve(c.type);
    const auto at = intensityPosition(world.intensity, curve);
    s.activity = intensify(s.activity, curve.activity, at);
    s.tension = intensify(s.tension, curve.tension, at);
    s.density = intensify(s.density, curve.density, at);
    s.tone = clamp01(s.tone + 0.3f * curve.tone * (at.above - 0.7f * at.below));
    s.fxDepth = clamp01(0.15f + 0.85f * s.activity * lerp(0.6f, 1.3f, population));
    s.life = world[Life];
    s.machinery = machinery;
    s.magic = world[Magic];
    s.era = world[Era];
    s.sync = c.sync;
    for (int e = 0; e < placeElements; ++e) s.element[std::size_t(e)] = c.elementOn[std::size_t(e)] ? clamp01(c.element[std::size_t(e)]) : 0.0f;
    return s;
}

void PlaceSlot::generateVariant(int v, int e, GenInputs in) {
    const auto k = std::size_t(e), kv = std::size_t(v);
    // B is busier, C sparser, the Fill busiest (and builds over its bar); each is
    // regenerated as a fresh take if it came out the same as A.
    switch (v) {
        case pat::VariantB:
            in.activity = std::min(1.0f, in.activity + 0.25f);
            in.density = std::min(1.0f, in.density + 0.2f);
            break;
        case pat::VariantC:
            in.activity = std::max(0.0f, in.activity - 0.25f);
            in.density = std::max(0.0f, in.density - 0.25f);
            break;
        case pat::VariantFill:
            in.activity = std::min(1.0f, in.activity + 0.45f);
            in.density = std::min(1.0f, in.density + 0.4f);
            in.tension = std::min(1.0f, in.tension + 0.1f);
            break;
        default: break;
    }
    auto& out = generated[kv][k];
    current->generate(e, in, out);
    if (v > 0 && generatedValid[0][k] && out.sameNotes(generated[0][k])) {
        in.variant = uint8_t(in.variant ^ (0x40 + 0x10 * v));
        current->generate(e, in, out);
    }
    // Still the same (a held instrument whose generator does not change with activity):
    // B moves every note up a chord step, C down an octave of the scale.
    if (v > 0 && v != pat::VariantFill && generatedValid[0][k] && out.sameNotes(generated[0][k]) && patternInfo(currentType, e).kind == Melodic) {
        const int shift = v == pat::VariantB ? 2 : -music::scaleFor(in.mood).size;
        for (int i = 0; i < out.count; ++i) {
            auto& n = out.notes[std::size_t(i)];
            n.degree = int8_t(std::clamp(int(n.degree) + shift, -40, 40));
        }
    }
    if (v == pat::VariantFill) {
        const int perBar = pat::stepsPerBar(out.resolution);
        for (int i = 0; i < out.count; ++i) {
            auto& n = out.notes[std::size_t(i)];
            const float at = float(n.step % std::max(perBar, 1)) / float(std::max(perBar, 1));
            n.velocity = uint8_t(std::clamp(int(float(n.velocity) * (0.85f + 0.35f * at)), 1, 127));
        }
    }
}

void PlaceSlot::updatePatterns(const PlaceSettings& target, int mood, const pat::SlotPatterns& patterns, const std::array<uint8_t, placeElements>& variants,
                               int paused) {
    const bool mine = patterns.type == currentType;
    // Generator inputs on a grid of 1/256 (rounded down, so a value exactly on one of
    // a generator's thresholds stays on the same side): a turning knob regenerates at
    // most a few hundred times, a smoother settling never does.
    const auto grid = [](float x) { return std::floor(clamp01(x) * 256.0f + 1.0e-3f) / 256.0f; };
    GenInputs in;
    in.character = target.character;
    in.mood = mood;
    in.activity = grid(target.activity);
    in.density = grid(target.density);
    in.tension = grid(target.tension);
    in.life = grid(target.life);
    in.key = seedKey;
    const bool all = allVariants != nullptr && allVariants->load(std::memory_order_relaxed);
    bool changed = false;
    for (int e = 0; e < placeElements; ++e) {
        const auto k = std::size_t(e);
        const auto& info = patternInfo(currentType, e);
        if (info.kind == NoPattern) {
            playing[k] = nullptr;
            continue;
        }
        const auto& element = patterns.element[k];
        controlsNow[k] = mine ? element.controls : defaultControls(currentType, e);
        const int want = pat::Element::clampVariant(variants[k]);
        for (int v = 0; v < pat::variantCount; ++v) {
            // A is always ready (the editor shows it); the others when they play, or
            // while the editor shows them.
            if (v != 0 && v != want && !all) continue;
            if (mine && element.edited(v)) continue;
            in.variant = mine ? element.generation(v) : 0;
            const auto kv = std::size_t(v);
            // REC armed: the generator holds the pattern as it is.
            if (e == paused && generatedValid[kv][k]) continue;
            if (!generatedValid[kv][k] || !(generatedFor[kv][k] == in)) {
                generateVariant(v, e, in);
                generatedFor[kv][k] = in;
                generatedValid[kv][k] = true;
                changed = true;
            }
        }
        playing[k] = mine && element.edited(want) ? &element.patternOf(want) : &generated[std::size_t(want)][k];
    }
    if (changed && feedback != nullptr) {
        auto& out = feedback->write();
        out.type = currentType;
        out.serial = ++feedbackSerial;
        out.pattern = generated[0];
        for (int v = 1; v < pat::variantCount; ++v) out.alt[std::size_t(v - 1)] = generated[std::size_t(v)];
        feedback->publish();
    }
}

bool PlaceSlot::render(const WorldFrame& world, const PlaceSlotControls& c, const pat::SlotPatterns& patterns, bool audible, uint64_t clock,
                       double hostBeat, float hostBpm, float* left, float* right, int n) {
    const int wanted = placeImplemented[std::size_t(std::clamp(c.type, 0, PlaceTypeCount - 1))] ? c.type : NoPlace;
    if (wanted != currentType && fade <= 0.0f) {
        // The old Place has faded out (or nothing played): switch engines.
        currentType = wanted;
        current = engineFor(wanted);
        if (current != nullptr) current->reset();
        freeBeat = hostBeat;
        seeded = false;
        fresh = true;
        for (auto& v : generatedValid) v.fill(false);
        playing.fill(nullptr);
        fx.reset();
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

    // Settings, smoothed at control rate. An element's mute and solo (set in the
    // Pattern Editor) fade it like its switch.
    auto target = settingsFor(world, c);
    if (patterns.type == currentType) {
        bool anySolo = false;
        for (const auto& el : patterns.element) anySolo |= el.controls.solo;
        for (int e = 0; e < placeElements; ++e) {
            const auto& el = patterns.element[std::size_t(e)].controls;
            if (el.mute || (anySolo && !el.solo)) target.element[std::size_t(e)] = 0.0f;
        }
    }
    // The keyboard alone (the host stopped): no patterns, only its live notes.
    if (c.keysOnly) target.element.fill(0.0f);
    const int keysElement = c.keys != nullptr && c.keysElement >= 0 && c.keysElement < placeElements ? c.keysElement : -1;
    auto variants = c.variant;
    if (c.keysArmed && keysElement >= 0) variants[std::size_t(keysElement)] = uint8_t(pat::Element::clampVariant(c.keysVariant));
    const std::array<float, 8> targets { target.activity, target.tension, target.density, target.swing, target.tone, target.size, target.variation, target.fxDepth };
    const bool snapArrange = fresh;
    if (fresh) {
        for (std::size_t i = 0; i < smooth.size(); ++i) smooth[i].snap(targets[i]);
        for (int e = 0; e < placeElements; ++e) elementSmooth[std::size_t(e)].snap(target.element[std::size_t(e)]);
        distance.snap(travelDistance(effectiveDistance(world, c.distance), c.travelFar));
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
    // The Arrangement: an instrument entering from silence starts at once (on the
    // downbeat, at full level), one coming back while it still rings rises over 20 ms,
    // one leaving stops its notes at the boundary and fades what rings over 250 ms.
    for (int e = 0; e < placeElements; ++e) {
        const auto k = std::size_t(e);
        const float want = clamp01(c.arrange[k]);
        float& g = arranged[k];
        if (snapArrange || (want > g && g < 1.0e-4f)) g = want;
        else if (want > g) g = std::min(want, g + arrangeUp * float(n));
        else g = std::max(want, g - arrangeDown * float(n));
        settings.element[k] *= g;
        settings.notes[k] = want > 0.0f && !c.ringOut && !c.keysOnly;
    }

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

    // Patterns: the composer's, or generated from the seed and the settings.
    updatePatterns(target, world.mood, patterns, variants, c.keysArmed ? keysElement : -1);
    for (int e = 0; e < placeElements; ++e) {
        const auto k = std::size_t(e);
        auto& play = context.play[k];
        play.pattern = playing[k];
        if (play.pattern == nullptr) continue;
        play.controls = controlsNow[k];
        // Variation scales every element's Randomness; at its default it is as set.
        play.randomness = clamp01(controlsNow[k].randomness * settings.variation / 0.35f);
        if (c.keysArmed && e == keysElement) {
            // REC armed: the pattern plays exactly as written.
            play.randomness = 0;
            play.controls.evolve = 0;
        }
        play.swing = patterns.type == currentType && patterns.element[k].edited(variants[k]) ? play.pattern->swing : settings.swing;
        play.kit = patternInfo(currentType, e).kind == Kit;
        play.key = mix64((uint64_t(seedKey) << 8) ^ uint64_t(e + 1));
    }
    lastBeat = context.beat;
    nextBeat = context.beat + context.beatsPerSample * double(n);
    context.liveElement = keysElement;
    context.live = keysElement >= 0 ? c.keys : nullptr;
    context.liveLevel = keysLevel.next(keysElement >= 0 ? clamp01(c.element[std::size_t(keysElement)]) : 0.0f);
    fx.begin(c.fx, n);
    context.fx = &fx;
    current->render(context, left, right, n);
    // The sends darken with distance too (the dry sound has its own distance filter).
    tiltSends(settings.tone, distance.value, n);

    // Distance and pan.
    const float d = distance.next(travelDistance(effectiveDistance(world, c.distance), c.travelFar));
    const float p = pan.next(std::clamp(c.pan, -1.0f, 1.0f));
    for (auto& f : distanceLp) f.setCutoff(lerpExp(20000.0f, 1100.0f, std::pow(d, 1.6f)), rate);
    const float near = lerp(1.0f, 0.3f, std::pow(d, 0.8f)), width = lerp(1.0f, 0.45f, d);
    const float right_ = std::max(p, 0.0f), left_ = std::max(-p, 0.0f);
    send = current->spaceSend() * lerp(0.4f, 1.6f, d);
    const float target_ = audible ? dbToGain(std::clamp(c.levelDb, -60.0f, 6.0f) + intensityDb(currentType, world.intensity)) * near * travelGain(c.travelFar) : 0.0f;
    // The sends follow the slot's gain and pan; far away, more of each instrument
    // reaches its reverb and delay.
    const float sendDistance = 1.0f + 0.5f * d;
    for (int i = 0; i < n; ++i) {
        fade = switching ? std::max(0.0f, fade - fadeStep) : std::min(1.0f, fade + fadeStep);
        const float g = gain.next(target_) * fade * fade;
        sendGain[0][std::size_t(i)] = g * sendDistance * (1.0f - 0.6f * right_);
        sendGain[1][std::size_t(i)] = g * sendDistance * (1.0f - 0.6f * left_);
        float l = finiteOr(left[i]), r = finiteOr(right[i]);
        // Weight: about +2.5 dB below 260 Hz.
        l += 0.33f * fullness[0].lp(l);
        r += 0.33f * fullness[1].lp(r);
        const float mid = 0.5f * (l + r), side = 0.5f * (l - r) * width;
        // Two poles: far away, bright instruments (bells, shakers, flutes) fade first.
        l = distanceLp[2].lp(distanceLp[0].lp(dcBlock[0].hp(mid + side)));
        r = distanceLp[3].lp(distanceLp[1].lp(dcBlock[1].hp(mid - side)));
        left[i] = (l * (1.0f - right_) + r * left_ * 0.7f) * g;
        right[i] = (r * (1.0f - left_) + l * right_ * 0.7f) * g;
    }
    return true;
}

} // namespace wb
