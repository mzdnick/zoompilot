"""
Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.

This file is part of sunnypilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
import ctypes
import importlib.util
import os
import sys
import time
from pathlib import Path

import pyray as rl

from openpilot.common.params import Params
from openpilot.system.ui.lib.application import FontWeight, gui_app
from openpilot.system.ui.widgets import Widget

_LIB_PATH = Path(__file__).resolve().parents[4] / 'sunnypilot/system/ui/screensaver_engine/libinfinite_drive.so'


def _raylib_providers() -> list[Path]:
  # pyray is only a shim package; the cffi module with raylib inside lives in
  # the raylib package. Prefer paths python already loaded (same name = promote,
  # not a second copy), then fall back to the package dir.
  found: dict[str, Path] = {}
  for mod in list(sys.modules.values()):
    f = getattr(mod, '__file__', None)
    if f and f.endswith('.so') and '_raylib' in Path(f).name:
      found[f] = Path(f)
  spec = importlib.util.find_spec('raylib')
  if spec and spec.submodule_search_locations:
    for f in Path(spec.submodule_search_locations[0]).glob('_raylib*.so'):
      found.setdefault(str(f), f)
  return list(found.values())


class ScreenSaver3D(Widget):
  def __init__(self, params: Params | None = None):
    super().__init__()
    self.set_rect(rl.Rectangle(0, 0, gui_app.width, gui_app.height))
    self._params = params or Params()
    self._start_time = None
    self._dismiss = False
    self._screensaver_timeout = 300
    self._seed = int(time.monotonic_ns() & 0xFFFFFFFF)
    self._inited = False
    self._lib = None
    self._load_error = None

  @staticmethod
  def available() -> bool:
    return _LIB_PATH.exists()

  def _load(self):
    self._load_error = None
    # raylib lives inside the cffi module with local scope; the engine's
    # undefined refs only resolve if we promote that module to global first
    try:
      for provider in _raylib_providers():
        ctypes.CDLL(str(provider), mode=os.RTLD_GLOBAL | os.RTLD_NOW)
      # RTLD_NOW makes a failed binding raise here instead of crashing on first draw
      lib = ctypes.CDLL(str(_LIB_PATH), mode=os.RTLD_NOW)
    except OSError as e:
      # shown on screen: devices in the field have no shell to read logs from
      self._load_error = str(e).replace(f'{_LIB_PATH.parent}/', '')[:100]
      return None
    lib.id_init.argtypes = [ctypes.c_uint64]
    lib.id_reset.argtypes = [ctypes.c_uint64]
    lib.id_render.argtypes = [ctypes.c_float]
    return lib

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
      if self._lib is None:
        self._lib = self._load()
      if self._lib is not None:
        if self._inited:
          self._lib.id_reset(self._seed)
        else:
          self._lib.id_init(self._seed)
          self._inited = True
    self._dismiss = False

  def hide_event(self):
    super().hide_event()
    self._dismiss = False
    self._start_time = None

  def _handle_mouse_release(self, mouse_pos):
    self._dismiss = True
    self._start_time = None
    gui_app.pop_widget()
    return super()._handle_mouse_release(mouse_pos)

  def _update_state(self):
    super()._update_state()

    if self._start_time and time.monotonic() - self._start_time > self._screensaver_timeout:
      self._dismiss = True
      self._start_time = None

  def _render(self, rect: rl.Rectangle):
    self.set_rect(rect)
    if self._lib is not None:
      self._lib.id_render(rl.get_frame_time())
    else:
      rl.clear_background(rl.BLACK)
      if self._load_error:
        font = gui_app.font(FontWeight.NORMAL)
        pos = rl.Vector2(40, 40)
        rl.draw_text_ex(font, self._load_error, pos, 40, 2, rl.WHITE)
    return -1
