# MQTT Protocol — Device Side

This document describes the MQTT protocol that thermo devices must implement
to communicate with the sensor_server. It is the client-side counterpart to
the server's `docs/mqtt-protocol.md` (in `../sensor_server/`).

## Topic structure

All topics follow:

```
{device_type}/{device_id}/{message_type}
```

Both `device_type` and `device_id` are defined via build flags in
`platformio.ini` (`-DMQTT_DEVICE_TYPE` and `-DDEVICE_ID`).

| Message type   | Direction        | Topic example                  |
|----------------|------------------|--------------------------------|
| `sensors`      | Device -> Server | `thermo/thermo_1/sensors`      |
| `status`       | Device -> Server | `thermo/thermo_1/status`       |
| `command`      | Server -> Device | `thermo/thermo_1/command`      |
| `capabilities` | Device -> Server | `thermo/thermo_1/capabilities` |
| `ack`          | Device -> Server | `thermo/thermo_1/ack`          |

## 1. Publishing sensor readings (`sensors`)

Publish a JSON object mapping metric names to numeric values. The payload
is built dynamically by `PayloadBuilder` — each enabled module contributes
its fields (see [architecture.md](architecture.md)).

**Example with `HAS_BME280` only:**

```json
{
  "temperature": 22.5,
  "humidity": 45.2,
  "pressure": 1013.1
}
```

**Example with `HAS_BME280` + `HAS_BATTERY`:**

```json
{
  "temperature": 22.0,
  "humidity": 50.0,
  "pressure": 1000.0,
  "battery_pct": 75,
  "battery_v": 7.80
}
```

**Constraints:**

- Payload must be valid JSON, max 10 KB
- Metric names: alphanumeric, `-`, `_`, max 64 chars
- Values: must be valid numbers (float)
- Publish with `retain = false`

### Metric reference

| Metric        | Module        | Type               | Unit    | Description                                          |
|---------------|---------------|--------------------|---------|------------------------------------------------------|
| `temperature` | `HAS_BME280`  | float (1 decimal)  | Celsius | Ambient temperature                                  |
| `humidity`    | `HAS_BME280`  | float (1 decimal)  | % RH    | Relative humidity                                    |
| `pressure`    | `HAS_BME280`  | float (1 decimal)  | hPa     | Atmospheric pressure                                 |
| `battery_pct` | `HAS_BATTERY` | uint               | %       | Battery state of charge (0-100)                      |
| `battery_v`   | `HAS_BATTERY` | float (2 decimals) | V       | Battery voltage (ESP: 2S 6.0-8.4V, MKR: 1S 3.0-4.2V) |

New modules can add additional metrics. The server auto-discovers metric
names from the sensor readings and capabilities message.

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

| Condition                    | Level     | Message            | Module        |
|------------------------------|-----------|--------------------|---------------|
| Battery SoC <= warn threshold | `warning` | `low_battery`      | `HAS_BATTERY` |
| Battery SoC <= crit threshold | `error`   | `critical_battery` | `HAS_BATTERY` |

Thresholds are platform-specific (defined in `config.h`):

| Platform          | Warning | Critical |
|-------------------|---------|----------|
| ESP8266 (2S LiPo) | 15%     | 5%       |
| MKR WiFi (1S LiPo)| 20%     | 5%       |

**Important:** Do NOT publish online/offline status. The server computes
online/offline from the timestamp of the last received message:

```
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

| Action                 | Value             | Handler | Behavior                               |
|------------------------|-------------------|---------|----------------------------------------|
| `set_interval`         | seconds (1-86400) | Core    | Updates publish/sleep interval         |
| `request_capabilities` | _(none)_          | Core    | Responds with capabilities (section 4) |

Modules can register additional commands via `reg.add_command()`. These
commands automatically appear in the capabilities message and are routed
by the dispatcher.

### Implementation notes

- **Continuous mode** (no `HAS_DEEP_SLEEP`): subscribe at startup, process
  commands in `loop()` via `network.loop()`. Always connected, no special
  handling needed.
- **One-shot mode** (`HAS_DEEP_SLEEP`) and **duty-cycle mode** (MKR WiFi):
  1. Subscribe to command topic
  2. Publish sensor data (triggers server-side command flush)
  3. Wait 5 seconds for pending commands (`MQTT_COMMAND_WAIT_MS`)
  4. Process each command as it arrives (ack immediately)
  5. Publish capabilities (reflects post-command state)
  6. Disconnect and sleep / power down radio
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

**Example with `HAS_BME280` only:**

```json
{
  "id": "ESP-00A1B2",
  "intrvl": 10,
  "metrics": {"temperature": "°C", "humidity": "%", "pressure": "hPa"},
  "cmds": {"set_interval": [{"n": "value", "t": "number"}]}
}
```

**Example with `HAS_BME280` + `HAS_BATTERY`:**

```json
{
  "id": "ESP-00A1B2",
  "intrvl": 300,
  "metrics": {"temperature": "°C", "humidity": "%", "pressure": "hPa", "battery_pct": "%", "battery_v": "V"},
  "cmds": {"set_interval": [{"n": "value", "t": "number"}]}
}
```

| Field     | Type   | Required | Description                                                          |
|-----------|--------|:--------:|----------------------------------------------------------------------|
| `id`      | string |   Yes    | Unique chip ID. Format: `ESP-XXXXXX` or `MKR-XXXXXXXX`. Max 256     |
| `intrvl`  | number |   Yes    | Current publish frequency in seconds (1-86400)                       |
| `metrics` | object |   Yes    | Metric name → unit string (`""` if no unit). Max 16 chars per unit   |
| `cmds`    | object |   Yes    | Command name → array of `{n, t}` params (`[]` if no params)          |

**Parameter definition format:** each entry is `{"n": "<param>", "t": "<number|string|boolean>"}`.

- Do NOT include `request_capabilities` in `cmds` (implicit).
- `intrvl` reflects the current value (may have been changed by a
  `set_interval` command or read from RTC memory).
- Publish with `retain = false`.

## 6. Timeout handling

If the device does not respond to `request_capabilities` within 60 seconds,
the server flags it with `alert_level = "error"` and
`alert_message = "no_capabilities_response"`. This is automatically cleared
when the device sends a valid capabilities response.

The server requests capabilities in three situations:

1. **First discovery** — first message from an unknown device
2. **Reconnection** — device was offline and starts publishing again
3. **After every command** — to refresh capabilities that may have changed

## 7. Authentication

MQTT broker requires username/password authentication (from `credentials.h`).
Anonymous access is disabled.

## 8. Device lifecycle (from server perspective)

```
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

```
# 1. Device powers on (HAS_BME280 + HAS_BATTERY), publishes first reading
thermo/thermo_1/sensors  <- {"temperature":22.5,"humidity":45,"pressure":1013.2,"battery_pct":85,"battery_v":7.92}

# 2. Server auto-discovers device, requests capabilities
thermo/thermo_1/command  -> {"action":"request_capabilities"}

# 3. Device responds (metrics/commands from ModuleRegistry)
thermo/thermo_1/capabilities <- {
    "id":"ESP-00A1B2",
    "intrvl":300,
    "metrics":{"temperature":"°C","humidity":"%","pressure":"hPa","battery_pct":"%","battery_v":"V"},
    "cmds":{"set_interval":[{"name":"value","type":"number"}]}
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
    "intrvl":120,
    "metrics":{"temperature":"°C","humidity":"%","pressure":"hPa","battery_pct":"%","battery_v":"V"},
    "cmds":{"set_interval":[{"name":"value","type":"number"}]}
}
```
