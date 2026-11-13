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
    bool recordedTail = true;   // a held note ends in its recorded release (else it fades over `release` on its loop)
    float legatoSeconds = 0.04f;   // a held note handing over to the next: its fade in (the old one's out, 1.5 times)
    float attack = 0;           // a fade-in (seconds) over the recorded attack: swells
    float release = 0.25f;      // a held note's release, a damped string's fade (seconds)
    float decay = 0;            // > 0: the recording dies away faster (seconds to fall 60 dB more)
    float toneHz = 16000;       // the voice's low-pass at full velocity
    float velocityTone = 1.0f;  // octaves darker at the softest velocity
    float velocityGain = 0.5f;  // how much velocity scales the level (the layers do the rest)
    float even = 0.5f;          // evens out the recorded notes' loudness within each layer (0: as recorded)
    // A second string of a course (lutes and ouds have paired strings): detuned, a little
    // later, at this level.
    float courseCents = 0, courseLevel = 0, courseMs = 0;
    // Held (looped) notes too: a second player of a section, playing another recorded take
    // of the note where there is one.
    bool courseLoops = false;
    float scoopSemis = 0, scoopSeconds = 0.05f;   // a slide into the note (as Scoop / Glide allows)
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
    // Recorded short hits (a synth's, with their own release) ring out as recorded even in a
    // held part: a short written note is a short hit, not a long one cut off.
    bool shortRings = false;
    // The voices' low-pass follows toneHz (and the mod wheel) while they sound, not only at
    // the note's start: a filter opening through a build.
    bool liveTone = false;
    // Milestone 17. Velocity layers crossfade (where the recordings' mapping asks for it):
    // a velocity between two layers plays both, weighted.
    bool crossfade = true;
    // A piano: a held key rings with its natural decay (no loop) and its damper stops it
    // when the key (or the pedal) lets go, in the recorded damper time (`damperScale`
    // times it; `release` where a key has none). Keys from `undampedFrom` up have no
    // damper (they ring on, as on a piano's top octaves).
    bool damper = false;
    float damperScale = 1;
    int undampedFrom = 128;
    // Notes played backwards (a piano's decay swelling into its strike): the chance per
    // note, and the longest stretch played (seconds; a note's own length if shorter).
    float reverse = 0, reverseSeconds = 2.0f, reverseShortest = 0.35f;
    // Live and pattern notes alike end at most this long after they start (seconds, 0: no
    // limit): a fragment.
    float fragmentSeconds = 0;
    // Off: new notes play on the synthesised part (a Character that keeps its model, as the
    // Suburbs' sunny electric piano); notes already sounding ring on.
    bool off = false;

    // The woodwinds pass.
    // The mod wheel is expression: it raises each sounding note's dynamic from its velocity
    // toward full (the velocity layers crossfading live, the level and the tone following),
    // instead of opening the filter and adding vibrato.
    bool expression = false;
    // Breath over the recording (the player's air, tuned to the note): its level against the
    // note's, its band (times the note's pitch) and Q, and how much stronger it is in the
    // attack (fading over `breathAttack` seconds). 0: none.
    float breath = 0, breathRatio = 2.0f, breathQ = 1.5f, breathBurst = 2.0f, breathAttack = 0.08f;
    // A soft key click at the start of a tongued (not slurred) note: its level against the note's.
    float keyNoise = 0;
    // > 0: the short articulations play only within this many semitones of the notes they
    // recorded (outside, the long articulation plays the note).
    float shortRange = 0;
    // Live notes (KEYS, Instrument mode) start on the long articulation.
    bool liveLong = false;
    // A second sample set (the developer's own recordings) that plays instead where the bank
    // has it, levelled to the first set (their layers' measured loudness).
    const char* alternate = nullptr;
    const char* alternateShort = nullptr;
    // A formant over the part (a resonance: the wood of a folk flute, a sci-fi sweep):
    // its centre, Q and boost (dB, 0: none). The Place may move it while notes sound.
    float formantHz = 1000, formantQ = 1.0f, formantDb = 0;
    // A bend into each tongued note that is part of the instrument's sound (semitones, - from
    // below; the ney's lean into its notes), whatever Scoop / Glide says; Scoop / Glide on
    // plays the designed scoop instead.
    float bendIn = 0, bendInSeconds = 0.05f;
    // One line (a flute, as its model plays): a new note takes over from the one sounding,
    // slurred from it while it still sounds (Instrument mode plays chords).
    bool mono = false;
    // Legato: how far into the recording a slurred note starts (seconds: past its attack) and
    // how much longer than the new note's fade in the old one takes to let go.
    float legatoSkip = 0.08f, legatoOverlap = 1.5f;
    // Off: a held note keeps its pitch to its end when the chords (or the Root or Mood) move
    // under it, as a melody instrument's model does.
    bool followChords = true;
    // > 0: the voice's low-pass also at most this many times the note's pitch (a pure tone:
    // the upper harmonics of every note rolled off alike).
    float toneTrack = 0;
    // Expression: how far toward the loudest layer full dynamics go (1: all the way; less, a
    // narrower range for a soft folk instrument, as its model plays).
    float layerSpan = 1;

    // The brass pass (Light-Seeker's brass shaping, copied).
    // Punch: each tongued note's level overshoots by this much at its start (dB at full
    // velocity, about a third of it on the softest) and settles back to its own level over
    // `punchSeconds` (a raised cosine). A safety limiter on the part's output takes back only
    // what Punch added, and only above `punchCeiling`: stacked stabs never clip.
    float punchDb = 0, punchSeconds = 0.12f, punchCeiling = 0.89f;
    // Bite: a bright band (4.8 kHz) on the attack, more with velocity, gone within about
    // 200 ms (`biteSeconds` to fall 9 dB); presence: a steady band at 2.9 kHz over the part.
    float bite = 0, biteSeconds = 0.07f, presence = 0;
    // A tonic pedal under the held notes (a drone's low brass, as SynthPatch::pedal): its level
    // and pitch (semitones from the key's root), and the recordings it plays (nullptr: the
    // part's own). Instrument mode: the lowest note held, an octave down, while held (none with
    // Drone Off).
    float pedal = 0, pedalSemis = -12;
    const char* pedalInstrument = nullptr;
    // The front (Light-Seeker's real brass): a tongued note on the sustains also strikes the
    // staccato of the note (the short articulation), levelled to the sustain and ringing out
    // as recorded, at this level (0: none). A recorded sustain swells in; the front is the
    // player's tongue, so a long note speaks as sharply as a short one.
    float front = 0;
    // With a front, the sustain starts this far into its recording (past its swell; seconds),
    // fading in over 20 ms under the front: the note's peak is its tongue, not its swell.
    float frontSkip = 0.08f;
    // Expression: where the softest velocity starts on the way from the softest layer to the
    // loudest (0: the softest layer itself). A brass section's recorded p to ff is far wider than
    // its model's velocity range: a line played a little softer would drop several dB.
    float layerFloor = 0;
    // Off: a held note starting where the last one ends is tongued, not slurred (a fanfare's,
    // a bugle call's repeated notes): it gets its own attack and front.
    bool slur = true;
    // A short note (on the short articulation) levelled to the long one at its key and dynamic
    // (the bank's measured loudness: Light-Seeker's staccatos levelled to their sustains), so
    // a line keeps its level as its notes get shorter.
    bool shortMatch = false;
    // With shortMatch: the short note this much over the long one's measured loudness (dB). The
    // bank measures over the loudest 400 ms, longer than a staccato's hit (Light-Seeker measures
    // a staccato by its loudest 100 ms).
    float shortOverDb = 0;
};

class SamplePart final : public Part {
public:
    static constexpr int maxVoices = 8;
    SamplePatch patch;
    void unshapeForInstrument() override {
        fallback.unshapeForInstrument();
        if (shaped) patch = designedPatch;
        shaped = false;
    }
    void shapeForInstrument(float a, float r) override {
        designedPatch = patch;
        shaped = true;
        fallback.shapeForInstrument(a, r);
        patch.tremoloDepth = 0;   // nothing pulses in time on its own
        // Held recordings: the fade in and the release; ringing ones: how fast they die
        // away below the middle (as recorded above it), and their damping when released.
        patch.attack = patch.attack * timeScale(a) + swell(a);
        patch.release = std::min(patch.release, instrumentRelease) * timeScale(r);
        // (The brass pass.) Release away from its middle on a looped, expressive instrument (the
        // woodwinds, the brass): the note fades over its loop for as long as asked (a long one
        // three times as long: a sampled fade falls 60 dB in its time, steeper than a model's
        // envelope), not through its recorded release (the same whatever Release says).
        if (patch.held && patch.expression && std::abs(r) > 0.25f) {
            patch.recordedTail = false;
            if (r > 0.0f) patch.release *= 3.0f;
        }
        if (!patch.held && r < 0.0f) patch.decay = (patch.decay > 0.0f ? patch.decay : 6.0f) * timeScale(r);
        patch.voices = maxVoices;
        patch.mono = false;
    }
    explicit SamplePart(Part& synthFallback) : fallback(synthFallback) {}

    // True when the recordings play (false: the fallback does).
    bool sampled() const { return main != nullptr && !patch.off; }
    bool loaded() const { return main != nullptr; }

    void prepare(float r) override {
        rate = r;
        fallback.prepare(r);
        for (auto& f : dc) f.setCutoff(20.0f, rate);
        for (auto& o : open) o.prepare(std::size_t(rate / 30.0f) + 8);
        for (auto& f : openDamp) f.setCutoff(2500.0f, rate);
        for (auto& f : presenceBand) f.set(std::min(2900.0f, 0.42f * r), 0.9f, r);
        limiterRelease = 1.0f - std::exp(-1.0f / (0.06f * r));
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
        for (auto& f : formant) f.reset();
        for (auto& o : open) o.clear();
        for (auto& f : openDamp) f.reset();
        bodyType = -1;
        strumBeat = -1.0e9;
        strumCount = 0;
        trem = 1;
        gainNow = -1;
        noiseAt = 0;   // the breath (the woodwinds) the same on every play
        pedalVoice = -1;
        startingPedal = false;
        limiterGain = limiterLowest = 1;
        for (auto& f : presenceBand) f.reset();
    }
    // Punch's safety limiter: the lowest gain it has taken since the last call (tests).
    float punchLimiterLowest() {
        const float g = limiterLowest;
        limiterLowest = 1;
        return g;
    }
    void setKey(uint32_t key, uint32_t salt) override {
        Part::setKey(key, salt);
        fallback.setKey(key, salt);
    }
    bool sustains() const override { return sampled() ? patch.held : fallback.sustains(); }
    float transposeSemis() const override { return sampled() ? patch.transpose : fallback.transposeSemis(); }
    void stopLive(int id) override {
        fallback.stopLive(id);
        if (!loaded()) return;
        for (std::size_t k = 0; k < voice.size(); ++k)
            if (voice[k].on && held[k].live == id) {
                // A short hit played live (released before it became a long one) rings as
                // recorded where the patch lets short hits ring.
                if (voice[k].upgrade && patch.shortRings && !voice[k].zone->loops()) {
                    held[k].on = false;
                    voice[k].upgrade = false;
                    continue;
                }
                held[k].until = -1.0e18;
                voice[k].upgrade = false;
                if (!patch.held) release(voice[k]);
            }
    }

    void start(const PartFrame& f, const NoteOn& n) override {
        if (!sampled()) {
            fallback.slide = slide;
            fallback.start(f, n);
            fallbackAge = 0;   // it rings on after the samples come back on
            return;
        }
        lastPlayed = n.semis + patch.transpose;
        const auto* inst = articulation(n);
        // A short articulation far from the notes it recorded: the long one plays the note.
        if (patch.shortRange > 0.0f && inst != main && inst != nullptr) {
            const float midi = keyOf(f, n);
            if (midi < inst->lowRoot - patch.shortRange || midi > inst->highRoot + patch.shortRange) inst = main;
        }
        startWith(f, n, inst, -1.0f, -1.0f);
    }

    // The second sample set plays (tests).
    bool alternatePlays() const { return alternateOn; }

private:
    // The MIDI note a note plays (its octave folded into the patch's range).
    float keyOf(const PartFrame& f, const NoteOn& n) const {
        const float hz = music::noteHz(f.root, n.semis + patch.transpose);
        float midi = 69.0f + 12.0f * std::log2(std::max(hz, 1.0f) / 440.0f);
        while (midi < float(patch.lowestKey) - 0.5f) midi += 12.0f;
        while (midi > float(patch.highestKey) + 0.5f && midi - 12.0f >= float(patch.lowestKey) - 0.5f) midi -= 12.0f;
        return midi;
    }
    // The dynamic a note plays at: its velocity, raised by the mod wheel toward full (expression).
    float dynamicOf(float velocity) const {
        const float v = dsp::clamp01(velocity);
        return patch.expression ? v + (1.0f - v) * dsp::clamp01(mod) : v;
    }
    // Expression: the whole range of dynamics crossfades across the layers (the softest
    // dynamic the softest layer alone, full the loudest alone), so the level rises evenly.
    float spreadOver(const samples::Instrument& inst, float dynamic) const {
        if (inst.layers < 2) return dynamic;
        const float lo = inst.layerCentre[0], hi = inst.layerCentre[std::size_t(std::min(inst.layers, int(inst.layerCentre.size())) - 1)];
        const float floor = dsp::clamp01(patch.layerFloor), span = std::max(floor, dsp::clamp01(patch.layerSpan));
        return lo + (floor + dsp::clamp01(dynamic) * (span - floor)) * (hi - lo);
    }
    float levelOf(float dynamic) const { return 1.0f - patch.velocityGain + patch.velocityGain * std::pow(dsp::clamp01(dynamic), 1.3f); }
    float cutoffOf(float dynamic) const {
        return patch.toneHz * std::exp2(-patch.velocityTone * (1.0f - dsp::clamp01(dynamic))) * (patch.expression ? 1.0f : std::exp2(2.0f * mod));
    }
public:

private:
    // Starts a note on one articulation; skip >= 0 starts that far into the recording,
    // attack >= 0 fades it in over that long (a short hit handing over to its long one).
    void startWith(const PartFrame& f, const NoteOn& n, const samples::Instrument* inst, float skipOver, float attackOver) {
        const float midi = keyOf(f, n);
        // (Expression: the layers at the note's dynamic, the mod wheel's raise included.)
        const float layerVelocity = patch.expression ? dynamicOf(n.velocity) : n.velocity;
        samples::ZonePair pair;
        if (inst->crossfade && patch.crossfade) pair = inst->findPair(int(std::lround(midi)), patch.expression ? spreadOver(*inst, layerVelocity) : layerVelocity, n.random2);
        else pair.a = inst->find(int(std::lround(midi)), layerVelocity, n.random2);
        // Expression on a short articulation: one take (the nearer layer's), its level between
        // the two layers' (two short takes summed peak apart; each note is heard on its own).
        float pickedGainDb = 0;
        bool picked = false;
        if (patch.expression && pair.a != nullptr && pair.b != nullptr && !pair.a->loops() && !pair.b->loops()) {
            const float target = pair.a->loudnessDb + (pair.b->loudnessDb - pair.a->loudnessDb) * pair.mix;
            if (pair.mix >= 0.5f) pair.a = pair.b;
            pickedGainDb = target - pair.a->loudnessDb;
            pair.b = nullptr;
            picked = true;
        }
        const auto* zone = pair.a;
        if (zone == nullptr) return;

        // (The pedal has the last voice to itself: no note or front takes it, and it takes no note's.)
        const int top = patch.pedal > 0.0f ? maxVoices - 1 : maxVoices;
        const int limit = startingFront ? top : std::clamp(patch.voices, 1, top);
        // A held note whose predecessor ends here is played legato: the old note lets go
        // quickly and the new one comes in without its full attack. (Not a short hit: it is
        // all attack.)
        bool legato = false;
        if (patch.held && patch.slur && n.live == 0 && inst != shortOne && !patch.damper && !startingPedal && !startingFront)
            for (int i = 0; i < limit; ++i) {
                const auto& h = held[std::size_t(i)];
                auto& old = voice[std::size_t(i)];
                if (old.on && h.on && h.until <= n.beat + 1.0e-6 && h.until >= n.beat - 0.07) {
                    held[std::size_t(i)].on = false;
                    release(old, patch.legatoSeconds * patch.legatoOverlap);
                    legato = true;
                }
            }
        if (patch.mono && !startingPedal && !startingFront)
            for (int i = 0; i < maxVoices; ++i) {
                auto& old = voice[std::size_t(i)];
                auto& h = held[std::size_t(i)];
                if (!old.on || old.pedal) continue;
                // Still sounding, or let go just now (its written end is this note's start):
                // the new note slurs on from it (a held one, not a short hit), and it goes.
                const bool justEnded = old.releasing && h.until >= n.beat - 0.07 && h.until <= n.beat + 1.0e-6;
                if (old.releasing && !justEnded) continue;
                if (patch.held && patch.slur && inst != shortOne && !patch.damper) legato = true;
                h.on = false;
                old.upgrade = false;
                if (old.releasing && (old.tailPos >= 0.0 || old.tailPosB >= 0.0)) {
                    // Still crossing into its recorded end: back on its sustain, to fade from there.
                    old.tailPos = old.tailPosB = -1;
                    old.tailMix = 0;
                    old.releasing = false;
                }
                if (old.releasing) hurry(old, patch.legatoSeconds * patch.legatoOverlap);
                else release(old, patch.legatoSeconds * patch.legatoOverlap);
            }
        const int k = startingPedal ? maxVoices - 1 : int(dsp::pickVoice(voice, limit, [](const Voice& v) { return v.on ? v.amp * v.fade * v.gain + 1.0e-9f : 0.0f; }));
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
        // The second layer of a crossfade, and both layers' weights (equal power, each at
        // its zone's own gain: a derived layer's level).
        if (pair.b != nullptr && pair.mix > 0.001f) {
            v.zoneB = pair.b;
            if (patch.expression) mixGains(*zone, *pair.b, pair.mix, v.mixA, v.mixB);
            else samples::crossfadeGains(*zone, *pair.b, pair.mix, v.mixA, v.mixB);
        } else {
            v.mixA = dsp::dbToGain(zone->gainDb + (picked ? pickedGainDb : 0.0f));
        }
        // A derived layer's brightness: a high shelf (the weighted mix of the two layers').
        const float shelfDb = pair.b != nullptr ? dsp::lerp(zone->shelfDb, pair.b->shelfDb, pair.mix) : zone->shelfDb;
        v.shelf = std::abs(shelfDb) > 0.05f;
        v.shelfGain = dsp::dbToGain(shelfDb);
        if (v.shelf) {
            v.shelfLp.setCutoff(std::min(zone->shelfHz, 0.4f * rate), rate);
            v.shelfLp.reset();
        }
        v.midi = v.target = midi;
        v.rateRatio = inst->rate / rate;
        const bool quick = patch.held && n.live == 0 && n.seconds < patch.quickBelow;
        // A tongued note on the sustains that a front will strike (see the end).
        const bool fronted = !legato && !startingPedal && !startingFront && attackOver < 0.0f && skipOver < 0.0f && patch.front > 0.0f && inst == main &&
                             shortOne != nullptr && zone->loops() && midi >= shortOne->lowRoot - patch.shortRange - 0.5f &&
                             midi <= shortOne->highRoot + patch.shortRange + 0.5f;
        const float skip = skipOver >= 0.0f ? skipOver
                                            : (fronted ? std::max(patch.startSeconds, patch.frontSkip)
                                                       : (quick ? patch.quickSeconds : (legato ? patch.legatoSkip : patch.startSeconds)));
        v.pos = double(std::min(skip * inst->rate, float(zone->length) * 0.25f));
        if (zone->loops() && v.pos >= double(zone->loopStart)) v.pos = double(zone->loopStart);
        v.posB = v.pos;
        if (v.zoneB != nullptr && v.zoneB->loops() && v.posB >= double(v.zoneB->loopStart)) v.posB = double(v.zoneB->loopStart);
        // Backwards: from where the note would have got to, back to its strike.
        v.backwards = patch.reverse > 0.0f && n.random < patch.reverse && skipOver < 0.0f;
        if (v.backwards) {
            const float span = std::clamp(n.live != 0 ? patch.reverseSeconds : std::min(n.seconds, patch.reverseSeconds), patch.reverseShortest, patch.reverseSeconds);
            v.pos = std::min(double(span * inst->rate), double(zone->length - 4));
            v.posB = v.zoneB != nullptr ? std::min(v.pos, double(v.zoneB->length - 4)) : v.pos;
        }
        v.limit = patch.fragmentSeconds > 0.0f ? int(patch.fragmentSeconds * rate) : -1;
        // Expression moves the note through the layers while it sounds; breath and the key
        // add to it: not the straight path.
        v.expressive = patch.expression && inst->crossfade && patch.crossfade && (v.zone->loops() || (v.zoneB != nullptr && v.zoneB->loops()));
        v.inst = inst;
        v.key = int(std::lround(midi));
        v.pick = n.random2;
        v.dyn = layerVelocity;
        v.noisy = patch.breath > 0.0f || patch.keyNoise > 0.0f;
        // Punch and bite on a tongued note (not slurred, not the pedal swelling in).
        const bool tongued = !legato && !startingPedal && attackOver < 0.0f;
        if (tongued && patch.punchDb > 0.05f) {
            v.punchDb = patch.punchDb * (0.35f + 0.65f * std::pow(dsp::clamp01(dynamicOf(velocity)), 1.3f));
            v.punchLength = std::max(1, int(patch.punchSeconds * rate));
            v.punchPos = 0;
            v.punchGain = dsp::dbToGain(v.punchDb);
            v.punchStep = 0;
        }
        if (tongued && patch.bite > 0.0f) {
            v.biteBand.set(std::min(4800.0f, 0.4f * rate), 0.7f, rate);
            v.biteBand.reset();
            v.biteNow = patch.bite * (0.4f + 0.9f * dsp::clamp01(dynamicOf(velocity))) * 1.8f;
            v.biteDecay = std::exp(-1.0f / (std::max(patch.biteSeconds, 0.005f) * rate));
        }
        v.pedal = startingPedal;
        v.plainable = v.zoneB == nullptr && !v.shelf && !v.backwards && v.limit < 0 && !v.expressive && !v.noisy;
        v.plain = v.plainable && v.biteNow <= 0.0f;
        // Level: velocity (the layers already differ), evened toward the median loudness
        // of the note's own layer (the layers keep their steps), the patch.
        const float even = std::clamp(patch.even * (inst->medianDb(zone->layer) - zone->loudnessDb), -12.0f, 12.0f);
        v.evenGain = dsp::dbToGain(even);
        v.gain = levelOf(patch.expression ? dynamicOf(velocity) : velocity) * v.evenGain * (startingPedal ? patch.pedal : 1.0f);
        // Its loudness as the bank measured it (the layers' mix), for the front's level.
        const float noteDb = (picked ? pair.a->loudnessDb + pickedGainDb
                                     : (pair.b != nullptr ? dsp::lerp(pair.a->loudnessDb, pair.b->loudnessDb, pair.mix) : pair.a->loudnessDb));
        if (startingFront) v.gain *= patch.front * dsp::dbToGain(std::clamp(frontRefDb - noteDb, -12.0f, 12.0f));
        else if (patch.shortMatch && inst == shortOne && main != nullptr && inst != main) {
            const auto lp = main->findPair(int(std::lround(midi)), patch.expression ? spreadOver(*main, layerVelocity) : layerVelocity, n.random2);
            if (lp.a != nullptr) {
                const float longDb = lp.b != nullptr ? dsp::lerp(lp.a->loudnessDb, lp.b->loudnessDb, lp.mix) : lp.a->loudnessDb;
                v.gain *= dsp::dbToGain(std::clamp(longDb - noteDb + patch.shortOverDb, -12.0f, 12.0f));
            }
        }
        const float attack = attackOver >= 0.0f ? attackOver : (fronted ? std::max(patch.attack, 0.02f) : (quick ? 0.008f : (legato ? patch.legatoSeconds : patch.attack)));
        v.amp = attack > 0.0005f ? 0.0f : 1.0f;
        v.attackStep = attack > 0.0005f ? 1.0f / (attack * rate) : 1.0f;
        v.fadeMul = patch.decay > 0.0f ? std::exp(-6.9078f / (patch.decay * weight::ring * n.decay * rate)) : 1.0f;
        v.pan = dsp::panGains(n.hasPan ? n.pan : spreadPan(patch.pan, patch.spread, n.random));
        v.trackHz = patch.toneTrack > 0.0f ? patch.toneTrack * 440.0f * std::exp2((midi - 69.0f) / 12.0f) : 1.0e9f;
        v.filter.set(std::min({ cutoffOf(patch.expression ? dynamicOf(velocity) : velocity), v.trackHz, 0.45f * rate }), 0.6f, rate);
        if (v.noisy) {
            // The player's breath, tuned to the note (a band around a harmonic), strongest in
            // the attack; a slurred note has no new attack (and no key click).
            v.breathBand.set(std::min(440.0f * std::exp2((midi - 69.0f) / 12.0f) * patch.breathRatio, 0.42f * rate), patch.breathQ, rate);
            v.breathBand.reset();
            v.keyLp.setCutoff(900.0f, rate);
            v.keyLp.reset();
            v.noiseAt = noiseAt;
            noiseAt += 1ull << 24;
            v.breathAge = legato ? 1.0f : 0.0f;
            v.keyLeft = legato || patch.keyNoise <= 0.0f ? 0 : int(0.012f * rate);
            v.keyLength = std::max(v.keyLeft, 1);
        }
        // A slide into the note, as Scoop / Glide says (off: the note starts on its pitch).
        v.scoop = slide.depth(patch.scoopSemis) - slide.plain(patch.scoopSemis);
        float scoopSeconds = patch.scoopSemis != 0.0f ? slide.time(patch.scoopSeconds) : slide.seconds;
        if (v.scoop == 0.0f && patch.bendIn != 0.0f && !legato) {
            v.scoop = patch.bendIn;
            scoopSeconds = patch.bendInSeconds;
        }
        v.scoopMul = v.scoop != 0.0f ? std::exp(-float(dsp::chunk) / (std::max(scoopSeconds, 0.002f) * rate)) : 0.0f;
        v.course = (!zone->loops() || patch.courseLoops) && patch.courseLevel > 0.0f;
        v.courseZone = zone;
        if (v.course && zone->loops()) {
            // Another take for the second player (it loops on its own points), tuned to the first.
            const auto* other = inst->find(int(std::lround(midi)), n.velocity, n.random2 + 0.5f - std::floor(n.random2 + 0.5f));
            if (other != nullptr && other->loops()) v.courseZone = other;
        }
        v.coursePos = v.pos - double(patch.courseMs * 0.001f * inst->rate);
        v.courseRatio = std::exp2(patch.courseCents / 1200.0f + (zone->root - v.courseZone->root) / 12.0f);
        v.vibratoAge = 0;
        v.velocity = velocity;
        held[std::size_t(k)].set(n);
        if ((!patch.held || (patch.shortRings && inst == shortOne && !zone->loops())) && n.live == 0) held[std::size_t(k)].on = false;   // it rings
        if (v.backwards) held[std::size_t(k)].on = false;   // a reversed note plays to its strike
        // An undamped piano key rings on whatever the key does.
        if (patch.damper && midi >= float(patch.undampedFrom) - 0.5f && n.live == 0) held[std::size_t(k)].on = false;
        held[std::size_t(k)].semis = n.semis;
        if (!patch.followChords) held[std::size_t(k)].follow = false;
        // Live (KEYS, Instrument mode): a short hit first; held past the short notes'
        // length it hands over to the long hit (see update()).
        v.upgrade = n.live != 0 && inst != main && main != nullptr;
        v.age = 0;
        v.note = n;
        pitch(v, 0);
        if (startingFront) {
            v.upgrade = false;   // (it lets go with its note: Release shapes the whole note)
        } else if (fronted) {
            frontRefDb = noteDb;
            startingFront = true;
            startWith(f, n, shortOne, -1.0f, -1.0f);
            startingFront = false;
        }
    }

public:

    void update(const PartFrame& f) override {
        if ((main == nullptr || patch.instrument != resolvedId || patch.shortInstrument != resolvedShort || patch.pedalInstrument != resolvedPedal) &&
            samples::Bank::get().ready())
            resolve();
        if (!sampled() || fallbackAge < fallbackRings) {
            fallback.bend = bend;
            fallback.mod = mod;
            fallback.instrumentMode = instrumentMode;
            fallback.drone = drone;
            fallback.slide = slide;
            fallback.update(f);
            if (!sampled() && !anySounding()) return;
        }
        if (!loaded()) return;
        if (patch.body != bodyType) {
            bodyType = std::clamp(patch.body, 0, dsp::BodyCount - 1);
            const auto& b = dsp::bodies[std::size_t(bodyType)];
            for (auto& side : bodyFilter)
                for (std::size_t k = 0; k < side.size(); ++k) side[k].set(b.mode[k].hz, b.mode[k].q, rate);
        }
        if (patch.formantDb != 0.0f)
            for (auto& fm : formant) fm.set(std::min(patch.formantHz, 0.42f * rate), std::max(patch.formantQ, 0.3f), rate);
        if (patch.sympathetic > 0.0f) {
            // (Instrument mode: in sympathy with the note played last, not the key.)
            const float base = instrumentMode ? lastPlayed - 12.0f : -12.0f;
            openPeriod[0] = rate / music::noteHz(f.root, base);
            openPeriod[1] = rate / music::noteHz(f.root, base + (instrumentMode ? 7.0f : float(music::degree(f.mood, 4))));
        }
        trem = 1.0f - patch.tremoloDepth * (0.5f + 0.5f * std::cos(float(f.beat * double(patch.tremoloPerBeat)) * dsp::twoPi));
        const float glide = glideFor(slide.glide(patch.glide), rate);
        // A live short hit whose key is still down after the short notes' length becomes
        // the long hit: the long recording comes in where the note has got to (its attack
        // already heard) over 30 ms while the short one lets go.
        const float shortSeconds = float(double(std::max(patch.shortBeats, patch.tinyBeats)) / std::max(f.beatsPerSecond, 0.01));
        for (std::size_t k = 0; k < voice.size(); ++k) {
            auto& v = voice[k];
            if (!v.on || !v.upgrade) continue;
            v.age += float(dsp::chunk) / rate;
            if (v.age < shortSeconds || !held[k].on || held[k].live == 0) continue;
            v.upgrade = false;
            NoteOn n = v.note;
            const float at = v.age;
            held[k].on = false;
            held[k].live = 0;
            release(v, 0.05f);
            startWith(f, n, main, at, 0.03f);
        }
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
            const bool expressing = patch.expression && !v.releasing && !v.pedal;
            if (expressing) express(v);
            if (patch.liveTone) v.filter.set(std::min({ cutoffOf(expressing ? v.dyn : v.velocity), v.trackHz, 0.45f * rate }), 0.6f, rate);
            v.scoop *= v.scoopMul;
            v.vibratoAge += float(dsp::chunk) / rate;
            if (v.breathAge < 1.0f) v.breathAge = std::min(1.0f, v.breathAge + float(dsp::chunk) / (std::max(patch.breathAttack, 0.005f) * rate));
            pitch(v, float(dsp::chunk) / rate);
        }
        if (patch.pedal > 0.0f || pedalVoice >= 0) updatePedal(f);
    }

    void render(float* left, float* right, int n) override {
        if (!sampled() || fallbackAge < fallbackRings) {
            fallback.render(left, right, n);
            if (sampled()) fallbackAge += n;
            if (!loaded() || !anySounding()) return;
        }
        const auto& b = dsp::bodies[std::size_t(std::max(bodyType, 0))];
        const float amount = dsp::clamp01(patch.bodyAmount);
        const bool body = bodyType > dsp::BodyNone && amount > 0.0f;
        const float direct = dsp::lerp(1.0f, b.direct, amount);
        const float sym = patch.sympathetic;
        const float boost = dsp::dbToGain(patch.formantDb) - 1.0f;
        const bool formed = patch.formantDb != 0.0f;
        // The level glides (20 ms) to a new gain (a Character's own) and the tremolo.
        const float target = patch.gain * trem * matchGain;
        if (gainNow < 0.0f) gainNow = target;
        const float follow = 1.0f - std::exp(-1.0f / (0.02f * rate));
        // The voices one after another over each chunk (the same sums in the same order as
        // sample by sample across them: bit for bit the same output, in tight loops).
        std::array<float, dsp::chunk> sumL {}, sumR {}, dryL {}, dryR {};
        const float presence = patch.presence * 1.6f;
        bool punched = false;   // in this chunk: a voice with Punch (dry: the sum without it)
        for (int i = 0; i < n; ++i) {
            if (i % dsp::chunk == 0) {
                const int m = std::min(dsp::chunk, n - i);
                std::fill(sumL.begin(), sumL.end(), 0.0f);
                std::fill(sumR.begin(), sumR.end(), 0.0f);
                punched = false;
                for (auto& v : voice) {
                    if (!v.on) continue;
                    if (v.punchLength > 0) {
                        if (!punched) {
                            dryL = sumL;
                            dryR = sumR;
                            punched = true;
                        }
                        // Punch: the gain at the end of this chunk (a raised cosine from the
                        // overshoot back to 0 dB), reached linearly over it.
                        v.punchPos += m;
                        const float t = std::min(1.0f, float(v.punchPos) / float(v.punchLength));
                        const float next = t >= 1.0f ? 1.0f : dsp::dbToGain(v.punchDb * 0.5f * (1.0f + std::cos(3.14159265f * t)));
                        v.punchStep = (next - v.punchGain) / float(m);
                        for (int k = 0; k < m && v.on; ++k) {
                            const float y = (v.plain && v.wait <= 0) ? tickPlain(v, *v.zone) : tick(v);
                            dryL[std::size_t(k)] += y * v.pan.l;
                            dryR[std::size_t(k)] += y * v.pan.r;
                            const float yp = y * v.punchGain;
                            v.punchGain += v.punchStep;
                            sumL[std::size_t(k)] += yp * v.pan.l;
                            sumR[std::size_t(k)] += yp * v.pan.r;
                        }
                        if (t >= 1.0f) v.punchLength = 0, v.punchGain = 1, v.punchStep = 0;
                        continue;
                    }
                    for (int k = 0; k < m && v.on; ++k) {
                        const float y = (v.plain && v.wait <= 0) ? tickPlain(v, *v.zone) : tick(v);
                        sumL[std::size_t(k)] += y * v.pan.l;
                        sumR[std::size_t(k)] += y * v.pan.r;
                        if (punched) {
                            dryL[std::size_t(k)] += y * v.pan.l;
                            dryR[std::size_t(k)] += y * v.pan.r;
                        }
                    }
                }
            }
            gainNow += (target - gainNow) * follow;
            const float g = gainNow;
            float l = sumL[std::size_t(i % dsp::chunk)], r = sumR[std::size_t(i % dsp::chunk)];
            if (body) {
                float bl = l * direct, br = r * direct;
                for (std::size_t k = 0; k < 4; ++k) {
                    bl += b.mode[k].gain * amount * bodyFilter[0][k].bandpass(l);
                    br += b.mode[k].gain * amount * bodyFilter[1][k].bandpass(r);
                }
                l = bl;
                r = br;
            }
            if (formed) {
                l += boost * formant[0].bandpass(l);
                r += boost * formant[1].bandpass(r);
            }
            if (presence > 0.0f) {
                l += presence * presenceBand[0].bandpass(l);
                r += presence * presenceBand[1].bandpass(r);
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
            float outL = dc[0].hp(l) * g, outR = dc[1].hp(r) * g;
            if (punched || limiterGain < 1.0f) {
                // Punch's safety limiter (instant attack, 60 ms release): only what Punch added is
                // ever taken back, and only above the ceiling.
                const std::size_t c = std::size_t(i % dsp::chunk);
                const float peak = std::max(std::abs(outL), std::abs(outR));
                const float dry = punched ? std::max(std::abs(dryL[c]), std::abs(dryR[c])) * std::abs(g) : 0.0f;
                const float floorLevel = std::max(patch.punchCeiling, dry);
                const float needed = peak > floorLevel ? floorLevel / peak : 1.0f;
                if (needed < limiterGain) limiterGain = needed;
                else {
                    limiterGain = std::min(needed, limiterGain + limiterRelease * (1.0f - limiterGain));
                    if (limiterGain > 0.99999f) limiterGain = 1.0f;
                }
                limiterLowest = std::min(limiterLowest, limiterGain);
                outL *= limiterGain;
                outR *= limiterGain;
            }
            left[i] += outL;
            right[i] += outR;
        }
    }

    SlideDesign designed() const override {
        if (!sampled()) return fallback.designed();
        return { patch.scoopSemis, patch.scoopSemis != 0.0f ? patch.scoopSeconds : 0.0f, patch.held ? patch.glide : 0.0f };
    }

    bool anySounding() const {
        for (const auto& v : voice)
            if (v.on) return true;
        return false;
    }
    // How many voices sound (tests).
    int sounding() const {
        int c = 0;
        for (const auto& v : voice) c += v.on ? 1 : 0;
        return c;
    }

private:
    struct Voice {
        bool on = false, releasing = false, course = false, backwards = false, shelf = false, plain = true;
        const samples::Zone* zone = nullptr;
        const samples::Zone* zoneB = nullptr;   // a crossfade's second layer
        double posB = 0, tailPosB = -1;
        float incB = 1, mixA = 1, mixB = 0, shelfGain = 1;
        int limit = -1;                        // samples left of a fragment (-1: no limit)
        dsp::OnePole shelfLp;
        const samples::Zone* courseZone = nullptr;
        double pos = 0, coursePos = 0, tailPos = -1;
        float rateRatio = 1, inc = 1, courseRatio = 1;
        float midi = 60, target = 60, scoop = 0, scoopMul = 0;
        float amp = 1, attackStep = 1, fade = 1, fadeMul = 1, releaseMul = 1;
        float tailMix = 0, tailStep = 0;
        float gain = 1, vibratoAge = 0, vibratoPhase = 0, velocity = 1;
        int wait = 0;
        dsp::PanGains pan { 0.7071f, 0.7071f };
        dsp::Svf filter;
        // Expression (the woodwinds pass): the note's instrument, key, round-robin pick and
        // dynamic, so the mod wheel can move it through the layers; its evened gain.
        bool expressive = false, noisy = false;
        const samples::Instrument* inst = nullptr;
        int key = 60;
        float pick = 0.5f, dyn = 1, evenGain = 1, trackHz = 1.0e9f;
        // Breath and the key click.
        dsp::Svf breathBand;
        dsp::OnePole keyLp;
        uint64_t noiseAt = 0;
        float breathAge = 1;
        int keyLeft = 0, keyLength = 1;
        // A live short hit that may still become its long one, its age and its note.
        bool upgrade = false;
        float age = 0;
        NoteOn note {};
        // The brass pass: Punch (the gain now, its step per sample, where it is in its
        // settling), bite (its band, its amount now, its decay per sample), the pedal note.
        float punchDb = 0, punchGain = 1, punchStep = 0;
        int punchPos = 0, punchLength = 0;
        dsp::Svf biteBand;
        float biteNow = 0, biteDecay = 1;
        bool pedal = false, plainable = true;
    };

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

    // One zone's sample at `pos`, then its position moved on: its loop (unless the note is
    // letting go), its crossing into the recorded tail.
#if defined(__GNUC__) || defined(__clang__)
    [[gnu::always_inline]]
#endif
    inline float advance(const samples::Zone& z, double& pos, double& tailPos, float inc, bool releasing, float tailMix) const {
        float y = read(z.data, std::clamp(pos, 1.0, double(z.length - 1)));
        if (tailPos >= 0.0) {
            y = y * (1.0f - tailMix) + read(z.data, tailPos) * tailMix;
            tailPos += double(inc);
        }
        pos += double(inc);
        if (z.loops() && !releasing && inc > 0.0f && pos >= double(z.loopEnd)) pos -= double(z.loopEnd - z.loopStart);
        if (tailPos >= double(z.length - 1)) tailPos = -1, pos = double(z.length);
        return y;
    }

    // A note with no crossfade, shelf, reverse or fragment limit (most of them): the
    // straight path, as before Milestone 17.
#if defined(__GNUC__) || defined(__clang__)
    [[gnu::always_inline]]
#endif
    inline float tickPlain(Voice& v, const samples::Zone& z) {
        float y = read(z.data, v.pos);
        if (v.course && v.coursePos >= 0.0) {
            const auto& c = *v.courseZone;
            if (v.coursePos < double(c.length - 1)) y += patch.courseLevel * read(c.data, v.coursePos);
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
        if (v.course && v.courseZone->loops() && !v.releasing && v.coursePos >= double(v.courseZone->loopEnd))
            v.coursePos -= double(v.courseZone->loopEnd - v.courseZone->loopStart);
        if (z.loops() && !v.releasing && v.pos >= double(z.loopEnd)) v.pos -= double(z.loopEnd - z.loopStart);
        if (v.tailPos >= double(z.length - 1)) v.tailPos = -1, v.tailMix = 0, v.pos = double(z.length);
        if (v.amp < 1.0f) v.amp = std::min(1.0f, v.amp + v.attackStep);
        v.fade *= v.fadeMul * v.releaseMul;
        const float out = v.filter.tick(y * v.mixA).lp * v.amp * v.fade * v.gain;
        if (v.pos >= double(z.length - 1) || v.fade < 1.0e-4f) v.on = false;
        return out;
    }

#if defined(__GNUC__) || defined(__clang__)
    [[gnu::always_inline]]
#endif
    inline float tick(Voice& v) {
        if (v.wait > 0) {
            --v.wait;
            return 0.0f;
        }
        const auto& z = *v.zone;
        if (v.plain) return tickPlain(v, z);
        float y = 0;
        if (v.backwards) {
            y = read(z.data, std::max(v.pos, 1.0)) * v.mixA;
            if (v.zoneB != nullptr) y += read(v.zoneB->data, std::max(v.posB, 1.0)) * v.mixB;
            v.pos -= double(v.inc);
            v.posB -= double(v.incB);
        } else {
            y = advance(z, v.pos, v.tailPos, v.inc, v.releasing, v.tailMix) * v.mixA;
            if (v.zoneB != nullptr) y += advance(*v.zoneB, v.posB, v.tailPosB, v.incB, v.releasing, v.tailMix) * v.mixB;
            if (v.tailPos >= 0.0 || v.tailPosB >= 0.0) {
                v.tailMix += v.tailStep;
                if (v.tailMix >= 1.0f) {
                    if (v.tailPos >= 0.0) v.pos = v.tailPos, v.tailPos = -1;
                    if (v.tailPosB >= 0.0) v.posB = v.tailPosB, v.tailPosB = -1;
                    v.tailMix = 0;
                }
            }
        }
        if (v.course && v.coursePos >= 0.0) {
            const auto& c = *v.courseZone;
            if (v.coursePos < double(c.length - 1)) y += patch.courseLevel * read(c.data, v.coursePos) * v.mixA;
        }
        v.coursePos += double(v.inc * v.courseRatio);
        if (v.course && v.courseZone->loops() && !v.releasing && v.coursePos >= double(v.courseZone->loopEnd))
            v.coursePos -= double(v.courseZone->loopEnd - v.courseZone->loopStart);
        if (v.shelf) {
            const float lo = v.shelfLp.lp(y);
            y = lo + v.shelfGain * (y - lo);
        }
        if (v.amp < 1.0f) v.amp = std::min(1.0f, v.amp + v.attackStep);
        v.fade *= v.fadeMul * v.releaseMul;
        float shaped = v.filter.tick(y).lp;
        if (v.biteNow > 0.0f) {
            shaped += v.biteNow * v.biteBand.bandpass(shaped);
            v.biteNow *= v.biteDecay;
            if (v.biteNow < 1.0e-4f) {
                v.biteNow = 0;
                v.plain = v.plainable;   // the attack over: back on the straight path
            }
        }
        float out = shaped * v.amp * v.fade * v.gain;
        if (v.noisy) out += breathOf(v) * v.amp * v.fade * v.gain;
        const bool endA = v.backwards ? v.pos <= 1.0 : v.pos >= double(z.length - 1);
        if (endA || v.fade < 1.0e-4f) v.on = false;
        if (v.limit >= 0) {
            // A fragment: a 15 ms fade as its time runs out.
            if (--v.limit <= 0) v.on = false;
            else if (v.limit < int(0.015f * rate)) v.fade *= 0.997f;
        }
        return out;
    }

    // The breath and the key click (relative to the note's level): white noise through the
    // breath's band, stronger in the attack, and the key's short low thump (a raised-cosine
    // burst of low noise: no edge to click).
    inline float breathOf(Voice& v) {
        const float n = noise.white(v.noiseAt++);
        float y = 0;
        if (patch.breath > 0.0f) {
            const float burst = v.breathAge < 1.0f ? 1.0f + patch.breathBurst * (1.0f - v.breathAge) * (1.0f - v.breathAge) : 1.0f;
            y += v.breathBand.bandpass(n) * patch.breath * burst;
        }
        if (v.keyLeft > 0) {
            const float t = 1.0f - float(v.keyLeft) / float(v.keyLength);
            y += v.keyLp.lp(n) * patch.keyNoise * 4.0f * (0.5f - 0.5f * std::cos(dsp::twoPi * t));
            --v.keyLeft;
        }
        return y;
    }

    // Expression: the note's dynamic follows its velocity raised by the mod wheel (gliding,
    // 30 ms); the layers it crossfades between move with it (the louder layer coming in where
    // the softer one has gone, each from where its recording has got to), its level and tone.
    void express(Voice& v) {
        const float target = dynamicOf(v.velocity);
        const float k = 1.0f - std::exp(-float(dsp::chunk) / (0.03f * rate));
        const float before = v.dyn;
        v.dyn += (target - v.dyn) * k;
        if (std::abs(target - v.dyn) < 1.0e-4f) v.dyn = target;
        if (v.dyn == before) return;
        v.gain = levelOf(v.dyn) * v.evenGain;
        v.filter.set(std::min({ cutoffOf(v.dyn), v.trackHz, 0.45f * rate }), 0.6f, rate);
        if (!v.expressive || v.inst == nullptr || v.tailPos >= 0.0 || v.tailPosB >= 0.0) return;
        const auto pair = v.inst->findPair(v.key, spreadOver(*v.inst, v.dyn), v.pick);
        if (pair.a == nullptr) return;
        const auto into = [](const samples::Zone& z, double pos) {
            // Where a newly entering layer starts: as far in as the sounding one (in its loop).
            if (z.loops() && pos >= double(z.loopEnd)) return double(z.loopStart) + std::fmod(pos - double(z.loopStart), double(z.loopEnd - z.loopStart));
            return std::min(pos, double(z.length - 2));
        };
        if (pair.a != v.zone || pair.b != v.zoneB) {
            if (v.zoneB != nullptr && pair.a == v.zoneB) {   // up a layer
                v.zone = v.zoneB;
                v.pos = v.posB;
                v.zoneB = pair.b;
                if (pair.b != nullptr) v.posB = into(*pair.b, v.pos);
            } else if (pair.b != nullptr && pair.b == v.zone) {   // down a layer
                v.zoneB = v.zone;
                v.posB = v.pos;
                v.zone = pair.a;
                v.pos = into(*pair.a, v.posB);
            } else if (pair.a == v.zone) {   // the second layer comes in or goes
                v.zoneB = pair.b;
                if (pair.b != nullptr) v.posB = into(*pair.b, v.pos);
            } else {
                return;   // another take: keep what sounds
            }
            v.rateRatio = v.inst->rate / rate;
        }
        if (v.zoneB != nullptr) mixGains(*v.zone, *v.zoneB, pair.mix, v.mixA, v.mixB);
        else v.mixA = dsp::dbToGain(v.zone->gainDb), v.mixB = 0;
        const float shelfDb = v.zoneB != nullptr ? dsp::lerp(v.zone->shelfDb, v.zoneB->shelfDb, pair.mix) : v.zone->shelfDb;
        v.shelfGain = dsp::dbToGain(shelfDb);
        if (!v.shelf && std::abs(shelfDb) > 0.05f) {
            v.shelf = true;
            v.shelfLp.setCutoff(std::min(v.zone->shelfHz, 0.4f * rate), rate);
            v.shelfLp.reset();
        }
    }

    // The pedal (the brass pass, as SynthPart's): the key's tonic under the held notes, on the
    // pedal's recordings, swelling in; Instrument mode: the lowest note held, an octave down,
    // while a key holds it (none with Drone Off).
    void updatePedal(const PartFrame& f) {
        bool any = false;
        float lowest = 1.0e9f;
        for (std::size_t k = 0; k < voice.size(); ++k)
            if (voice[k].on && !voice[k].pedal && held[k].on) {
                any = true;
                lowest = std::min(lowest, held[k].semis);
            }
        const float semis = instrumentMode ? lowest - 12.0f : patch.pedalSemis;
        const bool on = patch.pedal > 0.0f && any && (!instrumentMode || (drone != DroneOff && lowest < 1.0e8f));
        Voice* pv = nullptr;
        if (pedalVoice >= 0) {
            auto& v = voice[std::size_t(pedalVoice)];
            if (v.on && v.pedal && !v.releasing) pv = &v;
            else pedalVoice = -1;
        }
        if (on && pv == nullptr) {
            NoteOn n;
            n.semis = semis;
            n.beat = f.beat;
            n.length = 1.0e9;
            n.seconds = 1.0e9f;
            n.velocity = 0.8f;
            n.random = 0.5f;
            n.random2 = 0.5f;
            startingPedal = true;
            startWith(f, n, pedalInst != nullptr ? pedalInst : main, -1.0f, std::max(patch.attack, instrumentMode ? 0.05f : 0.5f));
            startingPedal = false;
            for (std::size_t k = 0; k < voice.size(); ++k)
                if (voice[k].on && voice[k].pedal && !voice[k].releasing && voice[k].age == 0.0f && held[k].on) {
                    pedalVoice = int(k);
                    held[k].follow = false;
                }
        } else if (on) {
            // It follows the key (the Root, a Location's far point), gliding as the drone does.
            NoteOn n;
            n.semis = semis;
            pv->target = keyOf(f, n);
        } else if (pv != nullptr) {
            held[std::size_t(pedalVoice)].on = false;
            release(*pv, instrumentMode ? patch.release * weight::release : std::max(patch.release * weight::release, 0.5f));
            pedalVoice = -1;
        }
    }

    // A voice already letting go (in its recorded end) goes within `seconds`, as a slurred
    // note takes over from it.
    void hurry(Voice& v, float seconds) {
        v.releaseMul = std::min(v.releaseMul, std::exp(-6.9078f / (std::max(seconds, 0.01f) * rate)));
    }

    // Two layers' gains for a crossfade: equal power for two recordings; a layer derived from
    // the same take (in phase with it) crossfades in amplitude, so the sum does not bulge.
    static void mixGains(const samples::Zone& a, const samples::Zone& b, float mix, float& ga, float& gb) {
        samples::crossfadeGains(a, b, mix, ga, gb);
        if (a.data != b.data) return;
        const float m = std::clamp(mix, 0.0f, 1.0f);
        const float c = std::cos(1.5707963f * m), s = std::sin(1.5707963f * m);
        ga *= c > 1.0e-6f ? (1.0f - m) / c : 0.0f;
        gb *= s > 1.0e-6f ? m / s : 0.0f;
    }

    void release(Voice& v, float seconds = -1.0f) {
        if (v.releasing) return;
        if (v.backwards && seconds <= 0.0f) return;   // it plays to its strike
        if (patch.damper && seconds <= 0.0f) {
            // A piano key let go: undamped strings ring on; elsewhere the damper, as recorded.
            if (v.midi >= float(patch.undampedFrom) - 0.5f) return;
            v.releasing = true;
            const float s = (v.zone->damperSeconds > 0.0f ? v.zone->damperSeconds : patch.release) * patch.damperScale;
            v.releaseMul = std::exp(-6.9078f / (std::max(s, 0.02f) * rate));
            return;
        }
        v.releasing = true;
        const float s = seconds > 0.0f ? seconds : patch.release * weight::release;
        const auto& z = *v.zone;
        if (seconds <= 0.0f && patch.recordedTail && z.release > 0 && v.pos < double(z.release)) {
            // The bow lifts: the recorded tail, faded in over 40 ms, and a slower release on top.
            v.tailPos = double(z.release);
            if (v.zoneB != nullptr && v.zoneB->release > 0 && v.posB < double(v.zoneB->release)) v.tailPosB = double(v.zoneB->release);
            v.tailMix = 0;
            v.tailStep = 1.0f / (0.04f * rate);
            v.releaseMul = std::exp(-6.9078f / (std::max(s * 2.0f, 0.05f) * rate));
        } else {
            v.releaseMul = std::exp(-6.9078f / (std::max(s, 0.01f) * rate));
        }
    }

    void pitch(Voice& v, float) {
        float semis = v.midi - v.zone->root + v.scoop + bend;
        const float wheel = patch.expression ? 0.0f : mod;   // (expression: the wheel is the dynamics)
        if (patch.vibratoDepth > 0.0f || wheel > 0.01f) {
            const float delay = wheel > 0.01f ? 0.0f : patch.vibratoDelay;
            const float depth = (patch.vibratoDepth + 0.45f * wheel) * dsp::clamp01((v.vibratoAge - delay) / 0.4f);
            v.vibratoPhase += patch.vibratoHz * float(dsp::chunk) / rate;
            v.vibratoPhase -= std::floor(v.vibratoPhase);
            semis += depth * std::sin(dsp::twoPi * v.vibratoPhase);
        }
        v.inc = v.rateRatio * std::exp2(semis / 12.0f);
        if (v.zoneB != nullptr) v.incB = v.rateRatio * std::exp2((semis + v.zone->root - v.zoneB->root) / 12.0f);
    }

    const samples::Instrument* articulation(const NoteOn& n) const {
        // Live: its length is not known yet, so the short hit (see update()).
        if (n.live != 0) return patch.liveLong ? main : (shortOne != nullptr ? shortOne : (tiny != nullptr ? tiny : main));
        if (tiny != nullptr && n.length <= double(patch.tinyBeats) + 1.0e-6) return tiny;
        if (shortOne != nullptr && n.live == 0 && n.length <= double(patch.shortBeats) + 1.0e-6) return shortOne;
        return main;
    }

    // Finds the recordings (once the bank is ready; no allocation).
    void resolve() {
        auto& bank = samples::Bank::get();
        if (!bank.ready() || patch.instrument == nullptr) return;
        resolvedId = patch.instrument;
        resolvedShort = patch.shortInstrument;
        main = bank.instrument(patch.instrument);
        shortOne = patch.shortInstrument != nullptr ? bank.instrument(patch.shortInstrument) : nullptr;
        tiny = patch.tinyInstrument != nullptr ? bank.instrument(patch.tinyInstrument) : nullptr;
        pedalInst = patch.pedalInstrument != nullptr ? bank.instrument(patch.pedalInstrument) : nullptr;
        resolvedPedal = patch.pedalInstrument;
        // The second sample set, where the bank has it: levelled to the first by their
        // layers' measured loudness (each note keeps its own dynamics).
        alternateOn = false;
        matchGain = 1;
        if (const auto* alt = patch.alternate != nullptr ? bank.instrument(patch.alternate) : nullptr) {
            if (main != nullptr) {
                const auto mean = [](const samples::Instrument& i) {
                    float sum = 0;
                    for (int l = 0; l < i.layers; ++l) sum += i.medianDb(l);
                    return sum / float(std::max(i.layers, 1));
                };
                const float a = mean(*main), b = mean(*alt);
                matchGain = dsp::dbToGain(std::clamp(a - b, -24.0f, 24.0f));
            }
            main = alt;
            const auto* altShort = patch.alternateShort != nullptr ? bank.instrument(patch.alternateShort) : nullptr;
            shortOne = altShort;
            alternateOn = true;
        }
    }

    Part& fallback;
    SamplePatch designedPatch {};
    bool shaped = false;
    const samples::Instrument *main = nullptr, *shortOne = nullptr, *tiny = nullptr, *pedalInst = nullptr;
    const char* resolvedPedal = nullptr;
    // The brass pass: the pedal's voice (-1: none), a note being started as the pedal, Punch's
    // safety limiter (its gain and release per sample), the presence band.
    int pedalVoice = -1;
    bool startingPedal = false, startingFront = false;
    float frontRefDb = 0;
    float limiterGain = 1, limiterLowest = 1, limiterRelease = 0;
    std::array<dsp::Svf, 2> presenceBand;
    bool alternateOn = false;
    float matchGain = 1;
    const char* resolvedId = nullptr;
    const char* resolvedShort = nullptr;
    std::array<Voice, maxVoices> voice {};
    std::array<HeldNote, maxVoices> held {};
    std::array<dsp::OnePole, 2> dc;
    std::array<std::array<dsp::Svf, 4>, 2> bodyFilter;
    std::array<dsp::Svf, 2> formant;
    int bodyType = -1;
    std::array<dsp::DelayLine, 2> open;
    std::array<dsp::OnePole, 2> openDamp;
    std::array<float, 2> openPeriod { 400, 300 };
    float lastPlayed = 0;
    // Samples since the fallback last started a note (it renders while its notes may ring).
    int fallbackAge = 1 << 30;
    static constexpr int fallbackRings = 48000 * 12;
    double strumBeat = -1.0e9;
    float strumFirst = 0, trem = 1, gainNow = -1;
    int strumCount = 0;
};

// The brass pass: a sampled part layered over its model (the Castle's drone: the recorded
// trombones over the model's pedal, sub and swell). Each note the model starts while the
// recordings play starts `under` its level (velocity scaled); notes already sounding keep theirs,
// so the bank becoming ready mid-play never steps the level. Without the recordings the model
// plays alone, as before.
class UnderLayer final : public Part {
public:
    UnderLayer(Part& model, SamplePart& recorded) : a(model), b(recorded) {}
    float under = 1;
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
        if (b.sampled()) {
            NoteOn m = n;
            m.velocity *= under;
            a.start(f, m);
        } else {
            a.start(f, n);
        }
        b.start(f, n);
    }
    void update(const PartFrame& f) override {
        for (auto* p : { &a, static_cast<Part*>(&b) }) {
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
        b.render(left, right, n);
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
    SamplePart& b;
};

// The developer's piano (Milestone 17), as every Place that plays it starts from: the held
// notes (natural decay, the dampers as recorded, the top keys undamped), the short hits
// for short written notes (they ring as recorded, damper and all), velocity layers
// crossfaded. A Place then sets its own tone, level and effects.
inline SamplePatch pianoPatch() {
    SamplePatch p;
    p.instrument = "piano-long";
    p.shortInstrument = "piano-short";
    p.shortBeats = 0.5f;
    p.shortRings = true;
    p.held = true;
    p.recordedTail = false;
    p.damper = true;
    p.undampedFrom = 89;          // F6: the strings above have no dampers
    p.release = 0.25f;            // a key without a measured damper time
    p.crossfade = true;
    p.velocityGain = 0.3f;        // the layers carry most of the dynamics
    p.velocityTone = 0.35f;
    p.even = 0.35f;
    p.toneHz = 18000;
    p.spread = 0.25f;
    p.voices = 8;
    p.legatoSeconds = 0.02f;
    return p;
}

// A woodwind (the woodwinds pass: VSCO 2 Community Edition's flute, piccolo, oboe, clarinet
// and bassoon), as every Place that plays one starts from: held notes on the looped
// sustains (legato from one to the next), notes of a sixteenth or less on the staccatos
// (where they recorded the note, else the sustain), the dynamics as crossfaded velocity
// layers and the mod wheel as expression, live notes on the sustain, the recorded ends on
// release; a little breath in the attack and a soft key click. A Place then sets its tone,
// level, vibrato, slide and colour.
inline SamplePatch woodwindPatch(const char* sustain, const char* staccato) {
    SamplePatch p;
    p.instrument = sustain;
    p.shortInstrument = staccato;
    p.shortBeats = 0.25f;
    p.shortRange = 4.0f;
    p.shortRings = false;
    p.liveLong = true;
    p.held = true;
    p.recordedTail = true;
    p.crossfade = true;
    p.expression = true;
    p.velocityGain = 0.25f;     // the layers carry most of the dynamics
    p.velocityTone = 0.5f;
    p.even = 0.75f;             // the registers' loudness evened toward the models' (the dynamics kept)
    p.toneHz = 16000;
    p.legatoSeconds = 0.025f;
    p.legatoSkip = 0.3f;        // a slurred note starts on its sustain, past the attack
    p.legatoOverlap = 6.0f;
    p.release = 0.12f;
    p.glide = 0;                // a melody: its notes do not follow the chords
    p.voices = 4;
    p.spread = 0.25f;
    p.breath = 0.015f;
    p.breathRatio = 2.0f;
    p.breathQ = 1.5f;
    p.breathBurst = 2.5f;
    p.breathAttack = 0.08f;
    p.keyNoise = 0.006f;
    p.mono = true;
    p.followChords = false;     // a melody: each note keeps its pitch, as the models do
    return p;
}

// A brass instrument (the brass pass: VSCO 2 Community Edition's horn, trumpet, tenor trombone
// and tuba), as every part that plays one starts from: held notes on the looped sustains
// (slurred from one to the next where the part plays one line), short notes on the staccatos
// (where they recorded the note, else the sustain), every recorded dynamic a crossfaded velocity
// layer and the mod wheel as dynamics (expression), live notes on the sustain, the recorded
// ends on release. The attack is struck: Punch (an overshoot settling over 120 ms, its safety
// limiter on the part), bite on the attack and a little presence, so it cuts through. A part
// then sets its tone, level, vibrato, slide and colour.
inline SamplePatch brassPatch(const char* sustain, const char* staccato) {
    SamplePatch p;
    p.instrument = sustain;
    p.shortInstrument = staccato;
    p.shortBeats = 0.25f;
    p.shortRange = 4.0f;
    p.shortRings = false;
    p.liveLong = true;
    p.held = true;
    p.recordedTail = true;
    p.crossfade = true;
    p.expression = true;
    p.velocityGain = 0.25f;     // the layers carry most of the dynamics
    p.velocityTone = 0.6f;      // brass darkens a lot when soft
    // Only a little evening of the registers: a brass part climbing into its loud top (a
    // fanfare at a Climax) gets louder, as the players and the models do.
    p.even = 0.25f;
    p.toneHz = 16000;
    p.legatoSeconds = 0.03f;
    p.legatoSkip = 0.3f;        // a slurred note starts on its sustain, past its swell
    p.legatoOverlap = 6.0f;
    p.release = 0.15f;
    p.voices = 4;
    p.spread = 0.25f;
    p.punchDb = 3.0f;
    p.punchSeconds = 0.12f;
    p.bite = 0.25f;
    p.presence = 0.1f;
    p.front = 1.0f;             // a long note's tongue: its staccato struck with it
    p.layerFloor = 0.25f;       // the softest velocity a quarter of the way up the recorded dynamics
    p.shortMatch = true;        // the staccatos as loud as the sustains at the note's dynamic,
    p.shortOverDb = 2.0f;       // measured by their hit
    return p;
}

} // namespace wb
