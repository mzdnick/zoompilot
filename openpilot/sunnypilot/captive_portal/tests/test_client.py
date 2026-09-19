"""
Copyright (c) 2026-, mzdnick.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

import contextlib
import http.server
import socket
import threading
import unittest
from typing import Any
from urllib.parse import parse_qs

from openpilot.sunnypilot.captive_portal.client import Connectivity, PortalClient, PortalError, Probe
from openpilot.sunnypilot.captive_portal.forms import FormField, LoginForm

TOKEN = "tok-123"

FORM_HTML = f"""<html><body>
<form action="/login" method="post">
<input type="hidden" name="token" value="{TOKEN}">
<input type="text" name="user">
<input type="password" name="password">
<input type="submit" name="go" value="Sign in">
</form></body></html>"""


# local copy of selfdrive.test.helpers.http_server_context so this file also runs on a
# host without the built cereal/params stack
@contextlib.contextmanager
def http_server_context(handler):
  server = http.server.HTTPServer(('127.0.0.1', 0), handler)
  t = threading.Thread(target=server.serve_forever, daemon=True)
  t.start()
  try:
    yield f"http://127.0.0.1:{server.server_port}"
  finally:
    server.shutdown()
    server.server_close()
    t.join()


class PortalHandler(http.server.BaseHTTPRequestHandler):
  """Hotel-style portal: probes redirect to /login until the form posts good credentials."""
  authed = False
  submitted: dict[str, str] = {}

  def log_message(self, format: str, *args: Any) -> None:  # noqa: A002
    pass

  def _redirect_login(self):
    self.send_response(302)
    self.send_header("Location", "/login")
    self.end_headers()

  def _body(self, body: str, status: int = 200):
    data = body.encode()
    self.send_response(status)
    self.send_header("Content-Length", str(len(data)))
    self.end_headers()
    self.wfile.write(data)

  def do_GET(self):
    if type(self).authed:
      if self.path.startswith("/generate_204"):
        self.send_response(204)
        self.end_headers()
      else:
        self._body("<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>")
      return
    if self.path.startswith("/login"):
      self._body(FORM_HTML)
    else:
      self._redirect_login()

  def do_POST(self):
    form = {k: v[0] for k, v in parse_qs(self.rfile.read(int(self.headers["Content-Length"])).decode()).items()}
    type(self).submitted = form
    if form.get("token") == TOKEN and form.get("user") == "guest" and form.get("password") == "letmein":
      type(self).authed = True
      self._redirect_login()
    else:
      self._body(FORM_HTML)


class PlainHandler(http.server.BaseHTTPRequestHandler):
  def log_message(self, format: str, *args: Any) -> None:  # noqa: A002
    pass

  def do_GET(self):
    self.send_response(200)
    self.send_header("Content-Length", "26")
    self.end_headers()
    self.wfile.write(b"<html><body>js only</body>")


def free_port() -> int:
  # a bound-then-released port so connection attempts are refused at once
  with socket.socket() as s:
    s.bind(("127.0.0.1", 0))
    return s.getsockname()[1]


class TestPortalClient(unittest.TestCase):

  def setUp(self):
    super().setUp()
    PortalHandler.authed = False
    PortalHandler.submitted = {}

  def make_client(self, base: str) -> PortalClient:
    return PortalClient(probes=(Probe(f"{base}/generate_204", "204"), Probe(f"{base}/detect", "success")))

  def test_probe_before_and_after_auth(self):
    with http_server_context(PortalHandler) as base:
      client = self.make_client(base)
      result = client.probe()
      self.assertIs(result.state, Connectivity.PORTAL)
      self.assertEqual(result.portal_url, f"{base}/login")

      PortalHandler.authed = True
      self.assertIs(client.probe().state, Connectivity.ONLINE)

  def test_login_success_sends_hidden_and_submit_fields(self):
    with http_server_context(PortalHandler) as base:
      client = self.make_client(base)
      profile = {"host": "127.0.0.1", "url": f"{base}/login",
                 "fields": {"user": "guest", "password": "letmein"}}
      result = client.login(profile)
      self.assertTrue(result.success, result.message)
      self.assertEqual(PortalHandler.submitted,
                       {"token": TOKEN, "user": "guest", "password": "letmein", "go": "Sign in"})
      self.assertIs(client.probe().state, Connectivity.ONLINE)

  def test_login_wrong_password_stays_behind_portal(self):
    with http_server_context(PortalHandler) as base:
      client = self.make_client(base)
      profile = {"host": "127.0.0.1", "url": f"{base}/login",
                 "fields": {"user": "guest", "password": "wrong"}}
      result = client.login(profile)
      self.assertFalse(result.success)
      self.assertIn("still behind", result.message)

  def test_login_blocked_when_portal_moves_off_pinned_host(self):
    with http_server_context(PortalHandler) as base:
      client = self.make_client(base)
      profile = {"host": "evil.example", "url": f"{base}/login",
                 "fields": {"user": "guest", "password": "letmein"}}
      with self.assertRaises(PortalError) as ctx:
        client.login(profile)
      self.assertIn("evil.example", str(ctx.exception))
      # nothing was posted: the host check fires before submit
      self.assertEqual(PortalHandler.submitted, {})

  def test_fetch_form_raises_without_form(self):
    with http_server_context(PlainHandler) as base:
      client = self.make_client(base)
      with self.assertRaises(PortalError):
        client.fetch_form(base + "/")

  def test_probe_offline_when_nothing_answers(self):
    client = PortalClient(probes=(Probe(f"http://127.0.0.1:{free_port()}/generate_204", "204"),))
    self.assertIs(client.probe().state, Connectivity.OFFLINE)

  def test_submit_uses_get_params_for_get_form(self):
    with http_server_context(PortalHandler) as base:
      client = self.make_client(base)
      form = LoginForm(action=base + "/login", method="get",
                       fields=(FormField("token", "hidden", TOKEN),))
      response = client.submit(form, {})
      self.assertEqual(response.status_code, 200)


if __name__ == "__main__":
  unittest.main()
