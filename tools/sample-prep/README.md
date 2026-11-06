# Sample preparation (Milestone 15)

`prepare.py` turns the developer's recordings (MP3s and WAVs anywhere in the private
`AdrianAbbas/Samples` repository, in its root or in subfolders such as `01-voice/` or
`02-piano/`) into what WORLDBUILDER's sampler loads: mono 16-bit
FLAC notes and one mapping file per instrument. The recordings and the prepared notes
never go into this public repository: CI fetches the recordings with a token
(`scripts/ci-fetch-samples.sh`) and the output lives in the build folder.

    pip install -r tools/sample-prep/requirements.txt
    git clone https://github.com/AdrianAbbas/Samples ../samples      # needs access
    python3 tools/sample-prep/prepare.py --source ../samples --out build/samples
    cmake -S . -B build -DWORLDBUILDER_SAMPLES_DIR=build/samples ...

`manifest.json` says what each recording contains (from the Samples repository's
`notes.txt`); the script checks it against the audio and prints a report (also written
to `report.json` in the output). Rerun it whenever recordings are added or changed; a new
recording needs a manifest entry.

What it does, per recording:

1. **MP3 padding**: GarageBand's MP3s carry Apple's `iTunSMPB` tag (encoder delay,
   padding, sample count); the LAME header is used when there is one instead. The
   encoder delay plus the decoder's 529 samples are taken off the start, the padding off
   the end, and trailing digital silence is dropped.
2. **Noise reduction (gentle)**: the room tone's spectrum, fitted per third of an octave to
   the recording's own quiet gaps, comes off with a soft spectral gate (at most 9 dB,
   smoothed so it does not warble).
3. **Slicing**: onsets from the level's rise; the expected notes are assigned in order by
   how clearly each expected pitch sounds in each onset (handling noises and finger clicks
   fall out and never shift the labels after them).
4. **Pitch**: YIN within 8 semitones of the label (strings ringing in sympathy can be louder
   than a high note), refined on the harmonics, octave-checked; the sampler is tuned to
   what was played. Steady tones well below a note (other strings ringing in sympathy) are
   notched out, and notes above 250 Hz get a gentle high-pass at half their pitch.
5. **Levels**: velocity layers recorded at different input gains are levelled by their
   noise floors (the room is the same in all of them), then each instrument gets one gain
   (its loudest peak at -1 dBFS), so the layers keep their dynamics.
6. **Loops and release tails** for sustained notes: the best-matching loop in the sustain
   (crossfaded) and the point where the bow lifts (the recorded tail plays on release).
7. **Output**: `<id>/<note>-<layer>-<take>.flac`, `<id>.json` (key ranges, velocity layers,
   round-robins, loop points, release points, loudness), `index.json`, `report.json`.

**Synth sweeps** (`sweeps.py`, Milestone 15b; manifest sets with `"kind": "synth"`): the
Korg Minilogue patches, each a WAV of all 37 keys played lowest to highest, one note at a
time, once with short hits and once with long (held) hits. Per recording:

1. **Decode**: 24-bit WAV, dual mono (one channel's worth), a 10 Hz high-pass for DC (the
   lowest patch goes down to 16 Hz). No noise reduction (a line input).
2. **Slicing** by the gaps between notes: a start is a rise out of the quiet before it by
   22 dB, relaxed step by step until all 37 are found (a pad's release can overlap the
   next note); with too many, the smallest rises (a pulse inside a note) go first.
3. **Pitch**: a first pass (YIN down to 14 Hz) for every note; the sweep's order then
   gives each note its key (the median offset, so the octave switch and the patch's own
   octaves do not matter), and each note is measured within 3 semitones of its key: the
   pitch its first six harmonics agree on (each at its power-weighted centroid, weighted
   by power), since a patch's oscillators are not one harmonic series. Every note
   is mapped at its own pitch; the sampler tunes it by the cents it is out. Notes the
   first pass heard elsewhere, more than 25 cents out or wandering are reported.
4. **Long hits**: the release point (the knee of the final fall), a loop early in the
   hold (up to 1.2 s; waveform and envelope matched so a pulsing patch keeps its pulse;
   crossfaded and levelled), and the rest of the hold cut out: each note is its attack,
   its loop and its recorded release.
5. **Trim and level**: every note ends once it has fallen 50 dB under its peak; one gain
   per patch for both articulations (they were recorded at the same gain), loudest
   peak at -1 dBFS.
6. **Mapping**: `mlg-<patch>-short.json` and `mlg-<patch>-long.json`, every note on its own
   key; only the lowest and highest notes stretch a fifth to fill the ends of the range
   (the sampler folds anything further by octaves). The report checks that the short
   hits really are the shorter (the static-y bass's two files are named the other way
   round: the manifest assigns them by what they contain).

**Finding the recordings** (Milestone 17): a manifest entry names a file; it is looked for
at that path, then by the same name (case, spacing and extension aside) in any subfolder,
so the recordings can be moved into folders or re-exported as WAVs without touching the
manifest. Every audio file the manifest does not mention is reported. At the end the log
says, per sample set, `Prepared: N notes (set: instruments)` (CI repeats these lines as
notices).

**Piano** (`chromatic.py`, `"kind": "piano"`): four sweeps of all 88 keys (three of short
hits, soft / medium / full; one of held notes at medium strength).

1. Onsets: every rise in the spectrum is a candidate; each key is assigned, in order, the
   candidate where its own partials rise most (dampers, pedal thumps and re-strikes never
   shift the labels); then refined to the hammer's attack.
2. The previous key ringing on into this one: where it is still within 45 dB, its partials
   are fitted just before the onset and the extrapolated tail subtracted.
3. Pitch: each key's inharmonic partials (f_n = n f0 sqrt(1 + B n^2)) give f0 and B. The
   stretch curve across the keys (a running median) is kept; its offset from A440 (the
   median over F3-F5) is taken off, and keys more than 8 cents off the curve (drifted
   unisons) are pulled onto it.
4. Short hits end once faded (the damper is recorded). Held notes: the damper point (where
   the decay steepens) is found; beyond it the note's own partials, fitted on the last half
   second of the hold, carry the natural decay on (additive resynthesis, crossfaded in
   phase), so a key held longer than it was recorded rings on and dies away. No loops.
5. Bad keys (no onset, barely struck, far louder, no pitch) are replaced by the nearest
   good key of the layer (the sampler shifts it) and reported.
6. Levels: each file's gain from its noise floor (the exports were normalised one by one);
   the layer steps smoothed across the keys; every layer's attack aligned to the medium
   take's (cross-correlation, so crossfaded layers do not comb-filter); the held notes
   matched to the medium short hits. The soft and full held notes are the medium ones with
   a gain and a high shelf measured from the short layers (derived zones share the audio).

**Voice** (`voice.py`, `"kind": "voice"`): vowel sweeps (soft / full layers, the falsetto
on top), choir stacks (five or six takes of a note: round-robins the ensemble draws its
singers from), drones, the chant, the grit, the hummed melody, the ghostly falsetto, and
textures (the whispering takes as long looped zones; breaths and consonants cut one by one).
Pitches come from the audio: the strongest harmonic series, moved an octave down while the
lower octave's odd harmonics are there, and in a sweep the notes' order settles the octave
(the chest sweeps sound an octave above their file names; reported). Loops keep clear of
the first 0.6 s (the scoop into the note) and are tuned to where their harmonics carry the
most energy.

**Hits** (`hits.py`, `"kind": "hits"`): the djembes, one file per stroke with three hits
per dynamic. The nine loudest onsets are the hits; they are layered by how loud they sound
(the recorded order is reported where it disagrees); the three takes of a layer are
round-robins; every stroke sits on its own key of one `djembe` instrument.

**Banjo** (`"kind": "pluck"`, as the guitar): the three sweeps with `"declip"` (the hard
and palm-muted sweeps clipped while recording: flat tops rebuilt by a spline, reported by
note) and `"extra"` files (the open fifth string, nine takes: shared out to the layers by
loudness as round-robins). A note played on another pitch than its label is filed under
the pitch it sounds.

**Orchestral percussion** (`vsco.py`, the level and drums pass): the Drama drums' bass drums,
large drum, tenor drums, snare, timpani, cymbals and gong from VS Chamber Orchestra 2:
Community Edition 1.1.0 (Versilian Studios, CC0 1.0; `docs/LICENSES/VSCO-2-CE.md`), fetched
from its official repository at a pinned commit by `scripts/ci-fetch-vsco.sh` (a sparse
checkout of only the files mapped, its licence checked; CI caches it) and prepared after
`prepare.py` into the same folder as one `orchperc` instrument: each sound on its own key,
four velocity layers (pp, mp, mf, ff) from the recorded dynamics, the takes as round-robins,
the two mic channels summed to mono, a 25-30 Hz high-pass, cut where they have fallen 60 dB,
each sound's loudest take at -1 dBFS (the dynamics as recorded).
