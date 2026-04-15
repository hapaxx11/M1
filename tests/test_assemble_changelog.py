#!/usr/bin/env python3
"""Tests for scripts/assemble_changelog.py."""

import os
import pathlib
import sys
import textwrap
import unittest

# Add scripts/ to path so we can import the module
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "scripts"))
import assemble_changelog as ac


class TestParseCategory(unittest.TestCase):
    def test_valid_categories(self):
        self.assertEqual(ac._parse_category("foo.added.md"), "added")
        self.assertEqual(ac._parse_category("bar.fixed.md"), "fixed")
        self.assertEqual(ac._parse_category("baz.changed.md"), "changed")
        self.assertEqual(ac._parse_category("qux.removed.md"), "removed")

    def test_case_insensitive(self):
        self.assertEqual(ac._parse_category("foo.ADDED.md"), "added")
        self.assertEqual(ac._parse_category("foo.Fixed.md"), "fixed")

    def test_invalid_category(self):
        self.assertIsNone(ac._parse_category("foo.unknown.md"))
        self.assertIsNone(ac._parse_category("foo.md"))
        self.assertIsNone(ac._parse_category("foo.txt"))
        self.assertIsNone(ac._parse_category("foo"))

    def test_multi_dot_description(self):
        self.assertEqual(ac._parse_category("v0.9.0.fixed.md"), "fixed")


class TestReadEntries(unittest.TestCase):
    def _write(self, tmp_path, content):
        p = tmp_path / "test.md"
        p.write_text(content)
        return p

    def test_single_entry_no_dash(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            p = self._write(pathlib.Path(d), "**Feature** — description")
            entries = ac._read_entries(p)
            self.assertEqual(len(entries), 1)
            self.assertTrue(entries[0].startswith("- "))

    def test_single_entry_with_dash(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            p = self._write(pathlib.Path(d), "- **Feature** — description")
            entries = ac._read_entries(p)
            self.assertEqual(len(entries), 1)
            self.assertEqual(entries[0], "- **Feature** — description")

    def test_multiple_entries(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            content = "Entry one\n\nEntry two\n\nEntry three"
            p = self._write(pathlib.Path(d), content)
            entries = ac._read_entries(p)
            self.assertEqual(len(entries), 3)

    def test_multiline_entry(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            content = "**Feature** — first line\n  continuation line"
            p = self._write(pathlib.Path(d), content)
            entries = ac._read_entries(p)
            self.assertEqual(len(entries), 1)
            self.assertIn("continuation", entries[0])

    def test_empty_file(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            p = self._write(pathlib.Path(d), "")
            entries = ac._read_entries(p)
            self.assertEqual(len(entries), 0)


class TestFormatSections(unittest.TestCase):
    def test_single_category(self):
        entries = {
            "added": ["- Feature A"],
            "changed": [],
            "fixed": [],
            "removed": [],
        }
        result = ac.format_sections(entries)
        self.assertIn("### Added", result)
        self.assertIn("- Feature A", result)
        self.assertNotIn("### Changed", result)

    def test_multiple_categories(self):
        entries = {
            "added": ["- Feature A"],
            "changed": [],
            "fixed": ["- Bug B"],
            "removed": [],
        }
        result = ac.format_sections(entries)
        self.assertIn("### Added", result)
        self.assertIn("### Fixed", result)
        # Added should come before Fixed
        self.assertLess(result.index("### Added"), result.index("### Fixed"))

    def test_empty(self):
        entries = {cat: [] for cat in ac.CATEGORY_ORDER}
        result = ac.format_sections(entries)
        self.assertEqual(result, "")


class TestParseExistingUnreleased(unittest.TestCase):
    def test_parse_with_entries(self):
        text = textwrap.dedent("""\
            ### Added

            - Existing feature A
            - Existing feature B

            ### Fixed

            - Existing bug fix
        """)
        entries = ac._parse_existing_unreleased(text)
        self.assertEqual(len(entries["added"]), 2)
        self.assertEqual(len(entries["fixed"]), 1)

    def test_parse_empty(self):
        entries = ac._parse_existing_unreleased("")
        self.assertTrue(all(len(v) == 0 for v in entries.values()))

    def test_parse_multiline_bullet(self):
        text = textwrap.dedent("""\
            ### Added

            - **Feature** — first line
              continuation line
        """)
        entries = ac._parse_existing_unreleased(text)
        self.assertEqual(len(entries["added"]), 1)
        self.assertIn("continuation", entries["added"][0])


class TestMergeEntries(unittest.TestCase):
    def test_merge_non_overlapping(self):
        existing = {"added": ["- A"], "changed": [], "fixed": [], "removed": []}
        new = {"added": [], "changed": [], "fixed": ["- B"], "removed": []}
        merged = ac.merge_entries(existing, new)
        self.assertEqual(merged["added"], ["- A"])
        self.assertEqual(merged["fixed"], ["- B"])

    def test_merge_overlapping(self):
        existing = {"added": ["- A"], "changed": [], "fixed": [], "removed": []}
        new = {"added": ["- B"], "changed": [], "fixed": [], "removed": []}
        merged = ac.merge_entries(existing, new)
        self.assertEqual(merged["added"], ["- A", "- B"])


class TestCollectFragments(unittest.TestCase):
    def test_collect(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            # Temporarily override the changelog dir
            orig = ac.CHANGELOG_DIR
            ac.CHANGELOG_DIR = pathlib.Path(d)
            try:
                (pathlib.Path(d) / "feat.added.md").write_text("Feature A")
                (pathlib.Path(d) / "bug.fixed.md").write_text("Bug fix B")
                (pathlib.Path(d) / "README.md").write_text("Ignore me")
                (pathlib.Path(d) / ".gitkeep").write_text("")

                entries, files = ac.collect_fragments()
                self.assertEqual(len(entries["added"]), 1)
                self.assertEqual(len(entries["fixed"]), 1)
                self.assertEqual(len(files), 2)
            finally:
                ac.CHANGELOG_DIR = orig

    def test_collect_empty(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            orig = ac.CHANGELOG_DIR
            ac.CHANGELOG_DIR = pathlib.Path(d)
            try:
                entries, files = ac.collect_fragments()
                self.assertTrue(all(len(v) == 0 for v in entries.values()))
                self.assertEqual(len(files), 0)
            finally:
                ac.CHANGELOG_DIR = orig


class TestAssembleIntegration(unittest.TestCase):
    """End-to-end test: fragment files → CHANGELOG.md update."""

    def test_assemble_into_empty_unreleased(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            changelog = d / "CHANGELOG.md"
            fragments = d / ".changelog"
            fragments.mkdir()

            changelog.write_text(textwrap.dedent("""\
                # Changelog

                ## [Unreleased]

                ## [0.9.0.1] - 2026-01-01

                ### Added

                - Old feature
            """))

            (fragments / "new-feature.added.md").write_text("**New feature** — details")
            (fragments / "fix-thing.fixed.md").write_text("**Fixed thing** — details")

            orig_dir = ac.CHANGELOG_DIR
            orig_file = ac.CHANGELOG_FILE
            ac.CHANGELOG_DIR = fragments
            ac.CHANGELOG_FILE = changelog
            try:
                rc = ac.assemble(preview=False, keep=False)
                self.assertEqual(rc, 0)

                result = changelog.read_text()
                self.assertIn("### Added", result)
                self.assertIn("**New feature**", result)
                self.assertIn("### Fixed", result)
                self.assertIn("**Fixed thing**", result)
                # Old release entries preserved
                self.assertIn("## [0.9.0.1]", result)
                self.assertIn("Old feature", result)
                # Fragment files deleted
                self.assertFalse(list(fragments.glob("*.added.md")))
                self.assertFalse(list(fragments.glob("*.fixed.md")))
            finally:
                ac.CHANGELOG_DIR = orig_dir
                ac.CHANGELOG_FILE = orig_file

    def test_assemble_merges_with_existing(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            changelog = d / "CHANGELOG.md"
            fragments = d / ".changelog"
            fragments.mkdir()

            changelog.write_text(textwrap.dedent("""\
                # Changelog

                ## [Unreleased]

                ### Added

                - Existing entry

                ## [0.9.0.1] - 2026-01-01
            """))

            (fragments / "another.added.md").write_text("**Another feature**")

            orig_dir = ac.CHANGELOG_DIR
            orig_file = ac.CHANGELOG_FILE
            ac.CHANGELOG_DIR = fragments
            ac.CHANGELOG_FILE = changelog
            try:
                rc = ac.assemble(preview=False, keep=True)
                self.assertEqual(rc, 0)

                result = changelog.read_text()
                # Both entries should be under a single ### Added heading
                import re as _re
                added_headings = _re.findall(r"^### Added$", result, _re.MULTILINE)
                self.assertEqual(len(added_headings), 1, "Should merge into single ### Added section")
                self.assertIn("Existing entry", result)
                self.assertIn("**Another feature**", result)
                # Fragment file kept
                self.assertTrue((fragments / "another.added.md").exists())
            finally:
                ac.CHANGELOG_DIR = orig_dir
                ac.CHANGELOG_FILE = orig_file

    def test_preview_no_changes(self):
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            d = pathlib.Path(d)
            changelog = d / "CHANGELOG.md"
            fragments = d / ".changelog"
            fragments.mkdir()

            original = "# Changelog\n\n## [Unreleased]\n"
            changelog.write_text(original)
            (fragments / "feat.added.md").write_text("Preview feature")

            orig_dir = ac.CHANGELOG_DIR
            orig_file = ac.CHANGELOG_FILE
            ac.CHANGELOG_DIR = fragments
            ac.CHANGELOG_FILE = changelog
            try:
                rc = ac.assemble(preview=True)
                self.assertEqual(rc, 0)
                # File should be unchanged
                self.assertEqual(changelog.read_text(), original)
            finally:
                ac.CHANGELOG_DIR = orig_dir
                ac.CHANGELOG_FILE = orig_file


if __name__ == "__main__":
    unittest.main()
