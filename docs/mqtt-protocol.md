# MQTT Protocol — Device Side

This document describes the MQTT protocol that thermo devices must implement
to communicate with the sensor_server. It is the client-side counterpart to
the server's `docs/mqtt-protocol.md` (in `../sensor_server/`).

**Related docs:** [architecture](architecture.md) | [configurations](configurations.md)

## Topic structure

All topics follow:

```text
{device_type}/{device_id}/{message_type}
```

`device_type` is a build flag (`-DMQTT_DEVICE_TYPE`); `device_id` is **provisioned at
runtime** (config store, not a build flag) and topics are built from it at boot. See
[ota-calibration-protocol.md](ota-calibration-protocol.md) §2.

| Message type   | Direction        | Topic example                  |
|----------------|------------------|--------------------------------|
| `sensors`      | Device -> Server | `thermo/thermo_1/sensors`      |
| `status`       | Device -> Server | `thermo/thermo_1/status`       |
| `command`      | Server -> Device | `thermo/thermo_1/command`      |
| `capabilities` | Device -> Server | `thermo/thermo_1/capabilities` |
| `commands`     | Device -> Server | `thermo/thermo_1/commands`     |
| `calibration`  | Device -> Server | `thermo/thermo_1/calibration`  |
| `ack`          | Device -> Server | `thermo/thermo_1/ack`          |

> **OTA, calibration & hardware versioning** are specified in full in
> [ota-calibration-protocol.md](ota-calibration-protocol.md). This document covers the base
> protocol; the sections below note where that spec extends or supersedes it.

## Connection & session model

Most thermo nodes are battery powered and spend ~99% of their time in deep
sleep: they are only connected for a ~2 s window once per `publish_interval`
(default 300 s). To make server→device commands survive that offline time, the
connection is set up as follows:

| Setting         | Value                    | Why                                                                 |
|-----------------|--------------------------|---------------------------------------------------------------------|
| Client ID       | `device_id` (stable)     | The broker ties the persistent session to this id across wakes.     |
| Clean session   | **`false`**              | Broker keeps the subscription and **queues** commands while asleep. |
| `command` sub   | **QoS 1**                | Only QoS ≥ 1 messages are queued for an offline session.            |
| Last Will (LWT) | **none**                 | Online status is derived from `last_seen`, not LWT (see below).     |
| Keepalive       | 15 s (PubSubClient dflt) | Short; the connection only lives for the brief awake window.        |

**Consequence for whoever publishes commands (the server, or any integrator):**

- Publish on `{type}/{id}/command` with **QoS 1** so the broker queues the
  message for a sleeping device and delivers it on the next reconnect. A QoS 0
  command sent while the device is asleep is silently dropped.
- Expect **latency**: a command issued while the device sleeps takes effect on
  its next wake (≤ `publish_interval` later). This is by design.
- The broker must persist sessions (`persistence true` in `mosquitto.conf`, no
  `persistent_client_expiration`). This is the default in this project.

QoS/retain used per message type:

| Message        | Direction        | QoS   | Retain | Notes                                                |
|----------------|------------------|-------|--------|------------------------------------------------------|
| `sensors`      | Device -> Server | 0     | no     | Device publishes at QoS 0 (server is always online). |
| `status`       | Device -> Server | 0     | no     | Alert JSON, see §2.                                  |
| `capabilities` | Device -> Server | 0     | no     | Sent in response to `request_capabilities`.          |
| `commands`     | Device -> Server | 0     | no     | Sent in response to `request_commands`.              |
| `calibration`  | Device -> Server | 0     | no     | Sent in response to `request_calibration`.           |
| `ack`          | Device -> Server | 0     | no     | Command acknowledgement, see §4.                     |
| `command`      | Server -> Device | **1** | no     | QoS 1 so it is queued for sleeping devices.          |

> The device firmware (PubSubClient) only **publishes** at QoS 0, which is fine
> because the server is always connected. Reliability is only needed in the
> server→device direction, which QoS 1 + the persistent session provide.

## 1. Publishing sensor readings (`sensors`)

Publish a JSON object mapping metric names to numeric values. The payload
is built dynamically by `PayloadBuilder` — each enabled module contributes
its fields (see [architecture.md](architecture.md)).

Devices publish **compact metric names** (`temp`, `humi`, `bat`, …) to keep
payloads small. The server maps them to canonical names for storage (see
*Metric reference* below).

**Example with `HAS_BME280` only:**

```json
{
  "temp": 22.5,
  "humi": 45.2,
  "press": 1013.1
}
```

**Example with `HAS_BME280` + `HAS_BATTERY`** (also carries `rssi`):

```json
{
  "temp": 22.0,
  "humi": 50.0,
  "press": 1000.0,
  "bat": 75,
  "batv": 7.80,
  "rssi": -47
}
```

**Constraints:**

- Payload must be valid JSON, max 10 KB
- Metric names: alphanumeric, `-`, `_`, max 64 chars
- Values: must be valid numbers (float)
- Publish with `retain = false`

### Metric reference

The device emits the **name sent on the wire** (compact); the server stores it
under the **canonical name** via its alias map (`METRIC_ALIASES` in
`../sensor_server/`).

| Sent (device) | Canonical (server) | Module                                     | Type          | Unit | Description                                        |
|---------------|--------------------|--------------------------------------------|---------------|------|----------------------------------------------------|
| `temp`        | `temperature`      | `HAS_BME280` / `HAS_SHT30` / `HAS_MKR_ENV` | float (1 dec) | °C   | Ambient temperature                                |
| `humi`        | `humidity`         | `HAS_BME280` / `HAS_SHT30` / `HAS_MKR_ENV` | float (1 dec) | % RH | Relative humidity                                  |
| `press`       | `pressure`         | `HAS_BME280` / `HAS_MKR_ENV`               | float (1 dec) | hPa  | Atmospheric pressure                               |
| `bat`         | `bat_percent`      | `HAS_BATTERY`                              | uint          | %    | Battery state of charge (0-100)                    |
| `batv`        | `bat_voltage`      | `HAS_BATTERY`                              | float (2 dec) | V    | Battery voltage (ESP 2S 6.0-8.3V, MKR 1S 3.3-4.1V) |
| `rssi`        | `rssi`             | core                                       | int           | dBm  | WiFi signal strength                               |
| `light`       | `light`            | `HAS_LIGHT`                                | uint          | %    | Relative light level (0 = dark, 100 = bright)      |
| `lux`         | `lux`              | `HAS_BH1750`                               | float (0 dec) | lx   | Calibrated illuminance (1-65535 lux)               |
| `uv`          | `uv_index`         | `HAS_MKR_ENV`                              | float         | —    | UV index                                           |
| `relay1`      | `relay1`           | `HAS_RELAY`                                | uint          | —    | Relay 1 state (0 = off, 1 = energized)             |
| `relay2`      | `relay2`           | `HAS_RELAY`                                | uint          | —    | Relay 2 state (0 = off, 1 = energized)             |

New modules can add additional metrics. The server auto-discovers metric names
from the sensor readings and capabilities message; add an entry to the server's
`METRIC_ALIASES` if a new compact name needs a canonical mapping.

## 2. Publishing status alerts (`status`)

Publish structured alerts when the device detects a problem or recovers.

```json
{
  "level": "warning",
  "message": "low_battery"
}
```

| Field     | Type   | Required | Values                               |
|-----------|--------|:--------:|--------------------------------------|
| `level`   | string |   Yes    | `"warning"`, `"error"`, or `"ok"`    |
| `message` | string |    No    | Human-readable detail, max 256 chars |

- `"ok"` (or `""`) **clears** any existing alert on the server.
- `"warning"`: degraded state (e.g. low battery, sensor drift).
- `"error"`: failure (e.g. sensor hardware fault).

### Automatic alerts

| Condition                     | Level     | Message            | Module        |
|-------------------------------|-----------|--------------------|---------------|
| Battery SoC <= warn threshold | `warning` | `low_battery`      | `HAS_BATTERY` |
| Battery SoC <= crit threshold | `error`   | `critical_battery` | `HAS_BATTERY` |

Thresholds are platform-specific (defined in `config.h`):

| Platform           | Warning | Critical |
|--------------------|---------|----------|
| ESP8266 (2S LiPo)  | 15%     | 5%       |
| MKR WiFi (1S LiPo) | 20%     | 5%       |

**Important:** Do NOT publish online/offline status. The server computes
online/offline from the timestamp of the last received message:

```json
is_online = (now - last_seen) < 3 * publish_interval
```

If `publish_interval` is unknown, the server uses a 5-minute default timeout.

**Do NOT use MQTT Last Will and Testament (LWT).** It conflicts with the
server's status topic expectations.

## 3. Receiving commands (`command`)

Subscribe to `{device_type}/{device_id}/command` to receive server commands.

**Payload format:**

```json
{
  "action": "<action_name>",
  "value": <optional_value>
}
```

### Command dispatch

Commands are routed through the `ModuleRegistry`:

1. Parse `action` field from JSON payload
2. If `action` is `request_capabilities` → respond with capabilities (section 4)
3. Otherwise → `registry.dispatch(action, payload)` iterates registered
   handlers and calls the first match

### Built-in commands

| Action                 | Value                                   | Handler     | Behavior                                        |
|------------------------|-----------------------------------------|-------------|-------------------------------------------------|
| `set_interval`         | seconds (1-86400)                       | Core        | Updates publish/sleep interval                  |
| `request_capabilities` | _(none)_                                | Core        | Responds with capabilities (section 5)          |
| `request_commands`     | _(none)_                                | Core        | Responds with the command list on `commands`    |
| `set_offset`           | `{"metric":"<name>","value":<float>}`   | Calibration | Set sensor offset (temp/humi/press)             |
| `set_calibration`      | `{"key":"bat_divider","value":<float>}` | Calibration | Set a generic calibration value (divider ratio) |
| `request_calibration`  | _(none)_                                | Calibration | Publishes current calibration on `calibration`  |
| `ota_update`           | see [OTA module](modules/ota.md)        | OTA         | Download + verify + flash new firmware, reboot  |
| `relay_toggle`         | `{"value":<1\|2>}`                      | Relay       | Toggle relay on/off                             |
| `relay_contact`        | `{"relay":<1\|2>,"value":<ms>}`         | Relay       | Activate relay, auto-revert after delay         |

> `request_calibration` now publishes on the dedicated `calibration` topic (not `ack`), and
> `ota_update` emits its own detailed ack (`start` / `error`+`message`). See
> [ota-calibration-protocol.md](ota-calibration-protocol.md) §4, §6.

Modules can register additional commands via `reg.add_command()`. These
commands automatically appear in the capabilities message and are routed
by the dispatcher.

### Implementation notes

- **Continuous mode** (no `HAS_DEEP_SLEEP`): subscribe at startup, process
  commands in `loop()` via `network.loop()`. Always connected, no special
  handling needed.
- **One-shot mode** (`HAS_DEEP_SLEEP`) and **duty-cycle mode** (MKR WiFi):
  1. Connect (persistent session, see *Connection & session model*) and
     subscribe to the command topic at QoS 1.
  2. On reconnect the broker immediately delivers any commands it **queued**
     while the device slept (they were published QoS 1).
  3. Publish sensor data.
  4. Keep the connection open for `MQTT_COMMAND_WAIT_MS` (**2 s**), pumping
     `network.loop()` so queued/arriving commands are processed and acked.
  5. If a `request_capabilities` was among them, the capabilities response is
     published during that window.
  6. Disconnect and sleep / power down radio. The persistent session (and any
     still-undelivered commands) survives on the broker until the next wake.
- Multiple commands may arrive per wake cycle — the firmware processes all
  of them during the wait window.
- `request_capabilities` is not acked — the capabilities response serves as
  implicit acknowledgement.
- Unknown actions are acked with `"status": "error"`.

## 4. Acknowledging commands (`ack`)

After processing a dispatched command, the device publishes an acknowledgement
on the `ack` topic. This is **not** sent for `request_capabilities` (which has
its own response via the `capabilities` topic).

```json
{
  "action": "set_interval",
  "status": "ok"
}
```

| Field    | Type   | Required | Description                                     |
|----------|--------|:--------:|-------------------------------------------------|
| `action` | string |   Yes    | The command action being acknowledged           |
| `status` | string |   Yes    | `"ok"` (command executed) or `"error"` (failed) |

- Acks are sent immediately after command processing.
- The `action` field must exactly match the `action` in the original command.
- Unknown commands are acked with `"status": "error"`.

## 5. Responding with capabilities (`capabilities`)

When the device receives `{"action": "request_capabilities"}`, it must publish
its capabilities within **60 seconds** on the `capabilities` topic.

The capabilities message is built dynamically from the `ModuleRegistry` —
it reflects exactly which modules are enabled in the current firmware build.

The message carries **identity + metrics only** — the command list moved to a separate
`commands` message (§5b) so each payload fits `MQTT_MAX_PACKET_SIZE` (512 B). See
[ota-calibration-protocol.md](ota-calibration-protocol.md) §5.

**Example with `HAS_BME280` + `HAS_BATTERY` + `HAS_OTA`:**

```json
{
  "id": "ESP-00A1B2",
  "hw": "E8BMEBAT",
  "hwrev": 1,
  "fw": "1.0.0",
  "ota": 1,
  "cal": 1,
  "intrvl": 300,
  "metrics": {"rssi": "dBm", "temp": "°C", "humi": "%", "press": "hPa", "bat": "%", "batv": "V"}
}
```

| Field     | Type   | Required | Description                                                                       |
|-----------|--------|:--------:|-----------------------------------------------------------------------------------|
| `id`      | string |   Yes    | Unique chip serial. Per unit                                                      |
| `hw`      | string |   Yes    | Hardware code — fixed 8 chars `^[A-Z0-9]{8}$`, identical across units of a type   |
| `hwrev`   | number |   Yes    | Hardware revision (physical/electrical), selects the compatible image with `hw`   |
| `fw`      | string |   Yes    | Firmware version (semver). Compare against latest image for `(hw, hwrev)`         |
| `ota`     | 0/1    |   Yes    | 1 if the device can perform OTA updates (server only offers updates then)         |
| `cal`     | 0/1    |   Yes    | 1 if the calibration store holds any value; 0 (fresh/reset) → server re-pushes it |
| `intrvl`  | number |   Yes    | Current publish frequency in seconds (1-86400)                                    |
| `metrics` | object |   Yes    | Metric name → unit string (`""` if no unit). Max 16 chars per unit                |

- `intrvl` reflects the current value (may have been changed by `set_interval`).
- Publish with `retain = false`.

### 5b. Command list (`commands`)

In response to `request_commands`, the device publishes the actionable command list on the
`commands` topic (Device → Server), within 60 s:

```json
{
  "commands": ["set_interval", "set_offset", "set_calibration", "request_calibration", "ota_update"],
  "command_params": {
    "set_interval": [{"n": "value", "t": "number"}],
    "set_offset":   [{"n": "metric", "t": "string"}, {"n": "value", "t": "number"}]
  }
}
```

- `commands` lists every registered action; `command_params` holds `{n, t}` params only for
  the commands that declare them (compact keys `n`/`t` to fit 512 B). Params: `t` ∈
  `number | string | boolean`.
- `request_capabilities` / `request_commands` are implicit and not listed.

## 6. Timeout handling

If the device does not respond to `request_capabilities`, the server flags it
with `alert_level = "error"` and `alert_message = "no_capabilities_response"`.
This is automatically cleared when the device sends a valid capabilities
response.

The response deadline is **`max(60 s, 2 × publish_interval)`**: a deep-sleep
device only sees the queued request on its next wake, so the server waits at
least two publish intervals before flagging a missing response. Do not expect
capabilities from a sleeping device sooner than its next wake.

The server requests capabilities in three situations:

1. **First discovery** — first message from an unknown device
2. **Reconnection** — device was offline and starts publishing again
3. **After every command** — to refresh capabilities that may have changed

## 7. Authentication

MQTT broker requires username/password authentication (from `credentials.h`).
Anonymous access is disabled.

## 8. Device lifecycle (from server perspective)

```json
Unknown
  |  First sensors message
  v
Discovered (unapproved) --> sensor data DROPPED until admin approves
  |  Admin approves
  v
Approved (active) --> data stored, commands enabled
  |
  |-- No data for 3x interval --> Offline
  |     |  Data received again --> Reconnected (capabilities re-requested)
  |
  |-- Admin revokes --> data dropped again
```

## 9. Example session

```text
# 1. Device powers on (HAS_BME280 + HAS_BATTERY), publishes first reading
thermo/thermo_1/sensors  <- {"temp":22.5,"humi":45,"press":1013.2,"bat":85,"batv":7.92,"rssi":-47}

# 2. Server auto-discovers device, requests capabilities (QoS 1)
thermo/thermo_1/command  -> {"action":"request_capabilities"}

# 3. Device responds with identity + metrics (commands via request_commands)
thermo/thermo_1/capabilities <- {
    "id":"ESP-00A1B2",
    "hw":"E8BMEBAT",
    "hwrev":1,
    "fw":"1.0.0",
    "ota":1,
    "cal":1,
    "intrvl":300,
    "metrics":{"rssi":"dBm","temp":"°C","humi":"%","press":"hPa","bat":"%","batv":"V"}
}

# 4. Admin approves from web UI — data now stored

# 5. Battery drops below 15%
thermo/thermo_1/status <- {"level":"warning","message":"low_battery"}

# 6. Admin changes publish interval via web UI
thermo/thermo_1/command -> {"action":"set_interval","value":120}

# 6b. Device acknowledges the command
thermo/thermo_1/ack <- {"action":"set_interval","status":"ok"}

# 7. Server auto-requests capabilities (after every command)
thermo/thermo_1/command -> {"action":"request_capabilities"}

# 8. Device responds with updated interval
thermo/thermo_1/capabilities <- {
    "id":"ESP-00A1B2",
    "hw":"E8BMEBAT",
    "hwrev":1,
    "fw":"1.0.0",
    "ota":1,
    "cal":1,
    "intrvl":120,
    "metrics":{"rssi":"dBm","temp":"°C","humi":"%","press":"hPa","bat":"%","batv":"V"}
}
```

## 10. Talking to a device directly (integrator quickstart)

Normally you drive devices through the server (web UI or the read-only HTTP
API). Talk to the broker directly only for debugging or a custom integration.
The examples use `mosquitto_pub`/`mosquitto_sub`; `$H` is the broker host and
`-u/-P` the credentials.

**Watch everything a device sends:**

```bash
mosquitto_sub -h $H -p 1883 -u USER -P PASS -v -t 'thermo/thermo_1/#'
# thermo/thermo_1/sensors      {"temp":22.5,"humi":45,"press":1013.2,"bat":85,"batv":7.92}
# thermo/thermo_1/capabilities {"id":"ESP-00A1B2","hw":"E8BMEBAT","hwrev":1,"fw":"1.0.0",...}
```

**Send a command — MUST be QoS 1** so a sleeping device receives it on its next
wake (a QoS 0 command is dropped while it is asleep):

```bash
# Change the publish interval to 600 s
mosquitto_pub -h $H -p 1883 -u USER -P PASS -q 1 \
  -t 'thermo/thermo_1/command' -m '{"action":"set_interval","value":600}'

# Watch for the ack (arrives on the device's next wake)
mosquitto_sub -h $H -p 1883 -u USER -P PASS -v -t 'thermo/thermo_1/ack'
# thermo/thermo_1/ack {"action":"set_interval","status":"ok"}
```

**Read a device's hardware code and firmware version** (for update detection):

```bash
mosquitto_sub -h $H -p 1883 -u USER -P PASS -t 'thermo/thermo_1/capabilities' &
mosquitto_pub -h $H -p 1883 -u USER -P PASS -q 1 \
  -t 'thermo/thermo_1/command' -m '{"action":"request_capabilities"}'
# -> capabilities response carries "hw" (hardware code) and "fw" (version).
```

The `hw`/`fw` pair is how you decide whether an update applies: `hw` identifies
the board (same hardware ⇒ same code), `fw` is the running version. Offer an
update when a newer image exists for that `hw`.

**Rules of thumb:**

- Always publish commands with **`-q 1`**. Without it, deep-sleep devices miss them.
- Expect the ack/response on the device's **next wake** (≤ `publish_interval`), not immediately.
- Do **not** set a retained command (`-r`) on the command topic: the firmware
  re-subscribes every wake and would re-execute a retained command each time.
- One command action per message (`{"action":...,"value":...}`); send several
  messages for several commands — they are queued and processed in order.
