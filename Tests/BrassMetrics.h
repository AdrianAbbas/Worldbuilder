#pragma once
// Copied from Dungeon-Crawler (Tests/BrassMetrics.h, read only; the brass pass): how hard a brass
// note hits, measured on a stereo render. Plain C++: no JUCE, no plug-in types.
//   attackToSustainDb  the loudest 5 ms RMS frame of the first 200 ms after the onset
//                      against the RMS of 0.8-1.3 s (the sustain), in dB (LIGHT SEEKER's
//                      `--brass-report` measure)
//   riseMs             10 % to 90 % of the peak (2 ms frames, the first frame reaching each), ms
//   hitMs              from the onset to the end of the first 2 ms frame within 3 dB of the
//                      peak (first 300 ms), in ms
//   sustainDb          the sustain's RMS (dBFS)
//   peakDb             the sample peak of the whole render (dBFS)
//   presenceDb         the 2-4 kHz share of the first 150 ms (dB of the total)
//   edgeDb             the 2-4 kHz share of the first 30-120 ms against the sustain's
//                      share: how much brighter the attack is than the held note
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace brassmetrics {

struct Render {
    const std::vector<float>& l;
    const std::vector<float>& r;
    double sr;
    std::size_t onset;   // the first note-on (samples)
};

struct Result { double attackToSustainDb = 0, riseMs = 0, hitMs = 0, sustainDb = -200, peakDb = -200, presenceDb = -200, edgeDb = 0; };

inline double toDb(double x) { return 20.0 * std::log10(std::max(x, 1.0e-10)); }

inline std::vector<double> envelope(const Render& p, double frameMs, double seconds) {
    const auto frame = std::max<std::size_t>(1, static_cast<std::size_t>(frameMs * 0.001 * p.sr));
    std::vector<double> env;
    const auto end = std::min(p.l.size(), p.onset + static_cast<std::size_t>(seconds * p.sr));
    for (std::size_t i = p.onset; i + frame <= end; i += frame) {
        double sum = 0;
        for (std::size_t k = i; k < i + frame; ++k) sum += 0.5 * (double(p.l[k]) * p.l[k] + double(p.r[k]) * p.r[k]);
        env.push_back(std::sqrt(sum / double(frame)));
    }
    return env;
}

inline double rmsBetween(const Render& p, double from, double to) {
    double sum = 0;
    const auto a = std::min(p.l.size(), p.onset + static_cast<std::size_t>(from * p.sr));
    const auto b = std::min(p.l.size(), p.onset + static_cast<std::size_t>(to * p.sr));
    for (std::size_t k = a; k < b; ++k) sum += 0.5 * (double(p.l[k]) * p.l[k] + double(p.r[k]) * p.r[k]);
    return std::sqrt(sum / double(std::max<std::size_t>(1, b - a)));
}

// A 2-pole band-pass (RBJ, constant 0 dB peak) for the 2-4 kHz share.
struct BandPass {
    double b0 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    BandPass(double sr, double hz, double q) {
        const double w = 2.0 * 3.14159265358979 * hz / sr, alpha = std::sin(w) / (2.0 * q), a0 = 1.0 + alpha;
        b0 = alpha / a0;
        b2 = -alpha / a0;
        a1 = -2.0 * std::cos(w) / a0;
        a2 = (1.0 - alpha) / a0;
    }
    double tick(double x) {
        const double y = b0 * x + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = x;
        y2 = y1;
        y1 = y;
        return y;
    }
};

// The 2-4 kHz share (dB of the total) between from and to seconds after the onset.
inline double bandShare(const Render& p, double from, double to) {
    BandPass bl(p.sr, 2830.0, 1.0), br(p.sr, 2830.0, 1.0);
    const auto a = std::min(p.l.size(), p.onset + static_cast<std::size_t>(from * p.sr));
    const auto b = std::min(p.l.size(), p.onset + static_cast<std::size_t>(to * p.sr));
    double band = 0, all = 0;
    for (std::size_t i = 0; i < b; ++i) {
        const double x = bl.tick(p.l[i]), y = br.tick(p.r[i]);
        if (i >= a) {
            band += x * x + y * y;
            all += double(p.l[i]) * p.l[i] + double(p.r[i]) * p.r[i];
        }
    }
    return 10.0 * std::log10(std::max(band, 1.0e-20) / std::max(all, 1.0e-20));
}

inline Result measure(const Render& p) {
    Result m;
    const auto e = envelope(p, 5.0, 0.2);
    double peak = 0;
    for (double v : e) peak = std::max(peak, v);
    const double sustain = rmsBetween(p, 0.8, 1.3);
    m.sustainDb = toDb(sustain);
    m.attackToSustainDb = toDb(peak) - m.sustainDb;
    // The rise: from the first 2 ms frame at 10 % of the peak (first 300 ms) to the first at
    // 90 % (LIGHT SEEKER's measure).
    const auto fine = envelope(p, 2.0, 0.3);
    double top = 0;
    for (double v : fine) top = std::max(top, v);
    std::size_t a = 0;
    while (a < fine.size() && fine[a] < 0.1 * top) ++a;
    std::size_t b = a;
    while (b < fine.size() && fine[b] < 0.9 * top) ++b;
    m.riseMs = 2.0 * double(b - a);
    // The hit's arrival: from the onset to the first 2 ms frame within 3 dB of the peak
    // (steadier than 10-90 % where detuned layers beat on the plateau).
    std::size_t c = 0;
    while (c < fine.size() && fine[c] < 0.708 * top) ++c;
    m.hitMs = 2.0 * double(c + 1);
    double pk = 0;
    for (std::size_t i = 0; i < p.l.size(); ++i) pk = std::max({ pk, double(std::abs(p.l[i])), double(std::abs(p.r[i])) });
    m.peakDb = toDb(pk);
    m.presenceDb = bandShare(p, 0.0, 0.15);
    m.edgeDb = bandShare(p, 0.03, 0.12) - bandShare(p, 0.8, 1.3);
    return m;
}

// The listening phrase (both plug-ins): a horn call at 100 bpm. Seconds, MIDI note, velocity.
struct PhraseNote { double at, length; int key; float velocity; };
inline std::vector<PhraseNote> hornPhrase() {
    constexpr double b = 0.6;   // a beat at 100 bpm
    std::vector<PhraseNote> notes;
    // Two short calls (G - C - E - G), then three chord stabs, then a held chord.
    const int call[4] { 55, 60, 64, 67 };
    for (int rep = 0; rep < 2; ++rep)
        for (int k = 0; k < 4; ++k) notes.push_back({ rep * 2.0 * b * 2 + k * 0.5 * b, 0.42 * b, call[k], k == 0 ? 0.95f : 0.8f });
    const double stabs = 4.0 * 2 * b;
    const int chords[3][3] { { 60, 64, 67 }, { 57, 60, 65 }, { 59, 62, 67 } };
    for (int s = 0; s < 3; ++s)
        for (int key : chords[s]) notes.push_back({ stabs + s * b, 0.3 * b, key, 0.95f - 0.1f * float(s) });
    for (int key : { 48, 55, 60, 64, 67 }) notes.push_back({ stabs + 4 * b, 4 * b, key, 0.9f });
    return notes;
}
inline double hornPhraseSeconds() { return 4.0 * 2 * 0.6 + 8 * 0.6 + 2.5; }

} // namespace brassmetrics
