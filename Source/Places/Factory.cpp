#include "Places.h"

namespace wb {

using namespace dsp;

FactoryPlace::FactoryPlace() : EnsemblePlace(1400) { setParts({ &anvil, &pistons, &steam, &metal, &gears, &bass }); }

void FactoryPlace::prepareFx() {
    comb.prepare(rate);
    hall.prepare(rate);
}

void FactoryPlace::resetFx() {
    comb.clear();
    hall.clear();
    for (auto& f : gritTone) f.reset();
}

void FactoryPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 1401);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size;
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == Anvil) {
        // Tuned strikes on the chord tones, an octave and more above the chord.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        out.base = 12;
        for (int bar = 0; bar < 4; ++bar) {
            const int at = bar * 16;
            switch (in.character) {
                case SteamMill:   // a ringing strike marking the bar, now and then an answer
                    if (ch(1, bar) < 0.55f + 0.4f * act) w.note(at, (bar & 1) == 0 ? 0 : 4, 8, 0.8f * human(2, at));
                    if (ch(3, bar) < act * 0.55f) w.note(at + 8, ch(4, bar) < 0.5f ? 2 : 4, 6, 0.5f * human(5, at));
                    if (ch(6, bar) < density * tension * 0.6f) w.note(at + 14, 0, 2, 0.35f);
                    break;
                case IronForge:   // the blacksmith: a heavy strike, then the hammer bouncing
                    for (int half = 0; half < 2; ++half) {
                        const int h = at + half * 8;
                        if (ch(7, h) < 0.75f + 0.25f * busy) w.note(h, half == 0 ? 0 : 4, 6, 1.0f * human(8, h));
                        if (ch(9, h) < 0.5f + 0.45f * density) w.note(h + 2, half == 0 ? 0 : 4, 1, 0.4f * human(10, h));
                        if (ch(11, h) < density * busy * 0.7f) w.note(h + 3, half == 0 ? 0 : 4, 1, 0.3f * human(12, h));
                        if (ch(13, h) < tension * 0.6f) w.note(h + 6, 2, 1, 0.35f);
                    }
                    break;
                default: {   // Assembly Line: a 3-3-4-2-4 stamping figure climbing the chord
                    static constexpr int steps[5] { 0, 3, 6, 10, 12 };
                    const int degrees[5] { 0, 2, 4, 2, size };
                    for (int k = 0; k < 5; ++k)
                        if (ch(14, at + k) < (k == 0 ? 0.97f : 0.45f + 0.5f * busy)) w.note(at + steps[k], degrees[k], 2, (k == 0 ? 0.85f : 0.6f) * human(15, at + k));
                    if (ch(16, bar) < density * tension) w.note(at + 14, 4, 1, 0.4f);
                    break;
                }
            }
        }
        return;
    }

    if (element == Pistons) {
        // A piston's thump (row 0) and its valve's chuff (row 1). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        for (int step = 0; step < out.lengthSteps; ++step) {
            const int pos = step % 16;
            float thump = 0, chuff = 0;
            switch (in.character) {
                case SteamMill:   // the water-wheel chug: thump on 1 and 3, chuff between
                    thump = pos % 8 == 0 ? 0.95f : 0.0f;
                    chuff = pos % 8 == 4 ? 0.85f : (pos % 8 == 6 ? density * 0.6f * busy : 0.0f);
                    break;
                case IronForge:   // the bellows: a slow, heavy three against four
                    thump = (pos == 0 || pos == 6) ? 0.9f : (pos == 12 ? 0.4f + 0.5f * density : 0.0f);
                    chuff = pos == 8 ? 0.85f : (pos == 14 ? density * 0.4f : 0.0f);
                    break;
                default:   // Assembly Line: a pump on every beat, valves on the off-beats
                    thump = pos % 4 == 0 ? 0.97f : ((pos & 1) != 0 ? density * busy * 0.35f : 0.0f);
                    chuff = pos % 4 == 2 ? 0.9f : 0.0f;
                    break;
            }
            chuff += tension * 0.3f * float(pos % 4 == 3);
            if (thump > 0 && ch(20, step) < thump) w.note(step, 0, 1, (pos % 8 == 0 ? 1.0f : 0.6f) * human(21, step));
            if (chuff > 0 && ch(22, step) < chuff) w.note(step, 1, 1, 0.7f * human(23, step));
        }
        return;
    }

    if (element == Steam) {
        // Long swells (row 0) and short hisses (row 1); a note's length is its sweep's.
        out.setGrid(pat::Sixteenth, 8);
        out.followChords = false;
        switch (in.character) {
            case SteamMill:
                for (int bar = 0; bar < 8; bar += 4)
                    if (bar == 0 || ch(30, bar) < 0.5f + 0.4f * act) w.note(bar * 16, 0, 24, 0.8f);
                for (int bar = 1; bar < 8; bar += 2)
                    if (ch(31, bar) < 0.3f + 0.5f * act) w.note(bar * 16 + 12, 1, 4, 0.6f);
                break;
            case IronForge:   // the bellows breathing in and out
                w.note(0, 0, 48, 0.9f);
                if (ch(32, 0) < 0.4f + 0.5f * act) w.note(64, 0, 40, 0.75f);
                for (int bar = 2; bar < 8; bar += 3)
                    if (ch(33, bar) < 0.4f + 0.5f * density) w.note(bar * 16 + 2, 1, 3, 0.55f);
                break;
            default:   // a pneumatic hiss at the end of every bar
                for (int bar = 0; bar < 8; ++bar)
                    if (ch(34, bar) < 0.6f + 0.35f * busy) w.note(bar * 16 + 12, 1, 3, 0.6f + 0.2f * float(bar % 2));
                if (ch(35, 0) < 0.3f + 0.5f * act) w.note(32, 0, 32, 0.6f);
                break;
        }
        return;
    }

    if (element == BowedMetal) {
        // Bowed sheet metal on the chord, held for the four bars; Tension grinds a
        // semitone against the root.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = true;
        switch (in.character) {
            case SteamMill:
                out.base = -5;
                w.note(0, 0, 64, 0.8f);
                w.note(0, 4, 64, 0.55f);
                break;
            case IronForge:
                out.base = -12;
                w.note(0, 0, 64, 0.9f);
                w.note(0, size, 64, 0.5f);
                break;
            default:
                out.base = 0;
                w.note(0, 0, 32, 0.7f);
                w.note(32, 2, 32, 0.6f);
                w.note(0, 4, 64, 0.5f);
                break;
        }
        if (tension > 0.6f) w.note(0, 0, 64, 0.5f, 1);
        return;
    }

    if (element == Gears) {
        // Gear clicks (row 0) and a ratchet (row 1). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        for (int step = 0; step < out.lengthSteps; ++step) {
            const int pos = step % 16;
            float click = 0, ratchet = 0;
            switch (in.character) {
                case SteamMill:
                    click = (pos & 1) == 0 ? (0.45f + 0.45f * density) * busy : 0.0f;
                    ratchet = step == 30 ? 0.6f : 0.0f;
                    break;
                case IronForge:   // a chain winding slowly
                    click = pos % 4 == 0 ? 0.6f : 0.0f;
                    ratchet = pos == 0 ? 0.35f + 0.5f * tension : 0.0f;
                    break;
                default:   // every sixteenth ticks, the ratchet catches twice a bar
                    click = 0.85f * std::min(busy, 1.0f) * (pos % 4 == 0 ? 1.15f : 0.8f + 0.2f * density);
                    ratchet = (pos == 7 || pos == 15) ? 0.7f : 0.0f;
                    break;
            }
            if (click > 0 && ch(40, step) < click) w.note(step, 0, 1, (pos % 4 == 0 ? 0.8f : 0.45f) * human(41, step));
            if (ratchet > 0 && ch(42, step) < ratchet) w.note(step, 1, 1, 0.7f);
        }
        return;
    }

    if (element == Bass) {
        // The furnace bass, two octaves down on the chord. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = -24;
        switch (in.character) {
            case SteamMill:   // root and fifth, a passing note into the next bar
                for (int bar = 0; bar < 2; ++bar) {
                    const int at = bar * 16;
                    w.note(at, 0, 6, 0.9f);
                    if (ch(50, bar) < 0.5f + 0.45f * act) w.note(at + 8, 4, 6, 0.75f);
                    if (ch(51, bar) < density * act) w.note(at + 14, ch(52, bar) < 0.5f ? 5 : 1, 2, 0.6f);
                }
                break;
            case IronForge:   // slow and heavy, an octave pickup
                w.note(0, 0, 12, 1.0f);
                if (ch(53, 0) < 0.4f + 0.5f * act) w.note(12, size, 4, 0.6f);
                w.note(16, 0, 8, 0.9f);
                if (ch(54, 0) < 0.3f + 0.6f * density) w.note(24, 4, 8, 0.75f);
                break;
            default: {   // eighth notes driving under the line
                static constexpr int line[8] { 0, 0, 7, 0, 4, 0, 7, 4 };
                for (int step = 0; step < out.lengthSteps; step += 2) {
                    const bool strong = step % 4 == 0;
                    if (ch(55, step) >= (strong ? 0.97f : (0.55f + 0.4f * density) * busy)) continue;
                    const int d = line[(step / 2) % 8];
                    w.note(step, d == 7 ? size : d, 2, (strong ? 0.9f : 0.65f) * human(56, step));
                }
                break;
            }
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void FactoryPlace::configure(const PlaceContext& c) {
    const auto& w = *c.world;
    const auto& s = c.s;
    const int q = frame.quality, mood = w.mood;
    const float tension = s.tension, tone = s.tone;
    const double t = double(c.clock) / rate;
    const bool mill = s.character == SteamMill, forge = s.character == IronForge, line = s.character == AssemblyLine;

    // Anvil: a struck bar of iron, its upper partials a quarter-tone off the notes (heard
    // as metal, never as other notes).
    {
        auto& p = anvil.patch;
        p.set({ 1.0f, 2.755f, 4.899f, 6.92f, 9.24f }, { 1.0f, 0.7f, 0.5f, 0.35f, 0.2f }, { 1.0f, 0.6f, 0.6f * tone, 0.4f * tone, 0.3f * tone * tone });
        p.decay = (forge ? 3.0f : (mill ? 2.4f : 1.0f)) * lerp(1.2f, 0.7f, tension);
        p.hardness = 0.7f + 0.3f * tone;
        p.toneHz = lerpExp(2500.0f, 10000.0f, tone);
        p.pan = forge ? -0.15f : 0.1f;
        p.spread = 0.3f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.0f;
    }
    // Pistons: a low tuned thump with a chuff of air, and the valve's tuned hiss.
    {
        auto& p = pistons.patch;
        auto& thump = p.rows[0];
        thump = {};
        thump.semis = -12;
        thump.bend = forge ? 1.15f : 1.35f;
        thump.decay = forge ? 0.6f : 0.35f;
        thump.tone = 0.8f;
        thump.noise = 0.22f + 0.15f * s.machinery;
        thump.noiseHz = 420;
        thump.noiseQ = 0.8f;
        thump.noiseDecay = 0.12f;
        thump.ratio = 1.5f;
        thump.partial = 0.3f;
        thump.pan = -0.1f;
        auto& valve = p.rows[1];
        valve = {};
        valve.tuned = false;
        valve.hz = 400;
        valve.tone = 0.0f;
        valve.noise = 0.45f;
        valve.noiseHz = music::noteHz(w.root, 24.0f + float(music::degree(mood, 4)) + 14.0f * tone);
        valve.noiseQ = 2.5f;
        valve.noiseDecay = mill ? 0.3f : (forge ? 0.45f : 0.16f);
        valve.pan = 0.3f;
        p.voices = 5;
        p.gain = 1.0f;
    }
    // Steam: a whistling swell around the fifth, and a short falling hiss.
    steam.patch.rows[0] = { SweepSwell, 7.0f + 12.0f * tone, 31.0f + 12.0f * tone, lerp(4.0f, 8.0f, tone), 1.0f, false, true };
    steam.patch.rows[1] = { SweepFall, 30.0f + 12.0f * tone, 54.0f + 12.0f * tone, 2.5f, 0.6f, false, false };
    steam.patch.tension = tension;
    steam.patch.voices = 2;
    steam.patch.gain = 1.0f;
    // Bowed metal: a sheet of iron under a bow, beating against its own octave.
    {
        auto& p = metal.patch;
        p.set({ 1.0f, 2.006f, 2.755f, 4.02f }, { 1.0f, 0.8f, 0.6f, 0.5f }, { 1.0f, 0.55f, 0.6f * tone, 0.5f * tone });
        p.ring = lerp(0.5f, 0.2f, tension) * (forge ? 1.4f : 1.0f);
        p.rosin = 0.35f + 0.25f * tone;
        p.noisiness = 0.6f + 0.3f * tension;
        p.attack = 1.6f;
        p.release = 2.2f;
        p.glide = 0.8f;
        p.vibratoHz = 0.35f;
        p.vibratoDepth = 0.06f + 0.1f * tension;
        p.spread = 0.6f;
        p.voices = byQuality(q, 2, 3, 4);
        p.held = true;
        p.gain = 1.0f + 0.8f * tension;
    }
    // Gears: a tuned tick high on the fifth, and the ratchet's rattle.
    {
        auto& p = gears.patch;
        auto& click = p.rows[0];
        click = {};
        click.semis = 24.0f + float(music::degree(mood, 4));
        click.bend = 1.0f;
        click.decay = line ? 0.05f : 0.08f;
        click.tone = 0.35f;
        click.noise = 0.3f;
        click.noiseHz = lerpExp(2500.0f, 9000.0f, tone);
        click.noiseQ = 1.5f;
        click.noiseDecay = 0.03f;
        click.ratio = 2.0f;
        click.partial = 0.3f;
        click.pan = 0.35f;
        auto& ratchet = p.rows[1];
        ratchet = {};
        ratchet.tuned = false;
        ratchet.hz = 300;
        ratchet.tone = 0.0f;
        ratchet.noise = 0.55f;
        ratchet.noiseHz = music::noteHz(w.root, 24.0f + 14.0f * tone);
        ratchet.noiseQ = 4.0f;
        ratchet.noiseDecay = 0.14f;
        ratchet.pan = -0.3f;
        p.voices = 4;
        p.gain = 0.8f + 0.5f * s.machinery;
    }
    // The furnace bass: saws and a sub through a low-pass that bites on each note.
    {
        auto& p = bass.patch;
        p.held = true;
        p.legato = false;
        p.attack = 0.005f;
        p.decay = 0.3f;
        p.sustain = 0.6f;
        p.release = 0.15f;
        p.detune = 6.0f;
        p.sub = 0.6f;
        p.cutoff = lerpExp(70.0f, 320.0f, tone) * (forge ? 0.7f : 1.0f) * std::exp2(0.3f * drift.smooth(t, 0.1));
        p.envOctaves = 0.4f;
        p.filterEnvOctaves = line ? 1.8f : 1.1f;
        p.filterDecay = 0.08f;
        p.q = 1.2f + tension;
        p.width = 0.2f;
        p.spread = 0.05f;
        p.voices = 2;
        p.gain = 1.0f;
    }
    // Life: more hands at work, the anvil and the gears ring out more.
    level = { 0.17f * lerp(0.75f, 1.3f, s.life), 0.28f, 0.42f, 0.15f, 1.1f * lerp(0.85f, 1.15f, s.life), 0.11f };

    // Combs tuned to the chord root, three and four octaves up, make the metal ring with
    // the harmony (high enough that their fifth harmonic, the root's major third, is
    // damped away); gritty saturation that deepens with Activity, Population and
    // Tension; an iron hall.
    const int chordRoot = music::inRange(music::chordTone(mood, c.chord, 0), 0);
    comb.set(music::noteHz(w.root, float(chordRoot + 24)), music::noteHz(w.root, float(chordRoot + 36)) * 1.003f,
             lerp(0.55f, 0.85f, 0.5f * s.machinery + 0.5f * tension), lerpExp(1500.0f, 4000.0f, tone));
    combSend = 0.2f + 0.3f * s.fxDepth;
    combWet = 0.5f;
    drive = 1.0f + 0.8f * s.fxDepth + 0.6f * tension + (forge ? 0.5f : 0.0f);
    for (auto& f : gritTone) f.setCutoff(lerpExp(800.0f, 16000.0f, tone), rate);
    hall.set(lerp(0.3f, 0.9f, s.size), lerp(0.9f, 3.2f, s.size) * (1.0f + 0.3f * s.magic), lerpExp(2000.0f, 6000.0f, tone));
    hallSend = 0.3f + 0.15f * s.size;
}

bool FactoryPlace::shape(const PlaceContext& c, const Scheduled& x, NoteOn& on) {
    if (x.element == Anvil) {
        // The hammer's bounces ring shorter than its strikes, and swing side to side.
        if (on.velocity < 0.5f) on.decay = 0.4f;
        on.hasPan = true;
        on.pan = (c.s.character == IronForge ? -0.2f : 0.1f) + 0.3f * (on.random - 0.5f);
    }
    return true;
}

void FactoryPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        // The metal (anvil, pistons, steam, gears) rings in the comb.
        const float metalIn = 0.5f * (busL[Anvil][k] + busR[Anvil][k] + busL[Pistons][k] + busR[Pistons][k] + busL[Steam][k] + busR[Steam][k]
                                      + busL[Gears][k] + busR[Gears][k]);
        float cl = 0, cr = 0, wl = 0, wr = 0;
        comb.process(metalIn * combSend, cl, cr);
        l += cl * combWet;
        r += cr * combWet;
        hall.process(l * hallSend, r * hallSend, wl, wr);
        l += 0.6f * wl;
        r += 0.6f * wr;
        // Grit: soft saturation, levelled, then a gentle high cut.
        left[i] = gritTone[2].lp(gritTone[0].lp(softClip(l * drive) / std::sqrt(drive))) * 1.1f;
        right[i] = gritTone[3].lp(gritTone[1].lp(softClip(r * drive) / std::sqrt(drive))) * 1.1f;
    }
}

} // namespace wb
