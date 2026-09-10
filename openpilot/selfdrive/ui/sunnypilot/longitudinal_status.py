"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

The line under the alpha longitudinal switch, the same on both device families. The switch is
the saved preference and is only editable offroad (forced offroad included), so onroad the
line says what the running session's stock ECU takeover is doing: initializing, ready or
failed. "Ready" never means engaged. The reason behind "initializing" reaches the driver as an
onroad alert when they press SET (selfdrived, car_specific.py), not here.
"""
from openpilot.system.ui.lib.multilang import tr

# capnp names of CarStateZP.StockEcuState
_READY = ("ready",)
_FAILED = ("failed",)


def alpha_long_status(started: bool, alpha_applied: bool, stock_ecu: str | None) -> str:
  """stock_ecu is carStateSP.zoompilot.stockEcu's name while card publishes, else None."""
  if not started or not alpha_applied:
    return ""
  if stock_ecu in _READY:
    return tr("ready")
  if stock_ecu in _FAILED:
    return tr("failed")
  return tr("initializing")
