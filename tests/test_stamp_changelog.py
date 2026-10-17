#!/usr/bin/env python3
"""Tests for scripts/stamp_changelog.py."""

import os
import pathlib
import sys
import tempfile
import textwrap
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "scripts"))
import assemble_changelog as ac  # noqa: E402
import stamp_changelog as sc  # noqa: E402

BASE = textwrap.dedent("""\
    # Changelog

    ## [Unreleased]

    ### Added

    - New feature

    ### Fixed

    - A fix
      with continuation

    ## [0.9.4.12] - 2026-10-15

    ### Added

    - Old feature
""")


class TestParseVersion(unittest.TestCase):
    def test_valid(self):
        self.assertEqual(sc.parse_version("0.9.4.12"), (0, 9, 4, 12))
        self.assertEqual(sc.parse_version("v0.9.4.12"), (0, 9, 4, 12))

    def test_invalid(self):
        self.assertIsNone(sc.parse_version(""))
        self.assertIsNone(sc.parse_version(None))
        self.assertIsNone(sc.parse_version("Unreleased"))
        self.assertIsNone(sc.parse_version("v1.x"))


class TestStamp(unittest.TestCase):
    def test_stamps_unreleased_content(self):
        out = sc.stamp(BASE, "0.9.4.13", "0.9.4.12", "2026-10-16")
        self.assertIn("## [Unreleased]\n\n## [0.9.4.13] - 2026-10-16\n\n### Added\n\n- New feature\n", out)
        self.assertIn("- A fix\n  with continuation\n\n## [0.9.4.12] - 2026-10-15", out)
        self.assertEqual(out.count("## [Unreleased]"), 1)
        self.assertTrue(out.endswith("- Old feature\n"))

    def test_idempotent_and_keeps_date(self):
        once = sc.stamp(BASE, "0.9.4.13", "0.9.4.12", "2026-10-16")
        twice = sc.stamp(once, "0.9.4.13", "0.9.4.12", "2026-10-20")
        self.assertEqual(once, twice)

    def test_missing_unreleased_is_noop(self):
        text = "# Changelog\n\n## [0.9.4.12] - 2026-10-15\n\n### Added\n\n- Old\n"
        self.assertEqual(sc.stamp(text, "0.9.4.13", "0.9.4.12", "2026-10-16"), text)

    def test_empty_unreleased_is_noop(self):
        text = "# Changelog\n\n## [Unreleased]\n\n## [0.9.4.12] - 2026-10-15\n\n### Added\n\n- Old\n"
        self.assertEqual(sc.stamp(text, "0.9.4.13", "0.9.4.12", "2026-10-16"), text)

    def test_stale_stamp_is_restamped(self):
        # The PR stamped 0.9.4.14, but the predicted next version moved on
        # to 0.9.4.15 (0.9.4.14 still unreleased): the old heading is folded.
        once = sc.stamp(BASE, "0.9.4.13", "0.9.4.12", "2026-10-16")
        restamped = sc.stamp(once.replace("0.9.4.13", "0.9.4.14"), "0.9.4.15", "0.9.4.13", "2026-10-17")
        self.assertIn("## [0.9.4.15] - 2026-10-17", restamped)
        self.assertNotIn("0.9.4.14", restamped)

    def test_folds_multiple_pending_sections(self):
        # e.g. a docs-only merge stamped 0.9.4.13 without a release build,
        # then main was merged into this PR (conflict resolved by keeping both).
        text = textwrap.dedent("""\
            ## [Unreleased]

            ### Fixed

            - Newest fix

            ## [0.9.4.13] - 2026-10-16

            ### Added

            - Docs entry

            ## [0.9.4.13] - 2026-10-16

            ### Added

            - Other entry

            ## [0.9.4.12] - 2026-10-15

            ### Added

            - Released
        """)
        out = sc.stamp(text, "0.9.4.13", "0.9.4.12", "2026-10-17")
        self.assertEqual(out.count("## [0.9.4.13]"), 1)
        self.assertIn("## [0.9.4.13] - 2026-10-16", out)
        self.assertEqual(out.count("### Added"), 2)  # merged + released section
        self.assertLess(out.index("Other entry"), out.index("Docs entry"))
        self.assertIn("- Newest fix", out)
        self.assertIn("## [0.9.4.12] - 2026-10-15\n\n### Added\n\n- Released", out)

    def test_released_sections_untouched(self):
        text = "## [Unreleased]\n\n## [0.9.4.12] - 2026-10-15\n\n### Added\n\n- Released\n"
        self.assertEqual(sc.stamp(text, "0.9.4.13", "0.9.4.12", "2026-10-16"), text)

    def test_no_released_version_only_unreleased(self):
        out = sc.stamp(BASE, "0.9.4.1", None, "2026-10-16")
        self.assertIn("## [0.9.4.1] - 2026-10-16", out)
        self.assertIn("## [0.9.4.12] - 2026-10-15", out)

    def test_version_lost_race_is_restamped(self):
        # PRs A and B both predicted 0.9.4.13; A was released first, so B's
        # still-unpublished section now *looks* released.  Comparing against
        # the published changelog keeps B's entries and re-stamps them.
        branch = sc.stamp(BASE, "0.9.4.13", "0.9.4.12", "2026-10-16")
        released = textwrap.dedent("""\
            # Changelog

            ## [Unreleased]

            ## [0.9.4.13] - 2026-10-16

            ### Added

            - A's feature

            ## [0.9.4.12] - 2026-10-15

            ### Added

            - Old feature
        """)
        out = sc.stamp(branch, "0.9.4.14", "0.9.4.13", "2026-10-17",
                       released_changelog=released)
        self.assertIn("## [0.9.4.14] - 2026-10-17", out)
        self.assertNotIn("## [0.9.4.13]", out)
        self.assertIn("- New feature", out)
        self.assertIn("- A fix\n  with continuation", out)
        self.assertNotIn("A's feature", out)

    def test_published_section_still_untouched_with_released_changelog(self):
        # The section really was published: its entries are in the released
        # changelog, so nothing is folded back in.
        text = "## [Unreleased]\n\n## [0.9.4.12] - 2026-10-15\n\n### Added\n\n- Released\n"
        self.assertEqual(
            sc.stamp(text, "0.9.4.13", "0.9.4.12", "2026-10-16", released_changelog=text),
            text,
        )


class TestNotes(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.frag_dir = pathlib.Path(self._tmp.name) / ".changelog"
        self.frag_dir.mkdir()
        self._orig = ac.CHANGELOG_DIR
        ac.CHANGELOG_DIR = self.frag_dir

    def tearDown(self):
        ac.CHANGELOG_DIR = self._orig
        self._tmp.cleanup()

    def test_notes_from_stamped_section(self):
        stamped = sc.stamp(BASE, "0.9.4.13", "0.9.4.12", "2026-10-16")
        out = sc.notes(stamped, "v0.9.4.12")
        self.assertTrue(out.startswith("### Added\n\n- New feature"))
        self.assertIn("- A fix", out)
        self.assertNotIn("Old feature", out)
        self.assertNotIn("## [", out)

    def test_notes_include_leftover_fragments(self):
        (self.frag_dir / "late.fixed.md").write_text("Late fix")
        stamped = sc.stamp(BASE, "0.9.4.13", "0.9.4.12", "2026-10-16")
        out = sc.notes(stamped, "v0.9.4.12")
        self.assertIn("- A fix", out)
        self.assertIn("- Late fix", out)

    def test_notes_empty_when_nothing_pending(self):
        text = "## [Unreleased]\n\n## [0.9.4.12] - 2026-10-15\n\n### Added\n\n- Released\n"
        self.assertEqual(sc.notes(text, "v0.9.4.12"), "")

    def test_notes_keep_entries_whose_version_was_released_by_another_pr(self):
        # Same race as TestStamp.test_version_lost_race_is_restamped: the
        # entries must still reach the release notes.
        text = sc.stamp(BASE, "0.9.4.13", "0.9.4.12", "2026-10-16")
        released = (
            "# Changelog\n\n## [Unreleased]\n\n"
            "## [0.9.4.13] - 2026-10-16\n\n### Added\n\n- A's feature\n\n"
            "## [0.9.4.12] - 2026-10-15\n\n### Added\n\n- Old feature\n"
        )
        out = sc.notes(text, "v0.9.4.13", released_changelog=released)
        self.assertIn("- New feature", out)
        self.assertIn("- A fix", out)
        self.assertNotIn("A's feature", out)
        self.assertNotIn("Old feature", out)


if __name__ == "__main__":
    unittest.main()
