#!/usr/bin/env python3
"""
Release retention policy for M1 Hapax firmware.

Rules are applied hierarchically — the first matching rule wins:

  Rule 1 — same major.minor.build:
      Keep the 10 most recent releases that share major, minor, and build.
      All other releases (different major, minor, or build) are untouched.

  Rule 2 — same major.minor, different build (Rule 1 did not apply):
      Keep the 5 most recent releases that share major and minor.
      All other releases (different major or minor) are untouched.

  Rule 3 — same major, different minor (Rules 1–2 did not apply):
      Keep the 3 most recent releases that share major.
      All other releases (different major) are untouched.

  Rule 4 — new major (no existing release shares major, Rules 1–3 did not apply):
      For the immediately preceding major (current − 1), keep only the
      most recent release for each distinct minor version.  All other
      majors are untouched.

Tags that do not match the expected vMAJOR.MINOR.BUILD.RC pattern are
silently ignored and never deleted.

Usage
-----
  python3 scripts/release_retention.py \\
      --repo owner/repo \\
      --tag v0.9.0.12 \\
      [--dry-run]

  --dry-run  Print the actions that would be taken without deleting anything.
"""

import argparse
import json
import re
import subprocess
import sys
from typing import Optional

VERSION_RE = re.compile(r"^v(\d+)\.(\d+)\.(\d+)\.(\d+)$")


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

class Release:
    """A single GitHub release whose tag matches vMAJOR.MINOR.BUILD.RC."""

    __slots__ = ("tag", "major", "minor", "build", "rc", "created_at")

    def __init__(self, tag: str, major: int, minor: int, build: int, rc: int,
                 created_at: str) -> None:
        self.tag = tag
        self.major = major
        self.minor = minor
        self.build = build
        self.rc = rc
        self.created_at = created_at  # ISO-8601 string — used for sort only

    def __repr__(self) -> str:  # pragma: no cover
        return f"Release({self.tag!r})"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def parse_tag(tag: str) -> Optional[tuple]:
    """Return (major, minor, build, rc) ints or None if the tag doesn't match."""
    m = VERSION_RE.match(tag)
    if not m:
        return None
    return int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4))


def fetch_all_releases(repo: str) -> list:
    """
    Return all versioned GitHub releases, sorted most-recent first.

    Uses ``gh release list`` with a generous limit; tags that do not match
    vMAJOR.MINOR.BUILD.RC are excluded.
    """
    result = subprocess.run(
        [
            "gh", "release", "list",
            "--repo", repo,
            "--limit", "500",
            "--json", "tagName,createdAt",
        ],
        capture_output=True,
        text=True,
        check=True,
    )
    raw = json.loads(result.stdout)

    releases = []
    for entry in raw:
        tag = entry["tagName"]
        parsed = parse_tag(tag)
        if parsed is None:
            continue
        major, minor, build, rc = parsed
        releases.append(
            Release(
                tag=tag,
                major=major,
                minor=minor,
                build=build,
                rc=rc,
                created_at=entry["createdAt"],
            )
        )

    # Most-recent first (ISO-8601 strings sort lexicographically correctly).
    releases.sort(key=lambda r: r.created_at, reverse=True)
    return releases


def delete_release(repo: str, tag: str, dry_run: bool) -> None:
    """Delete a GitHub release and its associated tag."""
    if dry_run:
        print(f"  [dry-run] DELETE {tag}")
        return
    print(f"  DELETE {tag}")
    subprocess.run(
        [
            "gh", "release", "delete", tag,
            "--repo", repo,
            "--yes",
            "--cleanup-tag",
        ],
        check=True,
    )


# ---------------------------------------------------------------------------
# Core retention logic
# ---------------------------------------------------------------------------

def apply_retention(repo: str, new_tag: str, dry_run: bool = False) -> None:
    """
    Apply the four-tier retention policy for *new_tag* in *repo*.

    The newly created release must already be published before this function
    is called so it appears in the ``gh release list`` output.
    """
    parsed = parse_tag(new_tag)
    if parsed is None:
        print(
            f"Tag {new_tag!r} does not match vMAJOR.MINOR.BUILD.RC — "
            "retention policy skipped."
        )
        return

    new_major, new_minor, new_build, _new_rc = parsed
    all_releases = fetch_all_releases(repo)
    print(f"Versioned releases found: {len(all_releases)}")

    # -----------------------------------------------------------------------
    # Rule 1 — same major.minor.build
    # -----------------------------------------------------------------------
    same_mmb = [
        r for r in all_releases
        if r.major == new_major and r.minor == new_minor and r.build == new_build
    ]
    # > 1 because the new release itself is already in the list.
    if len(same_mmb) > 1:
        print(
            f"Rule 1: same major.minor.build ({new_major}.{new_minor}.{new_build}) — "
            f"{len(same_mmb)} releases found, keeping 10."
        )
        for r in same_mmb[:10]:
            print(f"  keep  {r.tag}")
        for r in same_mmb[10:]:
            delete_release(repo, r.tag, dry_run)
        return

    # -----------------------------------------------------------------------
    # Rule 2 — same major.minor, different build
    # -----------------------------------------------------------------------
    same_mm = [
        r for r in all_releases
        if r.major == new_major and r.minor == new_minor
    ]
    # same_mmb has exactly 1 entry (the new release) if we reach here, so
    # same_mm > 1 means there are older releases with the same major.minor
    # but a different build number.
    if len(same_mm) > 1:
        print(
            f"Rule 2: same major.minor ({new_major}.{new_minor}), different build — "
            f"{len(same_mm)} releases found, keeping 5."
        )
        for r in same_mm[:5]:
            print(f"  keep  {r.tag}")
        for r in same_mm[5:]:
            delete_release(repo, r.tag, dry_run)
        return

    # -----------------------------------------------------------------------
    # Rule 3 — same major, different minor
    # -----------------------------------------------------------------------
    same_m = [r for r in all_releases if r.major == new_major]
    # same_mm has exactly 1 entry (the new release) if we reach here.
    if len(same_m) > 1:
        print(
            f"Rule 3: same major ({new_major}), different minor — "
            f"{len(same_m)} releases found, keeping 3."
        )
        for r in same_m[:3]:
            print(f"  keep  {r.tag}")
        for r in same_m[3:]:
            delete_release(repo, r.tag, dry_run)
        return

    # -----------------------------------------------------------------------
    # Rule 4 — new major: prune previous major to one release per minor
    # -----------------------------------------------------------------------
    prev_major = new_major - 1
    if prev_major < 0:
        print("No previous major to prune (new_major is 0).")
        return

    prev_releases = [r for r in all_releases if r.major == prev_major]
    if not prev_releases:
        print(
            f"Rule 4: new major {new_major}, but no releases found for "
            f"major {prev_major} — nothing to prune."
        )
        return

    print(
        f"Rule 4: new major {new_major} — pruning major {prev_major} to "
        f"most-recent release per minor ({len(prev_releases)} releases found)."
    )
    seen_minors: set = set()
    for r in prev_releases:  # already sorted most-recent first
        if r.minor not in seen_minors:
            seen_minors.add(r.minor)
            print(f"  keep  {r.tag}  (most recent for minor {r.minor})")
        else:
            delete_release(repo, r.tag, dry_run)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> int:
    parser = argparse.ArgumentParser(
        description="Apply release retention policy after a new release is published."
    )
    parser.add_argument(
        "--repo",
        required=True,
        metavar="OWNER/REPO",
        help="GitHub repository (e.g. hapaxx11/M1)",
    )
    parser.add_argument(
        "--tag",
        required=True,
        metavar="TAG",
        help="Newly created release tag (e.g. v0.9.0.12)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print what would be deleted without actually deleting anything.",
    )
    args = parser.parse_args()

    apply_retention(repo=args.repo, new_tag=args.tag, dry_run=args.dry_run)
    return 0


if __name__ == "__main__":
    sys.exit(main())
