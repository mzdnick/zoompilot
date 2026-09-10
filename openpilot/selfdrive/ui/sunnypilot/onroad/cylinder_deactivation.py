"""
Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.

This file is part of sunnypilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
import time

import pyray as rl

from openpilot.cereal import custom
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.system.ui.lib.text_measure import measure_text_cached

# The element draws itself only while a state other than "all cylinders" is live, then
# lingers and fades. Cars without deactivation hardware never leave normal, so the
# element stays hidden without a setting.
VISIBLE_LINGER_S = 2.0
FADE_S = 0.5

PANEL_W = 104
PANEL_H = 78
PIP_W = 14
PIP_H = 26
PIP_GAP = 6
PIP_ROW_W = 4 * PIP_W + 3 * PIP_GAP
BAR_H = 5

STATE = custom.CarStateZP.CylinderDeactivation.State

PIP_ACTIVE = rl.Color(0x4f, 0xd0, 0x84, 0xff)     # cylinder firing
PIP_DIM = rl.Color(0x5d, 0x6b, 0x74, 0xff)        # cylinder idle (deactivation pending)
PIP_OFF = rl.Color(0x53, 0x9f, 0xc7, 0xff)        # fuel cut outline
BAR_FILL = rl.Color(0x4f, 0xd0, 0x84, 0xff)
BAR_TRACK = rl.Color(0x2b, 0x31, 0x36, 0xff)
PANEL_BG = rl.Color(0x14, 0x16, 0x18, 0xb4)
PANEL_BORDER = rl.Color(0x40, 0x45, 0x4a, 0x66)


def _alpha(color: rl.Color, a: int) -> rl.Color:
  return rl.Color(color.r, color.g, color.b, color.a * a // 255)


class CylinderDeactivationRenderer:
  def __init__(self):
    self._font = gui_app.font(FontWeight.SEMI_BOLD)
    self._last_active_t = 0.0

  def render(self, rect: rl.Rectangle, sm) -> None:
    cd = sm['carStateSP'].zoompilot.cylinderDeactivation
    state = cd.state
    now = time.monotonic()

    a = 255
    if state != STATE.normal:
      self._last_active_t = now
    else:
      since = now - self._last_active_t
      if since > VISIBLE_LINGER_S + FADE_S:
        return
      if since > VISIBLE_LINGER_S:
        a = int(255 * (1.0 - (since - VISIBLE_LINGER_S) / FADE_S))

    x = rect.x + 30
    y = rect.y + rect.height * 0.60
    self._draw_panel(rl.Rectangle(x, y, PANEL_W, PANEL_H), a)

    pips_x = x + (PANEL_W - PIP_ROW_W) / 2
    self._draw_pips(pips_x, y + 10, state, a)

    if state == STATE.entry:
      self._draw_bar(pips_x, y + 42, float(cd.entryProgress), a)
    self._draw_label(x, y + 52, state, a)

  def _draw_panel(self, panel: rl.Rectangle, a: int) -> None:
    rl.draw_rectangle_rounded(panel, 0.25, 8, _alpha(PANEL_BG, a))
    rl.draw_rectangle_rounded_lines_ex(panel, 0.25, 8, 2, _alpha(PANEL_BORDER, a))

  def _draw_pips(self, x: float, y: float, state, a: int) -> None:
    for i in range(4):
      pip = rl.Rectangle(x + i * (PIP_W + PIP_GAP), y, PIP_W, PIP_H)
      if state == STATE.engineBraking:
        # no cylinder fires during fuel cut
        rl.draw_rectangle_lines(int(pip.x), int(pip.y), int(pip.width), int(pip.height), _alpha(PIP_OFF, a))
      elif state == STATE.deactivated:
        # a count of active cylinders, not a map of which ones
        color = PIP_ACTIVE if i < 2 else PIP_DIM
        rl.draw_rectangle_rec(pip, _alpha(color, a))
      else:
        # entry ramp: all four still fire until the latch; the normal linger shows them firing
        color = PIP_DIM if state == STATE.entry else PIP_ACTIVE
        rl.draw_rectangle_rec(pip, _alpha(color, a))

  def _draw_bar(self, x: float, y: float, progress: float, a: int) -> None:
    rl.draw_rectangle_rec(rl.Rectangle(x, y, PIP_ROW_W, BAR_H), _alpha(BAR_TRACK, a))
    fill_w = PIP_ROW_W * max(0.0, min(1.0, progress))
    if fill_w > 0:
      rl.draw_rectangle_rec(rl.Rectangle(x, y, fill_w, BAR_H), _alpha(BAR_FILL, a))

  def _draw_label(self, panel_x: float, y: float, state, a: int) -> None:
    if state == STATE.entry:
      text = "DEACT"
    elif state == STATE.deactivated:
      text = "2/4"
    elif state == STATE.engineBraking:
      text = "FUEL CUT"
    else:
      return
    size = 16
    width = measure_text_cached(self._font, text, size).x
    rl.draw_text_ex(self._font, text, rl.Vector2(panel_x + (PANEL_W - width) / 2, y), size, 0,
                    _alpha(PIP_DIM, a))
