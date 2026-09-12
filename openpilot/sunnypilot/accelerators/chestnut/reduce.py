"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

Area-averaging a camera frame down before it crosses USB to the chestnut.

Since #38684 the warp runs on the board, so both full frames cross the link
every frame. On a 3X that is 7.47 MB against a 5 Gbps host controller, ~22 ms,
and it does not fit the 20 Hz budget once the model runs.

Reducing here rather than in camerad is deliberate: modeld is the only process
that knows which model is running, so the reduction lives and dies with the big
model. camerad, VisionIPC, the encoders, DEVICE_CAMERAS and FrameData are all
untouched, recordings stay native, and a fallback to the small model reads the
native frame on the very next frame with no camera restart.

A 2x2 average, not a point sample, and the wide camera is why. At scale 2 the
3X fisheye lands at 0.62 source px per model px, so the warp magnifies and a
point sample would discard three quarters of the photons before the warp ever
runs -- a 4x decimation with no prefilter, which is aliasing on exactly the
high-frequency content a fisheye sees. Averaging cannot give the resolution
back but it keeps the signal: every source pixel contributes.

Written as four strided adds on purpose. The obvious
reshape(h//2,2,w//2,2).sum((1,3)) builds full-size uint32 temporaries and costs
around 18 ms per frame; this costs around 1.5 ms for the same result.

Nothing heavy is imported: modeld/SConscript loads this at build time to pick
the same scale the runtime will.
"""
from __future__ import annotations

import numpy as np

from openpilot.system.camerad.cameras.nv12_info import get_nv12_info

# Per-frame budget for the link. One frame per camera crosses per model run, and
# over USB 3 Gen1 anything past ~2 MB/frame puts the transfer beyond what fits
# alongside the model in 50 ms. A 3X frame (3.74 MB) is over it and is reduced to
# 0.93 MB; a comma 4 frame (1.62 MB) is under it and is left alone.
FRAME_BUDGET_BYTES = 2_000_000
# powers of two only: the reduction is repeated halving, and an odd factor would
# not divide the frame evenly anyway on either camera
SCALES = (1, 2, 4)


def copy_size(w: int, h: int) -> int:
  """The bytes of an NV12 frame modeld hands the model device: padded planes, no guard."""
  stride, y_height, uv_height, _ = get_nv12_info(w, h)
  return stride * (y_height + uv_height)


def pick_scale(cam_w: int, cam_h: int, budget_bytes: int = FRAME_BUDGET_BYTES) -> int:
  """Smallest reduction whose frame fits the budget, or 1 if it already does.

  Per camera resolution, not per device, so a build carrying several cameras gets
  the right answer for each. A scale that would not divide the frame evenly is
  skipped: the truncation would drop a row or column and take the warp matrix
  with it.
  """
  if budget_bytes <= 0:
    return 1
  best = 1
  for s in SCALES:
    if cam_w % s or cam_h % s or (cam_w // s) % 2 or (cam_h // s) % 2:
      continue
    best = s
    if copy_size(cam_w // s, cam_h // s) <= budget_bytes:
      return s
  return best  # nothing fits; take the most we can and let the caller's log show it


def reduced_size(cam_w: int, cam_h: int, scale: int) -> tuple[int, int]:
  return cam_w // scale, cam_h // scale


def _box2(a: np.ndarray) -> np.ndarray:
  """Rounded mean of each 2x2, as four strided adds. uint16 holds 4*255 without saturating."""
  return ((a[0::2, 0::2].astype(np.uint16) + a[0::2, 1::2] +
           a[1::2, 0::2] + a[1::2, 1::2] + 2) >> 2).astype(np.uint8)


def _reduce_plane(a: np.ndarray, scale: int) -> np.ndarray:
  while scale > 1:
    a = _box2(a)
    scale //= 2
  return a


def box_nv12(src: np.ndarray, dst: np.ndarray, cam_w: int, cam_h: int, scale: int) -> None:
  """Write src, an NV12 frame, into dst reduced by `scale` with a 2x2 area average.

  dst is the flat frame view make_input_queues handed us, sized copy_size() of the
  reduced geometry. Y and the two chroma planes are averaged independently; the
  UV plane is half height and UVUV interleaved, so a pair is the unit.
  """
  s_stride, s_y_h, _, _ = get_nv12_info(cam_w, cam_h)
  out_w, out_h = reduced_size(cam_w, cam_h, scale)
  d_stride, d_y_h, d_uv_h, _ = get_nv12_info(out_w, out_h)
  planes = dst.reshape(d_y_h + d_uv_h, d_stride)

  y = src[:s_y_h * s_stride].reshape(s_y_h, s_stride)[:cam_h, :cam_w]
  planes[:out_h, :out_w] = _reduce_plane(y, scale)

  uv_rows = cam_h // 2
  uv = src[s_stride * s_y_h:s_stride * (s_y_h + uv_rows)].reshape(uv_rows, s_stride)
  uv = uv[:, :cam_w].reshape(uv_rows, cam_w // 2, 2)
  u = _reduce_plane(uv[..., 0], scale)
  v = _reduce_plane(uv[..., 1], scale)
  planes[d_y_h:d_y_h + out_h // 2, :out_w] = np.stack((u, v), axis=-1).reshape(out_h // 2, out_w)
