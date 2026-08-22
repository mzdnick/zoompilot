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

While engaged, openpilot relays the camera's `CAM_LANEINFO` frame byte
for byte: `carstate` decodes the raw frame through two whole-frame
signals (`FRAME_RAW_HI/LO`, added to the DBC for exactly this), and the
controller re-sends those exact bytes on each new camera frame, at the
camera's own cadence. The camera's hands warning passes through
untouched in both states — mapping openpilot's `steerRequired` events
onto those bits painted an orange steering wheel for every wheel-touch
and distraction alert. `CAM_LKAS` (0x243) is likewise an overlay on the
camera's exact frame: openpilot writes only the torque field, the
counter (continuing the camera's sequence at each engage edge) and the
zero-angle pattern, adjusting the checksum by exactly the fields
touched — a delta off the camera's own checksum, so bits outside the
formula's model keep the camera's own contributions. Decode-and-re-encode
cannot do any of this: the DBC documents only 23 of 0x440's 64 bits and
42 of 0x243's 64, and the packer zero-fills the undocumented rest. Device testing drove
that home three times — a curated signal list flashed the wrong
departure side (the dash picks the side from more than the
`LDW_WARN_LL/RL` bits), the full decoded relay still produced an
intermittent "front camera system malfunction" once the lane system went
active (live defined bits riding on zeroed undefined bits, byte 2 of
0x440 entirely undefined and plausibly a counter), and the byte-exact
0x440 alone still flashed the wrong side engaged, proving the dash reads
departure-side bits from 0x243 as well. A camera quiet past 1 s holds
the last 0x440 frame at 2 Hz so the dash keeps its lane display. The
dash stays a live, camera-driven lane-departure display in both states.
No toggle: the car's own lane-departure-alert setting governs, because
the camera obeys it.

## Device validation checklist

- Stock correction returns when disengaged (car settings: intervention
  ON).
- Stock dash amber-line lane-departure warnings return.
- No "front camera system malfunction" when the lane system activates,
  disengaged (watch the cruise-main-on moments in particular) or engaged.
- Engage and disengage repeatedly, including at speed; watch for LKAS
  faults around the handoffs. Counter semantics: while engaged our 0x243
  advances once per controller frame at 100 Hz with no relation to the
  camera's counter — that is the pre-branch behavior, proven by years of
  upstream Mazda use and this car's own pre-branch driving, and while
  engaged the camera's 0x243 is blocked, so no ECU can observe a
  continuing relationship anyway (cross-checks against the still-forwarded
  camera messages such as 0x242 are excluded by the same pre-branch
  evidence). The engage-edge seed continues the camera's sequence but is
  only ±1–2 counts exact: we cannot know which camera frame the panda
  last forwarded at the flip instant, and frames parsed after the flip
  never reached the car. That residual is still far milder than the
  arbitrary phase jump the EPS tolerated pre-branch. At disengage the
  camera resumes its own counter, so a jump remains on that edge — the
  EPS tolerance for it is the main open risk.
- Engaged: confirm orange lines appear on the correct side when crossing
  a line, same as disengaged. Curated, full-decoded and 0x440-byte-exact
  cuts all got this wrong or faulted (see above); the 0x243 overlay is
  the cut to retest. The relay fires on each new camera frame, so warn
  pulses are not clipped to the 2 Hz grid.
- The "hold the wheel" nag is the camera's in both states now: it should
  appear with hands off, clear within a second or two of a firm grip,
  and stay silent while merely touching the wheel.
- Steering-override disengage with ACC still on.
- Alpha-long on and off (CX-5 2022).
- Camera failure while disengaged now shows as a stock-like LKAS fault
  (no 0x243 replacement); engaged driving is unaffected.

A safety header change requires a panda firmware rebuild and reflash;
the prebuilt `main` branch picks it up at the next release cut.
