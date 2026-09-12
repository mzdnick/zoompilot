"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

The chestnut ModelState with the warp on QCOM.

Duck-types selfdrive.modeld.modeld.ModelState the way JetlinkModelState does,
so stock ModelState is left exactly as it was and is only ever built for the
small model. Same division of labour as jetlink: warp here, policy over the
link, image history on the far side.
"""
from __future__ import annotations

import os
from collections.abc import Callable

import numpy as np

from tinygrad.device import Device
from tinygrad.tensor import Tensor
from msgq.visionipc import VisionBuf

from openpilot.common.file_chunker import open_file_chunked
from openpilot.selfdrive.modeld.compile_modeld import MODELD_INPUTS
from openpilot.selfdrive.modeld.constants import ModelConstants
from openpilot.selfdrive.modeld.helpers import load_oob, modeld_pkl_path
from openpilot.selfdrive.modeld.parse_model_outputs import Parser
from openpilot.sunnypilot.accelerators.chestnut.compile_policy import check_chestnut_pkl
from openpilot.sunnypilot.accelerators.chestnut.policy_cache import (make_policy_queues, warped_nbytes)
from openpilot.sunnypilot.accelerators.jetlink import warp_cache
from openpilot.sunnypilot.modeld_v2.modeld_base import ModelStateBase
from openpilot.system.camerad.cameras.nv12_info import get_nv12_info

SEND_RAW_PRED = os.getenv('SEND_RAW_PRED')


class ChestnutModelState(ModelStateBase):
  prev_desire: np.ndarray  # for tracking the rising edge of the pulse

  def __init__(self, cam_w: int, cam_h: int, warp=None):
    ModelStateBase.__init__(self)
    self.chestnut = True

    pkl_path = modeld_pkl_path(True)
    jits = load_oob(open_file_chunked(pkl_path))
    check_chestnut_pkl(jits, pkl_path)
    self.model_device = jits['input_devices']['model']
    metadata = jits['metadata']
    self.input_shapes = metadata['input_shapes']
    self.output_slices = metadata['output_slices']
    self.vision_input_names = [k for k in self.input_shapes if 'img' in k]
    self.run_policy = jits['run_policy']

    # make_warp is sized in NV12 pixels and the model input after deinterleave,
    # so img (1, 12, 128, 256) is a 512x256 warp, MEDMODEL_INPUT_SIZE
    img_h, img_w = self.input_shapes['img'][2:]
    self.warp = warp if warp is not None else warp_cache.load_warp(cam_w, cam_h, img_w * 2, img_h * 2)
    # read once: it must be the device the cached JIT was compiled against
    self.warp_dev = Device.DEFAULT

    self.frame_skip = ModelConstants.MODEL_RUN_FREQ // ModelConstants.MODEL_CONTEXT_FREQ
    self.input_queues, self.npy, views = make_policy_queues(
      self.input_shapes, self.frame_skip, device=self.model_device,
      warped_bytes=warped_nbytes(self.input_shapes))
    self.warped_view = views['warped']

    # the warp's own two inputs stay separate NPY tensors: they are consumed on
    # QCOM and never cross the link
    self.tfm_npy = {'tfm': np.zeros((3, 3), dtype=np.float32), 'big_tfm': np.zeros((3, 3), dtype=np.float32)}
    self.warp_inputs = {k: Tensor(v, device='NPY').realize() for k, v in self.tfm_npy.items()}

    self.prev_desire = np.zeros(ModelConstants.DESIRE_LEN, dtype=np.float32)
    self.parser = Parser()
    self.yuv_size = get_nv12_info(cam_w, cam_h)[3]
    self.full_frames: dict[str, Tensor] = {}
    self._blob_cache: dict[tuple[str, int], Tensor] = {}

  def slice_outputs(self, model_outputs: np.ndarray, output_slices: dict[str, slice]) -> dict[str, np.ndarray]:
    return {k: model_outputs[np.newaxis, v] for k, v in output_slices.items()}

  def run(self, bufs: dict[str, VisionBuf], transforms: dict[str, np.ndarray],
          inputs: dict[str, np.ndarray], after_enqueue: Callable[[], None] | None = None) -> dict[str, np.ndarray]:
    for key in bufs.keys():
      ptr = np.frombuffer(bufs[key].data, dtype=np.uint8).ctypes.data
      # there is a ringbuffer of imgs, just cache tensors pointing to all of them
      cache_key = (key, ptr)
      if cache_key not in self._blob_cache:
        self._blob_cache[cache_key] = Tensor.from_blob(ptr, (self.yuv_size,), dtype='uint8', device=self.warp_dev)
      self.full_frames[key] = self._blob_cache[cache_key]

    # Model decides when action is completed, so desire input is just a pulse triggered on rising edge
    inputs['desire_pulse'][0] = 0
    self.npy['desire'][:] = np.where(inputs['desire_pulse'] - self.prev_desire > .99, inputs['desire_pulse'], 0)
    self.prev_desire[:] = inputs['desire_pulse']
    self.npy['traffic_convention'][:] = inputs['traffic_convention']
    self.npy['action_t'][:] = inputs['action_t']
    self.tfm_npy['tfm'][:, :] = transforms['img'][:, :]
    self.tfm_npy['big_tfm'][:, :] = transforms['big_img'][:, :]

    warped = warp_cache.call_warp(self.warp, **self.warp_inputs,
                                  frame=self.full_frames['img'], big_frame=self.full_frames['big_img'])
    # .data() rather than .numpy(): the same write-combined GPU mapping read,
    # but no per-frame allocation and no 52 ms outlier (see jetlink model_state)
    self.warped_view[:] = np.frombuffer(warped.data(), dtype=np.uint8)

    outs, = self.run_policy(**{k: self.input_queues[k] for k in MODELD_INPUTS})
    if after_enqueue is not None:
      after_enqueue()
    model_output = outs.numpy()[0]
    if not np.all(np.isfinite(model_output)):
      raise RuntimeError("model output not finite")
    outputs_dict = self.parser.parse_outputs(self.slice_outputs(model_output, self.output_slices))
    self.npy['prev_feat'][:] = model_output[self.output_slices['hidden_state']]

    if SEND_RAW_PRED:
      outputs_dict['raw_pred'] = model_output.copy()
    return outputs_dict

  def warmup(self) -> None:
    dummy_frames = {k: np.zeros(self.yuv_size, dtype=np.uint8) for k in self.vision_input_names}
    eye = np.eye(3, dtype=np.float32)
    dims = {'desire_pulse': ModelConstants.DESIRE_LEN, 'traffic_convention': 2, 'action_t': 2}
    self.run(dummy_frames, dict.fromkeys(self.vision_input_names, eye),
             {k: np.zeros(v, dtype=np.float32) for k, v in dims.items()})
    # the dummies are about to be freed; nothing may keep a tensor into them
    self._blob_cache.clear()
    self.full_frames.clear()
    self.input_queues, self.npy, views = make_policy_queues(
      self.input_shapes, self.frame_skip, device=self.model_device,
      warped_bytes=warped_nbytes(self.input_shapes))
    self.warped_view = views['warped']
    self.prev_desire[:] = 0
