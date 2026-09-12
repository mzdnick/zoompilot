#!/usr/bin/env python3
"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

Build the board-side policy JIT. Invoked by modeld/SConscript in place of
compile_modeld.py for the chestnut target on tici/tizi.

Mirrors compile_modeld's __main__ and imports everything real from it, so the
small model's build is untouched and its pickle is byte-identical.
"""
import argparse
import os
from functools import partial

from openpilot.selfdrive.modeld.compile_modeld import (MODELD_INPUTS, _parse_size, compile_jit,
                                                       make_run_policy, read_file_chunked_to_disk)
from openpilot.selfdrive.modeld.helpers import dump_oob, load_oob
from openpilot.sunnypilot.accelerators.chestnut.policy_cache import (make_policy_queues, make_run_policy_only,
                                                                     warped_nbytes, warped_shape)

# the top level of the pickle this writes. Deliberately not MODELD_PKL_KEYS:
# that contract belongs to compile_modeld's pickle, which the small model also
# uses, and widening it would force a rebuild on every small-model device
CHESTNUT_PKL_KEYS = ('metadata', 'input_devices', 'run_policy')


def check_chestnut_pkl(jits: dict, path) -> None:
  missing = [k for k in CHESTNUT_PKL_KEYS if k not in jits]
  if missing:
    raise RuntimeError(f"{path} is missing {missing}: it was compiled by a different compile_policy.py "
                       "than this modeld, rebuild it")


if __name__ == "__main__":
  from tinygrad.device import Device
  from tinygrad.engine.jit import TinyJit
  from openpilot.selfdrive.modeld.get_model_metadata import make_metadata_dict

  p = argparse.ArgumentParser()
  p.add_argument('--model-size', type=_parse_size, required=True, help='model input WxH, for the log line only')
  p.add_argument('--onnx', required=True)
  p.add_argument('--output', required=True)
  p.add_argument('--frame-skip', type=int, required=True)
  p.add_argument('--benchmark-runs', type=int, default=1)
  args = p.parse_args()

  model_path = read_file_chunked_to_disk(args.onnx)
  from tinygrad.nn.onnx import OnnxRunner
  model_runner = OnnxRunner(model_path)

  metadata = make_metadata_dict(model_path)
  input_shapes = metadata['input_shapes']
  print(f"policy JIT for {args.model_size[0]}x{args.model_size[1]}, warped input {warped_shape(input_shapes)}, "
        f"{warped_nbytes(input_shapes)/1e6:.3f} MB per frame over the link")

  run_policy = make_run_policy(model_runner, metadata, args.frame_skip)
  jit = TinyJit(make_run_policy_only(run_policy, input_shapes), prune=True)
  make_queues = partial(make_policy_queues, input_shapes, args.frame_skip,
                        warped_bytes=warped_nbytes(input_shapes))

  out = {
    'metadata': metadata,
    'input_devices': {'model': Device.DEFAULT},
    'run_policy': compile_jit(jit, MODELD_INPUTS, make_queues, args.benchmark_runs),
  }
  assert set(out) == set(CHESTNUT_PKL_KEYS), "model_state reads CHESTNUT_PKL_KEYS, keep it in step"

  with open(args.output, "wb") as f:
    dump_oob(out, f)
  with open(args.output, "rb") as f:
    load_oob(f)
    assert not f.read(1), "unexpected model buffer data"
  print(f"Saved policy JIT to {args.output} ({os.path.getsize(args.output) / 1e6:.2f} MB)")
