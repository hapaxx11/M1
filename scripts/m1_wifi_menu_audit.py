#!/usr/bin/env python3
"""Exercise every static WiFi menu destination without retaining ambient screen data."""

from __future__ import annotations

import time
import zlib

from m1_hil import BUTTONS, M1Hil, connect
from m1_resume import log_pass, resume_targets, should_run


TOP_LEVEL_TARGETS = (
    ("Scan & Connect", (0,)),
    ("Recon/Station Scan", (1, 0)),
    ("Recon/2.4G Survey", (1, 1)),
    ("Recon/Channel Airtime", (1, 2)),
    ("Recon/Deauth Monitor", (1, 3)),
    ("Recon/WPS Auditor", (1, 4)),
    ("Recon/CSI Radar", (1, 5)),
    ("Recon/Signal Monitor", (1, 6)),
    ("Recon/MAC Track", (1, 7)),
    ("Recon/Packet Monitor", (1, 8)),
    ("Recon/Beacon Sniff", (1, 9)),
    ("Recon/Probe Sniff", (1, 10)),
    ("Recon/EAPOL Sniff", (1, 11)),
    ("Recon/Deauth Sniff", (1, 12)),
    ("Recon/Pwnagotchi", (1, 13)),
    ("Recon/SAE / WPA3", (1, 14)),
    ("Wardrive/AP Wardrive", (3, 0)),
    ("Wardrive/Station Wardrive", (3, 1)),
    ("Wardrive/Save APs", (3, 2)),
    ("Wardrive/Load APs", (3, 3)),
    ("Wardrive/Clear APs", (3, 4)),
    ("Wardrive/Load SSIDs", (3, 5)),
    ("Wardrive/Clear SSIDs", (3, 6)),
    ("802.15.4/Zigbee Scan", (4, 0)),
    ("802.15.4/Thread Scan", (4, 1)),
    ("Peer Link/Scan Peers", (5, 0)),
    ("Peer Link/Messages", (5, 1)),
    ("Peer Link/Send Capture/Sub-GHz", (5, 2, 0)),
    ("Peer Link/Send Capture/NFC", (5, 2, 1)),
    ("Peer Link/Send Capture/RFID", (5, 2, 2)),
    ("Peer Link/Send Capture/Infrared", (5, 2, 3)),
    ("Peer Link/Remote Trigger/Request Sub-GHz", (5, 3, 0, 0)),
    ("Peer Link/Remote Trigger/Request Infrared", (5, 3, 0, 1)),
    ("Peer Link/Remote Trigger/Allow Incoming", (5, 3, 1)),
    ("Peer Link/Receive File", (5, 4)),
    ("Peer Link/Tic-Tac-Toe", (5, 5)),
    ("General/View AP Info", (6, 0)),
    ("General/Select APs", (6, 1)),
    ("General/Select STAs", (6, 2)),
    ("General/Join WiFi", (6, 3)),
    ("General/Set MACs", (6, 4)),
    ("General/Set Channel", (6, 5)),
    ("General/Shutdown WiFi", (6, 6)),
    ("General/Set EP SSID", (6, 7)),
    ("General/Select EP HTML", (6, 8)),
    ("General/WiFi Hotspot", (6, 9)),
    ("General/Saved Networks", (6, 10)),
    ("General/Status", (6, 11)),
)

TARGET_ACTIONS = (
    ("Target/Handshake", 2),
    ("Target/Cycle AP", 5),
)

NETSCAN_ACTIONS = (
    ("Net Scan/Ping", 0),
    ("Net Scan/ARP", 1),
    ("Net Scan/SSH", 2),
    ("Net Scan/Telnet", 3),
    ("Net Scan/Ports", 4),
)


def _click(client: M1Hil, name: str, count: int = 1) -> None:
    client.click(BUTTONS[name], count=count)


def _frame(client: M1Hil) -> bytes:
    framebuffer = client.request(0x13, {0x12}, timeout=8)
    if len(framebuffer) != 1024 or len(set(framebuffer)) < 2:
        raise AssertionError("framebuffer is missing or blank")
    return framebuffer


def _title_crc(framebuffer: bytes) -> int:
    region = bytes(
        (framebuffer[x + (y // 8) * 128] >> (y % 8)) & 1
        for y in range(10)
        for x in range(96)
    )
    return zlib.crc32(region)


def _wait_change(client: M1Hil, previous: bytes, timeout_s: float) -> bytes:
    deadline = time.monotonic() + timeout_s
    while True:
        current = _frame(client)
        if current != previous or time.monotonic() >= deadline:
            return current
        time.sleep(0.2)


def _wait_title(client: M1Hil, expected: int, timeout_s: float) -> bytes:
    deadline = time.monotonic() + timeout_s
    while True:
        current = _frame(client)
        if _title_crc(current) == expected:
            return current
        if time.monotonic() >= deadline:
            raise AssertionError("parent title did not render before timeout")
        time.sleep(0.1)


def _open_wifi(client: M1Hil) -> int:
    _click(client, "ok")
    _click(client, "down", 5)
    _click(client, "ok")
    return _title_crc(_frame(client))


def _reboot_wifi(client: M1Hil) -> M1Hil:
    port = client.port.port
    baud = client.port.baudrate
    try:
        client.request(0x05, timeout=5)
    except TimeoutError:
        pass
    client.close()
    time.sleep(15.0)
    fresh = connect(port, baud, timeout=20)
    _open_wifi(fresh)
    return fresh


def _audit_path(client: M1Hil, path: tuple[int, ...], wait_s: float = 5.0,
                ancestor_titles: tuple[int, ...] = (), reentry: bool = True) -> None:
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
    previous = exit_to_parent(previous)
    if reentry:
        for _selection in path:
            _click(client, "ok")
            current = _wait_change(client, previous, wait_s)
            if current == previous:
                raise AssertionError("re-entry did not change after OK")
            previous = current
    exit_to_parent(previous)


def _select_target(client: M1Hil, wifi_title: int) -> int | None:
    before = _frame(client)
    _click(client, "ok")
    scan_result = _wait_change(client, before, 12.0)
    _click(client, "ok")
    target = _wait_change(client, scan_result, 4.0)
    target_title = _title_crc(target)
    return None if target_title == wifi_title else target_title


def _run_target_actions(client: M1Hil) -> tuple[M1Hil, list[str]]:
    client = _reboot_wifi(client)
    wifi_title = _title_crc(_frame(client))
    if _select_target(client, wifi_title) is None:
        return client, ["Target actions (no AP available)"]

    for label, selection in resume_targets(TARGET_ACTIONS):
        print(f"RUN  {label}", flush=True)
        client = _reboot_wifi(client)
        wifi_title = _title_crc(_frame(client))
        if _select_target(client, wifi_title) is None:
            raise AssertionError("AP target disappeared during the audit")
        _audit_path(client, (selection,), 8.0,
                    ancestor_titles=(wifi_title,),
                    reentry=(label != "Target/Handshake"))
        log_pass(label)
    return client, []


def _connect_saved(client: M1Hil) -> int | None:
    wifi_title = _title_crc(_frame(client))
    _click(client, "down", 6)
    _click(client, "ok")
    general_title = _title_crc(_frame(client))
    _click(client, "down", 10)
    _click(client, "ok")
    saved = _wait_change(client, _frame(client), 4.0)
    saved_title = _title_crc(saved)
    _click(client, "ok")
    connected = _wait_change(client, saved, 25.0)
    connected_title = _title_crc(connected)
    if connected_title in (saved_title, general_title, wifi_title):
        return None
    return connected_title


def _run_connected_actions(client: M1Hil) -> tuple[M1Hil, list[str]]:
    client = _reboot_wifi(client)
    if _connect_saved(client) is None:
        return client, ["Connected Status and Net Scan (no saved connection available)"]

    if should_run("Connected/Status"):
        print("RUN  Connected/Status", flush=True)
        _audit_path(client, (0,), reentry=False)
        log_pass("Connected/Status")

    for label, selection in resume_targets(NETSCAN_ACTIONS):
        print(f"RUN  {label}", flush=True)
        client = _reboot_wifi(client)
        wifi_title = _title_crc(_frame(client))
        connected_title = _connect_saved(client)
        if connected_title is None:
            raise AssertionError("saved connection disappeared during the audit")
        _click(client, "down")
        _click(client, "ok")
        _audit_path(client, (selection,), 8.0,
                    ancestor_titles=(connected_title, wifi_title), reentry=False)
        log_pass(label)

    if should_run("Connected/Disconnect"):
        print("RUN  Connected/Disconnect", flush=True)
        client = _reboot_wifi(client)
        wifi_title = _title_crc(_frame(client))
        if _connect_saved(client) is None:
            raise AssertionError("saved connection disappeared before disconnect")
        _audit_path(client, (2,), ancestor_titles=(wifi_title,), reentry=False)
        log_pass("Connected/Disconnect")
    return client, []


def run(client: M1Hil) -> str:
    skipped: list[str] = [
        "Peer Link pairing (requires peer)",
        "Attacks and 802.15.4 Flood (RF transmission)",
        "Target Connect/Deauth/Beacon/PMKID (RF transmission)",
    ]
    try:
        for label, path in resume_targets(TOP_LEVEL_TARGETS):
            print(f"RUN  {label}", flush=True)
            client = _reboot_wifi(client)
            _audit_path(client, path, 8.0)
            log_pass(label)

        client, target_skipped = _run_target_actions(client)
        skipped.extend(target_skipped)
        client, connected_skipped = _run_connected_actions(client)
        skipped.extend(connected_skipped)

        if should_run("General/Disconnect"):
            print("RUN  General/Disconnect", flush=True)
            client = _reboot_wifi(client)
            _audit_path(client, (6, 12), 8.0, reentry=False)
            log_pass("General/Disconnect")
        _frame(client)
    finally:
        client.close()

    completed = len(TOP_LEVEL_TARGETS) + 1
    if not any(item.startswith("Target actions") for item in skipped):
        completed += len(TARGET_ACTIONS)
    if not any(item.startswith("Connected Status") for item in skipped):
        completed += 2 + len(NETSCAN_ACTIONS)
    summary = f"{completed} destinations responsive"
    if skipped:
        summary += f"; skipped: {', '.join(skipped)}"
    return summary
