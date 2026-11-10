#!/usr/bin/env python3
"""Tests for privacy-preserving HIL visual-geometry checks."""

import json
import pathlib
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
import m1_ux_audit as ux  # noqa: E402
import m1_bt_menu_audit as bt_audit  # noqa: E402
import m1_wifi_menu_audit as wifi_audit  # noqa: E402


def set_pixel(framebuffer, x, y):
    framebuffer[x + (y // 8) * 128] |= 1 << (y % 8)


def compliant_frame():
    framebuffer = bytearray(1024)
    for x in range(128):
        set_pixel(framebuffer, x, 10)
    set_pixel(framebuffer, 4, 4)
    return framebuffer


class TestUxGeometry(unittest.TestCase):
    def test_compliant_header_has_no_finding(self):
        self.assertEqual([], ux.analyze(compliant_frame()))

    def test_inverted_header_is_flagged(self):
        framebuffer = bytearray(1024)
        for y in range(12):
            for x in range(128):
                set_pixel(framebuffer, x, y)
        self.assertIn("inverted-header", ux.analyze(framebuffer))

    def test_missing_header_separator_is_flagged(self):
        framebuffer = bytearray(1024)
        set_pixel(framebuffer, 4, 4)
        self.assertIn("header-no-separator", ux.analyze(framebuffer))

    def test_solid_bottom_bar_is_flagged(self):
        framebuffer = compliant_frame()
        for y in range(52, 60):
            for x in range(128):
                set_pixel(framebuffer, x, y)
        self.assertIn("solid-bottom-bar", ux.analyze(framebuffer))

    def test_populated_rounded_buttons_are_not_a_solid_bar(self):
        framebuffer = compliant_frame()
        for left in (0, 43, 86):
            for y in range(54, 62):
                for x in range(left, left + 42):
                    set_pixel(framebuffer, x, y)
        self.assertNotIn("solid-bottom-bar", ux.analyze(framebuffer))

    def test_results_contain_only_labels_and_geometry(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            ux.start("demo", root, True)
            ux.observe(compliant_frame())
            ux.evaluate("Synthetic scene")
            ux.finish()
            results = json.loads((root / "demo-ux.json").read_text(encoding="utf-8"))
        self.assertEqual([{"label": "Synthetic scene", "findings": []}], results)

    def test_ux_navigation_uses_back_instead_of_reboot(self):
        for module in (bt_audit, wifi_audit):
            with self.subTest(module=module.__name__):
                client = mock.MagicMock()
                module._ux_main_title = 100
                with (
                    mock.patch.object(module.m1_ux_audit, "is_enabled", return_value=True),
                    mock.patch.object(module, "_frame", side_effect=(b"module", b"main")),
                    mock.patch.object(module, "_wait_change", return_value=b"main"),
                    mock.patch.object(module, "_title_crc",
                                      side_effect=lambda frame: 100 if frame == b"main" else 1),
                    mock.patch.object(module, "_click") as click,
                ):
                    if module is bt_audit:
                        self.assertIs(client, module._reboot_main(client))
                    else:
                        self.assertIs(client, module._reboot_wifi(client))
                self.assertIn(mock.call(client, "back"), click.call_args_list)
                client.request.assert_not_called()
                client.close.assert_not_called()
                module._ux_main_title = None

    def test_ux_navigation_falls_back_to_reboot_on_identity_mismatch(self):
        client = mock.MagicMock()
        fresh = mock.MagicMock()
        bt_audit._ux_main_title = 100
        with (
            mock.patch.object(bt_audit.m1_ux_audit, "is_enabled", return_value=True),
            mock.patch.object(bt_audit, "_frame", side_effect=(b"module", b"other")),
            mock.patch.object(bt_audit, "_wait_change", side_effect=(b"other", b"home")),
            mock.patch.object(bt_audit, "_title_crc",
                              side_effect=lambda frame: 100 if frame == b"main" else 1),
            mock.patch.object(bt_audit, "_click"),
            mock.patch.object(bt_audit, "connect", return_value=fresh),
            mock.patch.object(bt_audit.time, "sleep"),
        ):
            self.assertIs(fresh, bt_audit._reboot_main(client))
        client.close.assert_called_once_with()
        bt_audit._ux_main_title = None


if __name__ == "__main__":
    unittest.main()
