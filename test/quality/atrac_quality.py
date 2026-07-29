#!/usr/bin/env python3
"""Perceptual quality regression harness for ATRAC1.

Encodes a corpus with atracdenc, decodes it again with atracdenc, and measures
the coding noise against a masking threshold (noise-to-mask ratio). Saving a
baseline and comparing later turns that into a regression test: a change that
makes the coding noise more audible fails.

  NMR < 0 dB   coding noise sits below the masking threshold
  NMR > 0 dB   coding noise exceeds it

Requires numpy. No other dependencies: WAV I/O uses the standard library and
both encode and decode go through atracdenc itself.

  # does the metric itself behave? (no corpus needed)
  atrac_quality.py --atracdenc build/src/atracdenc --self-check

  # measure a corpus of 44.1 kHz 16-bit WAVs
  atrac_quality.py --atracdenc build/src/atracdenc --corpus ~/samples

  # record a baseline, then later check nothing got worse
  atrac_quality.py ... --corpus ~/samples --save-baseline base.json
  atrac_quality.py ... --corpus ~/samples --baseline base.json

Exit status is non-zero if the self-check fails, or if any track regresses by
more than --tolerance dB against the baseline.
"""

import argparse
import json
import math
import struct
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

SKIP = 77          # ctest SKIP_RETURN_CODE: numpy is optional for the build

try:
    import numpy as np
except ImportError:
    print("numpy not available, skipping quality check", file=sys.stderr)
    sys.exit(SKIP)

SR = 44100
FFT = 2048
HOP = 1024
BLOCK = 256          # frames per chunk, keeps memory flat on long files


# --------------------------------------------------------------------------- I/O

def read_wav(path):
    """16-bit PCM WAV -> float array, shape (frames, channels)."""
    with wave.open(str(path), "rb") as w:
        if w.getsampwidth() != 2:
            raise ValueError(f"{path}: expected 16-bit PCM")
        ch, n = w.getnchannels(), w.getnframes()
        raw = w.readframes(n)
    x = np.frombuffer(raw, dtype="<i2").astype(np.float64)
    return x.reshape(-1, ch) if ch > 1 else x.reshape(-1, 1)


def write_wav(path, x, sr=SR):
    x = np.clip(np.rint(x), -32768, 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(x.shape[1])
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(x.tobytes())


# ------------------------------------------------------------- masking model

def _bark(f):
    return 13 * np.arctan(0.00076 * f) + 3.5 * np.arctan((f / 7500.0) ** 2)


def _ath_db(f):
    """Absolute threshold of hearing, dB SPL."""
    f = np.maximum(f, 20.0) / 1000.0
    return 3.64 * f ** -0.8 - 6.5 * np.exp(-0.6 * (f - 3.3) ** 2) + 1e-3 * f ** 4


class Bands:
    """Bark-scale band layout, spreading matrix and hearing-threshold floor."""

    def __init__(self, sr=SR, n=FFT, step=0.5):
        self.win = np.hanning(n)
        freq = np.fft.rfftfreq(n, 1 / sr)
        z = _bark(freq)
        edges = np.arange(0, z[-1] + step, step)
        idx = [np.where((z >= lo) & (z < hi))[0] for lo, hi in zip(edges, edges[1:])]
        self.idx = [i for i in idx if len(i)]
        zc = np.array([z[i].mean() for i in self.idx])
        self.zc = zc
        # Schroeder spreading function between band centres, linear power
        dz = zc[:, None] - zc[None, :]
        sf = 15.81 + 7.5 * (dz + 0.474) - 17.5 * np.sqrt(1 + (dz + 0.474) ** 2)
        self.spread = 10 ** (sf / 10.0)
        # full-scale sine is taken as 96 dB SPL
        self.ath = 10 ** ((np.array([_ath_db(freq[i]).min() for i in self.idx]) - 96.0) / 10.0)

    def power(self, frames):
        spec = np.fft.rfft(frames * self.win, axis=-1)
        p = (2 * np.abs(spec) / self.win.sum()) ** 2
        return np.stack([p[..., i].sum(-1) for i in self.idx], -1)


def _frames(x, lo, hi):
    n = 1 + max(0, (len(x) - FFT) // HOP)
    hi = min(hi, n)
    if hi <= lo:
        return np.zeros((0, FFT))
    return np.stack([x[i * HOP:i * HOP + FFT] for i in range(lo, hi)])


def nmr(ref, test, bands=None):
    """Noise-to-mask ratio in dB, and the fraction of band-frames above threshold."""
    bands = bands or Bands()
    ref, test = np.asarray(ref, float), np.asarray(test, float)
    n = min(len(ref), len(test))
    ref, test = ref[:n], test[:n]
    if ref.ndim > 1:
        ref, test = ref.mean(1), test.mean(1)
    ref, test = ref / 32768.0, test / 32768.0
    err = ref - test

    total = 1 + max(0, (len(ref) - FFT) // HOP)
    if total <= 0:
        return float("nan"), float("nan")

    acc, above, cells = 0.0, 0, 0
    for lo in range(0, total, BLOCK):
        pr = bands.power(_frames(ref, lo, lo + BLOCK))
        pe = bands.power(_frames(err, lo, lo + BLOCK))
        mask = pr @ bands.spread.T
        # tonality from spectral flatness: tonal maskers mask less than noisy ones
        with np.errstate(divide="ignore"):
            geo = np.exp(np.mean(np.log(np.maximum(pr, 1e-30)), axis=1))
            ari = np.maximum(pr.mean(1), 1e-30)
            sfm = 10 * np.log10(np.maximum(geo / ari, 1e-30))
        tonal = np.clip(sfm / -60.0, 0, 1)[:, None]
        offset = tonal * (14.5 + bands.zc[None, :]) + (1 - tonal) * 5.5
        thr = np.maximum(mask / 10 ** (offset / 10.0), bands.ath[None, :])
        ratio = pe / thr
        acc += float(ratio.sum())
        above += int((ratio > 1).sum())
        cells += ratio.size
    # mean of per-band-frame ratios; a ratio of sums lets the wide high-frequency
    # hearing-threshold floors swamp the numerator
    return 10 * math.log10(max(acc / max(cells, 1), 1e-30)), above / max(cells, 1)


# ------------------------------------------------------------------ alignment

def find_lag(ref, test, max_lag=8192, seconds=10):
    """Samples by which `test` lags `ref`, via FFT cross-correlation."""
    a = test[:seconds * SR].mean(1) if test.ndim > 1 else test[:seconds * SR]
    b = ref[:seconds * SR].mean(1) if ref.ndim > 1 else ref[:seconds * SR]
    n = 1 << (len(a) + len(b) - 1).bit_length()
    c = np.fft.irfft(np.fft.rfft(a, n) * np.conj(np.fft.rfft(b, n)), n)
    return int(np.argmax(c[:max_lag]))


# ------------------------------------------------------------------- codec run

def roundtrip(atracdenc, src, tmp):
    """Encode and decode with atracdenc; returns the decoded signal."""
    aea, out = tmp / "q.aea", tmp / "q.wav"
    for cmd in (["-e", "atrac1", "-i", str(src), "-o", str(aea)],
                ["-d", "-i", str(aea), "-o", str(out)]):
        r = subprocess.run([str(atracdenc), *cmd], capture_output=True)
        if r.returncode != 0:
            raise RuntimeError(f"atracdenc {cmd[0]} failed: {r.stderr.decode()[-300:]}")
    dec = read_wav(out)
    aea.unlink(missing_ok=True)
    out.unlink(missing_ok=True)
    return dec


def measure(atracdenc, src, tmp, bands):
    ref = read_wav(src)
    dec = roundtrip(atracdenc, src, tmp)
    lag = find_lag(ref, dec)
    dec = dec[lag:]
    value, above = nmr(ref, dec, bands)
    return {"nmr_db": round(value, 3), "audible_frac": round(above, 5), "lag": lag}


# ------------------------------------------------------------------ self-check

def self_check(atracdenc, tmp, bands):
    """Inject noise at known SNRs; the metric must fall monotonically.

    Run on both broadband and tonal signals, because a metric can behave on one
    and invert on the other -- which is exactly what disqualified peaqb here.
    """
    rng = np.random.default_rng(0)
    t = np.arange(8 * SR) / SR
    cases = {
        "broadband": (rng.standard_normal((len(t), 2)) * 6000),
        "pure tone": (np.sin(2 * np.pi * 440 * t)[:, None] * np.ones(2) * 12000),
    }
    ok = True
    for name, sig in cases.items():
        print(f"  {name}:")
        prev = None
        for snr in (75, 60, 45, 30, 15):
            power = (sig ** 2).mean()
            noise = rng.normal(0, math.sqrt(power / 10 ** (snr / 10)), sig.shape)
            value, above = nmr(sig, sig + noise, bands)
            flag = ""
            if prev is not None and value < prev - 1e-9:
                flag, ok = "   NOT MONOTONIC", False
            prev = value
            print(f"    SNR {snr:3d} dB -> NMR {value:+7.2f} dB  "
                  f"({100 * above:5.1f}% of band-frames audible){flag}")
    print(f"  metric self-check: {'pass' if ok else 'FAIL'}")
    return ok


# ------------------------------------------------------------------------ main

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--atracdenc", required=True, help="path to the atracdenc binary")
    ap.add_argument("--corpus", help="directory of 44.1 kHz 16-bit WAV files")
    ap.add_argument("--baseline", help="compare against this baseline JSON")
    ap.add_argument("--save-baseline", help="write results as a baseline JSON")
    ap.add_argument("--tolerance", type=float, default=0.2,
                    help="dB a track may worsen before it counts as a regression")
    ap.add_argument("--self-check", action="store_true",
                    help="validate the metric on synthetic signals and exit")
    args = ap.parse_args()

    if not Path(args.atracdenc).exists():
        sys.exit(f"no such binary: {args.atracdenc}")
    bands = Bands()

    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        if args.self_check:
            print("metric self-check (no corpus needed):")
            sys.exit(0 if self_check(args.atracdenc, tmp, bands) else 1)

        if not args.corpus:
            sys.exit("need --corpus (or --self-check)")
        files = sorted(p for p in Path(args.corpus).iterdir()
                       if p.suffix.lower() == ".wav")
        if not files:
            sys.exit(f"no .wav files in {args.corpus}")

        results = {}
        print(f"{'track':<34}{'NMR dB':>9}{'audible':>10}")
        for p in files:
            try:
                results[p.name] = measure(args.atracdenc, p, tmp, bands)
            except Exception as exc:                      # keep going on one bad file
                print(f"{p.name:<34}  skipped: {exc}")
                continue
            r = results[p.name]
            print(f"{p.name:<34}{r['nmr_db']:>+9.2f}{100 * r['audible_frac']:>9.1f}%")

    if not results:
        sys.exit("nothing measured")
    mean = sum(r["nmr_db"] for r in results.values()) / len(results)
    print(f"\n{len(results)} track(s), mean NMR {mean:+.3f} dB")

    if args.save_baseline:
        Path(args.save_baseline).write_text(
            json.dumps({"mean_nmr_db": mean, "tracks": results}, indent=2) + "\n")
        print(f"baseline written to {args.save_baseline}")

    if args.baseline:
        base = json.loads(Path(args.baseline).read_text())
        worse = []
        for name, r in results.items():
            old = base["tracks"].get(name)
            if old is None:
                print(f"  {name}: not in baseline, skipped")
                continue
            delta = r["nmr_db"] - old["nmr_db"]          # positive = noisier = worse
            if delta > args.tolerance:
                worse.append((name, old["nmr_db"], r["nmr_db"], delta))
        print(f"baseline mean {base['mean_nmr_db']:+.3f} dB -> {mean:+.3f} dB "
              f"({mean - base['mean_nmr_db']:+.3f})")
        if worse:
            print(f"\nREGRESSION: {len(worse)} track(s) worse by more than "
                  f"{args.tolerance} dB")
            for name, old, new, delta in sorted(worse, key=lambda w: -w[3]):
                print(f"  {name}: {old:+.2f} -> {new:+.2f} ({delta:+.2f})")
            sys.exit(1)
        print("no regressions")


if __name__ == "__main__":
    main()
