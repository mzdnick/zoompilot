"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

import unittest
from unittest import mock

from openpilot.sunnypilot.models.fetcher import ModelParser
from openpilot.sunnypilot.models.spz_source import SPZ_CATALOG_URL, fetch_spz_bundles, parse_spz_catalog

# the shape the build-spz-models workflow publishes: a sunnypilot manifest bundle whose
# artifact points at zoompilot's recompiled pkl, plus an spz key with the StarPilot origin
SPZ_BUNDLE = {
  "short_name": "SPZR3A", "display_name": "StarPilot Polaris R3 A (October 29, 2026)",
  "is_20hz": True, "is_big": False, "ref": "spz_polaris_r3a", "environment": "release",
  "runner": "tinygrad", "index": 900, "minimum_selector_version": "21", "generation": "12",
  "build_time": "2026-10-30T00:00:00Z", "overrides": {"folder": "StarPilot", "lat": ".0", "long": ".3"},
  "models": [{"type": "chunked", "artifact": {
    "file_name": "spz_polaris_r3a_driving_tinygrad.pkl",
    "download_uri": {"url": f"{SPZ_CATALOG_URL.rsplit('/', 1)[0]}/models/spz/spz_polaris_r3a/spz_polaris_r3a_driving_tinygrad.pkl",
                     "sha256": "a" * 64}}}],
  "spz": {"id": "spz_polaris_r3a", "behavior_version": "v15", "source_onnx_sha256": "b" * 64},
}


def _bundle(**overrides):
  return {**SPZ_BUNDLE, **overrides}


class TestParseSpzCatalog(unittest.TestCase):

  def test_valid_entries_pass_through(self):
    catalog = {"bundles": [_bundle(), _bundle(ref="spz_polaris_r3b", short_name="SPZR3B", index=901)]}
    parsed = parse_spz_catalog(catalog)
    self.assertEqual([b["ref"] for b in parsed], ["spz_polaris_r3a", "spz_polaris_r3b"])

  def test_entries_parse_as_sunnypilot_bundles(self):
    # the merged catalog must survive ModelParser: this is the selector-side contract
    sunnypilot_bundle = {"short_name": "LAV2", "display_name": "LAv2", "is_20hz": False, "is_big": False,
                         "ref": "f9d5fc7d", "environment": "development", "runner": "tinygrad", "index": 0,
                         "minimum_selector_version": "21", "generation": "4",
                         "models": SPZ_BUNDLE["models"]}
    merged = {"bundles": [sunnypilot_bundle, *_bundle()]}
    parsed = ModelParser.parse_models(merged)
    by_ref = {b.ref: b for b in parsed}
    self.assertIn("spz_polaris_r3a", by_ref)
    spz = by_ref["spz_polaris_r3a"]
    self.assertEqual(spz.internalName, "SPZR3A")
    self.assertEqual(spz.displayName, SPZ_BUNDLE["display_name"])
    self.assertTrue(spz.is20hz)
    self.assertEqual(spz.generation, 12)
    self.assertEqual(spz.overrides[0].key, "folder")
    self.assertEqual(spz.models[0].artifact.fileName, "spz_polaris_r3a_driving_tinygrad.pkl")
    self.assertEqual(spz.models[0].artifact.downloadUri.sha256, "a" * 64)

  def test_missing_required_keys_drops_entry(self):
    catalog = {"bundles": [{k: v for k, v in SPZ_BUNDLE.items() if k != "models"}]}
    self.assertEqual(parse_spz_catalog(catalog), [])

  def test_is_big_entry_is_dropped(self):
    # an is_big bundle would taint the qcom catalog cache (ModelFetcher._cache_matches_source)
    catalog = {"bundles": [_bundle(is_big=True)]}
    self.assertEqual(parse_spz_catalog(catalog), [])

  def test_bad_artifact_drops_entry(self):
    bad_sha = _bundle(models=[{"type": "chunked", "artifact": {"file_name": "x.pkl",
                                 "download_uri": {"url": "https://x", "sha256": "short"}}}])
    wrong_type = _bundle(models=[{"type": "snpe", "artifact": SPZ_BUNDLE["models"][0]["artifact"]}])
    self.assertEqual(parse_spz_catalog({"bundles": [bad_sha, wrong_type, _bundle()]}), [_bundle()])

  def test_not_a_catalog_is_empty(self):
    self.assertEqual(parse_spz_catalog([]), [])
    self.assertEqual(parse_spz_catalog({"bundles": "nope"}), [])


class TestFetchSpzBundles(unittest.TestCase):

  def setUp(self):
    self.params = mock.MagicMock()
    self.params.get.return_value = None
    from openpilot.sunnypilot.models.fetcher import ModelCache
    self.cache = ModelCache(self.params, suffix="_Spz")

  def test_fetch_success_returns_and_caches(self):
    response = mock.MagicMock(status_code=200)
    response.json.return_value = {"bundles": [_bundle()]}
    with mock.patch("openpilot.sunnypilot.models.spz_source.requests.get", return_value=response) as get:
      parsed = fetch_spz_bundles(self.cache)
    get.assert_called_once_with(SPZ_CATALOG_URL, timeout=10)
    self.assertEqual([b["ref"] for b in parsed], ["spz_polaris_r3a"])
    self.assertEqual(self.params.put.call_count, 2)  # cache data + sync time

  def test_fetch_failure_falls_back_to_cache(self):
    cached = {"bundles": [_bundle(ref="spz_cached")]}
    self.params.get.side_effect = lambda key, **k: cached if "Cache" in str(key) else 1
    with mock.patch("openpilot.sunnypilot.models.spz_source.requests.get",
                    side_effect=ConnectionError("down")):
      parsed = fetch_spz_bundles(self.cache)
    self.assertEqual([b["ref"] for b in parsed], ["spz_cached"])

  def test_fetch_failure_without_cache_is_empty(self):
    with mock.patch("openpilot.sunnypilot.models.spz_source.requests.get",
                    side_effect=ConnectionError("down")):
      self.assertEqual(fetch_spz_bundles(self.cache), [])


if __name__ == "__main__":
  unittest.main()
