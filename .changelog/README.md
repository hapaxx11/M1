# Changelog Fragments

This directory holds **changelog fragment files** — one file per change,
assembled into `CHANGELOG.md` automatically at release time.

## Why fragments?

When multiple branches edit `CHANGELOG.md` simultaneously, merge conflicts
are inevitable.  Fragment files are **new files** — git merges them without
conflicts.  CI assembles them into the changelog when a release is published.

## File naming

```
<short-description>.<category>.md
```

Where `<category>` is one of:

| Category  | Changelog heading |
|-----------|-------------------|
| `added`   | `### Added`       |
| `changed` | `### Changed`     |
| `fixed`   | `### Fixed`       |
| `removed` | `### Removed`     |

**Examples:**

```
ir-universal-remote.added.md
firmware-download-buffer.fixed.md
remove-legacy-menu.removed.md
```

Use lowercase, hyphens, and keep it short.  The description part is only for
humans browsing the directory — it does not appear in the assembled changelog.

## File content

Write the **bullet-point text** for your changelog entry.  You may omit the
leading `- ` dash — the assembler adds it if missing.

Multi-line entries are supported: indent continuation lines by 2 spaces.

```markdown
**Infrared: Universal Remote quick-access remotes** — Momentum-style category
  quick-remotes (TV, AC, Audio, Projector, Fan, LED) accessible directly from
  the Universal Remote dashboard.
```

Multiple entries can go in one file, separated by a blank line:

```markdown
**IR: Brute-force power scan** — Iterate through all known power codes.

**IR: Per-function universal files** — Added Universal_Vol_up.ir, etc.
```

## Workflow

1. **On your branch**, create a fragment file here instead of editing
   `CHANGELOG.md`.
2. Commit the fragment with your code changes.
3. When the PR merges and a release build runs, CI assembles all fragments
   into `CHANGELOG.md` and deletes the fragment files.

## Local preview

To see what the assembled changelog would look like without modifying files:

```bash
python scripts/assemble_changelog.py --preview
```

To assemble locally (e.g. to review before a manual release):

```bash
python scripts/assemble_changelog.py --keep    # assemble but keep fragments
python scripts/assemble_changelog.py            # assemble and delete fragments
```
