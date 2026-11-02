"""Struck one-shots (Milestone 17): hand drums and other percussion hits.

Used by prepare.py for the manifest's "kind": "hits" sets:

    {"id": "djembe", "kind": "hits", "hitsPerFile": 9, "layers": ["soft", "medium", "hard"],
     "velocity": [[0, 0.4], [0.4, 0.75], [0.75, 1]],
     "files": [{"file": ..., "stroke": "big-bass", "key": 36}, ...]}

Each recording is one stroke, played three times per dynamic (soft, medium, hard: nine
hits, about a second or two apart). The three hits of a dynamic become round-robins, the
dynamics velocity layers. One instrument holds every stroke, each on its own key
(`key`), so a kit part plays a stroke by its key. Per recording:

 1. Decode (24-bit WAV, dual mono). No noise reduction (the exports are gated: digital
    silence between the hits).
 2. Hits: the level rising 15 dB out of the 50 ms before it; the `hitsPerFile` loudest
    are the hits (the hand's own noises between them, 40-70 dB down, are not).
 3. Layers: the notes say soft, medium, hard in that order, three each. The player's
    dynamics do not always follow (a medium slap louder than the hard ones): the hits
    are ordered by how loud they sound (K-weighted, their first 100 ms) and the three
    quietest are soft, the next three medium, the loudest hard. Where that differs from
    the recorded order it is reported.
 4. Each hit runs until it has fallen 60 dB under its peak or the next hit starts (at
    most `maxSeconds`), with a 2 ms lead-in and a cosine fade-out.
 5. Pitch: each stroke's strongest low resonance (the bass's Helmholtz boom, the head's
    tone), reported. The kit plays strokes at their recorded pitch; a Place transposes
    them.
 6. Levels: the exports were normalised one file at a time (every file peaks at
    -0.1 dBFS), so the strokes' levels against each other are not in the recordings:
    every stroke keeps its own (hardest hit's peak at -1 dBFS) and the Places set each
    row's level. Within a stroke the dynamics are as played.
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
    live = np.abs(left) > 1e-4
    corr = float(np.corrcoef(left[live], right[live])[0, 1]) if live.sum() > 1000 and audio.shape[1] > 1 else 1.0
    x = signal.sosfiltfilt(signal.butter(2, 20, "highpass", fs=RATE, output="sos"), audio.mean(axis=1))
    info = sf.info(path)
    return x, corr, f"{info.subtype}, {info.channels} ch"


def find_hits(x, count):
    env = P.envelope(x)
    sm = np.convolve(env, np.ones(2) / 2, mode="same")
    top = sm.max()
    found, i = [], 10
    while i < len(sm) - 1:
        if sm[i] - sm[max(0, i - 10):i].min() >= 15 and sm[i] > top - 60:
            peak = i + int(np.argmax(sm[i:i + 20]))
            start = i
            while start > 0 and sm[start - 1] < sm[start] and sm[start] > sm[max(0, i - 10):i].min() + 3:
                start -= 1
            found.append({"start": start, "peak": peak, "level": float(sm[peak])})
            i = peak + 40
        else:
            i += 1
    keep = sorted(sorted(found, key=lambda h: -h["level"])[:count], key=lambda h: h["start"])
    dropped = [h for h in found if h not in keep]
    return keep, dropped, env


def hit_loudness(y):
    k = P.k_weight(y[:int(0.1 * RATE)])
    return float(-0.691 + 10 * np.log10(np.mean(k ** 2) + 1e-24))


def resonance(y):
    """The strongest spectral peak between 40 and 900 Hz in the hit's first 300 ms (Hz)."""
    seg = y[int(0.005 * RATE):int(0.3 * RATE)]
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 1 << 16))
    freqs = np.fft.rfftfreq(1 << 16, 1.0 / RATE)
    band = (freqs > 40) & (freqs < 900)
    return float(freqs[band][np.argmax(spec[band])])


def run(spec, source, out_dir, report, locate):
    sid = spec["id"]
    print(f"[{sid}] ...", flush=True)
    count = int(spec.get("hitsPerFile", 9))
    layers = spec.get("layers", ["soft", "medium", "hard"])
    velocity = spec.get("velocity", [[0.0, 0.4], [0.4, 0.75], [0.75, 1.0]])
    per = count // len(layers)
    max_seconds = float(spec.get("maxSeconds", 2.0))
    os.makedirs(os.path.join(out_dir, sid), exist_ok=True)
    zones = []
    strokes = {}
    for f in spec["files"]:
        path = locate(f["file"])
        if path is None:
            report["problems"].append(f"{sid}: missing file '{f['file']}'")
            continue
        x, corr, fmt = load(path)
        hits, dropped, env = find_hits(x, count)
        entry = {"file": f["file"], "set": sid, "layer": f["stroke"], "format": fmt,
                 "stereo": "dual mono" if corr > 0.999 else f"stereo (L/R correlation {corr:.3f}, mixed to mono)",
                 "noiseFloorDb": round(float(np.percentile(env, 20)), 1), "expected": count, "found": len(hits),
                 "candidates": len(hits) + len(dropped), "riseNeededDb": 15,
                 "rejected": [f"{h['start'] * P.ENV_HOP / RATE:.2f}s ({h['level']:.0f} dB: a hand noise)" for h in dropped], "notes": []}
        report["files"].append(entry)
        if len(hits) != count:
            report["problems"].append(f"{sid}/{f['stroke']}: expected {count} hits, found {len(hits)}")
        cut = []
        for k, h in enumerate(hits):
            a = max(0, h["start"] * P.ENV_HOP - int(0.002 * RATE))
            stop = hits[k + 1]["start"] * P.ENV_HOP - int(0.005 * RATE) if k + 1 < len(hits) else len(x)
            stop = min(stop, a + int(max_seconds * RATE))
            y = x[a:stop].copy()
            sm = np.convolve(P.envelope(y), np.ones(6) / 6, mode="same")
            live = np.nonzero(sm > sm.max() - 50)[0]
            if len(live):
                y = y[:min(len(y), (int(live[-1]) + 6) * P.ENV_HOP)]
            fin = int(0.002 * RATE)
            y[:fin] *= np.linspace(0, 1, fin)
            fout = int(np.clip(0.25 * len(y), 0.01 * RATE, 0.15 * RATE))
            y[-fout:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fout)))
            cut.append({"audio": y, "at": a, "order": k, "loud": hit_loudness(y)})
        # Layers by loudness; the recorded order is reported where it differs.
        by_level = sorted(cut, key=lambda c: c["loud"])
        moved = []
        for rank, c in enumerate(by_level):
            c["layer"] = min(len(layers) - 1, rank // per)
            recorded = min(len(layers) - 1, c["order"] // per)
            if recorded != c["layer"]:
                moved.append(f"hit {c['order'] + 1} (played {layers[recorded]}, sounds {layers[c['layer']]})")
        if moved:
            report["warnings"].append(f"{sid}/{f['stroke']}: by loudness, " + "; ".join(moved))
        hz = resonance(max(cut, key=lambda c: c["loud"])["audio"]) if cut else 0.0
        strokes[f["stroke"]] = {"key": f["key"], "resonanceHz": round(hz, 1), "note": P.name_of(69 + 12 * math.log2(hz / 440.0)) if hz > 0 else "?"}
        rr = {}
        for c in sorted(cut, key=lambda c: c["order"]):
            r = rr.get(c["layer"], 0)
            rr[c["layer"]] = r + 1
            c["rr"] = r
            entry["notes"].append({"label": f"{f['stroke']} {layers[c['layer']]} {r + 1}", "at": round(c["at"] / RATE, 2),
                                   "seconds": round(len(c["audio"]) / RATE, 2), "detected": f"{c['loud']:.1f} LUFS"})
        # One gain per stroke: its hardest hit's peak at -1 dBFS.
        peak = max([np.abs(c["audio"]).max() for c in cut] + [1e-9])
        gain = 10 ** (-1 / 20) / peak
        for c in cut:
            a = c["audio"] * gain
            name = f"{f['stroke']}-{layers[c['layer']]}-{c['rr'] + 1}.flac"
            sf.write(os.path.join(out_dir, sid, name), a.astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
            env_z = P.envelope(a[:int(0.5 * RATE)])
            top = int(np.argmax(env_z)) * P.ENV_HOP
            head = a[top:top + int(0.1 * RATE)]
            zones.append({"file": f"{sid}/{name}", "label": f["stroke"], "root": float(f["key"]), "loKey": f["key"], "hiKey": f["key"],
                          "loVel": velocity[c["layer"]][0], "hiVel": velocity[c["layer"]][1], "layer": c["layer"], "rr": c["rr"],
                          "length": len(a), "rms": round(float(P.db(np.sqrt(np.mean(head ** 2)))), 2), "loudness": round(hit_loudness(a), 2),
                          "peak": round(float(P.db(np.abs(a).max())), 2), "source": f["file"], "at": round(c["at"] / RATE, 3)})
        # The layers' loudness in order (soft < medium < hard), by construction; reported.
        means = [np.mean([hit_loudness(c["audio"] * gain) for c in cut if c["layer"] == l]) for l in range(len(layers)) if any(c["layer"] == l for c in cut)]
        report.setdefault("layerSteps", {}).setdefault(sid, {})[f["stroke"]] = {layers[i]: round(float(m - means[-1]), 1) for i, m in enumerate(means)}
    if not zones:
        return []
    zones.sort(key=lambda z: (z["loKey"], z["layer"], z["rr"]))
    mapping = {"id": sid, "name": spec["name"], "kind": "hits", "rate": RATE, "channels": 1, "layers": layers, "gainDb": 0.0,
               "strokes": strokes, "zones": zones}
    with open(os.path.join(out_dir, f"{sid}.json"), "w") as f:
        json.dump(mapping, f, indent=1)
    report.setdefault("strokes", {})[sid] = strokes
    return [mapping]
