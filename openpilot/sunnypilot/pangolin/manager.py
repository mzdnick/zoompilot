"""
Copyright (c) 2026-, mzdnick.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

import hashlib
import os
import shutil
import signal
import subprocess
import time
import urllib.request

from openpilot.common.params import Params
from openpilot.common.swaglog import cloudlog

# pinned pangolin-cli release; a bump needs a new SHA-256 and a fork commit
CLI_VERSION = "0.17.0"
CLI_SHA256 = "dcfd98abc5b9716922b2fc72d7a5a98b470840f9380410901e008b836ede9198"
CLI_URL = f"https://github.com/fosrl/cli/releases/download/{CLI_VERSION}/pangolin-cli_linux_arm64"
CLI_PATH = "/data/pangolin/pangolin"

DOWNLOAD_TIMEOUT = 120  # seconds; the binary is about 28 MB
BACKOFF_START = 30.0  # seconds before the first retry after a crash or failed download
BACKOFF_MAX = 300.0
STABLE_RUNTIME = 60.0  # seconds; a run this long counts as healthy and resets the backoff

CREDENTIAL_PARAMS = {
  "PangolinEndpoint": "PANGOLIN_ENDPOINT",
  "PangolinClientId": "PANGOLIN_CLIENT_ID",
  "PangolinClientSecret": "PANGOLIN_CLIENT_SECRET",
}


def pangolin_ready(params: Params | None = None) -> bool:
  """Gate for process_config: run only when the toggle is on and every credential is set."""
  params = params or Params()
  return params.get_bool("PangolinEnabled") and all(params.get(p) for p in CREDENTIAL_PARAMS)


def file_sha256(path: str) -> str | None:
  digest = hashlib.sha256()
  try:
    with open(path, "rb") as f:
      for chunk in iter(lambda: f.read(1 << 20), b""):
        digest.update(chunk)
  except OSError:
    return None
  return digest.hexdigest()


class PangolinManager:
  """Provisions the pinned pangolin-cli binary and keeps the client tunnel attached."""

  def __init__(self, params: Params | None = None):
    self.params = params or Params()
    self.backoff = BACKOFF_START
    self._status = ""
    self._proc: subprocess.Popen | None = None

  def set_status(self, status: str):
    if status != self._status:
      self._status = status
      self.params.put("PangolinStatus", status.encode())
      cloudlog.info(f"pangolin_clientd: {status}")

  def ensure_binary(self) -> bool:
    if file_sha256(CLI_PATH) == CLI_SHA256:
      return True

    self.set_status(f"downloading pangolin-cli {CLI_VERSION}")
    os.makedirs(os.path.dirname(CLI_PATH), exist_ok=True)
    tmp_path = CLI_PATH + ".tmp"
    try:
      with urllib.request.urlopen(CLI_URL, timeout=DOWNLOAD_TIMEOUT) as response, open(tmp_path, "wb") as tmp:
        shutil.copyfileobj(response, tmp)
    except OSError as e:
      cloudlog.warning(f"pangolin_clientd: download failed: {e}")
      self._remove(tmp_path)
      return False

    if file_sha256(tmp_path) != CLI_SHA256:
      # a wrong or partial download must never reach the exec path
      cloudlog.warning("pangolin_clientd: download checksum mismatch")
      self._remove(tmp_path)
      return False

    os.chmod(tmp_path, 0o755)
    os.replace(tmp_path, CLI_PATH)
    return True

  @staticmethod
  def _remove(path: str):
    try:
      os.remove(path)
    except OSError:
      pass

  def run_client(self) -> int:
    env = os.environ.copy()
    for param, env_name in CREDENTIAL_PARAMS.items():
      value = self.params.get(param)
      if value:
        env[env_name] = value.decode()
    # credentials ride the environment; the command line stays secret-free for `ps`
    self._proc = subprocess.Popen([CLI_PATH, "up", "client", "--attach"], env=env,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert self._proc.stdout is not None
    for line in self._proc.stdout:
      cloudlog.info(f"pangolin-cli: {line.strip()}")
    return self._proc.wait()

  def handle_term(self, signum, frame):
    # the manager stops this process; the client child must not outlive it
    if self._proc:
      self._proc.terminate()
    raise SystemExit(0)

  def main_thread(self):
    while True:
      if not self.ensure_binary():
        self.set_status(f"download failed, retry in {int(self.backoff)} s")
        time.sleep(self.backoff)
        self.backoff = min(self.backoff * 2, BACKOFF_MAX)
        continue

      self.set_status("running")
      started = time.monotonic()
      try:
        returncode = self.run_client()
      finally:
        if self._proc and self._proc.poll() is None:
          self._proc.kill()

      if time.monotonic() - started >= STABLE_RUNTIME:
        self.backoff = BACKOFF_START
      self.set_status(f"client exited ({returncode}), retry in {int(self.backoff)} s")
      time.sleep(self.backoff)
      self.backoff = min(self.backoff * 2, BACKOFF_MAX)


def main():
  manager = PangolinManager()
  signal.signal(signal.SIGTERM, manager.handle_term)
  manager.main_thread()


if __name__ == "__main__":
  main()
