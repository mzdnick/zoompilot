"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

Switched off with a lift on, the lift goes out at its own rate instead of stepping off the
accel request.
"""
import pytest

from openpilot.common.realtime import DT_MDL as DT
from openpilot.sunnypilot.selfdrive.controls.lib.e2e_lead_gap import controller as c
from openpilot.sunnypilot.selfdrive.controls.lib.e2e_lead_gap.controller import Inhibit
from openpilot.sunnypilot.selfdrive.controls.lib.e2e_lead_gap.tests.helpers import build_sm
from openpilot.sunnypilot.selfdrive.controls.lib.e2e_lead_gap.tests.test_controller import FAR, V, run


def test_switch_off_lets_the_lift_out_at_boost_rise():
  sm = build_sm(V, FAR)
  ctl, _ = run(sm)
  boost = ctl.boost
  assert boost > 0.
  ctl.enabled = False  # the params pass lands within PARAMS_UPDATE_PERIOD
  _, out = run(sm, frames=1, ctl=ctl)
  assert out == pytest.approx(boost - c.BOOST_RISE * DT)
  assert ctl.inhibit == Inhibit.disabled
  run(sm, frames=round(boost / (c.BOOST_RISE * DT)) + 2, ctl=ctl)
  assert ctl.boost == 0.
  _, out = run(sm, frames=1, ctl=ctl, a_model=0.3)
  assert out == 0.3


def test_a_binding_mpc_still_cuts_it_at_once():
  sm = build_sm(V, FAR)
  ctl, _ = run(sm)
  ctl.enabled = False
  _, out = run(sm, frames=1, ctl=ctl, a_mpc=0.)
  assert out == 0.
