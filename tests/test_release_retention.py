#!/usr/bin/env python3
"""Tests for scripts/release_retention.py — compute_deletions() helper."""

import os
import sys
import unittest

# Make scripts/ importable without installing.
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "scripts"))
import release_retention as rr


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _r(tag: str, ts: str = "2026-01-01T00:00:00Z") -> rr.Release:
    """Construct a Release from a version tag and an optional ISO-8601 timestamp."""
    parsed = rr.parse_tag(tag)
    if parsed is None:
        raise ValueError(f"Bad tag for test fixture: {tag!r}")
    major, minor, build, rc = parsed
    return rr.Release(tag=tag, major=major, minor=minor, build=build, rc=rc,
                      created_at=ts)


def _releases(*specs: tuple) -> list:
    """Build a Release list from (tag, timestamp) pairs, sorted most-recent first."""
    releases = [_r(tag, ts) for tag, ts in specs]
    releases.sort(key=lambda r: r.created_at, reverse=True)
    return releases


# ---------------------------------------------------------------------------
# parse_tag
# ---------------------------------------------------------------------------

class TestParseTag(unittest.TestCase):

    def test_valid_tags(self):
        self.assertEqual(rr.parse_tag("v0.9.0.12"), (0, 9, 0, 12))
        self.assertEqual(rr.parse_tag("v1.0.0.1"),  (1, 0, 0, 1))
        self.assertEqual(rr.parse_tag("v10.2.3.4"), (10, 2, 3, 4))

    def test_invalid_tags(self):
        for bad in ("1.2.3.4", "v1.2.3", "vX.Y.Z.W", "latest", "", "v0.9.0"):
            with self.subTest(tag=bad):
                self.assertIsNone(rr.parse_tag(bad))


# ---------------------------------------------------------------------------
# compute_deletions — Rule 1 (same major.minor.build, keep 10)
# ---------------------------------------------------------------------------

class TestRule1(unittest.TestCase):

    def _same_mmb(self, count):
        specs = [(f"v0.9.0.{i}", f"2026-01-{i:02d}T00:00:00Z") for i in range(1, count + 1)]
        return _releases(*specs)

    def test_under_limit_no_deletions(self):
        rels = self._same_mmb(5)
        self.assertEqual(rr.compute_deletions("v0.9.0.5", rels), [])

    def test_exactly_at_limit_no_deletions(self):
        rels = self._same_mmb(10)
        self.assertEqual(rr.compute_deletions("v0.9.0.10", rels), [])

    def test_over_limit_deletes_oldest(self):
        rels = self._same_mmb(12)
        deletions = rr.compute_deletions("v0.9.0.12", rels)
        self.assertEqual(len(deletions), 2)
        self.assertIn("v0.9.0.1", deletions)
        self.assertIn("v0.9.0.2", deletions)
        self.assertNotIn("v0.9.0.12", deletions)
        self.assertNotIn("v0.9.0.3", deletions)

    def test_rule1_wins_over_rule2_territory(self):
        """Rule 1 fires even when older same-minor releases exist (different build)."""
        specs = [
            (f"v0.9.0.{i}", f"2026-01-{i:02d}T00:00:00Z") for i in range(1, 12)
        ] + [("v0.9.1.1", "2025-12-01T00:00:00Z")]
        rels = _releases(*specs)
        deletions = rr.compute_deletions("v0.9.0.11", rels)
        # 11 same-mmb entries → keep 10, delete 1 oldest
        self.assertEqual(len(deletions), 1)
        self.assertIn("v0.9.0.1", deletions)
        # v0.9.1.1 belongs to a different (minor, build) and must not be touched
        self.assertNotIn("v0.9.1.1", deletions)


# ---------------------------------------------------------------------------
# compute_deletions — Rule 2 (same major.minor, different build, keep 5)
# ---------------------------------------------------------------------------

class TestRule2(unittest.TestCase):

    def test_single_release_no_deletions(self):
        rels = [_r("v0.9.0.1", "2026-01-01T00:00:00Z")]
        self.assertEqual(rr.compute_deletions("v0.9.0.1", rels), [])

    def test_different_builds_triggers_rule2(self):
        specs = [
            ("v0.9.1.1", "2026-01-06T00:00:00Z"),
            ("v0.9.0.5", "2026-01-05T00:00:00Z"),
            ("v0.9.0.4", "2026-01-04T00:00:00Z"),
            ("v0.9.0.3", "2026-01-03T00:00:00Z"),
            ("v0.9.0.2", "2026-01-02T00:00:00Z"),
            ("v0.9.0.1", "2026-01-01T00:00:00Z"),
        ]
        rels = _releases(*specs)
        deletions = rr.compute_deletions("v0.9.1.1", rels)
        self.assertEqual(len(deletions), 1)
        self.assertIn("v0.9.0.1", deletions)
        self.assertNotIn("v0.9.1.1", deletions)

    def test_exactly_at_limit_no_deletions(self):
        specs = [
            ("v0.9.4.1", "2026-01-05T00:00:00Z"),
            ("v0.9.3.1", "2026-01-04T00:00:00Z"),
            ("v0.9.2.1", "2026-01-03T00:00:00Z"),
            ("v0.9.1.1", "2026-01-02T00:00:00Z"),
            ("v0.9.0.1", "2026-01-01T00:00:00Z"),
        ]
        rels = _releases(*specs)
        self.assertEqual(rr.compute_deletions("v0.9.4.1", rels), [])


# ---------------------------------------------------------------------------
# compute_deletions — Rule 3 (same major, different minor, keep 3)
# ---------------------------------------------------------------------------

class TestRule3(unittest.TestCase):

    def test_new_minor_triggers_rule3(self):
        specs = [
            ("v0.12.0.1", "2026-04-01T00:00:00Z"),
            ("v0.11.0.1", "2026-03-01T00:00:00Z"),
            ("v0.10.0.1", "2026-02-01T00:00:00Z"),
            ("v0.9.0.1",  "2026-01-01T00:00:00Z"),
        ]
        rels = _releases(*specs)
        deletions = rr.compute_deletions("v0.12.0.1", rels)
        self.assertEqual(len(deletions), 1)
        self.assertIn("v0.9.0.1", deletions)
        self.assertNotIn("v0.12.0.1", deletions)

    def test_exactly_three_no_deletions(self):
        specs = [
            ("v0.11.0.1", "2026-03-01T00:00:00Z"),
            ("v0.10.0.1", "2026-02-01T00:00:00Z"),
            ("v0.9.0.1",  "2026-01-01T00:00:00Z"),
        ]
        rels = _releases(*specs)
        self.assertEqual(rr.compute_deletions("v0.11.0.1", rels), [])


# ---------------------------------------------------------------------------
# compute_deletions — Rule 4 (new major, prune prev major by minor.build)
# ---------------------------------------------------------------------------

class TestRule4(unittest.TestCase):

    def test_basic_one_per_minor_build(self):
        """Each distinct (minor, build) in the previous major keeps its most-recent RC."""
        specs = [
            # New major
            ("v1.0.0.1", "2026-05-01T00:00:00Z"),
            # Previous major — two (minor, build) groups
            ("v0.9.0.3", "2026-04-03T00:00:00Z"),  # most recent (9,0) → keep
            ("v0.9.0.2", "2026-04-02T00:00:00Z"),  # older     (9,0) → delete
            ("v0.9.0.1", "2026-04-01T00:00:00Z"),  # oldest    (9,0) → delete
            ("v0.8.0.2", "2026-03-02T00:00:00Z"),  # most recent (8,0) → keep
            ("v0.8.0.1", "2026-03-01T00:00:00Z"),  # older     (8,0) → delete
        ]
        rels = _releases(*specs)
        deletions = rr.compute_deletions("v1.0.0.1", rels)
        self.assertIn("v0.9.0.2", deletions)
        self.assertIn("v0.9.0.1", deletions)
        self.assertIn("v0.8.0.1", deletions)
        self.assertNotIn("v0.9.0.3", deletions)
        self.assertNotIn("v0.8.0.2", deletions)
        self.assertNotIn("v1.0.0.1", deletions)
        self.assertEqual(len(deletions), 3)

    def test_groups_by_minor_AND_build_not_just_minor(self):
        """(9,0) and (9,1) are separate groups; both survivors are kept."""
        specs = [
            ("v1.0.0.1", "2026-05-01T00:00:00Z"),
            # minor=9, build=1 group
            ("v0.9.1.3", "2026-04-04T00:00:00Z"),  # most recent (9,1) → keep
            ("v0.9.1.2", "2026-04-03T00:00:00Z"),  # older       (9,1) → delete
            ("v0.9.1.1", "2026-04-02T00:00:00Z"),  # oldest      (9,1) → delete
            # minor=9, build=0 group
            ("v0.9.0.5", "2026-04-01T00:00:00Z"),  # most recent (9,0) → keep
            ("v0.9.0.4", "2026-03-31T00:00:00Z"),  # older       (9,0) → delete
        ]
        rels = _releases(*specs)
        deletions = rr.compute_deletions("v1.0.0.1", rels)
        # (9,1): keep v0.9.1.3, delete v0.9.1.2 and v0.9.1.1
        self.assertNotIn("v0.9.1.3", deletions)
        self.assertIn("v0.9.1.2", deletions)
        self.assertIn("v0.9.1.1", deletions)
        # (9,0): keep v0.9.0.5, delete v0.9.0.4
        self.assertNotIn("v0.9.0.5", deletions)
        self.assertIn("v0.9.0.4", deletions)
        self.assertEqual(len(deletions), 3)

    def test_single_release_per_minor_build_no_deletions(self):
        specs = [
            ("v1.0.0.1", "2026-05-01T00:00:00Z"),
            ("v0.9.0.1", "2026-04-01T00:00:00Z"),
            ("v0.8.0.1", "2026-03-01T00:00:00Z"),
        ]
        rels = _releases(*specs)
        self.assertEqual(rr.compute_deletions("v1.0.0.1", rels), [])

    def test_no_prev_major_releases_no_deletions(self):
        specs = [("v1.0.0.1", "2026-05-01T00:00:00Z")]
        rels = _releases(*specs)
        self.assertEqual(rr.compute_deletions("v1.0.0.1", rels), [])

    def test_major_zero_no_prev_major_exists(self):
        """new_major == 0 → prev_major would be -1 → no deletions."""
        rels = [_r("v0.9.0.1", "2026-01-01T00:00:00Z")]
        self.assertEqual(rr.compute_deletions("v0.9.0.1", rels), [])

    def test_rule4_only_touches_prev_major(self):
        """Releases with major < prev_major are left alone."""
        specs = [
            ("v2.0.0.1", "2026-05-01T00:00:00Z"),
            # prev_major == 1
            ("v1.9.0.3", "2026-04-03T00:00:00Z"),  # most recent (9,0) → keep
            ("v1.9.0.2", "2026-04-02T00:00:00Z"),  # older       (9,0) → delete
            # major == 0 — untouched by Rule 4 (only major-1 is pruned)
            ("v0.9.0.1", "2026-03-01T00:00:00Z"),
            ("v0.8.0.1", "2026-02-01T00:00:00Z"),
        ]
        rels = _releases(*specs)
        deletions = rr.compute_deletions("v2.0.0.1", rels)
        self.assertIn("v1.9.0.2", deletions)
        self.assertNotIn("v1.9.0.3", deletions)
        self.assertNotIn("v0.9.0.1", deletions)
        self.assertNotIn("v0.8.0.1", deletions)
        self.assertEqual(len(deletions), 1)


# ---------------------------------------------------------------------------
# compute_deletions — malformed / edge cases
# ---------------------------------------------------------------------------

class TestEdgeCases(unittest.TestCase):

    def test_malformed_new_tag_no_deletions(self):
        rels = [_r("v0.9.0.1")]
        for bad in ("not-a-version", "v1.2.3", "", "latest"):
            with self.subTest(tag=bad):
                self.assertEqual(rr.compute_deletions(bad, rels), [])

    def test_empty_release_list(self):
        self.assertEqual(rr.compute_deletions("v0.9.0.1", []), [])


# ---------------------------------------------------------------------------
# compute_deletions — rule hierarchy (first matching rule wins)
# ---------------------------------------------------------------------------

class TestRuleHierarchy(unittest.TestCase):

    def test_rule1_beats_rule2(self):
        """11 releases in same mmb → Rule 1 fires; Rule 2 not evaluated."""
        specs = [(f"v0.9.0.{i}", f"2026-01-{i:02d}T00:00:00Z") for i in range(1, 12)]
        rels = _releases(*specs)
        deletions = rr.compute_deletions("v0.9.0.11", rels)
        # Rule 1: keep 10, delete 1
        self.assertEqual(len(deletions), 1)
        self.assertIn("v0.9.0.1", deletions)

    def test_rule2_beats_rule3(self):
        """Different builds in same minor → Rule 2 fires; Rule 3 not evaluated."""
        specs = [
            ("v0.9.2.1", "2026-01-06T00:00:00Z"),
            ("v0.9.1.1", "2026-01-05T00:00:00Z"),
            ("v0.9.0.4", "2026-01-04T00:00:00Z"),
            ("v0.9.0.3", "2026-01-03T00:00:00Z"),
            ("v0.9.0.2", "2026-01-02T00:00:00Z"),
            ("v0.9.0.1", "2026-01-01T00:00:00Z"),
        ]
        rels = _releases(*specs)
        # same_mmb=[v0.9.2.1] (1) → Rule 1 skip
        # same_mm=all 6 (minor=9) → Rule 2 fires, keep 5, delete 1
        deletions = rr.compute_deletions("v0.9.2.1", rels)
        self.assertEqual(len(deletions), 1)
        self.assertIn("v0.9.0.1", deletions)

    def test_rule3_beats_rule4(self):
        """New minor within same major → Rule 3 fires; Rule 4 not evaluated."""
        specs = [
            ("v0.10.0.1", "2026-03-01T00:00:00Z"),
            ("v0.9.0.1",  "2026-02-01T00:00:00Z"),
            ("v0.8.0.1",  "2026-01-01T00:00:00Z"),
        ]
        rels = _releases(*specs)
        # same_mmb=[v0.10.0.1] → Rule 1 skip
        # same_mm=[v0.10.0.1]  → Rule 2 skip
        # same_m=3              → Rule 3 fires (keep 3 → no deletions)
        deletions = rr.compute_deletions("v0.10.0.1", rels)
        self.assertEqual(deletions, [])

    def test_rule4_fires_when_rules_123_skip(self):
        """Completely new major with no same-major releases → Rule 4."""
        specs = [
            ("v1.0.0.1", "2026-05-01T00:00:00Z"),
            ("v0.9.0.2", "2026-04-02T00:00:00Z"),
            ("v0.9.0.1", "2026-04-01T00:00:00Z"),
        ]
        rels = _releases(*specs)
        deletions = rr.compute_deletions("v1.0.0.1", rels)
        # Rule 4: prev_major=0, one (minor, build) group → keep most recent, delete 1
        self.assertEqual(len(deletions), 1)
        self.assertIn("v0.9.0.1", deletions)
        self.assertNotIn("v0.9.0.2", deletions)


if __name__ == "__main__":
    unittest.main()
