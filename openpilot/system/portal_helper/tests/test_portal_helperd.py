import http.client
import http.server
import json
import socket
import threading
import time
import unittest
from unittest import mock

from openpilot.system.portal_helper import portal_helperd as ph


class FakeParams:
  def __init__(self):
    self.values = {}

  def put(self, key, value):
    self.values[key] = value


class OriginHandler(http.server.BaseHTTPRequestHandler):
  def log_message(self, fmt, *args) -> None:
    pass

  def do_GET(self) -> None:
    body = b"origin-ok " + self.path.encode()
    self.send_response(200)
    self.send_header("Content-Length", str(len(body)))
    self.end_headers()
    self.wfile.write(body)

  def do_POST(self) -> None:
    length = int(self.headers.get("Content-Length") or 0)
    body = b"origin-post:" + self.rfile.read(length)
    self.send_response(200)
    self.send_header("Content-Length", str(len(body)))
    self.end_headers()
    self.wfile.write(body)


class TestCheckInternet(unittest.TestCase):
  def test_ok_204(self):
    with mock.patch.object(ph, "_probe", return_value=(204, {}, b"")):
      self.assertEqual(ph.check_internet(), (ph.STATE_OK, ""))

  def test_portal_redirect(self):
    headers = {"Location": "http://portal.hotel/login"}
    with mock.patch.object(ph, "_probe", return_value=(302, headers, b"")):
      self.assertEqual(ph.check_internet(), (ph.STATE_PORTAL, "http://portal.hotel/login"))

  def test_portal_login_page(self):
    with mock.patch.object(ph, "_probe", return_value=(200, {}, b"<html>sign in</html>")):
      state, url = ph.check_internet()
      self.assertEqual(state, ph.STATE_PORTAL)
      self.assertTrue(url.startswith("http://"))

  def test_msft_body_ok_after_first_fails(self):
    results = [OSError("no dns"), (200, {}, b"Microsoft Connect Test")]
    with mock.patch.object(ph, "_probe", side_effect=results):
      self.assertEqual(ph.check_internet(), (ph.STATE_OK, ""))

  def test_offline_when_all_probes_fail(self):
    with mock.patch.object(ph, "_probe", side_effect=OSError("no route")):
      self.assertEqual(ph.check_internet(), (ph.STATE_OFFLINE, ""))

  def test_offline_when_status_out_of_range(self):
    results = [(500, {}, b""), (404, {}, b"")]
    with mock.patch.object(ph, "_probe", side_effect=results):
      self.assertEqual(ph.check_internet(), (ph.STATE_OFFLINE, ""))


class TestHelperState(unittest.TestCase):
  def test_update_detects_change(self):
    state = ph.HelperState(8090)
    self.assertTrue(state.update(state=ph.STATE_PORTAL, url="http://p"))
    self.assertFalse(state.update(state=ph.STATE_PORTAL, url="http://p"))
    self.assertEqual(state.get()["url"], "http://p")
    self.assertEqual(state.get()["port"], 8090)


class TestServer(unittest.TestCase):
  def setUp(self):
    ph.PortalHelperHandler.helper = ph.HelperState(8090)
    ph.PortalHelperHandler.helper.update(state=ph.STATE_PORTAL, ssid="HotelWiFi", ip="10.0.0.42")

    origin = http.server.ThreadingHTTPServer(("127.0.0.1", 0), OriginHandler)
    threading.Thread(target=origin.serve_forever, daemon=True).start()
    self.origin_port = origin.server_address[1]

    proxy = ph.PortalHelperServer(("127.0.0.1", 0), ph.PortalHelperHandler)
    threading.Thread(target=proxy.serve_forever, daemon=True).start()
    self.proxy_port = proxy.server_address[1]

    self.addCleanup(origin.shutdown)
    self.addCleanup(origin.server_close)
    self.addCleanup(proxy.shutdown)
    self.addCleanup(proxy.server_close)

  def _get(self, target: str) -> tuple[int, bytes]:
    conn = http.client.HTTPConnection("127.0.0.1", self.proxy_port, timeout=5)
    conn.request("GET", target)
    resp = conn.getresponse()
    body = resp.read()
    conn.close()
    return resp.status, body

  def test_status_page(self):
    status, body = self._get("/")
    self.assertEqual(status, 200)
    self.assertIn("Wi-Fi Login Helper", body.decode())
    self.assertIn("10.0.0.42:8090", body.decode())

  def test_proxy_get(self):
    status, body = self._get(f"http://127.0.0.1:{self.origin_port}/x?a=1")
    self.assertEqual(status, 200)
    self.assertEqual(body, b"origin-ok /x?a=1")

  def test_proxy_post(self):
    conn = http.client.HTTPConnection("127.0.0.1", self.proxy_port, timeout=5)
    conn.request("POST", f"http://127.0.0.1:{self.origin_port}/login", body=b"user=1")
    resp = conn.getresponse()
    body = resp.read()
    conn.close()
    self.assertEqual(resp.status, 200)
    self.assertEqual(body, b"origin-post:user=1")

  def test_proxy_forbidden_when_no_portal(self):
    ph.PortalHelperHandler.helper.update(state=ph.STATE_OK)
    status, _ = self._get(f"http://127.0.0.1:{self.origin_port}/x")
    self.assertEqual(status, 403)

  def test_connect_tunnel(self):
    echo = socket.socket()
    echo.bind(("127.0.0.1", 0))
    echo.listen(1)
    echo_port = echo.getsockname()[1]

    def echo_run():
      conn, _ = echo.accept()
      while True:
        data = conn.recv(4096)
        if not data:
          break
        conn.sendall(data)
      conn.close()

    threading.Thread(target=echo_run, daemon=True).start()

    with mock.patch.object(ph, "CONNECT_ALLOWED_PORTS", (echo_port,)):
      sock = socket.create_connection(("127.0.0.1", self.proxy_port), timeout=5)
      sock.sendall(f"CONNECT 127.0.0.1:{echo_port} HTTP/1.1\r\nHost: x\r\n\r\n".encode())
      reply = b""
      while b"\r\n\r\n" not in reply:
        reply += sock.recv(4096)
      self.assertIn(b" 200 ", reply)

      sock.sendall(b"ping")
      self.assertEqual(sock.recv(4096), b"ping")
      sock.close()

  def test_connect_port_restricted(self):
    sock = socket.create_connection(("127.0.0.1", self.proxy_port), timeout=5)
    sock.sendall(b"CONNECT 127.0.0.1:12345 HTTP/1.1\r\nHost: x\r\n\r\n")
    reply = sock.recv(4096)
    self.assertIn(b" 403 ", reply)
    sock.close()


class TestLoop(unittest.TestCase):
  def test_loop_writes_params(self):
    params = FakeParams()

    def run_thread():
      with mock.patch.object(ph, "default_route_is_wifi", return_value=True), \
           mock.patch.object(ph, "check_internet", return_value=(ph.STATE_PORTAL, "http://portal/x")), \
           mock.patch.object(ph, "current_ssid", return_value="HotelWiFi"), \
           mock.patch.object(ph, "wlan_ipv4", return_value="10.0.0.42"), \
           mock.patch.object(ph, "POLL_PERIOD_PORTAL", 0.05), \
           mock.patch.object(ph, "POLL_PERIOD_OK", 0.05), \
           mock.patch.object(ph, "POLL_PERIOD_IDLE", 0.05):
        ph.portal_helperd_thread(host="127.0.0.1", port=0, params=params)

    threading.Thread(target=run_thread, daemon=True).start()
    time.sleep(0.3)

    value = json.loads(params.values["WifiPortalState"])
    self.assertEqual(value["state"], ph.STATE_PORTAL)
    self.assertEqual(value["ssid"], "HotelWiFi")
    self.assertEqual(value["ip"], "10.0.0.42")
    self.assertEqual(value["url"], "http://portal/x")


if __name__ == "__main__":
  unittest.main()
