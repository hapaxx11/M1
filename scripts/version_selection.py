#!/usr/bin/env python3
"""Select consistent firmware versions from the header and published releases."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

MAX_COMPONENT = 255
VERSION_RE = re.compile(r"v?(\d+)\.(\d+)\.(\d+)\.(\d+)")
VERSION_MACROS = (
    "FW_VERSION_MAJOR",
    "FW_VERSION_MINOR",
    "FW_VERSION_BUILD",
    "FW_VERSION_RC",
)


def parse_version(value: str | None) -> tuple[int, int, int, int] | None:
    if not value:
        return None
    match = VERSION_RE.fullmatch(value.strip())
    if not match:
        return None
    version = tuple(int(component) for component in match.groups())
    if any(component > MAX_COMPONENT for component in version):
        return None
    return version


def read_header_version(path: pathlib.Path) -> tuple[int, int, int, int]:
    contents = path.read_text()
    values = []
    for macro in VERSION_MACROS:
        match = re.search(
            rf"^#define\s+{macro}\s+(\d+)\s*$",
            contents,
            re.MULTILINE,
        )
        if not match:
            raise ValueError(f"Could not read {macro} from {path}")
        value = int(match.group(1))
        if value > MAX_COMPONENT:
            raise ValueError(f"{macro} must be between 0 and {MAX_COMPONENT}")
        values.append(value)
    return tuple(values)


def release_versions(tags: list[str] | list[dict[str, str]]) -> list[tuple[int, int, int, int]]:
    versions = []
    for tag in tags:
        if isinstance(tag, dict):
            tag = tag.get("tagName", "")
        version = parse_version(tag)
        if version is not None:
            versions.append(version)
    return versions


def next_release_version(
    current: tuple[int, int, int, int],
    tags: list[str] | list[dict[str, str]],
) -> tuple[tuple[int, int, int, int] | None, tuple[int, int, int, int]]:
    versions = release_versions(tags)
    latest = max(versions, default=None)
    matching = [version for version in versions if version[:3] == current[:3]]

    if latest is not None and current[:3] != latest[:3] and current > latest:
        next_version = current
    else:
        latest_prefix = max(matching, default=None)
        next_published_rc = latest_prefix[3] + 1 if latest_prefix is not None else 1
        next_rc = max(current[3], next_published_rc)
        next_version = (*current[:3], next_rc)

    if next_version[3] > MAX_COMPONENT:
        raise ValueError(
            "The revision component has reached 255; bump the build, minor, or major component."
        )
    return latest, next_version


def validate_bump(
    current: tuple[int, int, int, int],
    tags: list[str] | list[dict[str, str]],
    requested: str,
) -> tuple[int, int, int, int]:
    requested_version = parse_version(requested)
    if requested_version is None:
        raise ValueError(
            f"Invalid version '{requested}'. Use four numeric components between 0 and 255, "
            "such as v0.9.5.0."
        )

    baseline = max([current, *release_versions(tags)])
    major, minor, build, revision = baseline
    candidates = (
        (major + 1, 0, 0, 0),
        (major, minor + 1, 0, 0),
        (major, minor, build + 1, 0),
        (major, minor, build, revision + 1),
    )
    allowed = [version for version in candidates if all(v <= MAX_COMPONENT for v in version)]
    if requested_version not in allowed:
        if not allowed:
            raise ValueError(
                f"No next version is representable from {format_version(baseline)}."
            )
        versions = ", ".join(format_version(version) for version in sorted(allowed))
        raise ValueError(
            "The next version must increment exactly one component and reset all lower "
            f"components to zero. Effective current: {format_version(baseline)}. "
            f"Allowed: {versions}."
        )
    return requested_version


def format_version(version: tuple[int, int, int, int]) -> str:
    return ".".join(map(str, version))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    release_parser = subparsers.add_parser("next-release")
    release_parser.add_argument("--header", default="m1_csrc/m1_fw_update_bl.h")
    bump_parser = subparsers.add_parser("validate-bump")
    bump_parser.add_argument("--header", default="m1_csrc/m1_fw_update_bl.h")
    bump_parser.add_argument("--requested", required=True)
    args = parser.parse_args()

    try:
        current = read_header_version(pathlib.Path(args.header))
        tags = json.load(sys.stdin)
        if args.command == "next-release":
            released, next_version = next_release_version(current, tags)
            print(f"released={format_version(released) if released else ''}")
            print(f"version={format_version(next_version)}")
        else:
            version = validate_bump(current, tags, args.requested)
            print(f"version={format_version(version)}")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
