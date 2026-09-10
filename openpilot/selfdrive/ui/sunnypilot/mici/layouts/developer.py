"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
from openpilot.selfdrive.ui.mici.layouts.settings.developer import DeveloperLayoutMici
from openpilot.selfdrive.ui.ui_state import ui_state


class DeveloperLayoutMiciSP(DeveloperLayoutMici):
  """The alpha longitudinal switch is the saved preference (offroad-only); its subtitle is the
  running session's mode: initializing, ready or failed. Same line as the tizi description."""

  SUBTITLE_FONT_SIZE = 24  # the sentence-length subtitle the sunnypilot cards use

  def __init__(self):
    super().__init__()
    self._alpha_long_toggle._sub_label.set_font_size(self.SUBTITLE_FONT_SIZE)

  def _update_state(self):
    super()._update_state()
    if ui_state.alpha_long_status != self._alpha_long_toggle.value:
      self._alpha_long_toggle.set_value(ui_state.alpha_long_status)
