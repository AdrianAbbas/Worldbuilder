"""The woodwinds from VSCO 2: Community Edition (the woodwinds pass).

VSCO 2 CE (Versilian Studios, https://github.com/sgossner/VSCO-2-CE, 1.1.0) is CC0 1.0
(docs/LICENSES/VSCO-2-CE.md). CI fetches the files below with the percussion
(scripts/ci-fetch-vsco.sh, from vsco.py --print-fetch); they are never committed here.
vsco.py calls prepare_woodwinds() after the percussion.

Instruments (ids), each mono at 44.1 kHz, its dynamics as velocity layers (crossfaded by the
sampler), its pitch measured from the audio (VSCO names its files an octave low):

  ww-flute          flute, sustained without vibrato (p, f), the piccolo's sustains above C7
  ww-flute-vib      flute with vibrato (one recorded dynamic; a derived f layer: the gain and
                    brightness the plain flute's f has over its p), the piccolo above C7
  ww-flute-stac     flute staccato (p, mf, f; two takes), the piccolo's staccatos above C7
  ww-piccolo        piccolo, sustained (p, a derived f)       ww-piccolo-stac  staccato
  ww-oboe           oboe, sustained (p, f)                    ww-oboe-vib      with vibrato (p, f)
  ww-oboe-stac      oboe staccato (p, mf, f; two takes)
  ww-clarinet       clarinet, sustained (p, mf, f)            ww-clarinet-stac staccato (p, mf, f)
  ww-bassoon        bassoon, sustained (p, f)                 ww-bassoon-vib   with vibrato (p, f)
  ww-bassoon-stac   bassoon staccato (p, f; two takes)

Per sustained note: decoded, the two channels summed, a gentle high-pass under the note
(a third of its pitch, at most 40 Hz), the onset found (20 ms of breath kept before the tone),
the pitch measured (YIN near the file's note an octave up, refined to where its first four
harmonics carry the most energy) on the loop; a loop found in the steady sustain (0.6-2.6 s long, the best-matching waveforms,
crossfaded and levelled, prepare.py's search), the rest of the sustain cut out, the recorded
release (where the player stops blowing, 60 ms before it, to 48 dB under the note; at most 1 s, the room's tail
beyond left to the Places) kept after the loop: a held note is its attack, its loop and its
recorded end. Staccatos: cut where they have fallen 48 dB (at most 1.2 s; some takes' tails are
gated to digital silence below).
Ticks in a note's quiet end (30 dB under it) bridged. Levels: one gain per instrument (sustains, vibrato and staccatos alike), its loudest note at
-1 dBFS: the dynamics and the articulations keep their recorded levels.
"""

import glob
import json
import math
import os
import re

import numpy as np
import soundfile as sf
from scipy import signal

import prepare as P

RATE = P.RATE
NAMES = {"p": [0.0, 0.55], "f": [0.55, 1.0]}

# Each set: its instrument id, the files, the articulation, how a file name gives its
# dynamic (and which layer each dynamic is), its take, and its family (one gain each).
SETS = [
    {"id": "ww-flute", "family": "flute", "glob": "Woodwinds/Flute/susNV/LDFlute_susNV_*.wav", "held": True,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1, "3": 1}, "names": ["p", "f"], "prefer": {"3": 1}},
    {"id": "ww-flute-vib", "family": "flute", "glob": "Woodwinds/Flute/susvib/LDFlute_susvib_*.wav", "held": True,
     "dyn": r"_v(\d)_", "layers": {"1": 0}, "names": ["p", "f"], "derive": "ww-flute"},
    {"id": "ww-flute-stac", "family": "flute", "glob": "Woodwinds/Flute/stac/LDFlute_stac_*.wav", "held": False,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1, "3": 2, "4": 2}, "names": ["p", "mf", "f"]},
    {"id": "ww-piccolo", "family": "piccolo", "glob": "Woodwinds/Piccolo/Sus/piccolo_*_sustain*.wav", "held": True,
     "dyn": None, "layers": {None: 0}, "names": ["p", "f"], "derive": "ww-flute"},
    {"id": "ww-piccolo-stac", "family": "piccolo", "glob": "Woodwinds/Piccolo/Stac/piccolo_*_staccato*.wav", "held": False,
     "dyn": None, "layers": {None: 0}, "names": ["mf"]},
    {"id": "ww-oboe", "family": "oboe", "glob": "Woodwinds/Oboe/Sus/Oboe_Sus_*.wav", "held": True,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "3": 1}, "names": ["p", "f"]},
    {"id": "ww-oboe-vib", "family": "oboe", "glob": "Woodwinds/Oboe/Vib/Oboe_Vib_*.wav", "held": True,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "3": 1}, "names": ["p", "f"]},
    {"id": "ww-oboe-stac", "family": "oboe", "glob": "Woodwinds/Oboe/Stacc/Oboe_Stacc_*.wav", "held": False,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1, "3": 2}, "names": ["p", "mf", "f"]},
    {"id": "ww-clarinet", "family": "clarinet", "glob": "Woodwinds/Clarinet/susLong/DCClar_susLong_*.wav", "held": True,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1, "3": 2}, "names": ["p", "mf", "f"]},
    {"id": "ww-clarinet-stac", "family": "clarinet", "glob": "Woodwinds/Clarinet/stac/DCClar_stac_*.wav", "held": False,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1, "3": 2}, "names": ["p", "mf", "f"]},
    {"id": "ww-bassoon", "family": "bassoon", "glob": "Woodwinds/Bassoon/sus/PSBassoon_*.wav", "held": True,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1}, "names": ["p", "f"]},
    {"id": "ww-bassoon-vib", "family": "bassoon", "glob": "Woodwinds/Bassoon/vib/PSBassoon_*.wav", "held": True,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1}, "names": ["p", "f"]},
    {"id": "ww-bassoon-stac", "family": "bassoon", "glob": "Woodwinds/Bassoon/stac/PSBassoon_*.wav", "held": False,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1}, "names": ["p", "f"]},
]
# The flute instruments carry the piccolo's notes above their own top (C7).
EXTEND = {"ww-flute": "ww-piccolo", "ww-flute-vib": "ww-piccolo", "ww-flute-stac": "ww-piccolo-stac"}

FETCH = sorted("/" + s["glob"] for s in SETS)


def velocity_ranges(count):
    if count == 1:
        return [[0.0, 1.0]]
    if count == 2:
        return [[0.0, 0.55], [0.55, 1.0]]
    return [[0.0, 0.4], [0.4, 0.72], [0.72, 1.0]]


def label_of(path):
    m = re.search(r"_([A-G]#?-?\d)_", os.path.basename(path) + "_")
    return P.midi_of(m.group(1)) + 12 if m else None   # VSCO names an octave low


def decode(path):
    audio, rate = sf.read(path, dtype="float64", always_2d=True)
    if rate != RATE:
        audio = signal.resample_poly(audio, RATE, rate, axis=0)
    return audio.mean(axis=1)


def envelope(x, ms=5.0):
    win = max(1, int(ms * 0.001 * RATE))
    frames = len(x) // win
    return np.sqrt(np.mean(x[: frames * win].reshape(frames, win) ** 2, axis=1) + 1e-30), win


def onset(x, preroll):
    env, win = envelope(x)
    top = env.max()
    first = int(np.argmax(env > top * 10 ** (-36 / 20)))
    return max(0, first * win - int(preroll * RATE))


def measure_pitch(y, label, a, b):
    """MIDI pitch over y[a:b]: YIN within 3 semitones of the label, refined on the harmonics'
    summed energy."""
    seg = y[a:b]
    if len(seg) < 4096:
        return None, 1.0
    fmin, fmax = 440 * 2 ** ((label - 3 - 69) / 12), 440 * 2 ** ((label + 3 - 69) / 12)
    frames = P.yin(seg, fmin=fmin * 0.9, fmax=fmax * 1.1)
    good = [f for f, ap in frames if ap < 0.3 and fmin <= f <= fmax]
    if len(good) < 3:
        return None, 1.0
    f0 = float(np.median(good))
    f0 = harmonic_pitch(seg[: int(min(len(seg), 1.0 * RATE))], f0)
    return 69 + 12 * math.log2(f0 / 440.0), float(np.median([ap for _, ap in frames]))


def harmonic_pitch(x, f0, harmonics=4):
    """The frequency within 60 cents of f0 whose first harmonics carry the most energy (a
    Hann-windowed DFT at each candidate, 1-cent steps, refined to 0.2): what the ear hears
    as the note's pitch, and what the tests measure."""
    n = np.arange(len(x))
    w = x * np.hanning(len(x))

    def energy(f):
        e = 0.0
        for k in range(1, harmonics + 1):
            if f * k >= 0.45 * RATE:
                break
            e += abs(np.dot(w, np.exp(-2j * np.pi * f * k * n / RATE))) ** 2
        return e

    cands = f0 * 2 ** (np.arange(-60, 61) / 1200)
    best = max(cands, key=energy)
    fine = best * 2 ** (np.arange(-1.0, 1.01, 0.2) / 1200)
    return float(max(fine, key=energy))


def tail_end(y, start, longest, peak=None):
    """Where y (from `start`) has fallen 48 dB under the note's peak for good (below, some
    takes' tails are gated to digital silence), at most `longest` seconds on (the room's tail
    beyond: the Places add their own space)."""
    env, win = envelope(y[start:], 10.0)
    if not len(env):
        return len(y)
    top = peak if peak is not None else env.max()
    loud = np.nonzero(env > top * 10 ** (-48 / 20))[0]
    return start + min((int(loud[-1]) + 1) * win if len(loud) else len(env) * win, int(longest * RATE))


def declick(y):
    """Smooths isolated spikes in the quiet end of a note (30 dB or more under its peak): a
    sample step 8 times the local median step is bridged by interpolation (the recordings'
    tails carry a few such ticks)."""
    if len(y) < 2048:
        return y, 0
    env, win = envelope(y, 10.0)
    quiet = np.repeat(env < env.max() * 10 ** (-30 / 20), win)
    quiet = np.concatenate([quiet, np.zeros(len(y) - len(quiet), dtype=bool)])
    d = np.abs(np.diff(y))
    med = signal.medfilt(d, 441) + 1e-9
    spikes = np.nonzero((d > 8 * med) & quiet[1:])[0]
    y = y.copy()
    fixed = 0
    for i in spikes:
        a, b = max(0, i - 2), min(len(y) - 1, i + 3)
        y[a:b + 1] = np.linspace(y[a], y[b], b - a + 1)
        fixed += 1
    return y, fixed


def fade_end(y):
    fade = min(int(0.12 * RATE), len(y) // 4)
    if fade > 0:
        y[-fade:] *= 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, fade))
    return y


def prepare_held(x, label):
    """A sustained note: (audio, root, loop, release, notes) or None."""
    start = onset(x, 0.02)
    y = x[start:]
    region = P.sustain_region(y)
    if region is None:
        return None, "no sustain"
    a, b = region
    a = max(a, int(0.35 * RATE))           # past the attack and its first swell
    b_loop = min(b, a + int(2.9 * RATE))   # a loop of at most about 2.6 s
    loop = P.find_loop(y, 440 * 2 ** ((label - 69) / 12), (a, b_loop))
    if not loop:
        return None, "no loop"
    s, e, cost, _ = loop
    root, aper = measure_pitch(y, label, s, e)
    if root is None:
        return None, "no pitch"
    y = P.crossfade_loop(y, s, e, int(0.12 * RATE))
    # The recorded end: from 60 ms before the player stops blowing, to where it has
    # fallen 60 dB; joined after the loop (the sustain between is never heard).
    release_from = max(e, b - int(0.06 * RATE))
    end = tail_end(y, release_from, 1.0, envelope(y, 10.0)[0].max())
    tail = y[release_from:end].copy()
    if len(tail) < int(0.05 * RATE):
        tail = y[e: min(len(y), e + int(0.3 * RATE))].copy()
        tail *= np.linspace(1.0, 0.0, len(tail))
    # The loop, 8 samples of what follows it (the bank copies the loop's start over the first
    # three, for interpolation), then the recorded end: the sampler crosses into it on release.
    pad = 8
    out = np.concatenate([y[:e + pad], fade_end(tail)])
    return (out, root, (s, e), e + pad, {"loop": f"{s / RATE:.2f}-{e / RATE:.2f}s (match {cost:.3f})", "aperiodicity": round(aper, 3)}), None


def prepare_short(x, label):
    start = onset(x, 0.008)
    y = x[start:]
    end = tail_end(y, 0, 1.2)
    y = fade_end(y[:end].copy())
    root, aper = measure_pitch(y, label, int(0.03 * RATE), min(len(y), int(0.23 * RATE)))
    if root is None:
        # Too short to measure on its own: the file's note (the sustains' tuning is measured).
        root = float(label)
    return (y, root, None, None, {}), None


def loudness(y):
    k = P.k_weight(y[: int(3.0 * RATE)])
    win = int(0.4 * RATE)
    if len(k) <= win:
        return float(-0.691 + 10 * np.log10(np.mean(k ** 2) + 1e-24))
    total = np.cumsum(np.concatenate(([0.0], k ** 2)))
    power = (total[win:] - total[:-win]) / win
    return float(-0.691 + 10 * np.log10(max(power.max(), 0.0) + 1e-24))


def brightness(y):
    """Energy above 2 kHz against the whole (dB), over the loudest second."""
    seg = y[int(0.2 * RATE): int(1.2 * RATE)]
    if len(seg) < 2048:
        seg = y
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    freqs = np.fft.rfftfreq(len(seg), 1.0 / RATE)
    return float(10 * np.log10((spec[freqs > 2000].sum() + 1e-20) / (spec.sum() + 1e-20)))


def key_ranges(zones):
    """Within each layer: each pitch covers halfway to its neighbours; the lowest reaches
    down an octave and the highest up a fifth (the sampler shifts the nearest recording)."""
    for z in zones:
        roots = sorted({round(q["root"], 1) for q in zones if q["layer"] == z["layer"]})
        r = round(z["root"], 1)
        lower = [q for q in roots if q < r - 0.5]
        upper = [q for q in roots if q > r + 0.5]
        z["loKey"] = math.floor((max(lower) + r) / 2) + 1 if lower else int(round(r)) - 12
        z["hiKey"] = math.floor((min(upper) + r) / 2) if upper else int(round(r)) + 7


def prepare_woodwinds(source, out):
    """Prepares every woodwind set into `out`; returns (ids, note count, problems)."""
    problems, report = [], {}
    raw = {}   # id -> list of take dicts (audio before the family gain)
    for s in SETS:
        files = sorted(glob.glob(os.path.join(source, s["glob"])))
        if not files:
            problems.append(f"{s['id']}: no files ({s['glob']})")
            continue
        takes = []
        for f in files:
            label = label_of(f)
            if label is None:
                problems.append(f"{s['id']}: {os.path.basename(f)} has no note name")
                continue
            dyn = None
            if s["dyn"]:
                m = re.search(s["dyn"], os.path.basename(f))
                dyn = m.group(1) if m else None
                if dyn not in s["layers"]:
                    problems.append(f"{s['id']}: {os.path.basename(f)} has no dynamic it knows (skipped)")
                    continue
            x = signal.sosfiltfilt(signal.butter(2, min(40.0, 440 * 2 ** ((label - 69) / 12) / 3), "highpass", fs=RATE, output="sos"), decode(f))
            got, why = (prepare_held if s["held"] else prepare_short)(x, label)
            if got is None:
                problems.append(f"{s['id']}: {os.path.basename(f)}: {why} (skipped)")
                continue
            y, root, loop, release, notes = got
            y, ticks = declick(y)
            if ticks:
                notes["ticks"] = ticks
            if abs(root - label) > 0.6:
                problems.append(f"{s['id']}: {os.path.basename(f)} sounds {100 * (root - label):+.0f} cents from its note (kept at its own pitch)")
            takes.append({"layer": s["layers"][dyn], "dyn": dyn, "audio": y, "root": root, "label": label, "loop": loop, "release": release,
                          "source": os.path.relpath(f, source), "notes": notes})
        raw[s["id"]] = takes
    # One gain per family: its loudest note peaks at -1 dBFS.
    family_peak = {}
    for s in SETS:
        for t in raw.get(s["id"], []):
            family_peak[s["family"]] = max(family_peak.get(s["family"], 0.0), float(np.abs(t["audio"]).max()))
    for s in SETS:
        g = 10 ** (-1 / 20) / max(family_peak.get(s["family"], 1.0), 1e-9)
        for t in raw.get(s["id"], []):
            t["audio"] = t["audio"] * g
            t["loudness"] = loudness(t["audio"])
            t["bright"] = brightness(t["audio"])
    ids, total = [], 0
    for s in SETS:
        takes = raw.get(s["id"], [])
        if not takes:
            continue
        sid = s["id"]
        os.makedirs(os.path.join(out, sid), exist_ok=True)
        names = s["names"]
        vel = velocity_ranges(len(names))
        # Where a note has two takes of one dynamic (the flute's v2 and v3 both f), the
        # preferred dynamic only.
        chosen = []
        for t in takes:
            rivals = [q for q in takes if q is not t and q["layer"] == t["layer"] and round(q["label"]) == round(t["label"]) and q["dyn"] != t["dyn"]]
            if rivals and s.get("prefer") and t["dyn"] not in s["prefer"]:
                continue
            chosen.append(t)
        zones = []
        rr = {}
        for t in sorted(chosen, key=lambda q: (q["root"], q["layer"], q["source"])):
            k = (t["layer"], round(t["root"]))
            rr[k] = rr.get(k, 0) + 1
            zones.append({"take": t, "layer": t["layer"], "root": round(t["root"], 3), "rr": rr[k] - 1, "gainDb": 0.0, "shelfDb": 0.0})
        # A derived loud layer (a set with one recorded dynamic): its notes again, at the gain
        # and brightness the reference set's f has over its p (medians over its notes).
        if s.get("derive") and len(names) == 2:
            ref = raw.get(s["derive"], [])
            pairs = []
            for t in ref:
                if t["layer"] != 0:
                    continue
                loud = [q for q in ref if q["layer"] == 1 and round(q["label"]) == round(t["label"])]
                if loud:
                    pairs.append((loud[0]["loudness"] - t["loudness"], loud[0]["bright"] - t["bright"]))
            gain = float(np.median([p[0] for p in pairs])) if pairs else 6.0
            shelf = float(np.median([p[1] for p in pairs])) if pairs else 3.0
            for z in [z for z in zones if z["layer"] == 0]:
                zones.append({**z, "layer": 1, "gainDb": round(gain, 2), "shelfDb": round(max(0.0, shelf), 2), "derived": True})
            report[sid + " derived f"] = f"{gain:+.1f} dB, shelf {shelf:+.1f} dB above 2 kHz (from {s['derive']}, {len(pairs)} notes)"
        layers_used = len(names)
        # The piccolo above the flute's top, in every layer, levelled to the flute's top note.
        ext = EXTEND.get(sid)
        if ext and raw.get(ext):
            top = max(z["root"] for z in zones)
            top_loud = {lay: np.median([z["take"]["loudness"] + z["gainDb"] for z in zones if z["layer"] == lay and z["root"] > top - 7.5]) for lay in range(layers_used)}
            picc = [t for t in raw[ext] if t["root"] > top + 1.5]
            picc_loud = np.median([t["loudness"] for t in raw[ext]]) if raw[ext] else 0.0
            for t in picc:
                for lay in range(layers_used):
                    zones.append({"take": t, "layer": lay, "root": round(t["root"], 3), "rr": 0, "gainDb": round(float(top_loud[lay] - picc_loud), 2), "shelfDb": 0.0,
                                  "piccolo": True})
        key_ranges(zones)
        written = {}
        mapping_zones = []
        for z in zones:
            t = z["take"]
            key = id(t)
            if key not in written:
                base = f"{P.name_of(int(round(t['root']))).replace('#', 's')}-{names[min(t['layer'], len(names) - 1)]}-{z['rr'] + 1}"
                if z.get("piccolo"):
                    base = "piccolo-" + base
                name = f"{sid}/{base}.flac"
                n = 2
                while name in written.values():
                    name = f"{sid}/{base}-{n}.flac"
                    n += 1
                sf.write(os.path.join(out, name), t["audio"].astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
                written[key] = name
            mz = {"file": written[key], "label": P.name_of(int(round(t["label"]))), "root": z["root"], "loKey": z["loKey"], "hiKey": z["hiKey"],
                  "loVel": vel[z["layer"]][0], "hiVel": vel[z["layer"]][1], "layer": z["layer"], "rr": z["rr"], "length": int(len(t["audio"])),
                  "rms": round(P.db(np.sqrt(np.mean(t["audio"][: int(0.3 * RATE)] ** 2))), 2), "loudness": round(t["loudness"] + z["gainDb"], 2),
                  "peak": round(P.db(np.abs(t["audio"]).max()), 2), "source": t["source"]}
            if z["gainDb"] != 0.0:
                mz["gainDb"] = z["gainDb"]
            if z["shelfDb"] != 0.0:
                mz["shelfDb"], mz["shelfHz"] = z["shelfDb"], 2000.0
            if t["loop"]:
                mz["loopStart"], mz["loopEnd"] = int(t["loop"][0]), int(t["loop"][1])
                mz["release"] = int(t["release"])
            mapping_zones.append(mz)
        mapping_zones.sort(key=lambda m: (m["root"], m["layer"], m["rr"]))
        mapping = {"id": sid, "name": f"{s['family'].capitalize()} (VSCO 2 Community Edition, CC0)", "kind": "woodwind", "rate": RATE, "channels": 1,
                   "crossfade": True, "layers": names, "gainDb": 0.0, "licence": "CC0 1.0 (VSCO 2 CE 1.1.0, Versilian Studios)", "zones": mapping_zones}
        with open(os.path.join(out, sid + ".json"), "w") as f:
            json.dump(mapping, f, indent=1)
        notes = sorted({P.name_of(int(round(m["root"]))) + f" {100 * (m['root'] - round(m['root'])):+.0f}c" for m in mapping_zones if m["layer"] == 0},
                       key=lambda q: P.midi_of(q.split()[0]))
        seconds = sum({m["file"]: m["length"] for m in mapping_zones}.values()) / RATE
        print(f"  {sid:17s} {len(mapping_zones):3d} zones ({len(written)} files, {seconds:.0f} s), layers {names}: " + ", ".join(notes))
        ids.append(sid)
        total += len(mapping_zones)
    for k, v in report.items():
        print(f"  {k}: {v}")
    return ids, total, problems
