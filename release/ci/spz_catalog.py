#!/usr/bin/env python3
"""Build the spz_models.json catalog from compiled modeld_v2 artifacts.

Run by the build-spz-models workflow after openpilot/sunnypilot/modeld_v2/compile_modeld.py
compiles StarPilot's spz ONNX files. Each bundle follows the sunnypilot models manifest
schema so the device merges it into the qcom catalog (spz_source.py).

Every artifact is opened and checked before it is listed: the model must carry an
`action` output (spz models are fine-tuned for action control), and the 20 Hz flag is
derived from the same features-buffer convention the compiler's frame-skip uses.
"""

import argparse
import hashlib
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))

from openpilot.sunnypilot.modeld_v2.helpers import read_parts  # noqa: E402

REQUIRED_JSON_VERSION = "21"  # sunnypilot.models.helpers.REQUIRED_JSON_VERSION
FOLDER = "StarPilot"
INDEX_BASE = 900  # above the sunnypilot catalog's indexes; spz stays visually grouped


def short_name(model_id: str) -> str:
  # spz_polaris_r3a -> POLAR3A: first four letters of every id token after spz_
  tokens = model_id.removeprefix("spz_").split("_")
  return "".join(token[:4] for token in tokens).upper()[:8]


def _sha256(path: pathlib.Path) -> str:
  with path.open("rb") as f:
    return hashlib.file_digest(f, "sha256").hexdigest()


def artifact_metadata(pkl_path: pathlib.Path) -> dict:
  _, extras = read_parts(str(pkl_path))
  return extras["metadata"]["model"]


def bundle_for(build: dict, pkl_path: pathlib.Path, hf_repo: str, build_time: str) -> dict:
  metadata = artifact_metadata(pkl_path)
  slices = metadata.get("output_slices", {})
  if "action" not in slices:
    raise ValueError(f"{build['id']} has no action output; refusing to publish a spz model that cannot drive action control")
  features_buffer = metadata["input_shapes"]["features_buffer"]
  url = f"https://huggingface.co/datasets/{hf_repo}/resolve/main/models/spz/{build['id']}/{pkl_path.name}"
  return {
    "short_name": short_name(build["id"]),
    "display_name": build["display_name"],
    "is_20hz": features_buffer[1] < 99,  # FULL_HISTORY_BUFFER_LEN: the legacy 10 Hz class
    "is_big": False,
    "ref": build["id"],
    "environment": "release",
    "runner": "tinygrad",
    "index": 0,  # assigned by the caller after dedup against previous entries
    "minimum_selector_version": REQUIRED_JSON_VERSION,
    "generation": "12",
    "build_time": build_time,
    "overrides": {"folder": FOLDER, "lat": ".0", "long": ".3"},
    "models": [{"type": "chunked", "artifact": {"file_name": pkl_path.name,
                "download_uri": {"url": url, "sha256": _sha256(pkl_path)}}}],
    "spz": {"id": build["id"], "behavior_version": build["behavior_version"],
            "source_onnx_sha256": build["source_onnx_sha256"]},
  }


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument("--models-dir", required=True, help="compiled <id>_driving_tinygrad.pkl files")
  parser.add_argument("--builds-dir", required=True, help="workflow-written <id>.json build descriptors")
  parser.add_argument("--hf-repo", required=True)
  parser.add_argument("--previous", help="existing spz_models.json whose entries are kept")
  parser.add_argument("--build-time", required=True)
  parser.add_argument("--output", required=True)
  args = parser.parse_args()

  models_dir, builds_dir = pathlib.Path(args.models_dir), pathlib.Path(args.builds_dir)
  bundles = []
  for build_file in sorted(builds_dir.glob("*.json")):
    build = json.loads(build_file.read_text())
    pkl = models_dir / f"{build['id']}_driving_tinygrad.pkl"
    if not pkl.is_file():
      raise SystemExit(f"compiled artifact missing for {build['id']}: {pkl}")
    bundle = bundle_for(build, pkl, args.hf_repo, args.build_time)
    bundle["index"] = INDEX_BASE + len(bundles)
    bundles.append(bundle)
    print(f"{build['id']}: {bundle['short_name']} 20hz={bundle['is_20hz']} sha={bundle['models'][0]['artifact']['download_uri']['sha256'][:12]}")

  if args.previous and pathlib.Path(args.previous).is_file():
    previous = json.loads(pathlib.Path(args.previous).read_text())
    rebuilt = {b["ref"] for b in bundles}
    bundles += [b for b in previous.get("bundles", []) if b.get("ref") not in rebuilt and b.get("is_big") is False]

  pathlib.Path(args.output).write_text(json.dumps({"bundles": bundles}, indent=1) + "\n")
  print(f"wrote {args.output} with {len(bundles)} bundles")


if __name__ == "__main__":
  main()
