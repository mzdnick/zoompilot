"""
Copyright (c) 2026-, zoompilot and a number of other contributors.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

from openpilot.cereal import custom
from opendbc.car import structs
from openpilot.selfdrive.selfdrived.events import Events
from openpilot.sunnypilot.selfdrive.selfdrived.events import EventsSP
from openpilot.sunnypilot.mads.mads import ModularAssistiveDrivingSystem
from openpilot.common.test import OpenpilotTestCase

EventNameSP = custom.OnroadEventSP.EventName


def make_car_state(available, can_valid=True):
  cs = structs.CarState()
  cs.cruiseState.available = available
  cs.canValid = can_valid
  return cs


def make_mads(mocker, brand):
  sd = mocker.MagicMock()
  sd.CP = structs.CarParams()
  sd.CP.brand = brand
  sd.CP_SP = structs.CarParamsSP()
  sd.params = mocker.MagicMock()
  sd.params.get_bool = mocker.MagicMock(side_effect=lambda k: {
    "Mads": True, "MadsMainCruiseAllowed": True,
    "DisengageOnAccelerator": True, "MadsUnifiedEngagementMode": True,
  }.get(k, False))
  sd.events = Events()
  sd.events_sp = EventsSP()
  sd.enabled = False
  sd.enabled_prev = False
  sd.initialized = True
  sd.CS_prev = structs.CarState()
  ps = mocker.MagicMock()
  ps.controlsAllowedLateral = True
  ps.safetyModel = structs.CarParams.SafetyModel.mazda
  sd.sm = {'pandaStates': [ps]}
  sd.state_machine = mocker.MagicMock()

  mads = ModularAssistiveDrivingSystem(sd)
  mads.enabled_toggle = True
  return mads, sd


class TestMainCruiseBootArm(OpenpilotTestCase):
  """Mazda's CRZ_AVAILABLE is the armed-cruise level and can already be on when openpilot
  starts, so an empty CS_prev makes it read as a rising edge and lateral engages with no
  user action in that drive."""

  def _run(self, brand, samples):
    mocker = self._fixture("mocker")
    mads, sd = make_mads(mocker, brand)
    for available, can_valid in samples:
      mads.update_events(make_car_state(available, can_valid))
    return sd.events_sp.has(EventNameSP.lkasEnable)

  def test_boot_with_main_armed_does_not_enable(self):
    assert not self._run("mazda", [(True, True)])

  def test_boot_absorb_releases_after_main_seen_off(self):
    assert self._run("mazda", [(True, True), (False, True), (True, True)])

  def test_mid_drive_press_still_enables(self):
    assert self._run("mazda", [(False, True), (True, True)])

  def test_invalid_can_does_not_latch_off(self):
    assert not self._run("mazda", [(False, False), (True, True)])

  def test_other_brands_keep_the_boot_arm(self):
    assert self._run("toyota", [(True, True)])
