# zoo backend

Point this device at a self-hosted [zoo](https://github.com/mzdnick/zoo) server
instead of comma connect. Uploads, pairing, and the athena live channel all
switch; nothing else changes.

## One-time setup

Over SSH, set the server URL once:

```
printf 'http://192.168.1.50:8000' > /data/params/d/ZooApiUrl
```

Use `https://…` when the server has TLS (e.g. behind `tailscale serve`).

## Switching

Toggle **Settings → Device → zoo backend** and restart the device, or over SSH:

```
printf '1' > /data/params/d/ZooBackendEnabled   # on
printf '0' > /data/params/d/ZooBackendEnabled   # off (back to comma)
```

The toggle takes effect on the next manager start.

## Registering with the server

Nothing to do by hand. The device registers against whatever backend it
boots with, and when the toggle CHANGES, the old backend's dongle identity
is cleared automatically at the next boot so the device re-registers with
the new one (comma → zoo or zoo → comma). After a switch, open
**Settings → Device → Pair Device** — the QR points at the zoo web UI when
the toggle is on — and claim the device in the zoo web app.

The only manual step left is setting the server URL once (SSH above, or any
params editor of your choice).

## How it works

`launch_env.sh` reads the two params at boot and exports `API_HOST`,
`ATHENA_HOST` (ws/wss derived from the URL scheme), and `ZOO_BACKEND_ACTIVE`
for every openpilot process. The pairing dialogs read
`openpilot/common/api/backend.py` to build the QR URL for the active backend.
