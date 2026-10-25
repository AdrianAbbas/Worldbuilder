#pragma once
// Measured and published acoustic data the instrument models are built on: body
// resonances of plucked instruments, mode ratios of bars, bells, bowls, glass and
// membranes, sung-vowel formants. Sources and notes: docs/INSTRUMENT_NOTES.md.
#include <array>

namespace wb::dsp {

//==============================================================================
// Bodies of plucked and struck string instruments: a few resonant modes (frequency,
// Q, gain) a string's sound passes through, plus how much passes straight on.
struct BodyMode {
    float hz, q, gain;
};
struct BodySpec {
    std::array<BodyMode, 4> mode;
    float direct;
};
enum BodyType { BodyNone, BodyGuitar, BodyLute, BodyOud, BodyHarp, BodyZither, BodyHarpsichord, BodyCount };
inline constexpr std::array<BodySpec, BodyCount> bodies { {
    { { { { 100, 1, 0 }, { 200, 1, 0 }, { 400, 1, 0 }, { 2000, 1, 0 } } }, 1.0f },
    // Classical guitar: the coupled air (Helmholtz) and top-plate modes near 100 and
    // 200 Hz, the next plate mode near 400 Hz, a broad presence region.
    { { { { 98, 10, 0.9f }, { 204, 12, 0.75f }, { 390, 9, 0.45f }, { 2600, 1.3f, 0.3f } } }, 0.55f },
    // Lute: a smaller, lighter body, its lowest resonances higher (about 130 and 270 Hz).
    { { { { 132, 9, 0.55f }, { 275, 11, 0.65f }, { 540, 8, 0.45f }, { 2200, 1.5f, 0.3f } } }, 0.6f },
    // Oud: a deep bowl back, a warm low air mode, nasal upper mids.
    { { { { 110, 9, 0.8f }, { 240, 10, 0.6f }, { 820, 5, 0.4f }, { 1900, 2.0f, 0.35f } } }, 0.5f },
    // Harp: a soundboard, broad and low-Q.
    { { { { 150, 5, 0.4f }, { 330, 5, 0.5f }, { 700, 4, 0.3f }, { 1800, 1.2f, 0.2f } } }, 0.7f },
    // Zither: a shallow box.
    { { { { 205, 8, 0.5f }, { 420, 8, 0.5f }, { 900, 5, 0.35f }, { 3000, 1.5f, 0.25f } } }, 0.6f },
    // Harpsichord: a large thin soundboard, bright.
    { { { { 90, 7, 0.35f }, { 180, 7, 0.45f }, { 410, 5, 0.4f }, { 1250, 2.5f, 0.35f } } }, 0.65f },
} };

// The violin family's body (the violin's: the A0 air mode near 280 Hz, the B1- and B1+
// corpus modes near 460 and 550 Hz, the "bridge hill" between 2 and 3 kHz). A viola, a
// cello and a double bass scale it down.
inline constexpr std::array<BodyMode, 5> violinBody { { { 280, 14, 0.7f }, { 460, 16, 0.8f }, { 550, 16, 0.75f }, { 1100, 4, 0.25f }, { 2500, 1.3f, 0.55f } } };
enum StringSize { Violin, Viola, Cello, DoubleBass, StringSizeCount };
inline constexpr std::array<float, StringSizeCount> stringBodyScale { 1.0f, 0.78f, 0.45f, 0.28f };

//==============================================================================
// Mode ratios (to the sounding note) of struck objects.
// Church bell, the note being the prime (strike note): hum, prime, tierce (minor
// third), quint, nominal, then the upper partials.
inline constexpr std::array<float, 8> bellRatios { 0.5f, 1.0f, 1.2f, 1.5f, 2.0f, 2.5f, 2.67f, 3.0f };
// Uniform free bar (untuned glockenspiel, chimes): 1 : 2.76 : 5.40 : 8.93.
inline constexpr std::array<float, 4> freeBarRatios { 1.0f, 2.756f, 5.404f, 8.933f };
// Marimba (overtone tuned to two octaves, the next near 9.2), vibraphone (4, ~10),
// xylophone (a twelfth, 3).
inline constexpr std::array<float, 3> marimbaRatios { 1.0f, 3.99f, 9.2f };
inline constexpr std::array<float, 3> vibraphoneRatios { 1.0f, 3.98f, 10.6f };
inline constexpr std::array<float, 3> xylophoneRatios { 1.0f, 3.0f, 6.1f };
// Tibetan singing bowl (180 mm): 1 : 2.78 : 5.18 : 8.16 : 11.66.
inline constexpr std::array<float, 5> bowlRatios { 1.0f, 2.778f, 5.181f, 8.163f, 11.66f };
// Wine glass: 1 : 2.32 : 4.25 : 6.63.
inline constexpr std::array<float, 4> glassRatios { 1.0f, 2.32f, 4.25f, 6.63f };
// Cantilever tine (kalimba, music-box tooth, e-piano tine): 1 : 6.27 : 17.55.
inline constexpr std::array<float, 3> tineRatios { 1.0f, 6.267f, 17.55f };
// Steel pan note area: tuned 1 : 2 : 3 (octave and twelfth).
inline constexpr std::array<float, 4> steelPanRatios { 1.0f, 2.0f, 3.0f, 4.02f };

// Membranes. Ideal circular membrane (0,1) (1,1) (2,1) (0,2) (3,1) (1,2).
inline constexpr std::array<float, 6> membraneRatios { 1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f };
// Timpani, air loaded: the (1,1) (2,1) (3,1) (4,1) (5,1) modes near 1 : 1.5 : 1.98 : 2.44 : 2.94.
inline constexpr std::array<float, 5> timpaniRatios { 1.0f, 1.5f, 1.98f, 2.44f, 2.94f };

//==============================================================================
// Sung vowels: five formants (Hz, dB, bandwidth Hz) for bass, tenor, alto and
// soprano; vowels a, e, i, o, u.
struct Formant {
    float hz, db, bw;
};
using VowelFormants = std::array<Formant, 5>;
enum VoiceRange { VoiceBass, VoiceTenor, VoiceAlto, VoiceSoprano, VoiceRangeCount };
inline constexpr std::array<std::array<VowelFormants, 5>, VoiceRangeCount> vowelFormants { {
    { { // bass
        { { { 600, 0, 60 }, { 1040, -7, 70 }, { 2250, -9, 110 }, { 2450, -9, 120 }, { 2750, -20, 130 } } },
        { { { 400, 0, 40 }, { 1620, -12, 80 }, { 2400, -9, 100 }, { 2800, -12, 120 }, { 3100, -18, 120 } } },
        { { { 250, 0, 60 }, { 1750, -30, 90 }, { 2600, -16, 100 }, { 3050, -22, 120 }, { 3340, -28, 120 } } },
        { { { 400, 0, 40 }, { 750, -11, 80 }, { 2400, -21, 100 }, { 2600, -20, 120 }, { 2900, -40, 120 } } },
        { { { 350, 0, 40 }, { 600, -20, 80 }, { 2400, -32, 100 }, { 2675, -28, 120 }, { 2950, -36, 120 } } },
    } },
    { { // tenor
        { { { 650, 0, 80 }, { 1080, -6, 90 }, { 2650, -7, 120 }, { 2900, -8, 130 }, { 3250, -22, 140 } } },
        { { { 400, 0, 70 }, { 1700, -14, 80 }, { 2600, -12, 100 }, { 3200, -14, 120 }, { 3580, -20, 120 } } },
        { { { 290, 0, 40 }, { 1870, -15, 90 }, { 2800, -18, 100 }, { 3250, -20, 120 }, { 3540, -30, 120 } } },
        { { { 400, 0, 40 }, { 800, -10, 80 }, { 2600, -12, 100 }, { 2800, -12, 120 }, { 3000, -26, 120 } } },
        { { { 350, 0, 40 }, { 600, -20, 60 }, { 2700, -17, 100 }, { 2900, -14, 120 }, { 3300, -26, 120 } } },
    } },
    { { // alto
        { { { 800, 0, 80 }, { 1150, -4, 90 }, { 2800, -20, 120 }, { 3500, -36, 130 }, { 4950, -60, 140 } } },
        { { { 400, 0, 60 }, { 1600, -24, 80 }, { 2700, -30, 120 }, { 3300, -35, 150 }, { 4950, -60, 200 } } },
        { { { 350, 0, 50 }, { 1700, -20, 100 }, { 2700, -30, 120 }, { 3700, -36, 150 }, { 4950, -60, 200 } } },
        { { { 450, 0, 70 }, { 800, -9, 80 }, { 2830, -16, 100 }, { 3500, -28, 130 }, { 4950, -55, 135 } } },
        { { { 325, 0, 50 }, { 700, -12, 60 }, { 2530, -30, 170 }, { 3500, -40, 180 }, { 4950, -64, 200 } } },
    } },
    { { // soprano
        { { { 800, 0, 80 }, { 1150, -6, 90 }, { 2900, -32, 120 }, { 3900, -20, 130 }, { 4950, -50, 140 } } },
        { { { 350, 0, 60 }, { 2000, -20, 100 }, { 2800, -15, 120 }, { 3600, -40, 150 }, { 4950, -56, 200 } } },
        { { { 270, 0, 60 }, { 2140, -12, 90 }, { 2950, -26, 100 }, { 3900, -26, 120 }, { 4950, -44, 120 } } },
        { { { 450, 0, 70 }, { 800, -11, 80 }, { 2830, -22, 100 }, { 3800, -22, 130 }, { 4950, -50, 135 } } },
        { { { 325, 0, 50 }, { 700, -16, 60 }, { 2700, -35, 170 }, { 3800, -40, 180 }, { 4950, -60, 200 } } },
    } },
} };

} // namespace wb::dsp
