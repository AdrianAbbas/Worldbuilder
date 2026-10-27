# Sample preparation (Milestone 15)

`prepare.py` turns the developer's recordings (MP3s in the root of the private
`AdrianAbbas/Samples` repository) into what WORLDBUILDER's sampler loads: mono 16-bit
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
