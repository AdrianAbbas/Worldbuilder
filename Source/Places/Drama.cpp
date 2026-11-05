#include "Drama.h"
#include "../Arrangement/Arrangement.h"
#include <string>

namespace wb::drama {

using namespace dsp;

const Theme& theme(int type) {
    // War drums are the large djembe pitched down (the deeper, the bigger the Place);
    // tribal drums sit near their own pitch; metal and hybrid kits add the synthesised
    // metal and electronic hits.
    static const std::array<Theme, PlaceTypeCount> table { {
        {},
        { KitWar, OstStrings, HitBrass, HitChoir, StyleDriving, -8, 0.7f, "War drums, spiccato strings, a brass ostinato and choir stabs" },        // castle
        { KitTribal, OstPluck, HitBrass, HitChoir, StyleDriving, -3, 0.45f, "Hand drums driving, plucked strings, horn and choir stabs" },          // village
        { KitHybrid, OstPulse, HitBraam, HitSynth, StyleDriving, -10, 0.6f, "Hybrid pulses, synth swells and braams, hybrid percussion" },         // spaceport
        { KitMetal, OstBass, HitMetal, HitBraam, StyleDriving, -9, 0.5f, "Metal percussion, pistons as rhythm, a grinding bass and braams" },     // factory
        { KitHybrid, OstBass, HitSynth, HitPiano, StyleDriving, -9, 0.45f, "A pulsing bass, electronic drums, synth stabs and low piano" },      // city
        { KitTribal, OstStrings, HitChoir, HitBrass, StyleDriving, -5, 0.55f, "Tribal drums, driving strings, choir and horn stabs" },             // forest
        { KitHybrid, OstPulse, HitBraam, HitChoir, StyleSuspense, -11, 0.65f, "Throbbing pulses, low hybrid hits, braams and an uneasy choir" },  // hive
        { KitWar, OstStrings, HitChoir, HitBrass, StyleEpic, -9, 0.8f, "Taiko-like temple drums, strings, a great choir and brass" },             // temple
        { KitWar, OstBass, HitPiano, HitBraam, StyleSuspense, -11, 0.8f, "Deep drums in the dark, a low pulse, piano strikes and braams" },      // cavern
        { KitWar, OstStrings, HitPiano, HitChoir, StyleTragic, -9, 0.75f, "Low piano strikes, dissonant strings, distant drums and a choir" },    // ruins
        { KitHybrid, OstPulse, HitBraam, HitSynth, StyleDriving, -10, 0.4f, "Hybrid pulses and percussion, alarm-like synth stabs and braams" },  // spaceship
        { KitTribal, OstPulse, HitSynth, HitChoir, StyleDriving, -4, 0.5f, "Hand drums, a bright pulse, synth and choir stabs" },                // biodome
        { KitWar, OstStrings, HitBrass, HitChoir, StyleEpic, -8, 0.6f, "Battle drums at sea, spiccato strings, brass and choir" },                 // sea ship
        { KitMarch, OstPiano, HitPiano, HitBrass, StyleSuspense, -5, 0.4f, "A piano ostinato, a tight snare, low piano and brass stabs" },       // suburbs
        { KitHybrid, OstPulse, HitSynth, HitBrass, StyleDriving, -8, 0.45f, "Electronic drums, a synth pulse, synth and brass stabs" },           // mall
        { KitMarch, OstStrings, HitBrass, HitBraam, StyleDriving, -6, 0.45f, "Snares and bass drums, a string ostinato, brass and braams" },     // military
        { KitTribal, OstPluck, HitBrass, HitChoir, StyleDriving, -3, 0.5f, "Hand drums, plucked guitar, brass and choir stabs" },                 // beach
        { KitTribal, OstPluck, HitBrass, HitChoir, StyleDriving, -4, 0.6f, "Frame drums racing, plucked strings, horn and choir" },              // desert
        { KitWar, OstStrings, HitBrass, HitChoir, StyleEpic, -7, 0.75f, "Court timpani and war drums, strings, fanfare brass and choir" },      // palace
        { KitTribal, OstPluck, HitBrass, HitChoir, StyleDriving, -2, 0.4f, "Stomping hand drums, strummed strings, horn and voices" },           // tavern
        { KitHybrid, OstPulse, HitBraam, HitSynth, StyleSuspense, -10, 0.55f, "A hybrid pulse, soft hits, braams and synth stabs" },             // station
        { KitHybrid, OstBass, HitSynth, HitBraam, StyleDriving, -10, 0.45f, "A grinding synth bass, hybrid drums, cyberpunk stabs and braams" }, // megacity
        { KitHybrid, OstPulse, HitBrass, HitBraam, StyleDriving, -10, 0.6f, "Battle-stations pulses, hybrid drums, brass and braams" },          // bridge
        { KitMetal, OstBass, HitMetal, HitBraam, StyleDriving, -10, 0.5f, "Metal hits and drills as rhythm, a grinding bass, braams" },          // mining
        { KitWar, OstPulse, HitChoir, HitBraam, StyleEpic, -11, 0.8f, "Ritual drums pitched down, ancient pulses, choir and braams" },         // alien ruins
        { KitHybrid, OstPulse, HitChoir, HitBraam, StyleEpic, -12, 0.85f, "Vast hybrid swells, a slow pulse, a great choir and braams" },      // nebula
    } };
    return table[std::size_t(std::clamp(type, 0, PlaceTypeCount - 1))];
}

// The balance per Place, style and tier (Tests "loudprobe --balance" writes the table).
namespace {
struct BalanceRow {
    std::array<std::array<Balance, 3>, 5> style {};
    // The Classic loudness at Score Intensity 62, 80 and 100 % against 50 % (dB), and at 35 %.
    std::array<float, 3> classic {};
    float calm = 0;
    // Each of its six instruments alone against the whole Place (dB, Classic at 50 %).
    std::array<float, placeElements> element {};
    // In an arrangement: what a section at Tension, Action and Climax plays above (or
    // below) the level the engine's section estimate expects (dB, measured).
    std::array<float, 3> arranged {};
    // The same when it plays in the second or a further slot (a lighter Drama layer).
    std::array<float, 3> second {};
    // The orchestration pass: each of the six's busier variant B against A (dB, measured at
    // Score Intensity 100 %).
    std::array<float, placeElements> busier {};
};
#include "DramaBalance.inc"
#include "DramaBalanceV1.inc"
constexpr std::array<float, 4> probeIntensity { 0.5f, 0.62f, 0.8f, 1.0f };
// The table in use: the orchestration pass's (the Place untouched, the layer under it), or
// (Layer::mixPass false, the listening renders' "before") the loudness pass's.
const BalanceRow& rowOf(int placeType) { return (Layer::mixPass ? balanceTable : balanceTableV1)[std::size_t(placeType)]; }
} // namespace

float arrangedCorrectionDb(int placeType, int tier, int rank) {
    if (placeType <= 0 || placeType >= PlaceTypeCount || tier <= Calm) return 0.0f;
    const auto& row = rowOf(placeType);
    return (rank > 0 ? row.second : row.arranged)[std::size_t(std::clamp(tier, 1, 3) - 1)];
}

float elementWeightDb(int placeType, int element) {
    if (placeType <= 0 || placeType >= PlaceTypeCount || element < 0 || element >= placeElements) return 0.0f;
    return rowOf(placeType).element[std::size_t(element)];
}

float busierDb(int placeType, int element) {
    if (placeType <= 0 || placeType >= PlaceTypeCount || element < 0 || element >= placeElements) return 0.0f;
    return balanceTable[std::size_t(placeType)].busier[std::size_t(element)];
}

float balancedDb(int placeType) {
    if (placeType <= 0 || placeType >= PlaceTypeCount) return 0.0f;
    const float calm = rowOf(placeType).calm;   // 35 % against 50 %
    return calm + std::clamp(-0.5f * calm, 0.5f, 1.0f);
}

float classicRiseDb(int placeType, float intensity) {
    if (placeType <= 0 || placeType >= PlaceTypeCount) return 0.0f;
    const auto& row = rowOf(placeType);
    if (intensity < 0.5f) {
        // Calm: 35 % measured, the slope carried on below it (at most 12 dB down).
        return std::max(-12.0f, row.calm * (0.5f - std::max(intensity, 0.0f)) / 0.15f);
    }
    const auto& c = row.classic;
    const std::array<float, 4> at { 0.0f, c[0], c[1], c[2] };
    const float i = std::clamp(intensity, 0.5f, 1.0f);
    for (int k = 0; k < 3; ++k)
        if (i <= probeIntensity[std::size_t(k + 1)]) {
            const float t = (i - probeIntensity[std::size_t(k)]) / (probeIntensity[std::size_t(k + 1)] - probeIntensity[std::size_t(k)]);
            return at[std::size_t(k)] + t * (at[std::size_t(k + 1)] - at[std::size_t(k)]);
        }
    return at[3];
}

Balance balance(int placeType, int style, int tier, int rank, float intensity) {
    if (balanceOff || !loudnessPass || tier <= Calm || placeType <= 0 || placeType >= PlaceTypeCount) return {};
    const auto& row = rowOf(placeType);
    const int t = std::clamp(tier, 1, 3);
    Balance b = row.style[std::size_t(std::clamp(style, 1, StyleCount - 1) - 1)][std::size_t(t - 1)];
    if (Layer::mixPass) {
        // The orchestration pass: the Place plays at its own level (it never makes room); the
        // Drama layer sits under it, at the level measured at the tier's probe point, and
        // follows the Place's own rise with Score Intensity away from it.
        b.placeDb = 0.0f;
        if (intensity >= 0.0f)
            b.layerDb += std::clamp(classicRiseDb(placeType, intensity) - classicRiseDb(placeType, probeIntensity[std::size_t(t)]), -4.0f, 4.0f);
        return b;
    }
    // The second and further Places play a lighter layer (no drums; hits only from the
    // third): their own instruments make less room.
    if (rank > 0) {
        const float share = layerShare[std::size_t(t - 1)], f = rankLayerPower(rank);
        b.placeDb = std::min(0.0f, b.placeDb + 10.0f * std::log10((1.0f - share * f) / (1.0f - share)));
    }
    // Away from the tier's probe point: the Place's own Classic rise balanced too.
    if (intensity >= 0.0f) {
        const float off = std::clamp(classicRiseDb(placeType, probeIntensity[std::size_t(t)]) - classicRiseDb(placeType, intensity), -4.0f, 4.0f);
        b.placeDb = std::min(0.0f, b.placeDb + off);   // (the layer's own level stays as it is)
    }
    return b;
}
namespace {
// A bar of drums for one style and tier: one 16-step line per voice. 'X' accent,
// 'x' normal, 'o' soft, 'g' ghost, '.' none.
struct DrumBar {
    const char *low, *mid, *high, *snare, *tom;
};
constexpr const char* none = "................";
// [style - 1][tier - 1]
constexpr DrumBar drumBars[5][3] {
    { // Driving: a heartbeat, then the 3-3-2 war-drum groove, then double time
      { "X.....o.........", none, "........o.......", none, none },
      { "X..x..x.X..x..x.", "....x.......x..x", "..o...o...o...o.", "....X.......X...", none },
      { "X.x.X.x.X.x.X.x.", "..x...x...x..x.x", "gogogogogogogogo", "..X...X...X...X.", "...x.......x...." } },
    { // Epic: half time and huge
      { "X...............", "........o.......", none, none, none },
      { "X.......X.....x.", "....o.......x...", none, "........X.......", "............x.x." },
      { "X..x....X..x..x.", "....x..x....x..x", "..g...g...g...g.", "........X.......", "X.......X...x.xx" } },
    { // Tragic: almost no drums, a distant toll
      { none, none, none, none, none },
      { "X...............", none, none, none, "........o......." },
      { "X.......x.......", none, none, none, "............o.o." } },
    { // Triumphant: a march
      { "X...............", none, none, "....o.......o...", none },
      { "X.......X.......", "....x.......x...", none, "....X..o....X.oo", none },
      { "X...X...X...X...", "..x...x...x...x.", "g.g.g.g.g.g.g.g.", "..o.X.ooo.o.X.oo", "............xxXX" } },
    { // Suspense: a heartbeat, ticking, then pounding
      { "X..o............", none, none, none, none },
      { "X..o....X..o....", none, "....g.......g...", none, "..............o." },
      { "X..x....X..x.x..", "..x...x...x...x.", "g.g.g.g.g.g.g.g.", "....x.......x...", "............x.xx" } },
};
// The ostinato: 'R' root (accent), 'r' root, 'o' octave, 'f' fifth, 't' third, 'b' a
// semitone above the root, '-' tie, '.' rest.
constexpr const char* ostinatos[5][3] {
    { "r.r.r.r.r.r.r.r.", "RrrRrrRrRrrRrrfr", "RrRrfrRrRroRtrfr" },
    { "r...r...r...r...", "R.r.R.r.R.r.f.r.", "RrRrRrRrfrfrorfr" },
    { "r-------f-------", "r.t.f.t.o.t.f.t.", "RtftotftRtftotft" },
    { "r...f...r...f...", "R.rrf.ffR.rro.f.", "RrfrRrfrRrorRrfr" },
    { "r.b.r.b.r.b.r.b.", "rrbrrrbrrrbrrrbr", "RrbrRrbrRrbrRbbr" },
};
// Stabs on the hit instruments, and every how many bars the line plays.
struct HitBar {
    const char* line;
    int every;
};
constexpr HitBar hitBars[5][3] {
    { { none, 1 }, { "X...............", 2 }, { "X.........x.....", 1 } },
    { { none, 1 }, { "X...............", 2 }, { "X.......x.......", 1 } },
    { { "X...............", 4 }, { "X...............", 2 }, { "X.......x.......", 1 } },
    { { none, 1 }, { "X.......x.......", 2 }, { "X..x.X..X...X.x.", 1 } },
    { { "X...............", 4 }, { "X...............", 2 }, { "X.....x.........", 1 } },
};
float velocityOf(char c) {
    switch (c) {
        case 'X': case 'R': return 1.0f;
        case 'x': return 0.78f;
        case 'o': return 0.5f;
        case 'g': return 0.32f;
        default: return 0.0f;
    }
}
// Each part's level in the layer (dB), set by measurement (Tests "drama": the balance probe).
// The Drama loudness pass staged the hits (Tests "hitprobe"): no hit's 400 ms loudness
// above the whole layer's, its true peak at most about 8 dB above it: the low piano
// strikes -4 dB, braams -3.5, synth stabs -2.5, the crash and metal stabs -3, impacts
// -2.5, choir -1, a metal kit's clanks -1.
//                                               drums perc metal swell strings pulse bass pluck low brass choir piano braam synth
//                                               ... hands brassOst pianoOst metalHit impact
constexpr std::array<float, Layer::PartCount> trimBeforeDb { 7.0f, -6.0f, 5.0f, 4.0f, 1.0f, 0.0f, 0.0f, 0.0f, -10.0f, -8.0f, 3.0f, 0.0f, 0.0f, 0.0f, 5.0f,
                                                             -8.0f, 0.0f, 5.0f, -6.0f };   // (before the loudness pass; tests only)
constexpr std::array<float, Layer::PartCount> trimDb { 7.0f, -6.0f, 4.0f, 4.0f, 1.0f, 0.0f, 0.0f, 0.0f, -10.0f, -8.0f, 2.0f, -4.0f, -3.5f, -2.5f, 5.0f,
                                                       -8.0f, 0.0f, 2.0f, -8.5f };
const char* hitName(int hit) {
    static constexpr std::array<const char*, 6> names { "Brass", "Choir", "Low Piano", "Braam", "Synth", "Metal" };
    return names[std::size_t(std::clamp(hit, 0, 5))];
}
} // namespace

//==============================================================================
// The Drama instruments' starting patterns.

void generate(int placeType, int element, int variant, const GenInputs& in, pat::Pattern& out) {
    const auto& th = theme(placeType);
    const int style = styleFor(placeType, in.style);
    const int si = std::clamp(style, 1, StyleCount - 1) - 1;
    int t = std::clamp(in.tier, int(Tension), int(Climax));
    // B is busier (the next tier's bars; at a climax, more on top), C sparser (the tier
    // below; at Tension, barely anything), the Fill drives into the next section.
    bool extra = false, sparse = false;
    if (variant == pat::VariantB) {
        if (t < Climax) ++t;
        else extra = true;
    } else if (variant == pat::VariantC) {
        if (t > Tension) --t;
        else sparse = true;
    }
    const bool fill = variant == pat::VariantFill;
    const int ti = t - 1;
    const uint32_t take = in.variant;   // Generate: another take
    const auto chance = [&](int a, int b) { return dsp::unipolar(dsp::mix64((uint64_t(in.key) << 20) ^ (uint64_t(take) << 44) ^ uint64_t(a * 977 + b * 31 + element))); };
    out.clear();
    out.swing = 0;
    out.base = 0;
    PatternWriter w { out };
    if (element == DramaDrums) {
        out.setGrid(pat::Sixteenth, fill ? 1 : 4);
        out.followChords = false;
        if (in.rank > 0) return;   // the leading slot plays the drums
        const auto& d = drumBars[si][ti];
        const int kit = th.kit;
        const float highGain = kit == KitMetal ? 0.55f : (kit == KitHybrid ? 0.7f : (kit == KitMarch ? 0.5f : 0.8f));
        const int bars = fill ? 1 : 4;
        for (int bar = 0; bar < bars; ++bar)
            for (int pos = 0; pos < 16; ++pos) {
                const int step = bar * 16 + pos;
                bool high = false;
                const auto put = [&](int row, char c, float gain) {
                    const float v = velocityOf(c);
                    if (v <= 0.0f) return;
                    w.note(step, row, 1, v * gain, 0, c == 'g' ? 0.8f : 1.0f);
                    high |= row == RowHigh;
                };
                if (sparse) {
                    // Tension's C: the heartbeat's first stroke, every other bar.
                    if (pos == 0 && (bar & 1) == 0) put(RowLow, d.low[0], 1.0f);
                    continue;
                }
                if (fill) {
                    // A bar into the next section: the downbeats, a snare roll swelling over the
                    // second half, the toms over the last beat.
                    if (pos == 0 || pos == 8) put(RowLow, 'X', 1.0f);
                    if (pos >= 8) w.note(step, RowSnare, 1, (0.4f + 0.075f * float(pos - 8)) * 0.8f);
                    if (pos >= 12) w.note(step, pos < 14 ? RowTomHigh : RowTomLow, 1, 0.6f + 0.1f * float(pos - 12));
                    continue;
                }
                put(RowLow, d.low[pos], 1.0f);
                put(RowMid, d.mid[pos], 0.9f);
                // A hybrid kit's hats from Action on, merged with the high line (the louder of the two).
                const bool hat = kit == KitHybrid && t >= Action && ((pos & 1) == 0 || t == Climax);
                if (hat) {
                    const float v = std::max(velocityOf(d.high[pos]) * highGain, (pos & 1) == 0 ? 0.55f : 0.35f);
                    w.note(step, RowHigh, 1, v);
                    high = true;
                } else {
                    put(RowHigh, d.high[pos], highGain);
                }
                put(RowSnare, d.snare[pos], kit == KitMarch ? 1.0f : 0.8f);
                // Toms: their line, and a fill on the last beat of every fourth bar (every other at a climax).
                const bool fillBar = (t >= Action && (bar & 3) == 3) || (t == Climax && (bar & 1) == 1) || (extra && pos >= 12);
                if (fillBar && pos >= 12) w.note(step, pos < 14 ? RowTomHigh : RowTomLow, 1, 0.6f + 0.1f * float(pos - 12));
                else if (velocityOf(d.tom[pos]) > 0.0f) put((pos & 4) != 0 ? RowTomHigh : RowTomLow, d.tom[pos], 0.85f);
                // Pistons as rhythm (metal kits), hats (hybrid kits).
                if (kit == KitMetal && (pos & 1) == 0 && (t >= Action || (pos & 3) == 0)) w.note(step, RowPistons, 1, pos % 4 == 0 ? 0.9f : 0.6f);
                // B at a climax: ghost strokes on every free sixteenth.
                if (extra && !high && (pos & 1) == 1) w.note(step, RowHigh, 1, 0.32f * highGain / 0.8f, 0, 0.8f);
                // Another take: an extra low stroke somewhere in each bar.
                if (take != 0 && pos == 0) {
                    static constexpr int spots[4] { 3, 6, 10, 14 };
                    w.note(bar * 16 + spots[int(chance(bar, 1) * 3.999f)], RowLow, 1, 0.6f);
                }
            }
        out.normalise();
        return;
    }
    if (element == DramaOstinato) {
        out.setGrid(pat::Sixteenth, fill ? 1 : 2);
        // At Tension a pedal on the key's root; from Action on the chord's root.
        out.followChords = t > Tension;
        if (in.rank > 1) return;   // the two leading slots play the ostinato
        const int scale = music::scaleFor(in.mood).size;
        if (fill) {
            // A run into the next section: sixteenths climbing the chord, louder.
            static constexpr int climb[16] { 0, 0, 2, 2, 4, 4, 7, 7, 0, 2, 4, 7, 9, 11, 14, 14 };
            for (int pos = 0; pos < 16; ++pos) {
                const int d = climb[pos] >= 7 ? climb[pos] - 7 + scale : climb[pos];
                w.note(pos, d, 1, 0.6f + 0.4f * float(pos) / 15.0f);
            }
            return;
        }
        const char* line = ostinatos[(si + int(take % 5u)) % 5][ti];
        for (int bar = 0; bar < 2; ++bar)
            for (int pos = 0; pos < 16; ++pos) {
                const char c = line[pos];
                if (c == '.' || c == '-') continue;
                if (sparse && (pos & 3) != 0) continue;   // Tension's C: quarter notes
                int ties = 0;
                while (pos + 1 + ties < 16 && line[pos + 1 + ties] == '-') ++ties;
                int degree = 0, alter = 0;
                switch (c) {
                    case 'o': degree = scale; break;
                    case 'f': degree = 4; break;
                    case 't': degree = 2; break;
                    case 'b': alter = 1; break;
                    default: break;
                }
                // Tension at the top: the fifths become a semitone's rub.
                if (c == 'f' && in.tension > 0.72f && bar == 1) {
                    degree = 0;
                    alter = 1;
                }
                const int step = bar * 16 + pos;
                w.note(step, degree, ties > 0 ? ties + 1 : 1, c == 'R' ? 1.0f : 0.66f, alter);
                // B at a climax: the accents doubled an octave up.
                if (extra && c == 'R') w.note(step, degree + scale, 1, 0.7f, alter);
            }
        out.normalise();
        return;
    }
    // The hits: eight bars of thirty-seconds (the stabs' lengths).
    out.setGrid(pat::ThirtySecond, fill ? 1 : 8);
    out.followChords = false;
    if (fill) {
        // A cymbal swell over the bar, a stab as the pickup.
        if (in.rank == 0) w.note(0, RowSwell, 32, 0.8f);
        w.note(28, RowStabA, 2, 0.8f);
        return;
    }
    const auto& h = hitBars[si][ti];
    const bool broad = style == StyleEpic || style == StyleTragic;
    for (int bar = 0; bar < 8; ++bar)
        for (int pos = 0; pos < 16; ++pos) {
            const int step = bar * 32 + pos * 2;
            const char c = h.line[pos];
            const float v = velocityOf(c);
            // Stabs, short and hard (held longer in the broad styles); a climax keeps them
            // shortest so the drums come through.
            const int length = broad ? (t == Climax ? 8 : 16) : (t == Climax ? 3 : (c == 'X' ? 6 : 4));
            if (sparse) {
                if (pos == 0 && (bar & 7) == 0 && v > 0.0f) w.note(step, RowStabA, length, 0.7f * v);
                continue;
            }
            if (v > 0.0f && ((bar % h.every) + h.every) % h.every == 0) {
                w.note(step, RowStabA, length, v);
                // The second hit joins on the phrase lines, and on every other bar's downbeat at a climax.
                if ((t == Climax && pos == 0 && (bar & 1) == 0) || ((bar & 3) == 0 && pos == 0 && t >= Action)) w.note(step, RowStabB, length, 0.8f * v);
            }
            // B at a climax: off-beat stabs pushing every bar.
            if (extra && (pos == 6 || pos == 10)) w.note(step, RowStabA, 3, 0.7f);
            if (in.rank == 0) {
                // The phrase line at a climax: an impact and a crash every four bars.
                if (t == Climax && pos == 0 && (bar & 3) == 0) w.note(step, RowImpact, 16, 1.0f);
                // A cymbal swell into the next phrase.
                if (pos == 0 && ((t == Climax && (bar & 3) == 3) || (t == Action && (bar & 7) == 7))) w.note(step, RowSwell, 32, 0.75f);
            }
            // Another take: a pickup stab before the phrase lines.
            if (take != 0 && pos == 14 && (bar & 3) == 3 && t >= Action) w.note(step, RowStabA, 2, 0.6f + 0.3f * chance(bar, 2));
        }
    out.normalise();
}

} // namespace wb::drama

//==============================================================================
// The Drama instruments' names and kit rows, per Place (Place.h).
namespace wb {
const ElementSpec& dramaElementSpec(int type, int element) {
    struct Table {
        std::array<std::array<ElementSpec, dramaElements>, PlaceTypeCount> spec {};
        std::array<std::string, PlaceTypeCount> hitNames;
        std::array<std::array<std::string, 2>, PlaceTypeCount> stabNames;
    };
    static const Table table = [] {
        Table x;
        static constexpr std::array<const char*, 5> drumNames { "War Drums", "Tribal Drums", "Metal Drums", "Hybrid Drums", "March Drums" };
        static constexpr std::array<int, 5> drumIcons { IconTimpani, IconHandDrum, IconPiston, IconSnare, IconSnare };
        static constexpr std::array<const char*, 5> ostinatoNames { "Spiccato Strings", "Synth Pulse", "Pulse Bass", "Plucked Ostinato", "Piano Ostinato" };
        static constexpr std::array<int, 5> ostinatoIcons { IconStrings, IconPulse, IconBass, IconLute, IconPiano };
        static constexpr std::array<const char*, 5> highRow { "Slap", "Slap", "Clank", "Hat", "Snare Ghost" };
        for (int t = 0; t < PlaceTypeCount; ++t) {
            const auto& th = drama::theme(t);
            const int kit = std::clamp(th.kit, 0, 4);
            auto& drums = x.spec[std::size_t(t)][0];
            drums.name = drumNames[std::size_t(kit)];
            drums.icon = drumIcons[std::size_t(kit)];
            drums.kind = Kit;
            drums.kit = { kit == drama::KitMarch ? "Bass Drum" : (kit == drama::KitHybrid ? "Low Hit" : (kit == drama::KitTribal ? "Low Drum" : "War Drum")),
                          kit == drama::KitHybrid ? "Mid Hit" : "Mid Drum", highRow[std::size_t(kit)], kit == drama::KitHybrid ? "Clap" : "Snare",
                          "High Tom", "Low Tom", kit == drama::KitMetal ? "Pistons" : nullptr, nullptr };
            drums.rows = kit == drama::KitMetal ? 7 : 6;
            drums.vary = pat::VaryVelocity;
            auto& ost = x.spec[std::size_t(t)][1];
            ost.name = ostinatoNames[std::size_t(std::clamp(th.ostinato, 0, 4))];
            ost.icon = ostinatoIcons[std::size_t(std::clamp(th.ostinato, 0, 4))];
            ost.kind = Melodic;
            ost.vary = pat::VaryVelocity;
            x.hitNames[std::size_t(t)] = std::string(drama::hitName(th.hitA)) + " & " + drama::hitName(th.hitB) + " Hits";
            x.stabNames[std::size_t(t)][0] = std::string(drama::hitName(th.hitA)) + " Stab";
            x.stabNames[std::size_t(t)][1] = std::string(drama::hitName(th.hitB)) + " Stab";
        }
        // The names' storage is in place: point the specs at it.
        for (int t = 0; t < PlaceTypeCount; ++t) {
            auto& hits = x.spec[std::size_t(t)][2];
            hits.name = x.hitNames[std::size_t(t)].c_str();
            hits.icon = IconHorn;
            hits.kind = Kit;
            hits.kit = { x.stabNames[std::size_t(t)][0].c_str(), x.stabNames[std::size_t(t)][1].c_str(), "Impact", "Swell", nullptr, nullptr, nullptr, nullptr };
            hits.rows = 4;
            hits.vary = pat::VaryVelocity;
        }
        return x;
    }();
    return table.spec[std::size_t(std::clamp(type, 0, PlaceTypeCount - 1))][std::size_t(std::clamp(element - placeElements, 0, dramaElements - 1))];
}
} // namespace wb

namespace wb::drama {

Layer::Layer() {
    parts = { &drums, &perc, &metal, &swell, &strings, &pulse, &bass, &pluck, &low, &brass, &choir, &piano, &braam, &synthStab, &hands,
              &brassOst, &pianoOst, &metalHit, &impact };
    // The recordings each part plays (resolved once, when the bank is ready).
    strings.patch.instrument = "violin-sustain";
    pulse.patch.instrument = "mlg-pulse-long";
    pulse.patch.shortInstrument = "mlg-pulse-short";
    bass.patch.instrument = "mlg-staticbass-long";
    bass.patch.shortInstrument = "mlg-staticbass-short";
    pluck.patch.instrument = "guitar";
    piano.patch = pianoPatch();
    pianoOst.patch = pianoPatch();
    braam.patch.instrument = "mlg-square-long";
    synthStab.patch.instrument = "mlg-cyberpunk-long";
    synthStab.patch.shortInstrument = "mlg-cyberpunk-short";
}

void Layer::clearPercussion() {
    room.clear();
    for (auto& f : hallCut) f.reset();
    for (auto& f : roomCut) f.reset();
    for (auto& f : percHp) f.reset();
    for (auto& f : percShelf) f.reset();
    percLimiter.reset();
    percTail = 0;
}

bool Layer::isPercussion(int part) {
    switch (part) {
        case PDrums: case PPerc: case PMetal: case PHands: case PMetalHit: case PImpact: case PSwell: case PLow: return true;
        default: return false;
    }
}

int Layer::elementOf(int part) {
    switch (part) {
        case PDrums: case PPerc: case PMetal: case PHands: return 0;
        case PStrings: case PPulse: case PBass: case PPluck: case PLow: case PBrassOst: case PPianoOst: return 1;
        default: return 2;
    }
}

void Layer::prepare(float sampleRate) {
    rate = sampleRate;
    for (auto* p : parts) p->prepare(rate);
    hall.prepare(rate);
    room.prepare(rate);
    room.set(0.12f, 0.35f, 6500.0f);
    for (auto& f : hallCut) f.setCutoff(220.0f, rate);
    for (auto& f : roomCut) f.setCutoff(300.0f, rate);
    for (auto& f : percHp) f.set(40.0f, 0.7f, rate);
    for (auto& f : percShelf) f.setCutoff(110.0f, rate);
    limiter.prepare(rate);
    percLimiter.prepare(rate);
    limiter.ceiling = limiterCeiling;
    level.setTime(0.08f, rate / float(chunk));
    reset();
}

void Layer::reset() {
    for (auto* p : parts) p->reset();
    hall.clear();
    clearPercussion();
    limiter.reset();
    gainNow.fill(-1.0f);
    quiet.fill(1 << 30);
    level.snap(0);
    latched = previous = want = Calm;
    x = 0;
    awake = false;
    fresh = true;
    gearChange = false;
    lastBar = gearBar = INT64_MIN;
    idleSamples = 1 << 30;
    count = 0;
    lastBeat = -1.0e18;
    type = -1;
    patchKey = 0;
}

void Layer::setSeed(uint32_t key) {
    seedKey = key;
    noise.setKey(key, 9100);
    for (int k = 0; k < PartCount; ++k) parts[std::size_t(k)]->setKey(key, 9110 + uint32_t(k));
}

float Layer::rand(int64_t step, uint32_t salt) const { return noise.unit((uint64_t(salt) << 48) ^ uint64_t(step + (int64_t(1) << 40))); }

float Layer::transposeSemis(int element) const {
    if (element != DramaOstinato) return 0.0f;
    return ostinatoRegister + parts[std::size_t(ostinatoPart)]->transposeSemis();
}

void Layer::configure(int placeType, const WorldFrame& world, int style, float tension, float bpm) {
    const auto& t = theme(placeType);
    const float tone = clamp01(0.45f + 0.35f * (world[Era] - 0.3f) + 0.25f * heat);
    const float dark = clamp01(0.5f * tension + 0.4f * world[Danger]);
    const int q = std::clamp(world.quality, 0, 2);   // Eco, Standard, High: the voices
    (void) bpm;
    // War, tribal, metal, hybrid and march kits from the developer's djembes: the large
    // drum's bass as the war drum, its tone and the small drum as the mid drums, the
    // slaps on top; pitched down (tape speed) for size.
    {
        auto& p = drums.patch;
        const float s = t.drumSemis;
        const float bright = t.kit == KitTribal ? 1.6f : (t.kit == KitWar ? 0.7f : 1.0f);
        p.rows = {};
        p.rows[0] = { t.kit == KitMarch ? djembe::BigMutedBass : djembe::BigBass, djembe::BigMutedBass, 0.15f, s, 1.25f, -0.05f, 3500.0f * bright, 0.6f, 25.0f };
        p.rows[1] = { djembe::BigTone, djembe::SmallBass, 0.4f, s + 3.0f, 0.95f, 0.22f, 5500.0f * bright, 0.6f, 35.0f };
        p.rows[2] = { djembe::SmallSlap, djembe::BigSlap, 0.3f, s + 5.0f, 0.75f, -0.25f, 9000.0f * bright, 0.5f, 60.0f };
        p.rows[3] = { djembe::SmallMutedSlap, -1, 0.0f, s + 5.0f, 0.5f, 0.3f, 8000.0f * bright, 0.5f, 80.0f };
        if (t.kit == KitHybrid) {
            p.rows[0].semis = s - 2.0f;
            p.rows[0].decay = 0.9f;
        }
        p.velocityScale = lerp(1.0f, 1.5f, heat);
        p.drive = t.kit == KitHybrid || t.kit == KitMetal ? 0.25f : 0.08f;
        p.spread = 0.25f;
        p.voices = byQuality(q, 5, 7, 9);
        // Each kit at the same weight (measured: the war drums' dark tone loses the most).
        static constexpr float kitGain[5] { 1.6f, 1.0f, 0.6f, 0.75f, 1.0f };
        p.gain = kitGain[std::clamp(t.kit, 0, 4)];
        // The hands: the slaps and the ghost strokes, short.
        auto& h = hands.patch;
        h.rows = {};
        h.rows[0] = p.rows[2];
        h.rows[1] = p.rows[3];
        h.rows[0].decay = 0.5f;
        h.rows[1].decay = 0.35f;
        h.velocityScale = p.velocityScale;
        h.drive = p.drive;
        h.spread = 0.35f;
        h.voices = byQuality(q, 3, 5, 7);
        h.gain = p.gain;
        // The synthesised war drums (no recordings): deep membranes.
        auto& f = drumsSynth.patch;
        for (int r = 0; r < 4; ++r) {
            auto& row = f.rows[std::size_t(r)];
            row = DrumRow {};
            row.kind = dsp::DrumMembrane;
            row.semis = -24.0f + 5.0f * float(r) + 0.5f * s;
            row.bend = 1.08f;
            row.decay = r == 0 ? 1.6f : 0.7f;
            row.tone = 0.7f;
            row.noise = 0.15f + 0.1f * float(r);
            row.noiseHz = 600.0f + 900.0f * float(r);
            row.noiseDecay = 0.08f;
            row.hardness = 0.55f + 0.1f * float(r);
            row.position = 0.2f + 0.15f * float(r);
            row.pan = 0.15f * float(r - 1);
        }
        f.voices = 8;
        f.gain = 1.0f;
        auto& hs = handsSynth.patch;
        hs = f;
        for (auto& row : hs.rows) {
            row.semis += 12.0f;
            row.decay = 0.25f;
        }
    }
    // Snare (or a hybrid clap), two toms and an impact: a deep boom with a burst of air.
    {
        auto& p = perc.patch;
        auto& snare = p.rows[0];
        snare = DrumRow {};
        snare.tuned = false;
        snare.hz = t.kit == KitHybrid ? 240.0f : 195.0f;
        snare.bend = 1.1f;
        snare.decay = 0.25f;
        snare.tone = t.kit == KitHybrid ? 0.15f : 0.35f;
        snare.noise = t.kit == KitHybrid ? 0.9f : 0.6f;
        snare.noiseHz = t.kit == KitHybrid ? 1800.0f : 3000.0f;
        snare.noiseQ = 0.7f;
        snare.noiseDecay = t.kit == KitMarch ? 0.22f : 0.16f;
        snare.kind = t.kit == KitHybrid ? dsp::DrumTwoTone : dsp::DrumMembrane;
        snare.hardness = 0.85f;
        snare.snare = t.kit == KitHybrid ? 0.0f : 0.7f;
        snare.position = 0.35f;
        snare.pan = 0.1f;
        for (int r = 1; r <= 2; ++r) {
            auto& tom = p.rows[std::size_t(r)];
            tom = DrumRow {};
            tom.kind = dsp::DrumMembrane;
            tom.semis = r == 1 ? -10.0f : -17.0f;
            tom.bend = 1.12f;
            tom.decay = 0.9f;
            tom.tone = 0.8f;
            tom.noise = 0.12f;
            tom.noiseHz = 900;
            tom.noiseDecay = 0.06f;
            tom.hardness = 0.7f;
            tom.position = 0.25f;
            tom.pan = r == 1 ? -0.35f : 0.3f;
        }
        auto& impact = p.rows[3];
        impact = DrumRow {};
        impact.kind = dsp::DrumTimpani;
        impact.semis = -26.0f;
        impact.bend = 1.25f;
        impact.decay = 3.2f;
        impact.tone = 1.0f;
        impact.noise = 0.35f;
        impact.noiseHz = 350;
        impact.noiseQ = 0.6f;
        impact.noiseDecay = 0.5f;
        impact.ratio = 1.5f;
        impact.partial = 0.3f;
        impact.hardness = 0.95f;
        impact.position = 0.1f;
        p.voices = byQuality(q, 4, 5, 6);
        p.gain = 1.0f;
    }
    // A crash, a metal clank tuned to the key, a piston and a closed hat.
    {
        auto& p = metal.patch;
        auto& crash = p.rows[0];
        crash = DrumRow {};
        crash.tuned = false;
        crash.hz = 430;
        crash.bend = 1.0f;
        crash.decay = 1.8f;
        crash.tone = 0.25f;
        crash.noise = 0.9f;
        crash.noiseHz = 6500;
        crash.noiseQ = 0.45f;
        crash.noiseDecay = 1.5f;
        crash.ratio = 2.76f;
        crash.partial = 0.7f;
        crash.pan = -0.2f;
        auto& clank = p.rows[1];
        clank = DrumRow {};
        clank.semis = 12;
        clank.decay = 0.7f;
        clank.tone = 0.7f;
        clank.noise = 0.25f;
        clank.noiseHz = 5000;
        clank.noiseDecay = 0.04f;
        clank.ratio = 2.76f;
        clank.partial = 0.6f;
        clank.pan = 0.3f;
        auto& piston = p.rows[2];
        piston = DrumRow {};
        piston.tuned = false;
        piston.hz = 85;
        piston.bend = 1.35f;
        piston.decay = 0.22f;
        piston.tone = 0.8f;
        piston.noise = 0.55f;
        piston.noiseHz = 1400;
        piston.noiseQ = 0.9f;
        piston.noiseDecay = 0.1f;
        piston.ratio = 2.1f;
        piston.partial = 0.4f;
        piston.pan = -0.3f;
        auto& hat = p.rows[3];
        hat = DrumRow {};
        hat.tuned = false;
        hat.hz = 7000;
        hat.decay = 0.05f;
        hat.tone = 0.0f;
        hat.noise = 0.8f;
        hat.noiseHz = 9500;
        hat.noiseQ = 0.7f;
        hat.noiseDecay = 0.035f;
        hat.pan = 0.25f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = t.kit == KitMetal ? 0.35f : 1.0f;   // the factory's pistons play every eighth
    }
    // Swells: a cymbal swell, a riser (landing on a low impact) and a downlifter.
    {
        auto& p = swell.patch;
        p.rows[0] = { SweepBuild, 46.0f, 84.0f, 0.8f, 0.7f, false, false };
        p.rows[1] = { SweepBuild, -12.0f, 60.0f, 2.5f, 0.9f, true, true };
        p.rows[2] = { SweepFall, -12.0f, 60.0f, 2.0f, 0.6f, false, true };
        p.tension = dark;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Spiccato strings: the recorded violin, short bows from where the note speaks, a
    // section's spread; darker when tense.
    {
        auto& p = strings.patch;
        p.held = true;
        p.attack = 0.0f;
        p.release = style == StyleTragic ? 0.35f : 0.07f;
        p.quickBelow = 0.4f;
        p.quickSeconds = 0.1f;
        p.vibratoDepth = style == StyleTragic ? 0.15f : 0.04f;
        p.toneHz = lerpExp(3500.0f, 13000.0f, tone) * lerp(1.0f, 0.7f, dark);
        p.velocityTone = 0.8f;
        p.velocityGain = 0.7f;
        p.transpose = 12.0f;
        p.lowestKey = 55;
        p.highestKey = 88;
        p.spread = 0.5f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 2.4f;
        auto& f = stringsSynth.patch;
        f.model = BowedString;
        f.held = true;
        f.attack = 0.015f;
        f.release = 0.08f;
        f.brightness = 0.6f + 0.3f * heat;
        f.players = 2;
        f.transpose = 12.0f;
        f.voices = 6;
        f.gain = 1.0f;
    }
    // The Minilogue's pulsing patch and its static-y bass: short hits ring as recorded.
    for (auto* part : { &pulse, &bass }) {
        auto& p = part->patch;
        p.shortBeats = 0.5f;
        p.held = true;
        p.shortRings = true;
        p.release = 0.08f;
        p.decay = 0.4f;
        p.toneHz = lerpExp(1800.0f, 11000.0f, tone);
        p.velocityTone = 0.7f;
        p.velocityGain = 0.5f;
        p.spread = 0.3f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = part == &bass ? 1.4f : 1.4f;
    }
    for (auto* part : { &pulseSynth, &bassSynth, &synthStabSynth }) {
        auto& p = part->patch;
        p.attack = 0.002f;
        p.decay = 0.16f;
        p.sustain = 0.0f;
        p.release = 0.06f;
        p.held = true;
        p.detune = 9;
        p.sub = part == &bassSynth ? 0.6f : 0.2f;
        p.cutoff = lerpExp(500.0f, 2400.0f, tone);
        p.envOctaves = 2.0f;
        p.filterEnvOctaves = 2.0f;
        p.filterDecay = 0.08f;
        p.q = 1.1f;
        p.voices = 6;
        p.gain = 0.8f;
    }
    // Plucked strings: the recorded guitar, hard and short.
    {
        auto& p = pluck.patch;
        p.held = false;
        p.decay = 0.6f;
        p.toneHz = lerpExp(3000.0f, 12000.0f, tone);
        p.velocityGain = 0.6f;
        p.spread = 0.4f;
        p.voices = 6;
        p.gain = 2.0f;
        pluckSynth.patch.decay = 0.5f;
        pluckSynth.patch.brightness = 0.7f;
        pluckSynth.patch.voices = 6;
    }
    // The low doubling: a filtered saw and sub under the ostinato's accents.
    {
        auto& p = low.patch;
        p.model = SynthAnalog;
        p.attack = 0.003f;
        p.decay = 0.16f;
        p.sustain = 0.0f;
        p.release = 0.08f;
        p.held = true;
        p.detune = 6;
        p.sub = 0.7f;
        p.cutoff = lerpExp(180.0f, 700.0f, heat) * lerp(1.0f, 0.8f, dark);
        p.envOctaves = 1.5f;
        p.filterEnvOctaves = 2.0f;
        p.filterDecay = 0.1f;
        p.q = 1.0f;
        p.width = 0.2f;
        p.spread = 0.1f;
        p.voices = 3;
        p.gain = 0.55f;
    }
    // Brass stabs and the brass ostinato: a section, bright when it plays hard.
    {
        auto& p = brass.patch;
        p.model = SynthBrass;
        p.attack = 0.012f;
        p.decay = 0.3f;
        p.sustain = 0.55f;
        p.release = 0.28f;
        p.held = true;
        p.detune = 7;
        p.sub = 0.15f;
        p.cutoff = lerpExp(700.0f, 1500.0f, tone) * lerp(0.9f, 1.25f, heat);
        p.formantHz = 600;
        p.formantLevel = 0.6f;
        p.breath = 0.03f;
        p.spread = 0.5f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 0.55f;
    }
    // Choir stabs: the developer's voice as a section, short and full.
    {
        auto& c = choirSynth.patch;
        c.vowel = style == StyleSuspense ? 0.4f : 2.0f;
        c.attack = style == StyleEpic || style == StyleTragic ? 0.25f : 0.03f;
        c.release = 0.5f;
        c.detune = 10;
        c.brightness = 0.55f + 0.3f * heat;
        c.spread = 0.7f;
        c.voices = 4;
        c.gain = 1.0f;
        auto& p = choir.patch;
        p = voiceFromChoir(c);
        p.singers = 3;
        p.detune = 10;
        p.timing = 0.02f;
        p.held = true;
        p.toneHz = lerpExp(4000.0f, 10000.0f, tone);
        p.voices = byQuality(q, 2, 3, 4);
        p.gain = 0.6f;
    }
    // Low piano strikes: the developer's piano, its lowest octaves hit hard.
    {
        auto& p = piano.patch;
        p.toneHz = lerpExp(2500.0f, 9000.0f, tone);
        p.spread = 0.3f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.5f;
        auto& f = pianoSynth.patch;
        f.attack = 0.002f;
        f.decay = 2.5f;
        f.sustain = 0.0f;
        f.release = 0.6f;
        f.held = false;
        f.detune = 3;
        f.sub = 0.3f;
        f.cutoff = 900;
        f.envOctaves = 2.0f;
        f.voices = 6;
    }
    // Braams: the Minilogue's harsh square held low, a slow filter.
    {
        auto& p = braam.patch;
        p.held = true;
        p.attack = 0.04f;
        p.release = 0.7f;
        p.toneHz = lerpExp(1500.0f, 7000.0f, tone);
        p.velocityTone = 0.8f;
        p.spread = 0.5f;
        p.voices = 4;
        p.gain = t.hitA == HitBraam ? 0.3f : 0.45f;
        auto& f = braamSynth.patch;
        f.attack = 0.05f;
        f.decay = 1.5f;
        f.sustain = 0.6f;
        f.release = 0.7f;
        f.held = true;
        f.detune = 14;
        f.sub = 0.5f;
        f.cutoff = 500;
        f.envOctaves = 2.5f;
        f.q = 1.4f;
        f.voices = 4;
        f.gain = 0.7f;
    }
    {
        auto& p = synthStab.patch;
        p.shortBeats = 0.5f;
        p.held = true;
        p.shortRings = true;
        p.release = 0.12f;
        p.toneHz = lerpExp(2500.0f, 12000.0f, tone);
        p.spread = 0.6f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 0.9f;
    }
    // The parts of their own (the Drama tier rework): the ostinato's brass doubling and a
    // piano ostinato as the stabs' brass and piano; the hits' crash and metal stabs as the
    // drums' metal (at full level: a metal kit's pistons are what plays quieter there);
    // the impact as the snare and toms' drum.
    brassOst.patch = brass.patch;
    pianoOst.patch = piano.patch;
    pianoOstSynth.patch = pianoSynth.patch;
    metalHit.patch = metal.patch;
    metalHit.patch.gain = 1.0f;
    impact.patch = perc.patch;
    kitNow = t.kit;
    static constexpr int ostinatoParts[] { PStrings, PPulse, PBass, PPluck, PPianoOst };
    ostinatoPart = ostinatoParts[std::clamp(t.ostinato, 0, 4)];
    ostinatoRegister = t.ostinato == OstBass ? -12.0f : 0.0f;
    hall.set(t.hall, lerp(1.5f, 4.5f, t.hall), lerpExp(2500.0f, 7000.0f, tone));
    hallWet = 0.35f + 0.4f * t.hall;
}

void Layer::add(int part, int offset, float semis, float velocity, double beat, double lengthBeats, int row, float pan, int live) {
    if (count >= maxNotes || velocity <= 0.0f) return;
    Note& note = pending[std::size_t(count++)];
    note.offset = offset;
    note.part = part;
    note.on = NoteOn {};
    note.on.semis = semis;
    note.on.row = row;
    note.on.velocity = std::clamp(velocity, 0.0f, 1.25f);
    note.on.beat = beat;
    note.on.length = lengthBeats;
    note.on.seconds = live != 0 ? 3600.0f : float(lengthBeats / std::max(frame.beatsPerSecond, 0.01));
    note.on.live = live;
    const uint64_t h = mix64(noise.key ^ (uint64_t(part + 1) << 52) ^ uint64_t(int64_t(std::floor(beat * 96.0)) + (int64_t(1) << 40)) ^ (uint64_t(row + 7) << 44)
                             ^ uint64_t(int64_t(semis * 4.0f) + 512) << 30);
    note.on.random = unipolar(h);
    note.on.random2 = unipolar(mix64(h));
    if (pan <= 1.0f) {
        note.on.hasPan = true;
        note.on.pan = pan;
    }
}

void Layer::chord(int part, int offset, int root, int mood, int degree, float velocity, double beat, double length, bool power, int colour) {
    // Dark, low voicings: the root an octave down with its fifth, the root, and (not for a
    // power chord) the third on top. colour 1: the chromatic mediant (a major triad a
    // minor sixth up), the epic shift into a climax.
    (void) root;
    int r = music::inRange(music::chordTone(mood, degree, 0), -5);
    int third = music::chordTone(mood, degree, 1) - music::chordTone(mood, degree, 0);
    int fifth = 7;
    if (colour == 1) {
        r = music::inRange(r + 8, -5);
        third = 4;
    }
    if (colour == 2) third = 1;   // a cluster: tension at the very top
    const auto played = [&](int semis, float v) { add(part, offset, float(semis), velocity * v, beat, length, 0, 2.0f, liveId); };
    played(r - 12, 0.9f);
    played(r - 12 + fifth, 0.75f);
    played(r, 0.8f);
    if (!power) played(r + 12 + third, 0.6f);
}

void Layer::playDrum(int row, int offset, float velocity, double beat, double length, int kit, int live) {
    liveId = live;
    switch (row) {
        case RowLow: add(PDrums, offset, 0, velocity, beat, length, 0, 2.0f, live); break;
        case RowMid: add(PDrums, offset, 0, velocity, beat, length, 1, 2.0f, live); break;
        case RowHigh:
            if (kit == KitMetal) add(PMetal, offset, 0, velocity, beat, length, 1, 2.0f, live);
            else if (kit == KitHybrid) add(PMetal, offset, 0, velocity, beat, length, 3, 2.0f, live);
            else if (kit == KitMarch) add(PPerc, offset, 0, velocity, beat, length, 0, 2.0f, live);   // snare ghost strokes
            else add(PHands, offset, 0, velocity, beat, length, ghost ? 1 : 0, 2.0f, live);           // slaps (the ghosts muted)
            break;
        case RowSnare: add(PPerc, offset, 0, velocity, beat, length, 0, 2.0f, live); break;
        case RowTomHigh: add(PPerc, offset, 0, velocity, beat, length, 1, 2.0f, live); break;
        case RowTomLow: add(PPerc, offset, 0, velocity, beat, length, 2, 2.0f, live); break;
        default: add(PMetal, offset, 0, velocity, beat, length, 2, 2.0f, live); break;   // pistons
    }
    liveId = 0;
}

void Layer::playOstinato(int offset, float semis, float velocity, double beat, double length, bool accent, bool onBeat, const Elements& el, int live) {
    add(ostinatoPart, offset, semis + ostinatoRegister, velocity, beat, length, 0, 2.0f, live);
    if (live != 0) return;
    const auto& th = theme(type);
    const float heatV = 0.82f + 0.3f * heat;
    // The parts double as the drama rises: a low thump under the accents (and on every
    // beat at a climax), the brass on the accents at a climax, a clank on a metal kit's.
    if (rankNow == 0 && latched >= Action && (accent || (latched == Climax && onBeat)))
        add(PLow, offset, float(rootNow - 12), (accent ? 0.95f : 0.7f) * heatV, beat, 0.25);
    if (latched == Climax && accent && (th.hitA == HitBrass || th.hitB == HitBrass)) add(PBrassOst, offset, semis, 0.6f * heatV, beat, 0.25);
    if (rankNow == 0 && th.kit == KitMetal && accent && el.notes[0] && el.gainTo[0] > 0.0f) add(PMetal, offset, 0, 0.6f, beat, 0.25, 1);
}

void Layer::playHit(int row, int offset, float velocity, double beat, double length, int placeType, const WorldFrame& world, int chordNow, int64_t bar,
                    float tension, int live) {
    const auto& th = theme(placeType);
    const int mood = world.mood;
    liveId = live;
    const int colour = live == 0 && tension > 0.85f && latched == Climax && (bar & 3) == 2 ? 2 : 0;
    const auto stab = [&](int which) {
        switch (which) {
            case HitBrass: chord(PBrass, offset, world.root, mood, chordNow, velocity, beat, length, false, colour); break;
            case HitChoir: chord(PChoir, offset, world.root, mood, chordNow, velocity, beat, std::max(0.75, length * 1.5), true, colour); break;
            case HitPiano: chord(PPiano, offset, world.root, mood, chordNow, velocity, beat, live != 0 ? length : 1.5, true, colour); break;
            case HitBraam: add(PBraam, offset, float(music::inRange(music::chordTone(mood, chordNow, 0), 0) - 24), velocity, beat, live != 0 ? length : 2.0, 0, 2.0f, live); break;
            case HitSynth: chord(PSynth, offset, world.root, mood, chordNow, velocity, beat, live != 0 ? length : 0.5, true, colour); break;
            default: add(PMetalHit, offset, 0, velocity, beat, 1.0, 1, 2.0f, live); break;
        }
    };
    switch (row) {
        case RowStabA: stab(th.hitA); break;
        case RowStabB: stab(th.hitB); break;
        case RowImpact:
            add(PImpact, offset, 0, velocity, beat, 2.0, 3, 2.0f, live);
            add(PMetalHit, offset, 0, 0.75f * velocity, beat, 2.0, 0, 2.0f, live);
            break;
        default: add(PSwell, offset, 0, velocity, beat, length, 0, 2.0f, live); break;
    }
    liveId = 0;
}

int Layer::advance(int placeType, const WorldFrame& world, const Cue& cue, float tension, double beat, double span) {
    if (placeType != type) {
        // A different Place in the slot: what rang is gone with the old one.
        if (type >= 0) {
            for (auto* p : parts) p->reset();
            hall.clear();
            clearPercussion();
        }
        type = placeType;
        fresh = true;
    }
    x = amountFor(world.intensity, tension, cue.drama);
    static constexpr float edges[TierCount + 1] { 0.0f, 0.1f, 0.45f, 0.78f, 1.0f };
    if (cue.drama <= 0.0f) {
        want = Calm;
        heat = 0;
    } else if (cue.gear >= 0.0f) {
        // The arrangement's gear (the Drama tier rework): the nearest tier, its heat how far
        // through it the section is.
        want = std::clamp(int(std::floor(cue.gear + 0.5f)), int(Calm), int(Climax));
        heat = clamp01(cue.gear - (float(want) - 0.5f));
    } else if (cue.tier >= 0) {
        want = std::clamp(cue.tier, 0, int(Climax));
        heat = 0.6f;
    } else {
        want = tierFor(x);
        heat = clamp01((x - edges[want]) / std::max(0.05f, edges[want + 1] - edges[want]));
    }
    // An evolving section: the Drama instruments harder as it rises (softer as it ends).
    if (cue.heat >= 0.0f && cue.drama > 0.0f) heat = clamp01(cue.heat);
    if (fresh) {
        latched = previous = want;
        fresh = false;
    }
    gearChange = false;
    // A jump (the transport located): latch again at once.
    if (beat < lastBeat - 1.0e-6 || beat > lastBeat + span + 1.0) {
        latched = previous = want;
        lastBar = int64_t(std::floor(beat / 4.0));
    }
    lastBeat = beat;
    // The bar line in this chunk: the tier latches there.
    const auto bar = int64_t(std::ceil(beat / 4.0 - 1.0e-9));
    if (double(bar) * 4.0 < beat + span && bar != lastBar) {
        previous = latched;
        latched = want;
        gearChange = latched > previous;
        gearBar = bar;
        lastBar = bar;
    }
    return latched;
}

void Layer::stingers(int64_t step, int offset, int placeType, int style, const WorldFrame& world, bool sync, double stepBeat, const Cue& cue,
                     const Elements& el) {
    const auto& th = theme(placeType);
    const int pos = int(((step % 16) + 16) % 16);
    const int64_t bar = music::floorDiv(step, int64_t(16));
    const int mood = world.mood;
    const int chordNow = sync ? music::chordDegree(stepBeat, world[Danger], world.harmony, mood) : world.chord;
    const int t = latched;
    const bool leads = cue.rank == 0;
    const double stepLen = 0.25;
    const bool drums = el.notes[0] && el.gainTo[0] > 0.0f, hits = el.notes[2] && el.gainTo[2] > 0.0f;
    // The section's start: a stinger lands there after a Riser, Drum fill, Drop or
    // Reverse swell.
    const double sectionStart = frame.beat - cue.sinceStart;
    const bool landing = cue.arrived != arr::TransCut && cue.arrived != arr::TransCrossfade && cue.arrived != arr::TransFilter
                         && std::abs(stepBeat - sectionStart) < 0.5 * stepLen;
    if (cue.transition == arr::TransDrop && cue.transitionT >= 0.0f) return;
    // Gear change (the tier rose on this bar) or an arrangement landing: impact, crash,
    // a big stab (the chromatic mediant into a climax), a braam (the hits).
    if (hits && pos == 0 && ((gearChange && bar == gearBar && t >= Action) || (landing && t >= Tension))) {
        const bool big = t == Climax || landing;
        // (The Drama loudness pass: these land with the pattern's own hits on the same
        // downbeat, so each is a little softer than it would be alone.)
        if (leads) {
            add(PImpact, offset, 0, loudnessPass ? (big ? 0.9f : 0.75f) : (big ? 1.1f : 0.85f), stepBeat, 2.0, 3);
            add(PMetalHit, offset, 0, loudnessPass ? (big ? 0.7f : 0.5f) : (big ? 0.9f : 0.6f), stepBeat, 2.0, 0);
        }
        const int colour = gearChange && t == Climax && style != StyleTragic && style != StyleSuspense ? 1 : 0;
        const auto stab = [&](int which, float v, double length) {
            switch (which) {
                case HitBrass: chord(PBrass, offset, world.root, mood, chordNow, v, stepBeat, length, false, colour); break;
                case HitChoir: chord(PChoir, offset, world.root, mood, chordNow, v, stepBeat, length * 2.0, false, colour); break;
                case HitPiano: chord(PPiano, offset, world.root, mood, chordNow, v, stepBeat, length * 3.0, true, colour); break;
                case HitBraam: add(PBraam, offset, float(music::inRange(music::chordTone(mood, chordNow, 0), 0) - 24), v, stepBeat, length * 2.0); break;
                case HitSynth: chord(PSynth, offset, world.root, mood, chordNow, v, stepBeat, 0.5, true, colour); break;
                default: add(PMetalHit, offset, 0, v, stepBeat, 1.0, 1); break;
            }
        };
        const float soft = loudnessPass ? 1.0f : 0.0f;
        stab(th.hitA, 0.95f - 0.1f * soft, 1.0);
        stab(th.hitB, 0.85f - 0.1f * soft, 1.0);
        if (th.hitA != HitPiano && th.hitB != HitPiano && big) chord(PPiano, offset, world.root, mood, chordNow, 0.9f - 0.2f * soft, stepBeat, 3.0, true, colour);
        if (th.hitA != HitBraam && th.hitB != HitBraam && big && (placeType >= SpaceStation || th.kit == KitHybrid || th.kit == KitMetal))
            add(PBraam, offset, float(music::inRange(music::chordTone(mood, chordNow, 0), 0) - 24), 0.8f - 0.2f * soft, stepBeat, 2.0);
    }
    // Into the next section: a cymbal swell over the whole transition and a drum roll
    // building over the last bar of a Riser or Reverse swell (a Drum fill plays the drums'
    // Fill).
    if (cue.transitionT >= 0.0f && (cue.transition == arr::TransRiser || cue.transition == arr::TransDrumFill || cue.transition == arr::TransReverse)) {
        const double remaining = cue.transitionLength * (1.0 - double(cue.transitionT)) - (stepBeat - frame.beat);
        if (leads && hits && std::abs(remaining - cue.transitionLength) < 0.5 * stepLen) add(PSwell, offset, 0, loudnessPass ? 0.7f : 0.9f, stepBeat, cue.transitionLength, 0);
        if (leads && drums && cue.transition != arr::TransDrumFill && remaining <= 4.0 + 1.0e-6 && remaining > 0.0 && t >= Tension) {
            const float up = float(1.0 - remaining / 4.0) * (loudnessPass ? 0.8f : 1.0f);   // (the loudness pass: the roll builds less far)
            add(PPerc, offset, 0, 0.35f + 0.65f * up, stepBeat, stepLen, (step & 1) == 0 ? 0 : 1);
            if (remaining <= 1.0 + 1.0e-6) add(PPerc, offset, 0, 0.8f, stepBeat, stepLen, 2);
            if ((step & 1) == 0) add(PDrums, offset, 0, 0.4f + 0.6f * up, stepBeat, stepLen, 0);
        }
    }
}

bool Layer::render(int placeType, const WorldFrame& world, const Cue& cue, float tension, double beat, double beatsPerSample, bool sync,
                   const Elements& el, uint64_t clock, float* left, float* right, int n) {
    const bool liveHere = el.live != nullptr && el.liveElement >= 0 && el.liveElement < dramaElements;
    // Classic (Drama 0): the layer does not exist (unless the keyboard plays it).
    if (cue.drama <= 0.0f && !awake && !liveHere) return false;
    if (placeType <= NoPlace) return false;
    const double span = beatsPerSample * double(n);
    const bool transitions = cue.drama > 0.0f && (cue.transitionT >= 0.0f || cue.sinceStart < 0.5);
    if (want <= Calm && latched <= Calm && !transitions && !liveHere && idleSamples > int(6.0f * rate)) {
        // Asleep: no work; it keeps its place in the song (advance) and wakes on a bar line.
        if (awake) {
            for (auto* p : parts) p->reset();
            hall.clear();
            clearPercussion();
            limiter.reset();
            awake = false;
        }
        return false;
    }
    awake = true;
    rankNow = cue.rank;
    const int style = styleFor(placeType, cue.style);
    frame.rate = rate;
    frame.root = world.root;
    frame.mood = world.mood;
    frame.chord = world.chord;
    frame.quality = std::clamp(world.quality, 0, 2);
    frame.beat = beat;
    frame.beatsPerSecond = beatsPerSample * double(rate);
    // The patches change slowly: set again only when what shapes them moves (on a grid).
    {
        const auto q = [](float v) { return uint64_t(std::clamp(int(v * 32.0f), 0, 63)); };
        const uint64_t key = uint64_t(placeType) | (uint64_t(style) << 6) | (q(heat) << 10) | (q(tension) << 16) | (q(world[Era]) << 22)
                             | (q(world[Danger]) << 28) | (uint64_t(latched) << 34) | (uint64_t(cue.rank) << 37) | (uint64_t(std::clamp(world.quality, 0, 2)) << 40) | (uint64_t(1) << 43);
        if (key != patchKey) {
            configure(placeType, world, style, tension, float(frame.beatsPerSecond * 60.0));
            patchKey = key;
        }
    }

    count = 0;
    const int mood = world.mood;
    const float heatV = 0.82f + 0.3f * heat;
    // A Drop's silence; the roll into a Riser or Reverse swell takes the leading slot's last bar.
    const bool dropping = cue.transition == arr::TransDrop && cue.transitionT >= 0.0f;
    const auto rolling = [&](double at) {
        if (cue.rank != 0 || cue.transitionT < 0.0f || latched < Tension) return false;
        if (cue.transition != arr::TransRiser && cue.transition != arr::TransReverse) return false;
        const double remaining = cue.transitionLength * (1.0 - double(cue.transitionT)) - (at - beat);
        return remaining <= 4.0 + 1.0e-6 && remaining > 0.0;
    };
    // The three Drama instruments' patterns (they rest at Calm).
    if (latched > Calm && !dropping)
        for (int k = 0; k < dramaElements; ++k) {
            const auto* play = el.play[std::size_t(k)];
            if (play == nullptr || !play->active() || !el.notes[std::size_t(k)] || el.gainTo[std::size_t(k)] <= 0.0f) continue;
            play->play(beat, span, [&](const pat::Event& ev) {
                if (rolling(ev.beat)) return;
                const int offset = std::clamp(int(std::ceil((ev.beat - beat) / beatsPerSample - 1.0e-9)), 0, n - 1);
                eventLog[std::size_t(logged++ % logSize)] = { clock + uint64_t(offset), placeElements + k, ev.degree, ev.velocity };
                const auto step = int64_t(std::floor(ev.beat * 4.0 + 1.0e-6));
                const int64_t bar = music::floorDiv(step, int64_t(16));
                const int chordAt = sync ? music::chordDegree(ev.beat, world[Danger], world.harmony, mood) : world.chord;
                if (k == 0) {
                    ghost = ev.velocity < 0.4f;
                    playDrum(std::clamp(ev.degree, 0, DrumRowCount - 1), offset, ev.velocity * heatV * (0.93f + 0.14f * rand(step, 41u + uint32_t(ev.degree))), ev.beat,
                             std::max(0.25, ev.length), kitNow);
                    ghost = false;
                } else if (k == 1) {
                    const float semis = float(pat::semitonesFor(play->followChords, play->base, mood, chordAt, ev.degree, ev.alter));
                    rootNow = play->followChords ? music::inRange(music::chordTone(mood, chordAt, 0), 0) : 0;
                    const float rankGain = cue.rank == 0 ? 1.0f : (cue.rank == 1 ? 0.7f : 0.6f);
                    const double length = ev.length <= 0.25 + 1.0e-6 ? 0.6 * ev.length : ev.length;   // single steps short (spiccato)
                    const bool onBeat = std::abs(ev.beat - std::round(ev.beat)) < 1.0e-3;
                    playOstinato(offset, semis, ev.velocity * heatV * rankGain * (0.94f + 0.12f * rand(step, 51u)), ev.beat, length, ev.velocity >= 0.9f, onBeat, el);
                } else {
                    playHit(std::clamp(ev.degree, 0, HitRowCount - 1), offset, ev.velocity * heatV * (cue.rank >= 2 ? 0.75f : 1.0f), ev.beat, ev.length, placeType,
                            world, chordAt, bar, tension);
                }
            });
        }
    // The arrangement's and the gear changes' own sounds, step by step.
    if (cue.drama > 0.0f)
        forEachStep(beat, span, 4, 0.0f, [&](int64_t step) {
            const double stepBeat = double(step) / 4.0;
            const int offset = std::clamp(int(std::ceil((stepBeat - beat) / beatsPerSample - 1.0e-9)), 0, n - 1);
            stingers(step, offset, placeType, style, world, sync, stepBeat, cue, el);
        });
    // The keyboard (KEYS, Instrument mode) on one of the Drama instruments.
    if (liveHere)
        for (int i = 0; i < el.live->count; ++i) {
            const auto& x = el.live->notes[std::size_t(i)];
            const double at = beat + beatsPerSample * double(x.offset);
            if (!x.on) {
                for (int k = 0; k < PartCount && count < maxNotes; ++k)
                    if (elementOf(k) == el.liveElement) {
                        Note& note = pending[std::size_t(count++)];
                        note = Note {};
                        note.offset = x.offset;
                        note.part = k;
                        note.stop = x.id;
                    }
                continue;
            }
            const int64_t step = int64_t(std::floor(at * 4.0));
            if (el.liveElement == 0) {
                ghost = false;
                playDrum(std::clamp(x.row, 0, DrumRowCount - 1), x.offset, x.velocity, at, 0.25, kitNow, x.id);
            } else if (el.liveElement == 1) {
                playOstinato(x.offset, x.semis - transposeSemis(DramaOstinato), x.velocity, at, liveLength, false, false, el, x.id);
            } else {
                playHit(std::clamp(x.row, 0, HitRowCount - 1), x.offset, x.velocity, at, x.row == RowSwell ? 4.0 : 1.0, placeType, world,
                        sync ? music::chordDegree(at, world[Danger], world.harmony, mood) : world.chord, music::floorDiv(step, int64_t(16)), tension, x.id);
            }
        }
    // Sort by offset (insertion, stable: a note-off before a new note on the same sample).
    for (int i = 1; i < count; ++i) {
        const Note v = pending[std::size_t(i)];
        int j = i;
        while (j > 0 && pending[std::size_t(j - 1)].offset > v.offset) {
            pending[std::size_t(j)] = pending[std::size_t(j - 1)];
            --j;
        }
        pending[std::size_t(j)] = v;
    }
    idleSamples = count > 0 ? 0 : idleSamples + n;
    for (int i = 0; i < count; ++i) startedCount += pending[std::size_t(i)].stop == 0 ? 1u : 0u;

    // The parts' levels: what each tier lets play (a part whose last note has rung out
    // is not rendered at all), summed per Drama instrument. The orchestration pass: the
    // percussion (the drums, impacts, swells, the low thump) is summed apart from the
    // melodic parts (the ostinato, the stabs) and leaves on a dry bus of its own.
    const bool split = mixPass && loudnessPass;
    std::array<bool, dramaElements> fed {}, fedPerc {};
    const float tierGain = el.instrument ? 0.8f
                                         : std::max(latched == Climax ? 1.0f : (latched == Action ? 0.8f : (latched == Tension ? 0.55f : 0.5f)), liveHere ? 0.8f : 0.0f);
    for (int k = 0; k < PartCount; ++k) {
        auto* part = parts[std::size_t(k)];
        bool any = false;
        for (int i = 0; i < count && !any; ++i) any = pending[std::size_t(i)].part == k;
        auto& q = quiet[std::size_t(k)];
        if (any) q = 0;
        // A part whose last note has rung out (well below -60 dB) is neither updated nor
        // rendered: an idle part costs nothing.
        static constexpr std::array<float, PartCount> tails { 5.0f, 7.0f, 5.0f, 24.0f, 3.0f, 4.0f, 4.0f, 4.0f, 2.0f, 3.0f, 5.0f, 10.0f, 6.0f, 4.0f, 3.0f,
                                                              3.0f, 10.0f, 5.0f, 7.0f };
        if (!any && q > int(tails[std::size_t(k)] * rate) && !(liveHere && elementOf(k) == el.liveElement && part->sustains())) continue;
        q += n;
        const int e = elementOf(k);
        // The drums' instrument is all percussion: its own bus is the percussion's.
        const bool apart = split && isPercussion(k) && e != 0;
        auto& feedFlag = apart ? fedPerc[std::size_t(e)] : fed[std::size_t(e)];
        auto& l = apart ? percElemL[std::size_t(e)] : elemL[std::size_t(e)];
        auto& r = apart ? percElemR[std::size_t(e)] : elemR[std::size_t(e)];
        if (!feedFlag) {
            std::fill(l.begin(), l.begin() + n, 0.0f);
            std::fill(r.begin(), r.begin() + n, 0.0f);
            feedFlag = true;
        }
        part->update(frame);
        std::fill(partL.begin(), partL.begin() + n, 0.0f);
        std::fill(partR.begin(), partR.begin() + n, 0.0f);
        int done = 0;
        for (int i = 0; i < count; ++i) {
            const auto& note = pending[std::size_t(i)];
            if (note.part != k) continue;
            if (note.offset > done) {
                part->render(partL.data() + done, partR.data() + done, note.offset - done);
                done = note.offset;
            }
            if (note.stop != 0) part->stopLive(note.stop);
            else if ((partMask >> k) & 1u) part->start(frame, note.on);
        }
        if (done < n) part->render(partL.data() + done, partR.data() + done, n - done);
        // At a climax the drums lead and what sustains steps back, so it stays punchy (the
        // orchestration pass: the drums drive without leading, the melodic parts a little back).
        const bool rhythm = k == PDrums || k == PPerc || k == PMetal || k == PLow || k == PImpact || k == PMetalHit;
        const float climaxShape = latched == Climax ? (split ? (rhythm ? 1.0f : 0.9f) : (rhythm ? 1.12f : 0.8f)) : 1.0f;
        const float trim = split ? (isPercussion(k) ? percussionTrim * el.percGain : melodicTrim) : 1.0f;
        const float g = tierGain * climaxShape * trim * dbToGain((loudnessPass ? trimDb : trimBeforeDb)[std::size_t(k)]);
        float& now = gainNow[std::size_t(k)];
        if (now < 0.0f) now = g;
        const float step = (g - now) / float(n);
        for (int i = 0; i < n; ++i) {
            now += step;
            l[std::size_t(i)] += finiteOr(partL[std::size_t(i)]) * now;
            r[std::size_t(i)] += finiteOr(partR[std::size_t(i)]) * now;
        }
    }
    // Each Drama instrument at its level (switch, mute, arrangement), through its effects.
    std::fill(busL.begin(), busL.begin() + n, 0.0f);
    std::fill(busR.begin(), busR.begin() + n, 0.0f);
    bool percussion = false;
    if (split) {
        std::fill(percL.begin(), percL.begin() + n, 0.0f);
        std::fill(percR.begin(), percR.begin() + n, 0.0f);
    }
    const auto applyLevel = [&](int e, std::array<float, dsp::chunk>& l, std::array<float, dsp::chunk>& r) {
        const float from = el.gainFrom[std::size_t(e)], to = el.gainTo[std::size_t(e)];
        if (from != 1.0f || to != 1.0f)
            for (int i = 0; i < n; ++i) {
                const float g = from + (to - from) * float(i + 1) / float(n);
                l[std::size_t(i)] *= g;
                r[std::size_t(i)] *= g;
            }
    };
    for (int e = 0; e < dramaElements; ++e) {
        if (fedPerc[std::size_t(e)]) {
            // The impacts, swells and the low thump: the instrument's level, no sends.
            auto& l = percElemL[std::size_t(e)];
            auto& r = percElemR[std::size_t(e)];
            applyLevel(e, l, r);
            for (int i = 0; i < n; ++i) {
                percL[std::size_t(i)] += l[std::size_t(i)];
                percR[std::size_t(i)] += r[std::size_t(i)];
            }
            percussion = true;
        }
        if (!fed[std::size_t(e)]) continue;
        auto& l = elemL[std::size_t(e)];
        auto& r = elemR[std::size_t(e)];
        applyLevel(e, l, r);
        const bool drumsBus = split && e == 0;
        // The orchestration pass: the drums send a quarter of their reverb (and delay), the
        // hits' stabs 60 %, both through a low-cut, so the shared reverbs get the attack.
        const float sendScale = split ? el.fxDepth * (drumsBus ? 0.25f : (e == 2 ? 0.6f : 1.0f)) : el.fxDepth;
        if (el.fx != nullptr) el.fx->process(placeElements + e, l.data(), r.data(), n, sendScale, split ? (drumsBus ? 300.0f : 150.0f) : 0.0f);
        auto& toL = drumsBus ? percL : busL;
        auto& toR = drumsBus ? percR : busR;
        for (int i = 0; i < n; ++i) {
            toL[std::size_t(i)] += l[std::size_t(i)];
            toR[std::size_t(i)] += r[std::size_t(i)];
        }
        percussion |= drumsBus;
    }
    // The look-ahead limiters on the layer's sums (a pure 1.5 ms delay while below their
    // ceiling), their ceiling where the slot's balance puts the layer.
    limiter.ceiling = limiterCeiling / std::clamp(el.slotGain, 0.05f, 1.0f);
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        if (loudnessPass) limiter.process(busL[k], busR[k]);
        peakOut = std::max(peakOut, std::max(std::abs(busL[k]), std::abs(busR[k])));
    }
    if (split) {
        percLimiter.ceiling = limiter.ceiling;
        if (percussion || percTail > 0)
            for (int i = 0; i < n; ++i) {
                const auto k = std::size_t(i);
                percLimiter.process(percL[k], percR[k]);
                peakOut = std::max(peakOut, std::max(std::abs(percL[k]), std::abs(percR[k])));
            }
    }
    // The layer's space and level (the second and further slots quieter).
    const float rankGain = cue.rank == 0 ? 1.0f : (cue.rank == 1 ? 0.75f : 0.55f);
    const float target = dbToGain(layerTrimDb) * rankGain * (0.75f + 0.25f * clamp01(cue.drama));
    const float from = level.value, to = level.next(target);
    // The orchestration pass: the melodic parts' hall a little drier, its input low-cut.
    const float wet = el.instrument ? 0.0f : hallWet * (latched == Climax ? 0.65f : 1.0f) * (split ? 0.75f : 1.0f);
    // The leading slot's layer has its own space; the others' reach the World Space with
    // their Place (one reverb, not four).
    if (cue.rank == 0 && wet > 0.0f) {
        for (int i = 0; i < n; ++i) {
            const auto k = std::size_t(i);
            float wl = 0, wr = 0;
            if (split) hall.process(hallCut[1].hp(hallCut[0].hp(busL[k] * 0.6f)), hallCut[3].hp(hallCut[2].hp(busR[k] * 0.6f)), wl, wr);
            else hall.process(busL[k] * 0.6f, busR[k] * 0.6f, wl, wr);
            const float g = lerp(from, to, float(i + 1) / float(n));
            left[i] += (busL[k] + wl * wet) * g;
            right[i] += (busR[k] + wr * wet) * g;
        }
    } else {
        for (int i = 0; i < n; ++i) {
            const float g = lerp(from, to, float(i + 1) / float(n)) * (1.0f + 0.5f * wet);
            left[i] += busL[std::size_t(i)] * g;
            right[i] += busR[std::size_t(i)] * g;
        }
    }
    // The percussion: tight and dry. The low end held (a high-pass at 40 Hz, 3 dB less below
    // about 110 Hz), a short room (0.35 s, its input high-passed at 300 Hz) at 10 % for the
    // leading slot, nothing for the others; out on the slot's dry bus (no World Space, no
    // fullness) when the slot gives one.
    if (split && (percussion || percTail > 0)) {
        percTail = percussion ? int(0.6f * rate) : std::max(0, percTail - n);
        const float roomWet = el.instrument ? 0.0f : (cue.rank == 0 ? 0.1f : 0.0f);
        float* outL = el.percL != nullptr ? el.percL : left;
        float* outR = el.percR != nullptr ? el.percR : right;
        for (int i = 0; i < n; ++i) {
            const auto k = std::size_t(i);
            float l = percHp[0].tick(percL[k]).hp, r = percHp[1].tick(percR[k]).hp;
            l -= 0.29f * percShelf[0].lp(l);
            r -= 0.29f * percShelf[1].lp(r);
            if (roomWet > 0.0f) {
                float wl = 0, wr = 0;
                room.process(roomCut[1].hp(roomCut[0].hp(l)), roomCut[3].hp(roomCut[2].hp(r)), wl, wr);
                l += wl * roomWet;
                r += wr * roomWet;
            }
            const float g = lerp(from, to, float(i + 1) / float(n));
            outL[i] += l * g;
            outR[i] += r * g;
        }
    }
    return true;
}

} // namespace wb::drama
