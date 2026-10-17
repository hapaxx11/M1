#!/usr/bin/env python3
"""Stamp the [Unreleased] section of CHANGELOG.md with the next release version.

Used by .github/workflows/prepare-changelog.yml (PR time, before merge to
main) and .github/workflows/build-release.yml (release notes).

"Pending" sections are the contiguous version headings directly below the
[Unreleased] marker that are not yet published (e.g. a section stamped by an
earlier run of the PR workflow, or by a merged docs-only PR that did not
trigger a release build).  Stamping folds the [Unreleased] content and every
pending section into a single heading for the predicted next version, so
re-running is idempotent and a stale prediction is corrected automatically on
the next run.

A section is unpublished when its version is newer than the latest published
release.  That version test alone is not enough when two PRs predict the same
version and one of them is released first: the loser's section then *looks*
released.  To avoid dropping its entries, --released-changelog may point at
CHANGELOG.md as it exists in the latest published release; entries that are
absent from it are still treated as pending even when the heading version is
not newer than the released one.

Usage
-----
    python scripts/stamp_changelog.py stamp --version 0.9.4.13 \\
        --released 0.9.4.12 [--date YYYY-MM-DD] \\
        [--released-changelog released/CHANGELOG.md]
    python scripts/stamp_changelog.py notes --released 0.9.4.12 \\
        [--released-changelog released/CHANGELOG.md]
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


def _entry_key(entry: str) -> str:
    """Normalise a bullet entry so published/pending comparisons are stable."""
    return " ".join(entry.split())


def published_entries(released_text: str | None) -> set[str] | None:
    """Return the entries already published, from a released CHANGELOG.md.

    `released_text` is CHANGELOG.md as of the latest published release.  Its
    [Unreleased] section is ignored — that content was not part of the
    release.  Returns None when no released changelog is available.
    """
    if not released_text:
        return None
    keys: set[str] = set()
    for m in HEADING_RE.finditer(released_text):
        if parse_version(m.group(1)) is None:
            continue  # [Unreleased] and other non-version headings
        nxt = HEADING_RE.search(released_text, m.end())
        body = released_text[m.end():nxt.start() if nxt else len(released_text)]
        for entries in ac._parse_existing_unreleased(body).values():
            keys.update(_entry_key(e) for e in entries)
    return keys


def _unpublished(entries: dict[str, list[str]], published: set[str]) -> dict[str, list[str]]:
    """Drop entries that already appear in the released changelog."""
    return {
        cat: [e for e in entries.get(cat, []) if _entry_key(e) not in published]
        for cat in ac.CATEGORY_ORDER
    }


def _split(text: str, released: tuple[int, ...] | None, published: set[str] | None = None):
    """Locate [Unreleased] and pending sections.

    Returns (content_start, end, unreleased_body, pending) where text[:content_start]
    ends with the [Unreleased] marker line, text[end:] is the untouched rest
    of the file, and pending is a list of (version, date, entries) in file order.
    Returns None when the [Unreleased] marker is absent.
    """
    try:
        _, content_start, section_end = ac._find_unreleased_bounds(text)
    except ValueError:
        return None

    unreleased_body = text[content_start:section_end]
    pending: list[tuple[str, str | None, dict[str, list[str]]]] = []
    pos = section_end
    while released is not None:
        m = HEADING_RE.match(text, pos)
        if not m:
            break
        ver = parse_version(m.group(1))
        if ver is None:
            break
        body_start = m.end() + 1 if m.end() < len(text) else m.end()
        nxt = re.search(r"^## \[", text[body_start:], re.MULTILINE)
        body_end = body_start + nxt.start() if nxt else len(text)
        entries = ac._parse_existing_unreleased(text[body_start:body_end])
        if ver <= released:
            # The heading claims a released version, but another PR may have
            # published it first — keep whatever this section contributes that
            # the published changelog does not already contain.
            if published is None:
                break
            entries = _unpublished(entries, published)
            if not any(entries.values()):
                break
        pending.append((m.group(1), m.group(2), entries))
        pos = body_end

    return content_start, pos, unreleased_body, pending


def _merged_entries(unreleased_body: str, pending) -> dict[str, list[str]]:
    """Merge pending sections (oldest first) and then [Unreleased] entries."""
    merged: dict[str, list[str]] = {cat: [] for cat in ac.CATEGORY_ORDER}
    for _, _, entries in reversed(pending):
        merged = ac.merge_entries(merged, entries)
    return ac.merge_entries(merged, ac._parse_existing_unreleased(unreleased_body))


def stamp(
    text: str,
    version: str,
    released: str | None,
    date: str,
    released_changelog: str | None = None,
) -> str:
    """Return CHANGELOG text with pending entries stamped as `version`.

    No-op when [Unreleased] is absent or there is nothing pending.  If the
    newest pending section already carries `version`, its date is preserved
    so repeated runs do not churn.
    """
    split = _split(text, parse_version(released), published_entries(released_changelog))
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


def notes(text: str, released: str | None, released_changelog: str | None = None) -> str:
    """Return the "What's Changed" markdown for the release being built.

    Combines pending (not yet released) changelog sections, any [Unreleased]
    entries, and any fragment files still present in .changelog/.
    """
    merged: dict[str, list[str]] = {cat: [] for cat in ac.CATEGORY_ORDER}
    split = _split(text, parse_version(released), published_entries(released_changelog))
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
    p_stamp.add_argument("--released-changelog", default="",
                         help="CHANGELOG.md as published in the latest release")

    p_notes = sub.add_parser("notes", help="Print release notes for pending entries")
    p_notes.add_argument("--released", default="", help="Latest published release version")
    p_notes.add_argument("--released-changelog", default="",
                         help="CHANGELOG.md as published in the latest release")

    args = parser.parse_args()
    released_changelog = None
    if args.released_changelog:
        path = pathlib.Path(args.released_changelog)
        if path.exists():
            released_changelog = path.read_text()
        else:
            print(f"WARNING: {path} not found — falling back to version comparison.",
                  file=sys.stderr)
    changelog: pathlib.Path = ac.CHANGELOG_FILE

    if args.cmd == "stamp":
        if parse_version(args.version) is None:
            print(f"ERROR: invalid version '{args.version}'", file=sys.stderr)
            return 1
        if not changelog.exists():
            print(f"{changelog} not found — nothing to stamp.")
            return 0
        text = changelog.read_text()
        new_text = stamp(text, args.version, args.released or None, args.date,
                         released_changelog)
        if new_text == text:
            print("Nothing to stamp.")
        else:
            changelog.write_text(new_text)
            print(f"Stamped CHANGELOG.md as [{args.version}]")
        return 0

    text = changelog.read_text() if changelog.exists() else ""
    print(notes(text, args.released or None, released_changelog))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
