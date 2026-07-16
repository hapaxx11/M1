---
name: docs-changelog
description: Documentation update rules: the mandatory changelog-fragment workflow (.changelog/*.md), and the doc-update matrix (README, ARCHITECTURE, DEVELOPMENT, flipper_import_agent, font inventory) that must be kept in sync with code changes. Load for any change that needs a changelog entry or documentation update.
---

# Documentation & Changelog Rules

> Extracted from CLAUDE.md. Every meaningful change needs a changelog fragment
> in .changelog/ — see .changelog/README.md.

## Documentation Update Rules

**Every agent session that makes code or documentation changes MUST also update the relevant
markdown files listed below.  Failing to do so is a workflow violation on par with forgetting
to build.**

### CHANGELOG.md — mandatory for every meaningful change

- **Location**: `CHANGELOG.md` (repo root).
- **Format**: [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) — `### Added`,
  `### Changed`, `### Fixed`, `### Removed` subsections.
- **Version label**: `[{major}.{minor}.{build}.{rc}]` — e.g. `[0.9.0.9]`.
  The date is the wall-clock date of the change (`YYYY-MM-DD`).

#### Fragment-based workflow (required)

> **DO NOT edit `CHANGELOG.md` directly on feature branches.**  Create a
> changelog **fragment file** instead.  This eliminates merge conflicts when
> multiple branches are in flight simultaneously.

- **Create a fragment file** in `.changelog/` for each logical change:
  ```
  .changelog/<short-description>.<category>.md
  ```
  where `<category>` is one of: `added`, `changed`, `fixed`, `removed`.
- **File content**: the bullet-point text for the entry — one entry per file.
  The leading `- ` dash is optional (the assembler normalises it).
  Multi-line entries are supported; indent continuation lines by 2 spaces.
  ```
  # .changelog/ir-quick-remotes.added.md
  **Infrared: Universal Remote quick-access remotes** — Momentum-style category
    quick-remotes accessible directly from the Universal Remote dashboard.
  ```
- **Multiple entries** in one file are separated by a blank line.
- **Naming**: use lowercase, hyphens, keep it short.  The description is for
  humans browsing the directory — it does not appear in the changelog.
- **CI assembly**: `build-release.yml` runs `scripts/assemble_changelog.py`
  at release time, which reads all fragment files, groups entries by category,
  merges them into the `[Unreleased]` section of `CHANGELOG.md`, deletes the
  fragment files, then stamps the version heading.
- **Local preview**: `python scripts/assemble_changelog.py --preview` shows
  what the assembled output would look like without modifying any files.

#### Category rules

- **When to add a fragment**:
  - New firmware feature, protocol, or UI screen → `<desc>.added.md`
  - Modification to existing behaviour, API, or protocol → `<desc>.changed.md`
  - Bug fix → `<desc>.fixed.md`
  - Removal of a feature or file → `<desc>.removed.md`
  - Documentation / process / tooling change → `<desc>.changed.md`
    with a "Documentation:" prefix in the entry text
- **When NOT to add a fragment**: Pure whitespace / formatting commits with
  zero functional effect.  Every other change needs a fragment.
- **One fragment per logical change**, not one per file edited.  Group related
  items into a single fragment when they form a cohesive feature or fix.

#### Rules that still apply

- Do **not** manually create versioned headings in `CHANGELOG.md` — CI stamps
  the version on release.
- **Retroactive changelog edits** to already-released sections are permitted
  **only** when the change increases accuracy of the released content AND is
  in the public interest and non-harmful.
- **CI stamper safety rule**: The stamper replaces the first occurrence of
  the `[Unreleased]` heading marker.  Never write that literal markdown
  heading string in changelog entry body text, fragment files, or code
  spans.  When referring to the heading in prose, write `[Unreleased]`
  (without the `## ` prefix).
- **Changelog stamp PRs are NOT changelog-worthy.**  The CI workflow creates a
  PR titled `changelog: stamp [Unreleased] as <tag>` after every release.
  These automated stamp PRs **MUST NOT** appear in changelog entries, release
  notes, or PR descriptions.  They are infrastructure housekeeping — they do
  not represent a user-visible change.  If you see one in auto-generated
  release notes, the `.github/release.yml` label exclusion has failed and
  should be investigated.

#### Backward compatibility

  The assembly script also preserves any entries that already exist under
  `[Unreleased]` in `CHANGELOG.md` (e.g. from legacy direct edits).  New
  fragment entries are merged into the correct category sections alongside
  existing entries.  Over time, all contributors should migrate to fragments.

### README.md — update when user-visible descriptions are stale

- Update the Sub-GHz protocol count if decoders are added or removed.
- Update the LF-RFID, NFC, IR, or feature lists if their descriptions no longer match
  reality.
- Update the build instructions if `CMAKE_PROJECT_NAME`, the post-build command, or
  prerequisites change.
- Update the SD card layout section if new top-level folders are introduced.

### documentation/flipper_import_agent.md — update when Flipper-derived files change

- When any file is added to `lfrfid/`, `Sub_Ghz/protocols/`, or `m1_csrc/flipper_*.c/h`:
  - Add it to the correct inventory table (Category 1 / 2 / 4).
  - Update the category subtotal.
  - Update the Grand Total table (file count + directly-copied count).
  - Update the threshold note if the directly-copied count changes significantly.
- When files are removed, remove them from the table and adjust totals accordingly.

### ARCHITECTURE.md — update when project structure changes

- Add new top-level directories to the component table.
- Update the Build System section if new build backends or CMake presets are introduced.

### DEVELOPMENT.md — update when build process or coding standards change

- Update build commands, environment requirements, or workflow rules if they change.
- Keep this in sync with the canonical `CLAUDE.md` build instructions.

### CLAUDE.md Font Inventory — update when fonts change

- When adding, removing, or changing any font in `m1_display.h` macros or the
  `m1_app_api.c` font table, update the **Font Inventory** table in the
  Architecture Rules section of this file.
- See the **Font Maintenance Rules** subsection for the full checklist.

---
