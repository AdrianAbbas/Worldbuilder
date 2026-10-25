#include "Places.h"

namespace wb {

using namespace dsp;

TemplePlace::TemplePlace() : EnsemblePlace(1800) { setParts({ &choir, &gongs, &bowls, &chant, &bells, &zither }); }

void TemplePlace::prepareFx() {
    shimmer.prepare(rate);
    hall.prepare(rate);
}

void TemplePlace::resetFx() {
    shimmer.clear();
    hall.clear();
    for (auto& f : tilt) f.reset();
}

void TemplePlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 1801);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Choir) {
        // A wordless choir on the chord, held for the four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        switch (in.character) {
            case SacredDawn:   // open and bright: root, fifth, the third above the octave
                out.base = 0;
                w.note(0, 0, 64, 0.7f);
                w.note(0, 4, 64, 0.6f);
                w.note(0, size + 2, 64, 0.5f);
                break;
            case AncientRite:
                out.base = -5;
                w.note(0, 0, 64, 0.75f);
                w.note(0, 2, 64, 0.65f);
                w.note(0, 4, 64, 0.65f);
                break;
            default:   // Forsaken: two thin swells, a semitone grinding with Tension
                out.base = -5;
                for (int at : { 0, 32 }) {
                    w.note(at, 0, 24, 0.65f);
                    if (tension > 0.6f) w.note(at, 0, 24, 0.5f, 1);
                    else w.note(at, 4, 24, 0.5f);
                }
                break;
        }
        return;
    }

    if (element == Gongs) {
        // Gongs an octave below the key's root (and its fifth). Eight bars.
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = false;
        switch (in.character) {
            case SacredDawn:
                w.note(0, -size, 32, 0.6f);
                if (ch(1, 0) < 0.3f + 0.5f * act) w.note(64, fifth - size, 32, 0.45f);
                break;
            case AncientRite:   // a heavy gong every other bar, root and fifth in turn
                for (int bar = 0; bar < 8; bar += 2) {
                    if (bar > 0 && ch(2, bar) >= 0.55f + 0.45f * busy) continue;
                    w.note(bar * 16, (bar % 4 == 0) ? -size : fifth - size, 24, (bar % 4 == 0 ? 0.9f : 0.7f) * human(3, bar));
                }
                if (ch(4, 0) < density * act) w.note(120, -size, 8, 0.4f);
                break;
            default:   // Forsaken: a lone gong; Tension brings another
                w.note(16 * int(ch(5, 0) * 4.0f), -size, 32, 0.7f);
                if (ch(6, 0) < 0.2f + 0.6f * tension) w.note(96, fifth - size, 32, 0.55f);
                break;
        }
        return;
    }

    if (element == Bowls) {
        // Singing bowls in the key, struck and left to sing. Eight bars.
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = false;
        const auto bowl = [&](uint32_t salt, int64_t at) { return 2 * int(ch(salt, at) * 3.0f) + (ch(salt + 1, at) < 0.3f ? size : 0); };
        switch (in.character) {
            case SacredDawn:   // a bowl every bar or two, each on its own note
                for (int bar = 0; bar < 8; ++bar)
                    if (ch(10, bar) < 0.35f + 0.5f * act || bar == 0) w.note(bar * 16 + 4 * int(ch(11, bar) * 2.0f), bowl(12, bar), 16, 0.6f * human(14, bar));
                break;
            case AncientRite:   // a bowl answering each gong
                for (int bar = 0; bar < 8; bar += 2)
                    if (ch(15, bar) < 0.5f + 0.45f * act || bar == 0) w.note(bar * 16 + 8, bowl(16, bar), 16, 0.55f);
                break;
            default:
                for (int bar = 0; bar < 8; bar += 4)
                    if (ch(18, bar) < 0.4f + 0.5f * act || bar == 0) w.note(bar * 16 + 12, bowl(19, bar), 24, 0.5f);
                break;
        }
        return;
    }

    if (element == Chant) {
        // The chant drone: the key's root two octaves down, and its fifth.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        w.note(0, -2 * size, 64, 0.9f);
        w.note(0, fifth - 2 * size, 64, in.character == Forsaken ? 0.4f : 0.65f);
        return;
    }

    if (element == Bells) {
        // Temple bells high in the key. Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        switch (in.character) {
            case SacredDawn:   // the morning bell: a few strikes, the last left ringing
                for (int bar = 0; bar < 4; bar += 2)
                    if (ch(30, bar) < 0.45f + 0.5f * act || bar == 0) {
                        const int count = 2 + int(ch(31, bar) * 2.0f);
                        for (int k = 0; k < count; ++k) w.note(bar * 16 + 4 * k, 2 * size, 4, (k == count - 1 ? 0.75f : 0.55f));
                    }
                break;
            case AncientRite:   // small ritual bells: eighths in the last bar
                for (int step = 48; step < 64; step += 2)
                    if (ch(32, step) < (0.5f + 0.45f * density) * busy) w.note(step, 2 * size + 2 * int(ch(33, step) * 3.0f), 2, (step % 4 == 0 ? 0.6f : 0.45f) * human(34, step));
                if (ch(35, 0) < act) w.note(16, 2 * size, 4, 0.5f);
                break;
            default:   // Forsaken: broken, scattered strikes
                for (int step = 0; step < 64; step += 4)
                    if (ch(36, step) < (0.06f + 0.14f * act) * (1.0f + tension)) w.note(step, size + int(ch(37, step) * float(size + 3)), 2, 0.4f + 0.2f * ch(38, step));
                if (out.count == 0) w.note(40, 2 * size, 2, 0.45f);
                break;
        }
        return;
    }

    if (element == Zither) {
        // Zither plucks on the chord. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        switch (in.character) {
            case SacredDawn:   // rippling runs up the scale
                out.base = 12;
                for (int bar = 0; bar < 2; ++bar) {
                    if (ch(40, bar) >= 0.2f + 0.8f * act && bar != 0) continue;
                    const int at = bar * 16 + 2 * int(ch(41, bar) * 3.0f), notes = 3 + int(5.0f * act + ch(42, bar) * 3.0f * density);
                    for (int k = 0; k < notes && at + k < 32; ++k) w.note(at + k, k, 2, (0.45f + 0.04f * float(k)) * human(43, at + k));
                }
                break;
            case AncientRite: {   // a processional ostinato in eighths
                out.base = 0;
                static constexpr int line[8] { 0, 4, 7, 4, 0, 4, 2, 4 };
                for (int step = 0; step < 32; step += 2) {
                    if (ch(44, step) >= (step % 8 == 0 ? 0.97f : (0.5f + 0.45f * density) * busy)) continue;
                    const int d = line[(step / 2) % 8];
                    w.note(step, d == 7 ? size : d, 2, (step % 8 == 0 ? 0.75f : 0.55f) * human(45, step));
                }
                break;
            }
            default:   // Forsaken: a few low plucks, far apart
                out.base = 0;
                for (int step = 0; step < 32; step += 4)
                    if (ch(46, step) < 0.15f + 0.35f * act || step == 0) w.note(step, 2 * int(ch(47, step) * 3.0f), 4, 0.55f);
                break;
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void TemplePlace::configure(const PlaceContext& c) {
    const auto& w = *c.world;
    const auto& s = c.s;
    const int q = frame.quality, mood = w.mood;
    const float tension = s.tension, tone = s.tone;
    const bool dawn = s.character == SacredDawn, rite = s.character == AncientRite, forsaken = s.character == Forsaken;

    // The choir: "ah" at dawn, "oh" at the rite, a hollow "oo" when forsaken.
    {
        auto& p = choir.patch;
        p.vowel = (dawn ? 2.6f : (rite ? 1.1f : 0.3f)) + 0.6f * tone - 0.3f - 0.6f * std::max(0.0f, tension - 0.4f);
        p.vowelDrift = 0.4f;
        p.attack = 2.5f;
        p.release = 3.0f;
        p.detune = 10.0f;
        p.vibratoDepth = 0.12f;
        p.brightness = tone;
        p.glide = 1.0f;
        p.spread = 0.8f;
        p.voices = byQuality(q, 2, 3, 4);
        p.gain = 1.0f;
    }
    // Gongs: a soft mallet on a big plate; its partials beat and spread, a quarter-tone
    // off any note above the fifth.
    {
        auto& p = gongs.patch;
        p.set({ 1.0f, 1.5f, 2.0f, 2.755f, 3.0f, 4.899f }, { 1.0f, 0.8f, 0.7f, 0.6f, 0.5f, 0.35f },
              { 1.0f, 0.45f, 0.5f, 0.45f * (0.4f + tone), 0.3f, 0.25f * tone });
        p.decay = (rite ? 7.0f : 9.0f) * (1.0f + 0.3f * s.size);
        p.hardness = 0.12f + 0.2f * tone;
        p.toneHz = lerpExp(2000.0f, 7000.0f, tone);
        p.spread = 0.3f;
        p.voices = 3;
        p.gain = 1.0f;
        // A large gong: the pitch sags as the strike's energy spreads, and the shimmer of
        // the high modes blooms after the strike (stronger at the rite).
        p.extras.glideSemis = rite ? 0.2f : -0.2f;
        p.extras.glideSeconds = 0.7f;
        p.extras.bloom = rite ? 0.8f : 0.5f;
    }
    // Singing bowls: close pairs of partials beating slowly, ringing long.
    {
        auto& p = bowls.patch;
        p.set({ 1.0f, 1.0035f, 2.74f, 2.752f, 5.2f, 5.22f }, { 1.0f, 1.0f, 0.6f, 0.6f, 0.35f, 0.35f },
              { 1.0f, 0.8f, 0.5f, 0.4f, 0.2f * tone, 0.15f * tone });
        p.decay = forsaken ? 10.0f : 8.0f;
        p.hardness = 0.2f + 0.2f * tone;
        p.toneHz = lerpExp(3000.0f, 9000.0f, tone);
        p.spread = 0.8f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.0f;
    }
    // The chant: low voices on "oo", "oh" and "ah", the vowel moving slowly like a mantra.
    {
        auto& p = chant.patch;
        p.vowel = rite ? 0.9f : 0.5f;
        p.vowelDrift = 0.8f;
        p.babble = rite ? 0.5f : 0.3f;
        p.chatter = 0.0f;
        p.attack = 3.0f;
        p.release = 3.0f;
        p.detune = 6.0f;
        p.vibratoDepth = 0.06f;
        p.brightness = 0.2f + 0.3f * tone;
        p.glide = 1.0f;
        p.spread = 0.3f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Temple bells: bright bells whose third and fifth partials are the scale's own.
    {
        auto& p = bells.patch;
        const float third = std::exp2(float(music::degree(mood, 2)) / 12.0f), fifth = std::exp2(float(music::degree(mood, 4)) / 12.0f);
        p.set({ 0.5f, 1.0f, third, fifth, 2.0f, 2.0f * fifth }, { 1.3f, 1.0f, 0.7f, 0.55f, 0.45f, 0.3f },
              { 0.3f, 1.0f, 0.45f, 0.35f, 0.5f * (0.5f + tone), 0.25f * tone });
        p.decay = forsaken ? 2.5f : 4.0f;
        p.hardness = 0.6f + 0.35f * tone;
        p.toneHz = lerpExp(3000.0f, 10000.0f, tone);
        p.spread = 0.7f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.0f;
        // A temple bell's slow beating swell (its "oneri").
        p.extras.beatHz = 1.6f;
    }
    // Zither: bright plucked strings.
    {
        auto& p = zither.patch;
        p.decay = (forsaken ? 3.0f : 1.8f) * lerp(1.0f, 0.6f, tension);
        p.brightness = 0.45f + 0.45f * tone;
        p.spread = 0.6f;
        p.voices = byQuality(q, 4, 6, 8);
        // Silk-and-metal strings plucked near the bridge, a shallow box, the open strings ringing.
        p.body = dsp::BodyZither;
        p.shape = { 0.12f, 0.25f, 0.2f, 0.0f, 0.0f, 0.35f, 0.0f };
        p.sympathetic = 0.2f;
        p.gain = 1.0f;
    }
    // Tension: the chant and the gongs come forward; Machinery quickens the ritual.
    level = { 0.3f, 0.2f * (0.7f + 0.8f * tension), 0.18f, 0.5f * (0.6f + 1.0f * tension), 0.14f * (1.0f + 0.5f * s.machinery),
              0.6f * (1.0f + 0.5f * s.machinery) };

    // A long shimmer reverb that rises an octave as it fades, and a stone hall.
    shimmer.set(1.0f, lerp(6.0f, 9.0f, s.size) * (1.0f + 0.3f * s.magic), 0.35f + 0.4f * s.fxDepth + (dawn ? 0.1f : 0.0f));
    shimmerSend = 0.25f + 0.25f * s.fxDepth;
    hall.set(lerp(0.5f, 1.0f, s.size), lerp(2.5f, 5.0f, s.size), lerpExp(2000.0f, 6000.0f, tone));
    hallSend = 0.3f;
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era), rate);
}

bool TemplePlace::shape(const PlaceContext&, const Scheduled& x, NoteOn& on) {
    // Bowls and bells ring from their own places around the hall.
    if (x.element == Bowls || x.element == Bells) {
        on.hasPan = true;
        on.pan = 0.75f * (2.0f * on.random - 1.0f);
    }
    return true;
}

void TemplePlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        float hl = 0, hr = 0, sl = 0, sr = 0;
        hall.process(l * hallSend, r * hallSend, hl, hr);
        // Everything but the chant rises into the shimmer.
        shimmer.process((l - busL[Chant][k]) * shimmerSend, (r - busR[Chant][k]) * shimmerSend, sl, sr);
        left[i] = tilt[2].lp(tilt[0].lp(l + 0.5f * hl + 0.75f * sl));
        right[i] = tilt[3].lp(tilt[1].lp(r + 0.5f * hr + 0.75f * sr));
    }
}

} // namespace wb
