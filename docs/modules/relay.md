# Dual Relay Module

## Overview

The relay module drives a **Dual Relay Board** (2 channels, 10A per relay) via two GPIO outputs.
Each relay can be toggled on/off or activated for a timed duration (contact mode). It is enabled by
the `-DHAS_RELAY` build flag in `platformio.ini`.

**Metrics published** (on `thermo/{id}/sensors`):

| Metric   | Unit | Description                            |
|----------|------|----------------------------------------|
| `relay1` | —    | State of relay 1 (0 = off, 1 = active) |
| `relay2` | —    | State of relay 2 (0 = off, 1 = active) |

**Commands** (on `thermo/{id}/command`):

| Action          | Payload                                                  | Effect                                                  |
|-----------------|----------------------------------------------------------|---------------------------------------------------------|
| `relay_toggle`  | `{"action":"relay_toggle","value":<1\|2>}`               | Toggle relay on/off. Cancels any pending contact timer. |
| `relay_contact` | `{"action":"relay_contact","relay":<1\|2>,"value":<ms>}` | Activate relay, auto-revert after delay (1–300000 ms).  |

## Board Specifications

| Parameter     | Value                                       |
|---------------|---------------------------------------------|
| Board         | Dual Relay Board, 10A per channel           |
| Relay type    | Mechanical, SPDT (NO + NC + COM per relay)  |
| Coil voltage  | 5V (see notes on 3.3V compatibility below)  |
| Control pins  | 2 (one per relay), active HIGH or LOW (TBC) |
| Max switching | 10A @ 250VAC / 10A @ 30VDC                  |

### Relay Contacts

Each relay has three terminals:

| Terminal | Name            | Description                            |
|----------|-----------------|----------------------------------------|
| COM      | Common          | Connected to either NO or NC           |
| NC       | Normally Closed | Connected to COM when relay is **off** |
| NO       | Normally Open   | Connected to COM when relay is **on**  |

### 3.3V Compatibility

The relay board is designed for 5V coil voltage. When driving from a 3.3V MCU (ESP8266, ESP32):

- The board likely has an onboard transistor driver + optocoupler — 3.3V GPIO HIGH should
  be sufficient to trigger the optocoupler input. **Test this before deploying.**
- If 3.3V GPIO is insufficient, use a level shifter or drive through an NPN transistor
  (e.g. 2N2222) with a pull-up to 5V.
- Always power the relay board VCC from the **5V rail**, not 3.3V.

## Wiring

### ESP8266 (D1 Mini)

| D1 Mini Pin | GPIO   | Relay Board | Notes                    |
|-------------|--------|-------------|--------------------------|
| D0          | GPIO16 | IN1         | Relay 1 control          |
| D3          | GPIO0  | IN2         | Relay 2 control (or D4)  |
| 5V          | —      | VCC         | 5V power for relay coils |
| GND         | —      | GND         | Common ground            |

> **Note:** GPIO assignment is a suggestion. Any free GPIO with push-pull output capability works.
> Avoid GPIO used by I2C (D1/D2) or SPI (D5/D6/D7/D8) if those buses are also in use.

### ESP32 CAM

| ESP32 Pin | GPIO   | Relay Board | Notes           |
|-----------|--------|-------------|-----------------|
| IO12      | GPIO12 | IN1         | Relay 1 control |
| IO13      | GPIO13 | IN2         | Relay 2 control |
| 5V        | —      | VCC         | 5V power        |
| GND       | —      | GND         | Common ground   |

> **Note:** Pin assignment depends on the ESP32 CAM variant and available GPIOs.

## Commands

### `relay_toggle`

Toggles the specified relay between on and off states. If a contact timer is pending on that relay,
it is cancelled.

```json
{"action":"relay_toggle","value":1}
```

| Field   | Type   | Required | Values | Description         |
|---------|--------|:--------:|--------|---------------------|
| `value` | number |   Yes    | 1 or 2 | Relay number        |

### `relay_contact`

Activates the specified relay, then automatically reverts it to the off state after the given delay.
If the relay is already active from a previous contact, the timer is reset.

```json
{"action":"relay_contact","relay":1,"value":2000}
```

| Field   | Type   | Required | Values       | Description              |
|---------|--------|:--------:|--------------|--------------------------|
| `relay` | number |   Yes    | 1 or 2       | Relay number             |
| `value` | number |   Yes    | 1–300000     | Delay before revert (ms) |

Use cases:
- **Door buzzer**: `relay_contact` with 3000 ms to unlock for 3 seconds
- **Garage door**: `relay_contact` with 500 ms to simulate a button press
- **Pump activation**: `relay_contact` with 60000 ms for a 1-minute run

## Timer Mechanism

The contact timer uses a two-phase approach to remain portable (no `millis()` dependency
inside the command handler):

1. **Command phase**: the handler stores the delay value and activates the relay.
2. **Tick phase**: `relay_module_tick(now_ms)` is called from `loop()` on each iteration.
   On the first tick after a command, the delay is resolved to an absolute deadline. On
   subsequent ticks, the deadline is checked and the relay is reverted when expired.

A `relay_toggle` command cancels any pending contact timer on the same relay.

## PlatformIO Configuration

### Build Flag

```ini
build_flags = -DHAS_RELAY
```

### Example Environment

```ini
[env:relay_esp8266]
extends = common_esp8266
build_flags =
    ${common_esp8266.build_flags}
    -DDEVICE_ID='"relay_1"'
    -DMQTT_DEVICE_TYPE='"thermo"'
    -DHAS_RELAY
    -DHAS_SERIAL_DEBUG
```

## Firmware Files

| File                                            | Role                                     |
|-------------------------------------------------|------------------------------------------|
| `lib/thermo_core/src/modules/relay_module.h`    | Module interface (register, contribute)  |
| `lib/thermo_core/src/modules/relay_module.cpp`  | Module logic (commands, timer, state)    |
| `src/hw/relay_driver.h` _(to create)_           | Hardware driver header (GPIO init/write) |
| `src/hw/relay_driver.cpp` _(to create)_         | Hardware driver implementation           |

### Module API

```c++
void relay_module_register(ModuleRegistry& reg);
void relay_module_contribute(PayloadBuilder& pb);
bool relay_module_tick(unsigned long now_ms);
void relay_module_reset();

using RelayWriter = void (*)(uint8_t relay_num, bool active);
void relay_module_set_writer(RelayWriter writer);
```

## MQTT Payload

When active, the device publishes relay state alongside other metrics on `thermo/{device_id}/sensors`:

```json
{"relay1":0,"relay2":1,"temperature":22.5,"humidity":48.3}
```

## Safety Considerations

- **Power loss**: relays revert to their de-energized state (NC closed, NO open) on power loss.
  After reboot, firmware initializes both relays to off (0).
- **Contact timer**: if the device reboots during a timed contact, the relay de-energizes
  immediately (fail-safe). No persistent timer state is stored across reboots.
- **Concurrent commands**: a `relay_toggle` cancels any pending `relay_contact` timer on
  the same relay. A new `relay_contact` replaces the previous timer.

## See Also

- [MQTT protocol](../mqtt-protocol.md) — command format and acknowledgement flow
- [Components inventory](../components.md) — Dual Relay Board hardware specs
- [Architecture](../architecture.md) — module system, command dispatch
