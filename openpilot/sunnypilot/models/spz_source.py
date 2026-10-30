"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

import re

import requests
from requests.exceptions import (SSLError, RequestException)

from openpilot.common.swaglog import cloudlog
from openpilot.sunnypilot.models.fetcher import ModelCache

# StarPilot's spz fine-tunes, recompiled by zoompilot CI into modeld_v2 artifacts and
# published as sunnypilot manifest bundles. The catalog follows the driving_models
# bundle schema, so entries merge into the qcom catalog and ride the existing
# selector, downloader, and modeld_v2 without fork-specific device code.
SPZ_CATALOG_URL = "https://huggingface.co/buckets/zoompilot/zoompilot_models/resolve/spz_models.json"

# every bundle key ModelParser._parse_bundle reads; the catalog generator emits these
REQUIRED_BUNDLE_KEYS = frozenset((
  "index", "short_name", "display_name", "generation", "environment",
  "minimum_selector_version", "models",
))
SHA256 = re.compile(r"[0-9a-f]{64}\Z")


def _artifact_valid(model: object) -> bool:
  artifact = getattr(model, "artifact", None) if not isinstance(model, dict) else model.get("artifact")
  if not isinstance(artifact, dict):
    return False
  download_uri = artifact.get("download_uri", {})
  return (isinstance(artifact.get("file_name"), str) and bool(artifact["file_name"]) and
          isinstance(download_uri, dict) and isinstance(download_uri.get("url"), str) and
          bool(download_uri["url"]) and isinstance(download_uri.get("sha256"), str) and
          bool(SHA256.fullmatch(download_uri["sha256"])))


def parse_spz_catalog(json_data: object) -> list[dict]:
  """Return the catalog's spz bundles as sunnypilot manifest bundle dicts.

  A malformed entry is dropped, not fatal: one bad model must not hide the rest
  or break the sunnypilot catalog it merges into.
  """
  bundles = json_data.get("bundles", []) if isinstance(json_data, dict) else None
  if not isinstance(bundles, list):
    return []
  parsed = []
  for bundle in bundles:
    if not isinstance(bundle, dict) or not REQUIRED_BUNDLE_KEYS <= bundle.keys():
      cloudlog.warning(f"spz catalog entry is missing keys: {bundle.get('ref', bundle.get('short_name')) if isinstance(bundle, dict) else bundle}")
      continue
    # spz bundles are small QCOM models by construction; is_big would taint the qcom
    # catalog cache (ModelFetcher._cache_matches_source)
    if bundle.get("is_big", False):
      cloudlog.warning(f"spz catalog entry {bundle.get('ref')} claims is_big; dropping")
      continue
    models = bundle["models"]
    if (not isinstance(models, list) or not models or
        any(not (isinstance(model, dict) and model.get("type") == "chunked" and _artifact_valid(model)) for model in models)):
      cloudlog.warning(f"spz catalog entry {bundle.get('ref')} has no valid chunked artifact; dropping")
      continue
    parsed.append(bundle)
  return parsed


def fetch_spz_bundles(cache: ModelCache) -> list[dict]:
  """Fetch the spz catalog, or fall back to its cache when the fetch fails.

  An empty list leaves the sunnypilot catalog untouched, so an unreachable
  catalog never degrades model selection.
  """
  try:
    response = requests.get(SPZ_CATALOG_URL, timeout=10)
    response.raise_for_status()
    bundles = parse_spz_catalog(response.json())
    if bundles:
      cache.set({"bundles": bundles})
      return bundles
    cloudlog.warning("spz catalog fetched but held no valid bundles")
  except (ConnectionError, SSLError, RequestException) as e:
    cloudlog.warning(f"spz catalog fetch failed: {e}")
  except Exception as e:
    cloudlog.exception(f"unexpected error fetching the spz catalog: {e}")

  cached_data, _ = cache.get()
  if cached_data:
    cloudlog.warning("using the cached spz catalog")
    return parse_spz_catalog(cached_data)
  return []
