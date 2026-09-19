"""
Copyright (c) 2026-, mzdnick.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

import json
import unittest
from unittest import mock

from openpilot.common.test import OpenpilotTestCase
from openpilot.cereal import custom
from openpilot.sunnypilot.captive_portal import manager as manager_module
from openpilot.sunnypilot.captive_portal.client import Connectivity, LoginResult, PortalClient, PortalError, ProbeResult
from openpilot.sunnypilot.captive_portal.manager import BACKOFF_START, PROBE_PERIOD, CaptivePortalManager

State = custom.CustomReserved10.State

PROFILE = {"host": "portal.example", "url": "http://portal.example/login",
           "fields": {"user": "guest", "password": "letmein"}}


def make_params(store: dict) -> mock.MagicMock:
  params = mock.MagicMock()
  params.get.side_effect = lambda key, **_: store.get(key)
  return params


class TestCaptivePortalManager(OpenpilotTestCase):

  def setUp(self):
    super().setUp()
    self.manager = CaptivePortalManager.__new__(CaptivePortalManager)
    self.manager.params = make_params({})
    self.manager.pm = mock.MagicMock()
    self.manager.ssid = ""
    self.manager.state = State.idle
    self.manager.message = ""
    self.manager.last_attempt = 0.0
    self.manager._last_probe = 0.0
    self.manager._next_attempt = 0.0
    self.manager._backoff = BACKOFF_START

    self.sent: list[tuple] = []
    self.manager.pm.send.side_effect = lambda _svc, msg: self.sent.append(
      (msg.customReserved10.detected, msg.customReserved10.state, msg.customReserved10.ssid, msg.customReserved10.message)
    )

  def set_wifi(self, ssid: str | None):
    self.mocker.patch.object(manager_module, "wifi_ssid", return_value=ssid)

  def make_client(self, probe_result: ProbeResult, login_result: LoginResult | Exception | None = None):
    self.manager.client = PortalClient()
    probe = self.mocker.patch.object(self.manager.client, "probe", return_value=probe_result)
    login = self.mocker.patch.object(self.manager.client, "login",
                                     side_effect=login_result if isinstance(login_result, Exception) else None,
                                     return_value=None if isinstance(login_result, Exception) else login_result)
    return probe, login

  def last(self) -> tuple:
    return self.sent[-1]

  def test_off_wifi_reports_idle(self):
    self.set_wifi(None)
    self.manager.tick(now=1000.0)
    self.assertEqual(self.last(), (False, State.idle, "", ""))

  def test_online_link_resets_backoff(self):
    self.set_wifi("HotelWiFi")
    self.manager._backoff = 99.0
    self.make_client(ProbeResult(Connectivity.ONLINE, None))
    self.manager.tick(now=1000.0)
    self.assertEqual(self.last()[1], State.online)
    self.assertEqual(self.manager._backoff, BACKOFF_START)

  def test_dead_link_reports_idle(self):
    self.set_wifi("HotelWiFi")
    self.make_client(ProbeResult(Connectivity.OFFLINE, None))
    self.manager.tick(now=1000.0)
    self.assertEqual(self.last()[1], State.idle)

  def test_portal_without_profile_reports_detection(self):
    self.set_wifi("HotelWiFi")
    self.make_client(ProbeResult(Connectivity.PORTAL, "http://portal.example/login"))
    self.manager.tick(now=1000.0)
    detected, state, ssid, message = self.last()
    self.assertTrue(detected)
    self.assertEqual(state, State.portal)
    self.assertEqual(ssid, "HotelWiFi")
    self.assertIn("no saved login", message)

  def test_invalid_profiles_json_is_treated_as_empty(self):
    self.set_wifi("HotelWiFi")
    self.manager.params = make_params({"CaptivePortalProfiles": b"{not json"})
    self.make_client(ProbeResult(Connectivity.PORTAL, "http://portal.example/login"))
    self.manager.tick(now=1000.0)
    self.assertEqual(self.last()[1], State.portal)

  def test_auto_login_success(self):
    self.set_wifi("HotelWiFi")
    self.manager.params = make_params({"CaptivePortalProfiles": json.dumps({"HotelWiFi": PROFILE}).encode()})
    _probe, login = self.make_client(ProbeResult(Connectivity.PORTAL, "http://portal.example/login"),
                                     LoginResult(True, "logged in"))
    self.manager.tick(now=1000.0)
    states = [s[1] for s in self.sent]
    self.assertEqual(states[-2], State.loggingIn)
    self.assertEqual(states[-1], State.online)
    login.assert_called_once_with(PROFILE)
    self.assertEqual(self.manager.last_attempt, 1000.0)
    self.assertEqual(self.manager._next_attempt, 0.0)

  def test_auto_login_failure_backs_off(self):
    self.set_wifi("HotelWiFi")
    self.manager.params = make_params({"CaptivePortalProfiles": json.dumps({"HotelWiFi": PROFILE}).encode()})
    _probe, login = self.make_client(ProbeResult(Connectivity.PORTAL, "http://portal.example/login"),
                                     LoginResult(False, "bad password"))
    self.manager.tick(now=1000.0)
    detected, state, _ssid, message = self.last()
    self.assertTrue(detected)
    self.assertEqual(state, State.loginFailed)
    self.assertEqual(message, "bad password")
    self.assertEqual(self.manager._next_attempt, 1000.0 + BACKOFF_START)
    self.assertEqual(self.manager._backoff, BACKOFF_START * 2)

    # an immediate tick stays on the cached failure: the probe period has not elapsed
    self.manager.tick(now=1000.5)
    self.assertEqual(self.last()[1], State.loginFailed)

    # the next probe shows the portal again, but the retry backoff holds the login off
    self.manager.tick(now=1000.0 + PROBE_PERIOD + 1)
    detected, state, _ssid, message = self.last()
    self.assertEqual(state, State.portal)
    self.assertIn("retrying", message)
    self.assertEqual(login.call_count, 1)

  def test_probe_throttled_within_period(self):
    self.set_wifi("HotelWiFi")
    probe, _login = self.make_client(ProbeResult(Connectivity.ONLINE, None))
    self.manager.tick(now=1000.0)
    self.manager.tick(now=1000.0 + PROBE_PERIOD - 1)
    self.assertEqual(probe.call_count, 1)

  def test_login_error_counts_as_failure(self):
    self.set_wifi("HotelWiFi")
    self.manager.params = make_params({"CaptivePortalProfiles": json.dumps({"HotelWiFi": PROFILE}).encode()})
    self.make_client(ProbeResult(Connectivity.PORTAL, "http://portal.example/login"),
                     PortalError("portal page unreachable: boom"))
    self.manager.tick(now=1000.0)
    self.assertEqual(self.last()[1], State.loginFailed)
    self.assertIn("unreachable", self.last()[3])


if __name__ == "__main__":
  unittest.main()
