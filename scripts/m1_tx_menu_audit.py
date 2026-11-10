#!/usr/bin/env python3
"""Opt-in transmit-scene traversal with bounded cleanup and no destructive actions."""

from __future__ import annotations

from m1_hil import M1Hil
import m1_ux_audit
from m1_resume import log_pass, resume_targets, should_run
from m1_wifi_menu_audit import _wait_change
from m1_bt_menu_audit import _audit_path, _click, _frame, _reboot_bluetooth, _reboot_menu
from m1_wifi_menu_audit import _audit_path as _audit_wifi_path
from m1_wifi_menu_audit import _reboot_wifi, _select_target, _title_crc
from m1_misc_menu_audit import _open_ir_saved
from m1_nfc_menu_audit import _select_saved as _select_nfc_saved
from m1_subghz_menu_audit import _read_raw_title, _select_saved as _select_subghz_saved
from m1_saved_item_audit import select_first_file


WIFI_TX = (
    ("WiFi/Attacks/Deauth", (2, 0)),
    ("WiFi/Attacks/Beacon Spam", (2, 1)),
    ("WiFi/Attacks/AP Clone", (2, 2)),
    ("WiFi/Attacks/Rickroll", (2, 3)),
    ("WiFi/Attacks/Probe Flood", (2, 5)),
    ("WiFi/Attacks/PMKID Grab", (2, 7)),
    ("WiFi/802.15.4 Flood", (4, 2)),
)

WIFI_TARGET_TX = (
    ("WiFi/Target/Deauth", 1),
    ("WiFi/Target/Beacon", 3),
    ("WiFi/Target/PMKID", 4),
)

BT_TX = (
    ("Bluetooth/BLE Advertise", (2,)),
    ("Bluetooth/BLE Spam/Sour Apple", (5, 1)),
    ("Bluetooth/BLE Spam/SwiftPair", (5, 2)),
    ("Bluetooth/BLE Spam/Samsung", (5, 3)),
    ("Bluetooth/BLE Spam/Google FP", (5, 4)),
    ("Bluetooth/BLE Spam/Flipper", (5, 5)),
    ("Bluetooth/BLE Spam/All", (5, 6)),
    ("Bluetooth/BLE Spam/AirTag Spoof", (5, 7)),
)

SUBGHZ_TX = ()

SKIPPED = (
    "Add Manually transmitter handoff (no completed transition or visible error)",
    "Portal credential capture and file-writing wizards",
    "Peer, live-card, captured-pager, decoded-signal, and missing-file fixtures",
    "Destructive operations and unknown user scripts/apps",
)


def _run_wifi(state: dict) -> None:
    for label, path in resume_targets(WIFI_TX):
        print(f"RUN  {label}", flush=True)
        state["client"] = _reboot_wifi(state["client"])
        _audit_wifi_path(state["client"], path, 12.0, reentry=False)
        log_pass(label)

    for label, selection in resume_targets(WIFI_TARGET_TX):
        print(f"RUN  {label}", flush=True)
        state["client"] = _reboot_wifi(state["client"])
        wifi_title = _title_crc(_frame(state["client"]))
        if _select_target(state["client"], wifi_title) is None:
            raise AssertionError("no target available for authorized transmit audit")
        _audit_wifi_path(state["client"], (selection,), 12.0,
                         ancestor_titles=(wifi_title,), reentry=False)
        log_pass(label)


def _run_bluetooth(state: dict) -> None:
    for label, path in resume_targets(BT_TX):
        print(f"RUN  {label}", flush=True)
        state["client"] = _reboot_bluetooth(state["client"])
        _audit_path(state["client"], path, False, 10.0, reentry=False)
        log_pass(label)


def _run_subghz(state: dict) -> None:
    for label, path in resume_targets(SUBGHZ_TX):
        print(f"RUN  {label}", flush=True)
        state["client"] = _reboot_menu(state["client"], 0)
        _audit_path(state["client"], path, False, 10.0, reentry=False)
        log_pass(label)

    if should_run("Sub-GHz/Playlist transmission"):
        label = "Sub-GHz/Playlist transmission"
        print(f"RUN  {label}", flush=True)
        state["client"] = _reboot_menu(state["client"], 0)
        _click(state["client"], "down", 3)
        _click(state["client"], "ok")
        if not select_first_file(state["client"]):
            raise AssertionError("no saved playlist file")
        _click(state["client"], "ok")
        ready = _frame(state["client"])
        _click(state["client"], "ok")
        active = _wait_change(state["client"], ready, 10.0)
        if active == ready:
            raise AssertionError("playlist did not enter transmission")
        m1_ux_audit.observe(active)
        _click(state["client"], "back")
        stopped = _wait_change(state["client"], active, 10.0)
        if stopped == active:
            raise AssertionError("playlist transmission did not stop")
        log_pass(label)

    if should_run("Sub-GHz/Saved Emulate"):
        state["client"], raw_title = _read_raw_title(state["client"])
        state["client"] = _reboot_menu(state["client"], 0)
        if _select_subghz_saved(state["client"], raw_title) != "parsed":
            raise AssertionError("parsed saved file required for transmit audit")
        _audit_path(state["client"], (0,), False, 10.0, reentry=False)
        log_pass("Sub-GHz/Saved Emulate")


def _run_ir_nfc(state: dict) -> None:
    if should_run("Infrared/ESL/Broadcast Page 0"):
        state["client"] = _reboot_menu(state["client"], 3)
        _audit_path(state["client"], (4, 0, 0), False, 10.0, reentry=False)
        log_pass("Infrared/ESL/Broadcast Page 0")

    if should_run("Infrared/Send All"):
        state["client"], present, command_title = _open_ir_saved(state["client"])
        if not present:
            raise AssertionError("saved IR file required for transmit audit")
        _audit_path(state["client"], (0,), False, 10.0, reentry=False,
                    ancestor_titles=(command_title,))
        log_pass("Infrared/Send All")

    if should_run("NFC/Saved Emulate UID"):
        state["client"] = _reboot_menu(state["client"], 2)
        if not _select_nfc_saved(state["client"]):
            raise AssertionError("valid NFC file required for transmit audit")
        _audit_path(state["client"], (0,), False, 10.0, reentry=False)
        log_pass("NFC/Saved Emulate UID")


def run(client: M1Hil) -> str:
    completed = 0
    state = {"client": client}
    try:
        _run_wifi(state)
        completed += len(WIFI_TX) + len(WIFI_TARGET_TX)
        _run_bluetooth(state)
        completed += len(BT_TX)
        _run_subghz(state)
        completed += len(SUBGHZ_TX) + 2
        _run_ir_nfc(state)
        completed += 3
        _frame(state["client"])
    finally:
        state["client"].close()
    return f"{completed} transmit destinations responsive; skipped: {', '.join(SKIPPED)}"
