"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

The chestnut ModelState with the frame reduced on the way to the board.

A subclass, not a fork: stock ModelState is constructed for the reduced geometry
and does everything else exactly as it does today -- the pickle contract,
check_camera_jit, the fused warp graph on the board. Only the frame copy and the
warp matrix change, and both stay inside this class. The small model constructs
stock ModelState directly and never reaches here, so its behaviour is the
behaviour it had before this file existed.

manager runs modeld as PythonProcess("modeld", "openpilot.selfdrive.modeld.modeld"),
so importing ModelState from it here is the same module object, not a second copy.
"""
from __future__ import annotations

from collections.abc import Callable

import numpy as np
from msgq.visionipc import VisionBuf

from openpilot.selfdrive.modeld.compile_modeld import make_input_queues
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.selfdrive.modeld.modeld import ModelState
from openpilot.sunnypilot.accelerators.chestnut import reduce
from openpilot.system.camerad.cameras.nv12_info import get_nv12_info


class ChestnutModelState(ModelState):
  def __init__(self, cam_w: int, cam_h: int):
    self.cam_w, self.cam_h = cam_w, cam_h
    self.scale = reduce.pick_scale(cam_w, cam_h)
    out_w, out_h = reduce.reduced_size(cam_w, cam_h, self.scale)
    # the parent sizes its frame views and looks up its jit by the geometry it is
    # given, so handing it the reduced one is the whole of the build-side change
    super().__init__(out_w, out_h, True)
    # the warp samples a frame that was point-sampled by scale, so source pixel
    # coords scale with it. Applied here, so modeld's loop is untouched
    self._px_from_cam = np.diag([1. / self.scale] * 2 + [1.]).astype(np.float32)

  def _reduce_into(self, bufs: dict[str, VisionBuf]) -> None:
    for key, buf in bufs.items():
      src = np.frombuffer(buf.data, dtype=np.uint8)
      reduce.subsample_nv12(src, self.frame_views[key], self.cam_w, self.cam_h, self.scale)

  def run(self, bufs: dict[str, VisionBuf], transforms: dict[str, np.ndarray],
          inputs: dict[str, np.ndarray], after_enqueue: Callable[[], None] | None = None) -> dict[str, np.ndarray]:
    if self.scale == 1:
      return super().run(bufs, transforms, inputs, after_enqueue)
    self._reduce_into(bufs)
    transforms = {k: (self._px_from_cam @ v).astype(np.float32) for k, v in transforms.items()}
    # the frames are in the views already; an empty dict is what stops the parent
    # copying a full-size frame over them (its only use of bufs is that copy)
    return super().run({}, transforms, inputs, after_enqueue)

  def warmup(self) -> None:
    if self.scale == 1:
      super().warmup()
      return
    # full-size dummies, so they go through the reduction the real frames take
    yuv_size = get_nv12_info(self.cam_w, self.cam_h)[3]
    dummy = {k: np.zeros(yuv_size, dtype=np.uint8) for k in self.vision_input_names}
    eye = np.eye(3, dtype=np.float32)
    dims = {'desire_pulse': ModelConstants.DESIRE_LEN, 'traffic_convention': 2, 'action_t': 2}
    self.run(dummy, dict.fromkeys(self.vision_input_names, eye),
             {k: np.zeros(v, dtype=np.float32) for k, v in dims.items()})
    self.input_queues, self.npy, self.frame_views = make_input_queues(
      self.input_shapes, self.frame_skip, device=self.model_device, frame_copy_size=self.frame_copy_size)
    self.prev_desire[:] = 0
