#include "Drama.h"
#include "../Arrangement/Arrangement.h"

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
//                                               drums perc metal swell strings pulse bass pluck low brass choir piano braam synth
constexpr std::array<float, Layer::PartCount> trimDb { 7.0f, -6.0f, 5.0f, 4.0f, 1.0f, 0.0f, 0.0f, 0.0f, -10.0f, -8.0f, 3.0f, 0.0f, 0.0f, 0.0f, 5.0f };
} // namespace

Layer::Layer() {
    parts = { &drums, &perc, &metal, &swell, &strings, &pulse, &bass, &pluck, &low, &brass, &choir, &piano, &braam, &synthStab, &hands };
    // The recordings each part plays (resolved once, when the bank is ready).
    strings.patch.instrument = "violin-sustain";
    pulse.patch.instrument = "mlg-pulse-long";
    pulse.patch.shortInstrument = "mlg-pulse-short";
    bass.patch.instrument = "mlg-staticbass-long";
    bass.patch.shortInstrument = "mlg-staticbass-short";
    pluck.patch.instrument = "guitar";
    piano.patch = pianoPatch();
    braam.patch.instrument = "mlg-square-long";
    synthStab.patch.instrument = "mlg-cyberpunk-long";
    synthStab.patch.shortInstrument = "mlg-cyberpunk-short";
}

void Layer::prepare(float sampleRate) {
    rate = sampleRate;
    for (auto* p : parts) p->prepare(rate);
    hall.prepare(rate);
    level.setTime(0.08f, rate / float(chunk));
    reset();
}

void Layer::reset() {
    for (auto* p : parts) p->reset();
    hall.clear();
    gainNow.fill(-1.0f);
    quiet.fill(1 << 30);
    level.snap(0);
    latched = previous = Calm;
    x = 0;
    awake = false;
    fresh = true;
    gearChange = false;
    lastBar = INT64_MIN;
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
    hall.set(t.hall, lerp(1.5f, 4.5f, t.hall), lerpExp(2500.0f, 7000.0f, tone));
    hallWet = 0.35f + 0.4f * t.hall;
}

void Layer::add(int part, int offset, float semis, float velocity, double beat, double lengthBeats, int row, float pan) {
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
    note.on.seconds = float(lengthBeats / std::max(frame.beatsPerSecond, 0.01));
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
    const auto played = [&](int semis, float v) { add(part, offset, float(semis), velocity * v, beat, length); };
    played(r - 12, 0.9f);
    played(r - 12 + fifth, 0.75f);
    played(r, 0.8f);
    if (!power) played(r + 12 + third, 0.6f);
}

void Layer::scheduleStep(int64_t step, int offset, int placeType, int style, const WorldFrame& world, float tension, bool sync, double stepBeat,
                         const Cue& cue, double beatsPerSample) {
    (void) beatsPerSample;
    const auto& th = theme(placeType);
    const int pos = int(((step % 16) + 16) % 16);
    const int64_t bar = music::floorDiv(step, int64_t(16));
    const int mood = world.mood;
    const int chordNow = sync ? music::chordDegree(stepBeat, world[Danger], world.harmony, mood) : world.chord;
    const int t = latched;
    const int si = std::clamp(style, 1, StyleCount - 1) - 1;
    const bool leads = cue.rank == 0;
    const float heatV = 0.82f + 0.3f * heat;
    const double stepLen = 0.25;
    // The section's start: a stinger lands there after a Riser, Drum fill, Drop or
    // Reverse swell.
    const double sectionStart = frame.beat - cue.sinceStart;
    const bool landing = cue.arrived != arr::TransCut && cue.arrived != arr::TransCrossfade && cue.arrived != arr::TransFilter
                         && std::abs(stepBeat - sectionStart) < 0.5 * stepLen;
    // Into a Drop: silence (the world ducks away).
    if (cue.transition == arr::TransDrop && cue.transitionT >= 0.0f) return;

    // Gear change (the tier rose on this bar) or an arrangement landing: impact, crash,
    // a big stab (the chromatic mediant into a climax), a braam.
    if (pos == 0 && ((gearChange && t >= Action) || (landing && t >= Tension))) {
        const bool big = t == Climax || landing;
        if (leads) {
            add(PPerc, offset, 0, big ? 1.1f : 0.85f, stepBeat, 2.0, 3);
            add(PMetal, offset, 0, big ? 0.9f : 0.6f, stepBeat, 2.0, 0);
        }
        const int colour = gearChange && t == Climax && style != StyleTragic && style != StyleSuspense ? 1 : 0;
        const int hit = th.hitB == HitMetal ? PMetal : th.hitB;
        (void) hit;
        const auto stab = [&](int which, float v, double length) {
            switch (which) {
                case HitBrass: chord(PBrass, offset, world.root, mood, chordNow, v, stepBeat, length, false, colour); break;
                case HitChoir: chord(PChoir, offset, world.root, mood, chordNow, v, stepBeat, length * 2.0, false, colour); break;
                case HitPiano: chord(PPiano, offset, world.root, mood, chordNow, v, stepBeat, length * 3.0, true, colour); break;
                case HitBraam: add(PBraam, offset, float(music::inRange(music::chordTone(mood, chordNow, 0), 0) - 24), v, stepBeat, length * 2.0); break;
                case HitSynth: chord(PSynth, offset, world.root, mood, chordNow, v, stepBeat, 0.5, true, colour); break;
                default: add(PMetal, offset, 0, v, stepBeat, 1.0, 1); break;
            }
        };
        stab(th.hitA, 0.95f, 1.0);
        stab(th.hitB, 0.85f, 1.0);
        if (th.hitA != HitPiano && th.hitB != HitPiano && big) chord(PPiano, offset, world.root, mood, chordNow, 0.9f, stepBeat, 3.0, true, colour);
        if (th.hitA != HitBraam && th.hitB != HitBraam && big && (placeType >= SpaceStation || th.kit == KitHybrid || th.kit == KitMetal))
            add(PBraam, offset, float(music::inRange(music::chordTone(mood, chordNow, 0), 0) - 24), 0.8f, stepBeat, 2.0);
        if (t == Climax) return;   // the climax's own first downbeat is the stab
    }

    // Into the next section: a drum roll building over the last bar of a Riser or a Drum
    // fill, and a cymbal swell over the whole transition.
    if (cue.transitionT >= 0.0f && (cue.transition == arr::TransRiser || cue.transition == arr::TransDrumFill || cue.transition == arr::TransReverse)) {
        const double remaining = cue.transitionLength * (1.0 - double(cue.transitionT)) - (stepBeat - frame.beat);
        if (leads && std::abs(remaining - cue.transitionLength) < 0.5 * stepLen) add(PSwell, offset, 0, 0.9f, stepBeat, cue.transitionLength, 0);
        if (leads && remaining <= 4.0 + 1.0e-6 && remaining > 0.0 && t >= Tension) {
            const float up = float(1.0 - remaining / 4.0);
            add(PPerc, offset, 0, 0.35f + 0.65f * up, stepBeat, stepLen, (step & 1) == 0 ? 0 : 1);
            if (remaining <= 1.0 + 1.0e-6) add(PPerc, offset, 0, 0.8f, stepBeat, stepLen, 2);
            if ((step & 1) == 0) add(PDrums, offset, 0, 0.4f + 0.6f * up, stepBeat, stepLen, 0);
            return;
        }
    }
    if (t <= Calm) return;
    const int ti = t - 1;

    // Drums (the leading slot).
    const auto& d = drumBars[si][ti];
    const bool fillBar = (t >= Action && (bar & 3) == 3) || (t == Climax && (bar & 1) == 1);
    if (leads) {
        const auto hit = [&](const char* line, int part, int row, float gain) {
            const char c = line[pos];
            const float v = velocityOf(c);
            if (v <= 0.0f) return;
            if (c == 'g' && rand(step, 31u + uint32_t(row) + uint32_t(part) * 7u) > 0.6f + 0.4f * heat) return;
            const int played = part == PHands && c == 'g' ? 1 : row;   // the ghost strokes muted
            add(part, offset, 0, v * gain * heatV * (0.93f + 0.14f * rand(step, 41u + uint32_t(row))), stepBeat, stepLen, played);
        };
        const bool metalKit = th.kit == KitMetal, hybrid = th.kit == KitHybrid;
        hit(d.low, PDrums, 0, 1.0f);
        hit(d.mid, PDrums, 1, 0.9f);
        if (metalKit) hit(d.high, PMetal, 1, 0.55f);
        else if (hybrid) hit(d.high, PMetal, 3, 0.7f);
        else hit(d.high, PHands, 0, 0.8f);
        hit(d.snare, PPerc, 0, th.kit == KitMarch ? 1.0f : 0.8f);
        // Toms: their line, and a fill on the last beat of every fourth bar (every other at a climax).
        if (fillBar && pos >= 12) add(PPerc, offset, 0, (0.6f + 0.1f * float(pos - 12)) * heatV, stepBeat, stepLen, pos < 14 ? 1 : 2);
        else {
            const char c = d.tom[pos];
            if (velocityOf(c) > 0.0f) add(PPerc, offset, 0, velocityOf(c) * 0.85f * heatV, stepBeat, stepLen, (pos & 4) != 0 ? 1 : 2);
        }
        // Pistons as rhythm (metal kits), hats (hybrid kits).
        if (metalKit && (pos & 1) == 0 && (t >= Action || (pos & 3) == 0)) add(PMetal, offset, 0, (pos % 4 == 0 ? 0.9f : 0.6f) * heatV, stepBeat, stepLen, 2);
        if (hybrid && t >= Action && ((pos & 1) == 0 || t == Climax)) add(PMetal, offset, 0, ((pos & 1) == 0 ? 0.55f : 0.35f) * heatV, stepBeat, stepLen, 3);
        // The phrase line at a climax: an impact and a crash every four bars.
        if (t == Climax && pos == 0 && (bar & 3) == 0) {
            add(PPerc, offset, 0, 1.0f, stepBeat, 2.0, 3);
            add(PMetal, offset, 0, 0.75f, stepBeat, 2.0, 0);
        }
        // A cymbal swell into the next phrase.
        if (pos == 0 && ((t == Climax && (bar & 3) == 3) || (t == Action && (bar & 7) == 7))) add(PSwell, offset, 0, 0.75f, stepBeat, 4.0, 0);
    }

    // The ostinato (the two leading slots): a pedal on the key's root at Tension, on the
    // chord's root from Action (a rising sequence where the harmony rises).
    if (cue.rank <= 1) {
        const char* line = ostinatos[si][ti];
        const char c = line[pos];
        if (c != '.' && c != '-') {
            int ties = 0;
            while (pos + 1 + ties < 16 && line[pos + 1 + ties] == '-') ++ties;
            const int degree = t == Tension ? 0 : chordNow;
            const int root = music::inRange(music::chordTone(mood, degree, 0), 0);
            int semis = root;
            switch (c) {
                case 'o': semis = root + 12; break;
                case 'f': semis = root + 7; break;
                case 't': semis = root + (music::chordTone(mood, degree, 1) - music::chordTone(mood, degree, 0)); break;
                case 'b': semis = root + 1; break;
                default: break;
            }
            // Tension at the top: the fifths become a semitone's rub.
            if (c == 'f' && tension > 0.72f && (bar & 1) == 1) semis = root + 1;
            const float v = (c == 'R' ? 1.0f : 0.66f) * heatV * (0.94f + 0.12f * rand(step, 51u));
            const double length = (ties > 0 ? double(ties + 1) : 0.6) * stepLen;
            static constexpr int ostinatoPart[] { PStrings, PPulse, PBass, PPluck, PPiano };
            const int part = ostinatoPart[std::clamp(th.ostinato, 0, 4)];
            const float register_ = th.ostinato == OstBass ? -12.0f : (th.ostinato == OstPiano ? 0.0f : 0.0f);
            add(part, offset, float(semis) + register_, v * (cue.rank == 0 ? 1.0f : 0.7f), stepBeat, length);
            // The low doubling: a thump under the accents (and on every beat at a climax).
            if (leads && t >= Action && (c == 'R' || (t == Climax && (pos & 3) == 0)))
                add(PLow, offset, float(root - 12), (c == 'R' ? 0.95f : 0.7f) * heatV, stepBeat, stepLen);
            // The brass ostinato at a climax: the accents doubled, short.
            if (t == Climax && c == 'R' && (th.hitA == HitBrass || th.hitB == HitBrass)) add(PBrass, offset, float(semis), 0.6f * heatV, stepBeat, 0.25);
            // Metal kits ring a clank on the accents.
            if (leads && th.kit == KitMetal && c == 'R') add(PMetal, offset, 0, 0.6f, stepBeat, stepLen, 1);
        }
    }

    // Stabs and hits on the downbeats.
    {
        const auto& h = hitBars[si][ti];
        const char c = h.line[pos];
        if (velocityOf(c) > 0.0f && ((bar % h.every) + h.every) % h.every == 0) {
            const float v = velocityOf(c) * heatV * (cue.rank >= 2 ? 0.75f : 1.0f);
            // Stabs, short and hard (held longer in the broad styles); a climax keeps them
            // shortest so the drums come through.
            const double length = style == StyleEpic || style == StyleTragic ? (t == Climax ? 1.0 : 2.0) : (t == Climax ? 0.35 : (c == 'X' ? 0.75 : 0.5));
            const int colour = tension > 0.85f && t == Climax && (bar & 3) == 2 ? 2 : 0;
            const auto stab = [&](int which, float gain) {
                switch (which) {
                    case HitBrass: chord(PBrass, offset, world.root, mood, chordNow, v * gain, stepBeat, length, false, colour); break;
                    case HitChoir: chord(PChoir, offset, world.root, mood, chordNow, v * gain, stepBeat, std::max(0.75, length * 1.5), true, colour); break;
                    case HitPiano: chord(PPiano, offset, world.root, mood, chordNow, v * gain, stepBeat, 1.5, true, colour); break;
                    case HitBraam: add(PBraam, offset, float(music::inRange(music::chordTone(mood, chordNow, 0), 0) - 24), v * gain, stepBeat, 2.0); break;
                    case HitSynth: chord(PSynth, offset, world.root, mood, chordNow, v * gain, stepBeat, 0.5, true, colour); break;
                    default: add(PMetal, offset, 0, v * gain, stepBeat, 1.0, 1); break;
                }
            };
            stab(th.hitA, 1.0f);
            // The second hit joins on the phrase lines, and on every other bar's downbeat at a climax.
            if ((t == Climax && pos == 0 && (bar & 1) == 0) || ((bar & 3) == 0 && pos == 0 && t >= Action)) stab(th.hitB, 0.8f);
        }
    }
}

bool Layer::render(int placeType, const WorldFrame& world, const Cue& cue, float tension, double beat, double beatsPerSample, bool sync, bool notes,
                   uint64_t clock, float* left, float* right, int n) {
    (void) clock;
    // Classic (Drama 0): the layer does not exist.
    if (cue.drama <= 0.0f && !awake) return false;
    if (placeType != type) {
        // A different Place in the slot: what rang is gone with the old one.
        if (type >= 0) {
            for (auto* p : parts) p->reset();
            hall.clear();
        }
        type = placeType;
        fresh = true;
    }
    if (placeType <= NoPlace) return false;
    const double span = beatsPerSample * double(n);
    x = amountFor(world.intensity, tension, cue.drama);
    int want = cue.tier >= 0 && cue.drama > 0.0f ? std::clamp(cue.tier, 0, int(Climax)) : tierFor(x);
    if (cue.drama <= 0.0f) want = Calm;
    // Heat: how far into its tier the drama is (velocities and brightness grow with it).
    static constexpr float edges[TierCount + 1] { 0.0f, 0.1f, 0.45f, 0.78f, 1.0f };
    heat = cue.tier >= 0 ? 0.6f : clamp01((x - edges[want]) / std::max(0.05f, edges[want + 1] - edges[want]));
    if (fresh) {
        latched = previous = want;
        fresh = false;
    }
    const bool transitions = cue.drama > 0.0f && (cue.transitionT >= 0.0f || cue.sinceStart < 0.5);
    if (want <= Calm && latched <= Calm && !transitions && idleSamples > int(6.0f * rate)) {
        // Asleep, it keeps its place in the song: it wakes on the next bar line, not mid-bar.
        lastBeat = beat;
        lastBar = int64_t(std::floor(beat / 4.0));
        if (awake) {
            for (auto* p : parts) p->reset();
            hall.clear();
            awake = false;
        }
        return false;
    }
    awake = true;
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

    // This chunk's steps (sixteenths), the tier latched on each bar line.
    count = 0;
    // A jump back (the transport located): latch again at once.
    if (beat < lastBeat - 1.0e-6 || beat > lastBeat + span + 1.0) latched = previous = want;
    lastBeat = beat;
    forEachStep(beat, span, 4, 0.0f, [&](int64_t step) {
        const double stepBeat = double(step) / 4.0;
        const int offset = std::clamp(int(std::ceil((stepBeat - beat) / beatsPerSample - 1.0e-9)), 0, n - 1);
        gearChange = false;
        if ((step & 15) == 0) {
            const int64_t b = step / 16;
            if (b != lastBar) {
                previous = latched;
                latched = want;
                gearChange = latched > previous;
                lastBar = b;
            }
        }
        if (notes) scheduleStep(step, offset, placeType, style, world, tension, sync, stepBeat, cue, beatsPerSample);
    });
    // Sort by offset (insertion: few notes).
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
    startedCount += uint32_t(count);

    // The parts' levels: what each tier lets play (a part whose last note has rung out
    // is not rendered at all).
    std::fill(busL.begin(), busL.begin() + n, 0.0f);
    std::fill(busR.begin(), busR.begin() + n, 0.0f);
    const float tierGain = latched == Climax ? 1.0f : (latched == Action ? 0.8f : (latched == Tension ? 0.55f : 0.5f));
    for (int k = 0; k < PartCount; ++k) {
        auto* part = parts[std::size_t(k)];
        bool any = false;
        for (int i = 0; i < count && !any; ++i) any = pending[std::size_t(i)].part == k;
        auto& q = quiet[std::size_t(k)];
        if (any) q = 0;
        // A part whose last note has rung out (well below -60 dB) is neither updated nor
        // rendered: an idle part costs nothing.
        static constexpr std::array<float, PartCount> tails { 5.0f, 7.0f, 5.0f, 24.0f, 3.0f, 4.0f, 4.0f, 4.0f, 2.0f, 3.0f, 5.0f, 10.0f, 6.0f, 4.0f, 3.0f };
        if (!any && q > int(tails[std::size_t(k)] * rate)) continue;
        q += n;
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
            if ((partMask >> k) & 1u) part->start(frame, note.on);
        }
        if (done < n) part->render(partL.data() + done, partR.data() + done, n - done);
        // At a climax the drums lead and what sustains steps back, so it stays punchy.
        const bool rhythm = k == PDrums || k == PPerc || k == PMetal || k == PLow;
        const float climaxShape = latched == Climax ? (rhythm ? 1.12f : 0.8f) : 1.0f;
        const float g = tierGain * climaxShape * dbToGain(trimDb[std::size_t(k)]);
        float& now = gainNow[std::size_t(k)];
        if (now < 0.0f) now = g;
        const float step = (g - now) / float(n);
        for (int i = 0; i < n; ++i) {
            now += step;
            busL[std::size_t(i)] += finiteOr(partL[std::size_t(i)]) * now;
            busR[std::size_t(i)] += finiteOr(partR[std::size_t(i)]) * now;
        }
    }
    // The layer's space and level (the second and further slots quieter).
    const float rankGain = cue.rank == 0 ? 1.0f : (cue.rank == 1 ? 0.75f : 0.55f);
    const float target = dbToGain(layerTrimDb) * rankGain * (0.75f + 0.25f * clamp01(cue.drama));
    const float from = level.value, to = level.next(target);
    const float wet = hallWet * (latched == Climax ? 0.65f : 1.0f);
    // The leading slot's layer has its own space; the others' reach the World Space with
    // their Place (one reverb, not four).
    if (cue.rank == 0) {
        for (int i = 0; i < n; ++i) {
            const auto k = std::size_t(i);
            float wl = 0, wr = 0;
            hall.process(busL[k] * 0.6f, busR[k] * 0.6f, wl, wr);
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
    return true;
}

} // namespace wb::drama
