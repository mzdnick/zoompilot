"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""

import unittest
from unittest import mock

from openpilot.sunnypilot.models.fetcher import ModelParser
from openpilot.sunnypilot.models.spz_source import SPZ_CATALOG_URL, fetch_spz_catalog, parse_spz_catalog

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
    merged = {"bundles": [sunnypilot_bundle, _bundle()]}
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


class TestFetchSpzCatalog(unittest.TestCase):

  def test_fetch_success_parses(self):
    response = mock.MagicMock(status_code=200)
    response.json.return_value = {"bundles": [_bundle()]}
    with mock.patch("openpilot.sunnypilot.models.spz_source.requests.get", return_value=response) as get:
      parsed = fetch_spz_catalog()
    get.assert_called_once_with(SPZ_CATALOG_URL, timeout=10)
    self.assertEqual([b["ref"] for b in parsed], ["spz_polaris_r3a"])

  def test_fetch_failure_is_empty_not_fatal(self):
    with mock.patch("openpilot.sunnypilot.models.spz_source.requests.get",
                    side_effect=ConnectionError("down")):
      self.assertEqual(fetch_spz_catalog(), [])


class TestFetcherZPSpz(unittest.TestCase):
  """spz entries serve through get_bundles_for_source, whatever the sunnypilot catalog cache holds."""

  def _fetcher(self, params_values: dict):
    params = mock.MagicMock()
    params.get.side_effect = lambda key, *a, **k: params_values.get(key)
    from openpilot.sunnypilot.models.fetcher_zp import ModelFetcherZP
    return ModelFetcherZP(params)

  def _spz_response(self):
    response = mock.MagicMock(status_code=200)
    response.json.return_value = {"bundles": [_bundle()]}
    return response

  SUNNYPILOT_BUNDLE = {"short_name": "LAV2", "display_name": "LAv2", "is_20hz": False, "is_big": False,
                       "ref": "f9d5fc7d", "environment": "development", "runner": "tinygrad", "index": 0,
                       "minimum_selector_version": "21", "generation": "4", "models": SPZ_BUNDLE["models"]}

  def _routed_response(self, sunnypilot_json):
    # fetcher and spz_source import the same requests module, so one patch must
    # route both catalogs by URL
    def route(url, *a, **k):
      response = mock.MagicMock(status_code=200)
      response.json.return_value = ({"bundles": [_bundle()]} if "zoompilot_models" in url else sunnypilot_json)
      return response
    return mock.patch("openpilot.sunnypilot.models.spz_source.requests.get", side_effect=route)

  def test_serve_time_merge_appends_spz_once(self):
    sunnypilot_json = {"bundles": [self.SUNNYPILOT_BUNDLE, _bundle()]}  # ref clash stays deduped
    fetcher = self._fetcher({})
    with self._routed_response(sunnypilot_json):
      bundles = fetcher.get_bundles_for_source("qcom")
    refs = [b.ref for b in bundles]
    self.assertEqual(refs.count("spz_polaris_r3a"), 1)  # appended once, not also baked in by the fetch
    self.assertIn("f9d5fc7d", refs)

  def test_serve_time_merge_survives_warm_sunnypilot_cache(self):
    # the user-facing case: the sunnypilot catalog is served from a warm cache and never
    # refetched, and the spz entries still appear on the same tick
    import time
    params_values = {
      "ModelManager_ModelsCache": {"bundles": [self.SUNNYPILOT_BUNDLE]},
      "ModelManager_LastSyncTime": int(time.monotonic() * 1e9),  # fresh qcom cache (ns): no refetch
      "ModelManager_SpzFetchAttempt": 0,  # outside the backoff window: spz fetch runs now
    }
    fetcher = self._fetcher(params_values)
    with self._routed_response({"bundles": []}) as route:
      bundles = fetcher.get_bundles_for_source("qcom")
      route.call_count  # qcom served from cache: the router only sees the spz fetch
      spz_urls = [c.args[0] for c in route.call_args_list]
      self.assertTrue(all("zoompilot_models" in u for u in spz_urls), spz_urls)
    self.assertIn("spz_polaris_r3a", [b.ref for b in bundles])
    self.assertIn("f9d5fc7d", [b.ref for b in bundles])

  def test_failed_fetch_backs_off_in_memory(self):
    fetcher = self._fetcher({})
    with mock.patch("openpilot.sunnypilot.models.fetcher_zp.fetch_spz_catalog", return_value=[]) as fetch:
      self.assertEqual(fetcher.spz_bundles(), [])
      self.assertEqual(fetcher.spz_bundles(), [])  # inside the backoff window
    self.assertEqual(fetch.call_count, 1)  # one attempt, not one per serve

  def test_last_good_list_serves_across_failures(self):
    fetcher = self._fetcher({})
    good = mock.MagicMock(return_value=[_bundle()])
    dead = mock.MagicMock(return_value=[])
    with mock.patch("openpilot.sunnypilot.models.fetcher_zp.fetch_spz_catalog", good):
      first = fetcher.spz_bundles()
    fetcher._spz_fetched_at -= 2 * 3600  # the hourly hold lapses
    fetcher._spz_attempt_at -= 2 * 3600  # past the backoff window too
    with mock.patch("openpilot.sunnypilot.models.fetcher_zp.fetch_spz_catalog", dead):
      second = fetcher.spz_bundles()
    dead.assert_called_once()
    self.assertEqual([b.ref for b in first], [b.ref for b in second])


  def test_spz_failure_never_blanks_the_selector(self):
    # the incident this file owes its shape to: an unregistered params key raised inside
    # the spz path and the selector fell back to an empty catalog. A spz exception must
    # leave the sunnypilot catalog untouched.
    fetcher = self._fetcher({})
    with mock.patch.object(type(fetcher), "spz_bundles", side_effect=RuntimeError("params registry")), \
         mock.patch("openpilot.sunnypilot.models.spz_source.requests.get") as get:
      get.return_value.status_code = 200
      get.return_value.json.return_value = {"bundles": [self.SUNNYPILOT_BUNDLE]}
      bundles = fetcher.get_bundles_for_source("qcom")
    self.assertEqual([b.ref for b in bundles], ["f9d5fc7d"])


if __name__ == "__main__":
  unittest.main()
