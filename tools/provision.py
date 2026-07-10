#!/usr/bin/env python3
"""Provision a device_id over serial, without an interactive monitor.

Sends the firmware's serial provisioning command (`provision <id>` or
`factory_reset`, see src/main.cpp) to a freshly-flashed device sitting in
provisioning mode, and prints its reply.

Opening the port toggles DTR/RTS, which resets the ESP — so the device reboots
into provisioning right as we connect, then receives the command.

Usage:
    python3 tools/provision.py thermo_1
    python3 tools/provision.py thermo_1 --port /dev/ttyUSB0
    python3 tools/provision.py --factory-reset --port /dev/ttyACM0

Needs pyserial (bundled with PlatformIO). If `python3` lacks it, run with
PlatformIO's interpreter, e.g.:
    ~/.platformio/penv/bin/python tools/provision.py thermo_1
"""

import argparse
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial not found. Run with PlatformIO's python:\n"
             "  ~/.platformio/penv/bin/python tools/provision.py ...")


def _autodetect_port():
    candidates = [p.device for p in list_ports.comports()
                  if "USB" in p.device or "ACM" in p.device or "usbserial" in p.device]
    if len(candidates) == 1:
        return candidates[0]
    if not candidates:
        sys.exit("No serial port found; pass --port explicitly.")
    sys.exit("Multiple serial ports found (%s); pass --port explicitly."
             % ", ".join(candidates))


def main():
    ap = argparse.ArgumentParser(description="Provision a device_id over serial.")
    ap.add_argument("device_id", nargs="?", help="id to provision (e.g. thermo_1)")
    ap.add_argument("--factory-reset", action="store_true", help="wipe the store instead")
    ap.add_argument("--port", help="serial port (auto-detected if omitted)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--boot-wait", type=float, default=2.5,
                    help="seconds to wait for the device to enter provisioning")
    args = ap.parse_args()

    if args.factory_reset:
        command = "factory_reset"
    elif args.device_id:
        command = "provision %s" % args.device_id
    else:
        ap.error("give a device_id, or use --factory-reset")

    port = args.port or _autodetect_port()
    print("==> %s on %s @ %d" % (command, port, args.baud))

    with serial.Serial(port, args.baud, timeout=1) as ser:
        # Opening the port reset the ESP. Wait until it has booted into
        # provisioning and printed its "[PROV]" banner — this guarantees the
        # UART has settled (past the boot-ROM baud switch / LittleFS format) and
        # run_provisioning() is actively reading, so the command isn't corrupted
        # by the boot window.
        banner = b""
        deadline = time.time() + max(args.boot_wait, 12.0)
        saw_banner = False
        while time.time() < deadline:
            banner += ser.read(256)
            if b"[PROV]" in banner:
                saw_banner = True
                break
        if not saw_banner:
            # SAMD/MKR (native USB) does not reset on port open, so its boot-time
            # [PROV] banner was printed before we connected. Send anyway and rely
            # on the reply below. (On ESP8266 the open resets it, so the banner
            # does appear.)
            print("(no [PROV] banner seen — sending anyway; normal for boards "
                  "that don't reset on port open, e.g. SAMD/MKR)", file=sys.stderr)

        # Terminate any partial line in the device buffer, then send the command.
        ser.reset_input_buffer()
        ser.write(b"\r\n")
        time.sleep(0.2)
        ser.reset_input_buffer()
        ser.write((command + "\r\n").encode())
        ser.flush()

        # Read the reply for a couple of seconds. On SAMD/MKR the command
        # triggers a reset that re-enumerates the USB CDC, so the port drops
        # mid-read — that disconnect is itself the success signal (device
        # rebooted after storing).
        reply_deadline = time.time() + 3.0
        got = b""
        rebooted = False
        while time.time() < reply_deadline:
            try:
                chunk = ser.read(256)
            except serial.SerialException:
                rebooted = True
                break
            if chunk:
                got += chunk
        text = got.decode(errors="replace").strip()
        if text:
            print(text)

    if "provisioned" in text or "rebooting" in text or "cleared" in text:
        print("==> OK")
    elif rebooted:
        print("==> device rebooted (USB re-enumerated) — provisioning almost "
              "certainly applied; verify it comes online on the server.")
    else:
        print("==> no confirmation seen; re-check the port / that the store was empty",
              file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
