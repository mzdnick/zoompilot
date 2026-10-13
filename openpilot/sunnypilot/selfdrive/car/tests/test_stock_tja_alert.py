"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

The camera's own traffic jam assist reaching its steering states (0x440, state 3+) is the
sub-second window before the stock-TJA clear completes and cruise falls with it; the alert
names the recovery. Route 0000013b: the same state ran 57 s blind because nothing watched it.
"""
import pytest

from openpilot.cereal import custom, messaging
from opendbc.car import structs
from openpilot.selfdrive.selfdrived.events import Events
from openpilot.sunnypilot.selfdrive.car.car_specific import CarSpecificEventsSP
from openpilot.sunnypilot.selfdrive.selfdrived.events import EVENTS_SP
from openpilot.sunnypilot.selfdrive.selfdrived.events_base import ET, AudibleAlert

EventNameSP = custom.OnroadEventSP.EventName


def _car_events(brand: str = 'mazda') -> CarSpecificEventsSP:
  CP = structs.CarParams()
  CP.brand = brand
  return CarSpecificEventsSP(CP, structs.CarParamsSP())


def _update(car_events: CarSpecificEventsSP, stock_tja: int):
  CS = structs.CarState()
  CS_SP = messaging.new_message('carStateSP').carStateSP
  CS_SP.zoompilot.stockTja = stock_tja
  return car_events.update(CS, Events(), CS_SP)


class TestStockTjaSteering:

  @pytest.mark.parametrize("state", [3, 4, 5])
  def test_steering_states_raise_it(self, state):
    assert _update(_car_events(), state).has(EventNameSP.stockTjaSteering)

  @pytest.mark.parametrize("state", [0, 1, 2])
  def test_off_and_merely_armed_do_not(self, state):
    # 2 is "armed": the clear rule owns that window before the camera ever steers
    assert not _update(_car_events(), state).has(EventNameSP.stockTjaSteering)

  def test_mazda_only(self):
    assert not _update(_car_events(brand='honda'), 3).has(EventNameSP.stockTjaSteering)

  def test_alert_names_the_recovery_and_stays_silent(self):
    alert = EVENTS_SP[EventNameSP.stockTjaSteering][ET.PERMANENT]
    assert alert.audible_alert == AudibleAlert.none
    assert alert.alert_text_1 == "Stock Traffic Jam Steering"
    assert "MAIN" in alert.alert_text_2 and "SET" in alert.alert_text_2
