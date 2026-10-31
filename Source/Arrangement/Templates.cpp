#include "Templates.h"
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

Arrangement makeTemplate(int which) {
    Arrangement a;
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
        default: break;
    }
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

