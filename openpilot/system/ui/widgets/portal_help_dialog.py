import json
import time

import pyray as rl

from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.system.ui.lib.multilang import tr
from openpilot.system.ui.widgets import Widget
from openpilot.system.ui.widgets.button import Button, ButtonStyle
from openpilot.system.ui.widgets.label import gui_label

MARGIN = 80
TITLE_FONT_SIZE = 72
TEXT_FONT_SIZE = 55
HINT_FONT_SIZE = 42
LINE_SPACING = 88
BUTTON_HEIGHT = 100
BACKGROUND_COLOR = rl.Color(27, 27, 27, 255)
TEXT_COLOR = rl.Color(201, 201, 201, 255)
HINT_COLOR = rl.Color(130, 130, 130, 255)
REFRESH_PERIOD = 1.0


class PortalHelpDialog(Widget):
  # Portal state comes from the portal_helper daemon via params, so the dialog
  # flips to "completed" by itself once the login succeeds
  def __init__(self, portal_state: dict | None = None):
    super().__init__()
    self._state: dict = dict(portal_state or {})
    self._params = None
    try:
      from openpilot.common.params import Params
      self._params = Params()
    except Exception:
      pass
    self._last_refresh = 0.0
    self._close_button = Button(tr("Close"), self._close, button_style=ButtonStyle.NORMAL, font_size=55)

  def _close(self):
    gui_app.pop_widget()

  def _update_state(self):
    now = time.monotonic()
    if self._params is None or now - self._last_refresh < REFRESH_PERIOD:
      return
    self._last_refresh = now
    try:
      raw = self._params.get("WifiPortalState")
      if raw:
        self._state = json.loads(raw)
    except Exception:
      pass

  def _render(self, _):
    rect = rl.Rectangle(0, 0, gui_app.width, gui_app.height)
    rl.draw_rectangle_rec(rect, BACKGROUND_COLOR)

    done = self._state.get("state") == "ok"
    ip = self._state.get("ip", "")
    port = self._state.get("port", 8090)
    proxy = f"{ip}:{port}" if ip else f"{tr('this device IP')}:{port}"

    title = tr("Login completed") if done else tr("Wi-Fi Login Required")
    gui_label(rl.Rectangle(rect.x, rect.y + MARGIN, rect.width, TITLE_FONT_SIZE * 1.3), title,
              font_size=TITLE_FONT_SIZE, color=rl.WHITE, font_weight=FontWeight.BOLD,
              alignment=rl.GuiTextAlignment.TEXT_ALIGN_CENTER)

    if done:
      lines = [tr("This device is now online."), tr("You can remove the HTTP proxy from your phone.")]
    else:
      lines = [
        tr("This Wi-Fi network needs a login page."),
        tr("Complete the login from your phone:"),
        tr("1. Join this Wi-Fi network on the phone"),
        tr("2. Set the phone's HTTP proxy to {}").format(proxy),
        tr("3. Open any http:// website and log in"),
        tr("4. Remove the proxy when done"),
      ]

    y = rect.y + MARGIN * 2 + TITLE_FONT_SIZE
    for i, line in enumerate(lines):
      weight = FontWeight.BOLD if not done and i == 3 else FontWeight.NORMAL
      gui_label(rl.Rectangle(rect.x + MARGIN * 2, y, rect.width - MARGIN * 4, TEXT_FONT_SIZE * 1.3), line,
                font_size=TEXT_FONT_SIZE, color=TEXT_COLOR, font_weight=weight)
      y += LINE_SPACING

    if not done:
      gui_label(rl.Rectangle(rect.x + MARGIN * 2, y + MARGIN / 2, rect.width - MARGIN * 4, HINT_FONT_SIZE * 1.3),
                tr("If nothing loads, open an http:// site (not https)."),
                font_size=HINT_FONT_SIZE, color=HINT_COLOR)

    button_width = 400
    self._close_button.set_rect(rl.Rectangle(rect.x + (rect.width - button_width) / 2,
                                             rect.y + rect.height - BUTTON_HEIGHT - MARGIN, button_width, BUTTON_HEIGHT))
    self._close_button.render(self._close_button.rect)
