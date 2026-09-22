"""
Copyright (c) 2021-, Haibin Wen, sunnypilot, and a number of other contributors.

This file is part of sunnypilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.
"""
import random
import time

from openpilot.common.parameterized import parameterized

from openpilot.cereal import custom
from openpilot.sunnypilot.selfdrive.controls.lib.speed_limit import LIMIT_MAX_MAP_DATA_AGE, CAMERA_MEMORY_MAX_AGE

from openpilot.sunnypilot.selfdrive.controls.lib.speed_limit.speed_limit_resolver import SpeedLimitResolver, ALL_SOURCES
from openpilot.sunnypilot.selfdrive.controls.lib.speed_limit.common import Policy
from openpilot.common.test import OpenpilotTestCase

SpeedLimitSource = custom.LongitudinalPlanSP.SpeedLimit.Source


def create_mock(properties, mocker):
  mock = mocker.MagicMock()
  for _property, value in properties.items():
    setattr(mock, _property, value)
  return mock


def setup_sm_mock(mocker):
  cruise_speed_limit = random.uniform(0, 120)
  live_map_data_limit = random.uniform(0, 120)

  car_state = create_mock({
    'gasPressed': False,
    'brakePressed': False,
    'standstill': False,
  }, mocker)
  car_state_sp = create_mock({
    'speedLimit': cruise_speed_limit,
  }, mocker)
  live_map_data = create_mock({
    'speedLimit': live_map_data_limit,
    'speedLimitValid': True,
    'speedLimitAhead': 0.,
    'speedLimitAheadValid': 0.,
    'speedLimitAheadDistance': 0.,
  }, mocker)
  gps_data = create_mock({
    'unixTimestampMillis': time.monotonic() * 1e3,
  }, mocker)
  sm_mock = mocker.MagicMock()
  sm_mock.__getitem__.side_effect = lambda key: {
    'carState': car_state,
    'liveMapDataSP': live_map_data,
    'carStateSP': car_state_sp,
    'gpsLocation': gps_data,
  }[key]
  return sm_mock


parametrized_policies = parameterized.expand(
  [
    (Policy.car_state_only, 'carStateSP', SpeedLimitSource.car),
    (Policy.car_state_priority, 'carStateSP', SpeedLimitSource.car),
    (Policy.map_data_only, 'liveMapDataSP', SpeedLimitSource.map),
    (Policy.map_data_priority, 'liveMapDataSP', SpeedLimitSource.map),
  ],
  names=["policy", "sm_key", "function_key"]
)


def resolver_class():
  return SpeedLimitResolver


class TestSpeedLimitResolverValidation(OpenpilotTestCase):

  @parameterized.expand(list(Policy), names=["policy"])
  def test_initial_state(self, resolver_class, policy):
    resolver = resolver_class()
    resolver.policy = policy
    for source in ALL_SOURCES:
      if source in resolver.limit_solutions:
        assert resolver.limit_solutions[source] == 0.
        assert resolver.distance_solutions[source] == 0.

  @parametrized_policies
  def test_resolver(self, resolver_class, policy, sm_key, function_key, mocker):
    resolver = resolver_class()
    resolver.policy = policy
    sm_mock = setup_sm_mock(mocker)
    source_speed_limit = sm_mock[sm_key].speedLimit

    # Assert the resolver
    resolver.update(source_speed_limit, sm_mock)
    assert resolver.speed_limit == source_speed_limit
    assert resolver.source == ALL_SOURCES[function_key]

  def test_resolver_combined(self, resolver_class, mocker):
    resolver = resolver_class()
    resolver.policy = Policy.combined
    sm_mock = setup_sm_mock(mocker)
    socket_to_source = {'carStateSP': SpeedLimitSource.car, 'liveMapDataSP': SpeedLimitSource.map}
    minimum_key, minimum_speed_limit = min(
      ((key, sm_mock[key].speedLimit) for key in
       socket_to_source.keys()), key=lambda x: x[1])

    # Assert the resolver
    resolver.update(minimum_speed_limit, sm_mock)
    assert resolver.speed_limit == minimum_speed_limit
    assert resolver.source == socket_to_source[minimum_key]

  @parametrized_policies
  def test_parser(self, resolver_class, policy, sm_key, function_key, mocker):
    resolver = resolver_class()
    resolver.policy = policy
    sm_mock = setup_sm_mock(mocker)
    source_speed_limit = sm_mock[sm_key].speedLimit

    # Assert the parsing
    resolver.update(source_speed_limit, sm_mock)
    assert resolver.limit_solutions[ALL_SOURCES[function_key]] == source_speed_limit
    assert resolver.distance_solutions[ALL_SOURCES[function_key]] == 0.

  @parameterized.expand(list(Policy), names=["policy"])
  def test_resolve_interaction_in_update(self, resolver_class, policy, mocker):
    v_ego = 50
    resolver = resolver_class()
    resolver.policy = policy

    sm_mock = setup_sm_mock(mocker)
    resolver.update(v_ego, sm_mock)

    # After resolution
    assert resolver.speed_limit is not None
    assert resolver.distance is not None
    assert resolver.source is not None

  @parameterized.expand(list(Policy), names=["policy"])
  def test_old_map_data_ignored(self, resolver_class, policy, mocker):
    resolver = resolver_class()
    resolver.policy = policy
    sm_mock = mocker.MagicMock()
    sm_mock['gpsLocation'].unixTimestampMillis = (time.monotonic() - 2 * LIMIT_MAX_MAP_DATA_AGE) * 1e3
    resolver._get_from_map_data(sm_mock)
    assert resolver.limit_solutions[SpeedLimitSource.map] == 0.
    assert resolver.distance_solutions[SpeedLimitSource.map] == 0.


def setup_fallback_sm(mocker, *, car_limit, cam_confirmed=False, osm_limit=0., osm_valid=False):
  """A SubMaster mock showing one carStateSP frame and the OSM section state."""
  car_state_sp = create_mock({
    'speedLimit': car_limit,
    'speedLimitCamConfirmed': cam_confirmed,
  }, mocker)
  live_map_data = create_mock({
    'speedLimit': osm_limit,
    'speedLimitValid': osm_valid,
    'speedLimitAhead': 0.,
    'speedLimitAheadValid': False,
    'speedLimitAheadDistance': 0.,
  }, mocker)
  gps_data = create_mock({
    'unixTimestampMillis': time.monotonic() * 1e3,
  }, mocker)
  sm_mock = mocker.MagicMock()
  sm_mock.__getitem__.side_effect = lambda key: {
    'liveMapDataSP': live_map_data,
    'carStateSP': car_state_sp,
    'gpsLocation': gps_data,
  }[key]
  return sm_mock


MPH = 0.44704
CAMERA_VALUE = 25. * MPH   # the latched camera confirmation
OSM_MATCHING = CAMERA_VALUE
OSM_MISMATCHED = 30. * MPH


class TestSpeedLimitCameraFallback(OpenpilotTestCase):
  """The fallback toggles act only while the car's nav-map fallback is displayed
  (limit shown, camera bit clear). With OSM present the priority toggle always takes
  it, the confirm toggle only when it matches the latched camera value; without OSM
  the car's value stands as the last tier. Toggles off is byte-identical to stock."""

  def _resolver(self, mocker, *, confirm=False, priority=False) -> SpeedLimitResolver:
    resolver = SpeedLimitResolver()
    resolver.camera_confirm_fallback = confirm
    resolver.osm_fallback_priority = priority
    resolver.policy = Policy.car_state_only
    # a camera confirmation happened one frame ago
    sm = setup_fallback_sm(mocker, car_limit=CAMERA_VALUE, cam_confirmed=True)
    resolver._update_camera_memory(sm['carStateSP'])
    return resolver

  def test_toggles_off_is_stock(self, mocker):
    resolver = self._resolver(mocker)
    sm = setup_fallback_sm(mocker, car_limit=35. * MPH, osm_limit=OSM_MATCHING, osm_valid=True)
    resolver.update(20., sm)
    assert resolver.source == SpeedLimitSource.car
    assert resolver.speed_limit == 35. * MPH

  def test_confirm_swaps_matching_osm(self, mocker):
    resolver = self._resolver(mocker, confirm=True)
    sm = setup_fallback_sm(mocker, car_limit=35. * MPH, osm_limit=OSM_MATCHING, osm_valid=True)
    resolver.update(20., sm)
    assert resolver.source == SpeedLimitSource.camera
    assert resolver.speed_limit == OSM_MATCHING

  def test_confirm_ignores_mismatched_osm(self, mocker):
    resolver = self._resolver(mocker, confirm=True)
    sm = setup_fallback_sm(mocker, car_limit=35. * MPH, osm_limit=OSM_MISMATCHED, osm_valid=True)
    resolver.update(20., sm)
    assert resolver.source == SpeedLimitSource.car

  def test_confirm_ignores_missing_osm(self, mocker):
    resolver = self._resolver(mocker, confirm=True)
    sm = setup_fallback_sm(mocker, car_limit=35. * MPH, osm_limit=0., osm_valid=False)
    resolver.update(20., sm)
    assert resolver.source == SpeedLimitSource.car

  def test_confirm_ignores_expired_latch(self, mocker):
    resolver = self._resolver(mocker, confirm=True)
    resolver.camera_memory_age = CAMERA_MEMORY_MAX_AGE + 1.
    sm = setup_fallback_sm(mocker, car_limit=35. * MPH, osm_limit=OSM_MATCHING, osm_valid=True)
    resolver.update(20., sm)
    assert resolver.source == SpeedLimitSource.car

  def test_confirm_freshens_on_live_camera(self, mocker):
    resolver = self._resolver(mocker, confirm=True)
    sm_live = setup_fallback_sm(mocker, car_limit=30. * MPH, cam_confirmed=True)
    resolver._update_camera_memory(sm_live['carStateSP'])
    sm = setup_fallback_sm(mocker, car_limit=35. * MPH, osm_limit=30. * MPH, osm_valid=True)
    resolver.update(20., sm)
    assert resolver.source == SpeedLimitSource.camera
    assert resolver.speed_limit == 30. * MPH

  def test_priority_takes_mismatched_osm(self, mocker):
    resolver = self._resolver(mocker, priority=True)
    sm = setup_fallback_sm(mocker, car_limit=35. * MPH, osm_limit=OSM_MISMATCHED, osm_valid=True)
    resolver.update(20., sm)
    assert resolver.source == SpeedLimitSource.camera
    assert resolver.speed_limit == OSM_MISMATCHED

  def test_priority_keeps_sd_as_last_tier(self, mocker):
    resolver = self._resolver(mocker, priority=True)
    sm = setup_fallback_sm(mocker, car_limit=35. * MPH, osm_limit=0., osm_valid=False)
    resolver.update(20., sm)
    assert resolver.source == SpeedLimitSource.car

  def test_priority_subsumes_confirm_when_both_on(self, mocker):
    resolver = self._resolver(mocker, confirm=True, priority=True)
    sm = setup_fallback_sm(mocker, car_limit=35. * MPH, osm_limit=OSM_MISMATCHED, osm_valid=True)
    resolver.update(20., sm)
    assert resolver.source == SpeedLimitSource.camera

  def test_live_camera_display_never_interposes(self, mocker):
    resolver = self._resolver(mocker, confirm=True, priority=True)
    sm = setup_fallback_sm(mocker, car_limit=35. * MPH, cam_confirmed=True,
                           osm_limit=OSM_MISMATCHED, osm_valid=True)
    resolver.update(20., sm)
    assert resolver.source == SpeedLimitSource.car

  def test_combined_policy_takes_camera_slot(self, mocker):
    resolver = self._resolver(mocker, priority=True)
    resolver.policy = Policy.combined
    sm = setup_fallback_sm(mocker, car_limit=35. * MPH, osm_limit=OSM_MISMATCHED, osm_valid=True)
    resolver.update(20., sm)
    assert resolver.source == SpeedLimitSource.camera
