#!/usr/bin/env python3
"""Exercise static 125 kHz RFID menu destinations without tag or file changes."""

from __future__ import annotations

import time

import m1_ux_audit
from m1_hil import M1Hil
from m1_resume import log_pass, resume_targets
from m1_bt_menu_audit import (
    _audit_path, _click, _frame, _reboot_menu, _title_crc, _wait_change,
)
from m1_saved_item_audit import select_first_file


TOP_LEVEL_TARGETS = (
    ("Read", (0,), False),
    ("Pet Scanner", (1,), False),
    ("Saved", (2,), False),
    ("Add Manually/EM4100", (3, 0), False),
    ("Add Manually/EM4100_32", (3, 1), False),
    ("Add Manually/EM4100_16", (3, 2), False),
    ("Add Manually/H10301", (3, 3), False),
    ("Add Manually/HID Generic", (3, 4), False),
    ("Add Manually/Indala26", (3, 5), False),
    ("Add Manually/AWID", (3, 6), False),
    ("Add Manually/Pyramid", (3, 7), False),
    ("Add Manually/Paradox", (3, 8), False),
    ("Add Manually/IoProxXSF", (3, 9), False),
    ("Add Manually/FDX-A", (3, 10), False),
    ("Add Manually/HID Ex Generic", (3, 11), False),
    ("Add Manually/Viking", (3, 12), False),
    ("Add Manually/FDX-B", (3, 13), False),
    ("Add Manually/Electra", (3, 14), False),
    ("Add Manually/Gallagher", (3, 15), False),
    ("Add Manually/Jablotron", (3, 16), False),
    ("Add Manually/PAC Stanley", (3, 17), False),
    ("Add Manually/Securakey", (3, 18), False),
    ("Add Manually/GProx II", (3, 19), False),
    ("Add Manually/Noralsy", (3, 20), False),
    ("Add Manually/Idteck", (3, 21), False),
    ("Add Manually/Keri", (3, 22), False),
    ("Add Manually/Nexwatch", (3, 23), False),
    ("Add Manually/Indala224", (3, 24), False),
    ("Add Manually/InstaFob", (3, 25), False),
    ("Import .rfid", (4,), False),
    ("Utilities/Clone Card", (5, 0), False),
    ("Utilities/Erase Tag", (5, 1), False),
    ("Utilities/T5577 Info", (5, 2), False),
    ("Utilities/RFID Fuzzer", (5, 3), False),
    ("Utilities/Brute Force FC", (5, 4), False),
    ("Utilities/Clear T5577 Password", (5, 5), False),
    ("Utilities/RFID Diagnostics", (5, 6), False),
)

SAVED_ACTIONS = (
    ("Saved/Emulate", 0),
    ("Saved/Send to Peer", 1),
    ("Saved/Edit", 3),
    ("Saved/Rename", 4),
    ("Saved/Delete (cancel)", 5),
    ("Saved/Info", 6),
)


def _run_saved_actions(client: M1Hil) -> tuple[M1Hil, list[str]]:
    skipped = ["Read Save/Emulate/Write (requires live card)", "Saved/Write (destructive)"]
    client = _reboot_menu(client, 1)
    _click(client, "down", 2)
    _click(client, "ok")
    if not select_first_file(client):
        skipped.append("Saved item actions (no saved file)")
        return client, skipped

    for label, selection in resume_targets(SAVED_ACTIONS):
        print(f"RUN  {label}", flush=True)
        if selection != 0:
            client = _reboot_menu(client, 1)
            _click(client, "down", 2)
            _click(client, "ok")
            if not select_first_file(client):
                raise AssertionError("saved file disappeared during the audit")
        _click(client, "ok")
        if selection == 5:
            _click(client, "down", selection)
            _click(client, "ok")
            m1_ux_audit.observe(_frame(client))
            _click(client, "back")
            _frame(client)
        else:
            _audit_path(client, (selection,), False, 8.0)
        log_pass(label)
    return client, skipped


def run(client: M1Hil) -> str:
    skipped: list[str] = []
    try:
        for label, path, edit in resume_targets(TOP_LEVEL_TARGETS):
            print(f"RUN  {label}", flush=True)
            client = _reboot_menu(client, 1)
            _audit_path(client, path, edit, 8.0)
            log_pass(label)
        client, skipped = _run_saved_actions(client)
    finally:
        client.close()
    completed = len(TOP_LEVEL_TARGETS)
    if not any(item.startswith("Saved item actions") for item in skipped):
        completed += len(SAVED_ACTIONS)
    summary = f"{completed} destinations responsive"
    if skipped:
        summary += f"; skipped: {', '.join(skipped)}"
    return summary


def run_read_stability(client: M1Hil, dwell_s: float = 2.0) -> str:
    try:
        _click(client, "ok")
        _click(client, "down")
        _click(client, "ok")
        menu = _frame(client)
        menu_title = _title_crc(menu)

        for label, selection in (("Read", 0), ("Pet Scanner", 1)):
            print(f"RUN  {label} read stability", flush=True)
            _click(client, "down", selection)
            _click(client, "ok")
            active = _wait_change(client, menu, 8.0)
            if active == menu:
                raise AssertionError(f"{label} did not open")
            active_title = _title_crc(active)

            for attempt in range(2):
                time.sleep(dwell_s)
                if _title_crc(_frame(client)) != active_title:
                    raise AssertionError(f"{label} did not remain responsive")
                _click(client, "back")
                menu = _wait_change(client, active, 3.0)
                if _title_crc(menu) != menu_title:
                    raise AssertionError(f"{label} BACK did not return to the RFID menu")
                if attempt == 0:
                    _click(client, "ok")
                    active = _wait_change(client, menu, 8.0)
                    if _title_crc(active) != active_title:
                        raise AssertionError(f"{label} re-entry failed")
            client.info()
    finally:
        client.close()
    return "Read and Pet Scanner dwell, BACK, and re-entry passed; tag detection requires live tags"
