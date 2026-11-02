#pragma once
// Recorded hand drums (Milestone 17): a kit whose rows play the developer's djembe strokes.
//
// Each row of a Place's kit names a stroke (bass, tone, slap, muted bass or muted slap, of
// the large or the small drum) and how the Place wants it: transposed, darker, shorter,
// panned. A hit picks the stroke's velocity layer (soft, medium, hard: crossfaded
// between them) and one of its three takes (a round-robin per hit, from the hit's own
// random number, so playback stays deterministic). The kit's patterns are the Place's
// own: only the sound changes.
//
// A row with no stroke (a shaker, a rattle) plays on the synthesised kit the part
// replaced, which also plays everything when the bank has no recordings.
#include "Ensemble.h"
#include "../Samples/SampleBank.h"

namespace wb {

// The strokes' keys in the "djembe" instrument (tools/sample-prep/manifest.json).
namespace djembe {
enum Stroke : int {
    BigBass = 36, BigTone = 38, BigSlap = 40, BigMutedBass = 41, BigMutedSlap = 43,
    SmallBass = 48, SmallTone = 50, SmallSlap = 52, SmallMutedBass = 53, SmallMutedSlap = 55
};
}

struct HitRow {
    int stroke = -1;          // the stroke's key (-1: the synthesised row plays)
    int alt = -1;             // a second stroke, played instead on some hits (`altChance`)
    float altChance = 0;
    float semis = 0;          // transposition (tape-speed: pitch and length together)
    float gain = 1, pan = 0;
    float toneHz = 18000;     // a low-pass at full velocity (darker when softer)
    float velocityTone = 0.5f;
    float lowCutHz = 30;
    float decay = 0;          // > 0: the hit dies away faster (seconds to fall 60 dB more)
    float startSeconds = 0;   // skipped at the start (a softer attack)
    // A pitch drop after the strike (as Scoop / Glide allows), semitones; by default the
    // synthesised drum's own (the row it replaces), so Scoop / Glide acts as it did.
    static constexpr float asDrum = 1.0e9f;
    float bendSemis = asDrum;
};

struct HitPatch {
    const char* instrument = "djembe";
    std::array<HitRow, 4> rows {};
    float gain = 1, spread = 0.2f;
    float velocityGain = 0.35f;   // how much velocity scales the level (the layers do the rest)
    float even = 0.4f;            // evens the takes' loudness within a layer
    float drive = 0;              // a little grit (a cramped room, a tape)
    float velocityScale = 1;      // played harder or softer (into the louder or softer layers)
    int voices = 8;
};

class HitPart final : public Part {
public:
    static constexpr int maxVoices = 10;
    HitPatch patch;
    explicit HitPart(DrumPart& synthFallback) : fallback(synthFallback) {}

    bool sampled() const { return kit != nullptr; }

    void shapeForInstrument(float a, float r) override {
        designedPatch = patch;
        shaped = true;
        fallback.shapeForInstrument(a, r);
        // Attack: a harder hand (brighter, sharper); Release: how long the skins ring.
        for (auto& row : patch.rows) {
            row.toneHz *= std::exp2(1.5f * std::clamp(a, -1.0f, 1.0f));
            if (r < 0.0f) row.decay = (row.decay > 0.0f ? row.decay : 1.2f) * timeScale(r);
        }
        patch.voices = maxVoices;
    }
    void unshapeForInstrument() override {
        fallback.unshapeForInstrument();
        if (shaped) patch = designedPatch;
        shaped = false;
    }
    void prepare(float r) override {
        rate = r;
        fallback.prepare(r);
        for (auto& f : dc) f.setCutoff(20.0f, rate);
        resolve();
        reset();
    }
    void reset() override {
        fallback.reset();
        for (auto& v : voice) v = {};
        for (auto& f : dc) f.reset();
        gainNow = -1;
    }
    void setKey(uint32_t key, uint32_t salt) override {
        Part::setKey(key, salt);
        fallback.setKey(key, salt);
    }
    SlideDesign designed() const override {
        if (!sampled()) return fallback.designed();
        // As the synthesised kit reports it (its drums' deepest bend), with the sampled
        // rows' own bends where they set one.
        float deepest = 0;
        for (std::size_t r = 0; r < patch.rows.size(); ++r) {
            const float b = bendOf(r);
            if (std::abs(b) > std::abs(deepest)) deepest = b;
        }
        return { deepest, deepest != 0.0f ? dsp::Drum::pitchSeconds : 0.0f, 0.0f };
    }

    void start(const PartFrame& f, const NoteOn& n) override {
        const auto& row = patch.rows[std::size_t(std::clamp(n.row, 0, 3))];
        if (!sampled() || row.stroke < 0) {
            fallback.slide = slide;
            fallback.start(f, n);
            return;
        }
        const int stroke = (row.alt >= 0 && n.random < row.altChance) ? row.alt : row.stroke;
        const float velocity = dsp::clamp01(n.velocity * patch.velocityScale);
        const auto pair = kit->crossfade ? kit->findPair(stroke, velocity, n.random2) : samples::ZonePair { kit->find(stroke, velocity, n.random2), nullptr, 0.0f };
        if (pair.a == nullptr) return;
        auto& v = voice[dsp::pickVoice(voice, std::clamp(patch.voices, 1, maxVoices), [](const Voice& x) { return x.on ? x.level + 1.0e-9f : 0.0f; })];
        v = {};
        v.on = true;
        v.a = pair.a;
        v.b = pair.b != nullptr && pair.mix > 0.001f ? pair.b : nullptr;
        const auto even = [&](const samples::Zone& z) {
            return dsp::dbToGain(std::clamp(patch.even * (kit->medianDb(z.layer) - z.loudnessDb), -9.0f, 9.0f));
        };
        v.gainA = even(*v.a);
        v.gainB = 0;
        if (v.b != nullptr) {
            float ga = 1, gb = 0;
            samples::crossfadeGains(*v.a, *v.b, pair.mix, ga, gb);
            // Evened toward the layers' medians as each layer is.
            v.gainA = ga * even(*v.a);
            v.gainB = gb * even(*v.b);
        }
        v.gain = row.gain * (1.0f - patch.velocityGain + patch.velocityGain * std::pow(velocity, 1.2f));
        v.pos = double(std::max(0.0f, row.startSeconds) * kit->rate);
        // A bend from above after the strike, as Scoop / Glide says.
        const float designedBend = bendOf(std::size_t(std::clamp(n.row, 0, 3)));
        const float bend = slide.depth(designedBend) + slide.plain(designedBend);
        v.bend = bend;
        v.bendMul = bend != 0.0f ? std::exp(-1.0f / (std::max(slide.time(dsp::Drum::pitchSeconds), 0.002f) * rate)) : 0.0f;
        v.bendMul32 = std::pow(v.bendMul, 32.0f);   // (the bend moves every 32 samples)
        v.semis = row.semis;
        v.ratio = kit->rate / rate;
        v.fadeMul = row.decay > 0.0f ? std::exp(-6.9078f / (row.decay * weight::skin * n.decay * rate)) : 1.0f;
        const float cutoff = row.toneHz * std::exp2(-row.velocityTone * (1.0f - velocity)) * std::exp2(1.5f * mod);
        v.lp.set(std::min(cutoff, 0.45f * rate), 0.6f, rate);
        v.hp.set(std::max(row.lowCutHz, 10.0f), 0.6f, rate);
        v.pan = dsp::panGains(n.hasPan ? n.pan : spreadPan(row.pan, patch.spread, n.random));
        v.wait = 0;
    }
    void update(const PartFrame& f) override {
        if (kit == nullptr && samples::Bank::get().ready()) resolve();
        fallback.bend = bend;
        fallback.mod = mod;
        fallback.instrumentMode = instrumentMode;
        fallback.slide = slide;
        fallback.update(f);
    }
    void render(float* left, float* right, int n) override {
        fallback.render(left, right, n);
        if (!sampled()) return;
        if (gainNow < 0.0f) gainNow = patch.gain;
        const float follow = 1.0f - std::exp(-1.0f / (0.02f * rate));
        // The hits one after another over each chunk (the same sums in the same order as
        // sample by sample across them: bit for bit the same output, in tight loops).
        std::array<float, dsp::chunk> sumL {}, sumR {};
        for (int i = 0; i < n; ++i) {
            if (i % dsp::chunk == 0) {
                const int m = std::min(dsp::chunk, n - i);
                std::fill(sumL.begin(), sumL.end(), 0.0f);
                std::fill(sumR.begin(), sumR.end(), 0.0f);
                for (auto& v : voice)
                    for (int k = 0; k < m && v.on; ++k) {
                        const float y = tick(v);
                        sumL[std::size_t(k)] += y * v.pan.l;
                        sumR[std::size_t(k)] += y * v.pan.r;
                    }
            }
            gainNow += (patch.gain - gainNow) * follow;   // a new gain glides in over 20 ms
            float l = sumL[std::size_t(i % dsp::chunk)], r = sumR[std::size_t(i % dsp::chunk)];
            if (patch.drive > 0.0f) {
                const float k = 1.0f + 8.0f * patch.drive;
                l = std::tanh(k * l) / k * (1.0f + 0.5f * patch.drive);
                r = std::tanh(k * r) / k * (1.0f + 0.5f * patch.drive);
            }
            left[i] += dc[0].hp(l) * gainNow;
            right[i] += dc[1].hp(r) * gainNow;
        }
    }

    // How many sampled hits sound (tests).
    int sounding() const {
        int c = 0;
        for (const auto& v : voice) c += v.on ? 1 : 0;
        return c;
    }

private:
    struct Voice {
        bool on = false;
        const samples::Zone* a = nullptr;
        const samples::Zone* b = nullptr;
        double pos = 0;
        float gainA = 1, gainB = 0, gain = 1, ratio = 1, semis = 0, bend = 0, bendMul = 0, bendMul32 = 0, fade = 1, fadeMul = 1, level = 0;
        int wait = 0, step = 0;
        float inc = 1;
        dsp::Svf lp, hp;
        dsp::PanGains pan { 0.7071f, 0.7071f };
    };

    // A row's designed bend (semitones): its own, or the synthesised drum's it replaces.
    float bendOf(std::size_t r) const {
        const auto& row = patch.rows[r];
        if (row.bendSemis < HitRow::asDrum * 0.5f) return row.bendSemis;
        return 12.0f * std::log2(std::max(fallback.patch.rows[r].bend, 0.01f));
    }

#if defined(__GNUC__) || defined(__clang__)
    [[gnu::always_inline]]
#endif
    static inline float read(const int16_t* d, double pos) {
        const auto i = int(pos);
        const float t = float(pos - double(i));
        const float xm1 = d[i - 1], x0 = d[i], x1 = d[i + 1], x2 = d[i + 2];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return (((c3 * t + c2) * t + c1) * t + x0) * (1.0f / 32768.0f);
    }

#if defined(__GNUC__) || defined(__clang__)
    [[gnu::always_inline]]
#endif
    inline float tick(Voice& v) {
        if ((v.step++ & 31) == 0) {
            v.bend *= v.bendMul32;
            v.inc = v.ratio * std::exp2((v.semis + v.bend + this->bend) / 12.0f);
        }
        float y = read(v.a->data, std::max(v.pos, 1.0)) * v.gainA;
        if (v.b != nullptr && v.pos < double(v.b->length - 1)) y += read(v.b->data, std::max(v.pos, 1.0)) * v.gainB;
        v.pos += double(v.inc);
        v.fade *= v.fadeMul;
        y = v.lp.tick(v.hp.tick(y).hp).lp * v.gain * v.fade;
        v.level = v.fade * v.gain;
        if (v.pos >= double(v.a->length - 1) || v.fade < 1.0e-4f) v.on = false;
        return y;
    }

    void resolve() {
        auto& bank = samples::Bank::get();
        if (!bank.ready() || patch.instrument == nullptr) return;
        kit = bank.instrument(patch.instrument);
    }

    DrumPart& fallback;
    HitPatch designedPatch {};
    bool shaped = false;
    const samples::Instrument* kit = nullptr;
    std::array<Voice, maxVoices> voice {};
    std::array<dsp::OnePole, 2> dc;
    float gainNow = -1;
};

} // namespace wb
