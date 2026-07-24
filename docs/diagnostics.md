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

| Element              | Topic / command       | Dir     | When                                                          |
|----------------------|-----------------------|---------|---------------------------------------------------------------|
| Health (existing)    | `{type}/{id}/status`  | dev→srv | auto on battery alert **+** response to `get_status`          |
| Diagnostics (new)    | `{type}/{id}/diag`    | dev→srv | auto when health ≥ `warning` **+** response to `get_diag`     |
| `get_status`         | `{type}/{id}/command` | srv→dev | on demand → device replies on `status`                        |
| `get_diag`           | `{type}/{id}/command` | srv→dev | on demand → device replies on `diag`                          |
| `set_confirm_uplink` | `{type}/{id}/command` | srv→dev | `{value:0\|1}` — toggle the uplink-delivery diagnostic (below) |

Commands are published **QoS 1, retain=false** (queued for deep-sleep devices via the persistent
session), like every other command.

**Capability advertisement.** `get_status`, `get_diag` and `set_confirm_uplink` are **not** listed
in the `commands` message. They are core diagnostics commands, present on every device, and the
server infers them from the **`"diag":1`** flag in the `capabilities` message (always emitted —
diagnostics is always-on, no build flag). This keeps the size-limited `commands` payload
(`MQTT_MAX_PACKET_SIZE`) for real module commands. (Likewise `ota_update` is inferred from
`"ota":1`.) Server rule: `supports_diag = capabilities.diag == 1`.

## Health model

Severity ladder: `ok` < `info` < `warning` < `error`. The evaluator returns the **highest**
matching level and a single dominant `message`:

| Level     | Conditions                                        | `message` examples                                         |
|-----------|---------------------------------------------------|------------------------------------------------------------|
| `error`   | reset = brownout/panic/watchdog; critical battery | `reset_brownout`, `reset_panic`, `critical_battery`        |
| `warning` | `miss>0`; low battery; RSSI < -80 dBm; low heap   | `missed_wakes`, `low_battery`, `weak_signal`, `low_memory` |
| `info`    | power-on / external reset; RSSI -70…-80 dBm       | `booted`, `fair_signal`                                    |
| `ok`      | nominal                                           | `ok`                                                       |

- **`get_status`** always replies `{"level":"…","message":"…"}` on `status`, even when `ok`
  (enum strings, not numeric codes — consistent with existing alerts, no code table to sync).
- **Auto-publish** the `diag` payload whenever level ≥ `warning`. Nominal wakes publish nothing
  extra (saves battery/bandwidth). `error` also raises a `status` alert.

## `diag` payload (fields)

Compact JSON within `MQTT_MAX_PACKET_SIZE` (512). Built by `format_diag_payload()`:

| Field                  | Source                    | Meaning                                                                |
|------------------------|---------------------------|------------------------------------------------------------------------|
| `level`                | `diag_evaluate()`         | `ok` / `info` / `warning` / `error`                                    |
| `message`              | `diag_evaluate()`         | dominant cause (enum string, e.g. `missed_wakes`)                      |
| `rst`                  | `platform_reset_cause()`  | 0=unknown 1=power-on 2=ext 3=sw 4=deep-sleep 5=brownout 6=panic 7=wdt  |
| `boot`                 | RTC counter               | boots since cold start (unexpected-reset detector)                     |
| `miss`                 | RTC counter               | consecutive **connect** failures since last publish                    |
| `wake_ms`              | `millis()` at publish     | wake→publish duration (slow-connect detector)                          |
| `seq`                  | RTC counter               | monotonic publish counter — a gap = lost publish(es)                   |
| `pubfail`              | RTC counter               | `publish()` calls that returned false since last publish               |
| `txsent`               | RTC counter               | publishes attempted while uplink-confirm mode is on (absent otherwise) |
| `txok`                 | RTC counter               | of those, confirmed delivered via broker loopback (absent otherwise)   |
| `wf`                   | RTC counter               | cumulative WiFi/DHCP-connect failures (absent when 0)                  |
| `mf`                   | RTC counter               | cumulative MQTT-connect failures, WiFi was up (absent when 0)          |
| `sf`                   | RTC counter               | cumulative wakes skipped on invalid sensor read (absent when 0)        |
| `bx`                   | RTC counter               | cumulative boots whose reset cause ≠ deep-sleep (absent when 0)        |
| `rssi`                 | WiFi                      | link strength at connect (dBm)                                         |
| `heap`                 | `platform_free_heap()`    | free heap bytes (0 = n/a, e.g. SAMD)                                   |
| `bat`                  | battery module            | SoC % — omitted when the device has no battery                         |
| `fw`/`hw`/`hwrev`/`id` | build constants + chip id | firmware version / hardware code / revision / chip serial              |

The `seq` counter (RTC-backed, survives deep sleep) closes the "lost publishes" blind spot below:
a gap between two `diag` reports' `seq` values means a publish was attempted but never received.

## Uplink-delivery confirmation (opt-in diagnostic)

`seq`/`pubfail`/`miss` cannot see a publish that is **sent cleanly but lost in transit**
(QoS-0 fire-and-forget: `publish()` returns true, the bytes leave, but the broker never
receives them — the residual loss in the "known blind spot" section). To *measure* that loss
directly, the device can confirm each publish end-to-end:

- **Toggle** with `set_confirm_uplink` (`{value:1}` on, `{value:0}` off; default **off**,
  persisted across sleep so it survives a wake). Costs nothing when off. Works on **all
  platforms**: the ESP deep-sleep path and the SAMD duty-cycle path both subscribe to the
  loopback and count confirmations.
- **Mechanism (broker loopback, no server change):** when on, the device subscribes to its
  **own `sensors` topic**. After publishing, if the broker loops the payload back (downlink =
  the strong RX direction), that proves the publish reached the broker — TCP is ordered, so
  the loopback implies the PUBLISH was received. Confirmation arrives inside the existing
  `MQTT_COMMAND_WAIT_MS` window.
- **Counters:** `txsent` (attempts) and `txok` (confirmed) accumulate in RTC and ride the
  `diag` payload. While the mode is on, `diag` is published **every wake** (not just on
  `warning`), so nominal wakes still report the counters. The server derives the
  confirmed-delivery rate as `txok / txsent`.
- **Off-by-one:** a wake's own loopback arrives *after* its `diag` was sent, so the newest
  attempt shows as pending until the next wake. The counters are cumulative, so the ratio is
  unaffected — only the single latest sample lags.

## Wake-path instrumentation (`wf` / `mf` / `sf` / `bx`)

Cumulative, **never-reset** RTC counters that attribute *where* a wake is lost — unlike `miss`,
which is cleared on every successful wake and so only shows the current burst. A wake reaches
`publish_sensor_data()` only after it clears WiFi assoc + IP, MQTT connect, and a valid sensor
read; each stage that aborts a wake bumps one counter:

| Counter | Bumped when …                                                                             |
|---------|-------------------------------------------------------------------------------------------|
| `wf`    | WiFi (assoc + DHCP) connect fails                                                         |
| `mf`    | MQTT connect fails (WiFi was up)                                                          |
| `sf`    | the sensor read is invalid, so the wake publishes nothing                                 |
| `bx`    | a boot's reset cause ≠ deep-sleep timer (cold start, brownout, or a spurious double-boot) |

Emitted in the `diag` payload **only when non-zero** (short keys, to stay within 512 B), so a
healthy node's payload is unchanged. Portable (fields live in `DiagCounters`), but most useful
on the ESP32-C3, where they localised the loss to `mf` (MQTT/TCP connect) with `wf`/`sf` at 0
and `bx` at 1 (only the cold boot). Derived: `boot − bx − (wf+mf+sf)` = wakes that actually
published — independent of `seq`, which the SYNC resends inflate.

**Server:** treat `wf`/`mf`/`sf`/`bx` like `txsent`/`txok` — optional integer columns, absent on
a normal wake. A rising `mf` with a strong `rssi` and `rst=4` is the C3 TCP-connect stall.

This turns the ~QoS-0 transit loss from an inferred quantity (server-side cadence gaps) into a
device-reported metric, and is the groundwork for a future confirmed-delivery / store-and-forward
publish path.

## Cross-platform

Reset cause and heap are MCU-specific; normalized behind `platform_diag.*`
(`platform_reset_cause()` → `ResetCause` enum, `platform_free_heap()`), same `#if`-per-platform
pattern as `hw_id.*`. The core `diagnostics` module stays platform-agnostic and receives values.

- ESP32: `esp_reset_reason()`, `ESP.getFreeHeap()`
- ESP8266: `ESP.getResetInfoPtr()->reason`, `ESP.getFreeHeap()` (no native brownout code)
- SAMD21: `PM->RCAUSE`, no simple heap metric (`heap`=0)

The RTC counters (`boot`/`seq`/`miss`/`pubfail`/`txsent`/`txok` + the confirm flag) are held in a
portable `DiagCounters` struct (`diagnostics.h`) and persisted across a sleep cycle per platform:

- **ESP32**: `RTC_DATA_ATTR` — retained through deep sleep automatically.
- **ESP8266**: no `RTC_DATA_ATTR`; the struct is saved to / restored from a dedicated **RTC
  user-memory** slot around deep sleep (`EspSleep::read/write_diag_counters`, called via
  `diag_counters_restore/persist` in `main.cpp`).
- **SAMD21 / native**: plain RAM — SAMD standby resumes execution (no reset), so the struct
  survives; native runs once.

`main.cpp` wraps every deep-sleep entry in `enter_deep_sleep()` (persist counters, then sleep) so
no update is lost across a wake.

## Server-side wiring

- Subscribe `{type}/{id}/status` and `{type}/{id}/diag`.
- Store `diag` payloads as a time series keyed by device; surface `rst`, `miss`, `wake_ms`,
  `rssi`, `heap` on a device health page.
- Alert rules: `rst=5` (brownout) → hardware alert; repeated `miss>0` → link/antenna alert;
  `rst∈{6,7}` → firmware crash alert.
- `get_status` / `get_diag` are commands on `{type}/{id}/command` (QoS 1); expect the reply on
  `status` / `diag` on the device's next wake (deep-sleep latency = up to one publish interval).

### Uplink-confirm counters (`txsent` / `txok`)

The `diag` payload carries `txsent`/`txok` **only while `set_confirm_uplink` is on** (both
absent otherwise). To surface the confirmed-delivery rate server-side:

- **Store**: add nullable integer columns `txsent`, `txok` to the diag-log store and read them
  in the diag handler (`data.get("txsent")` / `data.get("txok")`; keep them optional — a normal
  diag omits them). No change is needed to keep working: unknown fields are already ignored.
- **Display / metric**: confirmed-delivery rate = `txok / txsent` over a window (use deltas
  across two reports, like `seq`). A rate well below 100 % with `miss=0` and `rst=4` = uplink
  transit loss (marginal TX / link), **not** a power or firmware fault.
- **No server change required to still benefit**: while the mode is on the device publishes
  `diag` **every wake**, so `seq` is sampled densely — transit loss is computable as
  `Δseq − Δ(received sensors)` even without the new columns.
- **Sending the toggle**: there is no generic command UI; publish it directly, e.g.
  `mosquitto_pub -q 1 -t {type}/{id}/command -m '{"action":"set_confirm_uplink","value":1}'`.

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
