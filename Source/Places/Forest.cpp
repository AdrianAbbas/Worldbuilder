#include "Places.h"

namespace wb {

using namespace dsp;

ForestPlace::ForestPlace() : EnsemblePlace(1600) { setParts({ &flute, &birds, &wood, &kalimbaRec, &stringsRec, &rustle }); }

void ForestPlace::prepareFx() {
    diffuser.prepare(rate);
    space.prepare(rate);
}

void ForestPlace::resetFx() {
    diffuser.clear();
    space.clear();
    for (auto& f : tilt) f.reset();
}

void ForestPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 1601);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size;
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Flute) {
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = true;
        out.base = in.character == DarkWoods ? 0 : 12;
        // A phrase that walks the scale from a chord tone and comes to rest on one.
        const auto phrase = [&](int at, int notes, const int* lengths, int count, uint32_t salt, float velocity) {
            int degree = 2 * int(ch(salt, at) * 3.0f);
            int pos = at;
            for (int k = 0; k < notes && pos < out.lengthSteps; ++k) {
                const bool last = k == notes - 1;
                const int length = last ? lengths[0] * 2 : lengths[k % count];
                w.note(pos, degree, length, velocity * (k == 0 ? 1.0f : 0.85f + 0.2f * ch(salt + 1, at + k)));
                pos += length;
                const float move = ch(salt + 2, at + k);
                degree += move < 0.45f ? -1 : (move < 0.85f ? 1 : 2);
                degree = std::clamp(degree, -2, size + 4);
                if (k == notes - 2) degree = 2 * int(std::round(float(degree) / 2.0f));
            }
        };
        switch (in.character) {
            case Enchanted: {   // floating, long notes
                static constexpr int lengths[3] { 6, 4, 6 };
                bool any = false;
                for (int bar = 0; bar < 8; bar += 2)
                    if (ch(1, bar) < 0.3f + 0.5f * act || (bar == 6 && !any)) {
                        phrase(bar * 16 + 2, 3 + int(ch(2, bar) * 3.0f), lengths, 3, 3, 0.7f);
                        any = true;
                    }
                break;
            }
            case DarkWoods:   // low, sparse, a sigh falling a step
                for (int bar = 0; bar < 8; bar += 4) {
                    const int d = 2 * int(ch(5, bar) * 3.0f) + 1;
                    w.note(bar * 16 + 4, d, 12, 0.6f);
                    w.note(bar * 16 + 16, d - 1, 16, 0.5f);
                    if (ch(6, bar) < act) w.note(bar * 16 + 40, d + 1, 8, 0.45f);
                }
                break;
            default: {   // Morning Glade: a lilting dance in dotted rhythms
                static constexpr int lengths[2] { 3, 1 };
                for (int bar = 0; bar < 8; ++bar)
                    if (ch(8, bar) < 0.35f + 0.5f * act || bar == 0) phrase(bar * 16, 4 + int(ch(9, bar) * 4.0f * density), lengths, 2, 10, 0.75f);
                break;
            }
        }
        return;
    }

    if (element == Birds) {
        // Bird calls high in the key: melodic calls and trills, owl-like hoots in the
        // dark, a dawn chorus in the glade. Four bars. Tension quietens them.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        const float calm = 1.0f - 0.6f * tension;
        const auto high = [&](uint32_t salt, int64_t at) { return 2 * size + int(ch(salt, at) * float(size)); };
        switch (in.character) {
            case Enchanted:   // a rising three-note call, sometimes answered by a trill
                for (int bar = 0; bar < 4; ++bar) {
                    if (ch(20, bar) >= (0.35f + 0.5f * act) * calm * lerp(0.6f, 1.4f, life) && bar != 1) continue;
                    const int at = bar * 16 + 2 * int(ch(21, bar) * 4.0f), d = high(22, bar);
                    for (int k = 0; k < 3; ++k) w.note(at + k, d + k, k == 2 ? 3 : 1, 0.6f + 0.1f * float(k));
                    if (ch(23, bar) < life + 0.3f)
                        for (int k = 0; k < 4; ++k) w.note(at + 8 + k, d + 2 + (k & 1), 1, 0.5f);
                }
                break;
            case DarkWoods:   // an owl: two low hoots, far apart
                for (int bar = 0; bar < 4; bar += 2) {
                    const int d = size + 4 * int(ch(24, bar) * 2.0f);
                    w.note(bar * 16 + 4, d, 4, 0.6f);
                    if (ch(25, bar) < 0.5f + 0.5f * act) w.note(bar * 16 + 10, d - 1, 6, 0.55f);
                }
                break;
            default:   // Morning Glade: chirps everywhere, two birds answering
                for (int step = 0; step < 64; step += 2) {
                    if (ch(26, step) >= (0.12f + 0.3f * act) * lerp(0.6f, 1.6f, life) * calm * lerp(0.7f, 1.3f, density)) continue;
                    const int d = high(27, step) + (ch(28, step) < 0.5f ? 0 : size);
                    const int count = 1 + int(ch(29, step) * 3.0f);
                    for (int k = 0; k < count && step + k < 64; ++k) w.note(step + k, d - k, 1, 0.55f * human(30, step + k));
                    step += count;
                }
                break;
        }
        if (out.count == 0) w.note(20, 2 * size, 2, 0.5f);   // never a silent forest
        return;
    }

    if (element == Wood) {
        // A hollow log (row 0), a stick (row 1) and a seed shaker (row 2). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        for (int step = 0; step < out.lengthSteps; ++step) {
            const int pos = step % 16;
            float log = 0, stick = 0, shake = 0;
            switch (in.character) {
                case Enchanted:
                    log = (pos == 0) ? 0.85f : (pos == 6 ? 0.4f + 0.4f * density : 0.0f);
                    stick = (pos == 10 || pos == 13) ? 0.3f * busy : 0.0f;
                    shake = (pos & 1) == 0 ? 0.35f * density * busy : 0.0f;
                    break;
                case DarkWoods:   // a slow knock, as if something is out there
                    log = (pos == 0) ? 0.7f : (pos == 3 ? 0.3f * act : 0.0f);
                    stick = pos == 11 ? 0.25f * density : 0.0f;
                    break;
                default:   // Morning Glade: dancing clicks
                    log = (pos == 0 || pos == 8) ? 0.9f : ((pos == 6 || pos == 14) ? 0.5f * density : 0.0f);
                    stick = (pos % 4 == 2) ? 0.7f * busy : ((pos & 1) != 0 ? 0.2f * density * busy : 0.0f);
                    shake = (0.4f + 0.5f * density) * busy;
                    break;
            }
            stick += tension * 0.35f * float(pos % 4 == 3);
            if (log > 0 && ch(40, step) < log) w.note(step, 0, 1, (pos == 0 ? 0.9f : 0.6f) * human(41, step));
            if (stick > 0 && ch(42, step) < stick) w.note(step, 1, 1, 0.6f * human(43, step));
            if (shake > 0 && ch(44, step) < shake) w.note(step, 2, 1, ((pos % 4 == 2) ? 0.7f : 0.45f) * human(45, step));
        }
        return;
    }

    if (element == Kalimba) {
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        switch (in.character) {
            case Enchanted: {   // a glittering arpeggio up and down the chord
                out.base = 12;
                static constexpr int line[8] { 0, 2, 4, 7, 9, 7, 4, 2 };
                for (int step = 0; step < 32; ++step) {
                    const bool eighth = (step & 1) == 0;
                    if (ch(50, step) >= (eighth ? 0.5f + 0.45f * busy : 0.4f * density * busy)) continue;
                    const int d = line[(step / 2) % 8];
                    w.note(step, d == 7 ? size : (d == 9 ? size + 2 : d), 2, (step % 8 == 0 ? 0.7f : 0.5f) * human(51, step));
                }
                break;
            }
            case DarkWoods:   // a few low notes, far apart
                out.base = 0;
                for (int step = 0; step < 32; step += 4)
                    if (ch(52, step) < 0.2f + 0.35f * act || step == 0) w.note(step, 2 * int(ch(53, step) * 3.0f), 4, 0.6f);
                break;
            default: {   // Morning Glade: a bright cycling ostinato in eighths (it falters with Tension)
                out.base = 12;
                static constexpr int line[6] { 0, 2, 4, 7, 4, 2 };
                for (int step = 0; step < 32; step += 2) {
                    if (ch(54, step) >= (step % 8 == 0 ? 0.97f : (0.5f + 0.45f * busy) * (1.0f - 0.6f * tension))) continue;
                    const int d = line[(step / 2) % 6];
                    w.note(step, d == 7 ? size : d, 2, (step % 8 == 0 ? 0.75f : 0.55f) * human(55, step));
                }
                break;
            }
        }
        return;
    }

    if (element == Strings) {
        // A string pad on the chord, held for the four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        switch (in.character) {
            case Enchanted:   // sweet: a seventh above the triad
                out.base = -5;
                for (int d : { 0, 2, 4, 6 }) w.note(0, d, 64, d == 0 ? 0.8f : 0.6f);
                break;
            case DarkWoods:   // low open fifths
                out.base = -12;
                w.note(0, 0, 64, 0.85f);
                w.note(0, 4, 64, 0.65f);
                if (tension > 0.6f) w.note(0, 0, 64, 0.5f, 1);
                break;
            default:
                out.base = 0;
                for (int d : { 0, 2, 4 }) w.note(0, d, 64, d == 0 ? 0.75f : 0.6f);
                break;
        }
        return;
    }

    if (element == Rustle) {
        // Rustling leaves: breath bands high on the root and the fifth (not the chords).
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        w.note(0, 3 * size, 64, 0.8f);
        w.note(0, 3 * size + music::nearestDegree(in.mood, 7), 64, 0.6f);
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void ForestPlace::configure(const PlaceContext& c) {
    const auto& w = *c.world;
    const auto& s = c.s;
    const int q = frame.quality, mood = w.mood;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool enchanted = s.character == Enchanted, dark = s.character == DarkWoods, glade = s.character == MorningGlade;

    // A breathy wooden flute.
    {
        auto& p = flute.patch;
        p.h2 = 0.12f + 0.08f * tone;
        p.h3 = 0.05f;
        p.breath = 0.32f + (dark ? 0.12f : 0.0f);
        p.attack = 0.07f;
        p.release = dark ? 0.5f : 0.25f;
        p.scoop = 0.5f;
        p.vibratoHz = 5.0f;
        p.vibratoDepth = 0.14f + 0.12f * s.life;
        p.vibratoDelay = 0.3f;
        p.spread = 0.3f;
        p.voices = 1;
        // A wooden flute: an airy tone, a soft chiff, the player's breath around it.
        p.chiff = 0.3f;
        p.chiffSeconds = 0.04f;
        p.air = 0.12f;
        p.wander = 3.0f;
        p.gain = 1.0f;
    }
    // Birds: nearly pure whistles that swoop into each note; trills, or an owl's hoot.
    {
        auto& p = birds.patch;
        p.h2 = dark ? 0.0f : 0.08f;
        p.h3 = 0.0f;
        p.breath = dark ? 0.2f : 0.05f;
        p.attack = dark ? 0.05f : 0.008f;
        p.release = dark ? 0.25f : 0.04f;
        p.scoop = dark ? 1.5f : 3.0f;
        p.scoopSeconds = dark ? 0.08f : 0.025f;
        p.vibratoHz = enchanted ? 12.0f : (glade ? 18.0f : 4.0f);
        p.vibratoDepth = dark ? 0.05f : 0.3f;
        p.vibratoDelay = 0.05f;
        p.minSeconds = 0.05f;
        p.spread = 0.8f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Wood: a hollow log on the fifth below, a stick high on the third, a seed shaker.
    {
        auto& p = wood.patch;
        auto& log = p.rows[0];
        log = {};
        log.semis = float(music::degree(mood, 4)) - 12.0f;
        log.bend = 1.03f;
        log.decay = dark ? 0.6f : 0.35f;
        log.tone = 0.6f;
        log.noise = 0.12f;
        log.noiseHz = lerpExp(500.0f, 1400.0f, tone);
        log.noiseQ = 1.5f;
        log.noiseDecay = 0.03f;
        log.ratio = 2.755f;
        log.partial = 0.35f;
        log.pan = -0.2f;
        auto& stick = p.rows[1];
        stick = {};
        stick.semis = 24.0f + float(music::degree(mood, 2));
        stick.bend = 1.0f;
        stick.decay = 0.07f;
        stick.tone = 0.35f;
        stick.noise = 0.2f;
        stick.noiseHz = lerpExp(2500.0f, 6000.0f, tone);
        stick.noiseQ = 1.2f;
        stick.noiseDecay = 0.02f;
        stick.ratio = 2.755f;
        stick.partial = 0.2f;
        stick.pan = 0.3f;
        auto& shaker = p.rows[2];
        shaker = {};
        shaker.tuned = false;
        shaker.hz = 100;
        shaker.tone = 0.0f;
        shaker.noise = 0.25f;
        shaker.noiseHz = lerpExp(4000.0f, 8000.0f, tone);
        shaker.noiseQ = 0.8f;
        shaker.noiseDecay = 0.12f;
        shaker.pan = 0.45f;
        p.voices = 5;
        p.gain = 1.0f;
    }
    // Kalimba: a plucked tine, its bright overtone a quarter-tone off any note.
    {
        auto& p = kalimba.patch;
        // A plucked steel tine: its cantilever modes near 6 and 17 times the note ring
        // briefly over the fundamental.
        p.set({ 1.0f, 5.95f, 17.0f }, { 1.0f, 0.18f, 0.07f }, { 1.0f, 0.3f * (0.4f + tone), 0.1f * tone });
        p.decay = dark ? 2.4f : (enchanted ? 2.6f : 1.1f);
        p.hardness = 0.5f + 0.35f * tone;
        p.toneHz = lerpExp(2500.0f, 10000.0f, tone);
        p.spread = 0.7f;
        p.voices = byQuality(q, 4, 5, 6);
        p.gain = 1.0f;
    }
    // The recorded kalimba, as played: three velocity layers, ringing on. The enchanted
    // wood lets it ring and glitter; the dark woods darken it and fold its low notes into
    // the tines it has; the morning glade keeps it short and bright.
    {
        auto& p = kalimbaRec.patch;
        p.instrument = "kalimba";
        p.decay = glade ? 1.8f : 0.0f;
        p.toneHz = dark ? lerpExp(1800.0f, 6000.0f, tone) : lerpExp(4000.0f, 16000.0f, tone);
        p.velocityTone = 1.0f;
        p.velocityGain = 0.4f;
        p.even = 0.6f;
        p.release = 0.3f;
        p.lowestKey = 53;
        p.highestKey = 95;
        p.spread = 0.7f;
        p.voices = byQuality(q, 4, 5, 6);
        p.gain = 4.2f;
    }
    // Strings: a soft section pad; in dark woods it trembles with Tension. With Scoop /
    // Glide on, chord changes glide and each stroke settles from a little flat.
    {
        auto& p = strings.patch;
        // A string section of violas and cellos, bowed over the fingerboard (soft, veiled).
        p.model = BowedString;
        p.size = dsp::Viola;
        p.players = 3;
        p.spreadCents = 10.0f;
        p.position = 0.2f;
        p.brightness = lerp(0.2f, 0.65f, tone) * (dark ? 0.6f : 1.0f) * std::exp2(0.3f * drift.smooth(t, 0.06));
        p.noisiness = 0.12f;
        p.attack = 1.6f;
        p.release = 2.4f;
        p.glide = 1.2f;
        p.vibratoDepth = 0.15f;
        p.tremoloPerBeat = 4.0f;
        p.tremoloDepth = 0.6f * std::max(0.0f, tension - 0.3f);
        p.spread = 0.6f;
        p.voices = byQuality(q, 3, 4, 4);
        p.held = true;
        p.gain = 1.0f;
    }
    // The recorded string pad: the developer's bowed violin, looped while the chord holds,
    // an octave above the old section (the recordings start at the G string), two players
    // a note (another take, a few cents apart and a moment late). It swells in slowly, a
    // gentle vibrato after it speaks; darker and slower in the dark woods, where Tension
    // makes it tremble; the recorded release as the bow lifts.
    {
        auto& p = stringsRec.patch;
        p.instrument = "violin-sustain";
        p.held = true;
        p.transpose = 12.0f;
        p.attack = dark ? 2.0f : 1.4f;
        p.release = 2.4f;
        p.recordedTail = false;   // a pad fades slowly on its loop (the bow lifting would cut it short)
        p.legatoSeconds = 0.25f;  // chords hand over slowly
        p.glide = 1.2f;
        p.vibratoHz = dark ? 4.6f : 5.2f;
        p.vibratoDepth = 0.1f + 0.05f * s.life;
        p.vibratoDelay = 0.5f;
        p.tremoloPerBeat = 4.0f;
        p.tremoloDepth = 0.6f * std::max(0.0f, tension - 0.3f);
        p.courseLoops = true;
        p.courseLevel = 0.8f;
        p.courseCents = 7.0f;
        p.courseMs = 35.0f;
        p.toneHz = lerpExp(2200.0f, 9000.0f, tone) * (dark ? 0.6f : 1.0f) * std::exp2(0.3f * drift.smooth(t, 0.06));
        p.velocityTone = 0.6f;
        p.velocityGain = 0.5f;
        p.even = 0.7f;
        p.lowestKey = 55;    // the G string
        p.highestKey = 83;   // up to a fifth above the highest recorded note
        p.spread = 0.6f;
        p.voices = byQuality(q, 3, 4, 4);
        // Where the section model sat in each Character (measured, Tests "balance").
        p.gain = enchanted ? 4.15f : (dark ? 2.56f : 3.32f);
    }
    // Rustling leaves: tuned breath alone, in gusts.
    const float gust = clamp01(0.55f + 0.6f * drift.smooth(t + 70.0, 0.25));
    {
        auto& p = rustle.patch;
        p.tone = 0.0f;
        p.breath = 1.0f;
        p.breathQ = lerp(4.0f, 8.0f, tone);
        p.attack = 1.5f;
        p.release = 1.5f;
        p.scoop = 0.0f;
        p.vibratoDepth = 0.0f;
        p.spread = 0.9f;
        p.voices = 2;
        p.held = true;
        p.gain = gust * gust;
    }
    // Tension: the strings swell and tremble while the birds and the kalimba hush.
    const float hush = 1.0f - 0.7f * tension;
    // An enchanted wood rings; the dark woods swallow sound (moss, dense trees).
    sendScale.fill(enchanted ? 1.35f : (dark ? 0.7f : 1.0f));
    level = { 0.1f, 0.13f * lerp(0.6f, 1.4f, s.life) * hush, 0.4f * (1.0f + 0.8f * s.machinery) * (glade ? 1.3f : 1.0f), 0.18f * hush,
              0.06f * (enchanted ? 1.4f : (glade ? 0.6f : 1.0f)) * (0.5f + 2.0f * tension), 1.2f * (glade ? 0.7f : 1.0f) };

    // Soft diffusion and a damped forest space; Tone opens the canopy.
    space.set(lerp(0.4f, 0.95f, s.size), lerp(1.2f, 3.5f, s.size) * (dark ? 1.3f : 1.0f) * (1.0f + 0.4f * s.magic), lerpExp(1200.0f, 4500.0f, tone));
    // The enchanted wood is deep and blurred, the morning glade open and close.
    spaceSend_ = (0.4f + 0.2f * s.fxDepth) * (enchanted ? 1.4f : (glade ? 0.55f : 1.0f));
    diffusion = (0.3f + 0.3f * s.fxDepth) * (glade ? 0.5f : 1.0f);
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era) * (1.0f - 0.45f * tension), rate);   // menace darkens the canopy
}

bool ForestPlace::shape(const PlaceContext&, const Scheduled& x, NoteOn& on) {
    // Each bird call comes from its own spot in the trees.
    if (x.element == Birds) {
        on.hasPan = true;
        on.pan = 0.85f * (2.0f * on.random - 1.0f);
    }
    return true;
}

void ForestPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        // Flute, birds, wood and kalimba blur softly into the trees.
        float dl = busL[Flute][k] + busL[Birds][k] + busL[Wood][k] + busL[Kalimba][k];
        float dr = busR[Flute][k] + busR[Birds][k] + busR[Wood][k] + busR[Kalimba][k];
        diffuser.process(dl, dr);
        l += diffusion * dl;
        r += diffusion * dr;
        float wl = 0, wr = 0;
        space.process(l * spaceSend_, r * spaceSend_, wl, wr);
        left[i] = tilt[2].lp(tilt[0].lp(l + 0.7f * wl));
        right[i] = tilt[3].lp(tilt[1].lp(r + 0.7f * wr));
    }
}

} // namespace wb
