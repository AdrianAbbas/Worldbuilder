# Instrument notes (Milestone 6)

How the shared instrument models are built and the acoustic numbers behind them. The
code is in `Source/DSP/Voices.h`, `Source/DSP/Instruments.h` (voices),
`Source/DSP/InstrumentData.h` (the tables below) and `Source/Places/Ensemble.h` (the
parts that play them). Synthesis stays the engine: no samples ship.

Numbers marked *(web)* were checked against the sources listed with them during this
milestone. Numbers marked *(ref)* come from the standard references named, from
memory of their tables, and were not re-checked online here: treat those as the
first thing to verify against a recording.

## Plucked strings: lute, oud, guitar, harp, zither, harpsichord

**Model** (`dsp::WaveguideString`, `dsp::Pluck`, `PluckPart`): a digital waveguide.
- One delay loop per string with a one-pole loss filter (upper partials die faster),
  a stiffness all-pass (1–2 first-order sections: upper partials slightly sharp) and
  a first-order fractional-delay all-pass whose coefficient is solved exactly at the
  note's frequency, `c = sin(w(1-d)/2) / sin(w(1+d)/2)`, after subtracting the phase
  delays of the loss and stiffness filters at that frequency. Measured: every
  fundamental from 82 Hz to 1.3 kHz within 0.05 cent.
- The pluck is the force released at the pluck point: a zero-mean pulse whose split
  point is the pluck position (its spectrum has the comb `sin(n pi beta)` that real
  plucks have), softened by a one-pole whose cutoff rises with brightness and
  velocity (finger to plectrum, soft to hard).
- Nail, pick or quill noise outside the string (3.5 ms), velocity-scaled.
- Courses: a second string a few cents sharp, plucked 1.2 ms later (lute, oud,
  harpsichord's two 8' choirs).
- A damper (harpsichord jack): when the note ends the loop's gain and brightness drop.
- Higher notes decay sooner: T60 scaled by `(220 / f)^0.35`.
- Part level: the instrument's **body** (four resonant modes over a direct path) and
  two **sympathetic open strings** (root and fifth, an octave down) driven quietly by
  everything played.

**Body resonances** (`dsp::bodies`):

| Body | Modes (Hz) | Basis |
|---|---|---|
| Guitar | 98, 204, 390, broad 2.6 k | Coupled air (Helmholtz) and top-plate modes near 100 and 200 Hz; a laser-vibrometer study measured 96, 196, 294 Hz; uncoupled ~130 Hz air, ~180 Hz top *(web)* |
| Lute | 132, 275, 540, broad 2.2 k | A smaller, lighter body: the guitar's pattern moved up *(estimate)* |
| Oud | 110, 240, 820, broad 1.9 k | Deep bowl: a strong low air mode, nasal upper mids *(estimate)* |
| Harp | 150, 330, 700, broad 1.8 k | Soundboard: broad, low Q *(estimate)* |
| Zither | 205, 420, 900, broad 3 k | Shallow box *(estimate)* |
| Harpsichord | 90, 180, 410, 1.25 k | Large thin soundboard *(estimate)* |

Sources: B.E. Richardson et al., *Mode placement in guitars revisited* and
*Investigations of mode coupling in the guitar* (Institute of Acoustics proceedings);
DAGA 2021 laser-vibrometer guitar study; Delcamp forum notes by A. Carruth (informal).
Inharmonicity: for guitar strings B is about 1e-5 (5th partial ~0.2 cent sharp), for
piano strings about 1e-4 to 1e-3 *(ref: Fletcher & Rossing, The Physics of Musical
Instruments)*; the stiffness all-pass gives near-zero stretch on low notes and a few
cents on the 5th partial above ~800 Hz, in that range.

## Bowed strings: fiddle, string sections, double basses, hurdy-gurdy

**Model** (`dsp::StringVoice`, `BowedPart` model String):
- Helmholtz motion: the bridge force is a sawtooth (band-limited), combed by the bow
  position (harmonics near multiples of 1/beta weak), its corner rounded by pressure
  (a low-pass at 6–40 times the note, opening with brightness and level).
- Bow noise synchronous with the slip (bursts as each sawtooth period falls back),
  extra scrape at the start of a stroke, the pitch settling from 6–12 cents flat in
  ~60 ms, vibrato (semitones) coming in after 0.25 s.
- Sections: up to three players, a few cents apart, starting 12–60 ms apart, each with
  their own vibrato rate.
- Part level: the violin family's **body**, scaled for viola (0.78), cello (0.45) and
  double bass (0.28); the bridge hill moves less (square root of the scale).

| Violin body mode | Hz | Basis |
|---|---|---|
| A0 (air) | 280 | 278–292 Hz measured on several violins *(web)* |
| B1- (corpus) | 460 | 424–487 Hz *(web)* |
| B1+ (corpus) | 550 | 555–575 Hz *(web)* |
| (mid) | 1100 | broad, low *(estimate)* |
| Bridge hill | 2500, broad | a gentle hill between 2 and 3 kHz; present on Italian violins, not on all *(web)* |

Sources: J. McLennan, *The violin* (UNSW thesis, music acoustics pages); Jansson,
Barczewski & Kabała, *On the violin* (2016); KTH Speech, Music and Hearing notes;
Acta Acustica 2024 simulation-vs-measurement table. Bowed-string physics: Helmholtz
motion and the bow-position comb *(ref: Cremer, The Physics of the Violin;
Woodhouse)*. A full friction-curve bowed waveguide was not used: it is hard to keep
stable over the whole range unattended, and the shaped Helmholtz model gives the same
spectral facts.

The older **bowed resonator** (a bow driving a few tuned modes) stays for bowed
metal, singing bowls rubbed, glassy hums and the spaceship hull.

## Brass: horns, trumpets, bugle, trombones

**Model** (`dsp::SynthVoice`, model Brass):
- A lip buzz (sawtooth, optionally a second player) through a key-tracked low-pass
  whose cutoff rises steeply with how hard it is blown: `f0 x (1.3 + 16 x
  (level x velocity)^1.4)`. Loud, a soft-clipping stage adds the "brassy" edge.
- The attack: the lips find the note from below (up to ~35 cents flat, settling in
  ~30 ms with a slight overshoot) and a burst of buzz noise (the "blat").
- The bell and mouthpiece formant: French horn ~420 Hz, low brass ~520 Hz, bugle
  ~1100 Hz, trumpet ~1300 Hz.

Basis: trombone measurements show the first harmonic dominating at pp and the
strongest component moving to the 4th harmonic at ff, harmonics extending far up;
non-linear propagation in the bore is perceived as brightness at moderate dynamics
and as a brassy "cuivré" sound at high dynamics, most in narrow-bore instruments
*(web: Campbell; Stevenson & Campbell, Institute of Acoustics proceedings; Forum
Acusticum 2023)*. Lip-model starting transients agree best with a lip Q near 5
*(web)*; the 30 ms settle here is a perceptual stand-in.

## Free reeds: concertina, squeezebox (accordion)

**Model** (`dsp::SynthVoice`, model Reed): each reed a narrow pulse (a saw minus the
same saw ~0.31 of a period on), two reeds per note a few cents apart beating
(musette), the reed chamber's formant (1.7–2 kHz), a valve click and the bellows' air.
The musette beat (2–6 Hz at these detunes) is the accordion's signature
*(ref: Fletcher & Rossing, free reeds)*.

## Woodwinds: flute, recorder, ney

**Model** (`dsp::WindVoice`): an air jet against an edge. A near-sine with up to five
harmonics that grow as it is blown harder (and the pitch rises ~4 cents), a chiff at
the start (a burst of noise at ~2.5x the note: the jet briefly overblowing), breath
turbulence tuned to the note plus broad air above 1.2 kHz, vibrato that is mostly a
pulse in the breath (a third in pitch), and a slow wander of a few cents. With tone
0 the voice is breath alone (rustles, gusts, calls). *(ref: Fletcher & Rossing,
flutes; Coltman on flute vibrato being mostly amplitude.)*

## Struck modal objects: bells, bars, bowls, gongs, glass, tines

**Model** (`dsp::Bell`, `ModalPart`): up to 8 two-pole modes struck by a raised-cosine
of 0.15–4 ms (hard to soft; velocity makes it harder), with extras: doublets on the
lowest three modes (beating), a pitch glide after the strike (gongs), a bloom of the
upper modes (tam-tam), and strike-to-strike variation of the mode strengths.

| Object | Mode ratios | Basis |
|---|---|---|
| Church bell (to the prime) | hum 0.5, prime 1, tierce 1.2, quint 1.5, nominal 2, then 2.5, 2.67, 3.0 | Ideal ratios 0.5 : 1 : 1.2 : 1.5 : 2; a Liberty Bell replica measured 0.54, 1.00, 1.18, 1.58, 1.93 *(web: Hibberts; Perrin, Charnley & DePont 1982; Physics Today)* |
| Free bar (glockenspiel) | 1 : 2.756 : 5.404 : 8.933 | Uniform bar free at both ends *(web: CCRMA percussion notes)* |
| Marimba | 1 : 3.99 : 9.2 | Second partial tuned to two octaves, third near 9.2 *(web: CCRMA)* |
| Vibraphone | 1 : 3.98 : ~10.6 | First overtone two octaves up *(web: CCRMA; Yamaha)* |
| Xylophone | 1 : 3 : ~6 | First overtone a twelfth *(web: CCRMA)*; third partial *(estimate)* |
| Singing bowl (180 mm) | 1 : 2.78 : 5.18 : 8.16 : 11.66 | Csound table; ranges 2.7–2.9, 4.8–5.7 across bowls *(web: Archives of Acoustics 2016; Terwagne & Bush)*; split doublets beat *(web)* |
| Wine glass | 1 : 2.32 : 4.25 : 6.63 | *(ref: Csound modal-frequency table)* |
| Tine (kalimba, e-piano, music box) | 1 : 6.27 : 17.55 | Cantilever beam modes *(ref)* |
| Steel pan note | 1 : 2 : 3 | Note areas tuned to octave and twelfth *(ref: Rossing)* |

Gongs: the pitch glide after a strike (up or down, a fraction of a semitone here) and
the tam-tam's delayed shimmer are from the percussion literature *(ref: Rossing,
Science of Percussion Instruments; not re-checked)*.

## Membrane drums: timpani, frame drums, toms, snare, bass drum

**Model** (`dsp::Drum`, kinds Membrane and Timpani):
- Ideal circular membrane modes 1 : 1.594 : 2.136 : 2.296 : 2.653 : 2.918 (Bessel
  zeros; the 1.593 of the (1,1) mode confirmed *(web)*).
- Timpani: air loading pulls the (1,1), (2,1), (3,1)... modes toward a harmonic
  series, 1 : 1.5 : 1.98 : 2.44 : 2.94 *(ref: Rossing; the move toward harmonic ratios
  confirmed by the Durham and Purdue theses (web))*.
- Strike position: the centre favours the lowest mode, the edge the others; stick
  hardness from felt (6 ms) to wood (0.4 ms), harder when louder.
- Tension modulation: a hard hit starts sharp and falls (the existing bend).
- Snare wires: band-limited noise that rattles with the skin's motion, ~0.22 s.

## Choir

**Model** (`dsp::ChoirVoice`): three singers per voice, each a band-limited glottal
source (a sawtooth with a two-pole spectral tilt, ~12 dB/octave above a few hundred Hz)
with its own pitch offset, slow pitch jitter and vibrato; a five-formant vocal tract
shared by the section; aspiration noise gated by the glottal cycle.

Formants (`dsp::vowelFormants`): frequency, level and bandwidth of five formants for
bass, tenor, alto and soprano, vowels a, e, i, o, u, as in the Csound manual's
"Formant values" table *(ref; a partial copy was seen online, the original page was
unreachable)*. The voice range is chosen by the note (bass < 165 Hz < tenor < 290 Hz
< alto < 440 Hz < soprano). Sung-vowel formants converge at high pitch *(web:
koreascience soprano study)*: not modelled.

## Keys

- **Tine electric piano** (`dsp::TineVoice`, `FmPart` model Tine): a felt hammer on a
  tine (fundamental and its cantilever modes 6.27 and 17.55, which ring 0.12 s and
  0.04 s: the bell of the attack) next to a tone bar sustaining the fundamental; an
  asymmetric pickup adds second harmonic with the swing (the bark); a damper on note
  end.
- **Harpsichord**: the plucked string with the quill near the end (beta 0.07), two
  unison choirs, the jack's damper, the harpsichord soundboard body.
- **Music box**: a modal comb tooth (tine ratios) — set per Place.
- **Accordion / concertina**: the free reed above.

## Validation in the tests

`--only models` checks every rebuilt model directly (no Place): in tune (the
fundamental's pitch, partials where the tables put them), finite, starting without a
click, and louder **and** brighter at higher velocity.
