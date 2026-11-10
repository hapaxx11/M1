#!/usr/bin/env python3
"""Exercise every static Bluetooth menu destination without retaining ambient screen data."""

from __future__ import annotations

import time

from m1_hil import BUTTONS, M1Hil, connect
from m1_wifi_menu_audit import _click, _frame, _title_crc, _wait_change, _wait_title


TOP_LEVEL_TARGETS = (
    ("BLE Scan", (0,), False),
    ("Signal Finder", (1,), False),
    ("BLE Config", (3,), False),
    ("BLE Config/Edit", (3,), True),
    ("Sniffers/Analyzer", (4, 0), False),
    ("Sniffers/Generic", (4, 1), False),
    ("Sniffers/Flipper", (4, 2), False),
    ("Sniffers/AirTag Sniff", (4, 3), False),
    ("Sniffers/AirTag Monitor", (4, 4), False),
    ("Sniffers/Flock", (4, 5), False),
    ("BLE Spam/Unified picker", (5, 0), False),
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

SKIPPED_ACTIVE = (
    "BLE Advertise and active spam modes (explicitly authorized isolated test only)",
    "Bad-BT connected file browser and BLE peer/GATT actions (require peers)",
)


def _open_bluetooth(client: M1Hil) -> int:
    _click(client, "ok")
    _click(client, "down", 6)
    _click(client, "ok")
    return _title_crc(_frame(client))


def _reboot_main(client: M1Hil) -> M1Hil:
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
    _frame(fresh)
    return fresh


def _reboot_menu(client: M1Hil, selection: int) -> M1Hil:
    fresh = _reboot_main(client)
    _click(fresh, "down", selection)
    _click(fresh, "ok")
    _frame(fresh)
    return fresh


def _open_settings_menu(client: M1Hil) -> None:
    root = _frame(client)
    root_title = _title_crc(root)
    _click(client, "down", 10)
    current = _wait_change(client, root, 4.0)
    if _title_crc(current) == root_title:
        _click(client, "down", 9)
    _click(client, "ok")
    if _wait_change(client, current, 4.0) == current:
        raise AssertionError("Settings destination did not open")


def _reboot_settings(client: M1Hil) -> M1Hil:
    fresh = _reboot_main(client)
    _open_settings_menu(fresh)
    return fresh


def _open_apps_menu(client: M1Hil) -> bool:
    root = _frame(client)
    root_title = _title_crc(root)
    _click(client, "down", 10)
    current = _wait_change(client, root, 4.0)
    settings_title = _title_crc(current)
    if settings_title == root_title:
        return False
    _click(client, "up")
    _click(client, "ok")
    if _title_crc(_frame(client)) == settings_title:
        raise AssertionError("Apps destination identity mismatch")
    return True


def _reboot_bluetooth(client: M1Hil) -> M1Hil:
    return _reboot_menu(client, 6)


def _audit_path(client: M1Hil, path: tuple[int, ...], edit: bool,
                wait_s: float = 5.0, reentry: bool = True,
                ancestor_titles: tuple[int, ...] = ()) -> None:
    parent = _frame(client)
    parent_title = _title_crc(parent)
    accepted_titles = (parent_title,) + ancestor_titles

    def exit_to_parent(previous: bytes) -> bytes:
        for _level in range(len(path) + 4):
            if _title_crc(previous) in accepted_titles:
                return previous
            _click(client, "back")
            previous = _wait_change(client, previous, wait_s)
        raise AssertionError("BACK did not return to the parent destination")

    previous = parent
    for selection in path:
        _click(client, "down", selection)
        _click(client, "ok")
        current = _wait_change(client, previous, wait_s)
        if current == previous:
            raise AssertionError("destination did not change after OK")
        previous = current
    if edit:
        _click(client, "ok")
        current = _wait_change(client, previous, wait_s)
        if current == previous:
            raise AssertionError("editor did not change after OK")
        previous = current
    previous = exit_to_parent(previous)
    if reentry:
        for _selection in path:
            _click(client, "ok")
            current = _wait_change(client, previous, wait_s)
            if current == previous:
                raise AssertionError("re-entry did not change after OK")
            previous = current
    if edit:
        _click(client, "ok")
        current = _wait_change(client, previous, wait_s)
        if current == previous:
            raise AssertionError("editor re-entry did not change after OK")
        previous = current
    exit_to_parent(previous)


def run(client: M1Hil) -> str:
    try:
        for label, path, edit in TOP_LEVEL_TARGETS:
            print(f"RUN  {label}", flush=True)
            client = _reboot_bluetooth(client)
            _audit_path(client, path, edit, 8.0, reentry=(label != "BT Name"))
            print(f"PASS {label}", flush=True)
        _frame(client)
    finally:
        client.close()
    return (f"{len(TOP_LEVEL_TARGETS)} destinations responsive; "
            f"skipped: {', '.join(SKIPPED_ACTIVE)}")
