#include "Templates.h"
#include "Orchestra.h"
#include <cctype>
#include <string>
#include <vector>

namespace wb::arr {

namespace {
bool contains(const std::string& text, const char* word) { return text.find(word) != std::string::npos; }

std::string lower(const char* text) {
    std::string s(text != nullptr ? text : "");
    for (auto& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    while (!s.empty() && s.back() == ' ') s.pop_back();
    return s;
}

// A section's ramp, curve and colour for its kind.
struct KindLook {
    float start, end;
    uint8_t curve, colour;
};
constexpr std::array<KindLook, KindCount> looks { {
    { 0.22f, 0.35f, CurveLinear, 0 },   // intro
    { 0.4f, 0.78f, CurveEaseIn, 5 },    // build
    { 0.85f, 0.92f, CurveLinear, 2 },   // peak
    { 0.65f, 0.3f, CurveEaseOut, 3 },   // wind-down
    { 0.3f, 0.3f, CurveLinear, 7 },     // break
    { 0.3f, 0.08f, CurveLinear, 4 },    // outro
    { 0.5f, 0.5f, CurveLinear, 1 },     // loop
    { 0.55f, 0.62f, CurveSwell, 6 },    // bridge
} };

// A small deterministic choice from the World Seed.
struct Dice {
    uint32_t seed;
    bool coin(int a, int b, int c = 0) const { return (dsp::mix64((uint64_t(seed) << 24) ^ uint64_t(a * 977 + b * 131 + c * 17 + 5)) >> 33) & 1u; }
};

Section templateSection(const char* name, int bars, int repeats = 1) {
    auto s = makeSection(name, bars, 0);
    s.repeats = uint8_t(repeats);
    return s;
}
} // namespace

int roleOf(int type, int element) {
    // The Drama instruments: the drums and the ostinato drive, the hits accent.
    if (element == DramaDrums || element == DramaOstinato) return RoleRhythm;
    if (element == DramaHits) return RoleAccent;
    if (type <= NoPlace || type >= PlaceTypeCount || element < 0 || element >= placeElements) return RoleMelody;
    const auto& e = placeInfo(type).element[std::size_t(element)];
    const auto name = lower(e.name);
    if (e.icon == IconBass || contains(name, "bass")) return RoleBass;
    if (e.sustained) {
        switch (e.icon) {
            case IconWind: case IconCrowd: case IconSwarm: case IconWave: case IconSteam: return RoleTexture;
            default: return RoleBed;
        }
    }
    if (e.kind == Kit) {
        switch (e.icon) {
            case IconRiser: return RoleAccent;
            case IconSteam: return RoleTexture;
            default: return RoleRhythm;
        }
    }
    switch (e.icon) {
        case IconPulse: case IconGear: case IconPiston: case IconHandDrum: case IconShaker: case IconSnare: return RoleRhythm;
        case IconBell: case IconGong: case IconCrystal: case IconAnvil: case IconSiren: case IconAlarm: case IconPA: case IconBlip: case IconRadio:
        case IconJingle: case IconGlass: case IconBird: case IconDrip: case IconRiser: return RoleAccent;
        default: return RoleMelody;
    }
}

int kindOf(const Arrangement& a, int index) {
    if (index < 0 || index >= a.count) return KindLoop;
    const auto& s = a.sections[std::size_t(index)];
    const auto name = lower(s.nameText());
    if (contains(name, "intro") || contains(name, "approach") || contains(name, "mystery") || contains(name, "dawn")) return KindIntro;
    if (contains(name, "build") || contains(name, "tension") || contains(name, "rise") || contains(name, "rising")) return KindBuild;
    if (contains(name, "climax") || contains(name, "combat") || contains(name, "phase") || contains(name, "reveal") || contains(name, "boss")
        || contains(name, "battle") || contains(name, "chase") || contains(name, "peak"))
        return KindPeak;
    if (contains(name, "wind") || contains(name, "afterglow") || contains(name, "calm") || contains(name, "aftermath")) return KindCalm;
    if (contains(name, "break") || contains(name, "transition") || contains(name, "interlude")) return KindBreak;
    if (contains(name, "outro") || contains(name, "victory") || contains(name, "defeat") || contains(name, "ending") || contains(name, "finale"))
        return KindOutro;
    if (contains(name, "loop") || name == "a" || contains(name, "theme") || contains(name, "verse")) return KindLoop;
    if (contains(name, "bridge") || name == "b" || name == "c" || contains(name, "chorus")) return KindBridge;
    // A name of its own: where it sits and what its intensity does.
    const float mean = 0.5f * (s.intensityStart + s.intensityEnd), rise = s.intensityEnd - s.intensityStart;
    if (a.count > 1 && index == 0 && mean < 0.5f) return KindIntro;
    if (a.count > 1 && index == a.count - 1 && mean < 0.5f) return KindOutro;
    if (mean >= 0.72f) return KindPeak;
    if (rise >= 0.15f) return KindBuild;
    if (rise <= -0.15f) return KindCalm;
    return KindLoop;
}

namespace {
// The templates as they were before the Drama tier rework (short sections): the factory
// scenes from before it are built from them, so they sound exactly as they did.
std::vector<Section> legacySections(int which, Arrangement& a) {
    std::vector<Section> list;
    switch (std::clamp(which, 0, TemplateCount - 1)) {
        case ClassicArc:
            list = { templateSection("Intro", 8), templateSection("Build", 16), templateSection("Climax", 16), templateSection("Wind-down", 16),
                     templateSection("Outro", 8) };
            break;
        case ExplorationLoop:
            // The loop plays twice here; set it to Loop (endless) for a game's looping cue.
            list = { templateSection("Intro", 8), templateSection("Loop", 32, 2), templateSection("Outro", 8) };
            break;
        case Battle:
            list = { templateSection("Intro", 4), templateSection("Tension", 8), templateSection("Combat", 16), templateSection("Break", 8),
                     templateSection("Combat II", 16), templateSection("Victory", 4) };
            break;
        case BossFight:
            list = { templateSection("Approach", 8), templateSection("Phase 1", 16), templateSection("Transition", 4), templateSection("Phase 2", 16),
                     templateSection("Defeat", 8) };
            break;
        case TownTheme:
            list = { templateSection("A", 8), templateSection("B", 8), templateSection("A", 8), templateSection("C", 8) };
            a.end = EndLoop;
            a.loopTo = 0;
            break;
        case CinematicReveal:
            list = { templateSection("Mystery", 16), templateSection("Rise", 16), templateSection("Reveal", 8), templateSection("Afterglow", 16) };
            break;
        case Chase:
            list = { templateSection("Intro", 4), templateSection("Build", 8), templateSection("Chase", 16), templateSection("Break", 4),
                     templateSection("Build", 4), templateSection("Chase II", 16), templateSection("Outro", 4) };
            break;
        case Siege:
            list = { templateSection("Approach", 8), templateSection("Tension", 8), templateSection("Battle", 16), templateSection("Break", 8),
                     templateSection("Rising", 8), templateSection("Climax", 16), templateSection("Aftermath", 8) };
            break;
        case FinalStand:
            list = { templateSection("Intro", 8), templateSection("Build", 8), templateSection("Battle", 16), templateSection("Break", 4),
                     templateSection("Battle II", 16), templateSection("Climax", 8), templateSection("Victory", 8) };
            break;
        case Lament:
            list = { templateSection("Intro", 8), templateSection("Theme", 16), templateSection("Rise", 8), templateSection("Climax", 8),
                     templateSection("Wind-down", 8), templateSection("Outro", 8) };
            break;
        default: break;
    }
    return list;
}

// The templates for scoring, in proportions (the orchestration pass: bars at 120 BPM for a
// cue of about five minutes; makeTemplate fits them to the Length at the host's tempo):
// intros and endings about a tenth, build-ups that take their time, climaxes that last
// (a third or more); every section evolves inside itself.
std::vector<Section> scoringSections(int which, Arrangement& a) {
    std::vector<Section> list;
    switch (std::clamp(which, 0, TemplateCount - 1)) {
        case ClassicArc:
            list = { templateSection("Intro", 16), templateSection("Build", 32), templateSection("Climax", 56), templateSection("Wind-down", 24),
                     templateSection("Outro", 16) };
            break;
        case ExplorationLoop:
            // Set a loop to Loop (endless) for a game's looping cue.
            list = { templateSection("Intro", 16), templateSection("Loop", 48), templateSection("Bridge", 24), templateSection("Loop", 48),
                     templateSection("Outro", 16) };
            break;
        case Battle:
            list = { templateSection("Intro", 12), templateSection("Tension", 24), templateSection("Combat", 40), templateSection("Break", 16),
                     templateSection("Combat II", 48), templateSection("Victory", 12) };
            break;
        case BossFight:
            list = { templateSection("Approach", 16), templateSection("Phase 1", 40), templateSection("Transition", 12), templateSection("Phase 2", 56),
                     templateSection("Defeat", 16) };
            break;
        case TownTheme:
            list = { templateSection("A", 32), templateSection("B", 24), templateSection("A", 32), templateSection("C", 24) };
            a.end = EndLoop;
            a.loopTo = 0;
            break;
        case CinematicReveal:
            list = { templateSection("Mystery", 24), templateSection("Rise", 32), templateSection("Reveal", 48), templateSection("Afterglow", 24) };
            break;
        case Chase:
            list = { templateSection("Intro", 8), templateSection("Build", 24), templateSection("Chase", 40), templateSection("Break", 12),
                     templateSection("Build", 16), templateSection("Chase II", 40), templateSection("Outro", 12) };
            break;
        case Siege:
            list = { templateSection("Approach", 16), templateSection("Tension", 24), templateSection("Battle", 40), templateSection("Break", 16),
                     templateSection("Rising", 24), templateSection("Climax", 40), templateSection("Aftermath", 16) };
            break;
        case FinalStand:
            list = { templateSection("Intro", 12), templateSection("Build", 24), templateSection("Battle", 32), templateSection("Break", 12),
                     templateSection("Battle II", 40), templateSection("Climax", 32), templateSection("Victory", 16) };
            break;
        case Lament:
            list = { templateSection("Intro", 16), templateSection("Theme", 32), templateSection("Rise", 24), templateSection("Climax", 40),
                     templateSection("Wind-down", 24), templateSection("Outro", 16) };
            break;
        default: break;
    }
    for (auto& s : list) s.evolve = true;
    return list;
}
} // namespace

int scoringBars(int kind) {
    switch (kind) {
        case KindIntro: case KindBreak: case KindOutro: return 8;
        case KindPeak: case KindLoop: return 32;
        default: return 16;
    }
}

Arrangement makeTemplate(int which, bool legacy) {
    Arrangement a;
    const auto list = legacy ? legacySections(which, a) : scoringSections(which, a);
    for (const auto& s : list) a.sections[std::size_t(a.count++)] = s;
    for (int i = 0; i < a.count; ++i) {
        auto& s = a.sections[std::size_t(i)];
        const auto& look = looks[std::size_t(kindOf(a, i))];
        s.colour = look.colour;
        s.intensityStart = look.start;
        s.intensityEnd = look.end;
        s.curve = look.curve;
    }
    return a;
}

Arrangement autoArrange(const Arrangement& in, const std::array<int, placeSlots>& types, uint32_t seed) {
    // For scoring: every section evolving inside itself, its lengths as they are (the
    // orchestration pass: the Length control fits them before, by time).
    Arrangement a = in;
    for (int i = 0; i < a.count; ++i) a.sections[std::size_t(i)].evolve = true;
    a = autoArrangeLegacy(a, types, seed);
    // The Drama instruments: in every section (their tier decides whether they play), the
    // drums' Fill before a fight.
    for (int i = 0; i < a.count; ++i) {
        auto& s = a.sections[std::size_t(i)];
        const int next = a.nextOf(i);
        const bool intoFight = next >= 0 && next != i && kindOf(a, next) == KindPeak && kindOf(a, i) != KindPeak;
        for (int slot = 0; slot < placeSlots; ++slot)
            for (int e = placeElements; e < slotElements; ++e) {
                auto& c = s.cells[std::size_t(slot)][std::size_t(e)];
                c = Cell {};
                c.fill = e == DramaDrums && intoFight;
            }
    }
    // The orchestration pass: scored, not every instrument of every Place at once.
    return orchestrate(a, types, seed);
}

Arrangement autoArrangeLegacy(const Arrangement& in, const std::array<int, placeSlots>& types, uint32_t seed) {
    Arrangement a = in;
    const Dice dice { seed };
    // The active slots, in order: the first carries the intro alone; the others join a step later.
    std::array<int, placeSlots> order {};
    order.fill(-1);
    int active = 0;
    for (int slot = 0; slot < placeSlots; ++slot)
        if (types[std::size_t(slot)] != NoPlace) order[std::size_t(slot)] = active++;

    std::array<int, maxSections> kinds {};
    for (int i = 0; i < a.count; ++i) kinds[std::size_t(i)] = kindOf(a, i);
    int peaks = 0;

    for (int i = 0; i < a.count; ++i) {
        auto& s = a.sections[std::size_t(i)];
        const int kind = kinds[std::size_t(i)];
        const bool last = i == a.count - 1;
        const int next = a.nextOf(i);
        const int nextKind = next >= 0 ? kinds[std::size_t(next)] : -1;

        // Score Intensity: each kind's arc; later peaks climb higher.
        const auto& look = looks[std::size_t(kind)];
        s.intensityStart = look.start;
        s.intensityEnd = look.end;
        s.curve = look.curve;
        if (kind == KindPeak) {
            s.intensityStart = std::min(1.0f, look.start + 0.05f * float(peaks));
            s.intensityEnd = std::min(1.0f, look.end + 0.05f * float(peaks));
            ++peaks;
        }

        // The grid.
        for (int slot = 0; slot < placeSlots; ++slot) {
            const int type = types[std::size_t(slot)];
            const int place = order[std::size_t(slot)];
            bool anyOn = false;
            for (int e = 0; e < placeElements; ++e) {
                auto& c = s.cells[std::size_t(slot)][std::size_t(e)];
                c = Cell {};
                if (type == NoPlace) continue;
                const int role = roleOf(type, e);
                // Places after the first come in one step later in an intro or build-up.
                int k = kind;
                if (place > 0 && kind == KindIntro) k = -1;
                if (place > 0 && kind == KindBuild && i > 0 && kinds[std::size_t(i - 1)] != KindPeak) k = KindIntro;
                const bool flip = dice.coin(i, slot, e);
                uint8_t entry = EntryOff, variant = VariantA;
                switch (k) {
                    case -1: break;
                    case KindIntro:
                        if (role == RoleBed || role == RoleTexture) entry = EntryOn;
                        else if (role == RoleMelody && s.bars >= 8 && flip) {
                            entry = EntryFadeIn;
                            variant = VariantC;
                        }
                        break;
                    case KindBuild:
                        if (role <= RoleBass) entry = EntryOn;
                        else if (role == RoleRhythm) entry = EntryHalfway;
                        else if (role == RoleMelody) entry = flip ? EntryFadeIn : EntryHalfway;
                        else entry = s.bars >= 8 && flip ? EntryHalfway : EntryOff;
                        if (role == RoleAccent) variant = VariantC;
                        break;
                    case KindPeak:
                        entry = EntryOn;
                        if (role == RoleRhythm || role == RoleAccent) variant = VariantB;
                        else if (role == RoleMelody || role == RoleBass) variant = flip ? VariantB : VariantA;
                        break;
                    case KindCalm:
                        if (role <= RoleBass) entry = EntryOn;
                        else if (role == RoleRhythm) entry = EntryFadeOut;
                        else if (role == RoleMelody) {
                            entry = EntryOn;
                            variant = VariantC;
                        }
                        break;
                    case KindBreak:
                        if (role == RoleBed || role == RoleTexture) entry = EntryOn;
                        else if (role == RoleMelody && flip) {
                            entry = EntryOn;
                            variant = VariantC;
                        }
                        break;
                    case KindOutro:
                        if (role == RoleBed) entry = last && a.end == EndStop ? EntryFadeOut : EntryOn;
                        else if (role == RoleTexture) entry = EntryFadeOut;
                        else if (role == RoleMelody && flip) {
                            entry = EntryFadeOut;
                            variant = VariantC;
                        }
                        break;
                    case KindLoop:
                        entry = EntryOn;
                        if (role == RoleAccent) variant = VariantC;
                        break;
                    case KindBridge:
                        entry = role == RoleTexture && flip ? EntryOff : EntryOn;
                        if (role == RoleMelody || role == RoleBass) variant = (i % 2 == 0) != flip ? VariantB : VariantC;
                        else if (role == RoleRhythm) variant = VariantC;
                        else if (role == RoleAccent) entry = flip ? EntryOn : EntryOff;
                        break;
                    default: break;
                }
                c.entry = entry;
                c.variant = variant;
                // Rhythm parts play their Fills before a bigger section.
                c.fill = role == RoleRhythm && entry != EntryOff && entry != EntryFadeOut && a.changeAfter(i)
                         && (nextKind == KindPeak || nextKind == KindLoop || nextKind == KindBridge) && !last;
                anyOn |= entry != EntryOff;
            }
            // The first Place always has something playing in an intro.
            if (type != NoPlace && place == 0 && !anyOn) {
                int best = 0;
                for (int e = 1; e < placeElements; ++e)
                    if (roleOf(type, e) < roleOf(type, best)) best = e;
                s.cells[std::size_t(slot)][std::size_t(best)].entry = EntryOn;
            }
        }

        // The transition into the next section.
        const bool flip = dice.coin(i, 99, 1);
        uint8_t t = TransCut, length = 1;
        if (next < 0 || next == i) {
            t = TransCut;
        } else if (nextKind == KindIntro) {
            t = TransCrossfade;
        } else {
            switch (kind) {
                case KindIntro:
                    if (nextKind == KindBuild) t = flip ? TransReverse : TransCrossfade;
                    else {
                        t = flip ? TransRiser : TransReverse;
                        length = t == TransRiser ? 2 : 1;
                    }
                    break;
                case KindBuild:
                    if (nextKind == KindPeak) {
                        t = flip ? TransRiser : TransDrumFill;
                        length = t == TransRiser ? 2 : 1;
                    } else t = TransCrossfade;
                    break;
                case KindPeak:
                    if (nextKind == KindPeak) t = flip ? TransDrumFill : TransDrop;
                    else if (nextKind == KindCalm) {
                        t = flip ? TransCrossfade : TransFilter;
                        length = t == TransCrossfade ? 2 : 1;
                    } else if (nextKind == KindBreak) {
                        t = flip ? TransDrop : TransFilter;
                        length = t == TransDrop ? 2 : 1;
                    } else if (nextKind == KindOutro) {
                        t = flip ? TransFilter : TransCrossfade;
                        length = 2;
                    } else t = TransCrossfade;
                    break;
                case KindBreak:
                    t = nextKind == KindPeak ? (flip ? TransRiser : TransDrumFill) : TransCrossfade;
                    break;
                case KindCalm:
                    t = TransCrossfade;
                    length = 2;
                    break;
                case KindLoop:
                case KindBridge:
                    if (nextKind == KindOutro) {
                        t = TransCrossfade;
                        length = 2;
                    } else if (nextKind == KindPeak) t = TransRiser;
                    else t = flip ? TransDrumFill : TransCut;
                    break;
                default: t = TransCrossfade; break;
            }
        }
        // A transition no longer than the section.
        length = uint8_t(std::min<int>(length, std::max(1, t == TransDrop ? 4 : int(s.bars))));
        s.transition = t;
        s.transitionLength = length;
    }
    return a;
}

//==============================================================================
// The Drama tier: arcs.

int phraseShiftOf(const Section& s, int kind, int k, int phrases) {
    const bool falling = kind == KindCalm || kind == KindOutro || s.intensityEnd < s.intensityStart - 0.05f;
    if (phrases < 2) return 0;
    if (!falling) return k >= phrases / 2 ? 1 : (k == 0 && phrases >= 3 ? -1 : 0);
    return k >= phrases / 2 ? -1 : 0;
}

void dramatize(Arrangement& a, uint32_t seed) {
    const Dice dice { seed ^ 0xD4A3u };
    std::array<int, maxSections> kinds {};
    for (int i = 0; i < a.count; ++i) kinds[std::size_t(i)] = kindOf(a, i);
    int peaks = 0;
    for (int i = 0; i < a.count; ++i) {
        auto& s = a.sections[std::size_t(i)];
        const int kind = kinds[std::size_t(i)];
        const int prevKind = i > 0 ? kinds[std::size_t(i - 1)] : -1;
        switch (kind) {
            case KindIntro: s.drama = int8_t(drama::Calm); break;
            case KindBuild:
                // A build-up after a break climbs back from Tension into the fight.
                s.drama = int8_t(drama::Tension);
                s.intensityStart = 0.56f;
                s.intensityEnd = 0.72f;
                break;
            case KindPeak: {
                // The first climax drives (Action), the later ones (and one a build-up
                // has led into) go all the way.
                const bool top = peaks > 0 || prevKind == KindBuild;
                s.drama = int8_t(top ? drama::Climax : drama::Action);
                s.intensityStart = top ? 0.9f : 0.78f;
                s.intensityEnd = top ? 1.0f : 0.88f;
                s.curve = CurveLinear;
                ++peaks;
                break;
            }
            case KindBreak:
                s.drama = int8_t(drama::Calm);
                s.intensityStart = s.intensityEnd = 0.4f;
                break;
            case KindCalm: s.drama = int8_t(drama::Tension); break;
            case KindOutro: s.drama = int8_t(drama::Calm); break;
            case KindLoop: s.drama = int8_t(drama::Tension); break;
            case KindBridge: s.drama = int8_t(drama::Tension); break;
            default: break;
        }
    }
    // The transitions a score changes gear with.
    for (int i = 0; i < a.count; ++i) {
        auto& s = a.sections[std::size_t(i)];
        const int next = a.nextOf(i);
        if (next < 0 || next == i) continue;
        const int kind = kinds[std::size_t(i)], nextKind = kinds[std::size_t(next)];
        const int tier = s.drama, nextTier = a.sections[std::size_t(next)].drama;
        uint8_t t = s.transition, length = s.transitionLength;
        if (nextKind == KindPeak && kind != KindPeak) {
            // Into the fight: a Riser out of a build-up, a Drum fill out of a break or a calm.
            if (kind == KindBuild || kind == KindIntro) {
                t = TransRiser;
                length = 2;
            } else {
                t = TransDrumFill;
                length = 1;
            }
        } else if (kind == KindPeak && nextKind == KindPeak) {
            t = TransDrop;
            length = 2;
        } else if (kind == KindPeak && (nextKind == KindBreak || nextKind == KindBuild)) {
            // The drop: the fight falls away for a beat or two before the next build.
            t = TransDrop;
            length = uint8_t(dice.coin(i, 7) ? 2 : 1);
        } else if (kind == KindBreak && nextKind == KindBuild) {
            t = TransCrossfade;
            length = 1;
        } else if (tier >= drama::Action && nextTier <= drama::Tension && nextKind != KindBreak) {
            t = TransFilter;
            length = 1;
        }
        length = uint8_t(std::min<int>(length, std::max(1, t == TransDrop ? 4 : int(s.bars))));
        s.transition = t;
        s.transitionLength = length;
    }
}

//==============================================================================
// The Drama tier rework: drama and evolution shape what the arrangement plays.

float gearOf(const Section& s, float sectionIntensity, const Shaping& sh) {
    if (sh.drama <= 0.0f) return -1.0f;
    float tier = 0;
    if (s.drama >= 0) {
        tier = float(std::min<int>(s.drama, drama::Climax));
    } else {
        // A section without a tier: from its Score Intensity, the middle neutral (Tension: the
        // arrangement as written), Calm at 0, Climax at the top.
        const float i = dsp::clamp01(sectionIntensity + sh.intensityOffset);
        tier = i <= 0.5f ? 2.0f * i : 1.0f + 2.0f * drama::amountFor(i, 0.5f, 1.0f);
    }
    // The Drama meter as an offset: 100 % plays the section's own tier, every 50 % below it
    // one tier lower.
    return std::clamp(tier + 2.0f * (std::min(sh.drama, 1.0f) - 1.0f), 0.0f, 3.0f);
}

namespace {
// Where an instrument comes in over a section's first bars (a fraction of the section, by
// its role) and how quickly a kind of section brings them in.
constexpr std::array<float, RoleCount> entryFraction { 0.0f, 1.0f / 16.0f, 1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 4.0f, 3.0f / 8.0f };
constexpr std::array<float, KindCount> entrySpeed { 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.75f, 0.75f };
} // namespace

void shape(State& st, const Arrangement& a, const Shaping& sh) {
    if (!st.active || st.at.section < 0 || st.at.section >= a.count || st.at.ended || st.ringOut) return;
    const int index = st.at.section;
    const auto& s = a.sections[std::size_t(index)];
    const bool dramatic = sh.drama > 0.0f;
    if (!dramatic && !s.evolve && !s.orchestrated) return;
    const int kind = kindOf(a, index);
    const double bars = double(std::max<int>(s.bars, 1));
    const double b = st.at.bar;   // bars into this pass
    const double beatInBar = (b - std::floor(b)) * double(beatsPerBar);
    const bool dropping = st.transition == TransDrop && st.transitionT >= 0.0f;
    const bool crossfading = st.transition == TransCrossfade && st.transitionT >= 0.0f;
    // Whether an instrument rests in a cell (its entry Off) and was playing in the section before.
    const int previous = index > 0 ? index - 1 : (st.at.looped ? a.count - 1 : -1);
    const auto playedBefore = [&](int slot, int e) {
        return previous >= 0 && previous != index && a.sections[std::size_t(previous)].cells[std::size_t(slot)][std::size_t(e)].entry != EntryOff;
    };

    // Drama: the section's gear.
    float d = 0;
    if (dramatic) {
        const float sectionIntensity = st.intensity;
        const float gear = gearOf(s, sectionIntensity, sh);
        // A tiered section's Drama instruments and harmony play its gear; a section without a
        // tier keeps them on Score Intensity, as before.
        if (s.drama >= 0) {
            st.gear = gear;
            st.drama = std::clamp(int(std::floor(gear + 0.5f)), 0, int(drama::Climax));
        }
        // How strongly the gear reshapes the arrangement: fading in over the first 25 % of the meter.
        const float weight = std::min(1.0f, sh.drama / 0.25f);
        d = weight * (gear - 1.0f);
        st.intensity = dsp::clamp01(st.intensity + 0.06f * d);
    }
    // Evolve: the phrases (8 bars, 16 in long sections) and where this one stands.
    const int phrase = s.bars >= 48 ? 16 : 8;
    const int phrases = s.evolve ? std::max(1, int(s.bars) / phrase) : 1;
    const int k = std::min(phrases - 1, int(std::floor(b / double(phrase))));
    const bool falling = kind == KindCalm || kind == KindOutro || s.intensityEnd < s.intensityStart - 0.05f;
    const int phraseShift = s.evolve ? phraseShiftOf(s, kind, k, phrases) : 0;
    const bool phraseFill = s.evolve && phrases >= 2 && k < phrases - 1 && b - double(k * phrase) >= double(phrase) - 0.5;
    // Rising across the section (from a little below its ramp to a little above, so even a
    // climax at the top has room to grow); the Drama instruments play harder as it goes.
    const float through = float(std::clamp(b / bars, 0.0, 1.0));
    if (s.evolve && !falling) st.intensity = dsp::clamp01(dsp::clamp01(st.intensity) - 0.14f * (1.0f - through) + 0.04f * through);
    if (s.evolve) st.heat = falling ? 1.0f - through : through;
    if (s.evolve && !falling) st.swellDb = -2.5f * (1.0f - through);
    st.makeUpDb = s.orchestrated ? std::clamp(s.makeUpDb, 0.0f, 6.0f) : 0.0f;

    // The orchestration pass: an orchestrated section keeps to its voices and its budget.
    const bool orchestra = s.orchestrated;
    std::array<int, placeSlots> rankOf {};
    int places = 0;
    for (int slot = 0; slot < placeSlots; ++slot) rankOf[std::size_t(slot)] = sh.types[std::size_t(slot)] != NoPlace ? places++ : -1;
    std::array<int, VoiceCount> used {};
    int parts = 0;
    const Budget budget = budgetOf(kind);
    // The build-up's parts, in the order they come in across the section (pads and the
    // environment first, then the bass, the ostinato, the kit, colour, the lead, the hits).
    std::array<std::array<int8_t, slotElements>, placeSlots> order {};
    int entering = 0;
    if (orchestra) {
        for (int slot = 0; slot < placeSlots; ++slot) st.focus[std::size_t(slot)] = s.lead >= 0 && places > 1 && slot != s.lead ? 0.5f : 1.0f;
        const int tierNow = st.drama >= 0 ? st.drama : sectionTier(a, index, sh);
        const bool dramaNow = dramatic && tierNow > drama::Calm;
        // While the drama plays, the Drama drums take over from a Place's kit and the Drama
        // ostinato from a Place's ostinato: one kit, one ostinato.
        bool drums = false, ostinato = false;
        for (int slot = 0; slot < placeSlots && dramaNow; ++slot) {
            const int type = sh.types[std::size_t(slot)];
            if (type == NoPlace) continue;
            drums |= rankOf[std::size_t(slot)] == 0 && st.gain[std::size_t(slot)][std::size_t(DramaDrums)] > 0.0f && patternInfo(type, DramaDrums).kind != NoPattern;
            ostinato |= rankOf[std::size_t(slot)] <= 1 && st.gain[std::size_t(slot)][std::size_t(DramaOstinato)] > 0.0f && patternInfo(type, DramaOstinato).kind != NoPattern;
        }
        for (int slot = 0; slot < placeSlots; ++slot) {
            const int type = sh.types[std::size_t(slot)];
            if (type == NoPlace) continue;
            for (int e = 0; e < slotElements; ++e) {
                auto& g = st.gain[std::size_t(slot)][std::size_t(e)];
                if (g <= 0.0f || patternInfo(type, e).kind == NoPattern) continue;
                const int voice = voiceOf(type, e);
                if (!isDramaElement(e) && ((drums && voice == VoiceKit) || (ostinato && voice == VoiceOstinato))) {
                    g = 0.0f;
                    continue;
                }
                if (isDramaElement(e) && (!dramaNow || (e == DramaDrums && rankOf[std::size_t(slot)] > 0) || (e == DramaOstinato && rankOf[std::size_t(slot)] > 1))) continue;
                ++used[std::size_t(voice)];
                ++parts;
            }
        }
        if (s.evolve && !falling && st.at.pass == 0) {
            static constexpr std::array<int, VoiceCount> priority { 6, 2, 4, 3, 0, 1, 5, 7 };
            for (int p = 0; p < VoiceCount; ++p)
                for (int slot = 0; slot < placeSlots; ++slot) {
                    const int type = sh.types[std::size_t(slot)];
                    if (type == NoPlace) continue;
                    for (int e = 0; e < slotElements; ++e)
                        if (priority[std::size_t(voiceOf(type, e))] == p && s.cells[std::size_t(slot)][std::size_t(e)].entry != EntryOff && !playedBefore(slot, e)
                            && patternInfo(type, e).kind != NoPattern)
                            order[std::size_t(slot)][std::size_t(e)] = int8_t(1 + entering++);
                }
        }
    }

    for (int slot = 0; slot < placeSlots; ++slot) {
        const int type = sh.types[std::size_t(slot)];
        if (type == NoPlace) continue;
        auto& gain = st.gain[std::size_t(slot)];
        auto& variant = st.variant[std::size_t(slot)];
        auto& clip = st.clip[std::size_t(slot)];
        auto& doubling = st.doubling[std::size_t(slot)];
        // The instrument the Place keeps in the thinnest section (its lowest role playing).
        int keep = -1;
        for (int e = 0; e < placeElements; ++e)
            if (gain[std::size_t(e)] > 0.0f && patternInfo(type, e).kind != NoPattern && (keep < 0 || roleOf(type, e) < roleOf(type, keep))) keep = e;
        for (int e = 0; e < slotElements; ++e) {
            const auto ke = std::size_t(e);
            if (patternInfo(type, e).kind == NoPattern) continue;
            const auto& cell = s.cells[std::size_t(slot)][ke];
            const int role = roleOf(type, e);
            const bool drama = isDramaElement(e);
            const bool rhythm = role == RoleRhythm;
            int shift = 0;
            if (dramatic && !drama) {
                if (d < 0.0f) {
                    // Calm: the accents and the rhythm leave, then the melody; the bass turns sparse.
                    if (e != keep && ((role == RoleAccent && d <= -0.25f) || (rhythm && d <= -0.5f) || (role == RoleMelody && d <= -0.75f)) && gain[ke] > 0.0f) {
                        // (Orchestrated: never below the section's budget.)
                        if (!orchestra) gain[ke] = 0.0f;
                        else if (parts > budget.low) {
                            gain[ke] = 0.0f;
                            --parts;
                            --used[std::size_t(voiceOf(type, e))];
                        }
                    }
                    if ((role == RoleMelody && d <= -0.5f) || (role == RoleBass && d <= -0.75f)) shift = -1;
                } else {
                    // Dramatic: the instruments resting enter in order of their role, the
                    // busier variants, the rhythm section driving, the parts doubling.
                    static constexpr std::array<float, RoleCount> enterAt { 0.25f, 1.0f, 0.25f, 0.5f, 0.75f, 1.25f };
                    if (!dropping && !crossfading && cell.entry != EntryOn && cell.entry != EntryFadeOut && d >= enterAt[std::size_t(role)]
                        && (cell.entry == EntryOff || d >= 1.0f)) {
                        // (Orchestrated: only into a free voice, within the section's budget.)
                        const int voice = voiceOf(type, e);
                        if (!orchestra) gain[ke] = 1.0f;
                        else if (gain[ke] <= 0.0f && used[std::size_t(voice)] < voiceLimit[std::size_t(voice)] && parts < budget.high) {
                            gain[ke] = 1.0f;
                            ++used[std::size_t(voice)];
                            ++parts;
                        }
                    }
                    // One gear busier: the rhythm section first, then the bass, the melody and the
                    // rest (a sparse part two at a climax); the phrases move on top.
                    static constexpr std::array<float, RoleCount> busierAt { 1.5f, 1.5f, 1.0f, 0.5f, 1.25f, 1.5f };
                    if (d >= busierAt[std::size_t(role)]) shift = 1;
                    if (d >= 1.75f && variant[ke] == VariantC) shift = 2;
                    const auto& spec = elementSpec(type, e);
                    if (d >= 1.5f && spec.kind == Melodic && !spec.sustained && drama::doubles(type, e)) doubling[ke] = int8_t(role == RoleBass ? -1 : (role == RoleMelody ? 1 : 0));
                }
            }
            // Evolve: entrances over the first bars (first pass), exits towards a calm end.
            if (s.evolve && gain[ke] > 0.0f) {
                if (!falling && st.at.pass == 0 && !playedBefore(slot, e)) {
                    double entry = bars * double(entryFraction[std::size_t(role)] * entrySpeed[std::size_t(std::clamp(kind, 0, KindCount - 1))]);
                    entry = s.bars >= 16 ? std::floor(entry / 2.0) * 2.0 : std::floor(entry);
                    // Orchestrated: one by one across the section (a build-up over 80 % of its
                    // length, an intro or a climax half of it, a loop or a bridge 40 %), on
                    // four-bar lines.
                    const int j = order[std::size_t(slot)][ke];
                    if (orchestra && entering > 0) {
                        const double span = kind == KindBuild ? 0.8 : (kind == KindIntro || kind == KindPeak ? 0.5 : 0.4);
                        const double at = bars * span * double(j > 0 ? j - 1 : entering) / double(entering);
                        const double grid = s.bars >= 16 ? 4.0 : 1.0;
                        entry = std::floor(at / grid) * grid;
                    }
                    if (b < entry) gain[ke] = 0.0f;
                }
                if (falling && st.at.lastPass && e != keep && !drama) {
                    const double exit = role == RoleAccent ? 0.5 : (rhythm ? 0.625 : (role == RoleMelody ? 0.75 : 2.0));
                    if (b >= exit * bars) gain[ke] = 0.0f;
                }
            }
            if (variant[ke] == VariantFill) continue;
            // The variants change gear (linked cells; a clip carries its own).
            if (clip[ke] == nullptr) shift += phraseShift;
            if (shift != 0 && clip[ke] == nullptr) variant[ke] = gearVariant(std::clamp(variantGear(variant[ke]) + shift, 0, 2));
            // Fills: on the phrase lines, and every four bars where the rhythm section drives.
            const bool driveFill = dramatic && d >= 1.0f && rhythm && std::fmod(std::floor(b), 4.0) == 3.0 && beatInBar >= 2.0;
            if ((rhythm && phraseFill) || driveFill) {
                if (gain[ke] > 0.0f) {
                    variant[ke] = uint8_t(VariantFill);
                    clip[ke] = nullptr;
                }
            }
        }
    }
}

int sectionTier(const Arrangement& a, int k, const Shaping& sh) {
    if (k < 0 || k >= a.count) return drama::Calm;
    const auto& sec = a.sections[std::size_t(k)];
    const float intensity = 0.5f * (sec.intensityStart + sec.intensityEnd);
    if (sec.drama >= 0) return std::clamp(int(std::floor(gearOf(sec, intensity, sh) + 0.5f)), 0, int(drama::Climax));
    return drama::tierFor(drama::amountFor(dsp::clamp01(intensity + sh.intensityOffset), 0.5f, sh.drama));
}

//==============================================================================
// Milestone 16: section clips.

namespace {
float unit(uint64_t key, int a, int b) { return dsp::unipolar(dsp::mix64(key ^ (uint64_t(uint32_t(a)) << 32) ^ uint64_t(uint32_t(b) * 0x9E3779B9u))); }
} // namespace

void shapeClip(pat::Clip& c, int kind, int role, bool kit, int scaleSize, uint64_t key, bool runOut) {
    const int length = std::max<int>(c.lengthSteps, 1);
    const int perBar = pat::stepsPerBar(c.resolution);
    const int perBeat = std::max(1, perBar / 4);
    const int bars = std::max(1, length / std::max(perBar, 1));
    const bool melodic = !kit;
    const pat::Clip in = c;
    c.clear();
    const auto add = [&](pat::Note n, float velocity) {
        n.velocity = uint8_t(std::clamp(int(std::lround(float(n.velocity) * velocity)), 1, 127));
        n.length = int16_t(std::clamp<int>(n.length, 1, length - n.step));
        c.add(n);
    };
    // The notes that start a beat (or a half bar): what a sparse part keeps first.
    const auto strong = [&](const pat::Note& n) { return n.step % std::max(1, perBar / 2) == 0; };
    const auto onBeat = [&](const pat::Note& n) { return n.step % perBeat == 0; };
    for (int i = 0; i < in.count; ++i) {
        auto n = in.notes[std::size_t(i)];
        const float t = float(n.step) / float(length);   // 0..1 across the section
        const float u = unit(key, i, kind);
        const bool keepAlways = n.locked();
        switch (kind) {
            case KindIntro: {
                // Sparse and lower: the strong notes and a few others, an octave down, soft,
                // held longer; a little louder towards the end.
                if (!keepAlways && !(strong(n) || u < 0.2f)) break;
                if (melodic && role != RoleBass && role != RoleBed) n.degree = int8_t(std::clamp(int(n.degree) - scaleSize, -40, 40));
                n.length = int16_t(n.length * 2);
                add(n, 0.62f + 0.18f * t);
                break;
            }
            case KindBuild: {
                if (runOut && n.step >= length - perBar && role != RoleRhythm && melodic) break;   // the last bar is the run
                // Thin at first, fuller as it goes; climbing a scale step every quarter of
                // the section; louder; and in its last third the rhythm tightens: long
                // notes repeat on eighths, then sixteenths in the last bar.
                const float keep = 0.4f + 0.6f * t;
                if (!keepAlways && !onBeat(n) && u > keep) break;
                if (!keepAlways && onBeat(n) && t < 0.25f && u > 0.75f) break;
                if (melodic && role != RoleBed) n.degree = int8_t(std::clamp(int(n.degree) + int(std::floor(t * 4.0f)), -40, 40));
                const float velocity = 0.62f + 0.5f * t;
                const int lastBar = length - perBar;
                const int sub = n.step >= lastBar ? std::max(1, perBar / 16) : std::max(1, perBar / 8);
                if (t > 0.66f && role != RoleBed && n.length >= 2 * sub) {
                    for (int at = 0; at < n.length; at += sub) {
                        auto r = n;
                        r.step = int16_t(n.step + at);
                        r.length = int16_t(sub);
                        if (r.step >= length) break;
                        add(r, velocity * (at == 0 ? 1.0f : 0.8f));
                    }
                } else {
                    add(n, velocity);
                }
                break;
            }
            case KindPeak: {
                // Full: every note, louder; the melody doubled an octave up on its strong notes.
                add(n, 1.15f);
                if (melodic && role == RoleMelody && strong(n)) {
                    auto up = n;
                    up.degree = int8_t(std::clamp(int(n.degree) + scaleSize, -40, 40));
                    add(up, 0.8f);
                }
                break;
            }
            case KindCalm:
            case KindOutro: {
                // Thinning out: fewer notes as it goes, softer, held longer (an outro thinner
                // and lower still).
                const bool outro = kind == KindOutro;
                const float keep = outro ? 0.7f - 0.5f * t : 1.0f - 0.6f * t;
                if (!keepAlways && !(strong(n) && t < 0.5f) && u > keep) break;
                if (outro && melodic && role == RoleMelody) n.degree = int8_t(std::clamp(int(n.degree) - scaleSize, -40, 40));
                n.length = int16_t(std::lround(double(n.length) * (outro ? 2.0 : 1.5)));
                add(n, (outro ? 0.8f : 1.0f) - (outro ? 0.4f : 0.35f) * t);
                break;
            }
            case KindBreak: {
                if (!keepAlways && !strong(n) && u > 0.3f) break;
                add(n, 0.75f);
                break;
            }
            default: add(n, 1.0f); break;
        }
    }
    if (kind == KindBuild && runOut && melodic && role != RoleRhythm && role != RoleBed) {
        // The run into the next section: eighths (a melody: sixteenths) climbing the scale
        // from the section's last note, getting louder.
        int degree = 0;
        for (int i = c.count - 1; i >= 0; --i) {
            degree = c.notes[std::size_t(i)].degree;
            break;
        }
        const int step = std::max(1, perBar / (role == RoleMelody ? 16 : 8));
        const int from = length - perBar;
        int k = 0;
        for (int at = from; at < length; at += step, ++k) {
            pat::Note n;
            n.step = int16_t(at);
            n.length = int16_t(step);
            n.degree = int8_t(std::clamp(degree + 1 + k / (role == RoleMelody ? 2 : 1), -40, 40));
            n.velocity = uint8_t(std::clamp(70 + 50 * k * step / perBar, 1, 127));
            c.add(n);
        }
    }
    (void)bars;
}

Arrangement autoArrange(const Arrangement& in, const std::array<int, placeSlots>& types, uint32_t seed, const ClipSource& source) {
    Arrangement a = autoArrange(in, types, seed);
    if (!source.pattern) return a;
    for (int i = 0; i < a.count; ++i) {
        auto& s = a.sections[std::size_t(i)];
        const int kind = kindOf(a, i);
        if (kind == KindLoop || kind == KindBridge) continue;
        const int next = a.nextOf(i);
        const bool runOut = next >= 0 && next != i && kindOf(a, next) == KindPeak;
        for (int slot = 0; slot < placeSlots; ++slot) {
            const int type = types[std::size_t(slot)];
            if (type == NoPlace) continue;
            for (int e = 0; e < placeElements; ++e) {
                auto& cell = s.cells[std::size_t(slot)][std::size_t(e)];
                if (cell.entry == EntryOff || patternInfo(type, e).kind == NoPattern) continue;
                bool edited = false;
                const pat::Pattern* p = source.pattern(slot, e, std::min<int>(cell.variant, VariantC), edited);
                if (p == nullptr || p->count <= 0) continue;
                auto clip = clipFromPattern(*p, a.startOf(i), s.bars, edited);
                // An evolving section's clip changes variant on its phrases, as a linked
                // instrument would (the Drama tier rework).
                if (s.evolve) {
                    const int phrase = s.bars >= 48 ? 16 : 8, phrases = std::max(1, int(s.bars) / phrase);
                    for (int k = 0; k < phrases && phrases >= 2; ++k) {
                        const int v = gearVariant(std::clamp(variantGear(cell.variant) + phraseShiftOf(s, kind, k, phrases), 0, 2));
                        if (v == std::min<int>(cell.variant, VariantC)) continue;
                        bool otherEdited = false;
                        const pat::Pattern* q = source.pattern(slot, e, v, otherEdited);
                        if (q == nullptr) continue;
                        const auto part = clipFromPattern(*q, a.startOf(i) + k * phrase, k == phrases - 1 ? int(s.bars) - k * phrase : phrase, otherEdited);
                        // Replace the phrase's notes.
                        const int perBar = pat::stepsPerBar(clip->resolution);
                        if (pat::stepsPerBar(part->resolution) != perBar) continue;
                        const int from = k * phrase * perBar, to = from + part->lengthSteps;
                        pat::Clip merged = *clip;
                        merged.clear();
                        for (int n = 0; n < clip->count; ++n)
                            if (clip->notes[std::size_t(n)].step < from || clip->notes[std::size_t(n)].step >= to) merged.add(clip->notes[std::size_t(n)]);
                        for (int n = 0; n < part->count; ++n) {
                            auto x = part->notes[std::size_t(n)];
                            x.step = int16_t(x.step + from);
                            merged.add(x);
                        }
                        *clip = merged;
                    }
                }
                const uint64_t key = dsp::mix64((uint64_t(seed) << 24) ^ (uint64_t(i) << 12) ^ (uint64_t(slot) << 6) ^ uint64_t(e) ^ 0xC11Bull);
                shapeClip(*clip, kind, roleOf(type, e), patternInfo(type, e).kind == Kit, source.scaleSize, key, runOut && !cell.fill);
                if (clip->count <= 0) continue;   // nothing left: the variant plays (linked)
                cell.clip = clip;
            }
        }
    }
    return a;
}

} // namespace wb::arr

