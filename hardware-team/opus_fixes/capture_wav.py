#!/usr/bin/env python3
"""
capture_wav.py — companion to 02_wav_recorder.ino

Captures the PCM stream from the ESP32-S3 over serial, writes a real .wav,
and immediately runs the SAME analysis that found the fault in your last
recording (sub-20 Hz energy, clipping, DC offset) so you get a verdict
without having to ask anyone.

Usage:
    pip install pyserial numpy
    python capture_wav.py --port COM5            # Windows
    python capture_wav.py --port /dev/ttyUSB0    # Linux
    python capture_wav.py --port /dev/cu.usbmodem1101   # macOS

Then it sends 'r' to the board, records, saves out.wav, and prints a report.
"""

import argparse, sys, time, wave
import numpy as np

try:
    import serial
except ImportError:
    sys.exit("pyserial not installed.  Run:  pip install pyserial numpy")

MARK_START = b"<<<WAVSTART>>>"
MARK_END   = b"<<<WAVEND>>>"
SAMPLE_RATE = 16000

# ---- pass criteria (same thresholds as the firmware) -------------------
AUDIOBAND_MIN_PCT = 60.0   # % of energy that must sit above 80 Hz
CLIP_MAX_PCT      = 0.10   # % of samples allowed to hit the rail
ZCR_SPEECH_MIN    = 200    # zero-crossings/sec during speech
SNR_GOOD_DB       = 20.0


def capture(port, baud, timeout_s):
    ser = serial.Serial(port, baud, timeout=1)
    time.sleep(2.0)                     # let the board reset/settle
    ser.reset_input_buffer()
    ser.write(b"r")
    print(f"Sent 'r' — recording... speak now.")

    buf = bytearray()
    started = False
    t0 = time.time()

    while time.time() - t0 < timeout_s:
        chunk = ser.read(4096)
        if not chunk:
            continue
        buf.extend(chunk)

        if not started and MARK_START in buf:
            buf = bytearray(buf.split(MARK_START, 1)[1])
            started = True
            print("Stream started.")

        if started and MARK_END in buf:
            buf = bytearray(buf.split(MARK_END, 1)[0])
            print("Stream ended.")
            break

    ser.close()
    if not started:
        sys.exit("Never saw WAVSTART — check port, baud (921600), and that "
                 "02_wav_recorder.ino is running.")
    if len(buf) % 2:
        buf = buf[:-1]
    return np.frombuffer(bytes(buf), dtype="<i2")


def highpass(x, fc=80.0, sr=SAMPLE_RATE):
    """One-pole HPF, same as the firmware's, so numbers are comparable."""
    rc = 1.0 / (2 * np.pi * fc)
    dt = 1.0 / sr
    a = rc / (rc + dt)
    y = np.zeros_like(x, dtype=np.float64)
    xf = x.astype(np.float64)
    for i in range(1, len(xf)):
        y[i] = a * (y[i - 1] + xf[i] - xf[i - 1])
    return y


def band_energy_pct(x, sr=SAMPLE_RATE):
    """% of total energy in 85-4000 Hz (the speech band)."""
    spec = np.abs(np.fft.rfft(x.astype(np.float64)))
    freqs = np.fft.rfftfreq(len(x), 1 / sr)
    p = spec ** 2
    total = p.sum()
    if total == 0:
        return 0.0, {}
    bands = {
        "0-20 Hz (inaudible drift)": (0, 20),
        "20-85 Hz (rumble)":          (20, 85),
        "85-300 Hz (voice pitch)":    (85, 300),
        "300-3000 Hz (intelligibility)": (300, 3000),
        "3000-8000 Hz (sibilance)":   (3000, 8000),
    }
    breakdown = {k: 100 * p[(freqs >= lo) & (freqs < hi)].sum() / total
                 for k, (lo, hi) in bands.items()}
    speech = 100 * p[(freqs >= 85) & (freqs < 4000)].sum() / total
    return speech, breakdown


def analyse(x):
    print("\n" + "=" * 52)
    print(" ANALYSIS")
    print("=" * 52)
    print(f"  samples        : {len(x)}  ({len(x)/SAMPLE_RATE:.2f} s)")
    print(f"  min / max      : {x.min()} / {x.max()}")
    print(f"  DC offset      : {x.mean():.1f}")

    clip = 100 * np.mean((x == 32767) | (x == -32768))
    print(f"  clipped        : {clip:.3f} %   [want < {CLIP_MAX_PCT}%]")

    speech_pct, breakdown = band_energy_pct(x)
    print("\n  Energy by band:")
    for k, v in breakdown.items():
        print(f"    {k:<32s} {v:6.2f} %")
    print(f"\n  Speech band (85-4000 Hz): {speech_pct:.2f} %   "
          f"[need >= {AUDIOBAND_MIN_PCT}%]")

    hp = highpass(x)
    zcr = np.sum(np.diff(np.signbit(hp)) != 0) / (len(x) / SAMPLE_RATE)
    print(f"  Zero-crossing rate      : {zcr:.0f} /s   [need >= {ZCR_SPEECH_MIN}]")

    # crude SNR: loudest 20% of frames vs quietest 20%
    win = SAMPLE_RATE // 50
    frames = hp[: len(hp) // win * win].reshape(-1, win)
    rms = np.sqrt((frames ** 2).mean(axis=1))
    if len(rms) > 10:
        loud = np.percentile(rms, 90)
        quiet = np.percentile(rms, 10)
        snr = 20 * np.log10(loud / quiet) if quiet > 0 else 99
        print(f"  SNR (p90 vs p10)        : {snr:.1f} dB  [>{SNR_GOOD_DB} good]")

    print("\n" + "-" * 52)
    ok = True
    if speech_pct < AUDIOBAND_MIN_PCT:
        print(f"  FAIL  only {speech_pct:.1f}% of energy is in the speech band.")
        print("        -> this is drift/rumble, not voice.")
        print("        -> check SCK/WS wiring, shorten jumpers, reseat SD.")
        ok = False
    else:
        print(f"  PASS  {speech_pct:.1f}% of energy is real speech-band content.")

    if zcr < ZCR_SPEECH_MIN:
        print(f"  FAIL  zero-crossing rate {zcr:.0f}/s too low for speech.")
        ok = False
    else:
        print(f"  PASS  zero-crossing rate {zcr:.0f}/s.")

    if clip > CLIP_MAX_PCT:
        print(f"  FAIL  {clip:.3f}% clipped — lower SOFT_GAIN in the sketch.")
        ok = False
    else:
        print(f"  PASS  clipping {clip:.3f}%.")

    print("-" * 52)
    print("  *** RECORDING IS GOOD ***" if ok else "  *** RECORDING IS BAD — fix above ***")
    print("=" * 52)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="e.g. COM5 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=921600)
    ap.add_argument("--out", default="out.wav")
    ap.add_argument("--timeout", type=float, default=30.0)
    args = ap.parse_args()

    x = capture(args.port, args.baud, args.timeout)
    if len(x) == 0:
        sys.exit("No audio captured.")

    with wave.open(args.out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(x.tobytes())
    print(f"\nSaved {args.out}  ({len(x)/SAMPLE_RATE:.2f} s)")

    analyse(x)


if __name__ == "__main__":
    main()
