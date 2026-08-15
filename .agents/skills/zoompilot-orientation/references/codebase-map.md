# zoompilot codebase map

Use this map to find things fast. All paths are relative to the repo root.

## The four orientation shortcuts

When you do not know where something lives, start here:

| You want to find... | Go to |
| --- | --- |
| All messages on the bus | `openpilot/cereal/services.py` (registry) |
| All processes that run | `openpilot/system/manager/process_config.py` (`managed_processes`) |
| All config keys | `openpilot/common/params_keys.h` (single source of truth) |
| All fork-specific code | `openpilot/sunnypilot/` (mirrors the openpilot tree layout) |

Two schema files back the message registry:
- `openpilot/cereal/log.capnp` — upstream openpilot messages.
- `openpilot/cereal/custom.capnp` — fork extension messages. Any struct
  ending in `SP` is fork-specific.

## The fork chain

```
commaai/openpilot  (upstream)
      │
   sunnypilot  (fork)
      │
   zoompilot  (fork)   ← you are here
```

- Upstream code lives under the usual openpilot paths
  (`openpilot/selfdrive/`, `openpilot/system/`, `openpilot/common/`,
  `openpilot/cereal/`).
- Fork code lives under `openpilot/sunnypilot/`, which mirrors the upstream
  layout: `selfdrive/{car,controls,selfdrived,locationd,ui}`,
  `models/`, `modeld_v2/`, `mads/`, `mapd/`, `sunnylink/`, `system/`.

## Branch model (per repo)

| Repo | Uncompiled source | Prebuilt release | Prebuilt dev |
| --- | --- | --- | --- |
| openpilot | `master` | `release-mici` / `release-tizi` | `nightly` |
| sunnypilot | `master` | `release-mici` / `release-tizi` | `dev` |
| zoompilot | `develop` | `main` | (none) |

Traps:
- zoompilot source is `develop`, NOT `master`. Defaulting to `master` lands
  you on the wrong branch.
- zoompilot `main` is prebuilt, not source. Editing on `main` is wrong.
- zoompilot has no prebuilt dev branch.

## Device codenames

| Codename | Hardware | Status |
| --- | --- | --- |
| `tici` | comma three (c3) | deprecated. openpilot no longer officially supports it. |
| `tizi` | comma three x (c3x) | supported. closely similar to tici. |
| `mici` | comma four (c4) | supported. |

Traps:
- tici and tizi are closely similar. Many files used by tizi still reference
  tici. Seeing `tici` in code does NOT mean tici is the active target.
- openpilot looks ready to deprecate and remove tizi. tizi code may vanish
  during an upstream rebase.

## Subsystem map

| Subsystem | Location | Notes |
| --- | --- | --- |
| Model Manager | `openpilot/sunnypilot/models/` | The fork's signature feature. Downloads, validates, and selects driving models. |
| tinygrad modeld | `openpilot/sunnypilot/modeld_v2/` | tinygrad-based replacement driving-model daemon. |
| MADS | `openpilot/sunnypilot/mads/` | Modular Assistive Driving System. Decouples lateral (steering) enable from longitudinal (cruise) enable. |
| mapd | `openpilot/sunnypilot/mapd/` | Fork map-data daemon. OSM/Mapbox speed limits and road geometry. |
| locationd (fork) | `openpilot/sunnypilot/selfdrive/locationd/` | Fork native locationd (`locationd_llk`), publishes `liveLocationKalman`. |
| sunnylink | `openpilot/sunnypilot/sunnylink/` | Remote management + settings backup/restore. Counterpart to openpilot's athena. |
| Fork UI | `openpilot/selfdrive/ui/sunnypilot/` | Fork on-device UI. raylib/pyray (NOT Qt). See the `ui-raylib` skill. |
| Fork controls | `openpilot/sunnypilot/selfdrive/controls/controlsd_ext.py`, `openpilot/sunnypilot/selfdrive/controls/lib/` | Fork overlays on the planner/controller. |
| Fork car extensions | `openpilot/sunnypilot/selfdrive/car/` | `interfaces.py`, `cruise_arbiter.py`, `cruise_ext.py`, `intelligent_cruise_button_management/`. |
| Car ports | `opendbc_repo/opendbc/car/<make>/` | NOT under `openpilot/selfdrive/car/`. `opendbc` is an editable submodule; the `opendbc/` path is a symlink to `opendbc_repo/opendbc/`. Each make dir: `interface.py`, `carstate.py`, `carcontroller.py`, `values.py`, `fingerprints.py`. |
| DBC files | `opendbc_repo/opendbc/dbc/*.dbc` | CAN bus protocol per car (about 120 files). |

## Driving-cycle data flow

Trace any feature as a pipeline:

1. **Sensors in.** `pandad` (`openpilot/selfdrive/pandad/`) talks to the
   panda over USB, publishes raw `can` and `sendcan`. `sensord`
   (`openpilot/system/sensord/`) publishes `accelerometer` /
   `gyroscope` / `temperatureSensor`. `micd` publishes `soundPressure` /
   `rawAudioData`.
2. **Car decode.** `card` (`openpilot/selfdrive/car/card.py`) calls
   `get_car(...)` (from `opendbc.car.car_helpers`) to fingerprint the car
   and instantiate the right `Car Interface`. Each loop decodes `carState` /
   `carControl` using the per-make `carstate.py` + `carcontroller.py` + DBC
   parser. Fork overlays: `openpilot/sunnypilot/selfdrive/car/interfaces.py`,
   `cruise_arbiter.py`, MADS in `card.py`.
3. **Perception.** `modeld` / `modeld_tinygrad` consumes road camera frames
   (`roadCameraState`) and emits `modelV2` + `drivingModelData` (and fork
   `modelDataV2SP`). `dmonitoringmodeld` emits driver monitoring. `radard`
   fuses lead cars into `radarState`.
4. **Localization.** `locationd` (+ fork `locationd_llk` →
   `liveLocationKalman`), `paramsd` (`liveParameters`), `torqued`
   (`liveTorqueParameters`), `calibrationd` (`liveCalibration`),
   `lagd` (`liveDelay`).
5. **Planning.** `plannerd` runs `longitudinal_planner.py`
   (`selfdrive/controls/lib/`) → `longitudinalPlan` (+ fork
   `longitudinalPlanSP`). Lateral planning uses
   `latcontrol_{pid,torque,angle,curvature}.py`. `mapd` injects map speed
   limits into the SP longitudinal plan.
6. **Supervision.** `selfdrived` (`openpilot/selfdrive/selfdrived/`) is the
   supervisor. It owns the enable/disable state machine (`StateMachine`),
   the `Events` system, and the `AlertManager`. It publishes `selfdriveState`
   (+ fork `selfdriveStateSP`) and `onroadEvents` (+ fork `onroadEventsSP`).
7. **Control output.** `controlsd` (`openpilot/selfdrive/controls/`)
   subscribes to `selfdriveState` and the plans, runs the lateral controller
   and long control, and emits `carControl` (+ fork `carControlSP`) and the
   legacy `controlsState`. The UI (raylib/pyray, see the `ui-raylib` skill)
   and `soundd` read `selfdriveState` to render alerts.

## Fork pattern in one line

Trace any SP feature as **upstream message + fork mirror message + fork
daemon that bridges them**. Example: `longitudinalPlan` ↔
`longitudinalPlanSP`, with `plannerd` and the SP `lib` doing the bridging.
See the `add-fork-extension` skill for the full pattern.
