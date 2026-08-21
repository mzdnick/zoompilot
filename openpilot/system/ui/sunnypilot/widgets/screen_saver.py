"""
Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.

This file is part of sunnypilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
import os
import time

import pyray as rl

from openpilot.common.hardware import HARDWARE
from openpilot.common.params import Params
from openpilot.system.ui.lib.application import gui_app, FontWeight
from openpilot.system.ui.lib.text_measure import measure_text_cached
from openpilot.system.ui.widgets import Widget


class ScreenSaverSP(Widget):
  def __init__(self, params: Params | None = None):
    super().__init__()
    self.set_rect(rl.Rectangle(0, 0, gui_app.width, gui_app.height))
    self._params = params or Params()
    self._is_mici = HARDWARE.get_device_type() == 'mici' or (HARDWARE.get_device_type() == "pc" and os.getenv("BIG") != "1")

    self.vx = 180.0 if self._is_mici else 450.0
    self._hue = 150
    self._sweeps = []

    self.text = "sunnypilot"
    self.font_size = 50 if self._is_mici else 200
    self._start_time = None
    self._dismiss = False
    self._screensaver_timeout = 300

  @property
  def is_active(self) -> bool:
    return self._start_time is not None and not self._dismiss

  @property
  def was_dismissed(self) -> bool:
    return self._dismiss

  def initialize(self):
    self._screensaver_timeout = self._params.get("ScreenSaverTimeout", return_default=True)
    if self._start_time is None:
      self._start_time = time.monotonic()
    self._dismiss = False

  def hide_event(self):
    super().hide_event()
    self._dismiss = False
    self._start_time = None
    self._sweeps = []

  def _handle_mouse_release(self, mouse_pos):
    self._dismiss = True
    self._start_time = None
    gui_app.pop_widget()
    return super()._handle_mouse_release(mouse_pos)

  def _spawn(self):
    y = rl.get_random_value(0, max(int(self.rect.height - self.logo_height), 0))
    while self._hue_dist((new_hue := rl.get_random_value(0, 360)), self._hue) < 120:
      pass
    self._hue = new_hue
    self._sweeps.append({'x': -self.logo_width, 'y': y, 'color': rl.color_from_hsv(new_hue, 1, 1)})

  def _update_state(self):
    super()._update_state()

    self.font = gui_app.font(FontWeight.AUDIOWIDE)
    text_size = measure_text_cached(self.font, self.text, self.font_size, 0)
    self.logo_width = text_size.x
    self.logo_height = text_size.y

    if self._start_time and time.monotonic() - self._start_time > self._screensaver_timeout:
      self._dismiss = True
      self._start_time = None

    if not self._sweeps:
      self._spawn()
      return

    dt = rl.get_frame_time()
    for sweep in self._sweeps:
      sweep['x'] += self.vx * dt

    # new sweep enters the left edge as the newest starts exiting the right one
    if self._sweeps[-1]['x'] + self.logo_width >= self.rect.width:
      self._spawn()

    self._sweeps = [s for s in self._sweeps if s['x'] <= self.rect.width]

  @staticmethod
  def _hue_dist(a, b):
    d = abs(a - b)
    return min(d, 360 - d)

  def _render(self, rect: rl.Rectangle):
    self.set_rect(rect)
    rl.clear_background(rl.BLACK)
    for sweep in self._sweeps:
      rl.draw_text_ex(self.font, self.text, rl.Vector2(int(sweep['x']), int(sweep['y'])), self.font_size, 0, sweep['color'])
    return -1
