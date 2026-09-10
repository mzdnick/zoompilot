"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
from openpilot.selfdrive.ui.mici.layouts.offroad_alerts import MiciOffroadAlerts
from openpilot.selfdrive.ui.sunnypilot.mici.layouts.software import ReleaseNotesPage
from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.system.ui.lib.application import gui_app

UPDATE_KEY = "UpdateAvailable"


class MiciOffroadAlertsSP(MiciOffroadAlerts):
  """Upstream's update alert reboots on a tap and sends users to comma's blog for the notes.

  Ours opens the notes of the release that is waiting, which carry the install slider.
  """

  def __init__(self):
    super().__init__()

    self._notes_page = ReleaseNotesPage()
    for item in self.alert_items:
      if item.alert_data.key == UPDATE_KEY:
        item.set_click_callback(self._show_release_notes)

  def _show_release_notes(self):
    notes = ui_state.params.get("UpdaterNewReleaseNotes")
    self._notes_page.set_notes((notes or b"").decode("utf-8", "replace").strip())
    gui_app.push_widget(self._notes_page)

  def _refresh(self, pending_params: dict) -> int:
    active_count = super()._refresh(pending_params)

    for item in self.alert_items:
      alert = item.alert_data
      if alert.key != UPDATE_KEY or not alert.visible:
        continue

      # "version / branch / commit / date", the same fields upstream puts in the alert
      desc = (pending_params.get("UpdaterNewDescription") or "").split(" / ")
      version = f"\nzoompilot {desc[0]}, {desc[3]}\n" if len(desc) == 4 else ""
      alert.text = f"Update ready{version}. Tap to read what's new."
      item.update_alert_data(alert)

    return active_count
