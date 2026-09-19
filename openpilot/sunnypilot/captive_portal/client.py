"""
Copyright (c) 2026-, mzdnick.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

import time
from dataclasses import dataclass
from enum import Enum
from urllib.parse import urljoin, urlparse

import requests

from openpilot.sunnypilot.captive_portal.forms import NON_USER_INPUT_TYPES, LoginForm, best_form, parse_forms

# (connect, read) seconds. A dead link must not stall the daemon tick
PROBE_TIMEOUT = (2.0, 2.0)
PAGE_TIMEOUT = (2.0, 10.0)
MAX_FORM_PAGES = 3


class Connectivity(Enum):
  ONLINE = 0
  PORTAL = 1
  OFFLINE = 2


@dataclass(frozen=True)
class ProbeResult:
  state: Connectivity
  portal_url: str | None  # first hijack redirect target, when seen


@dataclass(frozen=True)
class LoginResult:
  success: bool
  message: str


@dataclass(frozen=True)
class Probe:
  url: str
  expect: str  # "204": empty no-content reply; "success": 200 page whose body says Success


# plain HTTP on purpose: a portal can only hijack the probe when it can intercept and
# answer it, which is exactly the condition being tested for
DEFAULT_PROBES = (
  Probe("http://connectivitycheck.gstatic.com/generate_204", "204"),
  Probe("http://captive.apple.com/hotspot-detect.html", "success"),
)


class PortalError(Exception):
  pass


class PortalClient:
  def __init__(self, probes: tuple[Probe, ...] = DEFAULT_PROBES, session: requests.Session | None = None):
    self.probes = probes
    self.session = session if session is not None else requests.Session()

  def probe(self) -> ProbeResult:
    portal_url = None
    got_response = False
    for p in self.probes:
      try:
        r = self.session.get(p.url, allow_redirects=False, timeout=PROBE_TIMEOUT)
      except requests.RequestException:
        continue
      got_response = True
      if p.expect == "204":
        if r.status_code == 204 and "Location" not in r.headers:
          return ProbeResult(Connectivity.ONLINE, None)
      else:
        if r.status_code == 200 and "Success" in r.text:
          return ProbeResult(Connectivity.ONLINE, None)
      if portal_url is None:
        portal_url = urljoin(p.url, r.headers.get("Location", "")) or p.url
    if not got_response:
      return ProbeResult(Connectivity.OFFLINE, None)
    return ProbeResult(Connectivity.PORTAL, portal_url)

  def fetch_form(self, url: str) -> tuple[LoginForm, str]:
    """Follow redirects to the form page. Returns (form, final page URL)."""
    try:
      r = self.session.get(url, allow_redirects=True, timeout=PAGE_TIMEOUT)
    except requests.RequestException as e:
      raise PortalError(f"portal page unreachable: {e}") from e
    form = best_form(parse_forms(r.text, r.url))
    if form is None:
      raise PortalError("no HTML form on the portal page (javascript portal?)")
    return form, r.url

  def submit(self, form: LoginForm, user_values: dict[str, str]) -> requests.Response:
    data = {}
    submit_sent = False  # a browser sends only the button that was clicked; one is enough
    for f in form.fields:
      if not f.name:
        continue
      if f.type == "submit":
        if not submit_sent:
          data[f.name] = f.value
          submit_sent = True
      elif f.type in NON_USER_INPUT_TYPES:
        data[f.name] = f.value
      elif f.type == "checkbox":
        if user_values.get(f.name):
          data[f.name] = f.value or "on"
      else:
        data[f.name] = user_values.get(f.name, f.value)
    kwargs = {"params": data} if form.method == "get" else {"data": data}
    try:
      return self.session.request(form.method, form.action, allow_redirects=True, timeout=PAGE_TIMEOUT, **kwargs)
    except requests.RequestException as e:
      raise PortalError(f"login request failed: {e}") from e

  def login(self, profile: dict, max_pages: int = MAX_FORM_PAGES) -> LoginResult:
    """Auto-login against the form flow captured in the profile.

    The profile pins the host its credentials were saved on; a portal that moves the
    form to a different host fails with PortalError instead of leaking them.
    """
    url = profile.get("url")
    if not url:
      return LoginResult(False, "profile has no portal url")
    pinned_host = profile.get("host")
    fields = profile.get("fields", {})
    for page in range(max_pages):
      form, _final_url = self.fetch_form(url)
      if pinned_host is not None and (urlparse(form.action).hostname or "") != pinned_host:
        raise PortalError(f"portal moved to {urlparse(form.action).hostname}, profile pins {pinned_host}")
      # only the first page may demand values the profile does not carry; later pages
      # (accept-terms style) fall back to the field defaults the page itself provides
      if page == 0:
        missing = [f.name for f in form.user_fields
                   if f.type in ("text", "email", "password") and not fields.get(f.name) and not f.value]
        if missing:
          return LoginResult(False, f"fields missing from profile: {', '.join(missing)}")
      try:
        r = self.submit(form, fields)
      except PortalError as e:
        return LoginResult(False, str(e))
      probe = self.probe()
      if probe.state == Connectivity.ONLINE:
        return LoginResult(True, "logged in")
      if probe.state == Connectivity.OFFLINE:
        return LoginResult(False, "network dropped during login")
      url = probe.portal_url or r.url
    return LoginResult(False, f"still behind the portal after {max_pages} pages")


def make_profile(ssid: str, form: LoginForm, user_values: dict[str, str]) -> dict:
  """Build the profile stored per SSID after a successful interactive login."""
  return {
    "host": urlparse(form.action).hostname or "",
    "url": form.action,
    "fields": {f.name: user_values[f.name] for f in form.user_fields if f.name in user_values},
    "savedAt": time.time(),  # noqa: TID251  # informational timestamp in the saved profile
    "ssid": ssid,
  }
