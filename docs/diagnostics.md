# Diagnostics & Health

Status: **implemented** — always-on, no build flag, on every device type.
A robust health + fault-diagnosis layer shared by all device types. Complements the existing
`status` alerts and the sensor payload; lets the server observe *why* a node misbehaves without
serial access (deep-sleep nodes have none in the field).

Code: `lib/thermo_core/src/diagnostics.*` (portable evaluator + payload formatter),
`lib/thermo_platform/src/platform_diag.*` (reset cause + heap), glue in `src/main.cpp`.

## Goals

1. `get_status` command → on-demand health summary (`ok` / `low_battery` / …).
2. Move the technical diagnostics (`rst`, `miss`, `wake_ms`, …) **out of the `sensors` payload**
   into a dedicated **`diag`** topic, published **only when there is a problem**.
3. `get_diag` command → force a `diag` publish on demand.
4. Enabled on **all devices**, **always on** (no build flag).
5. Server-side wiring documented (this file + `docs/mqtt-protocol.md`).

## Topics & commands

| Element              | Topic / command             | Dir     | When                                                             |
|----------------------|-----------------------------|---------|------------------------------------------------------------------|
| Health (existing)    | `{type}/{id}/status`        | dev→srv | auto on battery alert **+** response to `get_status`             |
| Diagnostics (new)    | `{type}/{id}/diag`          | dev→srv | auto when health ≥ `warning` **+** response to `get_diag`        |
| `get_status`         | `{type}/{id}/command`       | srv→dev | on demand → device replies on `status`                          |
| `get_diag`           | `{type}/{id}/command`       | srv→dev | on demand → device replies on `diag`                            |

Commands are published **QoS 1, retain=false** (queued for deep-sleep devices via the persistent
session), like every other command.

## Health model

Severity ladder: `ok` < `info` < `warning` < `error`. The evaluator returns the **highest**
matching level and a single dominant `message`:

| Level     | Conditions                                                    | `message` examples                          |
|-----------|---------------------------------------------------------------|---------------------------------------------|
| `error`   | reset = brownout/panic/watchdog; critical battery             | `reset_brownout`, `reset_panic`, `critical_battery` |
| `warning` | `miss>0`; low battery; RSSI < -80 dBm; low heap               | `missed_wakes`, `low_battery`, `weak_signal`, `low_memory` |
| `info`    | power-on / external reset; RSSI -70…-80 dBm                   | `booted`, `fair_signal`                     |
| `ok`      | nominal                                                       | `ok`                                        |

- **`get_status`** always replies `{"level":"…","message":"…"}` on `status`, even when `ok`
  (enum strings, not numeric codes — consistent with existing alerts, no code table to sync).
- **Auto-publish** the `diag` payload whenever level ≥ `warning`. Nominal wakes publish nothing
  extra (saves battery/bandwidth). `error` also raises a `status` alert.

## `diag` payload (fields)

Compact JSON within `MQTT_MAX_PACKET_SIZE` (512). Built by `format_diag_payload()`:

| Field                  | Source                    | Meaning                                                 |
|------------------------|---------------------------|---------------------------------------------------------|
| `level`                | `diag_evaluate()`         | `ok` / `info` / `warning` / `error`                     |
| `message`              | `diag_evaluate()`         | dominant cause (enum string, e.g. `missed_wakes`)       |
| `rst`                  | `platform_reset_cause()`  | 0=unknown 1=power-on 2=ext 3=sw 4=deep-sleep 5=brownout 6=panic 7=wdt |
| `boot`                 | RTC counter               | boots since cold start (unexpected-reset detector)      |
| `miss`                 | RTC counter               | consecutive **connect** failures since last publish     |
| `wake_ms`              | `millis()` at publish     | wake→publish duration (slow-connect detector)           |
| `seq`                  | RTC counter               | monotonic publish counter — a gap = lost publish(es)    |
| `pubfail`              | RTC counter               | `publish()` calls that returned false since last publish |
| `rssi`                 | WiFi                      | link strength at connect (dBm)                          |
| `heap`                 | `platform_free_heap()`    | free heap bytes (0 = n/a, e.g. SAMD)                    |
| `bat`                  | battery module            | SoC % — omitted when the device has no battery          |
| `fw`/`hw`/`hwrev`/`id` | build constants + chip id | firmware version / hardware code / revision / chip serial |

The `seq` counter (RTC-backed, survives deep sleep) closes the "lost publishes" blind spot below:
a gap between two `diag` reports' `seq` values means a publish was attempted but never received.

## Cross-platform

Reset cause and heap are MCU-specific; normalized behind `platform_diag.*`
(`platform_reset_cause()` → `ResetCause` enum, `platform_free_heap()`), same `#if`-per-platform
pattern as `hw_id.*`. The core `diagnostics` module stays platform-agnostic and receives values.

- ESP32: `esp_reset_reason()`, `ESP.getFreeHeap()`
- ESP8266: `ESP.getResetInfoPtr()->reason`, `ESP.getFreeHeap()` (no native brownout code)
- SAMD21: `PM->RCAUSE`, no simple heap metric (`heap`=0)

## Server-side wiring

- Subscribe `{type}/{id}/status` and `{type}/{id}/diag`.
- Store `diag` payloads as a time series keyed by device; surface `rst`, `miss`, `wake_ms`,
  `rssi`, `heap` on a device health page.
- Alert rules: `rst=5` (brownout) → hardware alert; repeated `miss>0` → link/antenna alert;
  `rst∈{6,7}` → firmware crash alert.
- `get_status` / `get_diag` are commands on `{type}/{id}/command` (QoS 1); expect the reply on
  `status` / `diag` on the device's next wake (deep-sleep latency = up to one publish interval).

## Implementation phases

1. ✅ `platform_diag.*` (portable reset cause + heap).
2. ✅ `diagnostics` (thermo_core): `HealthLevel`, `diag_evaluate()`, `format_diag_payload()`.
3. ✅ `diag` topic in `device_topics`.
4. ✅ `main.cpp`: RTC `boot`/`seq`/`miss`/`pubfail` counters, auto-publish on `warning`,
   `get_status`/`get_diag` handlers, **removed `rst`/`miss`/`wake_ms`/`seq`/`pubfail` from the
   `sensors` payload** (only `rssi` remains there).
5. ✅ Docs: `mqtt-protocol.md` (topics/commands/schema + server wiring), `CLAUDE.md` tables.
6. ✅ Native tests: `test_diagnostics.cpp` (level logic + payload format), `diag` topic assertion.

## Known blind spot — lost publishes vs `miss` (closed by `seq`)

`miss` counts only **connect failures** (WiFi/MQTT connect returned false). A record can be lost
**after** a successful connect — e.g. a QoS-0 sensor publish dropped on a marginal link, or a
brownout during the publish TX — with `miss=0`. The **monotonic `seq` counter** in the `diag`
payload closes this: a gap in `seq` between two reports = a publish attempted but not received.
This was the key signal in the C3 stability investigation (2026-07) that isolated a residual
~17% QoS-0 device→broker publish loss (`miss=0`, `pubfail=0`, clean link).
