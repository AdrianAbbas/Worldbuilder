#pragma once
// Sampled instruments (Milestone 15): a part that plays the developer's recordings.
//
// A sample-based instrument is just another engine for an element: it gets the same
// patterns, Arrangement, per-instrument effects, Characters and Score Intensity as the
// synthesised parts. Each note picks its recording from the bank (key range, velocity
// layer, a round-robin take chosen per note so playback stays deterministic) and
// pitch-shifts it from the recorded pitch; sustained recordings loop until the note
// ends, then cross into their recorded release tail. The patch reshapes the recording
// to fit the Place (attack, decay, tone, a second string of a course, a body, a slide
// into the note) without replacing what was recorded.
//
// The synthesised part it replaced stays behind it as a fallback: it plays only when the
// bank has no recordings (a build without the samples), so the plugin never goes silent.
#include "Ensemble.h"
#include "../Samples/SampleBank.h"

namespace wb {

struct SamplePatch {
    const char* instrument = nullptr;   // the bank's id (tools/sample-prep/manifest.json)
    // Articulations by written length: notes up to `shortBeats` play `shortInstrument`,
    // notes up to `tinyBeats` play `tinyInstrument` (nullptr: unused).
    const char* shortInstrument = nullptr;
    float shortBeats = 0;
    const char* tinyInstrument = nullptr;
    float tinyBeats = 0;
    float gain = 1, pan = 0, spread = 0.4f, transpose = 0;
    int voices = 6;
    bool held = false;          // notes last their length (sustained recordings loop), else they ring
    float attack = 0;           // a fade-in (seconds) over the recorded attack: swells
    float release = 0.25f;      // a held note's release, a damped string's fade (seconds)
    float decay = 0;            // > 0: the recording dies away faster (seconds to fall 60 dB more)
    float toneHz = 16000;       // the voice's low-pass at full velocity
    float velocityTone = 1.0f;  // octaves darker at the softest velocity
    float velocityGain = 0.5f;  // how much velocity scales the level (the layers do the rest)
    float even = 0.5f;          // evens out the recorded notes' loudness (0: as recorded)
    // A second string of a course (lutes and ouds have paired strings): detuned, a little
    // later, at this level.
    float courseCents = 0, courseLevel = 0, courseMs = 0;
    float scoopSemis = 0, scoopSeconds = 0.05f;   // a slide into the note
    float vibratoHz = 5, vibratoDepth = 0, vibratoDelay = 0.3f;   // semitones
    float glide = 0.05f;        // held notes following the chords
    float tremoloPerBeat = 0, tremoloDepth = 0;
    float strumMs = 0;          // as PluckPatch::strumMs
    int body = dsp::BodyNone;   // an instrument body (dsp::bodies) over the recording
    float bodyAmount = 0;
    float sympathetic = 0;      // open strings (root and fifth) ringing along
    float startSeconds = 0;     // skipped at the start (softens a hard attack)
    // Held notes shorter than this (seconds) start where the recording already speaks
    // (a bow's slow onset skipped, a quick fade in): fast bowing.
    float quickBelow = 0, quickSeconds = 0.12f;
    int lowestKey = 0, highestKey = 127;   // notes outside fold in by octaves
};

class SamplePart final : public Part {
public:
    static constexpr int maxVoices = 8;
    SamplePatch patch;
    explicit SamplePart(Part& synthFallback) : fallback(synthFallback) {}

    // True when the recordings play (false: the fallback does).
    bool sampled() const { return main != nullptr; }

    void prepare(float r) override {
        rate = r;
        fallback.prepare(r);
        for (auto& f : dc) f.setCutoff(20.0f, rate);
        for (auto& o : open) o.prepare(std::size_t(rate / 30.0f) + 8);
        for (auto& f : openDamp) f.setCutoff(2500.0f, rate);
        resolve();
        reset();
    }
    void reset() override {
        fallback.reset();
        for (auto& v : voice) v = {};
        for (auto& h : held) h = {};
        for (auto& f : dc) f.reset();
        for (auto& b : bodyFilter)
            for (auto& f : b) f.reset();
        for (auto& o : open) o.clear();
        for (auto& f : openDamp) f.reset();
        bodyType = -1;
        strumBeat = -1.0e9;
        strumCount = 0;
        trem = 1;
    }
    void setKey(uint32_t key, uint32_t salt) override {
        Part::setKey(key, salt);
        fallback.setKey(key, salt);
    }
    bool sustains() const override { return sampled() ? patch.held : fallback.sustains(); }
    float transposeSemis() const override { return sampled() ? patch.transpose : fallback.transposeSemis(); }
    void stopLive(int id) override {
        if (!sampled()) {
            fallback.stopLive(id);
            return;
        }
        for (std::size_t k = 0; k < voice.size(); ++k)
            if (voice[k].on && held[k].live == id) {
                held[k].until = -1.0e18;
                if (!patch.held) release(voice[k]);
            }
    }

    void start(const PartFrame& f, const NoteOn& n) override {
        if (!sampled()) {
            fallback.start(f, n);
            return;
        }
        const auto* inst = articulation(n);
        const float hz = music::noteHz(f.root, n.semis + patch.transpose);
        float midi = 69.0f + 12.0f * std::log2(std::max(hz, 1.0f) / 440.0f);
        while (midi < float(patch.lowestKey) - 0.5f) midi += 12.0f;
        while (midi > float(patch.highestKey) + 0.5f && midi - 12.0f >= float(patch.lowestKey) - 0.5f) midi -= 12.0f;
        const auto* zone = inst->find(int(std::lround(midi)), n.velocity, n.random2);
        if (zone == nullptr) return;

        const int limit = std::clamp(patch.voices, 1, maxVoices);
        // A held note whose predecessor ends here is played legato: the old note lets go
        // quickly and the new one comes in without its full attack.
        bool legato = false;
        if (patch.held && n.live == 0)
            for (int i = 0; i < limit; ++i) {
                const auto& h = held[std::size_t(i)];
                auto& old = voice[std::size_t(i)];
                if (old.on && h.on && h.until <= n.beat + 1.0e-6 && h.until >= n.beat - 0.07) {
                    held[std::size_t(i)].on = false;
                    release(old, 0.06f);
                    legato = true;
                }
            }
        const int k = int(dsp::pickVoice(voice, limit, [](const Voice& v) { return v.on ? v.amp * v.fade * v.gain + 1.0e-9f : 0.0f; }));
        auto& v = voice[std::size_t(k)];
        int wait = 0;
        float velocity = n.velocity;
        if (patch.strumMs > 0.0f && n.live == 0) {
            if (std::abs(n.beat - strumBeat) > 1.0e-6) {
                strumBeat = n.beat;
                strumFirst = n.semis;
                strumCount = 0;
            }
            const double eighth = n.beat * 2.0;
            const bool onEighth = std::abs(eighth - std::round(eighth)) < 1.0e-4;
            const bool up = !onEighth || (int64_t(std::llround(eighth)) & 1) != 0;
            const float place = up ? std::max(0.0f, strumFirst + 12.0f - n.semis) : std::max(0.0f, n.semis - strumFirst);
            wait = int(patch.strumMs * 0.001f * rate * place / 4.0f);
            velocity *= 1.0f - 0.06f * float(std::min(strumCount, 5));
            ++strumCount;
        }
        v = {};
        v.on = true;
        v.zone = zone;
        v.wait = wait;
        v.midi = v.target = midi;
        v.rateRatio = inst->rate / rate;
        const bool quick = patch.held && n.live == 0 && n.seconds < patch.quickBelow;
        const float skip = quick ? patch.quickSeconds : (legato ? 0.08f : patch.startSeconds);
        v.pos = double(std::min(skip * inst->rate, float(zone->length) * 0.25f));
        if (zone->loops() && v.pos >= double(zone->loopStart)) v.pos = double(zone->loopStart);
        // Level: velocity (the layers already differ), evened toward the instrument's
        // median loudness, the patch.
        const float even = std::clamp(patch.even * (inst->medianRmsDb - zone->rmsDb), -9.0f, 9.0f);
        v.gain = (1.0f - patch.velocityGain + patch.velocityGain * std::pow(dsp::clamp01(velocity), 1.3f)) * dsp::dbToGain(even);
        const float attack = quick ? 0.008f : (legato ? 0.04f : patch.attack);
        v.amp = attack > 0.0005f ? 0.0f : 1.0f;
        v.attackStep = attack > 0.0005f ? 1.0f / (attack * rate) : 1.0f;
        v.fadeMul = patch.decay > 0.0f ? std::exp(-6.9078f / (patch.decay * weight::ring * n.decay * rate)) : 1.0f;
        v.pan = dsp::panGains(n.hasPan ? n.pan : spreadPan(patch.pan, patch.spread, n.random));
        const float cutoff = patch.toneHz * std::exp2(-patch.velocityTone * (1.0f - dsp::clamp01(velocity))) * std::exp2(2.0f * mod);
        v.filter.set(std::min(cutoff, 0.45f * rate), 0.6f, rate);
        v.scoop = patch.scoopSemis;
        v.scoopMul = patch.scoopSemis != 0.0f ? std::exp(-float(dsp::chunk) / (std::max(patch.scoopSeconds, 0.002f) * rate)) : 0.0f;
        v.course = !zone->loops() && patch.courseLevel > 0.0f;
        v.coursePos = v.pos - double(patch.courseMs * 0.001f * inst->rate);
        v.courseRatio = std::exp2(patch.courseCents / 1200.0f);
        v.vibratoAge = 0;
        held[std::size_t(k)].set(n);
        if (!patch.held && n.live == 0) held[std::size_t(k)].on = false;   // it rings
        held[std::size_t(k)].semis = n.semis;
        pitch(v, 0);
    }

    void update(const PartFrame& f) override {
        if (main == nullptr && samples::Bank::get().ready()) resolve();
        if (!sampled()) {
            fallback.bend = bend;
            fallback.mod = mod;
            fallback.update(f);
            return;
        }
        if (patch.body != bodyType) {
            bodyType = std::clamp(patch.body, 0, dsp::BodyCount - 1);
            const auto& b = dsp::bodies[std::size_t(bodyType)];
            for (auto& side : bodyFilter)
                for (std::size_t k = 0; k < side.size(); ++k) side[k].set(b.mode[k].hz, b.mode[k].q, rate);
        }
        if (patch.sympathetic > 0.0f) {
            openPeriod[0] = rate / music::noteHz(f.root, -12.0f);
            openPeriod[1] = rate / music::noteHz(f.root, -12.0f + float(music::degree(f.mood, 4)));
        }
        trem = 1.0f - patch.tremoloDepth * (0.5f + 0.5f * std::cos(float(f.beat * double(patch.tremoloPerBeat)) * dsp::twoPi));
        const float glide = glideFor(patch.glide, rate);
        for (std::size_t k = 0; k < voice.size(); ++k) {
            auto& v = voice[k];
            auto& h = held[k];
            if (!v.on) continue;
            if (h.on && f.beat >= h.until) {
                h.on = false;
                release(v);
            }
            if (h.on && h.follow) {
                const float hz = music::noteHz(f.root, h.now(f) + patch.transpose);
                v.target = 69.0f + 12.0f * std::log2(hz / 440.0f);
            }
            v.midi = v.target + (v.midi - v.target) * glide;
            v.scoop *= v.scoopMul;
            v.vibratoAge += float(dsp::chunk) / rate;
            pitch(v, float(dsp::chunk) / rate);
        }
    }

    void render(float* left, float* right, int n) override {
        if (!sampled()) {
            fallback.render(left, right, n);
            return;
        }
        const auto& b = dsp::bodies[std::size_t(std::max(bodyType, 0))];
        const float amount = dsp::clamp01(patch.bodyAmount);
        const bool body = bodyType > dsp::BodyNone && amount > 0.0f;
        const float direct = dsp::lerp(1.0f, b.direct, amount);
        const float sym = patch.sympathetic;
        const float g = patch.gain * trem;
        for (int i = 0; i < n; ++i) {
            float l = 0, r = 0;
            for (auto& v : voice) {
                if (!v.on) continue;
                const float y = tick(v);
                l += y * v.pan.l;
                r += y * v.pan.r;
            }
            if (body) {
                float bl = l * direct, br = r * direct;
                for (std::size_t k = 0; k < 4; ++k) {
                    bl += b.mode[k].gain * amount * bodyFilter[0][k].bandpass(l);
                    br += b.mode[k].gain * amount * bodyFilter[1][k].bandpass(r);
                }
                l = bl;
                r = br;
            }
            if (sym > 0.0f) {
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
            left[i] += dc[0].hp(l) * g;
            right[i] += dc[1].hp(r) * g;
        }
    }

    // How many voices sound (tests).
    int sounding() const {
        int c = 0;
        for (const auto& v : voice) c += v.on ? 1 : 0;
        return c;
    }

private:
    struct Voice {
        bool on = false, releasing = false, course = false;
        const samples::Zone* zone = nullptr;
        double pos = 0, coursePos = 0, tailPos = -1;
        float rateRatio = 1, inc = 1, courseRatio = 1;
        float midi = 60, target = 60, scoop = 0, scoopMul = 0;
        float amp = 1, attackStep = 1, fade = 1, fadeMul = 1, releaseMul = 1;
        float tailMix = 0, tailStep = 0;
        float gain = 1, vibratoAge = 0, vibratoPhase = 0;
        int wait = 0;
        dsp::PanGains pan { 0.7071f, 0.7071f };
        dsp::Svf filter;
    };

    static float read(const int16_t* d, double pos) {
        const auto i = int(pos);
        const float t = float(pos - double(i));
        const float xm1 = d[i - 1], x0 = d[i], x1 = d[i + 1], x2 = d[i + 2];
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return (((c3 * t + c2) * t + c1) * t + x0) * (1.0f / 32768.0f);
    }

    float tick(Voice& v) {
        if (v.wait > 0) {
            --v.wait;
            return 0.0f;
        }
        const auto& z = *v.zone;
        float y = read(z.data, v.pos);
        if (v.course && v.coursePos >= 0.0) {
            if (v.coursePos < double(z.length - 1)) y += patch.courseLevel * read(z.data, v.coursePos);
        }
        if (v.tailPos >= 0.0) {
            // Crossing into the recorded release tail.
            y = y * (1.0f - v.tailMix) + read(z.data, v.tailPos) * v.tailMix;
            v.tailPos += double(v.inc);
            v.tailMix += v.tailStep;
            if (v.tailMix >= 1.0f) {
                v.pos = v.tailPos;
                v.tailPos = -1;
            }
        }
        v.pos += double(v.inc);
        v.coursePos += double(v.inc * v.courseRatio);
        if (z.loops() && !v.releasing && v.pos >= double(z.loopEnd)) v.pos -= double(z.loopEnd - z.loopStart);
        if (v.tailPos >= double(z.length - 1)) v.tailPos = -1, v.tailMix = 0, v.pos = double(z.length);
        if (v.amp < 1.0f) v.amp = std::min(1.0f, v.amp + v.attackStep);
        v.fade *= v.fadeMul * v.releaseMul;
        const float out = v.filter.tick(y).lp * v.amp * v.fade * v.gain;
        if (v.pos >= double(z.length - 1) || v.fade < 1.0e-4f) v.on = false;
        return out;
    }

    void release(Voice& v, float seconds = -1.0f) {
        if (v.releasing) return;
        v.releasing = true;
        const float s = seconds > 0.0f ? seconds : patch.release * weight::release;
        const auto& z = *v.zone;
        if (seconds <= 0.0f && z.release > 0 && v.pos < double(z.release)) {
            // The bow lifts: the recorded tail, faded in over 40 ms, and a slower release on top.
            v.tailPos = double(z.release);
            v.tailMix = 0;
            v.tailStep = 1.0f / (0.04f * rate);
            v.releaseMul = std::exp(-6.9078f / (std::max(s * 2.0f, 0.05f) * rate));
        } else {
            v.releaseMul = std::exp(-6.9078f / (std::max(s, 0.01f) * rate));
        }
    }

    void pitch(Voice& v, float) {
        float semis = v.midi - v.zone->root + v.scoop + bend;
        if (patch.vibratoDepth > 0.0f || mod > 0.01f) {
            const float delay = mod > 0.01f ? 0.0f : patch.vibratoDelay;
            const float depth = (patch.vibratoDepth + 0.45f * mod) * dsp::clamp01((v.vibratoAge - delay) / 0.4f);
            v.vibratoPhase += patch.vibratoHz * float(dsp::chunk) / rate;
            v.vibratoPhase -= std::floor(v.vibratoPhase);
            semis += depth * std::sin(dsp::twoPi * v.vibratoPhase);
        }
        v.inc = v.rateRatio * std::exp2(semis / 12.0f);
    }

    const samples::Instrument* articulation(const NoteOn& n) const {
        if (tiny != nullptr && n.live == 0 && n.length <= double(patch.tinyBeats) + 1.0e-6) return tiny;
        if (shortOne != nullptr && n.live == 0 && n.length <= double(patch.shortBeats) + 1.0e-6) return shortOne;
        return main;
    }

    // Finds the recordings (once the bank is ready; no allocation).
    void resolve() {
        auto& bank = samples::Bank::get();
        if (!bank.ready() || patch.instrument == nullptr) return;
        main = bank.instrument(patch.instrument);
        shortOne = patch.shortInstrument != nullptr ? bank.instrument(patch.shortInstrument) : nullptr;
        tiny = patch.tinyInstrument != nullptr ? bank.instrument(patch.tinyInstrument) : nullptr;
    }

    Part& fallback;
    const samples::Instrument *main = nullptr, *shortOne = nullptr, *tiny = nullptr;
    std::array<Voice, maxVoices> voice {};
    std::array<HeldNote, maxVoices> held {};
    std::array<dsp::OnePole, 2> dc;
    std::array<std::array<dsp::Svf, 4>, 2> bodyFilter;
    int bodyType = -1;
    std::array<dsp::DelayLine, 2> open;
    std::array<dsp::OnePole, 2> openDamp;
    std::array<float, 2> openPeriod { 400, 300 };
    double strumBeat = -1.0e9;
    float strumFirst = 0, trem = 1;
    int strumCount = 0;
};

} // namespace wb
