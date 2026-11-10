#!/usr/bin/env python3
"""Exercise Settings menus without destructive update, format, or power-off actions."""

from __future__ import annotations

import time

from m1_hil import M1Hil, connect
from m1_resume import log_pass, resume_targets
from m1_bt_menu_audit import (
    _audit_path, _click, _frame, _open_settings_menu, _reboot_settings, _title_crc,
)
from m1_wifi_menu_audit import _wait_change, _wait_title


TOP_LEVEL_TARGETS = (
    ("Dashboard", (0,), False),
    ("LCD/Brightness", (1, 0), False),
    ("LCD/Buzzer", (1, 1), False),
    ("LCD/LED Notify", (1, 2), False),
    ("LCD/LED Color", (1, 3), False),
    ("LCD/Low Battery Color", (1, 4), False),
    ("LCD/Orientation", (1, 5), False),
    ("LCD/Sleep After", (1, 6), False),
    ("LCD/Text Size", (1, 7), False),
    ("LCD/Dark Mode", (1, 8), False),
    ("LCD/Local TZ", (1, 9), False),
    ("LCD/RGB Backlight", (1, 10), False),
    ("LCD/External IR", (1, 11), False),
    ("Storage/About SD Card", (2, 0), False),
    ("Storage/Explore SD Card", (2, 1), False),
    ("Storage/Mount SD Card", (2, 2), False),
    ("Storage/Unmount SD Card", (2, 3), False),
    ("Power/Battery Info", (3, 0), False),
    ("Power/Reboot", (3, 1), False),
    ("Firmware Update/Image File", (4, 0), False),
    ("Firmware Update/Download", (4, 3), False),
    ("ESP32 Update/Image File", (5, 0), False),
    ("ESP32 Update/Start Address", (5, 1), False),
    ("ESP32 Update/Download", (5, 3), False),
    ("ESP32 Update/Check Info", (5, 5), False),
    ("Bluetooth Direct", (6,), False),
)

SKIPPED_DESTRUCTIVE = (
    "Storage/Format SD Card",
    "Power/Power Off",
    "Firmware Update/Start",
    "Firmware Update/Swap Banks",
    "ESP32 Update/Firmware Update",
    "ESP32 Update/Backup Flash",
    "LCD/External IR TX self-test (RF transmission)",
)


def _open_settings(client: M1Hil) -> None:
    _click(client, "ok")
    _open_settings_menu(client)


def _audit_power_reboot(client: M1Hil) -> M1Hil:
    client = _reboot_settings(client)
    port = client.port.port
    baud = client.port.baudrate
    before = _frame(client)
    _click(client, "down", 3)
    _click(client, "ok")
    _click(client, "down")
    _click(client, "ok")
    _wait_change(client, before, 4.0)
    _click(client, "right")
    client.close()
    time.sleep(15.0)
    fresh = connect(port, baud, timeout=20)
    _frame(fresh)
    return fresh


def _audit_lcd_row(client: M1Hil, selection: int) -> None:
    expected_title = _title_crc(_frame(client))
    _click(client, "down")
    _click(client, "ok")
    previous = _frame(client)
    for _index in range(selection):
        _click(client, "down")
        current = _wait_change(client, previous, 4.0)
        if current == previous:
            raise AssertionError("LCD row navigation did not change")
        previous = current
    _click(client, "back")
    _wait_title(client, expected_title, 4.0)


def run(client: M1Hil) -> str:
    try:
        for label, path, edit in resume_targets(TOP_LEVEL_TARGETS):
            print(f"RUN  {label}", flush=True)
            if label == "Power/Reboot":
                client = _audit_power_reboot(client)
            else:
                client = _reboot_settings(client)
                if path[0] == 1 and path[1] not in (3, 4, 10):
                    _audit_lcd_row(client, path[1])
                else:
                    _audit_path(client, path, edit, 8.0,
                                reentry=(path[0] != 1 and path != (5, 1)))
            log_pass(label)

        client = _reboot_settings(client)
        _audit_path(client, (6,), False, 8.0)
        _frame(client)
    finally:
        client.close()
    summary = f"{len(TOP_LEVEL_TARGETS)} destinations responsive"
    if SKIPPED_DESTRUCTIVE:
        summary += f"; skipped: {', '.join(SKIPPED_DESTRUCTIVE)}"
    return summary
