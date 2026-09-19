"""
Copyright (c) 2026-, mzdnick.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
import json
import threading
from enum import IntEnum

import pyray as rl

from openpilot.system.ui.lib.application import TextAlignment, gui_app
from openpilot.system.ui.lib.multilang import tr
from openpilot.system.ui.widgets import DialogResult, Widget
from openpilot.system.ui.widgets.button import Button, ButtonStyle
from openpilot.system.ui.widgets.keyboard import Keyboard
from openpilot.system.ui.widgets.label import gui_label

from openpilot.sunnypilot.captive_portal.client import Connectivity, PortalClient, PortalError, make_profile
from openpilot.sunnypilot.captive_portal.forms import LoginForm

MAX_FIELD_LENGTH = 128
MAX_PAGES = 3


class Phase(IntEnum):
  LOADING = 0     # first form fetch in flight
  ENTER = 1       # asking the portal form's fields, one keyboard at a time
  WORKING = 2     # login request in flight
  DONE = 3        # online and profile saved
  FAILED = 4


class CaptivePortalLoginUI(Widget):
  """Interactive captive portal login: fetch the portal form, ask each field on the
  keyboard, submit, and save the working login as the auto-login profile."""

  def __init__(self, wifi_manager):
    super().__init__()
    self._wifi_manager = wifi_manager
    self._client = PortalClient()

    # two keyboards: portal usernames are readable text, passwords start masked
    self._plain_kb = Keyboard(max_text_size=MAX_FIELD_LENGTH, min_text_size=0)
    self._secret_kb = Keyboard(max_text_size=MAX_FIELD_LENGTH, min_text_size=0,
                               password_mode=True, show_password_toggle=True)

    self.close_button = Button(tr("Close"), self._close, button_style=ButtonStyle.NORMAL)
    self.close_button.set_rect(rl.Rectangle(0, 0, 400, 100))
    self.retry_button = Button(tr("Retry"), self._retry, button_style=ButtonStyle.NORMAL)
    self.retry_button.set_rect(rl.Rectangle(0, 0, 400, 100))

    self.phase = Phase.LOADING
    self.message = ""
    self._form: LoginForm | None = None
    self._values: dict[str, str] = {}
    self._field_index = 0
    self._pages_done = 0
    self._asking = False

    self._start()

  def _start(self):
    self.phase = Phase.LOADING
    self.message = ""
    self._pages_done = 0
    self._asking = False
    threading.Thread(target=self._fetch_first_form, daemon=True).start()

  # --- worker-thread code; only touches plain state the render loop reads ---

  def _fetch_first_form(self):
    probe = self._client.probe()
    if probe.state == Connectivity.ONLINE:
      self._fail(tr("No captive portal: already online"))
    elif probe.state == Connectivity.OFFLINE or not probe.portal_url:
      self._fail(tr("Portal page unreachable"))
    else:
      self._load_form(probe.portal_url)

  def _load_form(self, url: str):
    try:
      form, _final_url = self._client.fetch_form(url)
    except PortalError as e:
      self._fail(str(e))
      return
    self._form = form
    self._values = {}
    self._field_index = 0
    self._asking = False
    self.phase = Phase.ENTER

  def _submit(self):
    assert self._form is not None
    self.phase = Phase.WORKING
    try:
      self._client.submit(self._form, self._values)
    except PortalError as e:
      self._fail(str(e))
      return
    probe = self._client.probe()
    if probe.state == Connectivity.ONLINE:
      self._save_profile()
      self.phase = Phase.DONE
    elif probe.portal_url and self._pages_done + 1 < MAX_PAGES:
      # multi-page portals: the follow-up page (accept terms style) is asked in turn
      self._pages_done += 1
      self._load_form(probe.portal_url)
    else:
      self._fail(tr("The portal did not accept these details"))

  def _save_profile(self):
    ssid = self._wifi_manager.connected_ssid
    if not ssid or self._form is None:
      return
    try:
      from openpilot.common.params import Params
      params = Params()
      raw = params.get("CaptivePortalProfiles") or b"{}"
      profiles = json.loads(raw)
      profiles[ssid] = make_profile(ssid, self._form, self._values)
      params.put("CaptivePortalProfiles", json.dumps(profiles).encode(), block=True)
    except Exception:
      # the login worked; a failed save must not turn the screen into an error
      self.message = tr("Logged in, but the login could not be saved")

  def _fail(self, message: str):
    self.phase = Phase.FAILED
    self.message = message

  # --- UI-thread code ---

  def _ask_next_field(self):
    assert self._form is not None
    fields = self._form.user_fields
    if self._field_index >= len(fields):
      threading.Thread(target=self._submit, daemon=True).start()
      return

    field = fields[self._field_index]
    keyboard = self._secret_kb if field.type == "password" else self._plain_kb

    def field_done(result):
      if result != DialogResult.CONFIRM:
        self._close()
        return
      value = keyboard.text.strip()
      if value:
        self._values[field.name] = value
      self._field_index += 1
      self._ask_next_field()

    keyboard.clear()
    keyboard.set_title(field.name, tr("field {} of {}").format(self._field_index + 1, len(fields)))
    keyboard.set_callback(field_done)
    gui_app.push_widget(keyboard)

  def _close(self):
    gui_app.pop_widget()

  def _retry(self):
    self._start()

  def _render(self, rect: rl.Rectangle):
    rl.draw_rectangle_rec(rect, rl.Color(13, 13, 13, 255))

    title_rect = rl.Rectangle(rect.x, rect.y + rect.height * 0.30, rect.width, 100)
    message_rect = rl.Rectangle(rect.x, rect.y + rect.height * 0.30 + 120, rect.width, 80)

    if self.phase == Phase.LOADING:
      gui_label(title_rect, tr("Contacting the portal..."), 72, alignment=TextAlignment.CENTER)
    elif self.phase == Phase.ENTER:
      gui_label(title_rect, tr("Captive portal login"), 72, alignment=TextAlignment.CENTER)
      if not self._asking:
        self._asking = True
        self._ask_next_field()
    elif self.phase == Phase.WORKING:
      gui_label(title_rect, tr("Signing in..."), 72, alignment=TextAlignment.CENTER)
    elif self.phase == Phase.DONE:
      gui_label(title_rect, tr("Logged in"), 72, alignment=TextAlignment.CENTER)
      gui_label(message_rect, self.message or tr("Login saved for this network"), 48, alignment=TextAlignment.CENTER)
    else:
      gui_label(title_rect, tr("Login failed"), 72, alignment=TextAlignment.CENTER)
      gui_label(message_rect, self.message, 48, alignment=TextAlignment.CENTER)
      self.retry_button.set_position(rect.x + rect.width / 2 - 420, rect.y + rect.height * 0.60)
      self.retry_button.render()

    self.close_button.set_position(rect.x + rect.width / 2 + 20, rect.y + rect.height * 0.60)
    self.close_button.render()
