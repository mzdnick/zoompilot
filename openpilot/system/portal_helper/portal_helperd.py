#!/usr/bin/env python3
# Captive portal helper. The device has no browser, so a Wi-Fi login page can
# never be completed on-device. This daemon detects the portal with 204 probes
# and relays a phone's traffic through the device, so the login completes for
# the device's MAC.

import html
import http.client
import json
import os
import socket
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from urllib.parse import urlsplit

PORT = 8090
LISTEN_HOST = "0.0.0.0"

STATE_OK = "ok"
STATE_PORTAL = "portal"
STATE_OFFLINE = "offline"

PROBE_URLS = (
  ("http://connectivitycheck.gstatic.com/generate_204", None),
  ("http://www.msftconnecttest.com/connecttest.txt", "Microsoft Connect Test"),
)
PROBE_TIMEOUT = 4.0

POLL_PERIOD_PORTAL = 15.0
POLL_PERIOD_OK = 60.0
POLL_PERIOD_IDLE = 5.0

# The proxy must not outlive the portal it exists to solve
CONNECT_ALLOWED_PORTS = (80, 443)
MAX_RELAY_BYTES = 2 * 1024 * 1024

HOP_HEADERS = {"connection", "keep-alive", "proxy-authenticate", "proxy-authorization",
               "proxy-connection", "te", "trailer", "transfer-encoding", "upgrade", "host"}


def _probe(url: str) -> tuple[int, dict[str, str], bytes]:
  u = urlsplit(url)
  conn = http.client.HTTPConnection(u.hostname, u.port or 80, timeout=PROBE_TIMEOUT)
  try:
    conn.request("GET", u.path or "/", headers={"User-Agent": "openpilot-portal-check"})
    resp = conn.getresponse()
    return resp.status, dict(resp.getheaders()), resp.read(4096)
  finally:
    conn.close()


def check_internet() -> tuple[str, str]:
  """Classify the current connection. Returns (state, portal URL)."""
  for url, expected_body in PROBE_URLS:
    try:
      status, headers, body = _probe(url)
    except (OSError, http.client.HTTPException):
      continue

    if status == 204 or (expected_body is not None and expected_body.encode() in body):
      return STATE_OK, ""
    if 200 <= status < 400:
      location = next((v for k, v in headers.items() if k.lower() == "location"), url)
      return STATE_PORTAL, location
  return STATE_OFFLINE, ""


def default_route_is_wifi() -> bool:
  try:
    with open("/proc/net/route") as f:
      for line in f.readlines()[1:]:
        cols = line.split()
        if cols[1] == "00000000" and int(cols[3], 16) & 0x1 and cols[0].startswith("wlan"):
          return True
  except (OSError, ValueError, IndexError):
    pass
  return False


def wlan_ipv4() -> str:
  # UDP connect only picks the route, no traffic is sent
  try:
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
      sock.connect(("192.0.2.1", 80))
      return sock.getsockname()[0]
  except OSError:
    return ""


def current_ssid() -> str:
  try:
    with socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM) as sock:
      sock.settimeout(0.2)
      sock.bind(f"\0portal-helper-{os.getpid()}")
      sock.connect("/run/wpa_supplicant/wlan0")
      sock.send(b"STATUS")
      out = sock.recv(8192).decode("utf-8", "replace")
    return next((line.split("=", 1)[1] for line in out.splitlines()
                 if line.startswith("ssid=") and not line.startswith("<")), "")
  except (OSError, ValueError):
    return ""


class HelperState:
  def __init__(self, port: int):
    self._lock = threading.Lock()
    self._state = {"state": STATE_OFFLINE, "url": "", "ssid": "", "ip": "", "port": port}

  def get(self) -> dict[str, Any]:
    with self._lock:
      return dict(self._state)

  def update(self, **changes: Any) -> bool:
    with self._lock:
      changed = any(k in self._state and self._state[k] != v for k, v in changes.items())
      if changed:
        self._state.update({k: v for k, v in changes.items() if k in self._state})
      return changed


def _status_page_html(s: dict[str, Any]) -> str:
  ssid = html.escape(s.get("ssid", "") or "Wi-Fi")
  ip = s.get("ip", "")
  port = s.get("port", PORT)
  proxy = f"{ip}:{port}" if ip else f"&lt;this device's IP&gt;:{port}"

  if s.get("state") == STATE_OK:
    status = "<p class='ok'>This device is online.</p><p>You can remove the HTTP proxy from your phone.</p>"
  elif s.get("state") == STATE_PORTAL:
    status = f"""
<p>This Wi-Fi network (<b>{ssid}</b>) needs a login page.</p>
<p>Complete the login from your phone:</p>
<ol>
<li>Join this same Wi-Fi network on the phone</li>
<li>Set the phone's HTTP proxy to <b>{proxy}</b></li>
<li>Open any <b>http://</b> website and log in</li>
<li>Remove the proxy when done</li>
</ol>
<p class='hint'>If nothing loads, open an http:// site (not https).</p>"""
  else:
    status = "<p>Not connected to Wi-Fi, or no captive portal detected.</p>"

  return f"""<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta http-equiv="refresh" content="5">
<title>zoompilot Wi-Fi Login Helper</title>
<style>
body {{ font-family: sans-serif; background: #1b1b1b; color: #c9c9c9; max-width: 40em; margin: 3em auto; padding: 0 1em; }}
h1 {{ color: #fff; font-size: 1.4em; }}
.ok {{ color: #7fd67f; font-weight: bold; }}
.hint {{ color: #888; }}
li {{ margin: 0.4em 0; }}
</style></head>
<body><h1>zoompilot Wi-Fi Login Helper</h1>
{status}
</body></html>"""


class PortalHelperHandler(BaseHTTPRequestHandler):
  protocol_version = "HTTP/1.1"
  helper: HelperState  # set once by portal_helperd_thread before the server starts

  def log_message(self, fmt: str, *args: object) -> None:
    pass

  def _forbidden(self) -> None:
    self.send_error(403, "portal helper inactive")

  def _proxy_active(self) -> bool:
    return self.helper.get()["state"] == STATE_PORTAL

  def do_GET(self) -> None:
    if self.path.startswith("http://"):
      self._proxy_request()
    elif self.path in ("/", "/status"):
      self._send_status_page()
    else:
      self._forbidden()

  def do_POST(self) -> None:
    if self.path.startswith("http://"):
      self._proxy_request()
    else:
      self._forbidden()

  def _proxy_request(self) -> None:
    if not self._proxy_active():
      self._forbidden()
      return

    u = urlsplit(self.path)
    target = u.path or "/"
    if u.query:
      target += "?" + u.query

    length = int(self.headers.get("Content-Length") or 0)
    body = self.rfile.read(min(length, MAX_RELAY_BYTES)) if length else None
    headers = {k: v for k, v in self.headers.items() if k.lower() not in HOP_HEADERS}

    conn = http.client.HTTPConnection(u.hostname, u.port or 80, timeout=10.0)
    try:
      conn.request(self.command, target, body=body, headers=headers)
      resp = conn.getresponse()
      resp_body = resp.read(MAX_RELAY_BYTES)

      self.send_response(resp.status)
      for k, v in resp.getheaders():
        if k.lower() not in ("connection", "keep-alive", "transfer-encoding", "content-length"):
          self.send_header(k, v)
      self.send_header("Content-Length", str(len(resp_body)))
      self.end_headers()
      if self.command != "HEAD":
        self.wfile.write(resp_body)
    except (OSError, http.client.HTTPException):
      self.send_error(502, "portal helper upstream failure")
    finally:
      conn.close()

  def do_CONNECT(self) -> None:
    if not self._proxy_active():
      self._forbidden()
      return
    host, _, port_str = self.path.partition(":")
    try:
      port = int(port_str or "443")
    except ValueError:
      self._forbidden()
      return
    if port not in CONNECT_ALLOWED_PORTS:
      self._forbidden()
      return

    try:
      upstream = socket.create_connection((host, port), timeout=10.0)
    except OSError:
      self.send_error(502, "portal helper upstream failure")
      return

    self.send_response(200, "Connection Established")
    self.end_headers()
    self.close_connection = True

    def pump(src: socket.socket, dst: socket.socket) -> None:
      try:
        while True:
          data = src.recv(65536)
          if not data:
            break
          dst.sendall(data)
      except OSError:
        pass
      finally:
        try:
          dst.shutdown(socket.SHUT_WR)
        except OSError:
          pass

    relay = threading.Thread(target=pump, args=(self.connection, upstream), daemon=True)
    relay.start()
    pump(upstream, self.connection)
    relay.join(timeout=5.0)
    upstream.close()

  def _send_status_page(self) -> None:
    body = _status_page_html(self.helper.get()).encode()
    self.send_response(200)
    self.send_header("Content-Type", "text/html; charset=utf-8")
    self.send_header("Content-Length", str(len(body)))
    self.send_header("Cache-Control", "no-store")
    self.end_headers()
    self.wfile.write(body)


class PortalHelperServer(ThreadingHTTPServer):
  daemon_threads = True
  allow_reuse_address = True


def portal_helperd_thread(host: str = LISTEN_HOST, port: int = PORT,
                          params: Any = None, log: Any = print) -> None:
  PortalHelperHandler.helper = HelperState(port)
  server = PortalHelperServer((host, port), PortalHelperHandler)
  threading.Thread(target=server.serve_forever, name="portal-helper-http", daemon=True).start()
  log(f"portal_helper: listening on {host}:{port}")

  while True:
    period = POLL_PERIOD_IDLE
    try:
      if default_route_is_wifi():
        state, url = check_internet()
        changed = PortalHelperHandler.helper.update(state=state, url=url, ssid=current_ssid(), ip=wlan_ipv4())
        period = POLL_PERIOD_OK if state == STATE_OK else POLL_PERIOD_PORTAL
      else:
        changed = PortalHelperHandler.helper.update(state=STATE_OFFLINE, url="", ssid="", ip="")

      if changed and params is not None:
        params.put("WifiPortalState", json.dumps(PortalHelperHandler.helper.get()).encode())
    except Exception:
      import traceback
      log(traceback.format_exc())
    time.sleep(period)


def main() -> None:
  # Lazy so the module (and its tests) stay importable without device libraries
  from openpilot.common.params import Params
  from openpilot.common.swaglog import cloudlog

  portal_helperd_thread(params=Params(), log=cloudlog.info)


if __name__ == "__main__":
  main()
