"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
from __future__ import annotations

import time

from openpilot.cereal import custom
from openpilot.common.params import Params
from openpilot.common.swaglog import cloudlog
from openpilot.sunnypilot import jetlink_adapter
from openpilot.sunnypilot.models.fetcher import ModelCache, ModelFetcher, ModelParser
from openpilot.sunnypilot.models.spz_source import fetch_spz_catalog

# stamped on the big-model catalog: whether it carries the newer catalogs' models
EXTENDED_KEY = "extended"

# the spz catalog lives in memory for the manager's process: refreshed hourly, and a
# failed fetch retries at most every five minutes while the last good list keeps serving
SPZ_CACHE_SECONDS = 3600
SPZ_RETRY_SECONDS = 300


class BigCatalogCache(ModelCache):
  """The big-model catalog's cache, expired when it was fetched for other hardware: a chestnut
  coming or going changes whether it is extended, and the cache would otherwise hide that for an
  hour. Once per change: offline, the refetch fails and the cache stands until it expires."""

  def __init__(self, params: Params, suffix: str, fetcher: ModelFetcherZP | None = None):
    super().__init__(params, suffix=suffix)
    self._refetched_extends: bool | None = None
    self._fetcher = fetcher

  def get(self) -> tuple[dict, bool]:
    cached_data, is_expired = super().get()
    if cached_data and not is_expired:
      extends = jetlink_adapter.should_extend_catalog()
      if bool(cached_data.get(EXTENDED_KEY)) != extends and self._refetched_extends != extends:
        self._refetched_extends = extends
        cloudlog.warning(f"big-model catalog was fetched {'without' if extends else 'with'} the newer catalogs; refetching")
        is_expired = True
      if self._fetcher is not None:
        self._fetcher._update_active_json(extends)
    return cached_data, is_expired


class ModelFetcherZP(ModelFetcher):
  """sunnypilot's fetcher with the big-model catalog extended through jetlink: without a chestnut
  it also lists the big models newer catalogs publish, so one released after this build can still
  be picked for an accelerator."""

  def __init__(self, params: Params):
    super().__init__(params)
    self.model_caches["chestnut"] = BigCatalogCache(params, self.MODEL_SOURCES["chestnut"][1], fetcher=self)
    self._spz_bundles: list[custom.ModelManagerSP.ModelBundle] = []
    self._spz_fetched_at = 0.0
    self._spz_attempt_at = 0.0
    cached = params.get(f"ModelManager_ModelsCache{self.MODEL_SOURCES['chestnut'][1]}")
    if isinstance(cached, dict) and cached.get(EXTENDED_KEY) is True:
      self._update_active_json(extended=True)

  def get_bundles_for_source(self, source: str) -> list[custom.ModelManagerSP.ModelBundle]:
    bundles = super().get_bundles_for_source(source)
    if source != "qcom":
      return bundles
    # spz entries join at serve time, not catalog-refetch time: a device updating to
    # this build shows them within one manager tick, whatever state the sunnypilot
    # catalog cache is in. Fail open: a spz problem must never take the selector down.
    try:
      spz = self.spz_bundles()
    except Exception:
      cloudlog.exception("spz catalog serve failed; serving the sunnypilot catalog alone")
      return bundles
    if spz:
      known_refs = {bundle.ref for bundle in bundles}
      merged = [bundle for bundle in spz if bundle.ref not in known_refs]
      if merged:
        cloudlog.debug(f"spz catalog served {len(merged)} bundles")
      return bundles + merged
    return bundles

  def spz_bundles(self) -> list[custom.ModelManagerSP.ModelBundle]:
    """The spz catalog held in memory for the process life: fetched at first serve,
    refreshed hourly, and a failed fetch keeps the last good list while backing off
    between attempts. Deliberately no params persistence: a params key the running
    build's registry does not know raises on write, and a device updated over a
    prebuilt install keeps that build's params_pyx (spz_source)."""
    now = time.monotonic()
    if self._spz_bundles and now - self._spz_fetched_at < SPZ_CACHE_SECONDS:
      return self._spz_bundles
    if now - self._spz_attempt_at < SPZ_RETRY_SECONDS:
      return self._spz_bundles  # between attempts: the last good list, possibly none
    self._spz_attempt_at = now
    dicts = fetch_spz_catalog()
    if dicts:
      self._spz_bundles = ModelParser.parse_models({"bundles": dicts})
      self._spz_fetched_at = time.monotonic()
    return self._spz_bundles

  def _extend_catalog(self, source: str, json_data: dict) -> dict:
    if source != "chestnut":
      return json_data
    extended = jetlink_adapter.should_extend_catalog()
    self._update_active_json(extended)
    return {**(jetlink_adapter.extend_catalog(json_data) if extended else json_data), EXTENDED_KEY: extended}
