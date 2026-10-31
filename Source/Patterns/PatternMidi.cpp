#include "PatternMidi.h"
#include <map>
#include <memory>
#include <vector>

namespace wb::pat {

namespace {
// MIDI note of the key root in the octave patterns count from (C3 = 48).
int rootNote(int root) { return 48 + std::clamp(root, 0, 11); }
} // namespace

void degreeFor(int mood, int semitones, bool snap, int& degree, int& alter) {
    const auto& scale = music::scaleFor(mood);
    const int target = snap ? music::quantise(mood, float(semitones)) : semitones;
    // The highest degree at or below the target.
    int d = music::floorDiv(target, 12) * scale.size;
    while (music::degree(mood, d + 1) <= target) ++d;
    while (music::degree(mood, d) > target) --d;
    degree = d;
    alter = target - music::degree(mood, d);
}

template <typename P>
void appendNotes(juce::MidiMessageSequence& track, const P& pattern, const MidiTarget& target, double beats, int channel, double offset) {
    const double perBeat = stepsPerBeat(pattern.resolution);
    const double passBeats = pattern.lengthBeats();
    if (pattern.count <= 0 || passBeats <= 0) return;
    const bool once = beats <= 0;
    const double end = once ? passBeats : beats;
    const int passes = once ? 1 : int(std::ceil(end / passBeats - 1.0e-9));
    struct Written {
        double on, off;
        int note, velocity;
    };
    std::vector<Written> notes;
    for (int pass = 0; pass < passes; ++pass)
        for (int i = 0; i < pattern.count; ++i) {
            const auto& n = pattern.notes[std::size_t(i)];
            const double beat = double(pass) * passBeats + double(n.step) / perBeat;
            if (beat >= end - 1.0e-9) continue;
            int note;
            if (target.kit) {
                note = kitNotes[std::size_t(std::clamp(int(n.degree), 0, int(kitNotes.size()) - 1))];
            } else {
                const int chord = pattern.followChords ? music::chordDegree(beat + offset, target.danger) : 0;
                note = rootNote(target.root) + semitonesFor(pattern, target.mood, chord, n.degree, n.alter);
            }
            if (note < 0 || note > 127) continue;
            // A note running past the end is cut there (once: as written).
            const double offBeat = once ? beat + double(n.length) / perBeat : std::min(end, beat + double(n.length) / perBeat);
            notes.push_back({ (beat + offset) * ticksPerBeat, (offBeat + offset) * ticksPerBeat, note, int(n.velocity) });
        }
    std::stable_sort(notes.begin(), notes.end(), [](const Written& a, const Written& b) { return a.on < b.on; });
    // MIDI cannot hold two notes of one pitch at once: a note still sounding when the
    // same pitch starts again ends there (a second note at the very same time is dropped).
    std::array<int, 128> sounding;
    sounding.fill(-1);
    std::vector<bool> keep(notes.size(), true);
    for (std::size_t i = 0; i < notes.size(); ++i) {
        auto& previous = sounding[std::size_t(notes[i].note)];
        if (previous >= 0 && notes[std::size_t(previous)].off > notes[i].on) {
            if (notes[std::size_t(previous)].on >= notes[i].on - 0.5) {
                keep[i] = false;
                continue;
            }
            notes[std::size_t(previous)].off = notes[i].on;
        }
        previous = int(i);
    }
    for (std::size_t i = 0; i < notes.size(); ++i) {
        if (!keep[i]) continue;
        const auto& n = notes[i];
        track.addEvent(juce::MidiMessage::noteOn(channel, n.note, juce::uint8(n.velocity)), n.on);
        track.addEvent(juce::MidiMessage::noteOff(channel, n.note), std::max(n.on + 1.0, n.off - 1.0));
    }
}

template <typename P>
juce::MidiFile toMidi(const P& pattern, const MidiTarget& target, double chordsFrom) {
    juce::MidiMessageSequence track;
    track.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0);
    track.addEvent(juce::MidiMessage::tempoMetaEvent(int(60000000.0 / std::clamp(double(target.bpm), 20.0, 999.0))), 0);
    if (chordsFrom != 0.0) {
        juce::MidiMessageSequence moved;
        appendNotes(moved, pattern, target, 0, 1, chordsFrom);
        moved.addTimeToMessages(-chordsFrom * ticksPerBeat);
        track.addSequence(moved, 0);
    } else {
        appendNotes(track, pattern, target, 0, 1);
    }
    track.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote(ticksPerBeat);
    file.addTrack(track);
    return file;
}

template <typename P>
int fromMidi(const juce::MidiFile& source, P& out, const MidiTarget& target, bool snap, int resolution, int fixedBars) {
    juce::MidiFile file(source);
    const short format = file.getTimeFormat();
    // SMPTE files: convert through seconds at the target tempo.
    if (format <= 0) file.convertTimestampTicksToSeconds();
    struct Raw {
        double beat, length;
        int note, velocity;
    };
    std::vector<Raw> raw;
    // A file that marks where its tracks end (as the scene export does, at the last bar)
    // keeps that length.
    double marked = 0;
    for (int t = 0; t < file.getNumTracks(); ++t) {
        juce::MidiMessageSequence seq(*file.getTrack(t));
        seq.updateMatchedPairs();
        for (int i = 0; i < seq.getNumEvents(); ++i) {
            const auto* e = seq.getEventPointer(i);
            if (e->message.isEndOfTrackMetaEvent() && format > 0) marked = std::max(marked, e->message.getTimeStamp() / double(format));
            if (!e->message.isNoteOn()) continue;
            const double on = e->message.getTimeStamp();
            const double off = e->noteOffObject != nullptr ? e->noteOffObject->message.getTimeStamp() : on + (format > 0 ? format / 4.0 : 0.125);
            const double toBeats = format > 0 ? 1.0 / double(format) : double(target.bpm) / 60.0;
            raw.push_back({ on * toBeats, std::max(0.0, off - on) * toBeats, e->message.getNoteNumber(), e->message.getVelocity() });
        }
    }
    if (raw.empty()) return 0;
    std::sort(raw.begin(), raw.end(), [](const Raw& a, const Raw& b) { return a.beat < b.beat; });

    // (On the heap: a long view holds 160 kB of notes.)
    auto held = std::make_unique<P>();
    P& p = *held;
    p.followChords = false;
    p.swing = out.swing;
    const double perBeat = stepsPerBeat(resolution);
    double end = marked;
    for (const auto& r : raw) end = std::max(end, r.beat + std::max(r.length, 1.0 / perBeat));
    const int bars = fixedBars > 0 ? std::clamp(fixedBars, 1, P::barLimit) : std::clamp(int(std::ceil(end / 4.0 - 1.0e-6)), 1, std::min(P::barLimit, maxBars));
    if (fixedBars > 0) p.setLength(resolution, bars);
    else p.setGrid(resolution, bars);

    // Kit: GM drum notes keep their rows, other pitches take the remaining rows by order.
    std::map<int, int> rowOf;
    if (target.kit) {
        int next = 0;
        std::vector<int> pitches;
        for (const auto& r : raw)
            if (std::find(pitches.begin(), pitches.end(), r.note) == pitches.end()) pitches.push_back(r.note);
        std::sort(pitches.begin(), pitches.end());
        for (int pitch : pitches) {
            const auto gm = std::find(kitNotes.begin(), kitNotes.end(), pitch);
            if (gm != kitNotes.end() && int(gm - kitNotes.begin()) < target.kitRows) rowOf[pitch] = int(gm - kitNotes.begin());
        }
        for (int pitch : pitches)
            if (rowOf.count(pitch) == 0) rowOf[pitch] = (next++) % std::max(1, target.kitRows);
    }
    for (const auto& r : raw) {
        Note n;
        n.step = int16_t(std::lround(r.beat * perBeat));
        if (n.step >= p.lengthSteps) continue;
        n.length = int16_t(std::max(1L, std::lround(r.length * perBeat)));
        n.velocity = uint8_t(std::clamp(r.velocity, 1, 127));
        if (target.kit) {
            n.degree = int8_t(rowOf[r.note]);
        } else {
            int degree = 0, alter = 0;
            degreeFor(target.mood, r.note - rootNote(target.root), snap, degree, alter);
            if (degree < -40 || degree > 40) continue;
            n.degree = int8_t(degree);
            n.alter = int8_t(alter);
        }
        p.add(n);
    }
    if (p.count == 0) return 0;
    out.copyFrom(p);
    return p.count;
}

template void appendNotes<Pattern>(juce::MidiMessageSequence&, const Pattern&, const MidiTarget&, double, int, double);
template void appendNotes<Clip>(juce::MidiMessageSequence&, const Clip&, const MidiTarget&, double, int, double);
template void appendNotes<Long>(juce::MidiMessageSequence&, const Long&, const MidiTarget&, double, int, double);
template juce::MidiFile toMidi<Pattern>(const Pattern&, const MidiTarget&, double);
template juce::MidiFile toMidi<Clip>(const Clip&, const MidiTarget&, double);
template juce::MidiFile toMidi<Long>(const Long&, const MidiTarget&, double);
template int fromMidi<Pattern>(const juce::MidiFile&, Pattern&, const MidiTarget&, bool, int, int);
template int fromMidi<Clip>(const juce::MidiFile&, Clip&, const MidiTarget&, bool, int, int);
template int fromMidi<Long>(const juce::MidiFile&, Long&, const MidiTarget&, bool, int, int);

} // namespace wb::pat
