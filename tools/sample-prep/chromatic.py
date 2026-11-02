"""Chromatic acoustic-piano sweeps: every key, lowest to highest, one note at a time.

Used by prepare.py for the manifest's "kind": "piano" sets:

    {"id": "piano", "kind": "piano", "lowest": "A0", "keys": 88,
     "short": [{"file": ..., "name": "soft", "velocity": [0, 0.4]}, ...medium, full],
     "long": {"file": ..., "name": "medium"}}

Four recordings of the same piano at the same input gain (the files were normalised on
export, so their gains come back from their noise floors): three sweeps of short hits
(soft, medium, full: the velocity layers) and one of held notes at medium strength (the
held articulation). Per recording:

 1. Decode (24-bit WAV, dual mono), a 20 Hz high-pass. No noise reduction (the pre-roll
    is 70-84 dB under the notes).
 2. Onsets: every rise in the spectrum (flux) is a candidate; each key is assigned, in
    order, the candidate where its own partials rise the most (a damper's thump, a pedal
    or a re-strike never shifts the labels after it). A key with no clear rise is
    reported missing. The onset is then refined to the hammer's attack (1 ms).
 3. The previous key ringing into this one (low strings ring on): where it is still
    above -45 dB of the new note at the onset, its partials are fitted just before the
    onset (frequency, amplitude, phase, decay) and the extrapolated tail is subtracted.
 4. Pitch: each key's partials (inharmonic: f_n = n f0 sqrt(1 + B n^2)) are measured and
    f0 and B fitted. The deviation from equal temperament across the keys is the
    piano's stretch tuning; a smooth curve through it (a running median) is kept, its
    offset from A440 (the median over F3-F5) taken off, and keys clearly off the curve
    (unisons drifted: more than `outlierCents`) are pulled onto it. Everything else
    stays as the tuner left it.
 5. Short hits end once they have faded into the floor (the damper is recorded). Held
    notes: the damper point (where the decay steepens) is found and the recording up to
    it is kept; beyond it, the note's natural decay continues as additive resynthesis of
    its partials (fitted on the last half second of the hold: frequency, decay,
    amplitude, phase), crossfaded in phase, so a key held longer than it was recorded
    rings on and dies away naturally. No loops.
 6. Bad keys (no onset, no pitch, or far quieter or louder than their neighbours) are
    replaced by the nearest good key of the same layer (shifted by the sampler) and
    reported.
 7. Levels: each file's gain from its noise floor (the same room and preamp gain), the
    velocity layers' steps smoothed across the keys so they rise evenly, the held notes
    matched to the medium short hits at the attack. The soft and full held notes are
    derived from the medium ones at runtime: each carries a gain and a high-shelf
    (brightness) measured from the short layers of the same key.
 8. Output: `piano-short` (three velocity layers) and `piano-long` (the held notes, with
    the derived layers as extra zones that share the medium audio).
"""

import json
import math
import os

import numpy as np
import soundfile as sf
from scipy import signal

import prepare as P

RATE = P.RATE


def hz(midi):
    return 440.0 * 2 ** ((midi - 69) / 12)


def db(x):
    return 20.0 * np.log10(np.maximum(np.abs(x), 1e-12))


def load(path):
    audio, rate = sf.read(path, dtype="float64", always_2d=True)
    if rate != RATE:
        raise SystemExit(f"{path}: {rate} Hz (expected {RATE})")
    left, right = audio[:, 0], audio[:, -1]
    live = np.abs(left) > 1e-4
    corr = float(np.corrcoef(left[live][:400000], right[live][:400000])[0, 1]) if live.sum() > 1000 and audio.shape[1] > 1 else 1.0
    x = audio.mean(axis=1)
    x = signal.sosfiltfilt(signal.butter(2, 20, "highpass", fs=RATE, output="sos"), x)
    info = sf.info(path)
    return x, corr, f"{info.subtype}, {info.channels} ch"


def floor_db(x, first_onset):
    """The noise floor (dBFS RMS) of the pre-roll, above 100 Hz (rumble varies)."""
    pre = x[int(0.05 * RATE):max(int(0.1 * RATE), first_onset - int(0.1 * RATE))]
    if len(pre) < RATE // 4:
        pre = x[-RATE:]
    hp = signal.sosfilt(signal.butter(2, 100, "highpass", fs=RATE, output="sos"), pre)[1000:]
    return float(10 * np.log10(np.mean(hp ** 2) + 1e-24))


# ---------------------------------------------------------------------------
# Onsets

FLUX_HOP = 441


def candidates(x):
    _, _, z = signal.stft(x, RATE, nperseg=2048, noverlap=2048 - FLUX_HOP, boundary=None)
    m = np.log1p(1000 * np.abs(z))
    flux = np.maximum(m[:, 1:] - m[:, :-1], 0).sum(axis=0)
    flux = flux / max(np.median(flux[flux > 0]), 1e-9)
    peaks, _ = signal.find_peaks(flux, height=3.0, distance=int(0.12 * RATE / FLUX_HOP))
    return [int((p + 1) * FLUX_HOP) for p in peaks]


SPEC_N = 1 << 15


def power_spec(x, a, b):
    a = max(0, a)
    seg = x[a:b]
    if len(seg) < 512:
        return None
    return np.abs(np.fft.rfft(seg * np.hanning(len(seg)), SPEC_N)) ** 2


def key_energy(spec, key, partials=6):
    binhz = RATE / SPEC_N
    f0 = hz(key)
    b = inharmonicity_guess(key)
    total = 0.0
    for n in range(1, partials + 1):
        fn = n * f0 * math.sqrt(1 + b * n * n)
        if fn > 16000:
            break
        lo, hi = int(fn * 2 ** (-70 / 1200) / binhz), int(fn * 2 ** (70 / 1200) / binhz) + 1
        total += float(spec[lo:hi].sum()) * (1.0 if n == 1 else 0.7)
    return total + 1e-18


def inharmonicity_guess(key):
    # Typical values: ~1e-4 in the tenor, rising toward both ends (short bass strings
    # wound, short treble strings stiff).
    return 1.2e-4 * 2 ** ((key - 60) / 12 * 0.9) + 2.5e-4 * 2 ** (-(key - 21) / 6)


def assign(x, cands, lowest, keys):
    """One candidate per key, in order, by how much the key's own partials rise there."""
    before = [power_spec(x, c - int(0.30 * RATE), c - int(0.02 * RATE)) for c in cands]
    after = [power_spec(x, c + int(0.02 * RATE), c + int(0.30 * RATE)) for c in cands]
    score = np.zeros((len(cands), keys))
    for i in range(len(cands)):
        if before[i] is None or after[i] is None:
            continue
        for k in range(keys):
            r = 10 * np.log10(key_energy(after[i], lowest + k) / key_energy(before[i], lowest + k))
            score[i, k] = np.clip(r, 0.0, 40.0)
    n, m = len(cands), keys
    neg, miss = -1.0e9, 6.0
    best = np.full((n + 1, m + 1), neg)
    best[:, 0] = 0.0
    how = np.zeros((n + 1, m + 1), dtype=np.int8)
    for j in range(1, m + 1):
        best[0][j] = best[0][j - 1] - miss
        how[0][j] = 2
    for i in range(1, n + 1):
        for j in range(1, m + 1):
            opts = (best[i - 1][j], best[i - 1][j - 1] + score[i - 1][j - 1] - 1.0, best[i][j - 1] - miss)
            k = int(np.argmax(opts))
            best[i][j], how[i][j] = opts[k], k
    out, i, j = {}, n, m
    while j > 0:
        k = how[i][j]
        if i == 0 or k == 2:
            j -= 1
        elif k == 1:
            out[j - 1] = (cands[i - 1], float(score[i - 1][j - 1]))
            i, j = i - 1, j - 1
        else:
            i -= 1
    return out


def refine_onset(x, at):
    """The hammer's attack near a flux candidate: the steepest rise of the 1 ms energy
    within 40 ms either side, less 3 ms."""
    a, b = max(0, at - int(0.04 * RATE)), min(len(x), at + int(0.04 * RATE))
    seg = x[a:b]
    if len(seg) < 200:
        return at
    hop = 44
    e = np.array([np.mean(seg[i:i + hop] ** 2) for i in range(0, len(seg) - hop, hop)])
    e = 10 * np.log10(e + 1e-20)
    sm = np.convolve(e, np.ones(3) / 3, mode="same")
    rise = sm[3:] - sm[:-3]
    k = int(np.argmax(rise))
    return max(0, a + k * hop - int(0.003 * RATE))


# ---------------------------------------------------------------------------
# Pitch: inharmonic partials

def partial_peaks(seg, key, max_partials=14):
    """[(n, freq, amplitude)] of the partials found near their predicted places."""
    n_fft = 1 << 18
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n_fft))
    binhz = RATE / n_fft
    f0, b = hz(key), inharmonicity_guess(key)
    top = spec.max()
    out = []
    for n in range(1, max_partials + 1):
        fn = n * f0 * math.sqrt(1 + b * n * n)
        if fn > 9000:
            break
        lo, hi = int(fn * 2 ** (-60 / 1200) / binhz), int(fn * 2 ** (60 / 1200) / binhz) + 1
        if hi >= len(spec) - 1:
            break
        i = lo + int(np.argmax(spec[lo:hi]))
        if spec[i] < top * 10 ** (-50 / 20) or i in (lo, hi - 1):
            continue
        a, c0, c = np.log(spec[i - 1] + 1e-20), np.log(spec[i] + 1e-20), np.log(spec[i + 1] + 1e-20)
        shift = 0.5 * (a - c) / (a - 2 * c0 + c) if (a - 2 * c0 + c) != 0 else 0.0
        out.append((n, (i + shift) * binhz, float(spec[i])))
        # Refine the inharmonicity guess as partials come in.
        if len(out) >= 3:
            fit = fit_f0_b(out)
            if fit:
                f0, b = fit
    return out


def fit_f0_b(peaks):
    """f_n^2 / n^2 = f0^2 + f0^2 B n^2: a line in n^2 (weighted by amplitude)."""
    if len(peaks) < 2:
        return None
    n = np.array([p[0] for p in peaks], dtype=float)
    f = np.array([p[1] for p in peaks])
    w = np.sqrt(np.array([p[2] for p in peaks]))
    y = (f / n) ** 2
    A = np.vstack([np.ones_like(n), n ** 2]).T
    coef, *_ = np.linalg.lstsq(A * w[:, None], y * w, rcond=None)
    f02, slope = coef
    if f02 <= 0:
        return None
    f0 = math.sqrt(f02)
    return f0, float(np.clip(slope / f02, 0.0, 0.02))


def measure_pitch(seg, key):
    """(MIDI pitch of f0, B, partials used) or (None, None, 0)."""
    peaks = partial_peaks(seg, key)
    if not peaks:
        return None, None, 0
    if key >= 90 or len(peaks) == 1:
        # The top: the fundamental alone is the pitch (few partials, the rest above 9 kHz).
        p1 = [p for p in peaks if p[0] == 1]
        if not p1:
            return None, None, 0
        return 69 + 12 * math.log2(p1[0][1] / 440.0), None, 1
    fit = fit_f0_b(peaks)
    if fit is None:
        return None, None, 0
    f0, b = fit
    return 69 + 12 * math.log2(f0 / 440.0), b, len(peaks)


# ---------------------------------------------------------------------------
# Sinusoidal models: the previous key's tail, the held note's continuation

def fit_partials(x, a, b, key, max_partials=40):
    """Partials of the note sounding in x[a:b]: [(freq, decay per sample (log), complex amp at a)]."""
    seg = x[a:b]
    if len(seg) < 2048:
        return []
    n_fft = 1 << 18
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), n_fft))
    binhz = RATE / n_fft
    f0, bb = hz(key), inharmonicity_guess(key)
    peaks = partial_peaks(seg, key, max_partials=14)
    fit = fit_f0_b(peaks) if len(peaks) >= 3 else None
    if fit:
        f0, bb = fit
    top = spec.max()
    freqs = []
    for n in range(1, max_partials + 1):
        fn = n * f0 * math.sqrt(1 + bb * n * n)
        if fn > 12000:
            break
        lo, hi = int(fn * 2 ** (-40 / 1200) / binhz), int(fn * 2 ** (40 / 1200) / binhz) + 1
        if hi >= len(spec) - 1:
            break
        i = lo + int(np.argmax(spec[lo:hi]))
        if spec[i] < top * 10 ** (-55 / 20):
            continue
        a_, c0, c = np.log(spec[i - 1] + 1e-20), np.log(spec[i] + 1e-20), np.log(spec[i + 1] + 1e-20)
        shift = 0.5 * (a_ - c) / (a_ - 2 * c0 + c) if (a_ - 2 * c0 + c) != 0 else 0.0
        freqs.append((i + shift) * binhz)
    if not freqs:
        return []
    # Decay per partial: the slope of its band's level (dB) across the window.
    t = np.arange(len(seg))
    decays = []
    win = int(0.05 * RATE)
    for f in freqs:
        # Demodulate and low-pass: the partial's envelope.
        base = seg * np.exp(-2j * np.pi * f * t / RATE)
        k = max(8, int(RATE / max(f * 0.15, 5.0)))
        env = np.abs(np.convolve(base, np.ones(k) / k, mode="same"))
        frames = np.array([env[i:i + win].mean() for i in range(0, len(env) - win, win)])
        if len(frames) < 4:
            decays.append(0.0)
            continue
        lv = 20 * np.log10(frames[1:-1] + 1e-12)
        tt = np.arange(len(lv)) * win / RATE
        slope = float(np.polyfit(tt, lv, 1)[0]) if len(lv) >= 2 else 0.0
        decays.append(float(np.clip(slope, -80.0, -0.5)))
    # Complex amplitudes by least squares with the decays fixed.
    cols = []
    for f, d in zip(freqs, decays):
        alpha = d / 20.0 * math.log(10) / RATE
        cols.append(np.exp((alpha + 2j * np.pi * f / RATE) * t))
    basis = np.array(cols).T
    real = np.hstack([basis.real, -basis.imag])
    coef, *_ = np.linalg.lstsq(real, seg, rcond=None)
    amps = coef[:len(freqs)] + 1j * coef[len(freqs):]
    return [(f, d / 20.0 * math.log(10) / RATE, amp) for f, d, amp in zip(freqs, decays, amps)]


def synth(parts, start_offset, length):
    """The partials from `start_offset` samples after the fit window's start, `length` long."""
    t = np.arange(length) + start_offset
    y = np.zeros(length)
    for f, alpha, amp in parts:
        y += np.real(amp * np.exp((alpha + 2j * np.pi * f / RATE) * t))
    return y


def model_residual_db(x, a, b, parts):
    model = synth(parts, 0, b - a)
    seg = x[a:b]
    return float(10 * np.log10(np.mean((seg - model) ** 2) / max(np.mean(seg ** 2), 1e-24)))


# ---------------------------------------------------------------------------
# Held notes: the damper point and the natural continuation

def damper_point(y):
    """Where the held note's decay steepens (the key let go), or None."""
    env = P.envelope(y)
    sm = np.convolve(env, np.ones(6) / 6, mode="same")
    if len(sm) < 200:
        return None
    peak = int(np.argmax(sm[:100]))
    start = max(peak + 120, 160)                       # at least 0.8 s in
    slope = (sm[20:] - sm[:-20]) / 0.1                 # dB per second over 100 ms
    hold = np.median(slope[peak + 40:start]) if start > peak + 60 else -10.0
    need = min(-25.0, 4.0 * hold)
    for i in range(start, len(slope) - 20):
        if slope[i] < need and slope[i + 10] < need * 0.6:
            return max(0, (i + 2) * P.ENV_HOP)
    return None


def hold_seconds(key):
    """How long a held note may ring in all (seconds): longer in the bass."""
    return float(np.clip(7.0 - 3.5 * (key - 40) / 40.0, 3.5, 7.0))


def extend(y, release, key, report, where):
    """y up to `release`, then its continuation resynthesised; returns (audio, info)."""
    fit_b = release - int(0.04 * RATE)
    fit_a = max(int(0.5 * RATE), fit_b - int(0.6 * RATE))
    if fit_b - fit_a < int(0.25 * RATE):
        return y[:release], None
    peak_db = 10 * np.log10(np.max(np.convolve(y[:release] ** 2, np.ones(441) / 441, mode="valid")) + 1e-24)
    tail_db = 10 * np.log10(np.mean(y[fit_a:fit_b] ** 2) + 1e-24)
    if tail_db < peak_db - 48:
        # Already faded into the room by the time the key was let go: nothing to continue.
        return y[:release], {"skipped": f"faded {peak_db - tail_db:.0f} dB before the damper"}
    parts = fit_partials(y, fit_a, fit_b, key)
    if not parts:
        return y[:release], None
    resid = model_residual_db(y, fit_a, fit_b, parts)
    if resid > -6:
        report["warnings"].append(f"{where}: its decay could not be modelled (residual {resid:.0f} dB): a key held longer ends where the recording does")
        return y[:release], {"skipped": f"model residual {resid:.0f} dB"}
    total = max(int(hold_seconds(key) * RATE), fit_b + int(1.0 * RATE))
    xf = int(0.12 * RATE)
    cross = fit_b - xf
    cont = synth(parts, cross - fit_a, max(0, total - cross))
    # Where the continuation has faded 60 dB under the note's peak, it ends.
    peak = np.abs(y[:release]).max()
    env = np.abs(signal.hilbert(cont)) if len(cont) else cont
    below = np.nonzero(env > peak * 10 ** (-60 / 20))[0]
    end = int(below[-1]) + 1 if len(below) else 0
    cont = cont[:max(end, xf + int(0.2 * RATE))]
    w = 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, xf))
    out = np.concatenate([y[:cross], y[cross:fit_b] * (1 - w) + cont[:xf] * w, cont[xf:]])
    fade = min(len(out) // 4, int(0.6 * RATE))
    out[-fade:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fade)))
    info = {"from": round(cross / RATE, 2), "partials": len(parts), "fitResidualDb": round(resid, 1),
            "seconds": round(len(out) / RATE, 2)}
    if resid > -10:
        report["warnings"].append(f"{where}: the decay's continuation fits loosely (residual {resid:.0f} dB)")
    return out, info


def subtract_previous(x, onset, prev_key, new_peak_db):
    """Takes the previous key's ringing tail out of the new note. Returns (x, dB taken off) or (x, None)."""
    a, b = onset - int(0.28 * RATE), onset - int(0.01 * RATE)
    if a < 0:
        return x, None
    pre = x[a:b]
    level = 10 * np.log10(np.mean(pre ** 2) + 1e-24)
    if level < new_peak_db - 45:
        return x, None
    parts = fit_partials(x, a, b, prev_key, max_partials=24)
    if not parts:
        return x, None
    resid = model_residual_db(x, a, b, parts)
    if resid > -6:
        return x, None       # not the previous key's tone (noise, a thump): left alone
    length = int(2.0 * RATE)
    tail = synth(parts, onset - a, length)
    tail *= np.linspace(1.0, 0.0, length) ** 2
    y = x.copy()
    end = min(len(y), onset + length)
    y[onset:end] -= tail[:end - onset]
    return y, round(-resid, 1)


# ---------------------------------------------------------------------------
# Spectral shape for the derived held layers

def band_levels(y, key):
    """(low, high) energy (dB) of a note's first 300 ms: below and above 3 x f0 (at least 600 Hz)."""
    seg = y[:int(0.3 * RATE)]
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)), 1 << 15)) ** 2
    freqs = np.fft.rfftfreq(1 << 15, 1.0 / RATE)
    split = max(600.0, 3 * hz(key))
    lo = spec[(freqs > 20) & (freqs < split)].sum()
    hi = spec[(freqs >= split) & (freqs < 16000)].sum()
    return 10 * np.log10(lo + 1e-20), 10 * np.log10(hi + 1e-20), split


def smooth_keys(values, width=5):
    """A running median, then mean, across keys (None kept out)."""
    keys = sorted(values)
    arr = np.array([values[k] for k in keys], dtype=float)
    med = np.array([np.median(arr[max(0, i - width // 2):i + width // 2 + 1]) for i in range(len(arr))])
    mean = np.convolve(np.pad(med, 1, mode="edge"), np.ones(3) / 3, mode="valid")
    return dict(zip(keys, mean))


# ---------------------------------------------------------------------------

def slice_short(x, onset, nxt, floor_lin):
    """A short hit: from its onset to where it fades into the floor (or the next key)."""
    stop = (nxt - int(0.01 * RATE)) if nxt is not None else len(x)
    stop = min(stop, onset + int(3.0 * RATE))
    y = x[onset:stop].copy()
    env = P.envelope(y)
    sm = np.convolve(env, np.ones(8) / 8, mode="same")
    # The end: faded into the floor, or 55 dB under the note's peak (inaudible under
    # anything else playing), whichever comes first.
    fl = max(20 * np.log10(floor_lin + 1e-12) + 4, sm.max() - 55)
    peak = int(np.argmax(sm[:min(len(sm), 80)]))
    end = len(y)
    for i in range(peak, len(sm)):
        if sm[i] < fl and sm[i:i + 20].max() < fl + 2:
            end = min(len(y), (i + 10) * P.ENV_HOP)
            break
    y = y[:end]
    fin = int(0.001 * RATE)
    y[:fin] *= np.linspace(0, 1, fin)
    fout = int(np.clip(0.2 * len(y), 0.01 * RATE, 0.08 * RATE))
    y[-fout:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fout)))
    return y


def align_to(ref, y, max_lag=220):
    """y shifted (whole samples) so its attack lines up with ref's: the lag (within
    +-5 ms) where their first 150 ms correlate best. Velocity layers crossfade, so a few
    milliseconds between their attacks would comb-filter them (partials cancelling)."""
    n = int(0.15 * RATE)
    hp = signal.butter(2, 80, "highpass", fs=RATE, output="sos")
    a = signal.sosfiltfilt(hp, ref[:n + max_lag])[:n]
    b = signal.sosfiltfilt(hp, y[:n + 2 * max_lag])
    best, lag = -1.0, 0
    for k in range(-max_lag, max_lag + 1):
        seg = b[max(0, k):max(0, k) + n] if k >= 0 else np.concatenate([np.zeros(-k), b[:n + k]])
        if len(seg) < n:
            continue
        c = float(np.dot(a, seg) / (np.linalg.norm(a) * np.linalg.norm(seg) + 1e-20))
        if c > best:
            best, lag = c, k
    if lag > 0:
        y = y[lag:]
    elif lag < 0:
        y = np.concatenate([np.zeros(-lag), y])
    return y, lag, best


def run(spec, source, out_dir, report, locate):
    sid = spec["id"]
    lowest = P.midi_of(spec.get("lowest", "A0"))
    keys = int(spec.get("keys", 88))
    outlier = float(spec.get("outlierCents", 8.0))
    files = [dict(l, articulation="short", layer=i) for i, l in enumerate(spec["short"])]
    files.append(dict(spec["long"], articulation="long", layer=1))
    print(f"[{sid}] ...", flush=True)
    takes = {}          # (articulation, layer) -> {key: {...}}
    floors = {}
    for f in files:
        path = locate(f["file"])
        tag = (f["articulation"], f["layer"])
        if path is None:
            report["problems"].append(f"{sid}: missing file '{f['file']}'")
            continue
        x, corr, fmt = load(path)
        cands = candidates(x)
        got = assign(x, cands, lowest, keys)
        onsets = {k: refine_onset(x, at) for k, (at, _) in got.items()}
        first = min(onsets.values()) if onsets else len(x) // 2
        floors[tag] = floor_db(x, first)
        entry = {"file": f["file"], "set": sid, "layer": f"{f['articulation']} {f['name']}", "format": fmt,
                 "stereo": "dual mono" if corr > 0.999 else f"stereo (L/R correlation {corr:.3f}, mixed to mono)",
                 "noiseFloorDb": round(floors[tag], 1), "expected": keys, "found": len(got), "candidates": len(cands),
                 "riseNeededDb": 0, "rejected": [], "notes": []}
        report["files"].append(entry)
        missing = [P.name_of(lowest + k) for k in range(keys) if k not in got]
        if missing:
            report["problems"].append(f"{sid}/{f['articulation']} {f['name']}: no onset for {', '.join(missing)}")
        ordered = sorted(onsets)
        floor_lin = 10 ** (floors[tag] / 20) * 1.4
        notes = {}
        for idx, k in enumerate(ordered):
            key = lowest + k
            on = onsets[k]
            nxt = onsets[ordered[idx + 1]] if idx + 1 < len(ordered) else None
            where = f"{sid}/{f['articulation']} {f['name']} {P.name_of(key)} at {on / RATE:.2f}s"
            peak_db = 10 * np.log10(np.mean(x[on:on + int(0.3 * RATE)] ** 2) + 1e-24)
            bleed = None
            if idx > 0 and ordered[idx - 1] == k - 1:
                x, bleed = subtract_previous(x, on, key - 1, peak_db)
            seg = x[on + int((0.3 if f["articulation"] == "long" else 0.04) * RATE):on + int((1.5 if f["articulation"] == "long" else 0.5) * RATE)]
            midi, b, used = measure_pitch(seg, key)
            note = {"label": P.name_of(key), "at": round(on / RATE, 2), "score": round(got[k][1], 1)}
            if bleed is not None:
                note["bleed"] = f"previous key's tail subtracted (model {bleed:.0f} dB under it)"
            if f["articulation"] == "short":
                y = slice_short(x, on, nxt, floor_lin)
                held = None
            else:
                stop = (nxt - int(0.01 * RATE)) if nxt is not None else len(x)
                y = x[on:stop].copy()
                rel = damper_point(y)
                held = rel
                if rel is None:
                    rel = len(y) - int(0.05 * RATE)
                    note["damper"] = "none found (undamped string or held to the next key)"
                else:
                    note["held"] = round(rel / RATE, 2)
                    # How fast the damper stops it (seconds to fall 60 dB), from the recording.
                    after = P.envelope(y[rel:rel + int(1.0 * RATE)])
                    if len(after) > 20:
                        sm = np.convolve(after, np.ones(4) / 4, mode="same")
                        drop = sm[2] - sm[min(len(sm) - 3, 40)]
                        note["damperSeconds"] = round(float(np.clip(60.0 / max(drop / (38 * P.ENV_HOP / RATE), 1.0), 0.05, 3.0)), 3)
                y, info = extend(y, rel, key, report, where)
                if info:
                    note["continued"] = info
                fin = int(0.001 * RATE)
                y[:fin] *= np.linspace(0, 1, fin)
                fout = int(0.03 * RATE)
                y[-fout:] *= 0.5 * (1 + np.cos(np.linspace(0, np.pi, fout)))
            note["seconds"] = round(len(y) / RATE, 2)
            notes[key] = {"audio": y, "midi": midi, "b": b, "partials": used, "note": note, "at": on, "held": held,
                          "loud": P.note_loudness(y)}
            entry["notes"].append(note)
        takes[tag] = notes
    if not takes:
        return []

    # --- Levels: each file's gain from its noise floor (same room, same preamp gain).
    # The file with the lowest floor was turned up least: each comes back down by its excess.
    trims = {tag: -(floors[tag] - min(floors.values())) for tag in floors}
    for tag, notes in takes.items():
        g = 10 ** (trims[tag] / 20)
        for n in notes.values():
            n["audio"] = n["audio"] * g
            n["loud"] += trims[tag]
    report["levelling"][sid] = {f"{a} {spec['short'][l]['name'] if a == 'short' else spec['long']['name']}": {"floorDb": round(floors[(a, l)], 1), "trimDb": round(trims[(a, l)], 1)}
                                for (a, l) in floors}

    # --- Bad keys: no pitch, or far off their neighbours' loudness.
    bad = {}
    for tag, notes in takes.items():
        label = f"{tag[0]} {spec['short'][tag[1]]['name'] if tag[0] == 'short' else spec['long']['name']}"
        for key in range(lowest, lowest + keys):
            n = notes.get(key)
            why = None
            if n is None:
                why = "missing"
            else:
                around = [notes[k]["loud"] for k in range(key - 3, key + 4) if k != key and k in notes]
                if around and n["loud"] < np.median(around) - 12:
                    why = f"{np.median(around) - n['loud']:.0f} dB quieter than its neighbours (barely struck)"
                elif around and n["loud"] > np.median(around) + 12:
                    why = f"{n['loud'] - np.median(around):.0f} dB louder than its neighbours"
                elif n["midi"] is None:
                    why = "no pitch found"
                elif abs(n["midi"] - key) > 0.6:
                    why = f"sounds {P.name_of(n['midi'])} ({100 * (n['midi'] - key):+.0f} cents)"
            if why:
                bad.setdefault(tag, {})[key] = why
                report["problems"].append(f"{sid}/{label} {P.name_of(key)}: {why}; replaced by its neighbour")

    # --- Tuning: the stretch curve, its offset from A440, outliers pulled onto it.
    dev = {}
    for key in range(lowest, lowest + keys):
        vals = []
        for tag, notes in takes.items():
            n = notes.get(key)
            if n is not None and n["midi"] is not None and key not in bad.get(tag, {}):
                # The held notes' measurement is the steadiest; weight it twice.
                vals += [100 * (n["midi"] - key)] * (2 if tag[0] == "long" else 1)
        if vals:
            dev[key] = float(np.median(vals))
    curve = smooth_keys(dev, width=7)
    mid = [curve[k] for k in curve if P.midi_of("F3") <= k <= P.midi_of("F5")]
    offset = float(np.median(mid)) if mid else 0.0
    tuning = {"offsetCents": round(offset, 1), "stretch": {}, "outliers": {}}
    target = {}
    for key in range(lowest, lowest + keys):
        if key not in dev:
            target[key] = curve.get(key, 0.0) - offset if key in curve else 0.0
            continue
        resid = dev[key] - curve[key]
        if abs(resid) > outlier:
            tuning["outliers"][P.name_of(key)] = round(resid, 1)
            target[key] = curve[key] - offset
        else:
            target[key] = dev[key] - offset
        if (key - lowest) % 12 == 0 or key == lowest + keys - 1:
            tuning["stretch"][P.name_of(key)] = round(curve[key] - offset, 1)
    report.setdefault("tuning", {})[sid] = tuning
    for tag, notes in takes.items():
        for key, n in notes.items():
            if n["midi"] is None:
                n["note"]["detected"] = "no pitch"
            else:
                n["note"]["detected"] = f"{100 * (n['midi'] - key):+.0f} cents (stretch {curve.get(key, 0.0) - offset:+.0f}, plays at {target.get(key, 0.0):+.0f})"
    for name, cents in tuning["outliers"].items():
        report["warnings"].append(f"{sid} {name}: {cents:+.0f} cents off the piano's stretch curve (its unisons have drifted); tuned onto it")

    # --- Layer steps: soft < medium < full, smoothed across the keys.
    short_layers = [l for l in range(len(spec["short"])) if ("short", l) in takes]
    steps = {}
    for l in short_layers:
        d = {}
        for key in range(lowest, lowest + keys):
            if ("short", 1) in takes and key in takes[("short", l)] and key in takes[("short", 1)] \
                    and key not in bad.get(("short", l), {}) and key not in bad.get(("short", 1), {}):
                d[key] = takes[("short", l)][key]["loud"] - takes[("short", 1)][key]["loud"]
        steps[l] = smooth_keys(d, width=9) if d else {}
    # The held notes' level against the medium short hits at the same key (the attack).
    long_vs = {}
    if ("long", 1) in takes and ("short", 1) in takes:
        for key in range(lowest, lowest + keys):
            a, b = takes[("long", 1)].get(key), takes[("short", 1)].get(key)
            if a is not None and b is not None and key not in bad.get(("long", 1), {}) and key not in bad.get(("short", 1), {}):
                long_vs[key] = a["loud"] - b["loud"]
        long_vs = smooth_keys(long_vs, width=9) if long_vs else {}

    # Each short note: trimmed so its step to the medium layer is the smoothed one
    # (the take's own tone kept), the medium layer evened toward its neighbours a little.
    med_curve = smooth_keys({k: n["loud"] for k, n in takes.get(("short", 1), {}).items() if k not in bad.get(("short", 1), {})}, width=7) \
        if ("short", 1) in takes else {}
    zones_short, zones_long = [], []
    replaced = []

    def pick(tag, key):
        """The note to use for a key (a bad one's nearest good neighbour)."""
        notes = takes.get(tag, {})
        if key in notes and key not in bad.get(tag, {}):
            return key, notes[key]
        for d in (1, -1, 2, -2, 3, -3):
            k = key + d
            if k in notes and k not in bad.get(tag, {}):
                return k, notes[k]
        return None, None

    for l in short_layers:
        tag = ("short", l)
        for key in range(lowest, lowest + keys):
            src, n = pick(tag, key)
            if n is None:
                continue
            if src != key:
                replaced.append(f"short {spec['short'][l]['name']} {P.name_of(key)} <- {P.name_of(src)}")
            want = med_curve.get(src, n["loud"]) + (steps[l].get(src, 0.0) if l != 1 else 0.0)
            want = 0.6 * want + 0.4 * n["loud"] if l == 1 else want
            trim = float(np.clip(want - n["loud"], -9.0, 9.0))
            root = src + (n["midi"] - src if n["midi"] is not None else 0.0) - target.get(src, 0.0) / 100.0
            zones_short.append({"key": key, "src": src, "layer": l, "audio": n["audio"] * 10 ** (trim / 20), "root": root,
                                "velocity": spec["short"][l]["velocity"], "name": spec["short"][l]["name"], "at": n["at"],
                                "file": spec["short"][l]["file"]})
    # Held notes: the medium take, matched to the medium short hit's level at its attack.
    shape = {}
    if ("long", 1) in takes:
        for key in range(lowest, lowest + keys):
            src, n = pick(("long", 1), key)
            if n is None:
                continue
            if src != key:
                replaced.append(f"long {P.name_of(key)} <- {P.name_of(src)}")
            match = -(long_vs.get(src, 0.0))
            want_med = med_curve.get(src)
            trim = float(np.clip(match + (0.0 if want_med is None else 0.0), -12.0, 12.0))
            root = src + (n["midi"] - src if n["midi"] is not None else 0.0) - target.get(src, 0.0) / 100.0
            zones_long.append({"key": key, "src": src, "audio": n["audio"] * 10 ** (trim / 20), "root": root, "at": n["at"],
                               "file": spec["long"]["file"], "damper": n["note"].get("damperSeconds"), "held": n["held"]})
        # The derived soft and full held layers: gain and brightness from the short layers.
        for l in short_layers:
            if l == 1:
                continue
            g, s = {}, {}
            for key in range(lowest, lowest + keys):
                a, m = takes[("short", l)].get(key), takes[("short", 1)].get(key)
                if a is None or m is None or key in bad.get(("short", l), {}) or key in bad.get(("short", 1), {}):
                    continue
                la, ha, _ = band_levels(a["audio"], key)
                lm, hm, _ = band_levels(m["audio"], key)
                g[key] = a["loud"] - m["loud"]
                s[key] = (ha - la) - (hm - lm)
            shape[l] = (smooth_keys(g, 9) if g else {}, smooth_keys(s, 9) if s else {})
    for r in replaced:
        report["warnings"].append(f"{sid}: {r} (bad key replaced by its neighbour)")
    # Every layer's attack lined up with the medium short hit's (same key, same recording
    # of it): crossfaded layers then add instead of cancelling.
    ref = {z["key"]: z["audio"] for z in zones_short if z["layer"] == 1 and z["src"] == z["key"]}
    lags = []
    for z in zones_short + zones_long:
        if z["src"] != z["key"] or z["key"] not in ref or (z in zones_short and z["layer"] == 1):
            continue
        z["audio"], lag, corr = align_to(ref[z["key"]], z["audio"])
        lags.append(lag)
    if lags:
        report.setdefault("alignment", {})[sid] = {"median lag (samples)": float(np.median(np.abs(lags))), "largest": int(max(np.abs(lags)))}

    # --- One gain for both articulations: the loudest peak at -1 dBFS.
    peak = max([np.abs(z["audio"]).max() for z in zones_short + zones_long] + [1e-9])
    gain = 10 ** (-1 / 20) / peak
    os.makedirs(os.path.join(out_dir, f"{sid}-short"), exist_ok=True)
    os.makedirs(os.path.join(out_dir, f"{sid}-long"), exist_ok=True)

    def zone_entry(z, sub, name, layer, vel, extra=None):
        a = z["audio"] * gain
        env = P.envelope(a[:int(1.5 * RATE)])
        top = int(np.argmax(np.convolve(env, np.ones(2) / 2, mode="same"))) * P.ENV_HOP
        head = a[top:top + int(0.3 * RATE)]
        m = {"file": f"{sub}/{name}", "label": P.name_of(z["key"]), "root": round(float(z["root"]), 3),
             "loKey": z["key"], "hiKey": z["key"], "loVel": vel[0], "hiVel": vel[1], "layer": layer, "rr": 0,
             "length": len(a), "rms": round(float(db(np.sqrt(np.mean(head ** 2)))), 2), "loudness": round(P.note_loudness(a), 2),
             "peak": round(float(db(np.abs(a).max())), 2), "source": z["file"], "at": round(z["at"] / RATE, 3)}
        if z["src"] != z["key"]:
            m["from"] = P.name_of(z["src"])
        if extra:
            m.update(extra)
        return m, a

    written = 0
    zs = []
    for z in zones_short:
        name = f"{P.name_of(z['key']).replace('#', 's')}-{z['name']}-1.flac"
        m, a = zone_entry(z, f"{sid}-short", name, z["layer"], z["velocity"])
        sf.write(os.path.join(out_dir, f"{sid}-short", name), a.astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
        written += 1
        zs.append(m)
    # Ends of the keyboard: the lowest and highest keys stretch a fifth (folded further).
    for layer in short_layers:
        mine = [m for m in zs if m["layer"] == layer]
        if mine:
            min(mine, key=lambda m: m["loKey"])["loKey"] -= 7
            max(mine, key=lambda m: m["hiKey"])["hiKey"] += 7
    zs.sort(key=lambda m: (m["loKey"], m["layer"]))
    mapping_short = {"id": f"{sid}-short", "name": spec["name"] + ", short hits", "kind": "piano", "articulation": "short", "rate": RATE,
                     "channels": 1, "layers": [l["name"] for l in spec["short"]], "gainDb": round(20 * math.log10(gain), 2),
                     "crossfade": True, "zones": zs}
    zl = []
    for z in zones_long:
        name = f"{P.name_of(z['key']).replace('#', 's')}-held-1.flac"
        sf.write(os.path.join(out_dir, f"{sid}-long", name), (z["audio"] * gain).astype(np.float32), RATE, subtype="PCM_16", format="FLAC")
        written += 1
        damp = z["damper"] if z["damper"] else None
        for l in range(len(spec["short"])):
            vel = spec["short"][l]["velocity"]
            extra = {"damperSeconds": damp} if damp else {}
            if l != 1 and l in shape:
                g, s = shape[l]
                extra["gainDb"] = round(float(np.clip(g.get(z["src"], 0.0), -24, 12)), 2)
                # Soft is never brighter than medium, full never darker (the soft high
                # keys' noise would otherwise read as brightness).
                extra["shelfDb"] = round(float(np.clip(s.get(z["src"], 0.0), -18, 0) if l < 1 else np.clip(s.get(z["src"], 0.0), 0, 12)), 2)
                extra["shelfHz"] = round(max(600.0, 3 * hz(z["key"])), 1)
                extra["derived"] = True
            m, _ = zone_entry(z, f"{sid}-long", name, l, vel, extra)
            if l != 1 and "gainDb" in m:
                m["loudness"] = round(m["loudness"] + m["gainDb"], 2)
            zl.append(m)
    for layer in range(len(spec["short"])):
        mine = [m for m in zl if m["layer"] == layer]
        if mine:
            min(mine, key=lambda m: m["loKey"])["loKey"] -= 7
            max(mine, key=lambda m: m["hiKey"])["hiKey"] += 7
    zl.sort(key=lambda m: (m["loKey"], m["layer"]))
    mapping_long = {"id": f"{sid}-long", "name": spec["name"] + ", held notes", "kind": "piano", "articulation": "long", "rate": RATE,
                    "channels": 1, "layers": [l["name"] for l in spec["short"]], "gainDb": round(20 * math.log10(gain), 2),
                    "crossfade": True, "zones": zl}
    for mp in (mapping_short, mapping_long):
        with open(os.path.join(out_dir, f"{mp['id']}.json"), "w") as f:
            json.dump(mp, f, indent=1)
    # Velocity order per key (short layers).
    for key in range(lowest, lowest + keys):
        lv = [next((m["loudness"] for m in zs if m["label"] == P.name_of(key) and m["layer"] == l), None) for l in short_layers]
        if None not in lv and any(lv[i] >= lv[i + 1] for i in range(len(lv) - 1)):
            report["warnings"].append(f"{sid} {P.name_of(key)}: the velocity layers are not in loudness order ({', '.join(f'{v:.1f}' for v in lv)} dB)")
    report.setdefault("prepared", {})[sid] = written
    return [mapping_short, mapping_long]
