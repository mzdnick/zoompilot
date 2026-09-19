"""
Copyright (c) 2026-, mzdnick.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

import hashlib
import io
import os
import stat

from openpilot.sunnypilot.pangolin import manager


class FakeParams:
  def __init__(self, values=None):
    self.values = dict(values or {})

  def get(self, key, **_):
    return self.values.get(key)

  def get_bool(self, key, **_):
    # the real store parses the bytes; b"0" is false
    return self.values.get(key) == b"1"

  def put(self, key, value, **_):
    self.values[key] = value if isinstance(value, bytes) else str(value).encode()


COMPLETE_CREDS = {
  "PangolinEnabled": b"1",
  "PangolinEndpoint": b"https://pangolin.example.com",
  "PangolinClientId": b"client-id",
  "PangolinClientSecret": b"client-secret",
}


def test_ready_needs_toggle_and_every_credential():
  assert manager.pangolin_ready(FakeParams(COMPLETE_CREDS))

  for key in COMPLETE_CREDS:
    values = dict(COMPLETE_CREDS)
    del values[key]
    assert not manager.pangolin_ready(FakeParams(values))

  disabled = dict(COMPLETE_CREDS)
  disabled["PangolinEnabled"] = b"0"
  assert not manager.pangolin_ready(FakeParams(disabled))


def test_file_sha256_of_missing_file_is_none(tmp_path):
  assert manager.file_sha256(str(tmp_path / "absent")) is None

  payload = b"pangolin"
  target = tmp_path / "binary"
  target.write_bytes(payload)
  assert manager.file_sha256(str(target)) == hashlib.sha256(payload).hexdigest()


def pin_binary(tmp_path, monkeypatch, sha256_of: bytes):
  path = str(tmp_path / "pangolin")
  monkeypatch.setattr(manager, "CLI_PATH", path)
  monkeypatch.setattr(manager, "CLI_URL", "https://example.invalid/pangolin")
  monkeypatch.setattr(manager, "CLI_SHA256", hashlib.sha256(sha256_of).hexdigest())
  return path


def fake_urlopen(payload: bytes):
  def _open(url, timeout=None):
    return io.BytesIO(payload)
  return _open


def test_ensure_binary_downloads_verifies_and_marks_executable(tmp_path, monkeypatch):
  payload = b"fake arm64 cli"
  path = pin_binary(tmp_path, monkeypatch, payload)
  monkeypatch.setattr(manager.urllib.request, "urlopen", fake_urlopen(payload))

  assert manager.PangolinManager(params=FakeParams()).ensure_binary()
  assert os.path.isfile(path)
  assert stat.S_IMODE(os.stat(path).st_mode) == 0o755
  assert not os.path.exists(path + ".tmp")


def test_ensure_binary_rejects_a_wrong_checksum(tmp_path, monkeypatch):
  path = pin_binary(tmp_path, monkeypatch, b"expected bytes")
  monkeypatch.setattr(manager.urllib.request, "urlopen", fake_urlopen(b"tampered"))

  assert not manager.PangolinManager(params=FakeParams()).ensure_binary()
  assert not os.path.exists(path)
  assert not os.path.exists(path + ".tmp")


def test_ensure_binary_survives_a_failed_download(tmp_path, monkeypatch):
  path = pin_binary(tmp_path, monkeypatch, b"anything")

  def _raise(url, timeout=None):
    raise OSError("network down")

  monkeypatch.setattr(manager.urllib.request, "urlopen", _raise)

  assert not manager.PangolinManager(params=FakeParams()).ensure_binary()
  assert not os.path.exists(path + ".tmp")


def test_existing_pinned_binary_short_circuits_the_download(tmp_path, monkeypatch):
  payload = b"already here"
  path = pin_binary(tmp_path, monkeypatch, payload)
  with open(path, "wb") as f:
    f.write(payload)

  def _fail(url, timeout=None):
    raise AssertionError("must not download")

  monkeypatch.setattr(manager.urllib.request, "urlopen", _fail)
  assert manager.PangolinManager(params=FakeParams()).ensure_binary()


def test_set_status_writes_each_change_once():
  params = FakeParams()
  mgr = manager.PangolinManager(params=params)

  mgr.set_status("running")
  mgr.set_status("running")

  assert params.values["PangolinStatus"] == b"running"
