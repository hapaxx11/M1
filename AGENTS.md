# AGENTS.md — Entry Point for AI Coding Agents

This repository's standing instructions for AI assistants live in
[`CLAUDE.md`](CLAUDE.md). **Read it first.**

`CLAUDE.md` is a **slim always-on core** (universal rules + a Skill Index). Deep,
task-specific rules have been split into **modular skills** under
[`.github/skills/`](.github/skills/) so that only the guidance relevant to your
current task needs to be loaded.

## What to do at the start of every task

1. Read [`CLAUDE.md`](CLAUDE.md) — it contains the ABSOLUTE RULES (no AI
   attribution, no unauthorized remote ops, no public exposure), the core
   workflow rules (build-after-code, bug-fix-needs-test), deploy/build info, the
   versioning quick-reference, and remote configuration.
2. Consult the **Skill Index** in `CLAUDE.md` and open the `SKILL.md` for any
   skill whose trigger matches your task. Skipping the relevant skill is how
   rules get overlooked.

## Skill directory

| Skill | Use for |
|-------|---------|
| [`subghz-protocols`](.github/skills/subghz-protocols/SKILL.md) | Sub-GHz protocols, replay, `.sub`/`.sgh`, bind wizard, freq presets, Sub-GHz menu |
| [`flipper-import`](.github/skills/flipper-import/SKILL.md) | Importing Flipper files (Sub-GHz / LF-RFID / NFC / IR) |
| [`esp32-coprocessor`](.github/skills/esp32-coprocessor/SKILL.md) | ESP32-C6, WiFi, Bluetooth, BLE, 802.15.4, AT commands, SPI |
| [`ui-scene-architecture`](.github/skills/ui-scene-architecture/SKILL.md) | Scenes, menus, button bars, fonts, saved-item UI, post-connection nav |
| [`firmware-testing`](.github/skills/firmware-testing/SKILL.md) | Host tests, pure-logic extraction, phase checklist, modularization |
| [`hardware-state-mgmt`](.github/skills/hardware-state-mgmt/SKILL.md) | Async RTOS + radio/ESP32/NFC/IR/Read Raw/backlight lifecycles |
| [`memory-heap`](.github/skills/memory-heap/SKILL.md) | `malloc`/heap/FreeRTOS/ISR allocation, heap-redirect checklist |
| [`vendored-deps`](.github/skills/vendored-deps/SKILL.md) | Updating u8g2 / FreeRTOS / FatFs / IRMP |
| [`docs-changelog`](.github/skills/docs-changelog/SKILL.md) | Changelog fragments + documentation update matrix |
| [`forks-tracker`](.github/skills/forks-tracker/SKILL.md) | Fork auditing, upstream comparison, cherry-pick decisions |

The rules in `CLAUDE.md` always apply regardless of which skill is loaded.
