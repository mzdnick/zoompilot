"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
import pytest

from openpilot.selfdrive.ui.sunnypilot.longitudinal_status import alpha_long_status


@pytest.mark.parametrize("stock_ecu,text", [
  (None, "initializing"),          # card not publishing yet
  ("starting", "initializing"),
  ("parkToTakeOver", "initializing"),  # the reason reaches the driver on a SET press, as an alert
  ("stockCruiseOn", "initializing"),
  ("restoring", "initializing"),
  ("restored", "initializing"),
  ("ready", "ready"),
  ("failed", "failed"),
])
def test_onroad_alpha_line(stock_ecu, text):
  assert alpha_long_status(True, True, stock_ecu) == text


def test_blank_offroad_and_under_stock_cruise():
  # offroad the switch is the saved preference and speaks for itself; under stock longitudinal
  # there is no takeover to report
  assert alpha_long_status(False, True, "ready") == ""
  assert alpha_long_status(True, False, "ready") == ""
