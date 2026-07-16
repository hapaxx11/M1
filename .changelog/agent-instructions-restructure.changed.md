**Documentation: Agent instruction restructure** — split the monolithic
  `CLAUDE.md` (~2,500 lines) into a slim always-loaded core plus modular,
  on-demand skills under `.github/skills/` (subghz-protocols, flipper-import,
  esp32-coprocessor, ui-scene-architecture, firmware-testing, hardware-state-mgmt,
  memory-heap, vendored-deps, docs-changelog, forks-tracker). Added a Skill Index
  routing table to `CLAUDE.md`, `AGENTS.md` and `.github/copilot-instructions.md`
  entrypoints, and `documentation/agent/versioning.md`. No functional firmware change.
