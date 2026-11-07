#!/usr/bin/env python3
"""Exercise every static Bluetooth menu destination without retaining ambient screen data."""

from __future__ import annotations

import time

from m1_hil import BUTTONS, M1Hil, connect
from m1_wifi_menu_audit import _click, _frame, _title_crc, _wait_change


TOP_LEVEL_TARGETS = (
    ("BLE Scan", (0,), False),
    ("Signal Finder", (1,), False),
    ("BLE Advertise", (2,), False),
    ("BLE Config", (3,), False),
    ("BLE Config/Edit", (3,), True),
    ("Sniffers/Analyzer", (4, 0), False),
    ("Sniffers/Generic", (4, 1), False),
    ("Sniffers/Flipper", (4, 2), False),
    ("Sniffers/AirTag Sniff", (4, 3), False),
    ("Sniffers/AirTag Monitor", (4, 4), False),
    ("Sniffers/Flock", (4, 5), False),
    ("BLE Spam/Unified All", (5, 0, 0), False),
    ("BLE Spam/Unified Apple", (5, 0, 1), False),
    ("BLE Spam/Unified Google", (5, 0, 2), False),
    ("BLE Spam/Unified Microsoft", (5, 0, 3), False),
    ("BLE Spam/Sour Apple", (5, 1), False),
    ("BLE Spam/SwiftPair", (5, 2), False),
    ("BLE Spam/Samsung", (5, 3), False),
    ("BLE Spam/Google FP", (5, 4), False),
    ("BLE Spam/Flipper", (5, 5), False),
    ("BLE Spam/All", (5, 6), False),
    ("BLE Spam/AirTag Spoof", (5, 7), False),
    ("Wardrive", (6,), False),
    ("Wardrive Continuous", (7,), False),
    ("Wardrive Flock", (8,), False),
    ("Detectors/Skimmers", (9, 0), False),
    ("Detectors/Flock", (9, 1), False),
    ("Detectors/Meta", (9, 2), False),
    ("GATT Discover", (10,), False),
    ("Bad-BT", (11,), False),
    ("BT Name", (12,), False),
    ("Saved Devices", (13,), False),
    ("BT Info", (14,), False),
)


def _open_bluetooth(client: M1Hil) -> int:
    _click(client, "ok")
    _click(client, "down", 6)
    _click(client, "ok")
    return _title_crc(_frame(client))


def _reboot_menu(client: M1Hil, selection: int) -> M1Hil:
    port = client.port.port
    baud = client.port.baudrate
    try:
        client.request(0x05, timeout=5)
    except TimeoutError:
        pass
    client.close()
    time.sleep(15.0)
    fresh = connect(port, baud, timeout=20)
    _click(fresh, "ok")
    _click(fresh, "down", selection)
    _click(fresh, "ok")
    _frame(fresh)
    return fresh


def _reboot_bluetooth(client: M1Hil) -> M1Hil:
    return _reboot_menu(client, 6)


def _audit_path(client: M1Hil, path: tuple[int, ...], edit: bool,
                wait_s: float = 5.0) -> None:
    before = _frame(client)
    for selection in path:
        _click(client, "down", selection)
        _click(client, "ok")
    if edit:
        _click(client, "ok")
    _wait_change(client, before, wait_s)
    _click(client, "back")
    _frame(client)


def run(client: M1Hil) -> str:
    try:
        for label, path, edit in TOP_LEVEL_TARGETS:
            print(f"RUN  {label}", flush=True)
            client = _reboot_bluetooth(client)
            _audit_path(client, path, edit, 8.0)
            print(f"PASS {label}", flush=True)
        _frame(client)
    finally:
        client.close()
    return (f"{len(TOP_LEVEL_TARGETS)} destinations responsive; "
            "skipped: Bad-BT connected file browser and BLE peer/GATT "
            "characteristic actions (require peers)")
