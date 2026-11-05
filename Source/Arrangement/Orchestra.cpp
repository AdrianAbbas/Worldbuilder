#include "Orchestra.h"
#include <vector>

namespace wb::arr {

int voiceOf(int type, int element) {
    if (element == DramaDrums) return VoiceKit;
    if (element == DramaOstinato) return VoiceOstinato;
    if (element == DramaHits) return VoiceHits;
    if (type <= NoPlace || type >= PlaceTypeCount || element < 0 || element >= placeElements) return VoiceColour;
    const auto& spec = elementSpec(type, element);
    switch (roleOf(type, element)) {
        case RoleBass: return VoiceBass;
        case RoleRhythm: return spec.kind == Kit ? VoiceKit : VoiceOstinato;
        case RoleMelody: return spec.sustained ? VoicePad : VoiceLead;
        case RoleBed: return VoicePad;
        case RoleTexture: return VoiceEnvironment;
        default: return VoiceColour;
    }
}

Budget budgetOf(int kind) {
    switch (kind) {
        case KindIntro: return { 2, 4 };
        case KindBuild: return { 4, 7 };
        case KindPeak: return { 7, 10 };
        case KindCalm: return { 3, 5 };
        case KindBreak: return { 2, 4 };
        case KindOutro: return { 2, 3 };
        case KindLoop: return { 5, 8 };
        default: return { 4, 7 };   // a bridge
    }
}

namespace {

// The voices a kind of section asks for, in order (the budget cuts the list).
const std::vector<int>& planFor(int kind) {
    static const std::array<std::vector<int>, KindCount> plans { {
        { VoicePad, VoiceEnvironment, VoiceLead, VoiceColour },                                                                  // intro
        { VoicePad, VoiceBass, VoiceOstinato, VoiceKit, VoiceLead, VoiceColour, VoiceEnvironment },                             // build
        { VoiceLead, VoiceBass, VoiceKit, VoiceOstinato, VoicePad, VoiceColour, VoiceHits, VoicePad, VoiceColour, VoiceEnvironment },   // peak
        { VoicePad, VoiceBass, VoiceLead, VoiceColour, VoiceEnvironment },                                                      // wind-down
        { VoicePad, VoiceEnvironment, VoiceLead, VoiceColour },                                                                  // break
        { VoicePad, VoiceLead, VoiceEnvironment },                                                                               // outro
        { VoiceLead, VoiceBass, VoiceKit, VoicePad, VoiceOstinato, VoiceColour, VoiceEnvironment, VoicePad },                   // loop
        { VoiceLead, VoicePad, VoiceBass, VoiceOstinato, VoiceColour, VoiceKit, VoiceEnvironment },                             // bridge
    } };
    return plans[std::size_t(std::clamp(kind, 0, KindCount - 1))];
}
// Which Place a voice prefers, counted from the section's lead: the lead's own melody and
// environment; the bass, the colour and the ostinato from the next Place; the kit and the
// pads from the one after (the other Places fill the other voices).
constexpr std::array<int, VoiceCount> preferOffset { 0, 1, 2, 1, 2, 0, 1, 0 };
// Kinds in which the drama may play (a tier above Calm): their Drama instruments are scored.
bool dramaKind(int kind) { return kind == KindBuild || kind == KindPeak || kind == KindLoop || kind == KindBridge || kind == KindCalm; }

struct Choice {
    int slot = -1, element = -1;
};

} // namespace

Arrangement orchestrate(const Arrangement& in, const std::array<int, placeSlots>& types, uint32_t seed) {
    Arrangement a = in;
    std::array<int, placeSlots> active {};
    std::array<int, placeSlots> rankOf {};
    rankOf.fill(-1);
    int n = 0;
    for (int slot = 0; slot < placeSlots; ++slot)
        if (types[std::size_t(slot)] != NoPlace) {
            rankOf[std::size_t(slot)] = n;
            active[std::size_t(n++)] = slot;
        }
    if (n == 0) return a;
    const auto has = [&](int slot, int e) { return patternInfo(types[std::size_t(slot)], e).kind != NoPattern; };
    int leads = 0;
    for (int i = 0; i < a.count; ++i) {
        auto& s = a.sections[std::size_t(i)];
        const int kind = kindOf(a, i);
        const auto budget = budgetOf(kind);
        const bool drama = dramaKind(kind);
        // The lead passes from Place to Place, section by section (the seed picks where it starts).
        const int leadPos = (leads++ + int(seed % uint32_t(n))) % n;
        std::array<std::array<bool, slotElements>, placeSlots> chosen {};
        std::array<int, VoiceCount> used {};
        int count = 0, leadSlot = -1;
        const auto pick = [&](int voice, bool counted) -> Choice {
            for (int k = 0; k < n; ++k) {
                const int slot = active[std::size_t((leadPos + preferOffset[std::size_t(voice)] + k) % n)];
                const int rank = rankOf[std::size_t(slot)];
                const int type = types[std::size_t(slot)];
                for (int e = 0; e < slotElements; ++e) {
                    if (chosen[std::size_t(slot)][std::size_t(e)] || !has(slot, e) || voiceOf(type, e) != voice) continue;
                    if (isDramaElement(e)) {
                        // The Drama instruments where the drama may play, and where their slot has
                        // notes: the drums on the leading slot, the ostinato on the first two.
                        if (!drama || (e == DramaDrums && rank > 0) || (e == DramaOstinato && rank > 1)) continue;
                        // A Place's own kit or ostinato first; the Drama one takes over at runtime.
                        if (counted && (voice == VoiceKit || voice == VoiceOstinato)) continue;
                    }
                    return { slot, e };
                }
            }
            (void) counted;
            return {};
        };
        const auto take = [&](const Choice& c, bool counted) {
            if (c.slot < 0) return false;
            chosen[std::size_t(c.slot)][std::size_t(c.element)] = true;
            if (counted) {
                ++used[std::size_t(voiceOf(types[std::size_t(c.slot)], c.element))];
                ++count;
            }
            return true;
        };
        for (int voice : planFor(kind)) {
            if (count >= budget.high) break;
            if (used[std::size_t(voice)] >= voiceLimit[std::size_t(voice)]) continue;
            Choice c = pick(voice, true);
            // No Place has the voice: a kit or an ostinato from the Drama instruments.
            if (c.slot < 0 && drama && (voice == VoiceKit || voice == VoiceOstinato))
                for (int k = 0; k < n && c.slot < 0; ++k) {
                    const int slot = active[std::size_t(k)];
                    const int e = voice == VoiceKit ? int(DramaDrums) : int(DramaOstinato);
                    if (has(slot, e) && rankOf[std::size_t(slot)] <= (voice == VoiceKit ? 0 : 1)) c = { slot, e };
                }
            if (take(c, true) && voice == VoiceLead) leadSlot = c.slot;
        }
        // Below the budget (Places with few instruments): more colour, pads, an environment.
        for (int voice : { int(VoiceColour), int(VoicePad), int(VoiceEnvironment), int(VoiceOstinato), int(VoiceBass) }) {
            while (count < budget.low && used[std::size_t(voice)] < voiceLimit[std::size_t(voice)]) {
                if (!take(pick(voice, true), true)) break;
            }
        }
        // While the drama plays, the Drama drums take over from the Place's kit and the Drama
        // ostinato from the Place's (one kit, one ostinato: the shaping swaps them).
        if (drama) {
            bool kit = false, ost = false;
            for (int slot = 0; slot < placeSlots; ++slot)
                for (int e = 0; e < placeElements; ++e)
                    if (chosen[std::size_t(slot)][std::size_t(e)]) {
                        kit |= voiceOf(types[std::size_t(slot)], e) == VoiceKit;
                        ost |= voiceOf(types[std::size_t(slot)], e) == VoiceOstinato;
                    }
            if (kit)
                for (int k = 0; k < n; ++k)
                    if (rankOf[std::size_t(active[std::size_t(k)])] == 0 && has(active[std::size_t(k)], DramaDrums)) take({ active[std::size_t(k)], DramaDrums }, false);
            if (ost) {
                const int slot = active[std::size_t(std::min(n - 1, (leadPos + 1) % std::min(n, 2)))];
                if (has(slot, DramaOstinato)) take({ slot, DramaOstinato }, false);
            }
        }
        if (leadSlot < 0) leadSlot = active[std::size_t(leadPos)];
        // The cells: a part kept plays as written; one brought in plays its kind's way.
        const bool lastSection = i == a.count - 1;
        for (int slot = 0; slot < placeSlots; ++slot) {
            const int type = types[std::size_t(slot)];
            for (int e = 0; e < slotElements; ++e) {
                auto& c = s.cells[std::size_t(slot)][std::size_t(e)];
                if (type == NoPlace || !chosen[std::size_t(slot)][std::size_t(e)]) {
                    c.entry = EntryOff;
                    continue;
                }
                if (c.entry != EntryOff) continue;
                const int voice = voiceOf(type, e);
                c.entry = EntryOn;
                c.variant = VariantA;
                switch (kind) {
                    case KindIntro:
                        if (voice == VoiceLead) {
                            c.entry = s.bars >= 8 ? EntryFadeIn : EntryOn;
                            c.variant = VariantC;
                        } else if (voice == VoiceColour) c.variant = VariantC;
                        break;
                    case KindOutro:
                        if (voice == VoiceLead || voice == VoiceEnvironment) c.entry = EntryFadeOut;
                        else if (voice == VoicePad && lastSection && a.end == EndStop) c.entry = EntryFadeOut;
                        if (voice == VoiceLead) c.variant = VariantC;
                        break;
                    case KindBreak:
                    case KindCalm:
                        if (voice == VoiceLead || voice == VoiceColour) c.variant = VariantC;
                        break;
                    case KindPeak:
                        if (voice == VoiceKit || voice == VoiceOstinato) c.variant = VariantB;
                        break;
                    default: break;
                }
            }
        }
        s.orchestrated = true;
        s.lead = int8_t(leadSlot);
    }
    return a;
}

Sounding sounding(const State& st, const Arrangement& a, const Shaping& sh) {
    Sounding on {};
    if (!st.active || st.at.ended) return on;
    const int tier = st.drama >= 0 ? st.drama : sectionTier(a, st.at.section, sh);
    int rank = 0;
    for (int slot = 0; slot < placeSlots; ++slot) {
        const int type = sh.types[std::size_t(slot)];
        if (type == NoPlace) continue;
        for (int e = 0; e < slotElements; ++e) {
            if (patternInfo(type, e).kind == NoPattern || st.gain[std::size_t(slot)][std::size_t(e)] <= 0.0f) continue;
            if (isDramaElement(e)) {
                if (sh.drama <= 0.0f || tier <= drama::Calm) continue;
                if ((e == DramaDrums && rank > 0) || (e == DramaOstinato && rank > 1)) continue;
            }
            on[std::size_t(slot)][std::size_t(e)] = true;
        }
        ++rank;
    }
    return on;
}

int countParts(const Sounding& on) {
    int n = 0;
    for (const auto& row : on)
        for (bool b : row) n += b ? 1 : 0;
    return n;
}

int countVoice(const Sounding& on, const std::array<int, placeSlots>& types, int voice) {
    int n = 0;
    for (int slot = 0; slot < placeSlots; ++slot)
        for (int e = 0; e < slotElements; ++e)
            if (on[std::size_t(slot)][std::size_t(e)] && voiceOf(types[std::size_t(slot)], e) == voice) ++n;
    return n;
}

//==============================================================================
// Lengths by time.

double lengthSeconds(float position) {
    static constexpr std::array<double, 4> anchors { 60.0, 210.0, 300.0, 480.0 };
    const float p = std::clamp(std::isfinite(position) ? position : lengthDefault, lengthMin, lengthMax);
    const int k = std::min(2, int(std::floor(p)));
    const double t = double(p) - double(k);
    return anchors[std::size_t(k)] * std::pow(anchors[std::size_t(k + 1)] / anchors[std::size_t(k)], t);
}

namespace {
int phraseOf(double bars) { return bars < 12.0 ? 4 : (bars < 64.0 ? 8 : 16); }
} // namespace

Arrangement fitLength(const Arrangement& in, double seconds, double bpm) {
    Arrangement a = in;
    if (a.count <= 0 || !(seconds > 0.0) || !(bpm > 0.0)) return a;
    const double target = seconds * bpm / 60.0 / double(beatsPerBar);   // bars on the timeline
    const int reach = std::min(a.firstEndless() + 1, a.count);           // sections after an endless one are never reached
    double now = 0;
    for (int i = 0; i < reach; ++i) now += double(a.sections[std::size_t(i)].span());
    if (now <= 0.0) return a;
    const double factor = target / now;
    std::array<double, maxSections> want {};
    for (int i = 0; i < reach; ++i) {
        auto& s = a.sections[std::size_t(i)];
        want[std::size_t(i)] = double(s.bars) * factor;
        const int q = phraseOf(want[std::size_t(i)]);
        s.bars = uint8_t(std::clamp(int(std::lround(want[std::size_t(i)] / q)) * q, std::min<int>(q, maxBars), maxBars));
    }
    // Into 5 % of the target where the phrases allow: a phrase more (or less) where the
    // rounding left the most behind, the main sections first.
    for (int step = 0; step < 64; ++step) {
        double total = 0;
        for (int i = 0; i < reach; ++i) total += double(a.sections[std::size_t(i)].span());
        const double error = total - target;
        if (std::abs(error) <= 0.05 * target) break;
        // The change that brings the total nearest the target (a tie: the section the rounding
        // left furthest behind; growing, a climax or a loop first; shrinking, the others).
        int best = -1;
        double bestError = std::abs(error), bestScore = -1.0e18;
        for (int i = 0; i < reach; ++i) {
            const auto& s = a.sections[std::size_t(i)];
            const int q = phraseOf(double(s.bars));
            if (error < 0.0 && int(s.bars) + q > maxBars) continue;
            if (error > 0.0 && int(s.bars) - q < q) continue;
            const double after = std::abs(error + (error < 0.0 ? 1.0 : -1.0) * double(q) * double(std::max<int>(s.repeats, 1)));
            const int kind = kindOf(a, i);
            const bool main = kind == KindPeak || kind == KindLoop;
            const double score = (want[std::size_t(i)] - double(s.bars)) * (error < 0.0 ? 1.0 : -1.0) + (main == (error < 0.0) ? 4.0 : 0.0);
            if (after < bestError - 1.0e-9 || (best >= 0 && std::abs(after - bestError) < 1.0e-9 && score > bestScore)) {
                bestError = after;
                bestScore = score;
                best = i;
            }
        }
        if (best < 0) break;
        auto& s = a.sections[std::size_t(best)];
        const int q = phraseOf(double(s.bars));
        s.bars = uint8_t(int(s.bars) + (error < 0.0 ? q : -q));
    }
    for (int i = 0; i < reach; ++i) {
        auto& s = a.sections[std::size_t(i)];
        if (s.bars == in.sections[std::size_t(i)].bars) continue;
        // A transition no longer than the section; the clips fitted (shorter: cut; longer: repeated).
        s.transitionLength = uint8_t(std::min<int>(s.transitionLength, std::max(1, s.transition == TransDrop ? 4 : int(s.bars))));
        for (auto& row : s.cells)
            for (auto& cell : row)
                if (cell.clip != nullptr && cell.clip->bars != s.bars) {
                    auto c = std::make_shared<pat::Clip>(*cell.clip);
                    const int from = cell.clip->bars > 0 ? int(cell.clip->bars) : 1, to = int(s.bars);
                    pat::Clip out = *c;
                    out.setLength(c->resolution, to);
                    out.clear();
                    const int perBar = pat::stepsPerBar(c->resolution);
                    for (int pass = 0; pass * from < to; ++pass)
                        for (int k = 0; k < c->count; ++k) {
                            auto note = c->notes[std::size_t(k)];
                            const int at = int(note.step) + pass * from * perBar;
                            if (at >= out.lengthSteps) break;
                            note.step = int16_t(at);
                            out.add(note);
                        }
                    *c = out;
                    cell.clip = c;
                }
    }
    return a;
}

Arrangement makeTemplate(int which, double seconds, double bpm) { return fitLength(makeTemplate(which, false), seconds, bpm); }

Arrangement makeTemplate(int which) { return makeTemplate(which, lengthSeconds(lengthDefault), 120.0); }

int newSectionBars(int kind, double seconds, double bpm) {
    // A kind's share of the Classic Arc's 144 bars (a loop or a bridge as a fight, a break
    // as an intro), at the Length and tempo.
    static constexpr std::array<double, KindCount> share { 16, 32, 56, 24, 16, 16, 56, 32 };
    const double bars = share[std::size_t(std::clamp(kind, 0, KindCount - 1))] / 144.0 * seconds * bpm / 60.0 / double(beatsPerBar);
    const int q = phraseOf(bars);
    return std::clamp(int(std::lround(bars / q)) * q, std::min(q, maxBars), maxBars);
}

} // namespace wb::arr
