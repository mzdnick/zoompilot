"""
Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.

This file is part of sunnypilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
import time

import pyray as rl

from openpilot.cereal import custom
from openpilot.selfdrive.ui import UI_BORDER_SIZE
from openpilot.selfdrive.ui.onroad.driver_state import BTN_SIZE

# The element draws itself only while a state other than "all cylinders" is live, then
# lingers and fades. Cars without deactivation hardware never leave normal, so the
# element stays hidden without a setting.
VISIBLE_LINGER_S = 2.0
FADE_S = 0.5

# The tici ring wraps the driver-monitoring circle: same center offset, sized to
# clear the BTN_SIZE face circle with a small gap. The band backing keeps the
# gauge readable over the road without darkening the face. Mici passes smaller
# radii centered on its steering-wheel icon and no backing.
_DM_RADIUS = BTN_SIZE // 2
_DM_CENTER_OFFSET = UI_BORDER_SIZE + _DM_RADIUS
RING_INNER_R = float(_DM_RADIUS + 8)
RING_OUTER_R = float(_DM_RADIUS + 24)
CORNER_X = float(_DM_CENTER_OFFSET)
CORNER_Y = float(_DM_CENTER_OFFSET)

# Turbine encoding: clockwise motion means load, counter-clockwise means drag.
# The latch holds two opposite quadrant arcs (2 of 4 cylinders); the exit
# linger closes them clockwise into a full ring ("all four firing again").
QUAD_DEG = 90.0
LATCH_ARCS = ((-90.0, 0.0), (90.0, 180.0))  # raylib angles: top-right, bottom-left
CUT_DASH_COUNT = 4
CUT_SPAN_DEG = 45.0
CUT_PERIOD_S = 3.0     # seconds per counter-clockwise revolution
LATCH_EXPAND_S = 0.3   # linger time to close the two arcs into a full ring

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
               corner_x: float = CORNER_X, corner_y: float = CORNER_Y,
               backing: str | None = None):
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

    # Linger + fade disabled: the element hides as soon as the engine returns
    # to all-cylinder operation. Restore this block to bring them back.
    # a = 255
    # if state != STATE.normal:
    #   self._last_active_t = now
    # else:
    #   since = now - self._last_active_t
    #   if since > VISIBLE_LINGER_S + FADE_S:
    #     return
    #   if since > VISIBLE_LINGER_S:
    #     a = int(255 * (1.0 - (since - VISIBLE_LINGER_S) / FADE_S))
    if state == STATE.normal:
      return

    a = 255
    cx = int(rect.x + self._corner_x)
    cy = int(rect.y + rect.height - self._corner_y)
    center = rl.Vector2(cx, cy)

    # Band backing removed: bare geometry in every state. Restore this block
    # (and pass backing="band") if daylight contrast is wanted again.
    # if self._backing == "band":
    #   rl.draw_ring(center, max(0.0, self._inner_r - 6), self._outer_r + 6, 0, 360, 64, _alpha(BACKING_OUTER, a))
    # elif self._backing == "disc":
    #   rl.draw_circle(cx, cy, self._outer_r + 16, _alpha(BACKING_OUTER, a))
    #   rl.draw_circle(cx, cy, self._outer_r + 6, _alpha(BACKING_INNER, a))

    if state == STATE.entry:
      # two arcs grow clockwise from 12 and 6 o'clock as the PCM ramp progresses,
      # fading in with the ramp and reaching full brightness at the latch
      p = max(0.0, min(1.0, float(cd.entryProgress)))
      # track removed: only the growing bars show
      # rl.draw_ring(center, self._inner_r, self._outer_r, 0, 360, 64, _alpha(RING_TRACK, a))
      if p > 0:
        entry_a = int(a * p)
        rl.draw_ring(center, self._inner_r, self._outer_r, -90, -90 + QUAD_DEG * p, 24, _alpha(RING_ACTIVE, entry_a))
        rl.draw_ring(center, self._inner_r, self._outer_r, 90, 90 + QUAD_DEG * p, 24, _alpha(RING_ACTIVE, entry_a))
    elif state == STATE.deactivated:
      for a0, a1 in LATCH_ARCS:
        rl.draw_ring(center, self._inner_r, self._outer_r, a0, a1, 24, _alpha(RING_ACTIVE, a))
    elif state == STATE.engineBraking:
      # four dashes spinning counter-clockwise: drag, not load
      off = -360.0 * (now % CUT_PERIOD_S) / CUT_PERIOD_S
      for k in range(CUT_DASH_COUNT):
        a0 = -90 + 90 * k + off
        rl.draw_ring(center, self._inner_r, self._outer_r, a0, a0 + CUT_SPAN_DEG, 16, _alpha(RING_CUT, a))
    # Linger + fade disabled (see top of render): the latched arcs used to close
    # clockwise into a full ring here, held for VISIBLE_LINGER_S, then faded.
    # elif since < LATCH_EXPAND_S:
    #   q = since / LATCH_EXPAND_S
    #   rl.draw_ring(center, self._inner_r, self._outer_r, -90, QUAD_DEG * q, 24, _alpha(RING_ACTIVE, a))
    #   rl.draw_ring(center, self._inner_r, self._outer_r, 90, 180 + QUAD_DEG * q, 24, _alpha(RING_ACTIVE, a))
    # else:
    #   rl.draw_ring(center, self._inner_r, self._outer_r, 0, 360, 64, _alpha(RING_ACTIVE, a))
