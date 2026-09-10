"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
import pyray as rl

from openpilot.selfdrive.ui.mici.layouts.settings.software import InstallUpdateButton, SoftwareLayoutMici
from openpilot.selfdrive.ui.mici.widgets.button import BigButton
from openpilot.selfdrive.ui.ui_state import ui_state
from openpilot.system.ui.lib.application import gui_app
from openpilot.system.ui.widgets.html_render import LIST_INDENT_PX, ElementType, HtmlRenderer
from openpilot.system.ui.widgets.scroller import NavRawScrollPanel

NO_NOTES = "<h2>No release notes available.</h2>"
NOTES_FONT_SIZE = 28  # the renderer defaults to the tici screen, mici is 536x240


class ReleaseNotesPage(NavRawScrollPanel):
  """Scrolling page for the notes updated writes to params: the top CHANGELOG.md entry, as html."""

  def __init__(self):
    super().__init__()
    self._content = HtmlRenderer(text=NO_NOTES, text_size={ElementType.P: NOTES_FONT_SIZE})

  def set_notes(self, notes: str):
    self._content.parse_html_content(notes or NO_NOTES)

  def _render(self, rect: rl.Rectangle):
    # the renderer indents a nested list without narrowing its wrap width, so leave the room here
    width = rect.width - LIST_INDENT_PX
    content_height = self._content.get_total_height(int(width))
    scroll_offset = round(self._scroll_panel.update(rect, content_height))
    self._content.render(rl.Rectangle(rect.x, rect.y + scroll_offset, width, content_height))


class ReleaseNotesButton(BigButton):
  def __init__(self):
    super().__init__("release notes", "", gui_app.texture("icons_mici/settings/device/info.png", 64, 64))
    self._page = ReleaseNotesPage()
    self.set_click_callback(self._on_click)

  @staticmethod
  def _pending() -> bool:
    return ui_state.params.get_bool("UpdateAvailable")

  def _update_state(self):
    super()._update_state()

    # "version / branch / commit / date"
    desc = ui_state.params.get("UpdaterNewDescription" if self._pending() else "UpdaterCurrentDescription") or ""
    value = desc.split(" / ")[0].strip()
    if self.get_value() != value:
      self.set_value(value)

  def _on_click(self):
    notes = ui_state.params.get("UpdaterNewReleaseNotes" if self._pending() else "UpdaterCurrentReleaseNotes")
    self._page.set_notes((notes or b"").decode("utf-8", "replace").strip())
    gui_app.push_widget(self._page)


class SoftwareLayoutSP(SoftwareLayoutMici):
  def __init__(self):
    super().__init__()

    # add_widget wraps the touch callback, so append and then move it up behind "install now"
    self._scroller.add_widget(ReleaseNotesButton())
    items = self._scroller.items
    install = next((i for i, item in enumerate(items) if isinstance(item, InstallUpdateButton)), len(items) - 2)
    items.insert(install + 1, items.pop())
