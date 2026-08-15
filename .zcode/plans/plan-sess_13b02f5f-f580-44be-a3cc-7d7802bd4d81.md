## Goal
New PR into `zoompilot/zoompilot` that achieves the goal of [upstream PR #1](https://github.com/zephleggett/openpilot/pull/1): make Mazda go offroad promptly at key-off despite `ignitionLine` lagging ~30s.

## Approach: the latch, kept (Option A)
Keep the latch algorithm. The never-reset is load-bearing — offroad happens exactly when `ignitionCan` turns false while `ignitionLine` is still true, so a resettable latch would oscillate onroad↔offroad. Fix targets the OFF transition; ON is handled by the pre-CAN `ignitionLine` fallback. Per-process latch independence is safe (all three processes latch during the drive, so all return False together on the falling edge). Ship raw, no debounce (0 dropouts in 188k segments).

Placement: `openpilot/common/ignition.py` — a pure utility shared by three core processes, matching `params.py`/`swaglog.py`. Not Mazda-gated (ON transition precedes fingerprinting; resolver is a no-op where both signals fall together).

## Changes

### 1. New file: `openpilot/common/ignition.py`
Module-global latch + pure `get_ignition_state(panda_states) -> bool`. No docstring, no cloudlog. 2-line comment max. Unknown-panda filter retained (defensive; matches original `manager`/`hardwared` inline filters; prevents a spurious `ignitionCan` on an unidentified slot from falsely latching).

```python
from cereal import log


# Once CAN ignition is seen on any valid panda, stop trusting ignitionLine:
# on Mazda it lags ~30s after key-off while ignitionCan falls promptly.
# The latch never resets; resetting would oscillate onroad/offroad.
ignition_can_seen = False


def get_ignition_state(panda_states) -> bool:
  global ignition_can_seen

  valid = [ps for ps in panda_states if ps.pandaType != log.PandaState.PandaType.unknown]
  if not valid:
    return False

  if any(ps.ignitionCan for ps in valid):
    ignition_can_seen = True
    return True

  return False if ignition_can_seen else any(ps.ignitionLine for ps in valid)
```

### 2. Three one-line call-site edits + imports
Replace each inline `any(ps.ignitionLine or ps.ignitionCan ...)` with `get_ignition_state(...)`. Exact current text verified:
- `openpilot/selfdrive/ui/ui_state.py:149` (import near line 8-14 block)
- `openpilot/system/hardware/hardwared.py:231` (import near line 25)
- `openpilot/system/manager/manager.py:152` (import near line 19)

## Out of scope
No tests (behavior verified by 188k-segment dataset, far stronger than pytest on a trivial pure function). No debounce, no pandad/C++/schema changes (Option B rejected), no Mazda gating.

## Workflow
Branch off a worktree on `mzdnick/zoompilot`, implement, verify with `git diff`, commit. **Do NOT create a PR.** After committing, draft (in chat only) a PR title and body for you to use manually.