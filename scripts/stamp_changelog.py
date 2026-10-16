#!/usr/bin/env python3
"""Stamp the [Unreleased] section of CHANGELOG.md with the next release version.

Used by .github/workflows/prepare-changelog.yml (PR time, before merge to
main) and .github/workflows/build-release.yml (release notes).

"Pending" sections are the contiguous version headings directly below the
[Unreleased] marker whose version is newer than the latest published release
(e.g. a section stamped by an earlier run of the PR workflow, or by a merged
docs-only PR that did not trigger a release build).  Stamping folds the
[Unreleased] content and every pending section into a single heading for the
predicted next version, so re-running is idempotent and a stale prediction is
corrected automatically on the next run.

Usage
-----
    python scripts/stamp_changelog.py stamp --version 0.9.4.13 \\
        --released 0.9.4.12 [--date YYYY-MM-DD]
    python scripts/stamp_changelog.py notes --released 0.9.4.12
"""

from __future__ import annotations

import argparse
import datetime
import pathlib
import re
import sys

import assemble_changelog as ac

HEADING_RE = re.compile(r"^## \[([^\]]+)\](?:[ \t]+-[ \t]+(\S+))?[ \t]*$", re.MULTILINE)


def parse_version(value: str | None) -> tuple[int, ...] | None:
    """Parse '0.9.4.12' or 'v0.9.4.12' into a tuple of ints (None if invalid)."""
    if not value:
        return None
    value = value.strip()
    if value.startswith("v"):
        value = value[1:]
    parts = value.split(".")
    if not parts or not all(p.isdigit() for p in parts):
        return None
    return tuple(int(p) for p in parts)


def _split(text: str, released: tuple[int, ...] | None):
    """Locate [Unreleased] and pending sections.

    Returns (content_start, end, unreleased_body, pending) where text[:content_start]
    ends with the [Unreleased] marker line, text[end:] is the untouched rest
    of the file, and pending is a list of (version, date, body) in file order.
    Returns None when the [Unreleased] marker is absent.
    """
    try:
        _, content_start, section_end = ac._find_unreleased_bounds(text)
    except ValueError:
        return None

    unreleased_body = text[content_start:section_end]
    pending: list[tuple[str, str | None, str]] = []
    pos = section_end
    while released is not None:
        m = HEADING_RE.match(text, pos)
        if not m:
            break
        ver = parse_version(m.group(1))
        if ver is None or ver <= released:
            break
        body_start = m.end() + 1 if m.end() < len(text) else m.end()
        nxt = re.search(r"^## \[", text[body_start:], re.MULTILINE)
        body_end = body_start + nxt.start() if nxt else len(text)
        pending.append((m.group(1), m.group(2), text[body_start:body_end]))
        pos = body_end

    return content_start, pos, unreleased_body, pending


def _merged_entries(unreleased_body: str, pending) -> dict[str, list[str]]:
    """Merge pending sections (oldest first) and then [Unreleased] entries."""
    merged: dict[str, list[str]] = {cat: [] for cat in ac.CATEGORY_ORDER}
    for _, _, body in reversed(pending):
        merged = ac.merge_entries(merged, ac._parse_existing_unreleased(body))
    return ac.merge_entries(merged, ac._parse_existing_unreleased(unreleased_body))


def stamp(text: str, version: str, released: str | None, date: str) -> str:
    """Return CHANGELOG text with pending entries stamped as `version`.

    No-op when [Unreleased] is absent or there is nothing pending.  If the
    newest pending section already carries `version`, its date is preserved
    so repeated runs do not churn.
    """
    split = _split(text, parse_version(released))
    if split is None:
        return text
    content_start, end, unreleased_body, pending = split

    merged = _merged_entries(unreleased_body, pending)
    if not any(merged.values()):
        return text

    if pending and pending[0][0] == version and pending[0][1]:
        date = pending[0][1]

    body = ac.format_sections(merged)
    stamped = f"\n## [{version}] - {date}\n\n{body}"
    rest = text[end:]
    if rest:
        stamped += "\n"
    return text[:content_start] + stamped + rest


def notes(text: str, released: str | None) -> str:
    """Return the "What's Changed" markdown for the release being built.

    Combines pending (not yet released) changelog sections, any [Unreleased]
    entries, and any fragment files still present in .changelog/.
    """
    merged: dict[str, list[str]] = {cat: [] for cat in ac.CATEGORY_ORDER}
    split = _split(text, parse_version(released))
    if split is not None:
        _, _, unreleased_body, pending = split
        merged = _merged_entries(unreleased_body, pending)
    fragments, _ = ac.collect_fragments()
    merged = ac.merge_entries(merged, fragments)
    return ac.format_sections(merged).strip()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="cmd", required=True)

    p_stamp = sub.add_parser("stamp", help="Stamp pending entries in CHANGELOG.md")
    p_stamp.add_argument("--version", required=True, help="Version to stamp, e.g. 0.9.4.13")
    p_stamp.add_argument("--released", default="", help="Latest published release version")
    p_stamp.add_argument("--date", default=datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d"))

    p_notes = sub.add_parser("notes", help="Print release notes for pending entries")
    p_notes.add_argument("--released", default="", help="Latest published release version")

    args = parser.parse_args()
    changelog: pathlib.Path = ac.CHANGELOG_FILE

    if args.cmd == "stamp":
        if parse_version(args.version) is None:
            print(f"ERROR: invalid version '{args.version}'", file=sys.stderr)
            return 1
        if not changelog.exists():
            print(f"{changelog} not found — nothing to stamp.")
            return 0
        text = changelog.read_text()
        new_text = stamp(text, args.version, args.released or None, args.date)
        if new_text == text:
            print("Nothing to stamp.")
        else:
            changelog.write_text(new_text)
            print(f"Stamped CHANGELOG.md as [{args.version}]")
        return 0

    text = changelog.read_text() if changelog.exists() else ""
    print(notes(text, args.released or None))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
