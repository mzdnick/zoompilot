"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

The chestnut board with the frame reduced before it crosses USB, tici/tizi only.

mici is excluded by device type, board or no board: its frame is already inside
the link budget, and leaving it on upstream's path keeps it aligned with the
full-frame models upstream is holding the line for. Unlike a warp split this
keeps the whole field of view and gives up resolution instead, so a model that
wants the whole frame still gets the whole frame.

Nothing heavy at module level: modeld imports this at module scope.
"""
from __future__ import annotations

SUPPORTED_DEVICES = ("tici", "tizi")


def supported() -> bool:
  """Should this device reduce frames for its board? One hardware query.

  False means every other path in the tree is the one that ran before: mici with
  or without a board, and any device with no board at all.
  """
  try:
    from openpilot.common.hardware import HARDWARE
    return HARDWARE.get_device_type() in SUPPORTED_DEVICES
  except Exception:
    return False


def make_model_state(cam_w: int, cam_h: int):
  from openpilot.sunnypilot.accelerators.chestnut.model_state import ChestnutModelState
  return ChestnutModelState(cam_w, cam_h)
