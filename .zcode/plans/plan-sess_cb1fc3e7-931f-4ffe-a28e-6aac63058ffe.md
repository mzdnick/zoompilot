## Fix: raise the offroad fan cap so a parked hot device can cool (base: origin/main)

### Problem
`fan_controller.py:18` pins the offroad fan to a flat 30 % ceiling. When the car is off, the fan cannot exceed 30 % even at 95 °C+. The forced-100 % override in `hardwared.py:342-346` is ignition-gated, so it never fires offroad. Result: the device overheats while parked.

The PID's `pos_limit` clips both output and integrator (`common/pid.py:53-58`), so it is the single correct knob. The existing feedforward already computes a high fan value when hot — the 30 % cap discards it.

### Approach
Modify `fan_controller.py` so the offroad ceiling scales with temperature: stay near 30 % when cool (preserves noise-reduction intent), ramp to 100 % across the danger→critical range. Deliberate fork divergence from upstream. No change to `hardwared.py`.

### Change 1 — `openpilot/system/hardware/fan_controller.py`
Add device-specific cap anchors (75/95 tici-tizi; 85/100 mici) and replace the flat 30 % cap with an `np.interp` ramp 30→100.

### Change 2 — `openpilot/system/hardware/tests/test_fan_controller.py`
Replace `test_offroad_limits` (asserts `<=30` at 100 °C) with two tests: cool offroad stays `<=30`; hot offroad exceeds 30.

### Setup
- Worktree at `../zoompilot-offroad-fan`, branch `offroad-fan-cap`, based on `origin/main` (per user: faster to flash/test; will rebase to `develop` if upstreamed).
- Verify: `pytest openpilot/system/hardware/tests/test_fan_controller.py`.