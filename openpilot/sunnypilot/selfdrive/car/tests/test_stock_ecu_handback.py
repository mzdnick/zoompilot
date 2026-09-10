"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
from opendbc.car import structs
from opendbc.sunnypilot.car.stock_ecu import StockEcuState, StockEcuStatus
from openpilot.sunnypilot.selfdrive.car.stock_ecu_handback import (HANDBACK_WAIT_T, REQUEST_KEY, RESULT_KEY,
                                                                   StockEcuHandBackGate, StockEcuHandBackServer)
from openpilot.sunnypilot.selfdrive.car.tests.fakes import FakeClock, FakeParams, answer

RESTORED, FAILED, NOT_NEEDED = StockEcuState.RESTORED, StockEcuState.FAILED, StockEcuState.NOT_NEEDED


def _gate(voluntary=True, **values):
  params = FakeParams(**values)
  clock = FakeClock()
  gate = StockEcuHandBackGate(params, voluntary=voluntary, now=clock)
  return gate, params, clock


class TestStockEcuHandBackGate:
  def test_offroad_is_always_ready_and_asks_nothing(self):
    gate, params, _ = _gate()
    assert gate.ready(started=False)
    assert params.get(REQUEST_KEY) is None

  def test_onroad_asks_once_then_waits_for_its_own_answer(self):
    gate, params, _ = _gate()
    assert not gate.ready(started=True)
    assert params.get(REQUEST_KEY) == {"id": 1}
    for _ in range(5):
      assert not gate.ready(started=True)
    assert params.get(REQUEST_KEY)["id"] == 1  # asked once
    answer(params, 7, RESTORED)  # somebody else's answer
    assert not gate.ready(started=True)
    answer(params, 1, RESTORED)
    assert gate.ready(started=True)
    assert not gate.pending

  def test_stale_answer_from_the_previous_request_cannot_satisfy_a_new_one(self):
    gate, params, _ = _gate()
    gate.ready(started=True)
    answer(params, 1, RESTORED)
    assert gate.ready(started=True)
    # the same consumer asks again later in the session: a fresh id, the old answer is not it
    assert not gate.ready(started=True)
    assert params.get(REQUEST_KEY)["id"] == 2
    assert not gate.ready(started=True)
    answer(params, 2, NOT_NEEDED)
    assert gate.ready(started=True)

  def test_second_consumer_joins_the_open_request(self):
    # a reboot pressed while forced offroad is already waiting: one hand-back, both proceed on it
    hw, params, _ = _gate()
    hw.ready(started=True)
    mgr = StockEcuHandBackGate(params, voluntary=False, now=FakeClock())
    assert not mgr.ready(started=True)
    assert params.get(REQUEST_KEY) == {"id": 1}
    answer(params, 1, RESTORED)
    assert hw.ready(started=True) and mgr.ready(started=True)

  def test_voluntary_stop_holds_on_failure_until_late_recovery(self):
    gate, params, clock = _gate(voluntary=True)
    gate.ready(started=True)
    answer(params, 1, FAILED)
    assert not gate.ready(started=True)
    assert gate.failed and gate.pending
    clock.t = HANDBACK_WAIT_T * 3  # the no-answer bound does not apply: card answered
    assert not gate.ready(started=True)
    answer(params, 1, RESTORED)
    assert gate.ready(started=True)

  def test_mandatory_stop_proceeds_on_failure(self):
    gate, params, _ = _gate(voluntary=False)
    gate.ready(started=True)
    answer(params, 1, FAILED)
    assert gate.ready(started=True)

  def test_no_answer_at_all_is_bounded(self):
    # no card alive to answer: nothing holds the ECU, so nothing is gained by waiting
    for voluntary in (True, False):
      gate, params, clock = _gate(voluntary=voluntary)
      assert not gate.ready(started=True)
      clock.t = HANDBACK_WAIT_T - 0.1
      assert not gate.ready(started=True)
      clock.t = HANDBACK_WAIT_T + 0.1
      assert gate.ready(started=True)

  def test_going_offroad_mid_wait_releases(self):
    gate, params, _ = _gate()
    assert not gate.ready(started=True)
    assert gate.ready(started=False)
    assert not gate.pending

  def test_withdrawal_removes_only_its_own_request(self):
    gate, params, _ = _gate()
    gate.ready(started=True)
    gate.reset()
    assert params.get(REQUEST_KEY) is None and not gate.pending and not gate.failed
    # a request opened by someone else stays
    gate.ready(started=True)
    params.put(REQUEST_KEY, {"id": 5})
    gate.reset()
    assert params.get(REQUEST_KEY)["id"] == 5

  def test_re_ask_after_withdrawal_gets_a_new_id(self):
    gate, params, _ = _gate()
    gate.ready(started=True)
    answer(params, 1, FAILED)
    gate.ready(started=True)
    gate.reset()
    assert not gate.ready(started=True)
    assert params.get(REQUEST_KEY)["id"] == 2
    assert not gate.ready(started=True)  # the failed answer for id 1 is not an answer for id 2


def _server(status=None, request_id=1):
  params = FakeParams()
  if request_id is not None:
    params.put(REQUEST_KEY, {"id": request_id})
  server = StockEcuHandBackServer(params, status)
  server.update_params()
  return server, params


def _step(server, enabled=False):
  cc_sp = structs.CarControlSP()
  server.update(enabled, cc_sp)
  return cc_sp


class TestStockEcuHandBackServer:
  def test_no_session_answers_not_needed_at_once(self):
    server, params = _server(status=None)
    cc_sp = _step(server)
    assert not cc_sp.stockEcuHandBack
    assert params.get(RESULT_KEY) == {"id": 1, "outcome": "notNeeded"}

  def test_nothing_requested_nothing_asserted(self):
    server, params = _server(status=StockEcuStatus(), request_id=None)
    for _ in range(10):
      assert not _step(server).stockEcuHandBack
    assert params.get(RESULT_KEY) is None

  def test_asserts_until_the_session_restores_then_answers(self):
    session = StockEcuStatus()
    server, params = _server(session)
    for _ in range(50):
      assert _step(server).stockEcuHandBack
      assert params.get(RESULT_KEY) is None
    session.handback_completed = True
    assert _step(server).stockEcuHandBack
    assert params.get(RESULT_KEY)["outcome"] == "restored"
    # the assert holds until the process exits
    for _ in range(10):
      assert _step(server).stockEcuHandBack

  def test_waits_for_disengagement_to_start_then_runs_to_the_end(self):
    session = StockEcuStatus()
    server, params = _server(session)
    for _ in range(20):
      assert not _step(server, enabled=True).stockEcuHandBack
    assert _step(server, enabled=False).stockEcuHandBack
    session.handback_completed = True
    assert _step(server, enabled=True).stockEcuHandBack
    assert params.get(RESULT_KEY)["outcome"] == "restored"

  def test_failure_then_late_recovery_are_two_answers(self):
    session = StockEcuStatus()
    server, params = _server(session)
    _step(server)
    session.handback_failed = True
    _step(server)
    assert params.get(RESULT_KEY) == {"id": 1, "outcome": "failed"}
    params.put(RESULT_KEY, None)  # a consumer would not clear it; make the re-answer observable
    for _ in range(5):
      _step(server)
    assert params.get(RESULT_KEY) is None  # answered once per outcome
    session.handback_failed = False
    session.handback_completed = True
    _step(server)
    assert params.get(RESULT_KEY)["outcome"] == "restored"

  def test_new_request_id_is_answered_again(self):
    session = StockEcuStatus()
    session.handback_completed = True
    server, params = _server(session)
    _step(server)
    assert params.get(RESULT_KEY)["id"] == 1
    params.put(REQUEST_KEY, {"id": 2})
    server.update_params()
    _step(server)
    assert params.get(RESULT_KEY)["id"] == 2

  def test_withdrawn_request_drops_the_assert(self):
    # the record gone is the withdrawal: the vehicle's session manager finishes any in-flight
    # restoration on its own and treats the next takeover as a first one
    session = StockEcuStatus()
    server, params = _server(session)
    assert _step(server).stockEcuHandBack
    params.remove(REQUEST_KEY)
    server.update_params()
    assert not _step(server).stockEcuHandBack
    assert not server.started
