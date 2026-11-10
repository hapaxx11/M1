#!/usr/bin/env python3
"""Exercise static NFC menu destinations without tag, credential, or file changes."""

from __future__ import annotations

from m1_hil import M1Hil
from m1_resume import log_pass, resume_targets
from m1_bt_menu_audit import _audit_path, _click, _frame, _reboot_menu, _title_crc
from m1_saved_item_audit import _has_file_icon, select_first_file
from m1_wifi_menu_audit import _wait_change


TOP_LEVEL_TARGETS = (
    ("Read", (0,), False),
    ("Detect Reader", (1,), False),
    ("Saved", (2,), False),
    ("Extra/Read MF Classic", (3, 0), False),
    ("Extra/Read MF Ultralight", (3, 1), False),
    ("Extra/Unlock NTAG/UL", (3, 2), False),
    ("Extra/Unlock SLIX-L", (3, 3), False),
    ("Extra/Tag Info", (3, 4), False),
    ("Extra/Clone Emulate", (3, 5), False),
    ("Extra/NFC Fuzzer", (3, 6), False),
    ("Extra/Write UID", (3, 7), False),
    ("Extra/Wipe Tag", (3, 8), False),
    ("Extra/Cyborg Detector", (3, 9), False),
    ("Extra/Read NDEF", (3, 10), False),
    ("Extra/NDEF Maker", (3, 11), False),
    ("Add Manually/UID", (4, 0), False),
    ("Add Manually/Mifare Classic 1K", (4, 1), False),
    ("Add Manually/Mifare Classic 4K", (4, 2), False),
    ("Add Manually/NTAG213", (4, 3), False),
    ("Add Manually/NTAG215", (4, 4), False),
    ("Add Manually/NTAG216", (4, 5), False),
    ("Field Detect", (5,), False),
    ("NDEF Maker/URL", (3, 11, 0), False),
    ("NDEF Maker/Text", (3, 11, 1), False),
    ("NDEF Maker/Phone", (3, 11, 2), False),
    ("NDEF Maker/Wi-Fi", (3, 11, 3), False),
)

SAVED_ACTIONS = (
    ("Saved/Edit UID", 3),
    ("Saved/Info", 5),
    ("Saved/Rename", 6),
    ("Saved/Delete (cancel)", 7),
)

CARD_ACTIONS = (
    ("Card Actions/Tag Info", 0),
    ("Card Actions/Clone Emulate", 1),
    ("Card Actions/NFC Fuzzer", 2),
    ("Card Actions/Cyborg Detector", 5),
    ("Card Actions/Read NDEF", 6),
    ("Card Actions/NDEF Maker", 7),
)


def _select_saved(client: M1Hil) -> int | None:
    _click(client, "down", 2)
    _click(client, "ok")
    if not select_first_file(client):
        return None
    current = _frame(client)
    browser_title = _title_crc(current)
    for _attempt in range(8):
        _click(client, "ok")
        opened = _wait_change(client, current, 8.0)
        _click(client, "down")
        responsive = _wait_change(client, opened, 4.0)
        if responsive != opened:
            _click(client, "up")
            _wait_change(client, responsive, 4.0)
            return browser_title
        _click(client, "back")
        dismissed = _wait_change(client, opened, 4.0)
        _click(client, "down")
        current = _wait_change(client, dismissed, 4.0)
        if not _has_file_icon(current):
            break
    return None


def _run_saved_actions(client: M1Hil) -> tuple[M1Hil, list[str]]:
    skipped = [
        "Read Save/Emulate/Card Actions (requires live card)",
        "Card Actions/Write UID and Wipe Tag (destructive)",
        "NDEF Maker/Wi-Fi security (text input required)",
        "Saved/Send to Peer (requires peer)",
        "Saved/Emulate UID and Unlock (requires compatible live card)",
    ]
    client = _reboot_menu(client, 2)
    nfc_root_title = _title_crc(_frame(client))
    ancestor_title = _select_saved(client)
    if not ancestor_title:
        skipped.append("Saved item actions (no saved file)")
        return client, skipped

    for label, selection in resume_targets(SAVED_ACTIONS):
        print(f"RUN  {label}", flush=True)
        if selection != 0:
            client = _reboot_menu(client, 2)
            nfc_root_title = _title_crc(_frame(client))
            ancestor_title = _select_saved(client)
            if not ancestor_title:
                raise AssertionError("saved file disappeared during the audit")
        if selection == 7:
            _click(client, "down", selection)
            _click(client, "ok")
            _frame(client)
            _click(client, "back")
            _frame(client)
        else:
            _audit_path(client, (selection,), False, 8.0, reentry=False,
                        ancestor_titles=(ancestor_title, nfc_root_title))
        log_pass(label)

    for label, selection in resume_targets(CARD_ACTIONS):
        print(f"RUN  {label}", flush=True)
        client = _reboot_menu(client, 2)
        nfc_root_title = _title_crc(_frame(client))
        ancestor_title = _select_saved(client)
        if not ancestor_title:
            raise AssertionError("saved file disappeared during card actions")
        _click(client, "down", 4)
        _click(client, "ok")
        _audit_path(client, (selection,), False, 8.0, reentry=False,
                    ancestor_titles=(ancestor_title, nfc_root_title))
        log_pass(label)
    return client, skipped


def run(client: M1Hil) -> str:
    skipped: list[str] = []
    try:
        for label, path, edit in resume_targets(TOP_LEVEL_TARGETS):
            print(f"RUN  {label}", flush=True)
            client = _reboot_menu(client, 2)
            _audit_path(client, path, edit, 8.0,
                        reentry=not label.startswith("NDEF Maker/"))
            log_pass(label)
        client, skipped = _run_saved_actions(client)
    finally:
        client.close()
    completed = len(TOP_LEVEL_TARGETS)
    if not any(item.startswith("Saved item actions") for item in skipped):
        completed += len(SAVED_ACTIONS) + len(CARD_ACTIONS)
    summary = f"{completed} destinations responsive"
    if skipped:
        summary += f"; skipped: {', '.join(skipped)}"
    return summary
