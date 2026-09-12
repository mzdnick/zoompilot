"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

Point-sampling a camera frame down before it crosses USB to the chestnut.

Since #38684 the warp runs on the board, so both full frames cross the link
every frame. On a 3X that is 7.47 MB against a 5 Gbps host controller, ~22 ms,
and it does not fit the 20 Hz budget once the model runs.

Reducing here rather than in camerad is deliberate: modeld is the only process
that knows which model is running, so the reduction lives and dies with the big
model. camerad, VisionIPC, the encoders, DEVICE_CAMERAS and FrameData are all
untouched, recordings stay native, and a fallback to the small model reads the
native frame on the very next frame with no camera restart.

Point-sampling, not a box filter, and that is the quality argument: the warp
already point-samples the native frame with Tensor.round() at 2.91x on the 3X
road camera. Subsampling first and warping at 1.45x is still one point sample
of the same original lattice, so aliasing is unchanged from what ships today.
The warp snaps to the nearest retained pixel, a sub-pixel shift at that ratio.
A box filter would be strictly better and needs a real kernel; this needs none.

Nothing heavy is imported: modeld/SConscript loads this by file at build time
to pick the same scale the runtime will.
"""
from __future__ import annotations

import numpy as np

from openpilot.system.camerad.cameras.nv12_info import get_nv12_info

# Per-frame budget for the link. One frame per camera crosses per model run, and
# over USB 3 Gen1 anything past ~2 MB/frame puts the transfer beyond what fits
# alongside the model in 50 ms. A 3X frame (3.74 MB) is over it and is reduced to
# 0.93 MB; a comma 4 frame (1.62 MB) is under it and is left alone.
FRAME_BUDGET_BYTES = 2_000_000
MAX_SCALE = 4


def copy_size(w: int, h: int) -> int:
  """The bytes of an NV12 frame modeld hands the model device: padded planes, no guard."""
  stride, y_height, uv_height, _ = get_nv12_info(w, h)
  return stride * (y_height + uv_height)


def pick_scale(cam_w: int, cam_h: int, budget_bytes: int = FRAME_BUDGET_BYTES) -> int:
  """Smallest integer reduction whose frame fits the budget, or 1 if it already does.

  Per camera resolution, not per device, so a build carrying several cameras
  gets the right answer for each. Scales that would not divide the frame evenly
  are skipped: the truncation would drop a row or column and take the warp
  matrix with it.
  """
  if budget_bytes <= 0:
    return 1
  best = 1
  for s in range(1, MAX_SCALE + 1):
    if cam_w % s or cam_h % s:
      continue
    best = s
    if copy_size(cam_w // s, cam_h // s) <= budget_bytes:
      return s
  return best  # nothing fits; take the most we can and let the caller's log show it


def reduced_size(cam_w: int, cam_h: int, scale: int) -> tuple[int, int]:
  return cam_w // scale, cam_h // scale


def subsample_nv12(src: np.ndarray, dst: np.ndarray, cam_w: int, cam_h: int, scale: int) -> None:
  """Write src, an NV12 frame, into dst reduced by `scale`. Two strided copies, no temporaries.

  dst is the flat frame view make_input_queues handed us, sized copy_size() of
  the reduced geometry.
  """
  s = scale
  s_stride, s_y_h, _, _ = get_nv12_info(cam_w, cam_h)
  out_w, out_h = reduced_size(cam_w, cam_h, s)
  d_stride, d_y_h, d_uv_h, _ = get_nv12_info(out_w, out_h)
  planes = dst.reshape(d_y_h + d_uv_h, d_stride)

  y = src[:s_y_h * s_stride].reshape(s_y_h, s_stride)
  planes[:out_h, :out_w] = y[:cam_h:s, :cam_w:s]

  # the UV plane is half height and UVUV interleaved, so a pair is the unit:
  # keep every s-th row and every s-th pair, and the pair stays whole
  uv_rows = cam_h // 2
  uv = src[s_stride * s_y_h:s_stride * (s_y_h + uv_rows)].reshape(uv_rows, s_stride)
  pairs = uv[:uv_rows:s, :cam_w].reshape(uv_rows // s, cam_w // 2, 2)[:, ::s, :]
  planes[d_y_h:d_y_h + out_h // 2, :out_w] = pairs.reshape(out_h // 2, out_w)
