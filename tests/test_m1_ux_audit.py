#!/usr/bin/env python3
"""Tests for privacy-preserving HIL visual-geometry checks."""

import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
import m1_ux_audit as ux  # noqa: E402


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
            data = json.loads((root / "demo-ux.json").read_text(encoding="utf-8"))
        self.assertEqual("1", data["format"])
        self.assertEqual([{"label": "Synthetic scene", "findings": []}], data["results"])

    def test_resume_preserves_findings_and_fresh_clears_them(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            ux.start("demo", root, True, version="v1")
            ux.observe(compliant_frame())
            ux.evaluate("First")
            ux.finish()
            ux.start("demo", root, True, version="v1")
            ux.evaluate("Second")
            ux.finish()
            data = json.loads((root / "demo-ux.json").read_text(encoding="utf-8"))
            self.assertEqual(["First", "Second"], [item["label"] for item in data["results"]])
            ux.start("demo", root, True, fresh=True, version="v1")
            ux.evaluate("Second")
            ux.finish()
            data = json.loads((root / "demo-ux.json").read_text(encoding="utf-8"))
            self.assertEqual(["Second"], [item["label"] for item in data["results"]])

if __name__ == "__main__":
    unittest.main()
