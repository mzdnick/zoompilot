"""A button press is a one-frame event, so a no-entry coinciding with it (the camera's
LKAS re-enable can blip a fault bit) used to eat the lateral request entirely: lateral
never came back without a full re-engage. The request is now re-offered for a short
window (LKAS_BUTTON_REQUEST_FRAMES); the machine still engages only once no no-entry
remains."""

from types import SimpleNamespace
from unittest import mock

from opendbc.car import structs
from openpilot.cereal import custom, log
from openpilot.sunnypilot.mads.mads import ModularAssistiveDrivingSystem
from openpilot.selfdrive.selfdrived.events import Events
from openpilot.sunnypilot.selfdrive.selfdrived.events import EventsSP

State = custom.ModularAssistiveDrivingSystem.ModularAssistiveDrivingSystemState
EventNameSP = custom.OnroadEventSP.EventName
EventName = log.OnroadEvent.EventName
ButtonType = structs.CarState.ButtonEvent.Type


class TestLkasButtonRetry:
  def _mads(self):
    events, events_sp = Events(), EventsSP()
    sm = mock.MagicMock()
    sm.__getitem__.return_value = []
    selfdrive = SimpleNamespace(
      CP=SimpleNamespace(brand="mazda", flags=0, passive=False),
      CP_SP=SimpleNamespace(), params=mock.MagicMock(),
      events=events, events_sp=events_sp, enabled=True, enabled_prev=True,
      initialized=True, sm=sm,
      CS_prev=SimpleNamespace(cruiseState=SimpleNamespace(available=True), gasPressed=False),
      state_machine=SimpleNamespace(soft_disable_timer=30, current_alert_types=[]))
    mads = ModularAssistiveDrivingSystem(selfdrive)
    mads.enabled_toggle = True
    return mads, events, events_sp

  def _frame(self, mads, events, events_sp, press, no_entry):
    # the real cycle rebuilds both event sets every frame
    events.clear()
    events_sp.clear()
    if no_entry:
      events.add(EventName.steerUnavailable)
    buttonEvents = [SimpleNamespace(type=ButtonType.lkas, pressed=True)] if press else []
    mads.update(SimpleNamespace(buttonEvents=buttonEvents,
                                cruiseState=SimpleNamespace(available=True),
                                brakePressed=False, regenBraking=False, gasPressed=False,
                                gearShifter=structs.CarState.GearShifter.drive))

  def test_press_survives_a_transient_no_entry(self):
    mads, events, events_sp = self._mads()
    self._frame(mads, events, events_sp, press=True, no_entry=True)
    assert mads.state_machine.state == State.disabled
    assert events_sp.has(EventNameSP.lkasEnable)

    # blocker outlives the press, request stays alive
    self._frame(mads, events, events_sp, press=False, no_entry=True)
    assert mads.state_machine.state == State.disabled
    assert events_sp.has(EventNameSP.lkasEnable)

    # blocker clears with the press long gone: engages
    self._frame(mads, events, events_sp, press=False, no_entry=False)
    assert mads.state_machine.state == State.enabled
    assert mads.enabled

  def test_the_request_window_expires(self):
    mads, events, events_sp = self._mads()
    self._frame(mads, events, events_sp, press=True, no_entry=True)
    for _ in range(80):
      self._frame(mads, events, events_sp, press=False, no_entry=True)
    assert mads.state_machine.state == State.disabled
    # the window is gone and so is the re-offer
    assert mads.lkas_button_request_frames == 0
    assert not events_sp.has(EventNameSP.lkasEnable)
