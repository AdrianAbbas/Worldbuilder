#include "PatternMidi.h"
#include <map>

namespace wb::pat {

namespace {
constexpr int ticksPerBeat = 960;
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

juce::MidiFile toMidi(const Pattern& pattern, const MidiTarget& target) {
    juce::MidiMessageSequence track;
    track.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4), 0);
    track.addEvent(juce::MidiMessage::tempoMetaEvent(int(60000000.0 / std::clamp(double(target.bpm), 20.0, 999.0))), 0);
    const double perBeat = stepsPerBeat(pattern.resolution);
    for (int i = 0; i < pattern.count; ++i) {
        const auto& n = pattern.notes[std::size_t(i)];
        const double beat = double(n.step) / perBeat;
        int note;
        if (target.kit) {
            note = kitNotes[std::size_t(std::clamp(int(n.degree), 0, int(kitNotes.size()) - 1))];
        } else {
            const int chord = pattern.followChords ? music::chordDegree(beat, target.danger) : 0;
            note = rootNote(target.root) + semitonesFor(pattern, target.mood, chord, n.degree, n.alter);
        }
        if (note < 0 || note > 127) continue;
        const double on = beat * ticksPerBeat, off = (beat + double(n.length) / perBeat) * ticksPerBeat;
        track.addEvent(juce::MidiMessage::noteOn(1, note, juce::uint8(n.velocity)), on);
        track.addEvent(juce::MidiMessage::noteOff(1, note), off - 1.0);
    }
    track.updateMatchedPairs();
    juce::MidiFile file;
    file.setTicksPerQuarterNote(ticksPerBeat);
    file.addTrack(track);
    return file;
}

int fromMidi(const juce::MidiFile& source, Pattern& out, const MidiTarget& target, bool snap, int resolution) {
    juce::MidiFile file(source);
    const short format = file.getTimeFormat();
    // SMPTE files: convert through seconds at the target tempo.
    if (format <= 0) file.convertTimestampTicksToSeconds();
    struct Raw {
        double beat, length;
        int note, velocity;
    };
    std::vector<Raw> raw;
    for (int t = 0; t < file.getNumTracks(); ++t) {
        juce::MidiMessageSequence seq(*file.getTrack(t));
        seq.updateMatchedPairs();
        for (int i = 0; i < seq.getNumEvents(); ++i) {
            const auto* e = seq.getEventPointer(i);
            if (!e->message.isNoteOn()) continue;
            const double on = e->message.getTimeStamp();
            const double off = e->noteOffObject != nullptr ? e->noteOffObject->message.getTimeStamp() : on + (format > 0 ? format / 4.0 : 0.125);
            const double toBeats = format > 0 ? 1.0 / double(format) : double(target.bpm) / 60.0;
            raw.push_back({ on * toBeats, std::max(0.0, off - on) * toBeats, e->message.getNoteNumber(), e->message.getVelocity() });
        }
    }
    if (raw.empty()) return 0;
    std::sort(raw.begin(), raw.end(), [](const Raw& a, const Raw& b) { return a.beat < b.beat; });

    Pattern p;
    p.followChords = false;
    p.swing = out.swing;
    const double perBeat = stepsPerBeat(resolution);
    double end = 0;
    for (const auto& r : raw) end = std::max(end, r.beat + std::max(r.length, 1.0 / perBeat));
    const int bars = std::clamp(int(std::ceil(end / 4.0 - 1.0e-6)), 1, maxBars);
    p.setGrid(resolution, bars);

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
    out = p;
    return p.count;
}

} // namespace wb::pat
