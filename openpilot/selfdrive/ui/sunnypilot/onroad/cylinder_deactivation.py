"""
Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.

This file is part of sunnypilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
import time

import pyray as rl

from openpilot.cereal import custom

# The element draws itself only while a state other than "all cylinders" is live, then
# lingers and fades. Cars without deactivation hardware never leave normal, so the
# element stays hidden without a setting.
VISIBLE_LINGER_S = 2.0
FADE_S = 0.5

# Ring gauge anchored in a corner of the road view. Defaults fit tici/tizi:
# lower-left, high enough to clear the bottom developer UI strip (60 px) and
# the bottom-center torque bar arc. Mici passes smaller radii centered on its
# steering-wheel indicator instead.
RING_INNER_R = 44.0
RING_OUTER_R = 60.0
CORNER_X = 125.0
CORNER_Y = 125.0
FUEL_CUT_GAP_DEG = 8.0

STATE = custom.CarStateZP.CylinderDeactivation.State

RING_ACTIVE = rl.Color(0x4f, 0xd0, 0x84, 0xff)   # deactivation latched (two cylinders firing)
RING_TRACK = rl.Color(0x5d, 0x6b, 0x74, 0x5a)    # gauge track during the entry ramp
RING_CUT = rl.Color(0x53, 0x9f, 0xc7, 0xff)      # fuel cut: no cylinder fires
BACKING_OUTER = rl.Color(0, 0, 0, 0x5a)
BACKING_INNER = rl.Color(0, 0, 0, 0x78)


def _alpha(color: rl.Color, a: int) -> rl.Color:
  return rl.Color(color.r, color.g, color.b, color.a * a // 255)


class CylinderDeactivationRenderer:
  def __init__(self, inner_r: float = RING_INNER_R, outer_r: float = RING_OUTER_R,
               corner_x: float = CORNER_X, corner_y: float = CORNER_Y, backing: bool = True):
    self._inner_r = inner_r
    self._outer_r = outer_r
    self._corner_x = corner_x
    self._corner_y = corner_y
    self._backing = backing
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

    cx = int(rect.x + self._corner_x)
    cy = int(rect.y + rect.height - self._corner_y)
    center = rl.Vector2(cx, cy)

    # soft backing so the ring reads over bright road
    if self._backing:
      rl.draw_circle(cx, cy, self._outer_r + 16, _alpha(BACKING_OUTER, a))
      rl.draw_circle(cx, cy, self._outer_r + 6, _alpha(BACKING_INNER, a))

    if state == STATE.engineBraking:
      # one dashed segment per cut cylinder
      span = 360 / 4
      for k in range(4):
        a0 = -90 + k * span + FUEL_CUT_GAP_DEG
        a1 = -90 + (k + 1) * span - FUEL_CUT_GAP_DEG
        rl.draw_ring(center, self._inner_r, self._outer_r, a0, a1, 24, _alpha(RING_CUT, a))
    elif state == STATE.entry:
      # the arc sweeps clockwise from 12 o'clock as the PCM confirmation ramp progresses
      rl.draw_ring(center, self._inner_r, self._outer_r, 0, 360, 64, _alpha(RING_TRACK, a))
      sweep = 360 * max(0.0, min(1.0, float(cd.entryProgress)))
      if sweep > 0:
        rl.draw_ring(center, self._inner_r, self._outer_r, -90, -90 + sweep, 64, _alpha(RING_ACTIVE, a))
    else:
      # deactivated, and the full-ring linger while the return to normal fades out
      rl.draw_ring(center, self._inner_r, self._outer_r, 0, 360, 64, _alpha(RING_ACTIVE, a))
