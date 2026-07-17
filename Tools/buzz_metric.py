#!/usr/bin/env python3
"""Buzz metric for vocal pitch-engine renders (QA-Fe WORLD arc, 2026-07-16).

Why this exists: the 18-WAV spectral battery (HF%, flatness, HNR, flux,
roughness) never registered the WORLD buzz because those are magnitude-domain
long-window stats, and WORLD reproduces the magnitude spectrum nearly
perfectly.  The buzz is a PHASE / temporal-fine-structure artifact: WORLD's
minimum-phase mono-pulse excitation fires every harmonic phase-aligned once
per glottal cycle, so high-band energy arrives as one spike per period.
That is F0-rate amplitude modulation of the band envelope -- measurable.

Metrics (computed on voiced regions only):
  buzz_index    -- energy at F0/2F0/3F0 in the modulation spectrum of the
                   2-6 kHz band envelope, over total 20-800 Hz modulation
                   energy.  Pulse-train excitation -> high; natural phase
                   dispersion (dry take, Rubber Band) -> low.
  band_kurtosis -- median excess kurtosis of the 2-6 kHz waveform per 50 ms
                   voiced window.  Spiky per-cycle arrivals -> leptokurtic.
  band_crest    -- median crest factor (peak/RMS) of the same windows.

Usage:  python Tools/buzz_metric.py file1.wav [file2.wav ...]
Deps: numpy only (self-contained WAV reader: PCM 16/24/32 + float32/64).
"""

import struct
import sys

import numpy as np

BAND_LO, BAND_HI = 2000.0, 6000.0
MOD_LO, MOD_HI = 20.0, 800.0
F0_LO, F0_HI = 70.0, 450.0
WIN_SEC = 0.050


def read_wav(path):
    """Return (mono float64 array, sample rate).  RIFF PCM/float only."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"{path}: not a RIFF/WAVE file")
    pos, fmt, raw = 12, None, None
    while pos + 8 <= len(data):
        cid, size = data[pos:pos + 4], struct.unpack_from("<I", data, pos + 4)[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", body, 0)
        elif cid == b"data":
            raw = body
        pos += 8 + size + (size & 1)
    if fmt is None or raw is None:
        raise ValueError(f"{path}: missing fmt/data chunk")
    tag, nch, fs, _, _, bits = fmt
    if tag == 0xFFFE and len(data) >= 2:  # WAVE_FORMAT_EXTENSIBLE
        tag = 3 if bits in (32, 64) else 1
    if tag == 3:
        x = np.frombuffer(raw, dtype=np.float32 if bits == 32 else np.float64)
    elif tag == 1 and bits == 16:
        x = np.frombuffer(raw, dtype=np.int16).astype(np.float64) / 32768.0
    elif tag == 1 and bits == 24:
        b = np.frombuffer(raw, dtype=np.uint8).reshape(-1, 3)
        x = ((b[:, 0].astype(np.int32)) | (b[:, 1].astype(np.int32) << 8)
             | (b[:, 2].astype(np.int32) << 16))
        x = np.where(x >= 1 << 23, x - (1 << 24), x).astype(np.float64) / float(1 << 23)
    elif tag == 1 and bits == 32:
        x = np.frombuffer(raw, dtype=np.int32).astype(np.float64) / float(1 << 31)
    else:
        raise ValueError(f"{path}: unsupported format tag={tag} bits={bits}")
    x = x.astype(np.float64)
    if nch > 1:
        x = x[: (len(x) // nch) * nch].reshape(-1, nch).mean(axis=1)
    return x, fs


def write_wav(path, x, fs):
    """Write mono float64 as 16-bit PCM (playable everywhere)."""
    pk = np.max(np.abs(x)) if len(x) else 0.0
    if pk > 1.0:
        x = x / pk * 0.999
    pcm = np.clip(np.round(x * 32767.0), -32768, 32767).astype("<i2").tobytes()
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, fs, fs * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(pcm)) + pcm)


def _bandpass_fft(x, fs, lo, hi):
    """Brickwall bandpass via FFT (offline analysis; phase-neutral)."""
    n = len(x)
    spec = np.fft.rfft(x)
    freqs = np.fft.rfftfreq(n, 1.0 / fs)
    spec[(freqs < lo) | (freqs > hi)] = 0.0
    return np.fft.irfft(spec, n)


def _analytic_env(x):
    """|analytic signal| via FFT Hilbert (no scipy)."""
    n = len(x)
    spec = np.fft.fft(x)
    h = np.zeros(n)
    h[0] = 1.0
    if n % 2 == 0:
        h[n // 2] = 1.0
        h[1:n // 2] = 2.0
    else:
        h[1:(n + 1) // 2] = 2.0
    return np.abs(np.fft.ifft(spec * h))


def voiced_mask(x, fs):
    """Energy gate: 50 ms frames above 15% of the 95th-percentile RMS."""
    w = max(1, int(WIN_SEC * fs))
    nf = len(x) // w
    if nf == 0:
        return np.zeros(len(x), dtype=bool)
    rms = np.sqrt((x[: nf * w].reshape(nf, w) ** 2).mean(axis=1))
    thr = 0.15 * np.percentile(rms, 95)
    m = np.repeat(rms > thr, w)
    return np.pad(m, (0, len(x) - len(m)), constant_values=False)


def estimate_f0(x, fs, mask):
    """Median F0 over voiced 100 ms windows via autocorrelation (<=1 kHz band)."""
    lp = _bandpass_fft(x, fs, 40.0, 1000.0)
    w = int(0.100 * fs)
    lag_lo, lag_hi = int(fs / F0_HI), int(fs / F0_LO)
    f0s = []
    for s in range(0, len(lp) - w, w):
        if not mask[s:s + w].mean() > 0.8:
            continue
        seg = lp[s:s + w] * np.hanning(w)
        ac = np.fft.irfft(np.abs(np.fft.rfft(seg, 2 * w)) ** 2)[:w]
        if ac[0] <= 0:
            continue
        ac /= ac[0]
        k = lag_lo + int(np.argmax(ac[lag_lo:lag_hi]))
        if ac[k] > 0.25:
            f0s.append(fs / k)
    return float(np.median(f0s)) if f0s else 0.0


def analyze(x, fs, f0_hint=0.0):
    mask = voiced_mask(x, fs)
    if mask.sum() < fs // 2:
        return None
    f0 = f0_hint if f0_hint > 0 else estimate_f0(x, fs, mask)

    band = _bandpass_fft(x, fs, BAND_LO, BAND_HI)

    # Per-window band kurtosis + crest over voiced windows.
    w = max(1, int(WIN_SEC * fs))
    kurts, crests = [], []
    for s in range(0, len(band) - w, w):
        if mask[s:s + w].mean() < 0.9:
            continue
        seg = band[s:s + w]
        sd = seg.std()
        if sd < 1e-9:
            continue
        z = (seg - seg.mean()) / sd
        kurts.append((z ** 4).mean() - 3.0)
        crests.append(np.max(np.abs(seg)) / sd)

    # Modulation spectrum of the band envelope over contiguous voiced chunks.
    env = _analytic_env(band)
    seg_len = int(1.0 * fs)
    psd_acc, n_seg = None, 0
    s = 0
    while s + seg_len <= len(env):
        if mask[s:s + seg_len].mean() > 0.9:
            seg = env[s:s + seg_len]
            seg = seg - seg.mean()
            spec = np.abs(np.fft.rfft(seg * np.hanning(seg_len))) ** 2
            psd_acc = spec if psd_acc is None else psd_acc + spec
            n_seg += 1
            s += seg_len // 2
        else:
            s += seg_len // 4
    buzz = float("nan")
    if psd_acc is not None and f0 > 0:
        freqs = np.fft.rfftfreq(seg_len, 1.0 / fs)
        total = psd_acc[(freqs >= MOD_LO) & (freqs <= MOD_HI)].sum()
        hits = 0.0
        for mult, tol in ((1, 0.10), (2, 0.07), (3, 0.05)):
            fc = mult * f0
            if fc > MOD_HI:
                break
            sel = (freqs >= fc * (1 - tol)) & (freqs <= fc * (1 + tol))
            hits += psd_acc[sel].sum()
        if total > 0:
            buzz = hits / total

    return {
        "fs": fs,
        "f0": f0,
        "voiced_sec": mask.sum() / fs,
        "mod_segments": n_seg,
        "buzz_index": buzz,
        "band_kurtosis": float(np.median(kurts)) if kurts else float("nan"),
        "band_crest": float(np.median(crests)) if crests else float("nan"),
    }


def main(paths):
    print(f"{'file':<46} {'fs':>6} {'F0':>6} {'buzz':>7} {'kurt':>7} {'crest':>6}")
    for p in paths:
        x, fs = read_wav(p)
        r = analyze(x, fs)
        name = p.replace("\\", "/").split("/")[-1][:45]
        if r is None:
            print(f"{name:<46} {fs:>6} {'--- too little voiced material ---':>34}")
            continue
        print(f"{name:<46} {r['fs']:>6} {r['f0']:>6.1f} {r['buzz_index']:>7.3f}"
              f" {r['band_kurtosis']:>7.2f} {r['band_crest']:>6.2f}")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    main(sys.argv[1:])
