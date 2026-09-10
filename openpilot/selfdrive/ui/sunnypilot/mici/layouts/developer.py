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

  def _update_state(self):
    super()._update_state()
    self._alpha_long_toggle.set_value(ui_state.alpha_long_status)
