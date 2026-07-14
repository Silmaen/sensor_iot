# OTA Update Module

## Overview

The OTA module lets the server push a new firmware image to a device **over WiFi**. It is
enabled by the `-DHAS_OTA` build flag (ESP8266 and ESP32-C3 only — the SAMD/MKR has no OTA).

It adds no metrics. It registers one command, `ota_update`, and advertises the capability
via `"ota":1` in the [capabilities message](../mqtt-protocol.md). The full protocol contract
(server + device) lives in [ota-calibration-protocol.md](../ota-calibration-protocol.md).

**Model:** server-push, pull-triggered. The device never updates on its own — it downloads
and flashes only when it receives an `ota_update` command. The image is fetched by HTTP from
the internal update server and verified before the device switches to it.

## Enabling

```ini
[env:sensor_8266_bmp80]
build_flags =
    ...
    -DHAS_OTA
    ; optional: minimum battery % to allow an update (default 40)
    -DOTA_MIN_BATTERY_PCT=40
```

The image is device-agnostic: a single binary per `(HW_CODE, HW_REV)` serves every unit of
that hardware type. `device_id` and calibration are **not** compiled in — they live in the
runtime config store (see [calibration](calibration.md) and the protocol doc), so an OTA
(which rewrites only the application partition) preserves them.

## The `ota_update` command

Sent by the server on `thermo/{id}/command` (QoS 1, so it is queued for a sleeping device):

```json
{
  "action": "ota_update",
  "value":  "http://srv.interne/fw/E8BMEBAT/1/1.1.0.bin",
  "md5":    "26a9ca3b519aab2ea24755c3b106b0da",
  "ver":    "1.1.0",
  "hw":     "E8BMEBAT",
  "hwrev":  1
}
```

| Field   | Meaning                                                           |
|---------|-------------------------------------------------------------------|
| `value` | HTTP URL of the image on the internal server                      |
| `md5`   | expected MD5 of the image (integrity check)                       |
| `ver`   | target version — the device refuses if it already runs it         |
| `hw`    | target hardware code — must match the device's compiled `HW_CODE` |
| `hwrev` | target hardware revision — must match the device's `HW_REV`       |

## Guards (checked in order)

Before flashing, the device rejects the update — publishing an error ack — in these cases:

| Reason         | Condition                                                         |
|----------------|-------------------------------------------------------------------|
| `hw_mismatch`  | `hw`/`hwrev` differ from the device's compiled `HW_CODE`/`HW_REV` |
| `same_version` | `ver` equals the running `FIRMWARE_VERSION` (idempotence)         |
| `low_battery`  | battery below `OTA_MIN_BATTERY_PCT` (only if `HAS_BATTERY`)       |
| `bad_request`  | a required field is missing/malformed                             |

## Acknowledgements

Acks are published on `thermo/{id}/ack`:

```json
{"action":"ota_update","status":"start"}
{"action":"ota_update","status":"error","message":"hw_mismatch"}
```

- `status:"start"` is published just before flashing (informational).
- `status:"error"` (with a `message` from the table above, plus `download_failed` /
  `md5_mismatch`) means the update was refused or failed — the **old firmware stays active**
  (OTA is atomic).
- **Success is not acked**: the device reboots into the new image. The server confirms
  success when the next `capabilities` message carries the new `fw`.

## Deep-sleep behaviour

The command is handled during the brief awake window after a wake-up. The download+flash is
**blocking** and ends with a reboot, so the device does not return to deep sleep during an
update. No change to the normal sleep cycle is needed. Latency: an update issued while the
device sleeps takes effect on its next wake (≤ `publish_interval`).

## Security (v1)

The IoT network is a filtered LAN with no Internet and a trusted internal image server, so v1
uses **plain HTTP + MD5** (integrity, not authentication):

- **ESP8266** verifies the `md5` before switching partitions (`ESPhttpUpdate.setMD5sum`).
- **ESP32-C3** currently relies on the app image's embedded SHA256 (verified by the
  bootloader); explicit MD5 is a planned v1.1 hardening. The `md5` field is still sent.

Signed images (RSA/SHA256) are an optional future hardening if the LAN trust model changes.

## Publishing an image

Building an env emits `firmware.bin` plus a `firmware.json` manifest (via
`tools/gen_manifest.py`). To publish it to the server registry + catalog + hosting:

```bash
# builds the env, reads the manifest, calls the server publication API
OTA_API_URL=http://srv.interne OTA_PUBLISH_TOKEN=... \
  python3 tools/publish_firmware.py sensor_8266_bmp80
```

Publishing does **not** deploy: pushing to a device is a separate, explicit server action.
See [ota-calibration-protocol.md §8](../ota-calibration-protocol.md) for the manifest format
and the publication API.

## First provisioning

A production image has no compiled `device_id`. On first boot the device enters serial
provisioning:

```
provision thermo_1     # assigns and stores the device_id, then reboots
factory_reset          # wipes the store (device_id + calibration)
```

After provisioning, the server re-pushes the unit's calibration (it reports `cal:0` until
then). See the protocol doc §3.1–§4.3.
