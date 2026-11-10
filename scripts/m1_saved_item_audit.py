#!/usr/bin/env python3
"""Privacy-safe file-picker helpers for saved-item HIL audits."""

from __future__ import annotations

from m1_hil import M1Hil
from m1_wifi_menu_audit import _click, _frame


_FILE_ICON = (0x3FF, 0x241, 0x29D, 0x301, 0x27D, 0x201, 0x255, 0x3FF)


def _file_icon_at_y(framebuffer: bytes, y: int) -> bool:
    actual = []
    for dy in range(8):
        value = 0
        for dx in range(10):
            x = 1 + dx
            value |= ((framebuffer[x + ((y + dy) // 8) * 128] >> ((y + dy) % 8)) & 1) << dx
        actual.append(value)
    return actual == list(_FILE_ICON) or actual == [(~value) & 0x3FF for value in _FILE_ICON]


def _is_file_icon(framebuffer: bytes, row: int, row_height: int = 8) -> bool:
    return _file_icon_at_y(framebuffer, 12 + row * row_height)


def _has_file_icon(framebuffer: bytes) -> bool:
    for y in range(12, 57):
        actual = []
        for dy in range(8):
            value = 0
            for dx in range(10):
                x = 1 + dx
                value |= ((framebuffer[x + ((y + dy) // 8) * 128] >> ((y + dy) % 8)) & 1) << dx
            actual.append(value)
        if actual == [(~value) & 0x3FF for value in _FILE_ICON]:
            return True
    return False


def select_first_file(client: M1Hil) -> bool:
    for _index in range(1, 49):
        _click(client, "down")
        if _has_file_icon(_frame(client)):
            return True
    return False
