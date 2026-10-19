#!/usr/bin/env python3
"""
Copyright (c) 2026-, Zeph Leggett.

This file is part of zoompilot and is licensed under the MIT License.
See the LICENSE.md file in the root directory for more details.

Set up the zoo private S3 archive on a laptop.

Prompts for the bucket and credentials, optionally validates a PGP public
key the same way the device will (empty = uploads stay unencrypted), PUTs
a probe object as a self-test, and prints the exact params to apply on
the device. Every field can also arrive as a flag or a ZOO_S3_* env var,
the [device] section of ~/.config/zoo/credentials is remembered between
runs, and --apply writes the params over SSH instead of pasting; the
printed lines stay for anyone who prefers paste. The device credential
should be put-only, scoped to the archive prefix; an archive ring budget
additionally needs list and delete capabilities.

Usage (from the repo root, with requests installed):
  python tools/zoo/configure.py [--self-test] [--apply [USER@HOST]]
      [--provider aws|r2|b2|minio] [--generate-key]
      [--endpoint URL] [--region R] [--bucket B] [--path-style 0|1]
      [--access-key-id ID] [--secret-access-key SECRET] [--pubkey PATH]
      [--prefix P] [--scope minimal|full|selected] [--budget 8G]
"""

import argparse
import base64
import configparser
import os
import secrets
import subprocess
import sys
import time

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, REPO_ROOT)

from openpilot.common.zoo import pgp
from openpilot.common.zoo.s3sig import S3Config, signed_put, parse_budget

STATE_DIR = os.path.expanduser('~/.config/zoo')
CREDENTIALS_PATH = os.path.join(STATE_DIR, 'credentials')
DEVICE_SECTION = 'device'

# the usual USB-tether address; any USER@HOST works as --apply's value
DEFAULT_APPLY_HOST = 'comma@192.168.43.1'

PROVIDER_GUIDES = {
  'aws': (['IAM console: create a user, attach an inline policy, create its access key.'], 'aws'),
  'r2': (['R2 dashboard: Manage API Tokens, create a token scoped to this bucket with object-level',
          'permissions. Put only; add List and Delete for a ring budget.',
          'Endpoint: https://<account_id>.r2.cloudflarestorage.com, path-style 1.'], None),
  'b2': (['B2 app keys: create a key scoped to this bucket and prefix,',
          'capabilities writeFiles; add listFiles and deleteFiles for a ring budget.'], None),
  'minio': (['MinIO: create a user and attach the policy below with mc admin policy.'], 'minio'),
}


def ask(prompt: str, default: str = '') -> str:
  suffix = f' [{default}]' if default else ''
  value = input(f'{prompt}{suffix}: ').strip()
  return value or default


def read_device_creds() -> dict:
  cp = configparser.ConfigParser()
  try:
    cp.read(CREDENTIALS_PATH)
  except configparser.Error:
    return {}
  return dict(cp[DEVICE_SECTION]) if cp.has_section(DEVICE_SECTION) else {}


def remember_device_creds(fields: dict) -> None:
  try:
    os.makedirs(STATE_DIR, exist_ok=True)
    cp = configparser.ConfigParser()
    cp.read(CREDENTIALS_PATH)
    if not cp.has_section(DEVICE_SECTION):
      cp.add_section(DEVICE_SECTION)
    for key, value in fields.items():
      cp[DEVICE_SECTION][key] = value
    with open(CREDENTIALS_PATH, 'w') as f:
      cp.write(f)
    os.chmod(CREDENTIALS_PATH, 0o600)
  except OSError:
    pass  # best effort: forgetting only costs re-typing


def last_prefix() -> str:
  try:
    with open(os.path.join(STATE_DIR, 'last-prefix')) as f:
      value = f.read().strip()
    if value and '/' not in value:
      return value
  except OSError:
    pass
  return ''


def remember_prefix(prefix: str) -> None:
  try:
    os.makedirs(STATE_DIR, exist_ok=True)
    with open(os.path.join(STATE_DIR, 'last-prefix'), 'w') as f:
      f.write(prefix)
  except OSError:
    pass  # best effort: forgetting only costs a fresh suggestion


def resolve(flag_value: str | None, env_name: str, stored: str | None = None) -> str | None:
  """flag beats env beats the remembered [device] credentials."""
  if flag_value:
    return flag_value
  env = os.environ.get(env_name)
  if env:
    return env
  return stored


def gather(value: str | None, prompt: str, default: str, tty: bool) -> str:
  """prompt only for what is still missing, and only on a terminal."""
  if value:
    return value
  if tty:
    return ask(prompt, default)
  return default


def provider_policy(provider: str, bucket: str, prefix: str, ring: bool) -> str | None:
  kind = PROVIDER_GUIDES[provider][1]
  if kind is None:
    return None
  actions = ['s3:PutObject'] + (['s3:ListBucket', 's3:DeleteObject'] if ring else [])
  resource = f'arn:aws:s3:::{bucket}/{prefix}/*'
  statements = [{'Effect': 'Allow', 'Action': actions, 'Resource': resource}]
  if ring:
    statements.append({'Effect': 'Allow', 'Action': ['s3:ListBucket'],
                       'Resource': f'arn:aws:s3:::{bucket}'})
  return '\n'.join(f'  {line}' for stmt in statements for line in (
    '{', f'    "Effect": "{stmt["Effect"]}",',
    f'    "Action": {stmt["Action"]},', f'    "Resource": "{stmt["Resource"]}"', '},'))


def print_provider_guide(provider: str, bucket: str, prefix: str, ring: bool) -> None:
  guide_lines, _ = PROVIDER_GUIDES[provider]
  print(f'--- {provider} setup guide ---')
  for line in guide_lines:
    print(line)
  if provider == 'aws':
    print('Put-only policy for this bucket and prefix (add the ring actions for a budget):')
  elif provider == 'minio':
    print('Policy for this bucket and prefix:')
  print(provider_policy(provider, bucket or '<bucket>', prefix or '<prefix>', ring) or '')
  print()


def generate_key(out_path: str) -> str:
  """A laptop-side archive key in the normal gpg keyring; returns the export path."""
  def gpg(*arguments: str) -> subprocess.CompletedProcess:
    return subprocess.run(['gpg', '--batch', '--passphrase', '', *arguments],
                          capture_output=True, text=True)

  result = gpg('--quick-generate-key', 'zoo archive', 'ed25519', 'cert', 'never')
  if result.returncode != 0:
    sys.exit(f'gpg key generation failed: {result.stderr.strip()[-300:]}')
  listing = gpg('--list-keys', '--with-colons', 'zoo archive')
  fingerprints = []
  for line in listing.stdout.splitlines():
    parts = line.split(':')
    if line.startswith('fpr:') and len(parts) > 9:
      fingerprints.append(parts[9])
  if not fingerprints:
    sys.exit('generated key not found in the gpg keyring')
  fpr = fingerprints[-1]
  result = gpg('--quick-add-key', fpr, 'cv25519', 'encr', 'never')
  if result.returncode != 0:
    sys.exit(f'gpg encryption subkey failed: {result.stderr.strip()[-300:]}')
  exported = gpg('--export', '--armor', fpr)
  if exported.returncode != 0 or not exported.stdout:
    sys.exit('gpg export failed')
  with open(out_path, 'w') as f:
    f.write(exported.stdout)
  print(f'PGP key generated: {out_path} (fingerprint {fpr})')
  print('The private half is unprotected in your gpg keyring. Back it up:')
  print(f'  gpg --export-secret-keys {fpr} > zoo-private-key-backup.asc')
  print('Losing it with no backup means an unreadable archive.')
  return out_path


def apply_script(values: list[tuple[str, str]]) -> list[str]:
  """The device-side lines, printed for paste and reused by --apply."""
  lines = []
  for name, value in values:
    if name in ('ZooS3PublicKey', 'ZooS3SecretAccessKey'):
      # base64 round-trip: quoting inside the value stays inert, and no
      # trailing newline sneaks into the param (a heredoc always adds one)
      payload = base64.b64encode(value.encode()).decode()
      lines.append(f"  echo '{payload}' | base64 -d > /data/params/d/{name}")
    else:
      lines.append(f"  printf '%s' '{value}' > /data/params/d/{name}")
  return lines


def apply_over_ssh(host: str, lines: list[str]) -> bool:
  script = '\n'.join(line.strip() for line in lines) + '\n'
  print(f'Applying the params over SSH to {host} ...')
  try:
    result = subprocess.run(['ssh', host, 'sh -s'], input=script, text=True,
                            capture_output=True, timeout=60)
  except FileNotFoundError:
    print('ssh was not found on this laptop; paste the printed lines instead')
    return False
  except subprocess.TimeoutExpired:
    print('ssh timed out; paste the printed lines instead')
    return False
  if result.returncode != 0:
    print(f'ssh failed (exit {result.returncode}): {result.stderr.strip()[-300:]}')
    print('The printed lines above are still the manual path.')
    return False
  print('params applied on the device')
  return True


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument('--self-test', action='store_true', help='PUT an encrypted probe object after validating')
  parser.add_argument('--apply', nargs='?', const=DEFAULT_APPLY_HOST, metavar='USER@HOST',
                      help='also write the params over SSH; the printed lines remain for manual paste')
  parser.add_argument('--provider', choices=sorted(PROVIDER_GUIDES), help='print a setup guide for this provider')
  parser.add_argument('--generate-key', metavar='PATH', nargs='?', const='pubkey.asc',
                      help='generate a laptop-side archive PGP key and export it to PATH (default pubkey.asc)')
  parser.add_argument('--endpoint')
  parser.add_argument('--region')
  parser.add_argument('--bucket')
  parser.add_argument('--path-style', choices=['0', '1'])
  parser.add_argument('--access-key-id')
  parser.add_argument('--secret-access-key')
  parser.add_argument('--pubkey', help='path to an armored PGP public key; empty means no encryption')
  parser.add_argument('--prefix')
  parser.add_argument('--scope', choices=['minimal', 'full', 'selected'])
  parser.add_argument('--budget', help='archive ring budget, e.g. 8G (empty = off)')
  args = parser.parse_args()

  print('zoo private S3 archive setup')
  print('The device credential must be put-only, scoped to the bucket or prefix.')
  print('A ring budget additionally needs list and delete on the same key.')
  print()

  stored = read_device_creds()
  tty = sys.stdin.isatty()
  endpoint = gather(resolve(args.endpoint, 'ZOO_S3_ENDPOINT', stored.get('endpoint')),
                    'S3 endpoint (e.g. https://s3.us-east-1.amazonaws.com, or a MinIO URL)', '', tty)
  region = gather(resolve(args.region, 'ZOO_S3_REGION', stored.get('region')), 'Region', 'us-east-1', tty)
  bucket = gather(resolve(args.bucket, 'ZOO_S3_BUCKET', stored.get('bucket')), 'Bucket', '', tty)
  path_style = gather(resolve(args.path_style, 'ZOO_S3_PATH_STYLE', stored.get('path_style')),
                      'Path-style addressing? 1 for MinIO/R2, 0 for AWS virtual-host', '1', tty) == '1'
  access_key_id = gather(resolve(args.access_key_id, 'ZOO_S3_ACCESS_KEY_ID', stored.get('access_key_id')),
                         'Access key id', '', tty)
  secret = gather(resolve(args.secret_access_key, 'ZOO_S3_SECRET_ACCESS_KEY', stored.get('secret_access_key')),
                  'Secret access key', '', tty)
  pubkey_path = gather(args.pubkey,
                       'Path to armored PGP public key, empty for no encryption (gpg --export -a KEYID > pubkey.asc)', '', tty)
  prefix = gather(resolve(args.prefix, 'ZOO_S3_ARCHIVE_PREFIX', stored.get('archive_prefix')) or '',
                  'Archive prefix inside the bucket', last_prefix() or secrets.token_hex(8), tty)
  scope = gather(args.scope, 'Upload scope: minimal (qlog+qcam+crash+boot), full, or selected (kept routes only)',
                 'minimal', tty)
  budget = gather(args.budget, 'Archive ring budget, e.g. 8G: the device deletes oldest files to stay under it (empty = off)',
                  '', tty)
  if budget and parse_budget(budget) is None:
    print(f'archive ring budget not understood: {budget!r} (examples: 8G, 500M)')
    return 1

  missing = [name for name, value in (('endpoint', endpoint), ('bucket', bucket),
                                      ('access key id', access_key_id), ('secret', secret)) if not value]
  if missing:
    print(f'missing required fields (no terminal to prompt): {", ".join(missing)}')
    print('pass them as flags or ZOO_S3_* environment variables')
    return 1
  if endpoint.startswith('http://'):
    print('WARNING: plain http means the signed request is a short-lived bearer credential')
    print('on that wire. Fine for a trusted-LAN MinIO; use https in production.')

  if args.generate_key and not pubkey_path:
    pubkey_path = generate_key(args.generate_key)

  public_key = None
  recipients = None
  if pubkey_path:
    try:
      with open(pubkey_path) as f:
        public_key = f.read()
      recipients = pgp.load_recipients(public_key, strict_expiry=True)
      algos = ['cv25519' if r['key'].algo == pgp.ECDH_ALGO else f'RSA{r["key"].fields["rsa"].key_size}' for r in recipients]
      print(f'PGP key ok: {len(recipients)} usable encryption key(s) ({", ".join(algos)})')

      # a public key cannot prove the private half exists: write one probe the
      # user decrypts, so a wrong-key setup or an untested backup key fails
      # here instead of months later
      key_ids = ' '.join(r['key'].key_id().hex() for r in recipients)
      probe_path = os.path.join(os.path.dirname(os.path.abspath(pubkey_path)), 'zoo-key-check.asc')
      with open(probe_path, 'w') as f:
        f.write(pgp.armor(pgp.encrypt(f'zoo key check ok: {key_ids}'.encode(), recipients)))
      print(f'Key check: gpg -d {probe_path} must print "zoo key check ok" and your key id')
    except (OSError, pgp.PgpError) as e:
      print(f'PGP key rejected: {e}')
      return 1
  else:
    print('No PGP key: uploads store unencrypted, readable to anyone with')
    print('access to the bucket (the provider included).')

  if args.provider:
    print_provider_guide(args.provider, bucket, prefix, bool(budget))

  cfg = S3Config(endpoint=endpoint, region=region, bucket=bucket, access_key_id=access_key_id,
                 secret_access_key=secret, path_style=path_style)
  if args.self_test:
    import requests

    # probe objects sort by wall clock
    probe_key = f'{prefix}/_selftest/{int(time.time())}'  # noqa: TID251
    probe = pgp.encrypt(b'zoo self-test', recipients) if recipients else b'zoo self-test'
    url, headers, body = signed_put(cfg, probe_key, probe)
    r = requests.put(url, headers=headers, data=body, timeout=15)
    if r.status_code == 200:
      print(f'self-test ok: probe stored at {probe_key}')
      print('(put-only credentials cannot delete it; remove it with your admin credentials)')
    else:
      print(f'self-test FAILED: HTTP {r.status_code}: {r.text[:200]}')
      return 1

  remember_prefix(prefix)
  remember_device_creds({'endpoint': endpoint, 'region': region, 'bucket': bucket,
                         'path_style': '1' if path_style else '0', 'access_key_id': access_key_id,
                         'secret_access_key': secret, 'archive_prefix': prefix})

  values = [
    ('ZooS3Endpoint', endpoint),
    ('ZooS3Region', region),
    ('ZooS3Bucket', bucket),
    ('ZooS3PathStyle', '1' if path_style else '0'),
    ('ZooS3AccessKeyId', access_key_id),
    ('ZooS3SecretAccessKey', secret),
  ]
  if public_key:
    values.append(('ZooS3Encryption', '1'))
    values.append(('ZooS3PublicKey', public_key))
  values += [
    ('ZooS3Prefix', prefix),
    ('ZooS3Scope', scope),
    ('ZooS3Enabled', '1'),
  ]
  if budget:
    values.append(('ZooS3ArchiveBudget', budget))

  lines = apply_script(values)
  applied = False
  if args.apply:
    print()
    applied = apply_over_ssh(args.apply, lines)
    if applied:
      print('Params written:')
      for name, value in values:
        shown = '<redacted>' if name in ('ZooS3SecretAccessKey', 'ZooS3PublicKey') else value
        print(f'  {name} = {shown}')
    else:
      print()
      print('Manual path, paste these lines over SSH:')
      for line in lines:
        print(line)
      return 1
  else:
    print()
    print('Apply on the device over SSH:')
    for line in lines:
      print(line)
  print()
  if applied:
    print('No restart needed when the archive switch is already on: uploads start')
    print('within about half a minute. Otherwise flip Private S3 Archive in sunnylink.')
  else:
    print('Restart the device, or just toggle Private S3 Archive in sunnylink: the')
    print('daemon picks the params up within about half a minute.')
  if budget:
    print('Ring on: the device key needs listFiles and deleteFiles too, and bucket')
    print('versioning with an expiry window protects the archive from a leaked key.')
  return 0


if __name__ == '__main__':
  raise SystemExit(main())
