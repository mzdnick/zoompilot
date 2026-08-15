# The SP mirror pattern: a walkthrough

This is the house pattern zoompilot (via sunnypilot) uses to extend
openpilot without forking the upstream message definitions. Read it before
extending a message, a service, or an enum.

## Why the pattern exists

openpilot's message schemas live in `openpilot/cereal/log.capnp`. The fork
does not edit upstream files (it rebases on them). To add fields, the fork
creates a parallel struct that mirrors the upstream one and carries the
extra fields. The convention is to suffix the name with `SP`.

So instead of editing `carParams`, the fork defines `CarParamsSP` in
`openpilot/cereal/custom.capnp`. A bridging daemon converts between the two
at runtime.

## The four touch points

When you add or extend an SP message, you change up to four places. Each is
marked so the fork can rebase cleanly.

Naming: the **struct** is PascalCase (for example `SelfdriveStateSP`, defined
in `custom.capnp`). The **service name** is camelCase (for example
`selfdriveStateSP`, registered in `services.py`). They share a stem but differ
in case. Keep both consistent with the upstream stem.

| Step | File | Marker |
| --- | --- | --- |
| 1. Define the struct | `openpilot/cereal/custom.capnp` | fork-only file; no marker needed |
| 2. Register the service | `openpilot/cereal/services.py` | `# sunnypilot` block |
| 3. Add params (if any) | `openpilot/common/params_keys.h` | `// --- sunnypilot params ---` block |
| 4. Write the bridge daemon | `openpilot/sunnypilot/...` | mirrors upstream layout |

## Worked examples in the tree

These are the canonical instances of the pattern. Read the relevant one
before building your own.

### `longitudinalPlan` ↔ `longitudinalPlanSP`
- Upstream: `longitudinalPlan` (in `log.capnp`), published by `plannerd`
  running `longitudinal_planner.py` (`selfdrive/controls/lib/`).
- Fork: `LongitudinalPlanSP` (in `custom.capnp`), carries DEC,
  SmartCruiseControl, and SpeedLimit state. `mapd` injects map speed limits
  into it.
- Bridge: the SP `lib` code converts between the two.

### `carParams` ↔ `CarParamsSP`
- Upstream: `carParams`, decoded by `card`.
- Fork: `CarParamsSP`, carries fork flags (MADS, neural-network lateral
  control, ICBM, etc.).
- Bridge: `convert_to_capnp` and related helpers in
  `openpilot/selfdrive/car/helpers.py`.

### `selfdriveState` ↔ `selfdriveStateSP`
- Upstream: `selfdriveState` from `selfdrived`.
- Fork: struct `SelfdriveStateSP` (PascalCase, in `custom.capnp`), service
  `selfdriveStateSP` (camelCase, in `services.py`). Carries MADS state and
  fork `AudibleAlert` slots.
- Bridge: `selfdrived` (`openpilot/selfdrive/selfdrived/selfdrived.py`)
  publishes both, with `controlsd_ext` providing the fork fields.

### `carControl` ↔ `CarControlSP`
- Conversion helper: `convert_carControlSP` in
  `openpilot/selfdrive/car/helpers.py`. Use this as the model for any
  upstream↔SP struct conversion.

## Extending an enum safely

Fork enums live in `custom.capnp`. openpilot enums are often defined with
reserved slots so downstream forks can add values without renumbering.

Rules:
- Append new values at the end. Never renumber existing values — they are
  serialized as integers on the wire.
- Use a reserved slot if one exists; do not invent a number.
- Keep both upstream and fork enum values in sync where they overlap.

Worked example: the alert-volume feature added two single-beep alerts,
`promptSingleLow` and `promptSingleHigh` (ids 31–32), to
`SelfdriveStateSP.AudibleAlert` in `custom.capnp`, using reserved slots.

## Decision guide

| You want to... | Do this |
| --- | --- |
| Add a brand-new fork message | Define the struct in `custom.capnp`, register in `services.py` (`# sunnypilot`), add params if needed, write the publisher daemon. No upstream message involved. |
| Add fields to an existing upstream message | Mirror the upstream struct as `*SP` in `custom.capnp`, register the `*SP` service, write a bridge daemon that converts between them. |
| Add a config setting | Add a param in `params_keys.h` (`// --- sunnypilot params ---`). See the `params-store` skill for flag choices. |
| Add a new background daemon | Write the daemon and register it in `process_config.py`. See the `managed-processes` skill. |

## What to check before you finish

- Did you touch the upstream `log.capnp` or `services.py` upstream block?
  If yes, you are editing the wrong place. Fork work goes in `custom.capnp`
  and the `# sunnypilot` block of `services.py`.
- Are you on the `develop` branch of zoompilot? (Not `master`, not `main`.)
- Did you renumber an enum value? Revert it. Append only.
- If you added a param, did you set the right `CLEAR_*` and `PERSISTENT`
  flags?
- If you added a daemon, did you import heavy/native libraries lazily
  inside the thread, not at module top?
