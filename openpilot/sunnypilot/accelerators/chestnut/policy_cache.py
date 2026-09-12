"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

The board-side policy JIT: the fused graph of #38684 with the warp taken out.

compile_modeld is imported, never edited: the small model's pickle has to stay
byte-identical, and the cheapest way to guarantee that is for its build to run
the same code on the same inputs. Everything here is additive.

The warped tensor is model-sized whatever the camera is, so unlike run_model
this JIT is not keyed by camera resolution -- one pickle serves every device
that can run it.
"""
from __future__ import annotations

import math

import numpy as np

from tinygrad.tensor import Tensor
from tinygrad.device import Device

from openpilot.selfdrive.modeld.compile_modeld import get_policy_npy_shapes

# what the warp hands over: (2, 6, model_h/2, model_w/2) uint8, both cameras
WARPED_CAMERAS = 2
WARPED_CHANNELS = 6


def warped_shape(input_shapes) -> tuple[int, int, int, int]:
  img = input_shapes['img']  # (1, 12, 128, 256)
  return (WARPED_CAMERAS, WARPED_CHANNELS, img[2], img[3])


def warped_nbytes(input_shapes) -> int:
  return math.prod(warped_shape(input_shapes))


def make_policy_queues(input_shapes, frame_skip, device, warped_bytes):
  """make_input_queues' queues, with the warped crop where the two frames were.

  tfm/big_tfm are gone from the packed block: they are the warp's inputs and
  the warp no longer runs on this device.
  """
  img = input_shapes['img']
  fb = input_shapes['features_buffer']
  dp = input_shapes['desire_pulse']
  feat_dim = math.prod(fb[2:])
  n_frames = img[1] // 6
  img_buf_shape = (frame_skip * (n_frames - 1) + 1, 6, img[2], img[3])

  shapes, sizes = get_policy_npy_shapes(input_shapes)
  packed_npy_size = sum(sizes) * np.dtype(np.float32).itemsize
  packed_input = np.zeros(packed_npy_size + warped_bytes, dtype=np.uint8)
  packed_npy_inputs = packed_input[:packed_npy_size].view(np.float32)
  npy = {k: v.reshape(s) for (k, s), v in
         zip(shapes.items(), np.split(packed_npy_inputs, np.cumsum(sizes[:-1])), strict=True)}
  input_queues = {
    'img_q': Tensor(np.zeros(img_buf_shape, dtype=np.uint8), device=device).contiguous().realize(),
    'big_img_q': Tensor(np.zeros(img_buf_shape, dtype=np.uint8), device=device).contiguous().realize(),
    'feat_q': Tensor(np.zeros((frame_skip * fb[1], fb[0], feat_dim), dtype=np.float32), device=device).contiguous().realize(),
    'desire_q': Tensor(np.zeros((frame_skip * dp[1], dp[0], dp[2]), dtype=np.float32), device=device).contiguous().realize(),
    'packed_npy_inputs': Tensor(packed_input, device='NPY').realize(),
  }
  return input_queues, npy, {'warped': packed_input[packed_npy_size:]}


def make_run_policy_only(run_policy, input_shapes):
  """One copyin per frame, same as the fused graph; the tail is the crop, not frames."""
  _, policy_sizes = get_policy_npy_shapes(input_shapes)
  packed_npy_size = sum(policy_sizes) * np.dtype(np.float32).itemsize
  shape = warped_shape(input_shapes)

  def run_model(img_q, big_img_q, feat_q, desire_q, packed_npy_inputs):
    packed = packed_npy_inputs.to(Device.DEFAULT)
    Tensor.realize(packed)
    npy = packed[:packed_npy_size].bitcast('float32')
    warped = packed[packed_npy_size:].reshape(*shape)
    return run_policy(warped, img_q, big_img_q, feat_q, desire_q, npy)

  return run_model
