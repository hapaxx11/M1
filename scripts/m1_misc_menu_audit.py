#!/usr/bin/env python3
"""Exercise remaining non-Sub-GHz menus without executing user content."""

from __future__ import annotations

from m1_hil import M1Hil
from m1_bt_menu_audit import (
    _audit_path, _click, _frame, _open_apps_menu, _reboot_main, _reboot_menu,
    _title_crc,
)
from m1_saved_item_audit import select_first_file
from m1_wifi_menu_audit import _wait_change, _wait_title


IR_TARGETS = (
    ("Universal/TV", (0, 0), False),
    ("Universal/AC", (0, 1), False),
    ("Universal/Audio", (0, 2), False),
    ("Universal/Projector", (0, 3), False),
    ("Universal/Fan", (0, 4), False),
    ("Universal/LED", (0, 5), False),
    ("Universal/Browse IRDB", (0, 6), False),
    ("Universal/Search IRDB", (0, 7), False),
    ("Universal/Learned", (0, 8), False),
    ("Universal/Create Remote", (0, 9), False),
    ("Universal/Favorites", (0, 10), False),
    ("Universal/Recent", (0, 11), False),
    ("Learn", (1,), False),
    ("Replay", (2,), False),
    ("Custom Remote", (3,), False),
    ("ESL/Broadcast", (4, 0), False),
    ("ESL/Target", (4, 1), False),
)

GPIO_TARGETS = (
    ("GPIO Control", (0,), True),
    ("3.3V power", (1,), True),
    ("5V power", (2,), True),
    ("USB-UART bridge", (3,), False),
    ("Signal Gen", (4,), False),
    ("CAN/Sniffer", (5, 0), False),
    ("CAN/Send Frame", (5, 1), False),
    ("CAN/Saved", (5, 2), False),
)

GAMES_TARGETS = tuple(
    (label, (index,), False)
    for index, label in enumerate((
        "Snake", "Tetris", "T-Rex Runner", "Pong", "Dice Roll",
        "Music Player", "Hex Viewer", "Clock", "2048", "Flappy Bird",
        "Coin Flip", "Rock Paper Scissors", "Queens",
    ))
)


IR_SAVED_ACTIONS = (
    ("Replay/Send to Peer", 1),
    ("Replay/Info", 2),
    ("Replay/Rename (cancel)", 3),
    ("Replay/Delete (cancel)", 4),
)

CUSTOM_REMOTE_ACTIONS = (
    ("Custom Remote/New Button (cancel)", 0),
    ("Custom Remote/Rename Remote (cancel)", 1),
    ("Custom Remote/Delete Remote (cancel)", 2),
)

CUSTOM_BUTTON_ACTIONS = (
    ("Custom Button/Rename (cancel)", 1),
    ("Custom Button/Delete (cancel)", 2),
)


def _open_ir_saved(client: M1Hil) -> tuple[M1Hil, bool, int]:
    client = _reboot_menu(client, 3)
    _click(client, "down", 2)
    _click(client, "ok")
    if not select_first_file(client):
        return client, False, 0
    _click(client, "ok")
    command_title = _title_crc(_frame(client))
    _click(client, "left")
    _frame(client)
    return client, True, command_title


def _run_ir_saved_actions(client: M1Hil) -> tuple[M1Hil, int, list[str]]:
    client, present, command_title = _open_ir_saved(client)
    if not present:
        return client, 0, ["Infrared saved-file actions (no saved file)"]
    for label, selection in IR_SAVED_ACTIONS:
        print(f"RUN  Infrared/{label}", flush=True)
        client, present, command_title = _open_ir_saved(client)
        if not present:
            raise AssertionError("saved IR file disappeared during the audit")
        if selection == 4:
            _click(client, "down", selection)
            _click(client, "ok")
            _frame(client)
            _click(client, "back")
            _frame(client)
        else:
            _audit_path(client, (selection,), False, 8.0, reentry=False,
                        ancestor_titles=(command_title,))
        print(f"PASS Infrared/{label}", flush=True)
    return client, len(IR_SAVED_ACTIONS), []


def _open_custom_root(client: M1Hil) -> M1Hil:
    client = _reboot_menu(client, 3)
    _click(client, "down", 3)
    _click(client, "ok")
    _frame(client)
    return client


def _custom_new_remote_vkb(client: M1Hil) -> bytes:
    before = _frame(client)
    _click(client, "ok")
    keyboard = _wait_change(client, before, 4.0)
    _click(client, "back")
    _frame(client)
    return keyboard


def _open_custom_remote(client: M1Hil) -> tuple[M1Hil, bool]:
    client = _open_custom_root(client)
    keyboard = _custom_new_remote_vkb(client)
    before = _frame(client)
    _click(client, "down")
    _click(client, "ok")
    current = _wait_change(client, before, 4.0)
    if _title_crc(current) == _title_crc(keyboard):
        _click(client, "back")
        _frame(client)
        return client, False
    return client, True


def _open_custom_button_actions(client: M1Hil) -> tuple[M1Hil, bool]:
    client, present = _open_custom_remote(client)
    if not present:
        raise AssertionError("custom remote disappeared during the audit")
    before = _frame(client)
    _click(client, "ok")
    capture = _wait_change(client, before, 4.0)
    capture_title = _title_crc(capture)
    _click(client, "back")
    before = _frame(client)
    _click(client, "down", 3)
    _click(client, "ok")
    actions = _wait_change(client, before, 4.0)
    if _title_crc(actions) == capture_title:
        _click(client, "back")
        _frame(client)
        return client, False
    return client, True


def _run_custom_actions(client: M1Hil) -> tuple[M1Hil, int, list[str]]:
    skipped: list[str] = []
    print("RUN  Infrared/Custom Remote/New Remote (cancel)", flush=True)
    client = _open_custom_root(client)
    _custom_new_remote_vkb(client)
    print("PASS Infrared/Custom Remote/New Remote (cancel)", flush=True)

    client, present = _open_custom_remote(client)
    if not present:
        return client, 1, ["Custom remote/button actions (no saved remote)"]
    for label, selection in CUSTOM_REMOTE_ACTIONS:
        print(f"RUN  Infrared/{label}", flush=True)
        client, present = _open_custom_remote(client)
        if not present:
            raise AssertionError("custom remote disappeared during the audit")
        if selection == 2:
            _click(client, "down", selection)
            _click(client, "ok")
            _frame(client)
            _click(client, "back")
            _frame(client)
        else:
            _audit_path(client, (selection,), False, 8.0)
        print(f"PASS Infrared/{label}", flush=True)

    client, button_present = _open_custom_button_actions(client)
    if not button_present:
        skipped.append("Custom button actions (no saved button)")
        return client, 1 + len(CUSTOM_REMOTE_ACTIONS), skipped
    for label, selection in CUSTOM_BUTTON_ACTIONS:
        print(f"RUN  Infrared/{label}", flush=True)
        client, button_present = _open_custom_button_actions(client)
        if not button_present:
            raise AssertionError("custom button disappeared during the audit")
        if selection == 2:
            _click(client, "down", selection)
            _click(client, "ok")
            _frame(client)
            _click(client, "back")
            _frame(client)
        else:
            _audit_path(client, (selection,), False, 8.0)
        print(f"PASS Infrared/{label}", flush=True)
    return client, 1 + len(CUSTOM_REMOTE_ACTIONS) + len(CUSTOM_BUTTON_ACTIONS), skipped


def _audit_gpio_control(client: M1Hil) -> None:
    expected_title = _title_crc(_frame(client))
    for _attempt in range(2):
        _click(client, "ok")
        _wait_title(client, expected_title, 4.0)
        client.info()


def _audit_power_toggle(client: M1Hil, selection: int) -> None:
    original = _frame(client)
    _click(client, "down", selection)
    _click(client, "ok")
    changed = _wait_change(client, original, 4.0)
    if changed == original:
        raise AssertionError("power rail did not toggle")
    _click(client, "ok")
    restored = _wait_change(client, changed, 4.0)
    _click(client, "up", selection)
    restored = _wait_change(client, restored, 4.0)
    if restored != original:
        raise AssertionError("power rail was not restored to its original state")


def run(client: M1Hil) -> str:
    skipped = [
        "External app/script execution",
        "IR Send All/Send and ESL transmit commands",
        "Create Remote save",
    ]
    extra_completed = 0
    try:
        for label, path, repeat in IR_TARGETS:
            print(f"RUN  Infrared/{label}", flush=True)
            client = _reboot_menu(client, 3)
            _audit_path(client, path, False, 8.0)
            print(f"PASS Infrared/{label}", flush=True)
        client, completed, action_skips = _run_ir_saved_actions(client)
        extra_completed += completed
        skipped.extend(action_skips)
        client, completed, action_skips = _run_custom_actions(client)
        extra_completed += completed
        skipped.extend(action_skips)

        for label, path, repeat in GPIO_TARGETS:
            print(f"RUN  GPIO/{label}", flush=True)
            client = _reboot_menu(client, 4)
            if path == (0,):
                _audit_gpio_control(client)
            elif path in ((1,), (2,)):
                _audit_power_toggle(client, path[0])
            else:
                _audit_path(client, path, False, 8.0)
                if repeat:
                    _audit_path(client, path, False, 8.0)
            print(f"PASS GPIO/{label}", flush=True)

        for label, path, _repeat in GAMES_TARGETS:
            print(f"RUN  Games/{label}", flush=True)
            client = _reboot_menu(client, 8)
            _audit_path(client, path, False, 8.0)
            print(f"PASS Games/{label}", flush=True)

        print("RUN  BadUSB script picker", flush=True)
        client = _reboot_menu(client, 7)
        _frame(client)
        _click(client, "back")
        _frame(client)
        print("PASS BadUSB script picker", flush=True)

        print("RUN  Apps browser", flush=True)
        client = _reboot_main(client)
        if _open_apps_menu(client):
            _click(client, "back")
            _frame(client)
            print("PASS Apps browser", flush=True)
        else:
            skipped.append("Apps menu (no external apps installed)")
            print("SKIP Apps menu (not present)", flush=True)
    finally:
        client.close()

    completed = len(IR_TARGETS) + len(GPIO_TARGETS) + len(GAMES_TARGETS) + 1 + extra_completed
    if not any(item.startswith("Apps menu") for item in skipped):
        completed += 1
    summary = f"{completed} destinations responsive"
    if skipped:
        summary += f"; skipped: {', '.join(skipped)}"
    return summary
