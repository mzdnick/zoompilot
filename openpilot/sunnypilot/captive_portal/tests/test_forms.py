"""
Copyright (c) 2026-, mzdnick.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

import unittest

from openpilot.sunnypilot.captive_portal.forms import best_form, parse_forms

LOGIN_PAGE = """
<html><body>
<form action="/login" method="POST">
  <input type="hidden" name="sessionid" value="abc123">
  <input type="text" name="username" value="">
  <input type="password" name="password">
  <input type="checkbox" name="accept" value="yes">
  <input type="submit" name="signin" value="Sign in">
</form>
</body></html>
"""


class TestParseForms(unittest.TestCase):

  def test_field_extraction(self):
    forms = parse_forms(LOGIN_PAGE, "http://portal.example/start")
    self.assertEqual(len(forms), 1)
    form = forms[0]
    self.assertEqual(form.action, "http://portal.example/login")
    self.assertEqual(form.method, "post")
    self.assertEqual([f.name for f in form.fields], ["sessionid", "username", "password", "accept", "signin"])
    self.assertEqual(form.fields[0].value, "abc123")

  def test_user_fields_skip_hidden_and_submit(self):
    form = parse_forms(LOGIN_PAGE, "http://portal.example/")[0]
    self.assertEqual([f.name for f in form.user_fields], ["username", "password", "accept"])

  def test_defaults_when_type_and_method_missing(self):
    forms = parse_forms('<form action="x"><input name="q"></form>', "http://a.example/")
    self.assertEqual(forms[0].method, "get")
    self.assertEqual(forms[0].fields[0].type, "text")

  def test_absolute_action_left_alone(self):
    form = parse_forms('<form action="https://other.example/auth">', "http://a.example/")[0]
    self.assertEqual(form.action, "https://other.example/auth")

  def test_unclosed_form_is_kept(self):
    form = parse_forms('<form action="/l"><input type="password" name="p">', "http://a.example/")[0]
    self.assertEqual(form.action, "http://a.example/l")
    self.assertEqual(form.user_fields[0].name, "p")

  def test_no_form(self):
    self.assertEqual(parse_forms("<html><body>javascript only</body></html>", "http://a.example/"), [])

  def test_best_form_prefers_password(self):
    html = """
    <form action="/search"><input name="q"></form>
    <form action="/login"><input name="user"><input type="password" name="pass"></form>
    """
    forms = parse_forms(html, "http://a.example/")
    self.assertEqual(best_form(forms).action, "http://a.example/login")

  def test_select_and_textarea_are_user_fields(self):
    html = '<form action="/l"><select name="room"></select><textarea name="note"></textarea></form>'
    form = parse_forms(html, "http://a.example/")[0]
    self.assertEqual([f.type for f in form.user_fields], ["select", "textarea"])


if __name__ == "__main__":
  unittest.main()
