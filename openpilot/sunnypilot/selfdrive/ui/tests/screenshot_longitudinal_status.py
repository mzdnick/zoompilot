#!/usr/bin/env python3
"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

Render the alpha longitudinal switch with each status line, on both device families, to PNG
under screenshots/{mici,tizi}/ (untracked). BIG must be set before the UI imports:
  PYTHONPATH=. BIG=0 SCALE=1   .venv/bin/python openpilot/sunnypilot/selfdrive/ui/tests/screenshot_longitudinal_status.py
  PYTHONPATH=. BIG=1 SCALE=0.5 .venv/bin/python openpilot/sunnypilot/selfdrive/ui/tests/screenshot_longitudinal_status.py
"""
import os
import sys
from pathlib import Path

BIG = os.getenv("BIG", "0") == "1"

import pyray as rl

from openpilot.common.prefix import OpenpilotPrefix
from openpilot.sunnypilot.selfdrive.ui.tests import screenshot_layouts

os.environ["BIG"] = "1" if BIG else "0"  # screenshot_layouts pins the mici UI at import; restore the family asked for
STATES = ("initializing", "ready", "failed")


def capture(widget, filename: str, frames=20):
  # screenshot_layouts.capture resets the scroller's position filter, which undoes the scroll
  # to the alpha toggle below; a plain render-to-texture keeps it
  from openpilot.system.ui.lib.application import gui_app
  rt = rl.load_render_texture(gui_app.width, gui_app.height)
  rect = rl.Rectangle(0, 0, gui_app.width, gui_app.height)
  for _ in range(frames):
    rl.begin_texture_mode(rt)
    rl.clear_background(rl.BLACK)
    widget.render(rect)
    rl.end_texture_mode()
    rl.begin_drawing()
    rl.end_drawing()
  image = rl.load_image_from_texture(rt.texture)
  rl.image_flip_vertical(image)
  screenshot_layouts.OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
  rl.export_image(image, str(screenshot_layouts.OUTPUT_DIR / filename))
  rl.unload_image(image)
  rl.unload_render_texture(rt)
  print(f"  {filename}")


def main():
  screenshot_layouts.OUTPUT_DIR = Path(__file__).parent / "screenshots" / ("tizi" if BIG else "mici")
  with OpenpilotPrefix():
    rl.set_config_flags(rl.FLAG_WINDOW_HIDDEN)
    from openpilot.system.ui.lib.application import gui_app
    gui_app.init_window("screenshot_longitudinal_status", fps=30)
    screenshot_layouts.setup_params()
    screenshot_layouts.setup_ui_state()
    from opendbc.car.structs import car
    from openpilot.common.params import Params
    from openpilot.selfdrive.ui.ui_state import ui_state
    # the toggle is hidden on a car without alpha long, and the layouts re-read CarParams on show
    cp = car.CarParams.new_message(brand="mazda", alphaLongitudinalAvailable=True, openpilotLongitudinalControl=True)
    Params().put("CarParamsPersistent", cp.to_bytes())
    ui_state.update_params()
    if BIG:
      from openpilot.selfdrive.ui.sunnypilot.layouts.settings.developer import DeveloperLayoutSP
      dev = DeveloperLayoutSP()
      item = dev._alpha_long_toggle
      item.set_parent_rect(rl.Rectangle(0, 0, gui_app.width, gui_app.height))
      item._set_description_visible(True)
    else:
      from openpilot.selfdrive.ui.sunnypilot.mici.layouts.developer import DeveloperLayoutMiciSP
      dev = DeveloperLayoutMiciSP()
      dev.show_event()
      capture(dev, "_layout.png", frames=2)
      dev._scroller.scroll_to(dev._alpha_long_toggle.rect.x - dev._scroller.rect.x - 40)
      item = dev
    for text in STATES:
      ui_state.alpha_long_status = text
      capture(item, f"alpha_long_{text}.png")
    (screenshot_layouts.OUTPUT_DIR / "_layout.png").unlink(missing_ok=True)
    gui_app.close()
  return 0


if __name__ == "__main__":
  sys.exit(main())
