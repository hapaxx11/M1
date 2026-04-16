#!/usr/bin/env python3
"""Assemble changelog fragment files into CHANGELOG.md.

Fragment files live in .changelog/ and follow the naming convention:

    <description>.<category>.md

where <category> is one of: added, changed, fixed, removed

Each file contains the bullet-point text for one changelog entry (without
the leading "- ").  Multiple entries may be separated by blank lines; each
non-blank paragraph becomes a separate bullet.

Examples
--------
    .changelog/ir-universal-remote.added.md
    .changelog/firmware-download-buffer.fixed.md
    .changelog/removed-legacy-menu.removed.md

Usage
-----
    python scripts/assemble_changelog.py            # assemble + delete fragments
    python scripts/assemble_changelog.py --preview   # dry-run: show assembled output
    python scripts/assemble_changelog.py --keep       # assemble but keep fragment files
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

# Paths are relative to repo root (script is invoked from repo root)
CHANGELOG_DIR = pathlib.Path(".changelog")
CHANGELOG_FILE = pathlib.Path("CHANGELOG.md")

CATEGORY_HEADING = {
    "added": "### Added",
    "changed": "### Changed",
    "fixed": "### Fixed",
    "removed": "### Removed",
}

CATEGORY_ORDER = ["added", "changed", "fixed", "removed"]

UNRELEASED_MARKER = "## [Unreleased]"


# ── Helpers ──────────────────────────────────────────────────────────────

def _parse_category(filename: str) -> str | None:
    """Extract category from a fragment filename like 'desc.added.md'."""
    parts = filename.rsplit(".", 2)
    if len(parts) < 3 or parts[-1] != "md":
        return None
    cat = parts[-2].lower()
    return cat if cat in CATEGORY_HEADING else None


def _read_entries(path: pathlib.Path) -> list[str]:
    """Read a fragment file and return a list of bullet entries.

    Each non-empty paragraph (separated by blank lines) becomes one bullet.
    Leading "- " is normalised: added if missing, preserved if present.
    """
    text = path.read_text().strip()
    if not text:
        return []

    entries: list[str] = []
    current_lines: list[str] = []

    for raw_line in text.splitlines():
        line = raw_line.rstrip()
        if line == "":
            if current_lines:
                entries.append("\n".join(current_lines))
                current_lines = []
        else:
            current_lines.append(line)

    if current_lines:
        entries.append("\n".join(current_lines))

    # Normalise: ensure each entry starts with "- "
    normalised: list[str] = []
    for entry in entries:
        first_line, *rest = entry.split("\n", 1)
        if not first_line.startswith("- "):
            first_line = "- " + first_line
        if rest:
            # Indent continuation lines by 2 spaces (standard markdown list)
            cont = rest[0]
            cont_lines = []
            for cl in cont.splitlines():
                if cl and not cl.startswith("  "):
                    cl = "  " + cl
                cont_lines.append(cl)
            first_line = first_line + "\n" + "\n".join(cont_lines)
        normalised.append(first_line)

    return normalised


# ── Core logic ───────────────────────────────────────────────────────────

def collect_fragments() -> tuple[dict[str, list[str]], list[pathlib.Path]]:
    """Read all fragment files and group entries by category."""
    entries: dict[str, list[str]] = {cat: [] for cat in CATEGORY_ORDER}
    fragment_files: list[pathlib.Path] = []

    if not CHANGELOG_DIR.exists():
        return entries, fragment_files

    for f in sorted(CHANGELOG_DIR.iterdir()):
        if f.name.startswith(".") or f.name.lower() == "readme.md":
            continue

        cat = _parse_category(f.name)
        if cat is None:
            print(f"  warning: skipping {f.name} (unrecognised format)", file=sys.stderr)
            continue

        new_entries = _read_entries(f)
        fragment_files.append(f)
        if new_entries:
            entries[cat].extend(new_entries)
        else:
            print(f"  warning: skipping {f.name} (empty fragment)", file=sys.stderr)

    return entries, fragment_files


def format_sections(entries: dict[str, list[str]]) -> str:
    """Format grouped entries into Keep-a-Changelog subsections."""
    parts: list[str] = []
    for cat in CATEGORY_ORDER:
        if not entries[cat]:
            continue
        parts.append(CATEGORY_HEADING[cat])
        parts.append("")
        for entry in entries[cat]:
            parts.append(entry)
        parts.append("")
    return "\n".join(parts)


def _find_unreleased_bounds(text: str) -> tuple[int, int, int]:
    """Return (marker_start, content_start, section_end) for [Unreleased].

    content_start is right after the marker line's newline.
    section_end is the position just before the next '## [' heading (or EOF).
    """
    idx = text.find(UNRELEASED_MARKER)
    if idx == -1:
        raise ValueError("No [Unreleased] section in CHANGELOG.md")

    # End of the marker line
    nl = text.find("\n", idx)
    content_start = (nl + 1) if nl != -1 else len(text)

    # Next version heading
    next_heading = re.search(r"^## \[", text[content_start:], re.MULTILINE)
    section_end = (content_start + next_heading.start()) if next_heading else len(text)

    return idx, content_start, section_end


def _parse_existing_unreleased(text: str) -> dict[str, list[str]]:
    """Parse any existing entries already under [Unreleased]."""
    entries: dict[str, list[str]] = {cat: [] for cat in CATEGORY_ORDER}

    # Reverse map heading → category
    heading_to_cat = {v: k for k, v in CATEGORY_HEADING.items()}

    current_cat: str | None = None
    current_entry_lines: list[str] = []

    def _flush():
        nonlocal current_entry_lines
        if current_cat and current_entry_lines:
            entries[current_cat].append("\n".join(current_entry_lines))
        current_entry_lines = []

    for line in text.splitlines():
        stripped = line.strip()

        # Category heading?
        if stripped in heading_to_cat:
            _flush()
            current_cat = heading_to_cat[stripped]
            continue

        # Blank line between entries
        if stripped == "":
            _flush()
            continue

        # Continuation or new bullet
        if stripped.startswith("- "):
            _flush()
            current_entry_lines.append(line.rstrip())
        elif current_entry_lines:
            # Continuation of a multi-line bullet
            current_entry_lines.append(line.rstrip())

    _flush()
    return entries


def merge_entries(
    existing: dict[str, list[str]],
    new: dict[str, list[str]],
) -> dict[str, list[str]]:
    """Merge new fragment entries into existing unreleased entries.

    New entries are appended after existing ones within each category.
    """
    merged: dict[str, list[str]] = {cat: [] for cat in CATEGORY_ORDER}
    for cat in CATEGORY_ORDER:
        merged[cat] = list(existing.get(cat, []))
        merged[cat].extend(new.get(cat, []))
    return merged


def assemble(*, preview: bool = False, keep: bool = False) -> int:
    """Main entry point: collect fragments and update CHANGELOG.md."""
    entries, fragment_files = collect_fragments()

    if not any(entries.values()):
        print("No changelog fragments found — nothing to assemble.")
        return 0

    total = sum(len(v) for v in entries.values())

    if preview:
        print(f"=== {total} entries from {len(fragment_files)} fragment(s) ===\n")
        print(format_sections(entries))
        return 0

    # Read existing changelog
    if not CHANGELOG_FILE.exists():
        print(f"ERROR: {CHANGELOG_FILE} not found", file=sys.stderr)
        return 1

    text = CHANGELOG_FILE.read_text()

    try:
        _, content_start, section_end = _find_unreleased_bounds(text)
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    # Parse existing entries in the [Unreleased] section
    existing_text = text[content_start:section_end]
    existing_entries = _parse_existing_unreleased(existing_text)

    # Merge
    merged = merge_entries(existing_entries, entries)
    merged_text = format_sections(merged)

    # Rebuild the file: marker line + merged content + rest
    before = text[: content_start]  # includes "## [Unreleased]\n"
    after = text[section_end:]

    if merged_text:
        new_text = before + "\n" + merged_text + after
    else:
        new_text = before + "\n" + after

    CHANGELOG_FILE.write_text(new_text)
    print(f"Assembled {total} new entries from {len(fragment_files)} fragment(s) into CHANGELOG.md")

    if not keep:
        for f in fragment_files:
            f.unlink()
            print(f"  deleted {f.name}")

    return 0


# ── CLI ──────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Assemble changelog fragment files into CHANGELOG.md",
    )
    parser.add_argument(
        "--preview",
        action="store_true",
        help="Show what would be assembled without modifying any files",
    )
    parser.add_argument(
        "--keep",
        action="store_true",
        help="Assemble into CHANGELOG.md but do not delete fragment files",
    )
    args = parser.parse_args()

    raise SystemExit(assemble(preview=args.preview, keep=args.keep))


if __name__ == "__main__":
    main()
