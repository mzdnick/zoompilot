#!/usr/bin/env bash

export OMP_NUM_THREADS=1
export MKL_NUM_THREADS=1
export NUMEXPR_NUM_THREADS=1
export OPENBLAS_NUM_THREADS=1
export VECLIB_MAXIMUM_THREADS=1

# models get lower priority than ui
# - ui is ~5ms
# - modeld is 20ms
# - DM is 10ms
# in order to run ui at 60fps (16.67ms), we need to allow
# it to preempt the model workloads. we have enough
# headroom for this until ui is moved to the CPU.
export QCOM_PRIORITY=12

if [ -z "$AGNOS_VERSION" ]; then
  export AGNOS_VERSION="19.7"
fi

export STAGING_ROOT="/data/safe_staging"

# zoo self-hosted backend: point API_HOST/ATHENA_HOST at a zoo server.
# Toggle:   /data/params/d/ZooBackendEnabled ("1"/"0") — settings → device
# Target:   /data/params/d/ZooApiUrl (e.g. http://192.168.1.50:8000 or
#           https://zoo.example.net). Applied on manager start.
ZOO_PARAMS_DIR="${ZOO_PARAMS_DIR:-/data/params/d}"

# When the backend selection CHANGES, drop the old backend's dongle
# identity so the device re-registers against the new backend at boot.
# The first run after installing this branch records the state without
# touching the existing (comma) identity.
ZOO_NOW="0"
if [ -f "$ZOO_PARAMS_DIR/ZooBackendEnabled" ] && [ "$(cat "$ZOO_PARAMS_DIR/ZooBackendEnabled" 2>/dev/null)" = "1" ]; then
  ZOO_NOW="1"
fi
ZOO_PREV="$(cat "$ZOO_PARAMS_DIR/ZooBackendPrevEnabled" 2>/dev/null || true)"
if [ -n "$ZOO_PREV" ] && [ "$ZOO_PREV" != "$ZOO_NOW" ]; then
  rm -f "$ZOO_PARAMS_DIR/DongleId"
  # zoo runs sunnylink with the same single identity: drop it too, so both
  # connections re-register against the new backend together.
  rm -f "$ZOO_PARAMS_DIR/SunnylinkDongleId"
fi
[ "$ZOO_PREV" != "$ZOO_NOW" ] && printf '%s' "$ZOO_NOW" > "$ZOO_PARAMS_DIR/ZooBackendPrevEnabled"

if [ "$ZOO_NOW" = "1" ]; then
  ZOO_API_URL="$(cat "$ZOO_PARAMS_DIR/ZooApiUrl" 2>/dev/null)"
  ZOO_API_URL="${ZOO_API_URL%/}"
  case "$ZOO_API_URL" in
    http://*|https://*)
      export API_HOST="$ZOO_API_URL"
      case "$ZOO_API_URL" in
        https://*) export ATHENA_HOST="wss://${ZOO_API_URL#https://}" ;;
        http://*)  export ATHENA_HOST="ws://${ZOO_API_URL#http://}" ;;
      esac
      # sunnylinkd points at the same zoo server. SUNNYLINK_ATHENA_HOST is
      # used verbatim as the ws URI (no path is appended device-side), so the
      # zoo sunnylink route path is part of the value.
      export SUNNYLINK_API_HOST="$ZOO_API_URL"
      case "$ZOO_API_URL" in
        https://*) export SUNNYLINK_ATHENA_HOST="wss://${ZOO_API_URL#https://}/ws/sp" ;;
        http://*)  export SUNNYLINK_ATHENA_HOST="ws://${ZOO_API_URL#http://}/ws/sp" ;;
      esac
      export ZOO_BACKEND_ACTIVE=1
      ;;
  esac
fi
