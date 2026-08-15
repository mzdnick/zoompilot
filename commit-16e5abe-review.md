ChatGPT:

I reviewed commit `16e5abeec35faff81c719a6f1d529c2619095819`. It is a fairly clean implementation, but **I would not call it fully production-ready yet**. I’d rate it around **8/10**: the core behavior looks correct, but there are a couple of things I would change before shipping broadly.

The commit adds a persistent `AlertVolumeBoost` parameter, updates the on-device and SunnyLink UIs, calculates a dB-domain boost, caps gain based on the source waveform peak, and explicitly excludes `warningSoft` and `warningImmediate`. ([GitHub][1])

### What looks good

The most important part—the actual gain calculation—is reasonably well thought out.

At 0%, the new branch isn't entered at all, so the old path remains literally:

```python
return ret * self.current_volume
```

That gives you the claimed bit-identical 0% behavior rather than merely an approximately equivalent calculation. ([GitHub][1])

The parameter is also defensively clamped:

```python
return max(0, min(ALERT_VOLUME_BOOST_MAX, value))
```

and malformed values fall back to zero rather than killing `soundd`. Since the main loop calls `load_param()` every 20 Hz, changes to the setting take effect without restarting `soundd`. ([GitHub][2])

The peak limiting is also sensible. The source audio is normalized to `[-1, 1]`, its peak is measured once at load time, and the resulting gain is capped at `1 / src_peak`, so the digital output shouldn't clip simply because the source file wasn't normalized. ([GitHub][2])

And I like the explicit safety decision to leave `warningSoft` and `warningImmediate` untouched. Those are the two most obvious alerts where changing loudness could have undesirable consequences. ([GitHub][2])

### The biggest issue: doing `log10()` and `pow()` in the audio callback

This is my main production-readiness objection.

`get_sound_data()` is called directly by the PortAudio callback, and the new code performs:

```python
cv_db = 20.0 * math.log10(volume)
...
volume = math.pow(10.0, eff_db / 20.0)
```

inside that callback. ([GitHub][2])

`soundd` is running a 48 kHz stream with a 4096-frame buffer, so the callback runs roughly every 85 ms. ([GitHub][3])

This probably works fine in practice—the computational load is tiny—but **real-time audio callbacks are exactly where unnecessary Python work, allocations, locks, or unpredictable latency should be avoided**.

There's no reason to calculate this gain every audio callback. `current_volume` only changes from the main `soundd_thread()` loop, which already runs at 20 Hz. ([GitHub][2])

I'd instead compute an effective multiplier in the main loop:

```python
self.alert_volume_multiplier = ...
```

and have the callback do essentially just:

```python
return ret * self.alert_volume_multiplier
```

That makes the real-time path almost identical to the original implementation.

### I'd also reconsider the safety-alert exclusion list

The commit explicitly excludes:

```python
warningSoft
warningImmediate
```

but `sound_list` also contains:

```python
promptDistracted
preAlert
```

and potentially other alerts that could have safety significance depending on how SunnyPilot uses them. ([GitHub][2])

`promptDistracted` in particular is worth examining carefully because it is the driver-monitoring warning sound (`dm_warning.wav`). I wouldn't automatically say it must be excluded, but I **wouldn't describe only two alerts as "safety-critical alerts" without explicitly documenting why the others are safe to boost**.

A better design might be an explicit category such as:

```python
BOOST_EXCLUDED_ALERTS = frozenset({
    AudibleAlert.warningSoft,
    AudibleAlert.warningImmediate,
    # promptDistracted if deemed safety-critical
})
```

with a comment explaining the safety classification.

### The commit's testing claim is stronger than the actual repository change

The commit message says:

> "Verified standalone across all 13 sounds x 3 cabin levels..." ([GitHub][1])

But the commit itself only changes five files and doesn't add an automated test. ([GitHub][1])

That's the biggest process gap I'd fix before calling it production-ready.

I'd want an actual regression test for `get_sound_data()` covering at least:

* 0% is byte-for-byte identical.
* 20/40/60/80/100% never produce a sample outside `[-1, 1]`.
* Boost never decreases output relative to stock.
* `warningSoft` and `warningImmediate` are completely unchanged.
* malformed/missing `AlertVolumeBoost` becomes 0.
* values `<0` and `>100` are clamped.
* both normal and Tizi sound assets are covered.
* changing the parameter while `soundd` is running takes effect.
* a waveform with a peak below 1.0 gets the expected gain.

That would turn the claims in the commit message into something enforceable.

### One subtle semantic issue with the volume curve

The formula is:

```python
eff_db = max(
    cv_db * (1.0 - v_frac),
    cv_db + v_frac * 6.0
)
```

So 100% boost effectively pushes the effective volume toward **0 dBFS**, rather than simply applying +6 dB.

That's probably intentional based on the description, and it produces an interesting result:

* quiet cabin → aggressively approaches full scale
* loud cabin → approximately +6 dB
* already-loud audio → peak cap prevents clipping

I actually think that's a reasonable UX model for a "make alerts louder" control.

However, I'd document the behavior more explicitly because `"Volume Boost 60%"` does **not** mean "+3.6 dB". It is a blend between two different gain strategies. The UI makes it look like a conventional percentage slider, which could be misleading. ([GitHub][1])

### Another recommendation: precompute the boost multiplier

Since there are only a handful of possible values—0, 20, 40, 60, 80, 100—the simplest implementation could precompute the effective multiplier whenever the parameter or `current_volume` changes.

For example:

```python
def update_volume_multiplier(self):
    volume = self.current_volume

    if self.alert_volume_boost and self.current_alert not in BOOST_EXCLUDED_ALERTS:
        v_frac = self.alert_volume_boost / 100.0
        cv_db = 20.0 * math.log10(volume)
        eff_db = max(
            cv_db * (1.0 - v_frac),
            cv_db + v_frac * ALERT_VOLUME_BOOST_DB,
        )
        volume = 10.0 ** (eff_db / 20.0)

        src_peak = self.loaded_sound_peaks.get(self.current_alert, 1.0)
        volume = min(volume, 1.0 / src_peak)

    self.volume_multiplier = volume
```

Then the audio callback remains extremely cheap.

### Minor things

The commit is also touching five separate UI/config surfaces for the same parameter. That's necessary in this architecture, but I'd make sure the two SunnyLink definitions can't drift from one another in the future. The values are currently consistent: 0–100, step 20. ([GitHub][1])

I'd also consider whether 20% increments are too coarse. They're fine for a first release, but a user wanting "a little louder" only gets 20% jumps.

### Verdict

**I would merge this for testing/staging, but I would make two changes before production:**

1. **Move the gain calculation out of the PortAudio callback.**
2. **Add real automated tests for the gain/safety behavior rather than relying exclusively on the standalone verification described in the commit message.**

I'd also explicitly review whether `promptDistracted` belongs in the excluded set.

Once those are addressed, **yes, the underlying approach is production-worthy**. I don't see an obvious audio-clipping bug, parameter corruption issue, or regression in the 0% path from the code itself. The audio pipeline, parameter reload, and safety exclusions are all integrated coherently. ([GitHub][2])

**Overall: 8/10 — good implementation, not quite production-ready until the real-time callback and test coverage are cleaned up.**

[1]: https://github.com/zoompilot/zoompilot/commit/16e5abeec35faff81c719a6f1d529c2619095819 "feat(soundd): add Volume Boost setting (0-100% dB gain) · zoompilot/zoompilot@16e5abe · GitHub"
[2]: https://github.com/zoompilot/zoompilot/blob/16e5abeec35faff81c719a6f1d529c2619095819/openpilot/selfdrive/ui/soundd.py "zoompilot/openpilot/selfdrive/ui/soundd.py at 16e5abeec35faff81c719a6f1d529c2619095819 · zoompilot/zoompilot · GitHub"
[3]: https://github.com/zoompilot/zoompilot/blob/41988b2/openpilot/selfdrive/ui/soundd.py "zoompilot/openpilot/selfdrive/ui/soundd.py at 41988b2f8d1dd93b79010f254a37305110ac7823 · zoompilot/zoompilot · GitHub"


-------

Claude:

This commit adds a **Volume Boost** setting to `soundd.py` in the zoompilot fork (Mazda-focused openpilot/sunnypilot fork). It's a small, well-scoped, and fairly carefully engineered change. Here's my read:

## What it does
- New param `AlertVolumeBoost` (0–100%, default 0) lets the user boost alert loudness.
- In `get_sound_data`, it computes a dB-domain gain: for quiet cabins it lifts volume toward full scale; for loud cabins it adds up to +6 dB — taking the *max* of the two formulas.
- The result is capped by `1.0 / src_peak`, a per-sound headroom value precomputed at load time, so output can never clip or exceed full scale.
- `warningSoft` and `warningImmediate` (the safety-critical alerts) are explicitly excluded — they always play at stock volume.
- UI exposed in three places: on-device settings layout, sunnylink JSON, and sunnylink YAML source — all kept in sync at 20% steps.

## Strengths
- **Correct dB math with a subtle edge case handled well.** For volumes above 0 dBFS (if `current_volume` can exceed 1.0), the "lift toward full scale" formula would actually *reduce* gain — but since the code takes `max(A, B)`, formula B (flat +6 dB) wins in that case, so boost never becomes a de-boost. That's a nice bit of defensive design, whether or not it was intentional.
- **No-clip guarantee is real**, not just claimed: the `1/src_peak` cap combined with peak-per-sound precomputation means output is mathematically bounded to full scale regardless of boost level.
- **Safety-critical alerts are hard-excluded**, and the commit explains why (they already ship near 0 dBFS, so boost could only distort them, not help them).
- **Division-by-zero guarded**: `float(np.max(np.abs(data))) or 1.0` protects against a theoretical all-silent WAV.
- **`log10(0)` guarded** via the `volume > 0` check before computing `cv_db`.
- Defaults are safe: 0% is bit-identical to stock, and the commit claims nothing is ever quieter than stock.
- All three config surfaces (on-device UI, JSON, YAML) were updated consistently — a common place for forks like this to drift.

## Gaps / things I'd flag before calling it fully production-ready
1. **No automated test committed.** The commit message describes solid verification (13 sounds × 3 cabin levels × 6 boost levels, checking invariants), but none of that appears to have landed as a test file in this diff. For an alert/audio subsystem in a driving-assist product, I'd want that verification script or a pytest checked into the repo so it runs in CI and guards against regressions — not just a one-time manual run.
2. **Only the two most urgent alerts are excluded.** Alerts like `promptDistracted` still get boosted. That may be intentional (they're not "urgent takeover" alerts), but it's worth being an explicit, documented decision rather than an implicit one, since the exclusion list is exactly the kind of thing a future contributor might carelessly edit.
3. **No cap on how large the "lift toward full scale" boost can be in absolute dB terms.** In a very quiet cabin, boost=100% can push an alert from, say, -40 dBFS to 0 dBFS — a ~40 dB jump, far beyond the "+6 dB" framing in the commit message (which only strictly applies to the loud-cabin branch). It's bounded from a clipping standpoint, but not from a "sudden loud noise startles driver" UX standpoint. Worth a deliberate product decision, not a blocker.
4. **Thread-safety is implicit, not documented.** `alert_volume_boost` is written on the param-reload path and read inside the real-time audio callback. This is almost certainly fine under CPython's GIL for a simple int, but a one-line comment noting that assumption would help future maintainers who might change the type.

## Recommendation
This is solid, careful work for what it is — I wouldn't block it on those points, but I'd request the test-suite addition before merging to a shared/production branch, and I'd ask the author to make the "which alerts are excluded" decision explicit in a comment or PR description so it's a deliberate, reviewable choice rather than something that quietly falls out of which two enum values happened to be picked.


