"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

The chestnut board as a warp-split accelerator, on tici/tizi only.

Upstream #38684 fused warp and policy into one JIT on the board, so both full
camera frames cross USB every frame. On a 3X that is 7.47 MB against a 5 Gbps
link and it does not fit the 20 Hz budget. This runs the warp on QCOM instead
and sends the 0.39 MB warped crop, the same split jetlink uses, reusing
jetlink's warp_cache for the comma-side half.

mici is deliberately NOT included: its frame is 3.24 MB, already inside the
budget, and leaving it on upstream's fused path keeps it aligned with the
full-frame models upstream is holding the line for. A mici with a chestnut
behaves exactly as it did before this module existed.

Nothing here is imported unless the board is fitted AND the device is
tici/tizi; see `supported()`.
"""
from __future__ import annotations

SUPPORTED_DEVICES = ("tici", "tizi")


def supported() -> bool:
  """Should this device use the warp split? Cheap: one hardware query.

  False means every other path in the tree is the one that ran before: mici
  with or without a board, and any device with no board at all.
  """
  try:
    from openpilot.common.hardware import HARDWARE
    return HARDWARE.get_device_type() in SUPPORTED_DEVICES
  except Exception:
    return False


def prepare() -> bool:
  """Bring QCOM up before modeld goes realtime. Mirrors accelerators.prepare().

  tinygrad initialises the device on its first kernel and spawns threads doing
  it; started after config_realtime_process(7, 54) they inherit SCHED_FIFO 54
  on core 7 and preempt the frame loop. jetlink paid 5% of frames for this.
  """
  if not supported():
    return False
  from openpilot.sunnypilot.accelerators.jetlink import warp_cache
  warp_cache.init_device()
  return True


def make_model_state(cam_w: int, cam_h: int):
  from openpilot.sunnypilot.accelerators.chestnut.model_state import ChestnutModelState
  return ChestnutModelState(cam_w, cam_h)
