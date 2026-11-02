#pragma once
// The developer's voice (Milestone 17): a sampler of sung vowels, choir stacks, drones
// and whispers, shaped per Place.
//
// A note sings one vowel (or between two, morphing as the vowel drifts) from the vowel
// sweeps, soft and full takes crossfaded by velocity, its sustain looped and its
// recorded tail played when it ends. An ensemble adds more singers from the choir stacks
// (five or six takes of a note, sung one at a time): each another take, a few cents off,
// a little late, somewhere else in the stereo field, from one voice up to a full choir.
// Above the highest recorded note the voice is shifted up a little (pitch and formants
// together) and the synthesised choir takes over over a fifth.
//
// Each Place shapes it its own way (a filter, a band-pass and grit for a radio, ring
// modulation and drift for something alien, a fast flutter for insects, transposition),
// so no two uses sound alike. Whisper and breath recordings play as textures: unpitched,
// from a different point each note, several murmuring at once.
//
// The part it replaced stays behind it: it plays only when the bank has no recordings.
#include "Ensemble.h"
#include "../Samples/SampleBank.h"

namespace wb {

// A part that plays nothing (a voice layer added under another part has no fallback of
// its own: without recordings the layer is silent and the part above it plays alone).
class NullPart final : public Part {
public:
    void prepare(float r) override { rate = r; }
    void reset() override {}
    void start(const PartFrame&, const NoteOn&) override {}
    void update(const PartFrame&) override {}
    void render(float*, float*, int) override {}
};

// Two parts playing every note together: a part and a layer under it.
class LayerPart final : public Part {
public:
    LayerPart(Part& first, Part& second) : a(first), b(second) {}
    float levelB = 1;   // the layer's level against the part
    void prepare(float r) override {
        rate = r;
        a.prepare(r);
        b.prepare(r);
    }
    void reset() override {
        a.reset();
        b.reset();
    }
    void setKey(uint32_t key, uint32_t salt) override {
        a.setKey(key, salt);
        b.setKey(key, salt + 7);
    }
    void start(const PartFrame& f, const NoteOn& n) override {
        a.start(f, n);
        if (levelB > 0.0f) b.start(f, n);
    }
    void update(const PartFrame& f) override {
        for (auto* p : { &a, &b }) {
            p->bend = bend;
            p->mod = mod;
            p->instrumentMode = instrumentMode;
            p->drone = drone;
            p->slide = slide;
            p->update(f);
        }
    }
    void render(float* left, float* right, int n) override {
        a.render(left, right, n);
        if (levelB <= 0.0f) return;
        std::array<float, dsp::chunk> l {}, r {};
        const int m = std::min(n, dsp::chunk);
        b.render(l.data(), r.data(), m);
        for (int i = 0; i < m; ++i) {
            left[i] += l[std::size_t(i)] * levelB;
            right[i] += r[std::size_t(i)] * levelB;
        }
    }
    bool sustains() const override { return a.sustains() || b.sustains(); }
    float transposeSemis() const override { return a.transposeSemis(); }
    void stopLive(int id) override {
        a.stopLive(id);
        b.stopLive(id);
    }
    void shapeForInstrument(float x, float y) override {
        a.shapeForInstrument(x, y);
        b.shapeForInstrument(x, y);
    }
    void unshapeForInstrument() override {
        a.unshapeForInstrument();
        b.unshapeForInstrument();
    }
    SlideDesign designed() const override { return a.designed(); }

private:
    Part& a;
    Part& b;
};

// The recordings a voice sings: the bank's ids (tools/sample-prep/manifest.json).
namespace voice {
// The vowels in the choir's order (0 "ooh", 1 "oh", 2 "aah", 3 "ee"); "eh" lies between
// "aah" and "ee".
inline constexpr std::array<const char*, 4> vowels { "voice-ooh", "voice-oh", "voice-aah", "voice-ee" };
inline constexpr const char* hum = "voice-mm";
inline constexpr const char* stackOoh = "voice-choir-ooh";
inline constexpr const char* stackAah = "voice-choir-aah";
inline constexpr const char* stackMm = "voice-choir-mm";
// The synthesised choir's vowel (0 oo, 1 oh, 2 ah, 3 eh, 4 ee) on the recorded scale.
inline float recordedVowel(float synthVowel) {
    const float v = std::clamp(synthVowel, 0.0f, 4.0f);
    return v <= 2.0f ? v : 2.0f + 0.5f * (v - 2.0f);
}
} // namespace voice

struct VoicePatch {
    // What it sings: the vowels (morphing from `vowel`, drifting by `vowelDrift`) with
    // `hum` of the hummed "mm" blended in; or one recording set (`single`: a drone, the
    // chant, the grit, the ghostly falsetto, whispers, breaths).
    const char* single = nullptr;
    float vowel = 2, vowelDrift = 0.3f, driftHz = 0.08f, hum = 0;
    // The ensemble: how many singers (1: a solo voice, up to 8), the choir stacks' takes
    // for the extra ones; how far apart they are in pitch (cents), time (seconds) and space.
    int singers = 1;
    float detune = 8, timing = 0.05f, spread = 0.6f;
    bool stacks = true;          // the extra singers from the choir stacks (else other takes of the vowels)
    float gain = 1, pan = 0, transpose = 0;
    int voices = 4;
    bool held = true;            // notes last their length (loops), else one-shots
    float attack = 0.08f, release = 0.6f, glide = 0.4f;
    float velocityGain = 0.4f, even = 0.5f;
    // Tone: a low-pass (darker when soft) and a high-pass.
    float toneHz = 12000, velocityTone = 0.6f, lowCutHz = 40;
    // Per-Place colour.
    float bandHz = 0, bandQ = 1.2f, bandMix = 1;   // a band-pass (a radio, a comm channel)
    float bandFollow = 0;     // > 0: the band-pass sits on this harmonic of the note instead (a murmur resonating on the key)
    float drive = 0;                              // grit (tanh, level kept)
    float ringHz = 0, ringRatio = 0, ringMix = 0; // ring modulation: a fixed rate, or a ratio to the note
    float tremoloHz = 0, tremoloDepth = 0;        // a flutter (insect wings)
    float wobbleCents = 0, wobbleHz = 0.15f;      // a slow, uneven pitch drift
    float vibratoHz = 5, vibratoDepth = 0;        // added vibrato (the takes have their own)
    float chatter = 0, chatterHz = 3;             // level chatter, syllable-like (murmurs)
    float startJitter = 0;   // textures: each note starts somewhere else in the take (0..1 of it)
    int take = -1;           // a texture's take (-1: chosen per note)
    float shiftLimit = 5;    // above the top recording: shifted up at most this far before the synth takes over
    float synthOver = 7;     // ... over this many semitones
};

class VoicePart final : public Part {
public:
    static constexpr int maxVoices = 6, maxReaders = 12;
    VoicePatch patch;
    // `fallback`: plays when there are no recordings. `synth`: a synthesised choir blended
    // in above the recorded range (may be the fallback; nullptr: none).
    VoicePart(Part& fallbackPart, ChoirPart* synthChoir) : fallback(fallbackPart), synth(synthChoir) {}

    bool sampled() const { return main[0] != nullptr || single != nullptr; }

    void shapeForInstrument(float a, float r) override {
        designedPatch = patch;
        shaped = true;
        fallback.shapeForInstrument(a, r);
        if (synth != nullptr && synth != &fallback) synth->shapeForInstrument(a, r);
        patch.attack = patch.attack * timeScale(a) + swell(a);
        patch.release = std::min(patch.release, instrumentRelease) * timeScale(r);
        patch.voices = maxVoices;
        patch.chatter *= 0.5f;
    }
    void unshapeForInstrument() override {
        fallback.unshapeForInstrument();
        if (synth != nullptr && synth != &fallback) synth->unshapeForInstrument();
        if (shaped) patch = designedPatch;
        shaped = false;
    }

    void prepare(float r) override {
        rate = r;
        fallback.prepare(r);
        if (synth != nullptr && synth != &fallback) synth->prepare(r);
        for (auto& f : dc) f.setCutoff(20.0f, rate);
        resolve();
        reset();
    }
    void reset() override {
        fallback.reset();
        if (synth != nullptr && synth != &fallback) synth->reset();
        for (auto& v : voice) v = {};
        for (auto& h : held) h = {};
        for (auto& f : dc) f.reset();
        scoops.reset();
        gainNow = -1;
        time = 0;
    }
    void setKey(uint32_t key, uint32_t salt) override {
        Part::setKey(key, salt);
        fallback.setKey(key, salt);
        if (synth != nullptr && synth != &fallback) synth->setKey(key, salt + 3);
    }
    bool sustains() const override { return sampled() ? patch.held : fallback.sustains(); }
    float transposeSemis() const override { return sampled() ? patch.transpose : fallback.transposeSemis(); }
    void stopLive(int id) override {
        if (!sampled()) {
            fallback.stopLive(id);
            return;
        }
        if (synth != nullptr) synth->stopLive(id);
        endLive(held, id);
    }
    SlideDesign designed() const override {
        // Textures (whispers) follow no pitch: their slide stays the part's they replaced.
        if (!sampled() || (single != nullptr && single->unpitched)) return fallback.designed();
        return { 0.0f, 0.0f, patch.glide };
    }

    void start(const PartFrame& f, const NoteOn& n) override {
        if (!sampled()) {
            fallback.slide = slide;
            fallback.start(f, n);
            return;
        }
        const float hz = music::noteHz(f.root, n.semis + patch.transpose);
        const float midi = 69.0f + 12.0f * std::log2(std::max(hz, 1.0f) / 440.0f);
        // Above the recordings: the synthesised choir takes over.
        float synthW = 0;
        if (single == nullptr && synth != nullptr && midi > topRoot + 0.5f)
            synthW = dsp::clamp01((midi - topRoot) / std::max(patch.synthOver, 1.0f));
        if (synthW > 0.0f) {
            NoteOn s = n;
            s.velocity = n.velocity * std::sin(0.5f * dsp::pi * synthW);
            synth->slide = slide;
            synth->start(f, s);
        }
        if (synthW >= 0.999f) return;
        const int limit = std::clamp(patch.voices, 1, maxVoices);
        // A note whose predecessor ends here is sung on from it (legato).
        int k = -1;
        if (patch.held && n.live == 0)
            for (int i = 0; i < limit; ++i) {
                const auto& h = held[std::size_t(i)];
                if (voice[std::size_t(i)].on && h.on && h.until <= n.beat + 1.0e-6 && h.until >= n.beat - 0.07) {
                    held[std::size_t(i)].on = false;
                    letGo(voice[std::size_t(i)], 0.12f);
                }
            }
        k = int(dsp::pickVoice(voice, limit, [](const Voice& v) { return v.on ? v.level + 1.0e-9f : 0.0f; }));
        auto& v = voice[std::size_t(k)];
        v = {};
        v.on = true;
        v.midi = v.target = midi;
        v.velocity = n.velocity;
        v.weight = std::cos(0.5f * dsp::pi * synthW);
        v.amp = patch.attack > 0.0005f ? 0.0f : 1.0f;
        v.attackStep = patch.attack > 0.0005f ? 1.0f / (patch.attack * rate) : 1.0f;
        v.age = 0;
        v.seed = n.random * 997.0f + n.random2 * 61.0f;
        v.gain = (1.0f - patch.velocityGain + patch.velocityGain * std::pow(dsp::clamp01(n.velocity), 1.2f)) * v.weight;
        const float cutoff = patch.toneHz * std::exp2(-patch.velocityTone * (1.0f - dsp::clamp01(n.velocity))) * std::exp2(2.0f * mod);
        for (int c = 0; c < 2; ++c) {
            v.lp[std::size_t(c)].set(std::min(cutoff, 0.45f * rate), 0.6f, rate);
            v.hp[std::size_t(c)].set(std::max(patch.lowCutHz, 10.0f), 0.6f, rate);
            const float bandHz = patch.bandFollow > 0.0f ? hz * patch.bandFollow : patch.bandHz;
            if (bandHz > 0.0f) v.band[std::size_t(c)].set(std::clamp(bandHz, 60.0f, 0.4f * rate), patch.bandQ, rate);
        }
        v.ringPhase = 0;
        v.ringInc = (patch.ringRatio > 0.0f ? patch.ringRatio * hz : patch.ringHz) / rate;
        v.count = 0;
        if (single != nullptr) startSingle(v, n, midi);
        else startVowels(v, n, midi);
        if (v.count == 0) {
            v.on = false;
            return;
        }
        held[std::size_t(k)].set(n);
        if (!patch.held && n.live == 0) held[std::size_t(k)].on = false;
        // Scoop / Glide: a plain scoop from below into the note, as the choir has.
        scoops.start(std::size_t(k), slide, 0.0f, rate);
        v.scoop = scoops.now[std::size_t(k)];
        retune(v, 0.0f);
    }

    void update(const PartFrame& f) override {
        // The recordings: once the bank is ready, and again whenever the Place names other
        // ones (it sets the patch in configure(), after prepare()).
        if ((!resolved || patch.single != resolvedSingle) && samples::Bank::get().ready()) resolve();
        if (!sampled()) {
            fallback.bend = bend;
            fallback.mod = mod;
            fallback.instrumentMode = instrumentMode;
            fallback.drone = drone;
            fallback.slide = slide;
            fallback.update(f);
            return;
        }
        if (synth != nullptr) {
            synth->bend = bend;
            synth->mod = mod;
            synth->instrumentMode = instrumentMode;
            synth->slide = slide;
            synth->update(f);
        }
        const float dt = float(dsp::chunk) / rate;
        time += double(dt);
        const float glide = glideFor(slide.glide(patch.glide), rate);
        for (std::size_t k = 0; k < voice.size(); ++k) {
            auto& v = voice[k];
            auto& h = held[k];
            if (!v.on) continue;
            if (h.on && f.beat >= h.until) {
                h.on = false;
                letGo(v, -1.0f);
            }
            if (h.on && h.follow) {
                const float hz = music::noteHz(f.root, h.now(f) + patch.transpose);
                v.target = 69.0f + 12.0f * std::log2(hz / 440.0f);
            }
            v.midi = v.target + (v.midi - v.target) * glide;
            scoops.step(k);
            v.scoop = scoops.now[k];
            v.age += dt;
            // The vowel drifting between its two recordings (each voice its own way).
            if (v.morph) {
                const float d = patch.vowelDrift * noise.smooth(time + double(v.seed), double(patch.driftHz));
                const float t = dsp::clamp01(v.vowelT + d);
                for (int i = 0; i < v.count; ++i) {
                    auto& r = v.reader[std::size_t(i)];
                    if (r.role == 1) r.gain = r.base * std::cos(0.5f * dsp::pi * t);
                    else if (r.role == 2) r.gain = r.base * std::sin(0.5f * dsp::pi * t);
                }
            }
            if (patch.chatter > 0.0f)
                v.chatterGain = 1.0f - patch.chatter * dsp::clamp01(0.5f + 0.9f * noise.smooth(time * 1.37 + double(v.seed) * 3.0, double(patch.chatterHz)));
            else
                v.chatterGain = 1.0f;
            retune(v, dt);
        }
    }

    void render(float* left, float* right, int n) override {
        if (!sampled()) {
            fallback.render(left, right, n);
            return;
        }
        if (synth != nullptr) synth->render(left, right, n);
        // The level glides (20 ms) to a new gain (a Character's own), never steps.
        if (gainNow < 0.0f) gainNow = patch.gain;
        const float follow = 1.0f - std::exp(-1.0f / (0.02f * rate));
        // Each voice is rendered over the whole chunk (its readers one after another, then
        // its colour), not sample by sample across every voice: the same sums in the same
        // order (the output is bit for bit what it was), in tight loops.
        for (int at = 0; at < n; at += dsp::chunk) {
            const int m = std::min(dsp::chunk, n - at);
            std::array<float, dsp::chunk> sumL {}, sumR {};
            for (auto& v : voice)
                if (v.on) renderVoice(v, sumL.data(), sumR.data(), m);
            for (int i = 0; i < m; ++i) {
                gainNow += (patch.gain - gainNow) * follow;
                left[at + i] += dc[0].hp(sumL[std::size_t(i)]) * gainNow;
                right[at + i] += dc[1].hp(sumR[std::size_t(i)]) * gainNow;
            }
        }
    }

    // How many notes sound (tests), and the highest recorded pitch of the vowels.
    int sounding() const {
        int c = 0;
        for (const auto& v : voice) c += v.on ? 1 : 0;
        return c;
    }
    float top() const { return topRoot; }

private:
    struct Reader {
        const samples::Zone* zone = nullptr;
        double pos = 0, tailPos = -1;
        float inc = 1, rateRatio = 1, cents = 0, gain = 1, base = 1, tailMix = 0;
        dsp::PanGains pan { 0.7071f, 0.7071f };
        int delay = 0;
        int role = 0;          // 1: the vowel morph's first vowel, 2: its second, 0: fixed weight
        bool done = false;
        bool linear = false;   // an ensemble singer: linear interpolation (half the cost; in the background)
    };
    struct Voice {
        bool on = false, releasing = false, morph = false;
        std::array<Reader, maxReaders> reader {};
        int count = 0;
        float midi = 60, target = 60, velocity = 1, weight = 1, gain = 1, level = 0;
        float amp = 1, attackStep = 1, fade = 1, releaseMul = 1, tailStep = 0, age = 0, seed = 0;
        float vowelT = 0, chatterGain = 1, ringPhase = 0, ringInc = 0, vibratoPhase = 0, scoop = 0;
        std::array<dsp::Svf, 2> lp, hp, band;
    };

    static float readLinear(const int16_t* d, double pos) {
        const auto i = int(pos);
        const float t = float(pos - double(i));
        return (float(d[i]) + t * float(d[i + 1] - d[i])) * (1.0f / 32768.0f);
    }
    static float read(const int16_t* d, double pos) {
        const auto i = int(pos);
        const float t = float(pos - double(i));
        const float xm1 = d[i - 1], x0 = d[i], x1 = d[i + 1], x2 = d[i + 2];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return (((c3 * t + c2) * t + c1) * t + x0) * (1.0f / 32768.0f);
    }

    // The level a zone is evened toward (its instrument layer's median).
    float evenGain(const samples::Instrument& inst, const samples::Zone& z) const {
        return dsp::dbToGain(std::clamp(patch.even * (inst.medianDb(z.layer) - z.loudnessDb), -12.0f, 12.0f)) * dsp::dbToGain(z.gainDb);
    }

    void addReader(Voice& v, const samples::Instrument& inst, const samples::Zone* z, float gain, int role, float cents, float delaySeconds, float pan,
                   double startPos = -1.0) {
        if (z == nullptr || v.count >= maxReaders || gain <= 1.0e-4f) return;
        auto& r = v.reader[std::size_t(v.count++)];
        r = {};
        r.zone = z;
        r.rateRatio = inst.rate / rate;
        r.base = r.gain = gain * evenGain(inst, *z);
        r.role = role;
        r.cents = cents;
        r.delay = int(std::max(0.0f, delaySeconds) * rate);
        r.pan = dsp::panGains(std::clamp(pan, -1.0f, 1.0f));
        r.pos = startPos >= 0.0 ? startPos : 0.0;
        if (z->loops() && r.pos >= double(z->loopEnd)) r.pos = double(z->loopStart);
    }

    // The layers of one vowel recording around this velocity (`pair`: crossfaded; else the
    // nearest layer alone, for the morph's second vowel and the hum: half the cost).
    void addVowel(Voice& v, const samples::Instrument* inst, int key, float velocity, float pick, float gain, int role, float pan, bool pair = true) {
        if (inst == nullptr || gain <= 1.0e-4f) return;
        if (!pair) {
            addReader(v, *inst, inst->find(key, velocity, pick), gain, role, 0, 0, pan);
            return;
        }
        const auto p = inst->findPair(key, velocity, pick);
        if (p.a == nullptr) return;
        if (p.b != nullptr && p.mix > 0.001f) {
            // (addReader adds each zone's gainDb itself.)
            float ga = 1, gb = 0;
            samples::crossfadeGains(*p.a, *p.b, p.mix, ga, gb);
            addReader(v, *inst, p.a, gain * ga * dsp::dbToGain(-p.a->gainDb), role, 0, 0, pan);
            addReader(v, *inst, p.b, gain * gb * dsp::dbToGain(-p.b->gainDb), role, 0, 0, pan);
        } else {
            addReader(v, *inst, p.a, gain, role, 0, 0, pan);
        }
    }

    void startVowels(Voice& v, const NoteOn& n, float midi) {
        const int key = int(std::lround(std::min(midi, topRoot + patch.shiftLimit)));
        const float pan = n.hasPan ? n.pan : spreadPan(patch.pan, 0.3f * patch.spread, n.random);
        // The vowel: two recordings and where between them.
        const float vw = std::clamp(patch.vowel, 0.0f, 3.0f);
        const int i = std::min(int(vw), 2);
        const float t = vw - float(i);
        const float hum = dsp::clamp01(patch.hum);
        const float voiced = std::sqrt(1.0f - hum);
        v.vowelT = t;
        v.morph = patch.vowelDrift > 0.0f || (t > 0.001f && t < 0.999f);
        addVowel(v, main[std::size_t(i)], key, n.velocity, n.random2, voiced, 1, pan, t <= 0.5f);
        if (v.morph || t > 0.001f) addVowel(v, main[std::size_t(i + 1)], key, n.velocity, n.random2, voiced, 2, pan, t > 0.5f);
        // The first vowel at (1 - t), the second at t (equal power); update() moves t as
        // the vowel drifts.
        for (int r = 0; r < v.count; ++r) {
            auto& rd = v.reader[std::size_t(r)];
            rd.gain = rd.base * (rd.role == 1 ? std::cos(0.5f * dsp::pi * t) : std::sin(0.5f * dsp::pi * t));
        }
        if (hum > 0.0f && humInst != nullptr) addVowel(v, humInst, key, n.velocity, n.random2, std::sqrt(hum), 0, pan, false);
        // The ensemble: each extra singer another take (a choir stack's where one lies
        // within a fifth, else another take of the vowel), a few cents off, a little late,
        // elsewhere in the field. The section's level grows with it, about as a choir's does.
        const int singers = std::clamp(patch.singers, 1, 6);   // (a seventh and eighth add cost, not size)
        if (singers > 1) {
            const samples::Instrument* stack = !patch.stacks ? nullptr : (hum > 0.5f ? stackMm : (vw < 1.5f ? stackOoh : stackAah));
            const samples::Instrument* alt = main[std::size_t(t < 0.5f ? i : i + 1)];
            const float each = 0.75f / std::sqrt(float(singers - 1));
            for (int s = 1; s < singers; ++s) {
                const float u = float(s) / float(singers - 1);              // 0..1 across the section
                const float side = (s % 2 == 0 ? 1.0f : -1.0f) * (0.35f + 0.65f * u);
                const float jitter = std::fmod(v.seed * 0.618f + 0.37f * float(s), 1.0f);
                const float cents = patch.detune * (2.0f * std::fmod(0.31f + 0.618f * float(s) + 0.2f * n.random, 1.0f) - 1.0f);
                const float pick = std::fmod(n.random2 + 0.618f * float(s), 1.0f);
                const samples::Zone* z = nullptr;
                const samples::Instrument* from = nullptr;
                if (stack != nullptr) {
                    z = stack->find(key, n.velocity, pick);
                    from = stack;
                    if (z != nullptr && std::abs(z->root - float(key)) > 7.5f) z = nullptr;
                }
                if (z == nullptr && alt != nullptr) {
                    // Another layer or take of the vowel than the lead's.
                    z = alt->find(key, std::fmod(n.velocity + 0.5f * float(s), 1.0f), pick);
                    from = alt;
                }
                if (z == nullptr) continue;
                addReader(v, *from, z, voiced * each, 0, cents, patch.timing * jitter, std::clamp(pan + patch.spread * side, -1.0f, 1.0f));
                if (v.count > 0) v.reader[std::size_t(v.count - 1)].linear = true;
            }
        }
    }

    void startSingle(Voice& v, const NoteOn& n, float midi) {
        const auto* inst = single;
        const int key = int(std::lround(midi));
        const bool texture = inst->unpitched;
        const int singers = std::clamp(patch.singers, 1, 8);
        const float pan = n.hasPan ? n.pan : spreadPan(patch.pan, 0.3f * patch.spread, n.random);
        for (int s = 0; s < singers; ++s) {
            const float u = singers > 1 ? float(s) / float(singers - 1) : 0.5f;
            const float pick = patch.take >= 0 ? (float(patch.take) + 0.5f) / 3.0f : std::fmod(n.random2 + 0.618f * float(s), 1.0f);
            const auto* z = inst->find(key, n.velocity, pick);
            if (z == nullptr) continue;
            double startPos = -1.0;
            if (texture && patch.startJitter > 0.0f) {
                // Somewhere in the take (within its loop, so it murmurs on).
                const double span = z->loops() ? double(z->loopEnd - z->loopStart) : double(z->length) * 0.5;
                startPos = (z->loops() ? double(z->loopStart) : 0.0) + span * double(patch.startJitter) * double(std::fmod(n.random * 3.7f + 0.37f * float(s), 1.0f));
            }
            const float side = singers > 1 ? (2.0f * u - 1.0f) : 0.0f;
            const float cents = singers > 1 ? patch.detune * (2.0f * std::fmod(0.21f + 0.618f * float(s), 1.0f) - 1.0f) : 0.0f;
            const float delay = s == 0 ? 0.0f : patch.timing * std::fmod(0.43f + 0.618f * float(s) + n.random, 1.0f);
            addReader(v, *inst, z, (s == 0 ? 1.0f : 0.8f) / std::sqrt(float(singers)) * 1.3f, 0, cents, delay,
                      std::clamp(pan + patch.spread * side, -1.0f, 1.0f), startPos);
        }
    }

    void retune(Voice& v, float dt) {
        float semis = v.midi + bend - v.scoop;
        if (patch.wobbleCents > 0.0f) semis += 0.01f * patch.wobbleCents * noise.smooth(time * 0.7 + double(v.seed) * 5.0, double(patch.wobbleHz));
        if (patch.vibratoDepth > 0.0f || mod > 0.01f) {
            v.vibratoPhase += patch.vibratoHz * dt;
            v.vibratoPhase -= std::floor(v.vibratoPhase);
            semis += (patch.vibratoDepth + 0.35f * mod) * dsp::clamp01((v.age - 0.3f) / 0.5f) * std::sin(dsp::twoPi * v.vibratoPhase);
        }
        const bool texture = single != nullptr && single->unpitched;
        for (int i = 0; i < v.count; ++i) {
            auto& r = v.reader[std::size_t(i)];
            // Textures play at their own pitch, moved only by the transposition.
            const float s = texture ? patch.transpose + bend - v.scoop : semis - r.zone->root;
            r.inc = r.rateRatio * std::exp2((s + 0.01f * r.cents) / 12.0f);
        }
    }

    void letGo(Voice& v, float seconds) {
        if (v.releasing) return;
        v.releasing = true;
        const float s = seconds > 0.0f ? seconds : patch.release * weight::release;
        bool tail = false;
        if (seconds <= 0.0f)
            for (int i = 0; i < v.count; ++i) {
                auto& r = v.reader[std::size_t(i)];
                if (r.zone->release > 0 && r.pos < double(r.zone->release)) {
                    r.tailPos = double(r.zone->release);
                    tail = true;
                }
            }
        v.tailStep = 1.0f / (0.06f * rate);
        v.releaseMul = std::exp(-6.9078f / (std::max(tail ? s * 1.5f : s, 0.02f) * rate));
    }

    // One reader over `m` samples, summed into the voice's buffers; how many of them it was
    // still sounding at their start (a voice ends once all its readers have ended).
    static int renderReader(Reader& rd, float tailStep, float* vl, float* vr, int m) {
        const auto& z = *rd.zone;
        int i = 0;
        for (; i < m; ++i) {
            if (rd.done) break;
            if (rd.delay > 0) {
                --rd.delay;
                continue;
            }
            // A layer the vowel morph has all but faded out moves on unread.
            const bool heard = std::abs(rd.gain) > 1.0e-3f;
            float y = 0;
            if (heard) y = rd.linear ? readLinear(z.data, std::clamp(rd.pos, 1.0, double(z.length - 1))) : read(z.data, std::clamp(rd.pos, 1.0, double(z.length - 1)));
            if (rd.tailPos >= 0.0) {
                rd.tailMix = std::min(1.0f, rd.tailMix + tailStep);
                if (heard) y = y * (1.0f - rd.tailMix) + read(z.data, std::min(rd.tailPos, double(z.length - 1))) * rd.tailMix;
                rd.tailPos += double(rd.inc);
                if (rd.tailMix >= 1.0f) rd.pos = rd.tailPos, rd.tailPos = -1;
            }
            rd.pos += double(rd.inc);
            if (z.loops() && rd.tailPos < 0.0 && rd.tailMix <= 0.0f && rd.pos >= double(z.loopEnd)) rd.pos -= double(z.loopEnd - z.loopStart);
            if (rd.pos >= double(z.length - 1) || rd.tailPos >= double(z.length - 1)) rd.done = true;
            y *= rd.gain;
            vl[i] += y * rd.pan.l;
            vr[i] += y * rd.pan.r;
        }
        return i;
    }

    // One voice over `m` samples (m <= chunk), added into the part's sums.
    void renderVoice(Voice& v, float* outL, float* outR, int m) {
        std::array<float, dsp::chunk> vl {}, vr {};
        int sounding = 0;
        for (int k = 0; k < v.count; ++k) sounding = std::max(sounding, renderReader(v.reader[std::size_t(k)], v.tailStep, vl.data(), vr.data(), m));
        // The Place's colour, per side (the patch's settings, read once).
        const bool lowCut = patch.lowCutHz > 45.0f;   // (below that the part's DC filter is enough)
        const bool band = patch.bandHz > 0.0f || patch.bandFollow > 0.0f;
        const float bandMix = patch.bandMix;
        const bool drive = patch.drive > 0.0f;
        const float driveK = 1.0f + 12.0f * patch.drive;
        const float driveNorm = drive ? std::tanh(driveK) : 1.0f;
        const float driveLevel = 1.0f / (1.0f + 0.5f * patch.drive);
        const bool ring = patch.ringMix > 0.0f;
        const float ringMix = patch.ringMix;
        const bool tremolo = patch.tremoloDepth > 0.0f;
        for (int i = 0; i < m; ++i) {
            if (i >= sounding) {
                v.on = false;
                return;
            }
            float side[2] { vl[std::size_t(i)], vr[std::size_t(i)] };
            for (int c = 0; c < 2; ++c) {
                float y = lowCut ? v.hp[std::size_t(c)].tick(side[c]).hp : side[c];
                y = v.lp[std::size_t(c)].tick(y).lp;
                if (band) y = dsp::lerp(y, 1.6f * v.band[std::size_t(c)].bandpass(y), bandMix);
                if (drive) y = std::tanh(driveK * y) / driveNorm * driveLevel;
                side[c] = y;
            }
            float g = v.amp * v.fade * v.gain * v.chatterGain;
            if (ring) {
                v.ringPhase += v.ringInc;
                v.ringPhase -= std::floor(v.ringPhase);
                const float rm = std::sin(dsp::twoPi * v.ringPhase);
                for (auto& s : side) s = s * (1.0f - ringMix) + s * rm * ringMix * 1.4f;
            }
            if (tremolo) g *= 1.0f - patch.tremoloDepth * (0.5f + 0.5f * std::sin(dsp::twoPi * float(time) * patch.tremoloHz + v.seed));
            if (v.amp < 1.0f) v.amp = std::min(1.0f, v.amp + v.attackStep);
            v.fade *= v.releaseMul;
            v.level = v.amp * v.fade * v.gain;
            outL[i] += side[0] * g;
            outR[i] += side[1] * g;
            if (v.fade < 1.0e-4f) {
                v.on = false;
                return;
            }
        }
    }

    void resolve() {
        auto& bank = samples::Bank::get();
        if (!bank.ready()) return;
        resolved = true;
        resolvedSingle = patch.single;
        single = nullptr;
        main.fill(nullptr);
        if (patch.single != nullptr) {
            single = bank.instrument(patch.single);
            topRoot = single != nullptr ? single->highRoot : 127.0f;
            return;
        }
        for (std::size_t i = 0; i < main.size(); ++i) main[i] = bank.instrument(voice::vowels[i]);
        if (main[0] == nullptr || main[1] == nullptr || main[2] == nullptr || main[3] == nullptr) {
            main.fill(nullptr);
            return;
        }
        humInst = bank.instrument(voice::hum);
        stackOoh = bank.instrument(voice::stackOoh);
        stackAah = bank.instrument(voice::stackAah);
        stackMm = bank.instrument(voice::stackMm);
        // The top of the recordings: the highest note the vowels have (the falsetto's).
        topRoot = 0;
        for (const auto* m : main) topRoot = std::max(topRoot, m->highRoot);
    }

    Part& fallback;
    ChoirPart* synth = nullptr;
    VoicePatch designedPatch {};
    bool shaped = false;
    std::array<const samples::Instrument*, 4> main {};
    const samples::Instrument *humInst = nullptr, *stackOoh = nullptr, *stackAah = nullptr, *stackMm = nullptr, *single = nullptr;
    float topRoot = 127;
    bool resolved = false;
    const char* resolvedSingle = nullptr;
    std::array<Voice, maxVoices> voice {};
    std::array<HeldNote, maxVoices> held {};
    Scoops<maxVoices> scoops;
    std::array<dsp::OnePole, 2> dc;
    float gainNow = -1;
    double time = 0;
};

// A voice patch from a synthesised choir's: the same vowel (on the recorded scale), swell,
// release, glide, place in the field; a Place then gives it its own colour.
inline VoicePatch voiceFromChoir(const ChoirPatch& c) {
    VoicePatch v;
    v.vowel = voice::recordedVowel(c.vowel);
    v.vowelDrift = std::clamp(0.35f * c.vowelDrift, 0.0f, 0.6f);
    v.attack = c.attack;
    v.release = c.release;
    v.glide = c.glide;
    v.spread = c.spread;
    v.pan = c.pan;
    v.transpose = c.transpose;
    v.detune = c.detune;
    v.voices = c.voices;   // as many notes at once as the choir (its level depends on it)
    // The synthesised choir's level follows velocity all the way (a soft note is quiet):
    // so does the voice, with the soft and full takes' own tone on top.
    v.velocityGain = 0.9f;
    return v;
}

} // namespace wb
