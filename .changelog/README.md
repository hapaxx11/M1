# Changelog Fragments

This directory holds **changelog fragment files** — one file per change,
assembled into `CHANGELOG.md` automatically before a PR merges to `main`.

## Why fragments?

When multiple branches edit `CHANGELOG.md` simultaneously, merge conflicts
are inevitable.  Fragment files are **new files** — git merges them without
conflicts.  CI assembles them into the changelog in the PR branch once the
PR is ready for review (non-draft).

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
3. When the PR is (or becomes) ready for review and changes firmware inputs
   (the `paths` allow-list shared with `build-release.yml`), the **Prepare
   Changelog** workflow assembles all fragments into `CHANGELOG.md`, deletes the fragment
   files, stamps the predicted next release version
   (`scripts/stamp_changelog.py`), and commits the result back to the PR
   branch.  Pull that commit before pushing more work.  Fragments from PRs
   that don't change the binary (docs, CI, tests) stay in `.changelog/` until
   the next firmware PR assembles them.  Fork PRs cannot be
   written to — their fragments are assembled by the next same-repo PR.

## Release notes

The changelog sections not yet published (plus any fragments
still un-assembled on `main`) are rendered into the **"What's Changed"** section
of the GitHub release body (`build-release.yml`), instead of relying on
GitHub's PR-label-based auto-generated notes (which otherwise collapse
everything into a single "Other Changes" bullet when PRs aren't labelled). If
there is nothing to report, the release notes fall back to a placeholder
message.

A section counts as unpublished when its version is newer than the previous
release, or when its entries are absent from `CHANGELOG.md` as published in
that release — which is how a section keeps its entries after another PR
released the version it had predicted.

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
