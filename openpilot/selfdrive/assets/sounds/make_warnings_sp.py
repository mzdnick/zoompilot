#!/usr/bin/env python3
"""Generate louder, re-authored safety-warning WAVs for zoompilot (fork).

Why: the stock warning tones (critical.wav, dm_critical.wav) are steady/decaying
sines near 0 dBFS, so runtime gain can only add ~2-3 dB before clipping. These
fork tones use a clipped ("square-ish") waveform at a higher, speaker-efficient
frequency, which raises perceived loudness well beyond that on every device
(tici, tizi, mici). The original upstream WAVs are never modified.

Tuning on device:
  1. Play sweep_sp.wav and listen for the band where the speaker is loudest.
  2. Set CRITICAL_FREQ / DM_CRITICAL_FREQ near that band.
  3. Re-run this script and restart soundd.
  DRIVE controls harmonic richness: 1.0 = pure sine (soft), higher = more square
  (louder, harsher, cuts through music better).

Output: mono, 16-bit, 48 kHz (matches soundd load_sounds asserts).
"""
import os
import wave
import numpy as np

SR = 48000
MAX_INT16 = 32767
OUT_DIR = os.path.dirname(os.path.abspath(__file__))

# --- Tunables ---
DRIVE = 4.0               # 1.0 = sine, higher = more square / louder / harsher
CRITICAL_FREQ = 2400.0    # warningSoft (steady). Stock: 1316 Hz
DM_CRITICAL_FREQ = 1800.0  # warningImmediate (pulsed). Stock: 1045 Hz
CRITICAL_DUR = 0.54       # seconds, steady (loops -> continuous tone)
DM_CRITICAL_DUR = 0.54    # seconds, pulsed (loops -> repeating urgent beeps)
DM_PULSE_ON = 0.12        # seconds per beep (on)
DM_PULSE_OFF = 0.06       # seconds between beeps (off)


def squareish(freq: float, duration: float, drive: float = DRIVE) -> np.ndarray:
  """Sine pushed through a clipper. drive=1 -> sine (crest 3 dB),
  drive->inf -> square (crest 0 dB, max RMS). Length is rounded to an integer
  number of periods so the tone loops without a click."""
  periods = max(1, round(freq * duration))
  n = int(round(periods * SR / freq))
  t = np.arange(n) / SR
  return np.clip(drive * np.sin(2.0 * np.pi * freq * t), -1.0, 1.0)


def write_wav(name: str, x: np.ndarray) -> None:
  x = np.clip(x, -1.0, 1.0)
  pcm = (x * MAX_INT16).astype(np.int16)
  path = os.path.join(OUT_DIR, name)
  with wave.open(path, "wb") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes(pcm.tobytes())
  rms = float(np.sqrt(np.mean(x.astype(np.float64) ** 2)))
  rms_db = 20.0 * np.log10(rms) if rms > 0 else float("-inf")
  print(f"  {name}: peak={np.max(np.abs(x)):.3f}  rms={rms_db:6.2f} dBFS  "
        f"len={len(x) / SR:.3f}s  ({len(pcm)} frames)")


def make_critical() -> None:
  x = squareish(CRITICAL_FREQ, CRITICAL_DUR, DRIVE)
  write_wav("critical_sp.wav", x)


def make_dm_critical() -> None:
  """Pulsed urgent beeps: full-scale square carrier gated on/off. Distinct from
  the steady critical tone, and more attention-grabbing. Short fades on each
  edge avoid clicks."""
  carrier = squareish(DM_CRITICAL_FREQ, DM_CRITICAL_DUR, DRIVE)
  n = len(carrier)
  gate = np.zeros(n)
  on_n = int(SR * DM_PULSE_ON)
  off_n = int(SR * DM_PULSE_OFF)
  fade = int(SR * 0.001)  # 1 ms edge fade
  i = 0
  while i < n:
    seg_end = min(i + on_n, n)
    ramp = min(fade, seg_end - i)
    gate[i:i + ramp] = np.linspace(0.0, 1.0, ramp)              # fade in
    gate[i + ramp:seg_end] = 1.0
    ramp_out = min(fade, seg_end - (i + ramp))
    if ramp_out:
      gate[seg_end - ramp_out:seg_end] = np.linspace(1.0, 0.0, ramp_out)  # fade out
    i = seg_end + off_n
  write_wav("dm_critical_sp.wav", carrier * gate)


def make_sweep() -> None:
  """Constant-amplitude linear sweep 500 Hz -> 5 kHz over 5 s. Loudness
  differences heard during playback reveal the speaker + ear response, so the
  user can pick the most efficient warning frequency by ear."""
  dur = 5.0
  n = int(SR * dur)
  t = np.arange(n) / dur
  f0, f1 = 500.0, 5000.0
  freq = f0 + (f1 - f0) * t
  phase = 2.0 * np.pi * np.cumsum(freq) / SR
  write_wav("sweep_sp.wav", 0.7 * np.sin(phase))


if __name__ == "__main__":
  print("Generating zoompilot safety-warning WAVs:")
  make_critical()
  make_dm_critical()
  make_sweep()
  print(f"  DRIVE={DRIVE}, critical={CRITICAL_FREQ} Hz, dm_critical={DM_CRITICAL_FREQ} Hz")
  print("Play sweep_sp.wav on device to find the loudest frequency band.")
