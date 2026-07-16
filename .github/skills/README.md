# Agent Skills

Modular, on-demand instruction sets for AI coding agents working in this
repository. Each subdirectory contains a single `SKILL.md` with YAML frontmatter
(`name`, `description`) followed by the rules for that topic.

These skills were split out of the formerly-monolithic `CLAUDE.md` so that the
always-loaded core stays small and only task-relevant guidance is pulled in.

## How skills are used

- [`CLAUDE.md`](../../CLAUDE.md) (repo root) is the always-loaded core: universal
  rules + a **Skill Index** routing table.
- At the start of a task, match your work against the Skill Index and read the
  relevant `SKILL.md` in full.
- The core rules in `CLAUDE.md` always apply regardless of which skill is loaded.

## Skills

| Skill | Trigger |
|-------|---------|
| `subghz-protocols` | Sub-GHz protocols, replay, `.sub`/`.sgh`, bind wizard, freq presets, Sub-GHz menu |
| `flipper-import` | Importing Flipper files (Sub-GHz / LF-RFID / NFC / IR) |
| `esp32-coprocessor` | ESP32-C6, WiFi, Bluetooth, BLE, 802.15.4, AT commands, SPI |
| `ui-scene-architecture` | Scenes, menus, button bars, fonts, saved-item UI, post-connection nav |
| `firmware-testing` | Host tests, pure-logic extraction, phase checklist, modularization |
| `hardware-state-mgmt` | Async RTOS + radio/ESP32/NFC/IR/Read Raw/backlight lifecycles |
| `memory-heap` | `malloc`/heap/FreeRTOS/ISR allocation, heap-redirect checklist |
| `vendored-deps` | Updating u8g2 / FreeRTOS / FatFs / IRMP |
| `docs-changelog` | Changelog fragments + documentation update matrix |
| `forks-tracker` | Fork auditing, upstream comparison, cherry-pick decisions |

## Maintenance

- When a rule's source table lives in a skill (e.g. the Font Inventory in
  `ui-scene-architecture`, the Local Modification Registry in `vendored-deps`, the
  Public Forks Tracker in `forks-tracker`), update the table **in that skill file**.
- When adding a new skill, add a row to the Skill Index in `CLAUDE.md`, to
  `AGENTS.md`, and to this table.
