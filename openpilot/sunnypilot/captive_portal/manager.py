"""
Copyright (c) 2026-, mzdnick.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

import json
import time

from openpilot.common.params import Params
from openpilot.common.realtime import Ratekeeper
from openpilot.common.swaglog import cloudlog
from openpilot.cereal import custom, messaging

from openpilot.sunnypilot.captive_portal.client import Connectivity, LoginResult, PortalClient, PortalError

# the service rides an unused sunnypilot reserved slot; see the comment on the struct in custom.capnp
SERVICE = "customReserved10"
State = custom.CustomReserved10.State

PROBE_PERIOD = 10.0  # seconds between probes on a live link
BACKOFF_START = 30.0  # seconds before the first auto-login retry
BACKOFF_MAX = 300.0


def wifi_ssid() -> str | None:
  # the wpa_supplicant control socket exists only on comma devices; every other
  # host (CI, PC) lands in the except branch and reports "not on wifi"
  try:
    from openpilot.common.hardware.comma.hardware import wpa_supplicant_cmd
    return wpa_supplicant_cmd("STATUS").get("ssid") or None
  except Exception:
    return None


def load_profiles(params: Params) -> dict:
  raw = params.get("CaptivePortalProfiles")
  if not raw:
    return {}
  try:
    profiles = json.loads(raw)
    return profiles if isinstance(profiles, dict) else {}
  except ValueError:
    cloudlog.exception("captiveportald: CaptivePortalProfiles is not valid json")
    return {}


class CaptivePortalManager:
  """Detects wifi captive portals, auto-logs-in from a stored profile, and reports status."""

  def __init__(self):
    self.params = Params()
    self.pm = messaging.PubMaster([SERVICE])
    self.client = PortalClient()
    self.ssid = ""
    self.state = State.idle
    self.message = ""
    self.last_attempt = 0.0
    self._last_probe = 0.0
    self._next_attempt = 0.0
    self._backoff = BACKOFF_START

  def _report(self, state, message: str = ""):
    self.state, self.message = state, message
    msg = messaging.new_message(SERVICE, valid=True)
    m = msg.customReserved10
    m.detected = state in (State.portal, State.loggingIn, State.loginFailed)
    m.state = state
    m.ssid = self.ssid
    m.message = message
    m.lastAttempt = self.last_attempt
    self.pm.send(SERVICE, msg)

  def tick(self, now: float | None = None):
    now = time.monotonic() if now is None else now
    self.ssid = wifi_ssid() or ""
    if not self.ssid:
      self._report(State.idle)
      return

    if now - self._last_probe < PROBE_PERIOD:
      self._report(self.state, self.message)
      return
    self._last_probe = now

    probe = self.client.probe()
    if probe.state == Connectivity.ONLINE:
      self._backoff = BACKOFF_START
      self._next_attempt = 0.0
      self._report(State.online)
      return
    if probe.state == Connectivity.OFFLINE:
      self._report(State.idle)
      return

    profile = load_profiles(self.params).get(self.ssid)
    if profile is None:
      self._report(State.portal, "portal detected: no saved login for this network")
      return
    if now < self._next_attempt:
      self._report(State.portal, f"retrying login in {int(self._next_attempt - now)} s")
      return

    self.last_attempt = now
    self._report(State.loggingIn)
    try:
      result: LoginResult = self.client.login(profile)
    except PortalError as e:
      result = LoginResult(False, str(e))

    if result.success:
      self._backoff = BACKOFF_START
      self._next_attempt = 0.0
      cloudlog.info(f"captiveportald: logged in to '{self.ssid}'")
      self._report(State.online)
    else:
      self._next_attempt = now + self._backoff
      self._backoff = min(self._backoff * 2, BACKOFF_MAX)
      cloudlog.warning(f"captiveportald: login to '{self.ssid}' failed: {result.message}")
      self._report(State.loginFailed, result.message)

  def main_thread(self):
    rk = Ratekeeper(1, print_delay_threshold=None)
    while True:
      try:
        self.tick()
      except Exception:
        cloudlog.exception("captiveportald: tick failed")
      rk.keep_time()


def main():
  CaptivePortalManager().main_thread()


if __name__ == "__main__":
  main()
