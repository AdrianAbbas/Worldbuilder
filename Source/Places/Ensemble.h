#pragma once
// The instrument framework every Place is built from.
//
// A Part is one instrument: a pool of voices of one kind (synth, plucked string,
// modal bell or bar, drum kit, FM, choir, wind, noise sweeps) that plays one
// element's notes. Its sound is a patch the Place sets once per chunk (from its
// Character, Tension, Tone, the World Controls...), so a new Place is mostly a choice
// of parts, patches, pattern generators and an effect chain.
//
// EnsemblePlace renders up to six parts: each part renders its own bus, split at its
// own note starts (so every note begins on its exact sample), and the Place mixes the
// buses through its effects. A part that sustains (drones, pads, choirs) picks up a
// note it missed (the Place was just added, the element switched back on, the
// transport located) by asking its pattern what is sounding now.
//
// Nothing here allocates after prepare().
#include "Place.h"
#include "../DSP/Instruments.h"

namespace wb {

// What every part sees for one chunk.
struct PartFrame {
    float rate = 48000;
    int root = 2, mood = 0, chord = 0, quality = 1;
    double beat = 0;            // at the chunk start
    double beatsPerSecond = 2;
};

// One note for a part.
struct NoteOn {
    float semis = 0;            // above the key root (melodic)
    int row = 0;                // kit row
    float velocity = 1;
    double beat = 0, length = 0.25;   // in beats
    float seconds = 0.125f;     // the length in seconds
    float random = 0.5f, random2 = 0.5f;   // per note, deterministic
    float decay = 1;            // a Place may scale the part's decay for this note
    bool hasPan = false;
    float pan = 0;              // used when hasPan (else the part spreads its notes)
    bool extra = false;
    int tag = 0;                // a Place's own extra note
    // Held notes that follow the chords while they sound.
    bool follow = false;
    int base = 0, degree = 0, alter = 0;
    // A note played live from the keyboard (KEYS): its id (the MIDI note + 1), 0 for a
    // pattern's note. A live note lasts until the part is told it stopped.
    int live = 0;
};

// A live note's length until its key is released (beats).
inline constexpr double liveLength = 1.0e9;

// A held note's pitch and end, for parts that sustain.
struct HeldNote {
    bool on = false, follow = false;
    double until = 0, from = 0;
    int base = 0, degree = 0, alter = 0, live = 0;
    float semis = 0;
    void set(const NoteOn& n) {
        on = true;
        live = n.live;
        follow = n.follow;
        from = n.beat;
        until = n.beat + n.length;
        base = n.base;
        degree = n.degree;
        alter = n.alter;
        semis = n.semis;
    }
    // The pitch now: followers move with the chords.
    float now(const PartFrame& f) const {
        return follow ? float(pat::semitonesFor(true, base, f.mood, f.chord, degree, alter)) : semis;
    }
};

inline int byQuality(int quality, int eco, int standard, int high) { return quality <= 0 ? eco : (quality == 1 ? standard : high); }

// The weight of every instrument (Milestone 6): heavier, slower and more sustained
// than the Places were first voiced. Each part scales its patch's times by these, so
// the whole plugin can be made lighter or heavier here.
namespace weight {
inline constexpr float ring = 1.35f;      // plucked strings, struck bars and bells ring longer
inline constexpr float release = 1.5f;    // held notes (pads, brass, reeds, bows, voices, winds) let go slower
inline constexpr float skin = 1.15f;      // drum skins ring a little longer
inline constexpr float keys = 1.3f;       // electric and FM keys sustain longer
} // namespace weight

class Part {
public:
    virtual ~Part() = default;
    virtual void prepare(float rate) = 0;
    virtual void reset() = 0;
    virtual void start(const PartFrame& f, const NoteOn& note) = 0;
    // Once per chunk, before rendering: releases, retuning, the patch.
    virtual void update(const PartFrame& f) = 0;
    // Adds n samples.
    virtual void render(float* left, float* right, int n) = 0;
    // True for parts that sustain their notes (they catch up on missed ones).
    virtual bool sustains() const { return false; }
    void setKey(uint32_t key, uint32_t salt) { noise.setKey(key, salt); }

    // KEYS (Milestone 14): the part's own transposition (a live note is concert pitch, so
    // the part takes it off), a live note's key released (held notes let go; plucked
    // strings are damped; struck bells, bars and drums ring on), and the keyboard's pitch
    // bend (semitones) and mod wheel (0..1: vibrato where the instrument has it, else
    // brightness), which act on the whole instrument as on a synth.
    virtual float transposeSemis() const { return 0.0f; }
    virtual void stopLive(int) {}
    float bend = 0, mod = 0;

protected:
    // Held notes of live id `id` end now (the next update releases them).
    template <std::size_t N>
    static void endLive(std::array<HeldNote, N>& held, int id) {
        for (auto& h : held)
            if (h.on && h.live == id) h.until = -1.0e18;
    }
    float rate = 48000;
    dsp::Stream noise;
    uint64_t noiseAt = 0;
    float spreadPan(float pan, float spread, float random) const { return std::clamp(pan + spread * (2.0f * random - 1.0f), -1.0f, 1.0f); }
};

//==============================================================================
// Synth voices: brass drones, horns, pads, arps, basses, engines, organs.
struct SynthPatch {
    float attack = 0.01f, decay = 0.3f, sustain = 1.0f, release = 0.4f;
    float detune = 7, sub = 0.3f, cutoff = 800, envOctaves = 1, filterEnvOctaves = 0, filterDecay = 0.07f, q = 0.7f, width = 0.6f;
    float vibratoHz = 5, vibratoDepth = 0, vibratoDelay = 0.3f, glide = 0.05f;
    float tremoloPerBeat = 0, tremoloDepth = 0;   // a pulse in time (throb, trembling)
    float gain = 1, pan = 0, spread = 0.4f, transpose = 0;
    float pedal = 0;          // a tonic pedal under the held notes (level)
    float pedalSemis = -12;
    int voices = 4;
    bool held = true;         // notes last their length (else they decay)
    bool legato = false;      // a note that starts as another ends glides on from it
    // The voice model (dsp::SynthModel): Analog, Brass or Reed. Brass and Reed use
    // cutoff as a brightness scale (1000 = as built), formantHz for the bell or reed
    // chamber, breath for air noise; detune is a second player or reed (cents).
    int model = dsp::SynthAnalog;
    float formantHz = 1200, formantLevel = 0.6f, breath = 0.05f;
};

class SynthPart final : public Part {
public:
    static constexpr int maxVoices = 8;
    SynthPatch patch;
    void prepare(float r) override {
        rate = r;
        reset();
    }
    void reset() override {
        for (auto& v : voice) v.reset();
        pedalVoice.reset();
        for (auto& h : held) h = {};
        claimed.fill(-1.0e9);
        noiseAt = 0;
        trem = 1;
    }
    bool sustains() const override { return patch.held; }
    float transposeSemis() const override { return patch.transpose; }
    void stopLive(int id) override { endLive(held, id); }
    void start(const PartFrame& f, const NoteOn& n) override {
        const float hz = music::noteHz(f.root, n.semis + patch.transpose);
        const int limit = std::clamp(patch.voices, 1, maxVoices);
        int k = -1;
        if (patch.legato) {
            // The voice whose note ends here, nearest in pitch.
            float best = 1.0e9f;
            for (int i = 0; i < limit; ++i) {
                const auto& h = held[std::size_t(i)];
                if (!voice[std::size_t(i)].active() || h.until > n.beat + 1.0e-6 || h.until < n.beat - 0.07 || claimed[std::size_t(i)] == n.beat) continue;
                const float d = std::abs(h.semis - n.semis);
                if (d < best) {
                    best = d;
                    k = i;
                }
            }
        }
        const bool legato = k >= 0;
        if (k < 0) k = int(dsp::pickVoice(voice, limit, [](const dsp::SynthVoice& v) { return v.level(); }));
        auto& v = voice[std::size_t(k)];
        v.model = patch.model;
        v.velocity = n.velocity;
        v.start(hz, patch.attack, patch.decay, patch.held ? patch.sustain : 0.0f, rate, legato);
        v.gain = n.velocity;
        if (!legato) v.pan = dsp::panGains(n.hasPan ? n.pan : spreadPan(patch.pan, patch.spread, n.random));
        claimed[std::size_t(k)] = n.beat;
        // A live note on a part that decays still lets go when its key is released.
        if (patch.held || n.live != 0) held[std::size_t(k)].set(n);
        else held[std::size_t(k)] = {};
        held[std::size_t(k)].semis = n.semis;
        apply(v);
        v.update(rate);
    }
    void update(const PartFrame& f) override {
        bool any = false;
        for (int i = 0; i < maxVoices; ++i) {
            auto& v = voice[std::size_t(i)];
            auto& h = held[std::size_t(i)];
            if (h.on && f.beat >= h.until) {
                v.release(patch.release * weight::release, rate);
                h.on = false;
            }
            if (!v.active()) continue;
            if (h.on) {
                any = true;
                if (h.follow) v.targetHz = music::noteHz(f.root, h.now(f) + patch.transpose);
            }
            apply(v);
            v.update(rate);
        }
        if (patch.pedal > 0.0f && any) {
            const float hz = music::noteHz(f.root, patch.pedalSemis + patch.transpose);
            if (!pedalVoice.active() || pedalVoice.env.stage == dsp::Env::Release) pedalVoice.start(hz, std::max(patch.attack, 0.5f), 1.0f, 1.0f, rate);
            pedalVoice.targetHz = hz;
        } else if (pedalVoice.active() && pedalVoice.env.stage != dsp::Env::Release) {
            pedalVoice.release(std::max(patch.release * weight::release, 0.5f), rate);
        }
        if (pedalVoice.active()) {
            apply(pedalVoice);
            pedalVoice.subLevel = 0.6f;
            pedalVoice.gain = patch.pedal;
            pedalVoice.pan = dsp::panGains(patch.pan);
            pedalVoice.update(rate);
        }
        trem = 1.0f - patch.tremoloDepth * (0.5f + 0.5f * std::cos(float(f.beat * double(patch.tremoloPerBeat)) * dsp::twoPi));
    }
    void render(float* left, float* right, int n) override {
        const float g = patch.gain * trem;
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0;
            for (auto& v : voice)
                if (v.active()) v.tick(l, r);
            if (pedalVoice.active()) pedalVoice.tick(l, r);
            left[i] += l * g;
            right[i] += r * g;
        }
    }

private:
    void apply(dsp::SynthVoice& v) const {
        v.glide = glideFor(patch.glide, rate);
        v.detuneCents = patch.detune;
        v.subLevel = patch.sub;
        v.cutoff = patch.cutoff;
        v.envOctaves = patch.envOctaves;
        v.filterEnvOctaves = patch.filterEnvOctaves;
        v.filterDecay = std::exp(-float(dsp::chunk) / (std::max(patch.filterDecay, 0.005f) * rate));
        v.q = patch.q;
        v.width = patch.width;
        v.vibratoHz = patch.vibratoHz;
        v.vibratoDepth = patch.vibratoDepth + 0.45f * mod;
        v.vibratoDelay = mod > 0.01f ? 0.0f : patch.vibratoDelay;
        v.bend = bend;
        v.formantHz = patch.formantHz;
        v.formantLevel = patch.formantLevel;
        v.breath = patch.breath;
        if (!v.active()) v.model = patch.model;
    }
    std::array<dsp::SynthVoice, maxVoices> voice;
    std::array<HeldNote, maxVoices> held {};
    std::array<double, maxVoices> claimed {};
    dsp::SynthVoice pedalVoice;
    float trem = 1;
};

//==============================================================================
// Plucked strings: lutes, harps, guitars, ouds, zithers, harpsichords. Each note a
// waveguide string (or a course of two) plucked at its point; the part's body (the
// instrument's measured resonances) colours them all, and open strings tuned to the
// key ring along in sympathy.
struct PluckPatch {
    float decay = 1.5f, brightness = 0.5f, gain = 1, pan = 0, spread = 0.5f;
    int voices = 6;
    dsp::Pluck::Shape shape {};
    int body = dsp::BodyGuitar;
    float bodyAmount = 1;        // 0: the bare strings
    float sympathetic = 0;       // open strings ringing along (root and fifth)
    float transpose = 0;
    // Strummed: notes starting together are spread across the strings, low to high on a
    // down-stroke (on the beat and its eighths), high to low on an up-stroke (between),
    // this many ms per string, each a little softer.
    float strumMs = 0;
};

class PluckPart final : public Part {
public:
    static constexpr int maxVoices = 8;
    PluckPatch patch;
    void prepare(float r) override {
        rate = r;
        for (auto& p : voice) p.prepare(rate, 40.0f);
        for (auto& f : dc) f.setCutoff(20.0f, rate);
        for (auto& o : open) o.prepare(std::size_t(rate / 30.0f) + 8);
        for (auto& f : openDamp) f.setCutoff(2500.0f, rate);
        reset();
    }
    void reset() override {
        for (auto& p : voice) p.reset();
        for (auto& f : dc) f.reset();
        for (auto& b : bodyFilter)
            for (auto& f : b) f.reset();
        for (auto& o : open) o.clear();
        for (auto& f : openDamp) f.reset();
        noiseAt = 0;
        bodyType = -1;
        strumBeat = -1.0e9;
        strumCount = 0;
    }
    float transposeSemis() const override { return patch.transpose; }
    void stopLive(int id) override {
        // The key released: the string is damped (a lute or harp player's hand).
        for (std::size_t k = 0; k < voice.size(); ++k)
            if (liveId[k] == id && voice[k].active) {
                voice[k].damp(patch.shape.dampSeconds > 0.0f ? patch.shape.dampSeconds : 0.3f);
                liveId[k] = 0;
            }
    }
    void start(const PartFrame& f, const NoteOn& n) override {
        const auto k = dsp::pickVoice(voice, std::clamp(patch.voices, 1, maxVoices), [](const dsp::Pluck& p) { return p.level(); });
        auto& v = voice[k];
        liveId[k] = n.live;
        const auto& shape = patch.shape;
        // A live note rings until its key is released (stopLive damps it).
        const int length = n.live != 0 ? -1 : (patch.shape.dampSeconds > 0.0f ? std::max(1, int(n.seconds * rate)) : -1);
        float velocity = n.velocity;
        int wait = 0;
        if (patch.strumMs > 0.0f && n.live == 0) {
            // The strings of one stroke: the first note sets where the chord starts.
            if (std::abs(n.beat - strumBeat) > 1.0e-6) {
                strumBeat = n.beat;
                strumFirst = n.semis;
                strumCount = 0;
            }
            // Down on the beat and on its eighths; up on the eighths between and anything
            // off that grid (the sixteenths between, swung off-beats).
            const double eighth = n.beat * 2.0;
            const bool onEighth = std::abs(eighth - std::round(eighth)) < 1.0e-4;
            const bool up = !onEighth || (int64_t(std::llround(eighth)) & 1) != 0;
            const float place = up ? std::max(0.0f, strumFirst + 12.0f - n.semis) : std::max(0.0f, n.semis - strumFirst);
            wait = int(patch.strumMs * 0.001f * rate * place / 4.0f);
            velocity *= 1.0f - 0.06f * float(std::min(strumCount, 5));
            ++strumCount;
        }
        v.start(music::noteHz(f.root, n.semis + patch.transpose), patch.decay * weight::ring * n.decay * (n.extra ? 0.5f : 1.0f),
                dsp::clamp01(patch.brightness + 0.4f * mod), velocity, n.hasPan ? n.pan : spreadPan(patch.pan, patch.spread, n.random), rate, noiseAt, shape,
                length);
        v.wait = wait;
        v.bendTo(std::exp2(bend / 12.0f));
        noiseAt += 1ull << 24;
    }
    void update(const PartFrame& f) override {
        if (patch.body != bodyType) {
            bodyType = std::clamp(patch.body, 0, dsp::BodyCount - 1);
            const auto& b = dsp::bodies[std::size_t(bodyType)];
            for (auto& side : bodyFilter)
                for (std::size_t k = 0; k < side.size(); ++k) side[k].set(b.mode[k].hz, b.mode[k].q, rate);
        }
        // The open strings: the key's root and fifth, low.
        if (patch.sympathetic > 0.0f) {
            const float root = music::noteHz(f.root, -12.0f), fifth = music::noteHz(f.root, -12.0f + float(music::degree(f.mood, 4)));
            openPeriod[0] = rate / root;
            openPeriod[1] = rate / fifth;
        }
        // The pitch wheel bends the sounding strings.
        const float ratio = std::exp2(bend / 12.0f);
        for (auto& v : voice)
            if (v.active) v.bendTo(ratio);
    }
    void render(float* left, float* right, int n) override {
        const auto& b = dsp::bodies[std::size_t(std::max(bodyType, 0))];
        const float amount = dsp::clamp01(patch.bodyAmount);
        const float direct = dsp::lerp(1.0f, b.direct, amount);
        std::array<float, 4> g {};
        for (std::size_t k = 0; k < 4; ++k) g[k] = b.mode[k].gain * amount;
        const bool body = bodyType > dsp::BodyNone && amount > 0.0f;
        const float sym = patch.sympathetic;
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0;
            for (auto& p : voice) {
                if (!p.active) continue;
                const float y = p.tick(noise);
                l += y * p.pan.l;
                r += y * p.pan.r;
            }
            if (body) {
                float bl = l * direct, br = r * direct;
                for (std::size_t k = 0; k < 4; ++k) {
                    bl += g[k] * bodyFilter[0][k].bandpass(l);
                    br += g[k] * bodyFilter[1][k].bandpass(r);
                }
                l = bl;
                r = br;
            }
            if (sym > 0.0f) {
                // Two open strings driven quietly by everything played, ringing on.
                const float drive = 0.04f * (l + r);
                float ring = 0;
                for (std::size_t k = 0; k < 2; ++k) {
                    const float y = openDamp[k].lp(open[k].read(openPeriod[k]));
                    open[k].push(drive + 0.993f * y);
                    ring += y;
                }
                l += sym * ring;
                r += sym * ring;
            }
            // A plucked string's loop passes DC; block it.
            left[i] += dc[0].hp(l) * patch.gain;
            right[i] += dc[1].hp(r) * patch.gain;
        }
    }

private:
    std::array<dsp::Pluck, maxVoices> voice;
    std::array<int, maxVoices> liveId {};
    std::array<dsp::OnePole, 2> dc;
    std::array<std::array<dsp::Svf, 4>, 2> bodyFilter;
    int bodyType = -1;
    std::array<dsp::DelayLine, 2> open;
    std::array<dsp::OnePole, 2> openDamp;
    std::array<float, 2> openPeriod { 400, 300 };
    double strumBeat = -1.0e9;
    float strumFirst = 0;
    int strumCount = 0;
};

//==============================================================================
// Struck modal objects: bells, chimes, marimbas, glockenspiels, anvils, gongs.
struct ModalPatch {
    static constexpr int maxModes = dsp::Bell::maxModes;
    std::array<float, maxModes> ratios { 1, 2, 3, 4, 5, 6, 7, 8 }, decays { 1, 1, 1, 1, 1, 1, 1, 1 }, gains { 1, 0, 0, 0, 0, 0, 0, 0 };
    int modes = 1;
    bool scaleTuned = false;   // partials 3 and 4 become the scale's own third and fifth
    float decay = 2, hardness = 0.5f, gain = 1, pan = 0, spread = 0.6f, toneHz = 9000, transpose = 0;
    float velocityHardness = 0.35f;   // a harder strike when played louder
    dsp::Bell::Extras extras { 0.0f, 0.0f, 0.4f, 0.0f, 0.25f };
    int voices = 4;
    void set(std::initializer_list<float> r, std::initializer_list<float> d, std::initializer_list<float> g) {
        modes = int(std::min<std::size_t>(r.size(), maxModes));
        std::copy_n(r.begin(), modes, ratios.begin());
        std::copy_n(d.begin(), std::min<std::size_t>(d.size(), maxModes), decays.begin());
        std::copy_n(g.begin(), std::min<std::size_t>(g.size(), maxModes), gains.begin());
    }
};

class ModalPart final : public Part {
public:
    static constexpr int maxVoices = 8;
    ModalPatch patch;
    void prepare(float r) override {
        rate = r;
        reset();
    }
    void reset() override {
        for (auto& b : voice) b.reset();
        for (auto& f : tone) f.reset();
    }
    float transposeSemis() const override { return patch.transpose; }
    void start(const PartFrame& f, const NoteOn& n) override {
        auto ratios = patch.ratios;
        if (patch.scaleTuned) {
            ratios[2] = std::exp2(float(music::degree(f.mood, 2)) / 12.0f);
            ratios[3] = std::exp2(float(music::degree(f.mood, 4)) / 12.0f);
        }
        auto& b = voice[dsp::pickVoice(voice, std::clamp(patch.voices, 1, maxVoices), [](const dsp::Bell& v) { return v.level(); })];
        b.extras = patch.extras;
        const float hardness = dsp::clamp01(patch.hardness + patch.velocityHardness * (n.velocity - 0.6f) + 0.35f * mod);
        b.start(music::noteHz(f.root, n.semis + patch.transpose), patch.decay * weight::ring * n.decay, hardness, n.velocity,
                n.hasPan ? n.pan : spreadPan(patch.pan, patch.spread, n.random), rate, ratios.data(), patch.decays.data(), patch.gains.data(), patch.modes);
        b.update();
    }
    void update(const PartFrame&) override {
        for (auto& f : tone) f.setCutoff(std::min(patch.toneHz * (1.0f + 1.5f * mod), 0.45f * rate), rate);
        for (auto& b : voice)
            if (b.active) b.update();
    }
    void render(float* left, float* right, int n) override {
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0;
            for (auto& b : voice) {
                if (!b.active) continue;
                const float y = b.tick();
                l += y * b.pan.l;
                r += y * b.pan.r;
            }
            left[i] += tone[0].lp(l) * patch.gain;
            right[i] += tone[1].lp(r) * patch.gain;
        }
    }

private:
    std::array<dsp::Bell, maxVoices> voice;
    std::array<dsp::OnePole, 2> tone;
};

//==============================================================================
// A drum kit: each pattern row is a drum (two tuned partials, a pitch drop and a
// noise burst). Timpani, frame drums, snares, shakers, stomps, clanks.
struct DrumRow {
    float semis = -12;     // tuned to the key root (when `tuned`), else hz
    bool tuned = true;
    float hz = 200, bend = 1.05f, decay = 1, tone = 0.5f, noise = 0.05f, noiseHz = 800, noiseQ = 0.7f, noiseDecay = 0.1f;
    float ratio = 1.5f, partial = 0.3f, pan = 0;
    // dsp::DrumKind: TwoTone (wood, metal, stomps), Membrane or Timpani; for the skins,
    // where the stick lands (0 centre, 1 edge), how hard it is, snare wires, the shell.
    int kind = dsp::DrumTwoTone;
    float position = 0.3f, hardness = 0.5f, snare = 0, shellHz = 0, shell = 0;
};
struct DrumPatch {
    std::array<DrumRow, 4> rows {};
    float gain = 1, spread = 0;
    int voices = 6;
};

class DrumPart final : public Part {
public:
    static constexpr int maxVoices = 8;
    DrumPatch patch;
    void prepare(float r) override {
        rate = r;
        reset();
    }
    void reset() override {
        for (auto& d : voice) d.reset();
        noiseAt = 0;
    }
    void start(const PartFrame& f, const NoteOn& n) override {
        const auto& row = patch.rows[std::size_t(std::clamp(n.row, 0, 3))];
        auto& d = voice[dsp::pickVoice(voice, std::clamp(patch.voices, 1, maxVoices), [](const dsp::Drum& v) { return v.level(); })];
        d.ratio = row.ratio;
        d.partial = row.partial;
        d.kind = row.kind;
        d.position = row.position;
        // Louder hits are harder.
        d.hardness = dsp::clamp01(row.hardness + 0.3f * (n.velocity - 0.6f) + 0.3f * mod);
        d.snare = row.snare;
        d.shellHz = row.shellHz;
        d.shell = row.shell;
        const float hz = row.tuned ? music::noteHz(f.root, row.semis) : row.hz;
        d.start(hz, row.bend, row.decay * (row.kind == dsp::DrumTwoTone ? 1.0f : weight::skin) * n.decay, row.tone * n.velocity, row.noise * n.velocity, row.noiseHz, row.noiseQ, row.noiseDecay * n.decay,
                n.hasPan ? n.pan : spreadPan(row.pan, patch.spread, n.random), rate, noiseAt);
        d.update(rate);
        noiseAt += 1ull << 24;
    }
    void update(const PartFrame&) override {
        for (auto& d : voice)
            if (d.active()) d.update(rate);
    }
    void render(float* left, float* right, int n) override {
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0;
            for (auto& d : voice) {
                if (!d.active()) continue;
                const float y = d.tick(noise);
                l += y * d.pan.l;
                r += y * d.pan.r;
            }
            left[i] += l * patch.gain;
            right[i] += r * patch.gain;
        }
    }

private:
    std::array<dsp::Drum, maxVoices> voice;
};

//==============================================================================
// FM voices: electric pianos, glassy calls, PA tones, data blips, vibraphones.
// Model Fm: two-operator FM (glassy calls, PA tones, data blips, synthetic keys).
// Model Tine: a tine electric piano (decay: the fundamental's T60; brightness: bell
// and bark; release: the damper).
enum FmModel { FmTwoOp, FmTine };
struct FmPatch {
    float ratio = 2, index = 2, indexFloor = 0.2f, indexSeconds = 0.4f, feedback = 0;
    float attack = 0.002f, decay = 0.6f, sustain = 0, release = 0.3f;
    float vibratoHz = 5, vibratoDepth = 0, glide = 0;
    float gain = 1, pan = 0, spread = 0.5f, transpose = 0;
    int voices = 4;
    bool held = false;
    int model = FmTwoOp;
    float brightness = 0.5f;
};

class FmPart final : public Part {
public:
    static constexpr int maxVoices = 8;
    FmPatch patch;
    void prepare(float r) override {
        rate = r;
        reset();
    }
    void reset() override {
        for (auto& v : voice) v.reset();
        for (auto& v : tines) v.reset();
        for (auto& h : held) h = {};
    }
    bool sustains() const override { return patch.held; }
    float transposeSemis() const override { return patch.transpose; }
    void stopLive(int id) override { endLive(held, id); }
    void start(const PartFrame& f, const NoteOn& n) override {
        if (patch.model == FmTine) {
            const int k = int(dsp::pickVoice(tines, std::clamp(patch.voices, 1, maxVoices), [](const dsp::TineVoice& v) { return v.level(); }));
            auto& v = tines[std::size_t(k)];
            v.start(music::noteHz(f.root, n.semis + patch.transpose), patch.decay * weight::keys * n.decay * 4.0f, dsp::clamp01(patch.brightness + 0.4f * mod), n.velocity, rate);
            v.gainLevel = 1.0f;
            v.pan = dsp::panGains(n.hasPan ? n.pan : spreadPan(patch.pan, patch.spread, n.random));
            held[std::size_t(k)].set(n);
            v.update();
            return;
        }
        const int k = int(dsp::pickVoice(voice, std::clamp(patch.voices, 1, maxVoices), [](const dsp::FmVoice& v) { return v.level(); }));
        auto& v = voice[std::size_t(k)];
        apply(v);
        v.start(music::noteHz(f.root, n.semis + patch.transpose), patch.attack, patch.decay * weight::keys * n.decay, patch.held ? patch.sustain : 0.0f, rate,
                patch.indexSeconds * n.decay);
        v.gain = n.velocity;
        v.pan = dsp::panGains(n.hasPan ? n.pan : spreadPan(patch.pan, patch.spread, n.random));
        if (patch.held || n.live != 0) held[std::size_t(k)].set(n);
        else held[std::size_t(k)] = {};
        v.update(rate);
    }
    void update(const PartFrame& f) override {
        if (patch.model == FmTine) {
            for (int i = 0; i < maxVoices; ++i) {
                auto& v = tines[std::size_t(i)];
                auto& h = held[std::size_t(i)];
                if (h.on && f.beat >= h.until) {
                    // Keys held for the note's length, then the damper.
                    v.release(std::max(patch.release * weight::release, 0.08f));
                    h.on = false;
                }
                if (v.active()) {
                    v.bend = bend;
                    v.update();
                }
            }
            return;
        }
        for (int i = 0; i < maxVoices; ++i) {
            auto& v = voice[std::size_t(i)];
            auto& h = held[std::size_t(i)];
            if (h.on && f.beat >= h.until) {
                v.release(patch.release * weight::release, rate);
                h.on = false;
            }
            if (!v.active()) continue;
            if (h.on && h.follow) v.targetHz = music::noteHz(f.root, h.now(f) + patch.transpose);
            apply(v);
            v.update(rate);
        }
    }
    void render(float* left, float* right, int n) override {
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0;
            for (auto& v : voice) {
                if (!v.active()) continue;
                const float y = v.tick();
                l += y * v.pan.l;
                r += y * v.pan.r;
            }
            for (auto& v : tines) {
                if (!v.active()) continue;
                const float y = v.tick();
                l += y * v.pan.l;
                r += y * v.pan.r;
            }
            left[i] += l * patch.gain;
            right[i] += r * patch.gain;
        }
    }

private:
    std::array<dsp::TineVoice, maxVoices> tines;
    void apply(dsp::FmVoice& v) const {
        v.ratio = patch.ratio;
        v.index = patch.index;
        v.indexFloor = patch.indexFloor;
        v.feedback = patch.feedback;
        v.vibratoHz = patch.vibratoHz;
        v.vibratoDepth = patch.vibratoDepth + 0.45f * mod;
        v.bend = bend;
        v.glide = patch.glide > 0 ? glideFor(patch.glide, rate) : 0.0f;
    }
    std::array<dsp::FmVoice, maxVoices> voice;
    std::array<HeldNote, maxVoices> held {};
};

//==============================================================================
// A wordless choir: held notes on formant voices that follow the chords.
struct ChoirPatch {
    float vowel = 2, vowelDrift = 0.4f, attack = 1.2f, release = 1.5f, detune = 9, vibratoDepth = 0.12f, brightness = 0.4f, glide = 0.6f;
    float gain = 1, pan = 0, spread = 0.6f, transpose = 0;
    // Babble: each voice's vowel and level chatter at about this many syllables a second
    // (a crowd's murmur, a swarm), `chatter` deep (0: a steady choir).
    float babble = 0, chatter = 0;
    int voices = 4;
};

class ChoirPart final : public Part {
public:
    static constexpr int maxVoices = 6;
    ChoirPatch patch;
    void prepare(float r) override {
        rate = r;
        reset();
    }
    void reset() override {
        for (auto& v : voice) v.reset();
        for (auto& h : held) h = {};
        claimed.fill(-1.0e9);
    }
    bool sustains() const override { return true; }
    float transposeSemis() const override { return patch.transpose; }
    void stopLive(int id) override { endLive(held, id); }
    void start(const PartFrame& f, const NoteOn& n) override {
        const int limit = std::clamp(patch.voices, 1, maxVoices);
        // A voice whose note ends here sings on into the new one.
        int k = -1;
        float best = 1.0e9f;
        for (int i = 0; i < limit; ++i) {
            const auto& h = held[std::size_t(i)];
            if (!voice[std::size_t(i)].active() || h.until > n.beat + 1.0e-6 || h.until < n.beat - 0.07 || claimed[std::size_t(i)] == n.beat) continue;
            if (std::abs(h.semis - n.semis) < best) {
                best = std::abs(h.semis - n.semis);
                k = i;
            }
        }
        const bool legato = k >= 0;
        if (k < 0) k = int(dsp::pickVoice(voice, limit, [](const dsp::ChoirVoice& v) { return v.level(); }));
        auto& v = voice[std::size_t(k)];
        v.velocity = n.velocity;
        v.start(music::noteHz(f.root, n.semis + patch.transpose), patch.attack, rate, legato);
        v.gain = n.velocity;
        if (!legato) v.pan = dsp::panGains(n.hasPan ? n.pan : spreadPan(patch.pan, patch.spread, n.random));
        v.vibratoHz = 4.6f + 1.2f * n.random2;
        velocity[std::size_t(k)] = n.velocity;
        held[std::size_t(k)].set(n);
        claimed[std::size_t(k)] = n.beat;
        apply(v, k, f);
        v.update(rate);
    }
    void update(const PartFrame& f) override {
        time = f.beat / std::max(f.beatsPerSecond, 0.1);
        for (int i = 0; i < maxVoices; ++i) {
            auto& v = voice[std::size_t(i)];
            auto& h = held[std::size_t(i)];
            if (h.on && f.beat >= h.until) {
                v.release(patch.release * weight::release, rate);
                h.on = false;
            }
            if (!v.active()) continue;
            if (h.on && h.follow) v.targetHz = music::noteHz(f.root, h.now(f) + patch.transpose);
            apply(v, i, f);
            v.update(rate);
        }
    }
    void render(float* left, float* right, int n) override {
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0;
            for (auto& v : voice) {
                if (!v.active()) continue;
                const float y = v.tick();
                l += y * v.pan.l;
                r += y * v.pan.r;
            }
            left[i] += l * patch.gain;
            right[i] += r * patch.gain;
        }
    }

private:
    void apply(dsp::ChoirVoice& v, int i, const PartFrame&) {
        v.glide = glideFor(patch.glide, rate);
        v.detuneCents = patch.detune;
        v.vibratoDepth = patch.vibratoDepth + 0.45f * mod;
        v.bend = bend;
        v.brightness = patch.brightness;
        // Each voice drifts slowly around the vowel, so the choir breathes; babbling
        // voices move through syllables, louder and softer.
        const double drift = patch.babble > 0.0f ? double(patch.babble) : 0.09;
        v.vowel = patch.vowel + patch.vowelDrift * noise.smooth(time + 31.0 * i, drift);
        v.gain = velocity[std::size_t(i)];
        if (patch.chatter > 0.0f)
            v.gain *= 1.0f - patch.chatter * dsp::clamp01(0.5f + 0.8f * noise.smooth(time * 1.37 + 57.0 * i, double(patch.babble) * 1.6));
    }
    std::array<dsp::ChoirVoice, maxVoices> voice;
    std::array<HeldNote, maxVoices> held {};
    std::array<float, maxVoices> velocity {};
    std::array<double, maxVoices> claimed {};
    double time = 0;
};

//==============================================================================
// Wind instruments: flutes, recorders, ney, whistles. A note lasts its length.
struct WindPatch {
    float h2 = 0.15f, h3 = 0.08f, breath = 0.25f, attack = 0.05f, release = 0.12f, scoop = 0.6f;
    float tone = 1, breathQ = 3, scoopSeconds = 0.06f;   // tone 0: a tuned breath alone (rustle, gusts)
    float vibratoHz = 5, vibratoDepth = 0.15f, vibratoDelay = 0.25f;
    float gain = 1, pan = 0, spread = 0.3f, transpose = 0, minSeconds = 0.08f;
    int voices = 2;
    bool held = false;   // long notes it picks up again when it missed their start
    // The upper harmonics, the attack's chiff, broad air noise and the pitch's wander (cents).
    float h4 = 0, h5 = 0, chiff = 0.25f, chiffSeconds = 0.035f, air = 0.08f, wander = 3;
};

class WindPart final : public Part {
public:
    static constexpr int maxVoices = 4;
    WindPatch patch;
    void prepare(float r) override {
        rate = r;
        reset();
    }
    void reset() override {
        for (auto& v : voice) v.reset();
        for (auto& h : held) h = {};
        noiseAt = 0;
    }
    bool sustains() const override { return patch.held; }
    float transposeSemis() const override { return patch.transpose; }
    void stopLive(int id) override { endLive(held, id); }
    void start(const PartFrame& f, const NoteOn& n) override {
        const int limit = std::clamp(patch.voices, 1, maxVoices);
        // A monophonic line: the voice still sounding slurs on to the next note.
        int k = -1;
        for (int i = 0; i < limit && k < 0; ++i)
            if (held[std::size_t(i)].on && held[std::size_t(i)].until <= n.beat + 1.0e-6) k = i;
        if (k < 0) k = int(dsp::pickVoice(voice, limit, [](const dsp::WindVoice& v) { return v.level(); }));
        auto& v = voice[std::size_t(k)];
        apply(v);
        v.velocity = n.velocity;
        v.start(music::noteHz(f.root, n.semis + patch.transpose), patch.attack, patch.scoop, rate, noiseAt);
        noiseAt += 1ull << 24;
        v.gain = n.velocity;
        v.pan = dsp::panGains(n.hasPan ? n.pan : spreadPan(patch.pan, patch.spread, n.random));
        held[std::size_t(k)].set(n);
        // Legato: each note lasts its whole length (the next one slurs on from it).
        held[std::size_t(k)].until = n.beat + std::max(n.length, double(patch.minSeconds) * f.beatsPerSecond);
        v.update(rate);
    }
    void update(const PartFrame& f) override {
        for (int i = 0; i < maxVoices; ++i) {
            auto& v = voice[std::size_t(i)];
            auto& h = held[std::size_t(i)];
            if (h.on && f.beat >= h.until) {
                v.release(patch.release * weight::release, rate);
                h.on = false;
            }
            if (!v.active()) continue;
            apply(v);
            v.update(rate);
        }
    }
    void render(float* left, float* right, int n) override {
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0;
            for (auto& v : voice) {
                if (!v.active()) continue;
                const float y = v.tick(noise);
                l += y * v.pan.l;
                r += y * v.pan.r;
            }
            left[i] += l * patch.gain;
            right[i] += r * patch.gain;
        }
    }

private:
    void apply(dsp::WindVoice& v) const {
        v.h2 = patch.h2;
        v.h3 = patch.h3;
        v.breath = patch.breath;
        v.tone = patch.tone;
        v.breathQ = patch.breathQ;
        v.scoopSeconds = patch.scoopSeconds;
        v.vibratoHz = patch.vibratoHz;
        v.vibratoDepth = patch.vibratoDepth + 0.45f * mod;
        v.vibratoDelay = mod > 0.01f ? 0.0f : patch.vibratoDelay;
        v.bend = bend;
        v.h4 = patch.h4;
        v.h5 = patch.h5;
        // Breath alone (rustles, gusts) has no jet to chiff or wander.
        v.chiff = patch.tone > 0.0f ? patch.chiff : 0.0f;
        v.chiffSeconds = patch.chiffSeconds;
        v.air = patch.tone > 0.0f ? patch.air : 0.0f;
        v.wander = patch.tone > 0.0f ? patch.wander : 0.0f;
    }
    std::array<dsp::WindVoice, maxVoices> voice;
    std::array<HeldNote, maxVoices> held {};
};

//==============================================================================
// Bowed instruments. Model Resonator: a bow driving a few tuned modes (bowed metal,
// singing bowls, glassy hums, a ship's hull). Model String: a violin, viola, cello or
// double bass (one player or a section) through the violin family's body, scaled to
// its size. Held notes follow the chords while they sound (like the synth drones).
enum BowedModel { BowedResonator, BowedString };
struct BowedPatch {
    std::array<float, 4> ratios { 1, 2, 3, 4 }, decays { 1, 1, 1, 1 }, gains { 1, 0.5f, 0.3f, 0.2f };
    int modes = 4;
    float ring = 0.4f, rosin = 0.5f, noisiness = 0.3f, attack = 0.4f, release = 1.0f, glide = 0.3f;
    float vibratoHz = 5, vibratoDepth = 0, gain = 1, pan = 0, spread = 0.5f, transpose = 0;
    int voices = 4;
    bool held = true;   // notes last their length (else the bow lifts at once and the modes ring)
    // Model String.
    int model = BowedResonator, size = dsp::Violin, players = 1;
    float position = 0.12f;      // bow point, fraction of the string from the bridge (sul tasto ~0.2, ponticello ~0.04)
    float brightness = 0.5f;     // bow pressure and speed
    float spreadCents = 8;       // a section's detune
    float bodyAmount = 1;
    float tremoloPerBeat = 0, tremoloDepth = 0;   // bowed tremolo, in time
    void set(std::initializer_list<float> r, std::initializer_list<float> d, std::initializer_list<float> g) {
        modes = int(std::min<std::size_t>(r.size(), 4));
        std::copy_n(r.begin(), modes, ratios.begin());
        std::copy_n(d.begin(), std::min<std::size_t>(d.size(), 4), decays.begin());
        std::copy_n(g.begin(), std::min<std::size_t>(g.size(), 4), gains.begin());
    }
};

class BowedPart final : public Part {
public:
    static constexpr int maxVoices = 6;
    BowedPatch patch;
    void prepare(float r) override {
        rate = r;
        for (auto& v : strings) v.prepare(rate, 30.0f);
        reset();
    }
    void reset() override {
        for (auto& v : voice) v.reset();
        for (auto& v : strings) v.reset();
        for (auto& h : held) h = {};
        for (auto& side : body)
            for (auto& f : side) f.reset();
        noiseAt = 0;
        bodyFor = -1;
    }
    bool sustains() const override { return patch.held; }
    float transposeSemis() const override { return patch.transpose; }
    void stopLive(int id) override { endLive(held, id); }
    void start(const PartFrame& f, const NoteOn& n) override {
        const int limit = std::clamp(patch.voices, 1, maxVoices);
        const bool string = patch.model == BowedString;
        // A voice whose note ends here bows on into the new one.
        int k = -1;
        for (int i = 0; i < limit && k < 0; ++i)
            if (held[std::size_t(i)].on && held[std::size_t(i)].until <= n.beat + 1.0e-6 && held[std::size_t(i)].until >= n.beat - 0.07) k = i;
        const bool legato = k >= 0;
        if (k < 0)
            k = string ? int(dsp::pickVoice(strings, limit, [](const dsp::StringVoice& v) { return v.level(); }))
                       : int(dsp::pickVoice(voice, limit, [](const dsp::BowedVoice& v) { return v.level(); }));
        const float hz = music::noteHz(f.root, n.semis + patch.transpose);
        const float pan = n.hasPan ? n.pan : spreadPan(patch.pan, patch.spread, n.random);
        if (string) {
            auto& v = strings[std::size_t(k)];
            apply(v);
            // A harder stroke speaks sooner.
            v.start(hz, patch.attack * dsp::lerp(1.3f, 0.6f, n.velocity), n.velocity, n.random2, noiseAt, legato);
            v.gain = n.velocity;
            if (!legato) v.pan = dsp::panGains(pan);
            v.update();
        } else {
            auto& v = voice[std::size_t(k)];
            apply(v);
            v.start(hz, patch.attack, rate, noiseAt, legato);
            v.gainLevel = n.velocity;
            if (!legato) v.pan = dsp::panGains(pan);
            v.update(rate);
        }
        noiseAt += 1ull << 24;
        held[std::size_t(k)].set(n);
        if (!patch.held && n.live == 0) held[std::size_t(k)].until = n.beat;   // the bow lifts at once
    }
    void update(const PartFrame& f) override {
        const bool string = patch.model == BowedString;
        trem = 1.0f - patch.tremoloDepth * (0.5f + 0.5f * std::cos(float(f.beat * double(patch.tremoloPerBeat)) * dsp::twoPi));
        if (string && patch.size != bodyFor) {
            bodyFor = std::clamp(patch.size, 0, dsp::StringSizeCount - 1);
            const float scale = dsp::stringBodyScale[std::size_t(bodyFor)];
            for (auto& side : body)
                for (std::size_t m = 0; m < side.size(); ++m)
                    side[m].set(dsp::violinBody[m].hz * (m == 4 ? std::sqrt(scale) : scale), dsp::violinBody[m].q, rate);
        }
        for (int i = 0; i < maxVoices; ++i) {
            auto& h = held[std::size_t(i)];
            const bool end = h.on && f.beat >= h.until;
            if (string) {
                auto& v = strings[std::size_t(i)];
                if (end) {
                    v.release(patch.release * weight::release);
                    h.on = false;
                }
                if (!v.active()) continue;
                if (h.on && h.follow) v.targetHz = music::noteHz(f.root, h.now(f) + patch.transpose);
                apply(v);
                v.update();
            } else {
                auto& v = voice[std::size_t(i)];
                if (end) {
                    v.release(patch.release * weight::release, rate);
                    h.on = false;
                }
                if (!v.active()) continue;
                if (h.on && h.follow) v.targetHz = music::noteHz(f.root, h.now(f) + patch.transpose);
                apply(v);
                v.update(rate);
            }
        }
    }
    void render(float* left, float* right, int n) override {
        if (patch.model == BowedString) {
            const float amount = dsp::clamp01(patch.bodyAmount);
            for (int i = 0; i < n; ++i) {
                float l = 0, r = 0;
                for (auto& v : strings) {
                    if (!v.active()) continue;
                    const float y = v.tick(noise);
                    l += y * v.pan.l;
                    r += y * v.pan.r;
                }
                // The body: its modes on top of what passes straight through.
                float bl = l * (1.0f - 0.55f * amount), br = r * (1.0f - 0.55f * amount);
                for (std::size_t m = 0; m < body[0].size(); ++m) {
                    bl += amount * dsp::violinBody[m].gain * body[0][m].bandpass(l);
                    br += amount * dsp::violinBody[m].gain * body[1][m].bandpass(r);
                }
                left[i] += bl * patch.gain * trem;
                right[i] += br * patch.gain * trem;
            }
            return;
        }
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0;
            for (auto& v : voice) {
                if (!v.active()) continue;
                const float y = v.tick(noise);
                l += y * v.pan.l;
                r += y * v.pan.r;
            }
            left[i] += l * patch.gain;
            right[i] += r * patch.gain;
        }
    }

private:
    void apply(dsp::BowedVoice& v) const {
        v.modes = patch.modes;
        if (v.ratio != patch.ratios || v.decay != patch.decays || v.gain != patch.gains || std::abs(v.ringSeconds - patch.ring) > 1.0e-4f) v.tunedHz = 0;   // retune
        v.ratio = patch.ratios;
        v.decay = patch.decays;
        v.gain = patch.gains;
        v.ringSeconds = patch.ring;
        v.rosin = patch.rosin;
        v.noisiness = patch.noisiness;
        v.vibratoHz = patch.vibratoHz;
        v.vibratoDepth = patch.vibratoDepth + 0.45f * mod;
        v.bend = bend;
        v.glide = glideFor(patch.glide, rate);
    }
    void apply(dsp::StringVoice& v) const {
        v.players = std::clamp(patch.players, 1, dsp::StringVoice::maxPlayers);
        v.position = std::clamp(patch.position, 0.02f, 0.4f);
        v.brightness = dsp::clamp01(patch.brightness);
        v.bowNoise = patch.noisiness;
        v.vibratoDepth = patch.vibratoDepth + 0.45f * mod;
        v.vibratoDelay = mod > 0.01f ? 0.0f : 0.25f;
        v.bend = bend;
        v.spreadCents = patch.spreadCents;
        v.glide = glideFor(patch.glide, rate);
    }
    std::array<dsp::BowedVoice, maxVoices> voice;
    std::array<dsp::StringVoice, maxVoices> strings;
    std::array<HeldNote, maxVoices> held {};
    std::array<std::array<dsp::Svf, 5>, 2> body;
    int bodyFor = -1;
    float trem = 1;
};

//==============================================================================
// Water drops: a short "bloop" that rises into the note, then a sine ringing on the
// note itself (with a fainter one an octave and a fifth above). Cave drips, water-like
// plucks. Most of the energy sits on the note, so drips stay in the key.
struct DripPatch {
    float decay = 0.12f, rise = 4, below = 2, bloop = 0.6f, overtone = 0.25f, gain = 1, pan = 0, spread = 0.8f, transpose = 0;
    int voices = 6;
};

class DripPart final : public Part {
public:
    static constexpr int maxVoices = 8;
    DripPatch patch;
    void prepare(float r) override {
        rate = r;
        reset();
    }
    void reset() override {
        for (auto& p : bloop) p.stop();
        for (auto& p : drop) p.stop();
        for (auto& p : ring) p.stop();
    }
    float transposeSemis() const override { return patch.transpose; }
    void start(const PartFrame& f, const NoteOn& n) override {
        const auto k = dsp::pickVoice(drop, std::clamp(patch.voices, 1, maxVoices), [](const dsp::Ping& p) { return p.amp; });
        const float hz = music::noteHz(f.root, n.semis + patch.transpose);
        const float decay = patch.decay * n.decay * (0.8f + 0.4f * n.random2), pan = n.hasPan ? n.pan : spreadPan(patch.pan, patch.spread, n.random);
        const float polarity = n.random2 < 0.5f ? -1.0f : 1.0f;
        // The bloop starts `below` the note and rises `rise` semitones within a fifth of
        // the decay; the note fades in under it and rings on.
        bloop[k].start(hz * std::exp2(-patch.below / 12.0f), decay * 0.2f, std::exp2(patch.rise / 12.0f), polarity * n.velocity * patch.bloop, pan, rate, 0.0005f);
        drop[k].start(hz, decay, 1.0f, polarity * n.velocity, pan, rate, 0.004f);
        ring[k].start(hz * 3.0f, decay * 0.5f, 1.0f, polarity * n.velocity * patch.overtone, pan, rate, 0.004f);
    }
    void update(const PartFrame&) override {
        for (auto* pings : { &bloop, &drop, &ring })
            for (auto& p : *pings)
                if (p.active()) p.update(rate);
    }
    void render(float* left, float* right, int n) override {
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0;
            for (std::size_t k = 0; k < drop.size(); ++k) {
                float y = 0;
                if (bloop[k].active()) y += bloop[k].tick();
                if (drop[k].active()) y += drop[k].tick();
                if (ring[k].active()) y += ring[k].tick();
                l += y * drop[k].pan.l;
                r += y * drop[k].pan.r;
            }
            left[i] += l * patch.gain;
            right[i] += r * patch.gain;
        }
    }

private:
    std::array<dsp::Ping, maxVoices> bloop, drop, ring;
};

//==============================================================================
// Noise sweeps: resonant noise bands moving through the spectrum over a note's
// length (pass-bys, launch builds, falling sweeps, swells, gusts), tuned to the key.
enum SweepShape { SweepPassBy, SweepBuild, SweepFall, SweepSwell };
struct SweepRow {
    int shape = SweepPassBy;
    float lowSemis = -12, highSemis = 48, q = 3, level = 1;
    bool impact = false;   // a tuned low impact when the sweep ends
    bool moves = true;     // pans across the stereo field
};
struct SweepPatch {
    std::array<SweepRow, 4> rows {};
    float gain = 1, tension = 0;
    int voices = 1;
};

class NoisePart final : public Part {
public:
    static constexpr int maxVoices = 2;
    SweepPatch patch;
    void prepare(float r) override {
        rate = r;
        for (auto& v : voice) v.gain.setTime(0.02f, rate);
        reset();
    }
    void reset() override {
        for (auto& v : voice) {
            v.active = false;
            v.gain.snap(0);
            // As built, coefficients too: a sweep's first chunk plays before update()
            // tunes its bands, so stale ones would make a restart differ from the
            // first run.
            for (auto& b : v.band) b = dsp::Svf {};
            v.target = 0;
            v.pan = 0;
        }
        impact.reset();
        noiseAt = 0;
        index = 0;
    }
    void start(const PartFrame&, const NoteOn& n) override {
        const int limit = std::clamp(patch.voices, 1, maxVoices);
        int k = -1;
        for (int i = 0; i < limit && k < 0; ++i)
            if (!voice[std::size_t(i)].active) k = i;
        if (k < 0) return;   // all busy: a sweep is never cut short
        auto& v = voice[std::size_t(k)];
        v.active = true;
        v.row = std::clamp(n.row, 0, 3);
        v.start = n.beat;
        v.length = n.live != 0 ? 4.0 : std::max(0.5, n.length);   // a live sweep: one bar
        v.size = (0.7f + 0.3f * n.random) * (0.4f + 0.6f * std::min(n.velocity / 0.8f, 1.25f));
        v.panFrom = n.random2 < 0.5f ? -0.8f : 0.8f;
        v.panTo = -v.panFrom;
    }
    void update(const PartFrame& f) override {
        for (auto& v : voice) {
            v.target = 0;
            if (!v.active) continue;
            const auto& row = patch.rows[std::size_t(v.row)];
            const float x = float((f.beat - v.start) / v.length);
            float sweep = x, env = 0;
            switch (row.shape) {
                case SweepPassBy: env = std::pow(std::sin(dsp::pi * dsp::clamp01(x)), 2.0f); sweep = 0.25f + 0.5f * x; break;
                case SweepBuild: env = x * x * x; sweep = x; break;
                case SweepFall: env = std::pow(std::sin(dsp::pi * dsp::clamp01(x)), 2.0f) * 0.7f; sweep = 1.0f - x; break;
                default: env = std::sin(dsp::pi * dsp::clamp01(x)); sweep = 0.4f + 0.2f * std::sin(dsp::twoPi * x); break;
            }
            const float hz = music::noteHz(f.root, dsp::lerp(row.lowSemis, row.highSemis, dsp::clamp01(sweep)));
            v.pan = row.moves ? (row.shape == SweepPassBy ? dsp::lerp(v.panFrom, v.panTo, dsp::clamp01(x)) : 0.3f * std::sin(x * 6.0f)) : 0.0f;
            v.target = env * v.size * row.level;
            for (auto& b : v.band) b.set(hz, row.q * dsp::lerp(1.0f, 1.7f, patch.tension), rate);
            if (x >= 1.0f) {
                v.active = false;
                if (row.impact) {
                    impact.ratio = 1.5f;
                    impact.partial = 0.4f;
                    impact.start(music::noteHz(f.root, -24.0f), 1.5f, 6.0f, 0.8f, 0.5f, 300.0f, 0.6f, 1.5f, 0.0f, rate, noiseAt);
                    noiseAt += 1ull << 24;
                }
            }
        }
        if (impact.active()) impact.update(rate);
    }
    void render(float* left, float* right, int n) override {
        const float g = patch.gain;
        for (auto& v : voice) {
            if (!v.active && v.gain.value < 1.0e-5f) continue;
            const auto pan = dsp::panGains(v.pan);
            for (int i = 0; i < n; ++i) {
                const float a = v.gain.next(v.target);
                const uint64_t at = index + uint64_t(i);
                left[i] += v.band[0].bandpass(noise.white(at)) * a * 3.0f * pan.l * g;
                right[i] += v.band[1].bandpass(noise.white(at + (1ull << 44))) * a * 3.0f * pan.r * g;
            }
        }
        if (impact.active())
            for (int i = 0; i < n; ++i) {
                const float y = impact.tick(noise) * 0.16f * g;
                left[i] += y;
                right[i] += y;
            }
        index += uint64_t(n);
    }

private:
    struct Voice {
        bool active = false;
        int row = 0;
        double start = 0, length = 8;
        float size = 1, panFrom = -0.8f, panTo = 0.8f, pan = 0, target = 0;
        std::array<dsp::Svf, 2> band;
        dsp::Smoother gain;
    };
    std::array<Voice, maxVoices> voice;
    dsp::Drum impact;
    uint64_t index = 0;
};

//==============================================================================
// A Place made of parts. Subclasses choose the parts, set their patches in
// configure(), may add their own notes or shape each note, and mix the buses
// through their effects.
class EnsemblePlace : public Place {
public:
    void prepare(double sampleRate) override;
    void reset() override;
    void setSeed(uint32_t key) override;
    void render(const PlaceContext& c, float* left, float* right, int n) override;
    float partTranspose(int e) const override {
        return e >= 0 && e < placeElements && parts[std::size_t(e)] != nullptr ? parts[std::size_t(e)]->transposeSemis() : 0.0f;
    }

protected:
    explicit EnsemblePlace(uint32_t saltValue) : salt(saltValue) {}
    // Called from the subclass constructor.
    void setParts(std::array<Part*, placeElements> p) { parts = p; }

    virtual void prepareFx() {}
    virtual void resetFx() {}
    // Once per chunk, before anything plays: set the patches, the levels and the effects.
    virtual void configure(const PlaceContext& c) = 0;
    // Adds the Place's own notes (with a tag) to this chunk; returns the new count.
    virtual int addNotes(const PlaceContext&, int, std::array<Scheduled, maxScheduled>&, int count) { return count; }
    // Adjusts a note before its part starts it. Returning false drops it.
    virtual bool shape(const PlaceContext&, const Scheduled&, NoteOn&) { return true; }
    // Mixes the buses (already at each element's level) through the effects into
    // left and right (writes n samples).
    virtual void mix(const PlaceContext& c, float* left, float* right, int n) = 0;

    std::array<Part*, placeElements> parts {};
    // Each element's output level, set in configure() (the slot's element level is applied on top).
    std::array<float, placeElements> level {};
    // How much of each element reaches its reverb and delay sends (1: as the instrument's
    // effect controls say), set in configure() when a Character or setting wants more
    // space or less.
    std::array<float, placeElements> sendScale { 1, 1, 1, 1, 1, 1 };
    std::array<std::array<float, dsp::chunk>, placeElements> busL {}, busR {};
    float rate = 48000;
    PartFrame frame;
    dsp::Stream noise, drift;

private:
    NoteOn noteFor(const PlaceContext& c, const Scheduled& x) const;
    uint32_t salt = 0;
    std::array<bool, placeElements> catchUp {}, silent {};
};

} // namespace wb
