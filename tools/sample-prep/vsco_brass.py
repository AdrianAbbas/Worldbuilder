"""The brass from VSCO 2: Community Edition (the brass pass).

VSCO 2 CE (Versilian Studios, https://github.com/sgossner/VSCO-2-CE, 1.1.0) is CC0 1.0
(docs/LICENSES/VSCO-2-CE.md). CI fetches the files below with the percussion and the woodwinds
(scripts/ci-fetch-vsco.sh, from vsco.py --print-fetch); they are never committed here.
vsco.py calls prepare_brass() after the woodwinds.

Instruments (ids), each mono at 44.1 kHz, every recorded dynamic a velocity layer (crossfaded
by the sampler), its pitch measured from the audio (VSCO names its files an octave low):

  br-horn           F horn, sustained (p, mf, f)          br-horn-stac      staccato (p, mf, f; two takes)
  br-trumpet        trumpet, sustained (p, f)             br-trumpet-stac   staccato (p, mf, f; two takes)
  br-trombone       tenor trombone, sustained (p, mf, f)  br-trombone-stac  staccato (pp, p, f, ff; two takes)
  br-tuba           tuba, sustained (p, mf, f)            br-tuba-stac      staccato (p, f; four takes)
  br-section        a section for chords (the Drama layer's stabs): the tuba under E2, the
                    trombone to A#3, the trumpet above (p, mf, f: a layer a set has not
                    recorded is its nearer recorded dynamic at the level between)
  br-section-stac   the same from the staccatos

Per note, as the woodwinds (vsco_woodwinds.py: the loop search, the recorded end after the
loop, the pitch measure, the tick bridging), except:
- One channel: the two microphones are a spaced pair (Light-Seeker measured their correlation
  0.2-0.8, the tuba's sustains even negative: summed they comb-filter), so each set plays the
  louder channel over the set, for all its notes.
- The attack: cut 2 ms before the level first reaches 40 dB under the note's peak (a 1 ms fade
  in), so the note speaks on the beat and stacked notes strike together.
- A gentle high-pass under the note (a third of its pitch, at most 30 Hz: the tuba's lowest).
- No tick bridging: a brass tone's buzzing lips make steep, pulse-like edges every period, which
  the woodwinds' tick search reads as hundreds of thousands of ticks in the quiet releases
  (smoothing them dulls the note). The tests check the notes for clicks instead.
Levels: one gain per instrument (sustains and staccatos alike), its loudest note at -1 dBFS:
the dynamics and articulations keep their recorded levels. Each file is then stored at full
scale with the difference in its zone's gain (the played levels unchanged), so soft takes keep
their resolution. A take played more than 60 cents off its note is left out; a note recorded at
fewer dynamics than its set gets the missing layers from its nearest recorded one (the gain and
brightness the set's other notes have between those dynamics).
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
import vsco_woodwinds as W

RATE = P.RATE

SETS = [
    {"id": "br-horn", "family": "horn", "glob": "Brass/F Horn/sus/MOHorn_sus_*.wav", "held": True,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1, "3": 2, "4": 2}, "names": ["p", "mf", "f"], "prefer": {"3": 2}},
    {"id": "br-horn-stac", "family": "horn", "glob": "Brass/F Horn/stac/MOHorn_stac_*.wav", "held": False,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1, "3": 2}, "names": ["p", "mf", "f"]},
    {"id": "br-trumpet", "family": "trumpet", "glob": "Brass/Trumpet/sus/Sum_SHTrumpet_sus_*.wav", "held": True,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "3": 1}, "names": ["p", "f"]},
    {"id": "br-trumpet-stac", "family": "trumpet", "glob": "Brass/Trumpet/stac/Sum_SHTrumpet_stac_*.wav", "held": False,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1, "3": 2}, "names": ["p", "mf", "f"]},
    {"id": "br-trombone", "family": "trombone", "glob": "Brass/Tenor Trombone/sus/tenortbn_sus_*.wav", "held": True,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1, "3": 2}, "names": ["p", "mf", "f"]},
    {"id": "br-trombone-stac", "family": "trombone", "glob": "Brass/Tenor Trombone/stac/tenortbn_stac_*.wav", "held": False,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1, "3": 2, "4": 3}, "names": ["pp", "p", "f", "ff"]},
    {"id": "br-tuba", "family": "tuba", "glob": "Brass/Tuba/sus/Tuba3_sus_*.wav", "held": True,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1, "3": 2}, "names": ["p", "mf", "f"],
     # Its lowest take (F1, one soft take) wanders 40 cents over its first seconds: as a drone's
     # pedal it sounded flat. A#1 covers the keys below.
     "skip": {29}},
    {"id": "br-tuba-stac", "family": "tuba", "glob": "Brass/Tuba/stac/Tuba3_stac_*.wav", "held": False,
     "dyn": r"_v(\d)_", "layers": {"1": 0, "2": 1}, "names": ["p", "f"]},
]
# The sections: per instrument, the MIDI range it plays (where its recorded notes are
# within it; key ranges then reach halfway to the neighbours), and how its layers map onto
# the section's three (p, mf, f).
SECTION_SPLIT = [("tuba", -1, 39), ("trombone", 40, 58), ("trumpet", 59, 127)]
SECTIONS = [
    {"id": "br-section", "from": {"tuba": "br-tuba", "trombone": "br-trombone", "trumpet": "br-trumpet"}},
    {"id": "br-section-stac", "from": {"tuba": "br-tuba-stac", "trombone": "br-trombone-stac", "trumpet": "br-trumpet-stac"}},
]
SECTION_NAMES = ["p", "mf", "f"]

FETCH = sorted("/" + s["glob"] for s in SETS)


def velocity_ranges(count):
    if count == 4:
        return [[0.0, 0.3], [0.3, 0.55], [0.55, 0.8], [0.8, 1.0]]
    return W.velocity_ranges(count)


def decode_channels(path):
    audio, rate = sf.read(path, dtype="float64", always_2d=True)
    if rate != RATE:
        audio = signal.resample_poly(audio, RATE, rate, axis=0)
    return audio


def onset(x):
    """2 ms before the level (1 ms windows) first reaches 40 dB under the note's peak."""
    env, win = W.envelope(x, 1.0)
    first = int(np.argmax(env > env.max() * 10 ** (-40 / 20)))
    return max(0, first * win - int(0.002 * RATE))


def attack_trim(x):
    start = onset(x)
    y = x[start:].copy()
    fade = int(0.001 * RATE)
    y[:fade] *= np.linspace(0.0, 1.0, fade)
    return y


def prepare_held(x, label):
    # The woodwinds' held preparation on the trimmed note (its own onset then finds the start).
    return W.prepare_held(attack_trim(x), label)


def tail_end(y, below, longest):
    """Where y has fallen `below` dB under its peak for good (10 ms windows), at most `longest` s."""
    env, win = W.envelope(y, 10.0)
    loud = np.nonzero(env > env.max() * 10 ** (-below / 20))[0]
    return min((int(loud[-1]) + 1) * win if len(loud) else len(y), int(longest * RATE), len(y))


def prepare_short(x, label):
    y = attack_trim(x)
    # Cut 40 dB under the peak (the woodwinds' 48): below about 45 dB the recordings' gate
    # chatters on and off, steps of a few bits the ear (and the tests) hear as ticks; the
    # 120 ms fade ends the note before it.
    end = tail_end(y, 40.0, 1.4)
    y = W.fade_end(y[:end].copy())
    root, _ = W.measure_pitch(y, label, int(0.03 * RATE), min(len(y), int(0.23 * RATE)))
    if root is None:
        root = float(label)
    return (y, root, None, None, {}), None


def write_set(out, sid, family, names, zones, report):
    """Writes one instrument's notes and mapping (zones: dicts with take, layer, root, rr,
    gainDb, shelfDb)."""
    os.makedirs(os.path.join(out, sid), exist_ok=True)
    vel = velocity_ranges(len(names))
    W.key_ranges(zones)
    written, mapping_zones, lift = {}, [], {}
    for z in zones:
        t = z["take"]
        key = id(t)
        if key not in written:
            # Each file at full scale (its loudest sample at -1 dBFS), the difference back in the
            # zone's gain: a soft take's quiet end keeps its resolution (one gain per instrument
            # left soft staccatos' tails a few bits deep, steps the ear hears as grit).
            lift[key] = float(np.clip(-1.0 - P.db(np.abs(t["audio"]).max()), 0.0, 40.0))
            base = f"{P.name_of(int(round(t['root']))).replace('#', 's')}-{t['dynName']}-{z['rr'] + 1}"
            name = f"{sid}/{base}.flac"
            n = 2
            while name in written.values():
                name = f"{sid}/{base}-{n}.flac"
                n += 1
            sf.write(os.path.join(out, name), (t["audio"] * 10 ** (lift[key] / 20)).astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
            written[key] = name
        mz = {"file": written[key], "label": P.name_of(int(round(t["label"]))), "root": z["root"], "loKey": z["loKey"], "hiKey": z["hiKey"],
              "loVel": vel[z["layer"]][0], "hiVel": vel[z["layer"]][1], "layer": z["layer"], "rr": z["rr"], "length": int(len(t["audio"])),
              "rms": round(P.db(np.sqrt(np.mean(t["audio"][: int(0.3 * RATE)] ** 2))), 2), "loudness": round(t["loudness"] + z["gainDb"] + z.get("shelfLoud", 0.0), 2),
              "peak": round(P.db(np.abs(t["audio"]).max()), 2), "source": t["source"]}
        if round(z["gainDb"] - lift[key], 2) != 0.0:
            mz["gainDb"] = round(z["gainDb"] - lift[key], 2)
        if z.get("shelfDb", 0.0) != 0.0:
            mz["shelfDb"], mz["shelfHz"] = z["shelfDb"], 2000.0
        if t["loop"]:
            mz["loopStart"], mz["loopEnd"] = int(t["loop"][0]), int(t["loop"][1])
            mz["release"] = int(t["release"])
        mapping_zones.append(mz)
    mapping_zones.sort(key=lambda m: (m["root"], m["layer"], m["rr"]))
    label = {"horn": "F horn", "trumpet": "Trumpet", "trombone": "Tenor trombone", "tuba": "Tuba", "section": "Brass section"}[family]
    mapping = {"id": sid, "name": f"{label} (VSCO 2 Community Edition, CC0)", "kind": "brass", "rate": RATE, "channels": 1, "crossfade": True,
               "layers": names, "gainDb": 0.0, "licence": "CC0 1.0 (VSCO 2 CE 1.1.0, Versilian Studios)", "zones": mapping_zones}
    with open(os.path.join(out, sid + ".json"), "w") as f:
        json.dump(mapping, f, indent=1)
    notes = sorted({P.name_of(int(round(m["root"]))) + f" {100 * (m['root'] - round(m['root'])):+.0f}c" for m in mapping_zones if m["layer"] == 0},
                   key=lambda q: P.midi_of(q.split()[0]))
    seconds = sum({m["file"]: m["length"] for m in mapping_zones}.values()) / RATE
    print(f"  {sid:17s} {len(mapping_zones):3d} zones ({len(written)} files, {seconds:.0f} s), layers {names}: " + ", ".join(notes))
    return len(mapping_zones)


def prepare_brass(source, out):
    """Prepares every brass set (and the two sections) into `out`; returns (ids, note count, problems)."""
    problems, report = [], {}
    raw = {}
    for s in SETS:
        files = sorted(glob.glob(os.path.join(source, s["glob"])))
        if not files:
            problems.append(f"{s['id']}: no files ({s['glob']})")
            continue
        decoded = []
        for f in files:
            label = W.label_of(f)
            if label is None:
                problems.append(f"{s['id']}: {os.path.basename(f)} has no note name")
                continue
            if label in s.get("skip", ()):
                problems.append(f"{s['id']}: {os.path.basename(f)} left out (an unsteady pitch)")
                continue
            m = re.search(s["dyn"], os.path.basename(f))
            dyn = m.group(1) if m else None
            if dyn not in s["layers"]:
                problems.append(f"{s['id']}: {os.path.basename(f)} has no dynamic it knows (skipped)")
                continue
            decoded.append((f, label, dyn, decode_channels(f)))
        if not decoded:
            continue
        # The louder channel over the set (one microphone of the spaced pair).
        chans = min(a.shape[1] for _, _, _, a in decoded)
        energy = [sum(float(np.sum(a[:, c] ** 2)) for _, _, _, a in decoded) for c in range(chans)]
        ch = int(np.argmax(energy))
        if chans > 1:
            corr = [float(np.corrcoef(a[:, 0], a[:, 1])[0, 1]) for _, _, _, a in decoded[:: max(1, len(decoded) // 6)]]
            report[s["id"] + " channel"] = f"{'LR'[ch] if chans == 2 else ch} of {chans} (correlation between them {min(corr):+.2f} to {max(corr):+.2f})"
        takes = []
        for f, label, dyn, audio in decoded:
            hz = 440 * 2 ** ((label - 69) / 12)
            x = signal.sosfiltfilt(signal.butter(2, min(30.0, hz / 3), "highpass", fs=RATE, output="sos"), audio[:, ch])
            got, why = (prepare_held if s["held"] else prepare_short)(x, label)
            if got is None:
                problems.append(f"{s['id']}: {os.path.basename(f)}: {why} (skipped)")
                continue
            y, root, loop, release, notes = got
            ticks = 0   # no tick bridging: see the module's notes
            if abs(root - label) > 0.6:
                # A take that far off its note (the horn's G1 p staccato, a tuba A#0 take) was
                # played off it: left out (its neighbours cover the key).
                problems.append(f"{s['id']}: {os.path.basename(f)} sounds {100 * (root - label):+.0f} cents from its note (left out)")
                continue
            layer = s["layers"][dyn]
            takes.append({"layer": layer, "dyn": dyn, "dynName": s["names"][layer], "audio": y, "root": root, "label": label, "loop": loop,
                          "release": release, "source": os.path.relpath(f, source), "notes": notes, "ticks": ticks})
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
            t["loudness"] = W.loudness(t["audio"])
            t["bright"] = W.brightness(t["audio"])
    ids, total = [], 0
    chosen_by_set = {}
    for s in SETS:
        takes = raw.get(s["id"], [])
        if not takes:
            continue
        # Where a note has two takes of one layer from different dynamics (the horn's v3 and
        # v4 both f), the preferred dynamic only.
        chosen = []
        for t in takes:
            rivals = [q for q in takes if q is not t and q["layer"] == t["layer"] and round(q["label"]) == round(t["label"]) and q["dyn"] != t["dyn"]]
            if rivals and s.get("prefer") and t["dyn"] not in s["prefer"]:
                continue
            chosen.append(t)
        chosen_by_set[s["id"]] = chosen
        zones, rr = [], {}
        for t in sorted(chosen, key=lambda q: (q["root"], q["layer"], q["source"])):
            k = (t["layer"], round(t["root"]))
            rr[k] = rr.get(k, 0) + 1
            zones.append({"take": t, "layer": t["layer"], "root": round(t["root"], 3), "rr": rr[k] - 1, "gainDb": 0.0, "shelfDb": 0.0})
        # A note recorded at fewer dynamics than the set (the horn's D5 and F5: soft only): its
        # missing layers derived from its nearest recorded one, at the gain and brightness the
        # set's notes recorded at both have between them (medians), so every key keeps its own
        # pitch in every layer instead of borrowing a note far off.
        filled = 0
        layers = range(len(s["names"]))
        for label in sorted({round(t["label"]) for t in chosen}):
            have = {t["layer"] for t in chosen if round(t["label"]) == label}
            for want in [l for l in layers if l not in have]:
                src_layer = min(have, key=lambda l: (abs(l - want), -l))
                pairs = []
                for other in {round(t["label"]) for t in chosen}:
                    a = [t for t in chosen if round(t["label"]) == other and t["layer"] == src_layer]
                    b = [t for t in chosen if round(t["label"]) == other and t["layer"] == want]
                    if a and b:
                        pairs.append((b[0]["loudness"] - a[0]["loudness"], b[0]["bright"] - a[0]["bright"]))
                gain = float(np.median([q[0] for q in pairs])) if pairs else 6.0 * (want - src_layer)
                shelf = float(np.median([q[1] for q in pairs])) if pairs else 3.0 * (want - src_layer)
                shelf = max(0.0, shelf) if want > src_layer else min(0.0, shelf)
                for z in [z for z in zones if round(z["take"]["label"]) == label and z["layer"] == src_layer and not z.get("derived")]:
                    # The shelf (as the sampler plays it: a one-pole split at 2 kHz) adds loudness
                    # of its own: the gain takes it back, so the layer steps by the measured gain.
                    y = z["take"]["audio"]
                    k = 1.0 - math.exp(-2 * math.pi * 2000.0 / RATE)
                    lo = signal.lfilter([k], [1.0, k - 1.0], y)
                    extra = W.loudness(lo + 10 ** (shelf / 20) * (y - lo)) - z["take"]["loudness"]
                    zones.append({**z, "layer": want, "gainDb": round(gain - extra, 2), "shelfDb": round(shelf, 2), "derived": True,
                                  "shelfLoud": extra})
                    filled += 1
        if filled:
            report[s["id"] + " derived"] = f"{filled} zones of a dynamic a note was not recorded at (from its nearest recorded one)"
        total += write_set(out, s["id"], s["family"], s["names"], zones, report)
        ids.append(s["id"])
    # The sections (the Drama layer's chords): each instrument in its register, three layers.
    by_family = {s["family"]: s for s in SETS}
    for sec in SECTIONS:
        zones = []
        for fam, lo, hi in SECTION_SPLIT:
            sid = sec["from"][fam]
            src = next(s for s in SETS if s["id"] == sid)
            takes = [t for t in chosen_by_set.get(sid, []) if lo - 0.5 <= t["root"] <= hi + 0.5]
            count = len(src["names"])
            # The set's layers onto p, mf, f: two layers are p and f (mf: the nearer recording,
            # at the level halfway); four layers drop the softest (pp) and play p, f and ff.
            if count == 2:
                plan = {0: (0, None), 2: (1, None), 1: (1, "half")}
            elif count == 4:
                plan = {0: (1, None), 1: (2, None), 2: (3, None)}
            else:
                plan = {0: (0, None), 1: (1, None), 2: (2, None)}
            for target, (layer, how) in plan.items():
                for t in [q for q in takes if q["layer"] == layer]:
                    gain = 0.0
                    if how == "half":
                        soft = [q["loudness"] for q in takes if q["layer"] == 0 and round(q["label"]) == round(t["label"])]
                        gain = round(0.5 * (soft[0] - t["loudness"]), 2) if soft else -4.0
                    zones.append({"take": t, "layer": target, "root": round(t["root"], 3), "rr": 0, "gainDb": gain, "shelfDb": 0.0})
            if by_family[fam]["family"] != fam:
                problems.append(f"{sec['id']}: no {fam}")
        # Round-robins per key and layer.
        rr = {}
        for z in sorted(zones, key=lambda q: (q["root"], q["layer"], q["take"]["source"])):
            k = (z["layer"], round(z["root"]))
            z["rr"] = rr.get(k, 0)
            rr[k] = z["rr"] + 1
        if zones:
            total += write_set(out, sec["id"], "section", SECTION_NAMES, zones, report)
            ids.append(sec["id"])
        else:
            problems.append(f"{sec['id']}: no notes")
    for k, v in report.items():
        print(f"  {k}: {v}")
    ticks = sum(t["ticks"] for takes in raw.values() for t in takes)
    if ticks:
        print(f"  ticks bridged in the notes' quiet ends: {ticks}")
    return ids, total, problems
