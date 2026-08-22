# Mazda stock LKAS passthrough (disengaged)

With the device onroad, openpilot owns `CAM_LKAS` (0x243) and
`CAM_LANEINFO` (0x440) on bus 0. Before this change the stock camera's
own frames were blocked from reaching the car, so the stock lane-keep
correction and the dash lane-departure display were dead whenever
openpilot was disengaged.

## How it works

The Mazda safety mode (`opendbc_repo/opendbc/safety/modes/mazda.h`)
replaces those two channels only while openpilot is controlling:

- The 0x243 and 0x440 TX entries keep `check_relay` (harness fault
  detection unchanged) but opt out of the static forwarding block.
- `mazda_fwd_hook` forwards the camera's 0x243 and 0x440 to the car
  whenever openpilot is not controlling. "Controlling" is
  `controls_allowed || controls_allowed_lateral` (MADS-aware), not ACC
  state, so the passthrough also works with ACC on but lateral
  disengaged.
- While the camera's frames forward, the tx hook drops our own 0x243
  and 0x440, so the two senders never share the bus. The car controller
  is unchanged; the panda filters its frames.

The EPS sees one continuous sender per state, so bus handoffs happen
only at engage and disengage. Disengaged behavior matches a stock car,
and the car's own settings menu stays the user-facing control (warning
and correction settings flow to the camera on `CAM_SETTINGS` 0x485,
which is always forwarded car-to-camera).

Because our idle zero-torque frames yield to the camera's stream while
disengaged, the shared torque tests' "zero-torque commands transmit
while controls are off" contract no longer holds for Mazda. The shared
test base parametrizes it as `DISENGAGED_IDLE_STEER_TX` (default True,
same pattern as `NO_STEER_REQ_BIT`); the Mazda test class sets it
False.

While engaged, openpilot's 0x440 re-send relays the camera's whole
decoded `CAM_LANEINFO` frame from `CS.cam_laneinfo`, overriding only the
hands warnings, the way VW relays `LDW_SW_Warnung` through its
replacement HUD message. The first cut relayed a curated signal list and
device testing proved it wrong: a right-line departure flashed the left
alert and a left-line departure showed nothing, because the dash picks
the departure side from more than the `LDW_WARN_LL/RL` bits. The dash
stays a live, camera-driven lane-departure display in both states,
including the steering-override window with ACC on. No toggle: the car's
own lane-departure-alert setting governs, because the camera obeys it.

## Device validation checklist

- Stock correction returns when disengaged (car settings: intervention
  ON).
- Stock dash amber-line lane-departure warnings return.
- Engage and disengage repeatedly, including at speed; watch for LKAS
  faults around the handoffs (our 0x243 counter restarts at `frame % 16`
  while the camera's runs independently; a jump at each engage edge is
  expected — the EPS tolerance for it is the main open risk).
- Engaged: confirm orange lines appear on the correct side when crossing a
  line. Device test 2026-08-21 with the curated signal list showed a
  right-line departure flashing the left alert and a left-line departure
  showing nothing; the full-frame relay (opendbc 3e73dfd1) is the fix to
  retest. Flashes should look the same as disengaged. The camera natively
  sends 0x440 at ~2 Hz (measured from route 9ff653756), so our 2 Hz
  re-send is stock cadence; extra clipping beyond stock is not possible.
- Steering-override disengage with ACC still on.
- Alpha-long on and off (CX-5 2022).
- Camera failure while disengaged now shows as a stock-like LKAS fault
  (no 0x243 replacement); engaged driving is unaffected.

A safety header change requires a panda firmware rebuild and reflash;
the prebuilt `main` branch picks it up at the next release cut.
