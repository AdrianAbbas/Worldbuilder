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
    focus.setTime(0.4f, controlRate);
    for (auto& g : gearMakeUp) g.setTime(0.03f, controlRate);
    // Down fast (the louder tier's first downbeat is already balanced), back up slowly.
    placeComp.setTime(0.012f, controlRate);
    layerComp.setTime(0.012f, controlRate);
    percComp.setTime(0.012f, controlRate);
    compRelease = 1.0f - std::exp(-1.0f / (0.15f * controlRate));
    arrangeDown = 1.0f / (0.25f * rate);
    for (auto& f : dcBlock) f.setCutoff(12.0f, rate);   // a Place fades out and in over 0.3 s when the slot changes type
    fx.prepare(rate);
    dramaLayer.prepare(rate);
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
    focus.snap(1.0f);
    for (auto& g : gearMakeUp) g.snap(1.0f);
    for (auto& f : distanceLp) f.reset();
    for (auto& f : percLp) f.reset();
    for (auto& f : dcBlock) f.reset();
    for (auto& f : fullness) f.reset();
    fx.reset();
    dramaLayer.reset();
    dramaGainNow.fill(-1.0f);
    percComp.snap(1.0f);
    placeComp.snap(1.0f);
    layerComp.snap(1.0f);
    heldLayer = heldPerc = 1.0f;
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
    dramaLayer.reset();
    fx.reset();
    for (auto& s : elementSmooth) s.snap(0);
    arranged.fill(0.0f);
    for (auto& bus : sendTilt)
        for (auto& f : bus) f.reset();
}

void PlaceSlot::cutTo(int type) {
    const int wanted = type > NoPlace && type < PlaceTypeCount && placeImplemented[std::size_t(type)] ? type : NoPlace;
    currentType = wanted;
    current = engineFor(wanted);
    if (current != nullptr) current->reset();
    fade = 1;
    seeded = false;
    fresh = true;
    snapGain = true;
    for (auto& v : generatedValid) v.fill(false);
    playing.fill(nullptr);
    clipNow.fill(nullptr);
    fx.reset();
    dramaLayer.reset();
    for (auto& s : elementSmooth) s.snap(0);
    arranged.fill(0.0f);
    for (auto& bus : sendTilt)
        for (auto& f : bus) f.reset();
    keysLevel.snap(0);
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
    std::copy_n(c.slide.begin(), placeElements, s.slide.begin());
    return s;
}

void PlaceSlot::generateVariant(int v, int e, GenInputs in) {
    const auto k = std::size_t(e), kv = std::size_t(v);
    // The Drama instruments: written by their tier, style and Place (Drama.cpp).
    if (isDramaElement(e)) {
        drama::generate(currentType, e, v, in, generated[kv][k]);
        return;
    }
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

void PlaceSlot::updatePatterns(const PlaceSettings& target, int mood, const pat::SlotPatterns& patterns, const std::array<uint8_t, slotElements>& variants,
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
    const bool all = (allVariants != nullptr && allVariants->load(std::memory_order_relaxed))
                     || (allVariantsToo != nullptr && allVariantsToo->load(std::memory_order_relaxed));
    bool changed = false;
    for (int e = 0; e < slotElements; ++e) {
        const auto k = std::size_t(e);
        const auto& info = patternInfo(currentType, e);
        // The Drama instruments' patterns are written for the tier playing (Tension at
        // least), the Drama Style and the slot's rank.
        if (isDramaElement(e)) {
            in.tier = dramaLayer.patternTier();
            in.style = dramaStyleNow;
            in.rank = dramaRankNow;
        } else {
            in.tier = in.style = in.rank = 0;
        }
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

float PlaceSlot::patternEnergy(const pat::Pattern& p) {
    // The notes' power per beat (velocity squared, a note's length counted up to a beat).
    const double perBeat = pat::stepsPerBeat(p.resolution), beats = std::max(p.lengthBeats(), 0.25);
    double sum = 0;
    for (int i = 0; i < p.count; ++i) {
        const auto& n = p.notes[std::size_t(i)];
        const double v = double(n.velocity) / 127.0 * double(n.probability) / 100.0;
        sum += v * v * std::clamp(double(n.length) / perBeat, 0.25, 1.0);
    }
    return float(sum / beats);
}

namespace {
// Instrument mode: the world as the defaults set it (the World Controls and Score
// Intensity at their defaults), so one instrument sounds the same in every project.
WorldFrame neutralWorld(const WorldFrame& world) {
    WorldFrame w = world;
    for (int i = 0; i < WorldControlCount; ++i) w.c[std::size_t(i)] = specs[std::size_t(PTemperature + i)].def;
    w.intensity = 0.5f;
    return w;
}
} // namespace

bool PlaceSlot::render(const WorldFrame& worldIn, const PlaceSlotControls& c, const pat::SlotPatterns& patterns, bool audible, uint64_t clock,
                       double hostBeat, float hostBpm, float* left, float* right, int n) {
    const WorldFrame neutral = c.instrument ? neutralWorld(worldIn) : WorldFrame {};
    const WorldFrame& world = c.instrument ? neutral : worldIn;
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
        clipNow.fill(nullptr);
        fx.reset();
    }
    const bool switching = wanted != currentType;
    percussionNow = false;
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
        dramaLayer.setSeed(key);
    }

    // Settings, smoothed at control rate. An element's mute and solo (set in the
    // Pattern Editor) fade it like its switch.
    auto target = settingsFor(world, c);
    // The Drama instruments' levels (their switches, mutes and solos as the six's).
    std::array<float, dramaElements> dramaTarget {};
    for (int d = 0; d < dramaElements; ++d)
        dramaTarget[std::size_t(d)] = c.elementOn[std::size_t(placeElements + d)] ? clamp01(c.element[std::size_t(placeElements + d)]) : 0.0f;
    if (patterns.type == currentType) {
        bool anySolo = false;
        for (const auto& el : patterns.element) anySolo |= el.controls.solo;
        for (int e = 0; e < slotElements; ++e) {
            const auto& el = patterns.element[std::size_t(e)].controls;
            if (!(el.mute || (anySolo && !el.solo))) continue;
            if (e < placeElements) target.element[std::size_t(e)] = 0.0f;
            else dramaTarget[std::size_t(e - placeElements)] = 0.0f;
        }
    }
    // The keyboard alone (the host stopped): no patterns, only its live notes.
    if (c.keysOnly) {
        target.element.fill(0.0f);
        dramaTarget.fill(0.0f);
    }
    // Instrument mode without Place Ambience: just the instrument, in the Place's
    // smallest space with its effect character at its least (the instrument's own
    // reverb and delay sends are the space).
    if (c.instrument && !c.ambience) {
        target.size = 0.0f;
        target.fxDepth = 0.15f;
    }
    const int keysElement = c.keys != nullptr && c.keysElement >= 0 && c.keysElement < slotElements ? c.keysElement : -1;
    auto variants = c.variant;
    if (c.keysArmed && keysElement >= 0) variants[std::size_t(keysElement)] = uint8_t(pat::Element::clampVariant(c.keysVariant));
    const std::array<float, 8> targets { target.activity, target.tension, target.density, target.swing, target.tone, target.size, target.variation, target.fxDepth };
    const bool snapArrange = fresh;
    if (fresh) {
        for (std::size_t i = 0; i < smooth.size(); ++i) smooth[i].snap(targets[i]);
        for (int e = 0; e < placeElements; ++e) elementSmooth[std::size_t(e)].snap(target.element[std::size_t(e)]);
        for (int d = 0; d < dramaElements; ++d) elementSmooth[std::size_t(placeElements + d)].snap(dramaTarget[std::size_t(d)]);
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
    std::array<float, dramaElements> dramaLevel {};
    std::array<bool, dramaElements> dramaNotes {};
    for (int d = 0; d < dramaElements; ++d) dramaLevel[std::size_t(d)] = elementSmooth[std::size_t(placeElements + d)].next(dramaTarget[std::size_t(d)]);
    // The Arrangement: an instrument entering from silence starts at once (on the
    // downbeat, at full level), one coming back while it still rings rises over 20 ms,
    // one leaving stops its notes at the boundary and fades what rings over 250 ms.
    for (int e = 0; e < slotElements; ++e) {
        const auto k = std::size_t(e);
        const float want = clamp01(c.arrange[k]);
        float& g = arranged[k];
        if (snapArrange || (want > g && g < 1.0e-4f)) g = want;
        else if (want > g) g = std::min(want, g + arrangeUp * float(n));
        else g = std::max(want, g - arrangeDown * float(n));
        if (e >= placeElements) {
            dramaLevel[k - placeElements] *= g;
            dramaNotes[k - placeElements] = want > 0.0f && !c.ringOut && !c.keysOnly;
            continue;
        }
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
    context.chord = music::chordDegree(hostBeat, world[Danger], world.harmony, world.mood);

    // The Drama instruments' tier for this chunk (latched on the bar lines), before their
    // patterns are chosen. (Drama 0: the layer does not exist, unless the keyboard plays it.)
    const bool dramaKeys = keysElement >= placeElements;
    const bool dramaOn = (c.drama.drama > 0.0f || dramaLayer.active() || dramaKeys) && current != nullptr;
    const float dramaTension = clamp01(c.tension + 0.6f * (world[Danger] - 0.2f));
    if (dramaOn) dramaLayer.advance(currentType, world, c.drama, dramaTension, context.beat, context.beatsPerSample * double(n));
    dramaStyleNow = c.drama.style;
    dramaRankNow = c.drama.rank;
    // The Drama loudness pass: as the tier rises the Place makes room and the Drama layer
    // takes its share, so the slot stays about as loud as at Calm (a table measured per
    // Place, Drama Style and tier; Drama.cpp). Calm and Drama 0: exactly as before.
    float placeTo = 1.0f, layerTo = 1.0f, percTo = 1.0f;
    // The level and drums pass: the Place is never turned down (no ducking, no catch-up); the
    // Drama layer plays at its tier's level (the table) and keeps that level while it rings out
    // (Drama turned to 0, the tier back at Calm): no jump to its unbalanced level. A layer
    // waking up starts at its level at once (no swell from a stale gain), and a tier change
    // glides both ways in 12 ms.
    const bool levelMix = drama::levelPass && drama::Layer::mixPass && drama::loudnessPass;
    if (levelMix && dramaOn && !c.instrument && !(dramaKeys && c.drama.drama <= 0.0f && !dramaLayer.active())) {
        const int style = drama::styleFor(currentType, c.drama.style);
        const int t = dramaLayer.tier() > drama::Calm ? dramaLayer.tier() : dramaLayer.wanted();
        if (c.drama.drama > 0.0f && t > drama::Calm) {
            const auto b = drama::balance(currentType, style, t, c.drama.rank, world.intensity);
            layerTo = dbToGain(b.layerDb + std::min(0.0f, c.dramaLayerDb));
            percTo = dbToGain(std::clamp(b.percRelDb, -24.0f, 12.0f));
            heldLayer = layerTo;
            heldPerc = percTo;
        } else {
            layerTo = heldLayer;
            percTo = heldPerc;
        }
        if (!dramaLayer.active()) {
            layerComp.snap(layerTo);
            percComp.snap(percTo);
        }
    } else if (levelMix) {
        heldLayer = heldPerc = 1.0f;
    } else if (dramaOn && !c.instrument && c.drama.drama > 0.0f) {
        float placeDb = c.dramaShapeDb;
        const int style = drama::styleFor(currentType, c.drama.style);
        if (dramaLayer.tier() > drama::Calm) {
            const auto b = drama::balance(currentType, style, dramaLayer.tier(), c.drama.rank, world.intensity);
            placeDb += b.placeDb;
            layerTo = dbToGain(b.layerDb - b.placeDb + std::min(0.0f, c.dramaLayerDb));
            percTo = dbToGain(std::clamp(b.percRelDb, -24.0f, 12.0f));
        }
        // Rising, the Place makes room for the tier ahead before its bar line (its own rise
        // with Score Intensity does not wait for it); the layer keeps the tier it plays.
        if (dramaLayer.wanted() > dramaLayer.tier()) {
            const auto ahead = drama::balance(currentType, style, dramaLayer.wanted(), c.drama.rank, world.intensity);
            const auto now = drama::balance(currentType, style, dramaLayer.tier(), c.drama.rank, world.intensity);
            if (ahead.placeDb < now.placeDb) {
                placeDb += ahead.placeDb - now.placeDb;
                layerTo *= dbToGain(now.placeDb - ahead.placeDb);   // (the layer's own level stays)
            }
        }
        placeTo = dbToGain(placeDb);
    }
    const bool compResting = placeComp.value == 1.0f && layerComp.value == 1.0f && placeTo == 1.0f && layerTo == 1.0f;
    const auto follow = [&](dsp::Smoother& sm, float to) {
        if (to < sm.value || levelMix) return sm.next(to);
        return sm.value += compRelease * (to - sm.value);
    };
    const float placeGain = compResting ? 1.0f : follow(placeComp, placeTo);
    const float layerGain = compResting ? 1.0f : follow(layerComp, layerTo);
    const float percGain = percComp.value == 1.0f && percTo == 1.0f ? 1.0f : follow(percComp, percTo);
    if (!compResting && std::abs(placeComp.value - placeTo) < 1.0e-5f && std::abs(layerComp.value - layerTo) < 1.0e-5f) {
        placeComp.snap(placeTo);
        layerComp.snap(layerTo);
    }
    if (std::abs(percComp.value - percTo) < 1.0e-5f) percComp.snap(percTo);

    // Patterns: the composer's, or generated from the seed and the settings.
    updatePatterns(target, world.mood, patterns, variants, c.keysArmed ? keysElement : -1);
    // The orchestration pass: a part drama moves from A to its busier B keeps A where B came
    // out sparser (some generators draw B's optional phrases differently): the Place's own
    // instruments never thin out, or get quieter, as Drama rises.
    std::array<bool, placeElements> gearB {};
    if (drama::Layer::mixPass && drama::loudnessPass && c.drama.drama > 0.0f && !c.instrument)
        for (int e = 0; e < placeElements; ++e) {
            const auto k = std::size_t(e);
            if (c.writtenVariant[k] != pat::VariantA || variants[k] != pat::VariantB || playing[k] == nullptr) continue;
            if (c.clip[k] != nullptr && c.variant[k] != pat::VariantFill) continue;
            const bool mine = patterns.type == currentType;
            if (mine && patterns.element[k].edited(pat::VariantB)) continue;   // the composer's B plays as written
            const pat::Pattern& a = mine && patterns.element[k].edited(0) ? patterns.element[k].pattern : generated[0][k];
            if (patternEnergy(*playing[k]) < 0.9f * patternEnergy(a)) playing[k] = &a;
            else gearB[k] = true;
        }
    const int scaleSize = music::scaleFor(world.mood).size;
    std::array<pat::Playback, dramaElements> dramaPlay {};
    for (int e = 0; e < slotElements; ++e) {
        const auto k = std::size_t(e);
        auto& play = e < placeElements ? context.play[k] : dramaPlay[k - placeElements];
        // The section's own clip (Milestone 16), else the pattern (variant) it is linked to.
        const pat::Clip* clip = playing[k] != nullptr ? c.clip[k] : nullptr;
        clipNow[k] = clip;
        if (clip != nullptr) play.setClip(clip, c.clipOrigin, c.clipSpan);
        else play.set(playing[k]);
        if (!play.active()) continue;
        play.controls = controlsNow[k];
        // Variation scales every element's Randomness; at its default it is as set.
        play.randomness = clamp01(controlsNow[k].randomness * settings.variation / 0.35f);
        if (c.keysArmed && e == keysElement) {
            // REC armed: the pattern plays exactly as written.
            play.randomness = 0;
            play.controls.evolve = 0;
        }
        if (clip != nullptr) play.swing = clip->ownSwing ? clip->swing : settings.swing;
        else play.swing = patterns.type == currentType && patterns.element[k].edited(variants[k]) ? playing[k]->swing : settings.swing;
        play.kit = patternInfo(currentType, e).kind == Kit;
        play.key = mix64((uint64_t(seedKey) << 8) ^ uint64_t(e + 1));
        // The drama rising: a part doubled an octave up or down (the arrangement's gear).
        play.doubleDegrees = noDoubling ? 0 : c.doubling[k] * scaleSize;
        play.doubleVelocity = drama::Layer::mixPass && drama::loudnessPass ? 0.6f : 0.7f;
        // The Drama instruments play as written, without the Place's swing.
        if (e >= placeElements && clip == nullptr && !(patterns.type == currentType && patterns.element[k].edited(variants[k]))) play.swing = 0;
        // A clip's randomness is its own section's (the same at every pass of the song).
        if (clip != nullptr) play.key = mix64(play.key ^ (uint64_t(int64_t(std::llround(c.clipOrigin)) + 1) << 20));
    }
    // The orchestration pass: a part drama moved to a busier (or sparser) variant than written
    // plays at least as loud as its A (some Places' busier takes are softer: the Palace's
    // melody 4 dB): the Place's own instruments never get quieter as Drama rises.
    if (drama::Layer::mixPass && drama::loudnessPass && c.drama.drama > 0.0f && !c.instrument)
        for (int e = 0; e < placeElements; ++e) {
            const auto k = std::size_t(e);
            float want = 1.0f;
            // (Measured per Place and instrument: B in a higher register can sound softer.)
            // (Both ways: a busier B never makes the part quieter, nor louder than A by more than
            // it gets denser.)
            if (gearB[k] && clipNow[k] == nullptr && !noMakeUp) want = dbToGain(std::clamp(-drama::busierDb(currentType, e, dramaLayer.tier()), -6.0f, 3.0f));
            // A part doubled an octave plays denser, not louder: what the double adds is taken
            // from the part (measured per instrument), so the drama parts keep their room.
            if (c.doubling[k] != 0 && !noDoubling && !drama::forceDoubling) want *= dbToGain(-std::clamp(drama::doublingDb(currentType, e), 0.0f, 6.0f));
            const float g = gearMakeUp[k].value == 1.0f && want == 1.0f ? 1.0f : gearMakeUp[k].next(want);
            context.s.element[k] *= g;
        }
    lastBeat = context.beat;
    nextBeat = context.beat + context.beatsPerSample * double(n);
    context.liveElement = keysElement < placeElements ? keysElement : -1;
    context.live = keysElement >= 0 && keysElement < placeElements ? c.keys : nullptr;
    if (c.instrument && keysElement >= 0) keysLevel.snap(clamp01(c.element[std::size_t(keysElement)]));
    context.liveLevel = keysLevel.next(keysElement >= 0 ? clamp01(c.element[std::size_t(keysElement)]) : 0.0f);
    context.instrument = c.instrument;
    context.attack = c.attack;
    context.release = c.release;
    context.drone = c.drone;
    fx.begin(c.fx, n);
    context.fx = &fx;
    current->render(context, left, right, n);
    // The Drama instruments (only while a tier above Calm plays, or their tails ring, or
    // the keyboard plays one of them), through their own levels and effects.
    if (dramaOn) {
        drama::Layer::Elements el;
        for (int d = 0; d < dramaElements; ++d) {
            const auto k = std::size_t(d);
            el.play[k] = &dramaPlay[k];
            float level = dramaLevel[k];
            if (keysElement == placeElements + d) level = std::max(level, context.liveLevel > 0.0f ? context.liveLevel : keysLevel.value);
            el.gainFrom[k] = dramaGainNow[k] < 0.0f ? level / 0.8f * layerGain : dramaGainNow[k];
            el.gainTo[k] = level / 0.8f * layerGain;   // 0.8 (the default level) is the layer as designed
            dramaGainNow[k] = el.gainTo[k];
            el.notes[k] = dramaNotes[k];
        }
        el.fx = &fx;
        el.fxDepth = c.instrument ? 1.0f : lerp(0.5f, 1.5f, settings.fxDepth);
        if (dramaKeys) {
            el.liveElement = keysElement - placeElements;
            el.live = c.keys;
        }
        el.instrument = c.instrument;
        el.slotGain = placeGain;
        el.percGain = percGain;
        // The orchestration pass: the Drama percussion on the slot's dry bus (below).
        if (drama::Layer::mixPass && drama::loudnessPass) {
            std::fill_n(percL.begin(), n, 0.0f);
            std::fill_n(percR.begin(), n, 0.0f);
            el.percL = percL.data();
            el.percR = percR.data();
            percussionNow = true;
        }
        dramaLayer.render(currentType, world, c.drama, dramaTension, context.beat, context.beatsPerSample, c.sync, el, clock, left, right, n);
    }
    // The sends darken with distance too (the dry sound has its own distance filter).
    tiltSends(settings.tone, distance.value, n);

    // Distance and pan.
    // (Instrument mode: close, whatever the world.)
    if (c.instrument) distance.snap(0.0f);
    const float d = c.instrument ? distance.value : distance.next(travelDistance(effectiveDistance(world, c.distance), c.travelFar));
    const float p = pan.next(std::clamp(c.pan, -1.0f, 1.0f));
    for (auto& f : distanceLp) f.setCutoff(lerpExp(20000.0f, 1100.0f, std::pow(d, 1.6f)), rate);
    for (auto& f : percLp) f.setCutoff(lerpExp(20000.0f, 1100.0f, std::pow(d, 1.6f)), rate);
    const float near = lerp(1.0f, 0.3f, std::pow(d, 0.8f)), width = lerp(1.0f, 0.45f, d);
    const float right_ = std::max(p, 0.0f), left_ = std::max(-p, 0.0f);
    // The orchestration pass: a Place that does not lead an orchestrated section keeps less of
    // its space (World Space and reverb / delay sends), so several Places do not stack into mud.
    const float focusNow = focus.next(std::clamp(c.spaceFocus, 0.0f, 1.0f));
    send = c.instrument && !c.ambience ? 0.0f : current->spaceSend() * lerp(0.4f, 1.6f, d) * focusNow;
    float target_ = audible ? dbToGain(std::clamp(c.levelDb, -60.0f, 6.0f) + c.trimDb + intensityDb(currentType, world.intensity)) * near * travelGain(c.travelFar) : 0.0f;
    if (placeGain != 1.0f) target_ *= placeGain;
    if (snapGain) {
        gain.snap(target_);
        snapGain = false;
    }
    const bool ducking = c.duckFrom < 1.0f || c.duckTo < 1.0f;
    // The sends follow the slot's gain and pan; far away, more of each instrument
    // reaches its reverb and delay.
    const float sendDistance = (1.0f + 0.5f * d) * focusNow;
    for (int i = 0; i < n; ++i) {
        fade = switching ? std::max(0.0f, fade - fadeStep) : std::min(1.0f, fade + fadeStep);
        const float g = gain.next(target_) * fade * fade * (ducking ? lerp(c.duckFrom, c.duckTo, float(i + 1) / float(n)) : 1.0f);
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
        if (percussionNow) {
            // The Drama percussion: the same level, distance and pan, without the fullness
            // (its low end is its own) and without the World Space (the engine adds it dry).
            const float pm = 0.5f * (finiteOr(percL[std::size_t(i)]) + finiteOr(percR[std::size_t(i)])),
                        ps = 0.5f * (finiteOr(percL[std::size_t(i)]) - finiteOr(percR[std::size_t(i)])) * width;
            const float pl = percLp[2].lp(percLp[0].lp(pm + ps)), pr = percLp[3].lp(percLp[1].lp(pm - ps));
            percL[std::size_t(i)] = (pl * (1.0f - right_) + pr * left_ * 0.7f) * g;
            percR[std::size_t(i)] = (pr * (1.0f - left_) + pl * right_ * 0.7f) * g;
        }
    }
    return true;
}

} // namespace wb
