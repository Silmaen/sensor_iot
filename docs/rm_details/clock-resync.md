# Clock re-sync — grid-aligned wake-ups

## Problem

Measurement timestamps drift later over time and the offset accumulates
(e.g. `00:00:00` → `00:05:02` → `00:10:05` …). Two causes:

1. **Fixed sleep added after a variable wake.** The device sleeps a *fixed*
   `interval` (e.g. 300 s) *after* the wake work, so the real period is
   `wake_time + interval`. Wake time is ~2.7 s nominally but **11–14 s when the MQTT
   connect retry fires** → a linear, irregular drift of a few seconds per cycle.
2. **Deep-sleep RC-oscillator drift.** The ESP32-C3 Super Mini clones have no
   32 kHz crystal, so the RTC deep-sleep timer runs on the internal RC oscillator
   (~1 % after calibration → ~3 s per 300 s cycle). This accumulates on its own,
   independent of the wake time.

## Goal

Keep measurements aligned to a time grid within **±10 s** over a full night, with
**negligible battery cost**. Precision beyond that is not needed.

## Design — two configurable schedule modes

The wake schedule is configurable as **either** an interval **or** a clock boundary:

- **Interval mode (relative).** Wake every `interval` seconds. To remove the
  wake-time drift, sleep `interval − wake_elapsed`, where `wake_elapsed` is `millis()`
  at sleep time. **No time source needed.** Keeps consistent *spacing*; the phase is
  not anchored to the wall clock and the RTC RC-oscillator drift is not corrected
  (fine when the goal is just "evenly spaced").

- **Clock-boundary mode (absolute grid).** Wake aligned to wall-clock boundaries,
  cron-like: sleep until the next instant where `epoch % boundary == 0` (for
  `boundary = 300 s`, the `:00`/`:05`/`:10` … minutes). Needs network time (SNTP from
  the router). Re-anchors every sync, so it corrects **both** drift sources (wake time
  *and* RC oscillator) and holds the ±10 s target indefinitely. Alignment is on the
  **UTC epoch**; local is UTC+2 (whole hours) → identical minute grid.

The two modes are not rivals: clock-boundary is the accurate target, and its
degraded form (see below) *is* interval mode. Interval mode stands alone when there
is no NTP at all.

## Fallback when the time sync fails

In clock-boundary mode, if the SNTP query fails on a wake (NTP unreachable / no answer
in time), **never block the publish** — fall back for that cycle to interval-mode
behaviour: a wake-compensated `interval` sleep using `millis()` + the internal RTC.
The grid drift is **not** modelled or extrapolated; it is simply **re-anchored on the
next wake whose time sync succeeds** ("compensate the drift later"). So a stretch
without NTP degrades gracefully to even spacing and snaps back onto the grid as soon
as time is available again.

## Collision risk when grid-aligned (thundering herd)

Grid-aligning **synchronises all sensors**: they wake on the same `:00`/`:05` slots and
rush the connect phase (WiFi assoc + DHCP + TCP + MQTT CONNECT) at the same instant.
That phase is exactly the C3's fragile spot (the `mf` connect failures), so
synchronisation can *worsen* it. Note the irony: the sleep drift we are removing was
**de-synchronising the devices for free**; grid mode takes that spreading away.

Scale check: ~5 devices, tiny payloads, once per 5 min — WiFi CSMA/CA and the broker
absorb a small herd fine, so the risk is **modest, not catastrophic**. But the C3's
connect is already marginal, so any extra contention that tips a borderline wake into
`mf` is unwelcome. Cheap to defend against.

**Mitigation — deterministic per-device jitter.** Offset each device's slot by a fixed
amount derived from its `device_id` (e.g. `hash(device_id) % window`):

- De-collides without coordination or server involvement; **reproducible** (a device
  always lands at ~the same offset) and survives reboots.
- Composes with the schedule: target = `next_boundary + device_offset`.
- Fits the ±10 s tolerance: with ~5 devices, ~3–4 s spacing over a ~±8 s window
  separates them well while staying "on the grid".

Tension to keep in mind: the **jitter window vs the ±10 s tolerance**. Few devices → a
small window suffices. If the fleet grows, either widen the window (and relax the
tolerance) or distribute the offsets more finely. Alternative: random per-wake jitter
— simpler but non-reproducible and it blurs per-device regularity; deterministic is
preferred.

## Open questions (decide at implementation time)

- **Config surface:** how to select the mode + value over MQTT — extend
  `set_interval`, or add a schedule/boundary command. What is authoritative if both an
  interval and a boundary are configured (boundary wins? last-set wins?).
- **Sync cadence (clock-boundary mode):** SNTP every wake (simplest, already cheap
  since WiFi is up) vs periodic (keep time in the RTC between syncs, tolerate small
  drift). Leaning: every wake.
- **Cold start:** before the first successful sync, run interval mode; snap to the
  grid on the first sync (covered by the fallback rule above).
- **Non-divisor boundary:** if `boundary` does not divide the hour, aligning to epoch
  multiples still works; the grid just isn't on round minutes.
- **Reachability:** confirm the router's NTP is reachable from the IoT VLAN
  (`10.9.0.x`).

## Notes

- SNTP client is built into the ESP32 Arduino core; `gettimeofday`/RTC time persists
  across deep sleep on the ESP32.
- Battery: the extra work is one local UDP round-trip on an already-powered radio →
  negligible.
- Alternative time source considered: server-provided time over the existing MQTT
  link (no NTP dependency), rejected for now since the router already runs NTP.
