"""Active backend helpers: comma connect vs a self-hosted zoo server.

launch_env.sh exports ZOO_BACKEND_ACTIVE (plus API_HOST/ATHENA_HOST) when
the zoo toggle is on; see ZOO_BACKEND.md at the repo root.
"""
import os


def zoo_active() -> bool:
  return os.environ.get('ZOO_BACKEND_ACTIVE') == '1'


def pairing_base_url() -> str:
  """Web UI base for pairing QR links, for the active backend."""
  if zoo_active():
    return (os.environ.get('API_HOST') or '').rstrip('/')
  return 'https://connect.comma.ai'
