#!/usr/bin/env python3
"""Tests for scripts/version_selection.py."""

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "scripts"))
import version_selection as vs  # noqa: E402


class TestVersionParsing(unittest.TestCase):
    def test_accepts_valid_versions_with_or_without_prefix(self):
        self.assertEqual(vs.parse_version("v0.9.4.21"), (0, 9, 4, 21))
        self.assertEqual(vs.parse_version("0.9.4.21"), (0, 9, 4, 21))

    def test_rejects_malformed_and_out_of_range_versions(self):
        for value in ("", "v.0.9.4.1", "0.9.4.256", "0.9.4.x"):
            with self.subTest(value=value):
                self.assertIsNone(vs.parse_version(value))


class TestReleaseSelection(unittest.TestCase):
    def test_published_revision_wins_over_stale_header(self):
        released, next_version = vs.next_release_version(
            (0, 9, 4, 0), ["v0.9.4.20", "v0.9.4.21", "v0.9.3.99"]
        )
        self.assertEqual(released, (0, 9, 4, 21))
        self.assertEqual(next_version, (0, 9, 4, 22))

    def test_pending_header_revision_wins_over_older_release(self):
        released, next_version = vs.next_release_version(
            (0, 9, 4, 22), ["v0.9.4.21"]
        )
        self.assertEqual(released, (0, 9, 4, 21))
        self.assertEqual(next_version, (0, 9, 4, 22))

    def test_pending_prefix_bump_is_selected(self):
        released, next_version = vs.next_release_version(
            (0, 9, 5, 0), ["v0.9.4.21"]
        )
        self.assertEqual(released, (0, 9, 4, 21))
        self.assertEqual(next_version, (0, 9, 5, 0))

    def test_first_automatic_release_starts_at_revision_one(self):
        released, next_version = vs.next_release_version((0, 9, 4, 0), [])
        self.assertIsNone(released)
        self.assertEqual(next_version, (0, 9, 4, 1))

    def test_published_revision_255_cannot_be_incremented(self):
        with self.assertRaisesRegex(ValueError, "reached 255"):
            vs.next_release_version((0, 9, 4, 0), ["v0.9.4.255"])

    def test_pending_revision_255_can_still_be_released(self):
        released, next_version = vs.next_release_version(
            (0, 9, 4, 255), ["v0.9.4.254"]
        )
        self.assertEqual(released, (0, 9, 4, 254))
        self.assertEqual(next_version, (0, 9, 4, 255))


class TestManualBumpValidation(unittest.TestCase):
    def test_uses_latest_published_version_as_baseline(self):
        self.assertEqual(
            vs.validate_bump((0, 9, 4, 0), ["v0.9.4.21"], "v0.9.4.22"),
            (0, 9, 4, 22),
        )

    def test_uses_pending_header_revision_as_baseline(self):
        self.assertEqual(
            vs.validate_bump((0, 9, 4, 22), ["v0.9.4.21"], "v0.9.4.23"),
            (0, 9, 4, 23),
        )

    def test_rejects_components_above_firmware_storage_limit(self):
        for version in ("0.9.256.0", "0.9.4.256"):
            with self.subTest(version=version):
                with self.assertRaisesRegex(ValueError, "between 0 and 255"):
                    vs.validate_bump((0, 9, 4, 0), [], version)

    def test_rejects_build_overflow_from_increment(self):
        with self.assertRaisesRegex(ValueError, "between 0 and 255"):
            vs.validate_bump((0, 9, 255, 0), [], "0.9.256.0")

    def test_reports_when_no_component_can_be_incremented(self):
        with self.assertRaisesRegex(ValueError, "No next version"):
            vs.validate_bump((255, 255, 255, 255), [], "255.255.255.255")


if __name__ == "__main__":
    unittest.main()
