#!/usr/bin/env python3
"""Exercise Sub-GHz menus without RF transmissions or destructive edits."""

from __future__ import annotations

from m1_hil import M1Hil
from m1_bt_menu_audit import _audit_path, _click, _frame, _reboot_menu
from m1_saved_item_audit import select_first_file
from m1_wifi_menu_audit import _wait_change


TOP_LEVEL_TARGETS = (
    ("Read", (0,), False),
    ("Read Raw", (1,), False),
    ("Saved", (2,), False),
    ("Playlist picker", (3,), False),
    ("Analyzer/Smart ID", (4, 0), False),
    ("Analyzer/Frequency Analyzer", (4, 1), False),
    ("Analyzer/Spectrum Analyzer", (4, 2), False),
    ("Analyzer/RSSI Meter", (4, 3), False),
    ("Analyzer/Freq Scanner", (4, 4), False),
    ("Analyzer/Proto Pirate Receiver", (4, 5, 0), False),
    ("Analyzer/Proto Pirate Sub Decode", (4, 5, 1), False),
    ("Analyzer/Proto Pirate Timing Tuner", (4, 5, 2), False),
    ("Analyzer/Proto Pirate Models DB", (4, 5, 3), False),
    ("Analyzer/Proto Pirate Remote Analyzer", (4, 5, 4), False),
    ("Analyzer/Proto Pirate Emulate (cancel)", (4, 5, 5), False),
    ("Analyzer/Proto Pirate Brute Force (cancel)", (4, 5, 6), False),
    ("POCSAG/FLEX", (5,), False),
    ("Restaurant Pager", (6,), False),
    ("Restaurant Pager/Edit (cancel)", (6, 0), False),
    ("Restaurant Pager/Browse Saved", (6, 3), False),
    ("Restaurant Pager/Settings", (6, 4), False),
    ("TPMS", (7,), False),
    ("Weather Station", (8,), False),
    ("Brute Force (cancel)", (9,), False),
    ("Add Manually/Set Type", (10,), False),
    ("Remote picker", (11,), False),
    ("Bind/CAME Atomo", (12, 0), False),
    ("Bind/Nice FloR-S", (12, 1), False),
    ("Bind/Alutech AT-4N", (12, 2), False),
    ("Bind/DITEC GOL4", (12, 3), False),
    ("Bind/KingGates Stylo4k", (12, 4), False),
    ("Read/Config", (0,), True),
)

SAVED_ACTIONS = (
    ("Saved/Send to Peer", 1),
    ("Saved/Info", 2),
    ("Saved/Rename", 3),
    ("Saved/Delete (cancel)", 4),
)

MORE_RAW_ACTIONS = (
    ("MoreRAW/Decode", 0),
    ("MoreRAW/Rename", 1),
    ("MoreRAW/Delete (cancel)", 2),
)

ADD_MANUALLY_TARGETS = tuple(
    (label, index)
    for index, label in enumerate((
        "CAME Atomo 433", "Nice FloR-S 433", "Alutech AT-4N 433",
        "DITEC GOL4 433", "KingGates Stylo4k 433", "Princeton 433",
        "Princeton 315", "Nice FLO 12b", "Nice FLO 24b", "CAME 12bit",
        "CAME 24bit", "CAME 12b 868", "Linear 300", "Gate TX 433",
        "DoorHan 315", "DoorHan 433", "Holtek HT12X 433", "KeeLoq 433",
        "Star Line 433", "Jarolift 433",
    ))
)

KEELOQ_EDITOR_CHAIN = ("Set Button", "Set Counter", "Manufacturer Key")

SKIPPED_RF = (
    "Playlist transmission",
    "Proto Pirate transmission/key recovery execution",
    "Restaurant Send All/Save Station",
    "Saved Emulate",
    "Add Manually Transmitter",
    "Remote button transmission",
    "Bind step transmission",
    "Receiver Info/Save flow (requires decoded signal)",
)


def _select_saved(client: M1Hil) -> bool:
    _click(client, "down", 2)
    _click(client, "ok")
    if not select_first_file(client):
        return False
    _click(client, "ok")
    return True


def _run_saved_actions(client: M1Hil) -> tuple[M1Hil, list[str]]:
    skipped: list[str] = []
    client = _reboot_menu(client, 0)
    if not _select_saved(client):
        skipped.extend(("Saved actions (no saved file)", "MoreRAW actions (no saved file)"))
        return client, skipped

    for label, selection in SAVED_ACTIONS:
        print(f"RUN  {label}", flush=True)
        if selection != 1:
            client = _reboot_menu(client, 0)
            if not _select_saved(client):
                raise AssertionError("saved file disappeared during the audit")
        if selection == 4:
            _click(client, "down")
            _click(client, "ok")
            _frame(client)
            _click(client, "back")
            _frame(client)
        else:
            _audit_path(client, (selection,), False, 8.0)
        print(f"PASS {label}", flush=True)

    for label, selection in MORE_RAW_ACTIONS:
        print(f"RUN  {label}", flush=True)
        client = _reboot_menu(client, 0)
        if not _select_saved(client):
            raise AssertionError("saved file disappeared during MoreRAW")
        before = _frame(client)
        _click(client, "right")
        more = _wait_change(client, before, 3.0)
        if more == before:
            skipped.append("MoreRAW actions (no compatible raw file)")
            break
        if selection == 2:
            _click(client, "down")
            _click(client, "ok")
            _frame(client)
            _click(client, "back")
            _frame(client)
        else:
            _audit_path(client, (selection,), False, 8.0)
        print(f"PASS {label}", flush=True)
    return client, skipped


def run(client: M1Hil) -> str:
    skipped = list(SKIPPED_RF) + ["SignalSettings (requires compatible saved file)"]
    try:
        for label, path, config in TOP_LEVEL_TARGETS:
            print(f"RUN  {label}", flush=True)
            client = _reboot_menu(client, 0)
            if config:
                _click(client, "left")
                _frame(client)
            else:
                _audit_path(client, path, False, 8.0)
            print(f"PASS {label}", flush=True)

        print("RUN  Read/Config/Protocol Filter", flush=True)
        client = _reboot_menu(client, 0)
        _click(client, "left")
        _click(client, "down", 11)
        _click(client, "ok")
        _frame(client)
        _click(client, "back")
        _frame(client)
        print("PASS Read/Config/Protocol Filter", flush=True)

        for label, selection in ADD_MANUALLY_TARGETS:
            print(f"RUN  Add Manually/{label} editor (cancel)", flush=True)
            client = _reboot_menu(client, 0)
            _audit_path(client, (10, selection), False, 8.0)
            print(f"PASS Add Manually/{label} editor (cancel)", flush=True)

        client = _reboot_menu(client, 0)
        _click(client, "down", 10)
        _click(client, "ok")
        _click(client, "ok")
        _frame(client)
        for label in KEELOQ_EDITOR_CHAIN:
            print(f"RUN  Add Manually/{label} (cancel)", flush=True)
            _click(client, "ok")
            _frame(client)
            print(f"PASS Add Manually/{label} (cancel)", flush=True)

        client, saved_skipped = _run_saved_actions(client)
        skipped.extend(saved_skipped)
    finally:
        client.close()

    completed = (len(TOP_LEVEL_TARGETS) + 1 + len(ADD_MANUALLY_TARGETS) +
                 len(KEELOQ_EDITOR_CHAIN))
    if not any(item.startswith("Saved actions") for item in skipped):
        completed += len(SAVED_ACTIONS)
    if not any(item.startswith("MoreRAW actions") for item in skipped):
        completed += len(MORE_RAW_ACTIONS)
    summary = f"{completed} destinations responsive"
    if skipped:
        summary += f"; skipped: {', '.join(skipped)}"
    return summary
