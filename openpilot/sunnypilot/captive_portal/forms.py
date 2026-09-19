"""
Copyright (c) 2026-, mzdnick.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

from dataclasses import dataclass
from html.parser import HTMLParser
from urllib.parse import urljoin

# input types that carry no value the user edits
NON_USER_INPUT_TYPES = frozenset({"hidden", "submit", "button", "reset", "image"})


@dataclass(frozen=True)
class FormField:
  name: str
  type: str
  value: str


@dataclass(frozen=True)
class LoginForm:
  action: str
  method: str
  fields: tuple[FormField, ...] = ()

  @property
  def user_fields(self) -> tuple[FormField, ...]:
    return tuple(f for f in self.fields if f.name and f.type not in NON_USER_INPUT_TYPES)


class _FormParser(HTMLParser):
  # portal pages are routinely malformed; HTMLParser never raises on bad markup, and
  # an unterminated <form> is flushed on close() so it is not silently dropped
  def __init__(self):
    super().__init__(convert_charrefs=True)
    self.forms: list[dict] = []
    self._current: dict | None = None

  def handle_starttag(self, tag, attrs):
    a = {k.lower(): (v if v is not None else "") for k, v in attrs}
    if tag == "form":
      self._current = {"action": a.get("action", ""), "method": a.get("method", "get").lower(), "fields": []}
    elif self._current is not None and tag in ("input", "textarea", "select"):
      field_type = a.get("type", "text").lower() if tag == "input" else tag
      self._current["fields"].append(FormField(a.get("name", ""), field_type, a.get("value", "")))

  def handle_endtag(self, tag):
    if tag == "form" and self._current is not None:
      self.forms.append(self._current)
      self._current = None

  def close(self):
    super().close()
    if self._current is not None:
      self.forms.append(self._current)
      self._current = None


def parse_forms(html_text: str, base_url: str) -> list[LoginForm]:
  """Extract every HTML form, with the form action resolved against base_url."""
  parser = _FormParser()
  parser.feed(html_text)
  parser.close()
  return [LoginForm(urljoin(base_url, f["action"]), f["method"], tuple(f["fields"])) for f in parser.forms]


def best_form(forms: list[LoginForm]) -> LoginForm | None:
  """Pick the form most likely to be the login form: first one with a password field, else the first with a user field."""
  for form in forms:
    if any(f.type == "password" for f in form.user_fields):
      return form
  for form in forms:
    if form.user_fields:
      return form
  return forms[0] if forms else None
