"""
Copyright (c) 2026-, mzdnick.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

import pyray as rl

from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.system.ui.lib.application import gui_app
from openpilot.system.ui.lib.multilang import tr
from openpilot.system.ui.sunnypilot.widgets.list_view import button_item_sp, toggle_item_sp
from openpilot.system.ui.widgets import DialogResult, Widget
from openpilot.system.ui.widgets.keyboard import Keyboard
from openpilot.system.ui.widgets.scroller_tici import LineSeparator, Scroller

MAX_TEXT_SIZE = 256


class PangolinLayout(Widget):
  """Settings for the pangolin remote access client: one toggle and three credential fields."""

  def __init__(self):
    super().__init__()
    self._plain_kb = Keyboard(max_text_size=MAX_TEXT_SIZE, min_text_size=0)
    self._secret_kb = Keyboard(max_text_size=MAX_TEXT_SIZE, min_text_size=0,
                               password_mode=True, show_password_toggle=True)

    self._toggle = toggle_item_sp(
      title=tr("Pangolin remote access"),
      description=tr("Connect out to your own Pangolin server and allow SSH through a private resource on it. " +
                     "The device opens no ports. Requires the server endpoint and the machine client credentials."),
      param="PangolinEnabled",
    )
    self._endpoint_btn = button_item_sp(
      title=tr("Server endpoint"),
      button_text="",
      description=tr("Your Pangolin server URL, for example https://pangolin.example.com"),
      callback=self._edit_endpoint,
    )
    self._id_btn = button_item_sp(
      title=tr("Client ID"),
      button_text="",
      description=tr("The machine client ID from your Pangolin dashboard."),
      callback=self._edit_id,
    )
    self._secret_btn = button_item_sp(
      title=tr("Client secret"),
      button_text="",
      description=tr("The machine client secret from your Pangolin dashboard."),
      callback=self._edit_secret,
    )

    items = [
      self._toggle,
      LineSeparator(),
      self._endpoint_btn,
      LineSeparator(),
      self._id_btn,
      LineSeparator(),
      self._secret_btn,
    ]
    self._scroller = Scroller(items, line_separator=False, spacing=0)
    self._update_buttons()

  def _edit_endpoint(self):
    self._edit("PangolinEndpoint", tr("Server endpoint"), self._plain_kb)

  def _edit_id(self):
    self._edit("PangolinClientId", tr("Client ID"), self._plain_kb)

  def _edit_secret(self):
    self._edit("PangolinClientSecret", tr("Client secret"), self._secret_kb)

  def _edit(self, param: str, title: str, keyboard: Keyboard):
    current = ui_state.params.get(param)
    keyboard.clear()
    if current:
      keyboard.set_text(current.decode())
    keyboard.set_title(title)
    keyboard.set_callback(lambda result: self._saved(param, keyboard, result))
    gui_app.push_widget(keyboard)

  # the keyboard pops itself before the callback runs; empty input keeps the old value
  def _saved(self, param: str, keyboard: Keyboard, result: int):
    if result == DialogResult.CONFIRM and keyboard.text.strip():
      ui_state.params.put(param, keyboard.text.strip().encode())
    self._update_buttons()

  def _update_buttons(self):
    for btn, param, set_text in (
      (self._endpoint_btn, "PangolinEndpoint", tr("Set URL")),
      (self._id_btn, "PangolinClientId", tr("Set ID")),
      (self._secret_btn, "PangolinClientSecret", tr("Set secret")),
    ):
      btn.action_item.set_text(tr("Edit") if ui_state.params.get(param) else set_text)

  def _update_state(self):
    super()._update_state()
    status = ui_state.params.get("PangolinStatus")
    self._toggle.set_right_value((status.decode() if status else tr("stopped")).capitalize())

  def _render(self, rect: rl.Rectangle):
    self._scroller.render(rect)
