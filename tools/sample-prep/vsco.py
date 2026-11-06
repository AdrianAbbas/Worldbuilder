"""Orchestral percussion and woodwinds from VSCO 2: Community Edition (the level and drums pass,
the woodwinds pass: vsco_woodwinds.py).

VSCO 2 CE (Versilian Studios, https://github.com/sgossner/VSCO-2-CE, 1.1.0) is CC0 1.0
(docs/LICENSES/VSCO-2-CE.md). CI fetches the files below from the official repository at a
pinned commit (scripts/ci-fetch-vsco.sh); they are never committed here.

    python3 tools/sample-prep/vsco.py --source <VSCO-2-CE checkout> --out <prepared samples>

Run after prepare.py (it adds one instrument, `orchperc`, to the same output folder and its
index.json). Every sound sits on its own key (like the djembe kit), in four velocity layers
(pp, mp, mf, ff; crossfaded by the sampler) with the recorded takes as round-robins:

  36 bass drum (concert, 7 dynamics x 2 takes)     37 bass drum, muted (tight)
  38 large drum, mallet (taiko-like)               39 large drum, sticks
  41-45 timpani 1 (lowest) to 5                    48 snare (snares on)
  50 snare taps (ghost strokes)                    52 tom high (tenor drum)
  53 tom low (tenor drum)                          56 crash cymbal
  57 suspended cymbal (mallet)                     58-60 suspended cymbal swells (short, medium, long)
  61 gong                                          62 clash cymbals

Per file: decoded (44.1 kHz, the two mic channels summed to mono), a 25 Hz high-pass (the
bass drums 30 Hz: the room's rumble, not the drum), the attack found (the level rising out
of the silence; 2 ms kept before it), cut where it has fallen 60 dB under its peak (at most
the sound's own length limit) with a cosine fade. Levels: each sound keeps its recorded
dynamics (VSCO recorded each instrument's dynamics at one gain); the sound's loudest take
peaks at -1 dBFS. The loudness of each take (K-weighted, its loudest 400 ms) is in the map,
so the sampler can even the takes within a layer.
"""

import argparse
import glob
import json
import os
import re
import sys

import numpy as np
import soundfile as sf
from scipy import signal

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import prepare as P  # noqa: E402
import vsco_woodwinds as W  # noqa: E402

RATE = P.RATE
LAYERS = ["pp", "mp", "mf", "ff"]
VELOCITY = [[0.0, 0.3], [0.3, 0.55], [0.55, 0.8], [0.8, 1.0]]

# Each sound: its key, the files (a glob under the checkout), how a file's name gives its
# dynamic (a regex group) and how the dynamics map to the four layers, the longest it may
# ring and its high-pass.
DRUM_DYN = {"ppp": 0, "pp": 0, "p": 1, "mp": 1, "mf": 2, "f": 2, "ff": 3, "fff": 3}
SOUNDS = [
    {"name": "bass-drum", "key": 36, "glob": "Percussion/BDrumNewhit_v*_rr*_Sum.wav", "dyn": r"_v(\d)_",
     "layers": {"1": 0, "2": 1, "3": 1, "4": 2, "5": 2, "6": 3, "7": 3}, "max": 3.0, "hp": 30},
    {"name": "bass-drum-muted", "key": 37, "glob": "VSCO 1 Percussion/drums/bass/bdrum_muted_*.wav", "dyn": r"muted_([a-z]+)_",
     "layers": DRUM_DYN, "max": 1.5, "hp": 30},
    {"name": "large-drum-mallet", "key": 38, "glob": "VSCO 1 Percussion/drums/other/ethnic/giant/mallet/EthnicLargeMallet_hit_*.wav",
     "dyn": r"hit_([a-z]+)_", "layers": DRUM_DYN, "max": 2.5, "hp": 30},
    {"name": "large-drum-sticks", "key": 39, "glob": "VSCO 1 Percussion/drums/other/ethnic/giant/sticks/EthnicLargeSticks_hit_*.wav",
     "dyn": r"hit_([a-z]+)_", "layers": DRUM_DYN, "max": 2.0, "hp": 30},
    {"name": "snare", "key": 48, "glob": "Percussion/Snare2-HitSN_v*_rr*_Sum.wav", "dyn": r"_v(\d)_",
     "layers": {"1": 0, "3": 1, "5": 2, "7": 3, "9": 3}, "max": 1.2, "hp": 60},
    {"name": "snare-taps", "key": 50, "glob": "Percussion/Snare2-taps_v*_rr*_Sum.wav", "dyn": r"_v(\d)_",
     "layers": {"1": 0, "2": 1, "3": 2, "4": 3}, "max": 0.8, "hp": 80},
    {"name": "tom-high", "key": 52, "glob": "VSCO 1 Percussion/drums/tenor/tenor_higher/tenorH_*.wav", "dyn": r"tenorH_([a-z]+)_",
     "layers": DRUM_DYN, "max": 1.6, "hp": 40},
    {"name": "tom-low", "key": 53, "glob": "VSCO 1 Percussion/drums/tenor/tenor_lower/tenor_*.wav", "dyn": r"tenor_([a-z]+)_",
     "layers": DRUM_DYN, "max": 1.8, "hp": 35},
    {"name": "crash", "key": 56, "glob": "Percussion/cymbal-crash1_*_rr*.wav", "dyn": r"crash1_([a-z]+)_",
     "layers": {"pp": 0, "mp": 1, "mf": 2, "ff": 3}, "max": 5.0, "hp": 120},
    {"name": "suspended-cymbal", "key": 57, "glob": "Percussion/susCymb1-hit_*_rr*.wav", "dyn": r"hit_([a-z]+)_",
     "layers": {"pp": 0, "mp": 1, "f": 2, "fff": 3}, "max": 5.0, "hp": 120},
    {"name": "cymbal-swell-short", "key": 58, "glob": "Percussion/susCymb1-cresc-Short_v1.wav", "dyn": None, "max": 12.0, "hp": 120, "swell": True},
    {"name": "cymbal-swell-medium", "key": 59, "glob": "Percussion/susCymb1-cresc-Median_v1.wav", "dyn": None, "max": 14.0, "hp": 120, "swell": True},
    {"name": "cymbal-swell-long", "key": 60, "glob": "Percussion/susCymb1-cresc-Long_v1.wav", "dyn": None, "max": 18.0, "hp": 120, "swell": True},
    {"name": "gong", "key": 61, "glob": "Percussion/gongHit_*.wav", "dyn": r"gongHit_([a-z]+)\.",
     "layers": {"p": 0, "mf": 1, "f": 2, "fff": 3}, "max": 7.0, "hp": 30},
    {"name": "clash", "key": 62, "glob": "VSCO 1 Percussion/varMetal/Cymbals/clash/crash_hit_*.wav", "dyn": r"crash_hit_([a-z]+)_",
     "layers": {"pp": 0, "mp": 1, "ff": 2, "fff": 3}, "max": 5.0, "hp": 120},
]
for n in range(1, 6):
    SOUNDS.append({"name": f"timpani-{n}", "key": 40 + n, "glob": f"Percussion/Timpani/Timpani{n}_Hit_v*_rr*_Sum.wav", "dyn": r"_v(\d)_",
                   "layers": {"1": 0, "2": 1, "3": 2, "4": 3}, "max": 4.0, "hp": 30, "pitched": True})

# What scripts/ci-fetch-vsco.sh checks out (sparse, non-cone patterns): the licence and
# readmes, and every glob above.
# The woodwinds (the woodwinds pass, vsco_woodwinds.py) with them.
FETCH = ["/LICENSE", "/README.md", "/Readme.txt"] + sorted("/" + s["glob"] for s in SOUNDS) + W.FETCH


def load(path, hp):
    audio, rate = sf.read(path, dtype="float64", always_2d=True)
    if rate != RATE:
        audio = signal.resample_poly(audio, RATE, rate, axis=0)
    x = audio.mean(axis=1)
    return signal.sosfiltfilt(signal.butter(2, hp, "highpass", fs=RATE, output="sos"), x)


def trim(x, max_seconds, swell):
    a = np.abs(x)
    peak = float(a.max()) if len(a) else 0.0
    if peak <= 0:
        return None
    # The attack: the first sample within 40 dB of the peak (a swell: 50 dB), 2 ms kept before it.
    first = int(np.argmax(a > peak * (10 ** (-50 / 20) if swell else 10 ** (-40 / 20))))
    start = max(0, first - int(0.002 * RATE))
    y = x[start:]
    # The end: where the envelope (10 ms windows) has fallen 60 dB under the peak for good.
    win = int(0.01 * RATE)
    frames = len(y) // win
    env = np.sqrt(np.mean(y[: frames * win].reshape(frames, win) ** 2, axis=1) + 1e-30) if frames else np.array([peak])
    top = env.max()
    loud = np.nonzero(env > top * 10 ** (-60 / 20))[0]
    end = min(len(y), (int(loud[-1]) + 1) * win if len(loud) else len(y), int(max_seconds * RATE))
    y = y[:end].copy()
    fade = min(int(0.08 * RATE), len(y) // 4)
    if fade > 0:
        y[-fade:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, fade))
    return y


def loudness(y):
    k = P.k_weight(y[: int(1.9 * RATE)])
    win = int(0.4 * RATE)
    if len(k) <= win:
        return float(-0.691 + 10 * np.log10(np.mean(k ** 2) + 1e-24))
    total = np.cumsum(np.concatenate(([0.0], k ** 2)))
    power = (total[win:] - total[:-win]) / win
    return float(-0.691 + 10 * np.log10(max(power.max(), 0.0) + 1e-24))


def fundamental(y):
    """A drum's pitch: the strongest spectral peak between 50 and 400 Hz, 50-500 ms in."""
    seg = y[int(0.05 * RATE): int(0.5 * RATE)]
    if len(seg) < 1024:
        return 0.0
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 1 << 17))
    freqs = np.fft.rfftfreq(1 << 17, 1.0 / RATE)
    band = (freqs > 50) & (freqs < 400)
    return float(freqs[band][np.argmax(spec[band])])


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--source", required=True, help="a VSCO-2-CE checkout (scripts/ci-fetch-vsco.sh)")
    ap.add_argument("--out", required=True, help="the prepared samples folder (prepare.py --out)")
    ap.add_argument("--print-fetch", action="store_true", help="print the sparse-checkout patterns to fetch and exit")
    args = ap.parse_args()
    if args.print_fetch:
        print("\n".join(FETCH))
        return
    lic = os.path.join(args.source, "LICENSE")
    if not os.path.exists(lic) or "CC0 1.0 Universal" not in open(lic, encoding="utf-8", errors="replace").read(200):
        raise SystemExit(f"{lic}: not the CC0 1.0 licence: stop (docs/LICENSES/VSCO-2-CE.md)")
    out_id = "orchperc"
    os.makedirs(os.path.join(args.out, out_id), exist_ok=True)
    zones, strokes, problems = [], {}, []
    for s in SOUNDS:
        files = sorted(glob.glob(os.path.join(args.source, s["glob"])))
        if not files:
            problems.append(f"{s['name']}: no files ({s['glob']})")
            continue
        takes = []
        for f in files:
            if s.get("dyn"):
                m = re.search(s["dyn"], os.path.basename(f))
                if not m or m.group(1) not in s["layers"]:
                    problems.append(f"{s['name']}: {os.path.basename(f)} has no dynamic it knows (skipped)")
                    continue
                layer = s["layers"][m.group(1)]
            else:
                layer = None   # a swell: every layer
            y = trim(load(f, s["hp"]), s["max"], s.get("swell", False))
            if y is None:
                problems.append(f"{s['name']}: {os.path.basename(f)} is silent")
                continue
            takes.append((layer, y, os.path.relpath(f, args.source)))
        if not takes:
            continue
        # One gain per sound: its loudest take peaks at -1 dBFS (the dynamics as recorded).
        gain = 10 ** (-1 / 20) / max(float(np.abs(y).max()) for _, y, _ in takes)
        rr = {}
        info = {"key": s["key"], "files": len(takes)}
        for layer, y, src in takes:
            y = y * gain
            layers = range(len(LAYERS)) if layer is None else [layer]
            for lay in layers:
                k = rr.get(lay, 0)
                rr[lay] = k + 1
                name = f"{out_id}/{s['name']}-{LAYERS[lay]}-{k + 1}.flac"
                if layer is None and lay > 0:
                    name = f"{out_id}/{s['name']}-{LAYERS[0]}-{k + 1}.flac"   # the same audio in every layer
                else:
                    sf.write(os.path.join(args.out, name), y.astype(np.float32), RATE, subtype="PCM_16")
                zones.append({"file": name, "label": s["name"], "root": float(s["key"]), "loKey": s["key"], "hiKey": s["key"],
                              "loVel": VELOCITY[lay][0], "hiVel": VELOCITY[lay][1], "layer": lay, "rr": k, "length": int(len(y)),
                              "rms": round(P.db(np.sqrt(np.mean(y[: int(0.3 * RATE)] ** 2))), 2), "loudness": round(loudness(y), 2),
                              "peak": round(P.db(np.abs(y).max()), 2), "source": src})
        if s.get("pitched"):
            hz = [fundamental(y) for _, y, _ in takes]
            info["pitchHz"] = round(float(np.median(hz)), 1)
            info["note"] = P.name_of(int(round(69 + 12 * np.log2(info["pitchHz"] / 440.0)))) if info["pitchHz"] > 0 else "?"
        info["layers"] = {LAYERS[k]: v for k, v in sorted(rr.items())}
        strokes[s["name"]] = info
        print(f"  {s['name']:20s} key {s['key']}: {len(takes)} takes, layers {info['layers']}" + (f", {info['pitchHz']} Hz ({info['note']})" if s.get("pitched") else ""))
    mapping = {"id": out_id, "name": "Orchestral percussion (VSCO 2 Community Edition, CC0)", "kind": "hits", "rate": RATE, "channels": 1,
               "layers": LAYERS, "gainDb": 0.0, "crossfade": True, "strokes": strokes, "licence": "CC0 1.0 (VSCO 2 CE 1.1.0, Versilian Studios)",
               "zones": zones}
    with open(os.path.join(args.out, out_id + ".json"), "w") as f:
        json.dump(mapping, f, indent=1)
    index_path = os.path.join(args.out, "index.json")
    index = {"version": 1, "instruments": []}
    if os.path.exists(index_path):
        with open(index_path) as f:
            index = json.load(f)
    if out_id not in index["instruments"]:
        index["instruments"].append(out_id)
    # The woodwinds (the woodwinds pass).
    print("Woodwinds:")
    ww_ids, ww_notes, ww_problems = W.prepare_woodwinds(args.source, args.out)
    for i in ww_ids:
        if i not in index["instruments"]:
            index["instruments"].append(i)
    with open(index_path, "w") as f:
        json.dump(index, f, indent=1)
    for p in problems + ww_problems:
        print(" - " + p)
    print(f"Prepared: {len(zones)} notes (vsco: {out_id})")
    print(f"Prepared: {ww_notes} notes (vsco: {', '.join(ww_ids)})")
    if len(ww_ids) < len(W.SETS):
        raise SystemExit(f"only {len(ww_ids)} of {len(W.SETS)} woodwind sets were prepared")
    if len(strokes) < len(SOUNDS):
        raise SystemExit(f"only {len(strokes)} of {len(SOUNDS)} sounds were found")


if __name__ == "__main__":
    main()
