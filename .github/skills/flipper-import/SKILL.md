---
name: flipper-import
description: How to import Flipper Zero protocol/file formats into the M1 firmware (Sub-GHz, LF-RFID, NFC, IR), including the file inventory tables, category subtotals, and the adoption policy. Load whenever adding or modifying anything under lfrfid/, Sub_Ghz/protocols/, m1_csrc/flipper_*.c/h, or when porting a Flipper file parser.
---

# Flipper Import

> This skill wraps the canonical, deep reference. **Do not duplicate its content
> here — read the source of truth directly.**

## Source of truth

All Flipper import procedures, file-format details, inventory tables, category
subtotals, the grand-total table, and the adoption policy live in:

- [`documentation/flipper_import_agent.md`](../../../documentation/flipper_import_agent.md)

Read that file before importing or modifying any Flipper-derived protocol or
parser.

## When this applies

Load this skill (and open `documentation/flipper_import_agent.md`) when a change
touches any of:

- `lfrfid/` — LF-RFID protocol files
- `Sub_Ghz/protocols/` — Sub-GHz protocol decoders
- `m1_csrc/flipper_*.c/h` — Flipper file parsers/integration
- NFC / IR Flipper file import paths

## Mandatory maintenance rule (from the Documentation & Changelog skill)

When any file is added to `lfrfid/`, `Sub_Ghz/protocols/`, or
`m1_csrc/flipper_*.c/h`, you **must** update `documentation/flipper_import_agent.md`:

- Add it to the correct inventory table (Category 1 / 2 / 4).
- Update the category subtotal.
- Update the Grand Total table (file count + directly-copied count).
- Update the threshold note if the directly-copied count changes significantly.
- When files are removed, remove them from the table and adjust totals.

## Related skills

- `subghz-protocols` — for Sub-GHz replay/emulation, the file-format taxonomy,
  and the new-protocol / frequency-preset checklist.
- `docs-changelog` — for the changelog fragment and the full documentation
  update matrix.
