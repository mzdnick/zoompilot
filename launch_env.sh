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
if [ -f "$ZOO_PARAMS_DIR/ZooBackendEnabled" ] && [ "$(cat "$ZOO_PARAMS_DIR/ZooBackendEnabled" 2>/dev/null)" = "1" ]; then
  ZOO_API_URL="$(cat "$ZOO_PARAMS_DIR/ZooApiUrl" 2>/dev/null)"
  ZOO_API_URL="${ZOO_API_URL%/}"
  case "$ZOO_API_URL" in
    http://*|https://*)
      export API_HOST="$ZOO_API_URL"
      case "$ZOO_API_URL" in
        https://*) export ATHENA_HOST="wss://${ZOO_API_URL#https://}" ;;
        http://*)  export ATHENA_HOST="ws://${ZOO_API_URL#http://}" ;;
      esac
      export ZOO_BACKEND_ACTIVE=1
      ;;
  esac
fi
