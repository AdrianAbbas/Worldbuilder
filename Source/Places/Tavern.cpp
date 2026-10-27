#include "Places.h"

namespace wb {

using namespace dsp;

TavernPlace::TavernPlace() : EnsemblePlace(3000) { setParts({ &luteRec, &drum, &fiddleRec, &glasses, &squeezebox, &stomps }); }

void TavernPlace::prepareFx() {
    room.prepare(rate);
    warmth.prepare(rate);
}

void TavernPlace::resetFx() {
    room.clear();
    warmth.clear();
    for (auto& f : tilt) f.reset();
    for (auto& f : shelf) f.reset();
}

void TavernPlace::generate(int element, const GenInputs& in, pat::Pattern& out) const {
    out.clear();
    Chance chance;
    chance.stream.setKey(in.key ^ (uint32_t(in.variant) * 0x9E3779B9u), 3001);
    const auto ch = [&](uint32_t salt, int64_t pos) { return chance.at(pos, salt); };
    const float act = in.activity, density = in.density, tension = in.tension, life = in.life;
    const float busy = lerp(0.35f, 1.15f, act);
    const int size = music::scaleFor(in.mood).size, fifth = music::nearestDegree(in.mood, 7);
    const auto human = [&](uint32_t salt, int64_t pos) { return 1.0f + (ch(salt, pos) - 0.5f) * (0.15f + 0.4f * life); };
    PatternWriter w { out };

    if (element == LuteStrum) {
        // A lute strumming the chord. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = -5;
        const auto strum = [&](int at, float velocity, bool full) {
            w.note(at, 0, 2, velocity);
            w.note(at, 2, 2, velocity * 0.85f);
            w.note(at, 4, 2, velocity * 0.85f);
            if (full) w.note(at, size, 2, velocity * 0.7f);
        };
        switch (in.character) {
            case LivelyNight:   // a reel: down on the beat, up on the off-beat
                for (int step = 0; step < 32; step += 2) {
                    if (step % 8 != 0 && ch(1, step) >= (step % 4 == 0 ? 0.3f + 0.7f * busy : (0.4f + 0.5f * density) * busy)) continue;
                    strum(step, (step % 4 == 0 ? 0.65f : 0.45f) * human(2, step), step % 8 == 0);
                }
                break;
            case QuietCorner: {   // slow arpeggios by the fire
                static constexpr int arp[8] { 0, 2, 4, 7, 9, 7, 4, 2 };
                for (int step = 0; step < 32; step += 2) {
                    if (step % 8 != 0 && ch(3, step) >= (0.35f + 0.5f * density) * busy) continue;
                    const int d = arp[(step / 2) % 8];
                    w.note(step, d >= 7 ? d - 7 + size : d, 4, (step % 8 == 0 ? 0.6f : 0.45f) * human(4, step));
                }
                break;
            }
            default:   // Brawl: hammered sixteenths, too fast
                for (int step = 0; step < 32; ++step) {
                    if (step % 2 == 1 && ch(5, step) >= (0.4f + 0.5f * density) * busy) continue;
                    strum(step, (step % 4 == 0 ? 0.7f : 0.5f) * human(6, step), false);
                }
                break;
        }
        return;
    }

    if (element == FrameDrum) {
        // A frame drum (row 0) and its rim (row 1). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        switch (in.character) {
            case LivelyNight:
                for (int beat = 0; beat < 8; ++beat) {
                    if (beat % 2 == 0 || ch(18, beat) < 0.3f + 0.7f * busy) w.note(beat * 4, 0, 1, (beat % 2 == 0 ? 0.75f : 0.55f) * human(10, beat));
                    if (ch(11, beat) < (0.3f + 0.5f * density) * busy) w.note(beat * 4 + 2, 1, 1, 0.45f * human(12, beat));
                    if (ch(13, beat) < 0.25f * busy) w.note(beat * 4 + 3, 1, 1, 0.35f);
                }
                break;
            case QuietCorner:   // a soft heartbeat
                w.note(0, 0, 1, 0.5f);
                if (ch(14, 0) < 0.3f + 0.6f * act) w.note(16, 0, 1, 0.45f);
                if (ch(15, 0) < 0.3f * busy) w.note(24, 1, 1, 0.3f);
                break;
            default:   // Brawl: pounding and lurching
                for (int step = 0; step < 32; ++step)
                    if (step % 4 == 0 || ch(16, step) < (0.25f + 0.4f * density) * busy) w.note(step, step % 4 == 0 ? 0 : 1, 1, (step % 4 == 0 ? 0.75f : 0.5f) * human(17, step));
                break;
        }
        if (tension > 0.5f)
            for (int step = 28; step < 32; ++step) w.note(step, 0, 1, 0.3f + 0.4f * tension);
        return;
    }

    if (element == Fiddle) {
        // The fiddle: a tune over its open string (the root, droned underneath). Four bars.
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        switch (in.character) {
            case LivelyNight: {   // a reel: running eighths round the chord tones
                static constexpr int reel[16] { 4, 2, 0, 2, 4, 5, 4, 2, 0, 1, 2, 4, 7, 4, 2, 1 };
                w.note(0, -size, 32, 0.45f);
                w.note(32, -size, 32, 0.45f);
                for (int k = 0; k < 32; ++k) {
                    const int at = k * 2;
                    if (k % 8 != 0 && ch(20, k) >= (k % 4 == 0 ? 0.3f + 0.7f * busy : (0.45f + 0.5f * density) * busy)) continue;
                    const int d = reel[(k + (k / 16) * 5) % 16];
                    w.note(at, d >= 7 ? d - 7 + size : d, 2, (k % 2 == 0 ? 0.65f : 0.5f) * human(21, k));
                }
                break;
            }
            case QuietCorner: {   // a slow air
                static constexpr int air[8] { 0, 2, 4, 3, 2, 1, 2, 0 };
                w.note(0, -size, 64, 0.35f);
                for (int k = 0; k < 8; ++k) {
                    if (k % 2 == 1 && ch(22, k) >= 0.3f + 0.6f * busy) continue;
                    w.note(k * 8, air[k], k % 4 == 3 ? 8 : 6, 0.55f * human(23, k));
                }
                break;
            }
            default:   // Brawl: sawing on the root and the fifth, then a wild run
                for (int step = 0; step < 64; ++step) {
                    if (step % 2 == 1 && ch(24, step) >= (0.4f + 0.5f * density) * busy) continue;
                    const int bar = step / 16;
                    const int d = bar < 3 ? ((step / 2) % 2 == 0 ? 0 : fifth) : (step % 16) / 2;
                    w.note(step, d, 1, (step % 4 == 0 ? 0.65f : 0.5f) * human(25, step));
                }
                break;
        }
        return;
    }

    if (element == GlassClinks) {
        // Glasses clinking: tuned, high, in little clusters (a toast).
        out.setGrid(pat::Sixteenth, 4);
        out.followChords = false;
        float chanceOf = 0;
        switch (in.character) {
            case LivelyNight: chanceOf = 0.25f + 0.4f * act; break;
            case QuietCorner: chanceOf = 0.08f + 0.2f * act; break;
            default: chanceOf = 0.3f + 0.45f * act; break;
        }
        for (int beat = 0; beat < 16; ++beat) {
            if (beat != 2 && ch(30, beat) >= chanceOf * lerp(0.6f, 1.4f, life)) continue;
            const int at = beat * 4 + int(ch(31, beat) * 3.0f), clinks = 1 + int(ch(32, beat) * (1.0f + 2.5f * busy));
            for (int k = 0; k < clinks && at + k < 64; ++k) w.note(at + k, int(ch(33, at + k) * 5.0f), 1, (0.45f + 0.3f * ch(34, at + k)) * human(35, at + k));
        }
        return;
    }

    if (element == Squeezebox) {
        // Squeezebox chords on the chord: an oom-pah, long chords by the fire, frantic in a brawl. Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = true;
        out.base = -2;
        const auto chord = [&](int at, int length, float velocity) {
            w.note(at, 0, length, velocity);
            w.note(at, 2, length, velocity * 0.85f);
            w.note(at, 4, length, velocity * 0.85f);
        };
        switch (in.character) {
            case LivelyNight:   // off-beat chords
                for (int beat = 0; beat < 8; ++beat)
                    if (beat % 2 == 1 || ch(40, beat) < 0.3f * busy) chord(beat * 4 + 2, 2, 0.5f * human(41, beat));
                break;
            case QuietCorner:
                chord(0, 14, 0.5f);
                if (ch(42, 0) < 0.3f + 0.6f * act) chord(16, 14, 0.45f);
                break;
            default:
                for (int step = 0; step < 32; step += 2)
                    if (step % 4 == 2 || ch(43, step) < (0.3f + 0.4f * density) * busy) chord(step, 1, 0.5f * human(44, step));
                break;
        }
        return;
    }

    if (element == Stomps) {
        // Boots stamping (row 0) and hands clapping (row 1). Two bars.
        out.setGrid(pat::Sixteenth, 2);
        out.followChords = false;
        switch (in.character) {
            case LivelyNight:
                for (int beat = 0; beat < 8; ++beat) {
                    if (beat % 4 == 0 || ch(50, beat) < 0.2f + 0.7f * busy) w.note(beat * 4, 0, 1, 0.7f * human(51, beat));
                    if (beat % 2 == 1 && ch(52, beat) < (0.4f + 0.5f * density) * busy) w.note(beat * 4, 1, 1, 0.6f * human(53, beat));
                }
                break;
            case QuietCorner:   // a foot tapping now and then
                w.note(0, 0, 1, 0.4f);
                if (ch(54, 0) < 0.3f + 0.6f * act) w.note(16, 0, 1, 0.35f);
                break;
            default:   // Brawl: stamping, chairs, fists on tables
                for (int step = 0; step < 32; step += 2)
                    if (step % 8 == 0 || ch(55, step) < (0.35f + 0.5f * density) * busy) w.note(step, ch(56, step) < 0.6f ? 0 : 1, 1, (0.5f + 0.3f * ch(57, step)) * human(58, step));
                break;
        }
        return;
    }
    out.setGrid(pat::Sixteenth, 1);
}

void TavernPlace::configure(const PlaceContext& c) {
    const auto& s = c.s;
    const int q = frame.quality;
    const float tension = s.tension, tone = s.tone;
    const bool lively = s.character == LivelyNight, quiet = s.character == QuietCorner, brawl = s.character == Brawl;

    // The lute: bright gut strings, short when strummed hard.
    {
        auto& p = lute.patch;
        p.decay = (quiet ? 2.8f : 1.7f) * lerp(1.0f, 0.55f, tension);
        // A strum, not a block chord: the stroke crosses the strings (quicker in a brawl).
        p.strumMs = brawl ? 5.0f : (quiet ? 0.0f : 9.0f);
        p.brightness = 0.3f + 0.5f * tone;
        p.spread = 0.4f;
        p.pan = -0.25f;
        p.voices = byQuality(q, 5, 7, 8);
        // Strummed gut courses, a lute's body.
        p.body = dsp::BodyLute;
        p.shape = { 0.18f, 0.5f, 0.05f, 0.8f, 3.0f, 0.4f, 0.0f };
        p.sympathetic = 0.06f;
        p.gain = 1.0f;
    }
    // The recorded lute: the developer's classical guitar played as a lute. Paired gut
    // courses (a second string a few cents off), the lute's lighter body, a shorter
    // ring, brighter strings; the strum crosses the courses as before.
    {
        auto& p = luteRec.patch;
        p.instrument = "guitar";
        p.decay = (quiet ? 4.5f : 2.6f) * lerp(1.0f, 0.55f, tension);
        p.strumMs = lute.patch.strumMs;
        p.toneHz = lerpExp(3200.0f, 13000.0f, tone);
        p.velocityTone = 1.2f;
        p.courseCents = 4.0f;
        p.courseLevel = 0.45f;
        p.courseMs = 1.2f;
        p.body = dsp::BodyLute;
        p.bodyAmount = 0.35f;
        p.sympathetic = 0.03f;
        p.release = 0.25f;
        p.spread = 0.4f;
        p.pan = -0.25f;
        p.voices = byQuality(q, 5, 7, 8);
        p.gain = 0.65f;
    }
    // A frame drum on the root and its rim.
    {
        auto& p = drum.patch;
        auto& head = p.rows[0];
        head = {};
        head.semis = -12;
        head.bend = 1.08f;
        head.decay = quiet ? 1.2f : 0.7f;
        head.tone = 0.55f;
        head.noise = 0.08f;
        head.noiseHz = 600;
        head.noiseDecay = 0.1f;
        head.ratio = 1.6f;
        head.partial = 0.3f;
        head.pan = 0.15f;
        head.kind = dsp::DrumMembrane;
        head.position = 0.3f;
        head.hardness = 0.35f;
        auto& rim = p.rows[1];
        rim = {};
        rim.semis = 12.0f + float(music::degree(frame.mood, 4));
        rim.bend = 1.03f;
        rim.decay = 0.15f;
        rim.tone = 0.3f;
        rim.noise = 0.35f;
        rim.noiseHz = lerpExp(1500.0f, 3200.0f, tone);
        rim.noiseQ = 1.2f;
        rim.noiseDecay = 0.05f;
        rim.ratio = 1.5f;
        rim.partial = 0.2f;
        rim.pan = 0.25f;
        p.spread = 0.1f;
        p.voices = 5;
        p.gain = 1.0f;
    }
    // The fiddle: bowed gut strings, harmonic, a quick bow (long and sweet in the corner).
    {
        auto& p = fiddle.patch;
        // A fiddle: one player, bowed near the bridge in a brawl, a violin's body.
        p.model = BowedString;
        p.size = dsp::Violin;
        p.players = 1;
        p.position = brawl ? 0.06f : 0.1f;
        p.brightness = lerp(0.4f, 0.85f, tone) * (quiet ? 0.85f : 1.0f);
        p.noisiness = 0.25f + 0.2f * tension;
        p.attack = quiet ? 0.25f : 0.05f;
        p.release = quiet ? 0.6f : 0.15f;
        p.glide = 0.04f;
        p.vibratoDepth = (quiet ? 0.25f : 0.12f) + 0.08f * s.life;
        p.transpose = 12.0f;
        p.spread = 0.2f;
        p.pan = 0.2f;
        p.voices = 3;
        p.held = true;
        p.gain = 1.0f;
    }
    // The recorded fiddle: the developer's bowed violin (open strings, looped while a
    // note is held, the recorded release when the bow lifts). Vibrato on the stopped
    // notes, sweeter and slower in the quiet corner; the drone notes stay open strings.
    {
        auto& p = fiddleRec.patch;
        p.instrument = "violin-sustain";
        p.held = true;
        p.attack = quiet ? 0.12f : 0.0f;
        p.release = quiet ? 0.5f : 0.15f;
        p.vibratoHz = quiet ? 5.0f : 5.8f;
        p.vibratoDepth = (quiet ? 0.2f : 0.12f) + 0.06f * s.life;
        p.vibratoDelay = quiet ? 0.35f : 0.2f;
        p.glide = 0.04f;
        p.quickBelow = 0.3f;   // the reel's and the brawl's quick bowing
        p.toneHz = lerpExp(3500.0f, 14000.0f, tone) * (quiet ? 0.85f : 1.0f);
        p.velocityTone = 0.8f;
        p.velocityGain = 0.6f;
        p.transpose = 12.0f;
        p.lowestKey = 55;    // the violin's G string
        p.highestKey = 83;   // up to a fifth above the highest recorded note
        p.spread = 0.2f;
        p.pan = 0.2f;
        p.voices = 4;
        p.gain = 2.6f;
    }
    // Glass clinks: thin glass, its overtones a quarter-tone off any note.
    {
        auto& p = glasses.patch;
        p.set({ 1.0f, 2.32f, 4.25f, 6.63f }, { 1.0f, 0.55f, 0.35f, 0.25f }, { 1.0f, 0.45f, 0.25f * (0.4f + tone), 0.12f * tone });
        p.extras.beatHz = 2.5f;
        p.extras.variation = 0.45f;
        p.decay = quiet ? 1.6f : 0.9f;
        p.hardness = 0.85f;
        p.toneHz = lerpExp(5000.0f, 13000.0f, tone);
        p.transpose = 24.0f;
        p.spread = 0.8f;
        p.voices = byQuality(q, 3, 4, 6);
        p.gain = 1.0f;
    }
    // The squeezebox: two reeds a little apart, a bellows' breath.
    {
        auto& p = squeezebox.patch;
        p.held = true;
        p.legato = false;
        p.attack = quiet ? 0.3f : 0.03f;
        p.decay = 0.3f;
        p.sustain = 0.8f;
        p.release = quiet ? 0.5f : 0.1f;
        p.detune = 10.0f;
        p.sub = 0.0f;
        p.cutoff = lerpExp(700.0f, 2800.0f, tone);
        p.envOctaves = 0.3f;
        p.q = 0.9f;
        p.width = 0.5f;
        p.vibratoHz = 5.5f;
        p.vibratoDepth = 0.03f;
        p.tremoloPerBeat = 2.0f;
        p.tremoloDepth = 0.06f + 0.3f * tension;
        p.spread = 0.3f;
        p.pan = -0.1f;
        p.voices = byQuality(q, 4, 6, 7);
        // A squeezebox: free reeds in pairs, beating, a bright chamber.
        p.model = dsp::SynthReed;
        p.formantHz = 2000.0f;
        p.formantLevel = 0.6f;
        p.breath = 0.035f;
        // The left hand: a bass note under the chords.
        p.pedal = quiet ? 0.25f : 0.35f;
        p.pedalSemis = -12;
        p.gain = 1.0f;
    }
    // Stomps on the floorboards (tuned low, woody) and claps.
    {
        auto& p = stomps.patch;
        auto& stomp = p.rows[0];
        stomp = {};
        stomp.semis = -24;
        stomp.bend = 1.3f;
        stomp.decay = 0.25f;
        stomp.tone = 0.8f;
        stomp.noise = 0.25f;
        stomp.noiseHz = 350;
        stomp.noiseQ = 1.0f;
        stomp.noiseDecay = 0.05f;
        stomp.ratio = 2.3f;
        stomp.partial = 0.3f;
        auto& clap = p.rows[1];
        clap = {};
        clap.tuned = false;
        clap.hz = 100;
        clap.tone = 0.0f;
        clap.noise = 1.0f;
        clap.noiseHz = lerpExp(1200.0f, 2500.0f, tone);
        clap.noiseQ = 1.0f;
        clap.noiseDecay = 0.07f;
        clap.pan = -0.2f;
        p.spread = 0.5f;
        p.voices = 6;
        p.gain = 1.0f;
    }
    // Tension: drums, stomps and the squeezebox's bellows drive on, the lute shortens;
    // Life brings the glasses and the crowd's stamping; Machinery a busier drummer.
    // The quiet corner has the room to itself; a brawl's crowd soaks up the echoes.
    sendScale.fill(quiet ? 1.3f : (brawl ? 0.55f : 1.0f));
    level = { 0.55f * (1.0f - 0.25f * tension), 0.8f * (0.7f + 0.6f * tension) * (0.7f + 0.6f * s.machinery), 0.18f * (quiet ? 1.1f : 1.0f),
              0.14f * lerp(0.6f, 1.4f, s.life), 0.2f * (quiet ? 1.1f : 1.0f), 0.6f * (0.6f + 0.8f * tension) * lerp(0.8f, 1.2f, s.life) * (brawl ? 1.1f : 1.0f) };

    // A warm wooden room, tape warmth.
    room.set(lerp(0.1f, 0.45f, s.size), lerp(0.5f, 1.4f, s.size) * (1.0f + 0.3f * s.magic), lerpExp(2000.0f, 6000.0f, tone));
    roomSend = (quiet ? 0.4f : 0.3f) + 0.4f * s.fxDepth;
    warmth.set((lively ? 0.5f : (quiet ? 0.65f : 0.8f)) * (0.5f + 0.6f * s.fxDepth));
    dry = lerp(1.0f, 0.7f, s.size);
    for (auto& f : tilt) f.setCutoff(lerpExp(900.0f, 18000.0f, tone) * (0.8f + 0.5f * s.era) * (quiet ? 0.8f : 1.0f), rate);
    presence = 1.2f * (tone - 0.3f) + 0.6f * (s.era - 0.3f);
    for (auto& f : shelf) f.setCutoff(2000.0f, rate);
}

bool TavernPlace::shape(const PlaceContext&, const Scheduled& x, NoteOn& on) {
    // Glasses and boots all round the room.
    if (x.element == GlassClinks || x.element == Stomps) {
        on.hasPan = true;
        on.pan = 0.7f * (2.0f * on.random - 1.0f);
    }
    return true;
}

void TavernPlace::mix(const PlaceContext&, float* left, float* right, int n) {
    for (int i = 0; i < n; ++i) {
        const auto k = std::size_t(i);
        float l = 0, r = 0;
        for (int e = 0; e < placeElements; ++e) {
            l += busL[std::size_t(e)][k];
            r += busR[std::size_t(e)][k];
        }
        warmth.process(l, r);
        float wl = 0, wr = 0;
        room.process(l * roomSend, r * roomSend, wl, wr);
        l = dry * l + 0.6f * wl;
        r = dry * r + 0.6f * wr;
        l = tilt[2].lp(tilt[0].lp(l));
        r = tilt[3].lp(tilt[1].lp(r));
        left[i] = l + presence * (l - shelf[0].lp(l));
        right[i] = r + presence * (r - shelf[1].lp(r));
    }
}

} // namespace wb
