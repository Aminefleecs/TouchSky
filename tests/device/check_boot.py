#!/usr/bin/env python3
"""Flashes the firmware, resets the board and waits for its boot markers.

The firmware logs one "TS_OK <subsystem>" line per subsystem that came up.
Usage: check_boot.py <port> [subsystem ...]   (default: display)
Needs the ESP-IDF environment (idf.py and pyserial); run through tests/run.sh.
"""
import subprocess
import sys
import time
from pathlib import Path

import serial

FIRMWARE = Path(__file__).resolve().parents[2] / "firmware"
TIMEOUT_S = 60  # Wi-Fi association and the first API call are the slow ones


def main():
    port, wanted = sys.argv[1], set(sys.argv[2:] or ["display"])
    subprocess.run(["idf.py", "-p", port, "flash"], cwd=FIRMWARE, check=True)

    with serial.Serial(port, 115200, timeout=1) as link:
        # Pulse RTS (wired to EN through the USB-serial bridge) to reboot.
        link.dtr = False
        link.rts = True
        time.sleep(0.1)
        link.rts = False

        deadline = time.monotonic() + TIMEOUT_S
        while wanted and time.monotonic() < deadline:
            line = link.readline().decode(errors="replace").rstrip()
            if not line:
                continue
            print(line)
            if "TS_OK " in line:
                wanted.discard(line.split("TS_OK ", 1)[1].split()[0])

    if wanted:
        print(f"FAIL: no boot marker for: {', '.join(sorted(wanted))}", file=sys.stderr)
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
