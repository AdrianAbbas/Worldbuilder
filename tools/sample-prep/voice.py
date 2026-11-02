"""The developer's voice (Milestone 17): vowels, choir stacks, drones, chant, textures.

Used by prepare.py for the manifest's "kind": "voice" sets. Each set is one sampler
instrument; `mode` says how its recordings are cut:

  "sweep"   a vowel sung on a few notes, low to high, one at a time (each held 2-3 s):
            `layers` (soft / full: velocity layers; files with the same layer are
            round-robins), `notes` (what the file names say, checked against the audio),
            `falsetto` (more files whose notes go on top of every layer).
  "takes"   one or more files of held notes (choir stacks: five or six takes of one
            note; drones; a chant): every take a round-robin of the pitch it is sung on.
  "texture" unpitched sound (whispers, breaths): `cut` "whole" keeps each file as one
            long looped zone (a take: normal, higher, lower voice), "events" cuts every
            breath or consonant out as its own one-shot.

Pitches come from the audio, never the names: the strongest harmonic series, then an
octave check (a male voice's second harmonic is often louder than its fundamental, so
the series is moved an octave down while the odd harmonics of the lower octave are
there), and in a sweep the notes' order settles the octave (the median offset to the
names). Labels that disagree are reported (the chest sweeps sound an octave above
their names).

Held notes get a sustain loop (the best-matching stretch, crossfaded over 200 ms and
levelled), tuned on the loop; the recorded tail after the release point plays when the
note ends. No noise reduction: the recordings' floors are about -88 dBFS.
"""

import json
import math
import os

import numpy as np
import soundfile as sf
from scipy import signal

import prepare as P

RATE = P.RATE


def load(path):
    audio, rate = sf.read(path, dtype="float64", always_2d=True)
    if rate != RATE:
        raise SystemExit(f"{path}: {rate} Hz (expected {RATE})")
    left, right = audio[:, 0], audio[:, -1]
    live = np.abs(left) > 1e-5
    corr = float(np.corrcoef(left[live], right[live])[0, 1]) if live.sum() > 1000 and audio.shape[1] > 1 else 1.0
    x = signal.sosfiltfilt(signal.butter(2, 30, "highpass", fs=RATE, output="sos"), audio.mean(axis=1))
    info = sf.info(path)
    return x, corr, f"{info.subtype}, {info.channels} ch"


def segments(x, below=20.0, min_seconds=0.6, merge_gap=0.25, sparse=False):
    """Sung stretches: the smoothed level within `below` dB of the file's loud level (its
    98th percentile; `sparse` files, mostly silence, use their peak)."""
    env = np.convolve(P.envelope(x), np.ones(20) / 20, mode="same")
    top = env.max() if sparse else np.percentile(env, 98)
    on = env > top - below
    out, start = [], None
    for i, v in enumerate(list(on) + [False]):
        if v and start is None:
            start = i
        if not v and start is not None:
            out.append([start, i])
            start = None
    # Merge stretches split by a breath shorter than merge_gap.
    merged = []
    for s, e in out:
        if merged and (s - merged[-1][1]) * P.ENV_HOP / RATE < merge_gap:
            merged[-1][1] = e
        else:
            merged.append([s, e])
    return [(s * P.ENV_HOP, e * P.ENV_HOP) for s, e in merged if (e - s) * P.ENV_HOP / RATE >= min_seconds], env


def _peak_db(ls, binhz, f, cents=40):
    lo, hi = int(f * 2 ** (-cents / 1200) / binhz), int(f * 2 ** (cents / 1200) / binhz) + 1
    if hi >= len(ls) - 1 or lo < 1:
        return None
    around = ls[int(f * 2 ** (-200 / 1200) / binhz):int(f * 2 ** (200 / 1200) / binhz) + 1]
    return float(ls[lo:hi].max() - np.median(around))


def series_pitch(seg, fmin=38.0, fmax=1000.0):
    """MIDI pitch of the strongest harmonic series, moved down an octave while the lower
    octave's odd harmonics are there; refined on the first six harmonics."""
    n = 1 << 17
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n))
    ls = 20 * np.log10(spec + 1e-12)
    binhz = RATE / n
    best = None
    for m in np.arange(12 * math.log2(fmin / 440) + 69, 12 * math.log2(fmax / 440) + 69, 0.05):
        f0 = 440 * 2 ** ((m - 69) / 12)
        hs = [_peak_db(ls, binhz, k * f0) for k in range(1, 9)]
        hs = [h for h in hs if h is not None]
        if len(hs) < 3:
            continue
        score = float(np.mean(np.clip(hs, 0, 40)))
        if best is None or score > best[0] + 0.01:
            best = (score, m)
    if best is None:
        return None
    m = best[1]
    while True:
        f = 440 * 2 ** ((m - 12 - 69) / 12)
        if f < fmin:
            break
        odd = [_peak_db(ls, binhz, k * f) for k in (1, 3, 5)]
        odd = [o for o in odd if o is not None]
        if sum(o > 12 for o in odd) >= 2:
            m -= 12
        else:
            break
    return refine(seg, 440 * 2 ** ((m - 69) / 12))


def refine(seg, f0):
    n = 1 << 18
    p = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n)) ** 2
    fr = np.fft.rfftfreq(n, 1.0 / RATE)
    for _ in range(3):
        tot = w = 0.0
        for k in range(1, 7):
            lo, hi = np.searchsorted(fr, [k * f0 * 2 ** (-40 / 1200), k * f0 * 2 ** (40 / 1200)])
            if hi >= len(fr) or hi <= lo:
                continue
            c = 1200 * np.log2(fr[lo:hi] / (k * f0))
            tot += float(np.sum(p[lo:hi] * c))
            w += float(np.sum(p[lo:hi]))
        if w <= 0:
            break
        f0 *= 2 ** (tot / w / 1200)
    return 69 + 12 * math.log2(f0 / 440.0)


def harmonic_peak(seg, near):
    """The MIDI pitch within 60 cents of `near` whose first four harmonics hold the most
    energy in seg (Hann window, 2-cent then 0.4-cent steps)."""
    n = 1 << int(math.ceil(math.log2(len(seg) * 4)))
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n)) ** 2
    binhz = RATE / n

    def score(m):
        f = 440 * 2 ** ((m - 69) / 12)
        total = 0.0
        for k in range(1, 5):
            i = k * f / binhz
            if i + 2 >= len(spec):
                break
            j = int(i)
            t = i - j
            total += spec[j] * (1 - t) + spec[j + 1] * t
        return total

    best = max(np.arange(near - 0.6, near + 0.6001, 0.02), key=score)
    return float(max(np.arange(best - 0.02, best + 0.0201, 0.004), key=score))


def held_pitch(y, s, e, near):
    """The median pitch over [s, e) (YIN within 6 semitones of `near`), and its wander (cents)."""
    fmin, fmax = 440 * 2 ** ((near - 6 - 69) / 12), 440 * 2 ** ((near + 6 - 69) / 12)
    seg = signal.sosfiltfilt(signal.butter(4, 0.8 * fmin, "highpass", fs=RATE, output="sos"), y[s:e])
    frames = P.yin(seg, fmin=fmin, fmax=fmax, frame=2048, hop=1024)
    good = [f for f, ap in frames if ap < 0.3]
    if len(good) < 5:
        return None, None
    med = float(np.median(good))
    return 69 + 12 * math.log2(med / 440.0), float(np.std([1200 * math.log2(f / med) for f in good]))


def cut_note(x, s, e, tail_limit=1.2):
    """A sung note: 40 ms before the stretch to its fall into the floor (at most
    `tail_limit` s past the stretch), faded. Returns (audio, start, release index)."""
    a = max(0, s - int(0.04 * RATE))
    b = min(len(x), e + int(tail_limit * RATE))
    y = x[a:b].copy()
    env = np.convolve(P.envelope(y), np.ones(6) / 6, mode="same")
    peak = env.max()
    rel = e - a
    end = len(y)
    for i in range(rel // P.ENV_HOP, len(env)):
        if env[i] < peak - 50:
            end = min(len(y), (i + 6) * P.ENV_HOP)
            break
    y = y[:end]
    fin = int(0.01 * RATE)
    y[:fin] *= np.linspace(0, 1, fin)
    fout = int(np.clip(0.2 * (len(y) - rel), 0.02 * RATE, 0.3 * RATE)) if len(y) > rel else int(0.02 * RATE)
    y[-fout:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fout)))
    return y, a, min(rel, len(y) - 1)


def add_loop(y, midi, rel, report, where, max_loop=None):
    """A sustain loop for a held note; returns (audio, loop, measured root, wander)."""
    region = P.sustain_region(y[:rel + int(0.05 * RATE)])
    if region is None or region[1] - region[0] < int(0.7 * RATE):
        return y, None, midi, None
    a, b = region
    # A singer scoops into the note and settles: the loop keeps clear of the first 0.6 s
    # and, unless asked otherwise, takes the steadier last 1.8 s of the hold.
    a = max(a, int(0.6 * RATE))
    a = max(a, b - int((max_loop or 1.8) * RATE))
    if b - a < int(0.6 * RATE):
        a = max(0, b - int(0.6 * RATE))
    loop = P.find_loop(y, 440 * 2 ** ((midi - 69) / 12), (a, b))
    if not loop:
        return y, None, midi, None
    s, e, cost, _ = loop
    held, wander = held_pitch(y, s, e, midi)
    if held is None:
        held, wander = midi, None
    else:
        # The pitch the loop sounds at: where its first four harmonics carry the most energy
        # together (a long window over the loop: vibrato and drift averaged, as heard).
        held = harmonic_peak(y[s:e], held)
    xf = int(min(0.2, 0.3 * (e - s) / RATE) * RATE)
    y = P.crossfade_loop(y, s, e, xf)
    if cost > 0.15:
        report["warnings"].append(f"{where}: the best loop matches loosely ({cost:.3f}); crossfaded")
    return y, (s, e, region[1]), held, wander


def octave_fit(found, labels):
    """Each detected pitch moved to the octave the sweep's order puts it in."""
    offs = [f - l for f, l in zip(found, labels) if f is not None]
    if not offs:
        return found, 0
    octave = 12 * int(round(float(np.median(offs)) / 12))
    out = []
    for f, l in zip(found, labels):
        if f is None:
            out.append(None)
            continue
        target = l + octave
        out.append(f + 12 * round((target - f) / 12))
    return out, octave


def write_set(spec, zones, out_dir, report, unpitched=False):
    """Levels (one gain for the instrument: its loudest peak at -1 dBFS), key ranges,
    files and the mapping."""
    sid = spec["id"]
    os.makedirs(os.path.join(out_dir, sid), exist_ok=True)
    peak = max([np.abs(z["audio"]).max() for z in zones] + [1e-9])
    gain = 10 ** (-1 / 20) / peak
    layers = spec.get("layerNames", ["all"])
    for z in zones:
        z["audio"] = z["audio"] * gain
    # Round-robins per (layer, pitch cluster): takes within half a semitone share a key.
    for li in range(len(layers)):
        mine = sorted([z for z in zones if z["layer"] == li], key=lambda z: z["root"])
        clusters = []
        for z in mine:
            if clusters and (unpitched or abs(z["root"] - clusters[-1][0]["root"]) < 1.0):
                clusters[-1].append(z)
            else:
                clusters.append([z])
        for c in clusters:
            for r, z in enumerate(sorted(c, key=lambda z: z["order"])):
                z["rr"] = r
        roots = [float(np.mean([z["root"] for z in c])) for c in clusters]
        for k, c in enumerate(clusters):
            if unpitched:
                lo, hi = 0, 127
            else:
                r = roots[k]
                lo = math.floor((roots[k - 1] + r) / 2) + 1 if k > 0 else int(round(r)) - spec.get("stretchDown", 12)
                hi = math.floor((roots[k + 1] + r) / 2) if k + 1 < len(clusters) else int(round(r)) + spec.get("stretchUp", 12)
            for z in c:
                z["loKey"], z["hiKey"] = max(0, lo), min(127, hi)
    out = []
    for z in zones:
        name = f"{z['name']}-{layers[z['layer']]}-{z['rr'] + 1}.flac".replace("#", "s").replace(" ", "-")
        sf.write(os.path.join(out_dir, sid, name), z["audio"].astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
        a = z["audio"]
        env = P.envelope(a[:int(2.0 * RATE)])
        top = int(np.argmax(np.convolve(env, np.ones(10) / 10, mode="same"))) * P.ENV_HOP
        head = a[top:top + int(0.3 * RATE)]
        m = {"file": f"{sid}/{name}", "label": z["label"], "root": round(float(z["root"]), 3), "loKey": z["loKey"], "hiKey": z["hiKey"],
             "loVel": z["velocity"][0], "hiVel": z["velocity"][1], "layer": z["layer"], "rr": z["rr"], "length": len(a),
             "rms": round(float(P.db(np.sqrt(np.mean(head ** 2)))), 2), "loudness": round(P.note_loudness(a), 2),
             "peak": round(float(P.db(np.abs(a).max())), 2), "source": z["source"], "at": round(z["at"] / RATE, 3)}
        if z.get("loop"):
            m["loopStart"], m["loopEnd"], m["release"] = int(z["loop"][0]), int(z["loop"][1]), int(z["loop"][2])
        out.append(m)
    out.sort(key=lambda m: (m["root"], m["layer"], m["rr"]))
    mapping = {"id": sid, "name": spec["name"], "kind": "voice", "mode": spec["mode"], "rate": RATE, "channels": 1,
               "layers": layers, "gainDb": round(20 * math.log10(gain), 2), "unpitched": unpitched, "zones": out}
    with open(os.path.join(out_dir, f"{sid}.json"), "w") as f:
        json.dump(mapping, f, indent=1)
    return mapping


def file_entry(report, f, sid, layer, fmt, corr, found, expected):
    entry = {"file": f, "set": sid, "layer": layer, "format": fmt,
             "stereo": "dual mono" if corr > 0.999 else f"stereo (L/R correlation {corr:.3f}, mixed to mono)",
             "noiseFloorDb": None, "expected": expected, "found": found, "candidates": found, "riseNeededDb": 0,
             "rejected": [], "notes": []}
    report["files"].append(entry)
    return entry


def run_sweep(spec, locate, report):
    sid = spec["id"]
    zones = []
    order = 0
    files = [(l, False) for l in spec["layers"]] + [(l, True) for l in spec.get("falsetto", [])]
    for l, falsetto in files:
        path = locate(l["file"])
        if path is None:
            report["problems"].append(f"{sid}: missing file '{l['file']}'")
            continue
        x, corr, fmt = load(path)
        segs, _ = segments(x, below=l.get("below", 22.0))
        labels = [P.midi_of(n) for n in l["notes"]]
        if len(segs) > len(labels):
            # Keep the longest stretches (a breath or a false start is short), in order.
            segs = sorted(sorted(segs, key=lambda s: -(s[1] - s[0]))[:len(labels)])
        found = []
        for s, e in segs:
            a = s + int(0.3 * RATE)
            b = min(e - int(0.15 * RATE), a + int(1.5 * RATE))
            found.append(series_pitch(x[a:b]) if b - a > 8000 else None)
        fitted, octave = octave_fit(found, labels[:len(found)])
        entry = file_entry(report, l["file"], sid, l["name"] + (" (falsetto)" if falsetto else ""), fmt, corr, len(segs), len(labels))
        if len(segs) != len(labels):
            report["problems"].append(f"{sid}/{l['file']}: expected {len(labels)} notes, found {len(segs)}")
        if octave:
            report.setdefault("octaves", []).append(f"{l['file']}: sung {octave // 12:+d} octave(s) from the file's note names")
        for k, ((s, e), midi) in enumerate(zip(segs, fitted)):
            label = labels[k] if k < len(labels) else None
            where = f"{sid}/{l['name']} note {k + 1} at {s / RATE:.2f}s"
            if midi is None:
                report["problems"].append(f"{where}: no pitch; skipped")
                continue
            y, at, rel = cut_note(x, s, e)
            y, loop, held, wander = add_loop(y, midi, rel, report, where)
            if loop is None:
                report["problems"].append(f"{where}: no sustain loop (too short)")
            nominal = int(round(held))
            cents = 100 * (held - nominal)
            note = {"label": P.name_of(nominal), "at": round(at / RATE, 2), "seconds": round(len(y) / RATE, 2),
                    "detected": f"{P.name_of(nominal)} {cents:+.0f} cents" + (f" (file says {P.name_of(label)})" if label is not None and nominal != label else "")}
            if loop:
                note["loop"] = f"{loop[0] / RATE:.2f}-{loop[1] / RATE:.2f}s"
                note["loop wander"] = f"{wander:.0f} cents" if wander is not None else "?"
            if abs(cents) > 30:
                report["warnings"].append(f"{where}: sung {cents:+.0f} cents from {P.name_of(nominal)} (the sampler plays it in tune)")
            entry["notes"].append(note)
            layers = [l["layer"]] if not falsetto else list(range(len(spec["layerNames"])))
            for li in layers:
                zones.append({"name": P.name_of(nominal) + ("f" if falsetto else ""), "label": P.name_of(nominal), "root": held, "layer": li,
                              "velocity": spec["velocity"][li], "audio": y, "source": l["file"], "at": at, "loop": loop, "order": order})
                order += 1
    return zones


def run_takes(spec, locate, report):
    sid = spec["id"]
    zones = []
    order = 0
    for l in spec["files"]:
        path = locate(l["file"])
        if path is None:
            report["problems"].append(f"{sid}: missing file '{l['file']}'")
            continue
        x, corr, fmt = load(path)
        segs, _ = segments(x, below=l.get("below", 20.0), min_seconds=l.get("minSeconds", 1.0), merge_gap=l.get("mergeGap", 0.25))
        if "takes" in l and len(segs) > l["takes"]:
            segs = sorted(sorted(segs, key=lambda s: -(s[1] - s[0]))[:l["takes"]])
        entry = file_entry(report, l["file"], sid, l.get("name", "takes"), fmt, corr, len(segs), l.get("takes", "varies"))
        if "takes" in l and len(segs) != l["takes"]:
            report["problems"].append(f"{sid}/{l['file']}: expected {l['takes']} takes, found {len(segs)}")
        label = P.midi_of(l["note"]) if "note" in l else None
        pitches = []
        for s, e in segs:
            a = s + int(0.3 * RATE)
            b = min(e - int(0.15 * RATE), a + int(2.0 * RATE))
            pitches.append(series_pitch(x[a:b]) if b - a > 8000 else None)
        # One octave for the file (the takes are one note): the median's.
        good = [p for p in pitches if p is not None]
        centre = float(np.median(good)) if good else None
        for k, ((s, e), midi) in enumerate(zip(segs, pitches)):
            where = f"{sid}/{l.get('name', l['file'])} take {k + 1} at {s / RATE:.2f}s"
            if midi is None:
                report["problems"].append(f"{where}: no pitch; skipped")
                continue
            if centre is not None and abs(midi - centre) > 6 and not l.get("octavesApart"):
                midi += 12 * round((centre - midi) / 12)
            y, at, rel = cut_note(x, s, e, tail_limit=l.get("tail", 1.2))
            y, loop, held, wander = add_loop(y, midi, rel, report, where, max_loop=l.get("maxLoop"))
            if loop is None:
                report["problems"].append(f"{where}: no sustain loop (too short)")
            nominal = int(round(held))
            cents = 100 * (held - nominal)
            note = {"label": P.name_of(nominal), "at": round(at / RATE, 2), "seconds": round(len(y) / RATE, 2),
                    "detected": f"{P.name_of(nominal)} {cents:+.0f} cents" + (f" (file says {P.name_of(label)})" if label is not None and nominal != label else "")}
            if loop:
                note["loop"] = f"{loop[0] / RATE:.2f}-{loop[1] / RATE:.2f}s"
                note["loop wander"] = f"{wander:.0f} cents" if wander is not None else "?"
            entry["notes"].append(note)
            li = l.get("layer", 0)
            zones.append({"name": l.get("name", "take").replace(" ", "-") + "-" + P.name_of(nominal), "label": P.name_of(nominal), "root": held, "layer": li,
                          "velocity": spec.get("velocity", [[0.0, 1.0]])[li], "audio": y, "source": l["file"], "at": at, "loop": loop, "order": order})
            order += 1
    return zones


def run_texture(spec, locate, report):
    sid = spec["id"]
    zones = []
    order = 0
    for l in spec["files"]:
        path = locate(l["file"])
        if path is None:
            report["problems"].append(f"{sid}: missing file '{l['file']}'")
            continue
        x, corr, fmt = load(path)
        if spec["cut"] == "whole":
            env = np.convolve(P.envelope(x), np.ones(20) / 20, mode="same")
            live = np.nonzero(env > env.max() - 40)[0]
            a, b = int(live[0]) * P.ENV_HOP, min(len(x), (int(live[-1]) + 20) * P.ENV_HOP)
            y = x[a:b].copy()
            # One long loop over (nearly) all of it, crossfaded over half a second, so a
            # held note murmurs on without a seam.
            s, e = int(0.4 * RATE), len(y) - int(0.4 * RATE)
            xf = int(0.5 * RATE)
            y = P.crossfade_loop(y, s, e, xf)
            fin = int(0.02 * RATE)
            y[:fin] *= np.linspace(0, 1, fin)
            entry = file_entry(report, l["file"], sid, l["name"], fmt, corr, 1, 1)
            entry["notes"].append({"label": l["name"], "at": round(a / RATE, 2), "seconds": round(len(y) / RATE, 2), "detected": "unpitched",
                                   "loop": f"{s / RATE:.2f}-{e / RATE:.2f}s"})
            zones.append({"name": l["name"], "label": l["name"], "root": 60.0, "layer": 0, "velocity": [0.0, 1.0], "audio": y,
                          "source": l["file"], "at": a, "loop": (s, e, e), "order": l.get("order", order)})
            order += 1
        else:
            segs, _ = segments(x, below=l.get("below", 30.0), min_seconds=l.get("minSeconds", 0.25), merge_gap=0.12, sparse=True)
            entry = file_entry(report, l["file"], sid, l["name"], fmt, corr, len(segs), "varies")
            for s, e in segs:
                y, at, _ = cut_note(x, s, e, tail_limit=0.4)
                if len(y) > int(l.get("maxSeconds", 4.0) * RATE):
                    y = y[:int(l.get("maxSeconds", 4.0) * RATE)]
                    fout = int(0.2 * RATE)
                    y[-fout:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fout)))
                entry["notes"].append({"label": l["name"], "at": round(at / RATE, 2), "seconds": round(len(y) / RATE, 2), "detected": "unpitched"})
                zones.append({"name": l["name"], "label": l["name"], "root": 60.0, "layer": 0, "velocity": [0.0, 1.0], "audio": y,
                              "source": l["file"], "at": at, "loop": None, "order": order})
                order += 1
    return zones


def run(spec, source, out_dir, report, locate):
    print(f"[{spec['id']}] ...", flush=True)
    mode = spec["mode"]
    if mode == "sweep":
        zones = run_sweep(spec, locate, report)
    elif mode == "takes":
        zones = run_takes(spec, locate, report)
    else:
        zones = run_texture(spec, locate, report)
    if not zones:
        return []
    # Velocity layers in loudness order (soft under full), per pitch: reported.
    if mode == "sweep" and len(spec.get("layerNames", [])) > 1:
        for label in sorted({z["label"] for z in zones}, key=P.midi_of):
            lv = []
            for li in range(len(spec["layerNames"])):
                mine = [P.note_loudness(z["audio"]) for z in zones if z["label"] == label and z["layer"] == li]
                lv.append(float(np.mean(mine)) if mine else None)
            if None not in lv and any(lv[i] >= lv[i + 1] for i in range(len(lv) - 1)):
                report["warnings"].append(f"{spec['id']} {label}: the soft take is as loud as the full one ({', '.join(f'{v:.1f}' for v in lv)} LUFS)")
    return [write_set(spec, zones, out_dir, report, unpitched=(mode == "texture"))]
