# GitHub Copilot Instructions

The standing instructions for AI coding agents in this repository live in
[`CLAUDE.md`](../CLAUDE.md) (the slim always-on core) and the modular skills under
[`.github/skills/`](skills/).

**Start here:**

1. Read [`CLAUDE.md`](../CLAUDE.md) for the ABSOLUTE RULES and core workflow rules
   that apply to every task.
2. Use the **Skill Index** in `CLAUDE.md` to find and read the `SKILL.md` for any
   skill whose trigger matches your task before you begin.

See [`AGENTS.md`](../AGENTS.md) for the full skill directory.

Key non-negotiables (full text in `CLAUDE.md`):

- **No AI attribution** in commits, code, or files — never add `Co-Authored-By`.
- **No unauthorized remote operations** — all work is local unless told otherwise;
  push only to `origin` (hapaxx11/M1).
- **Always build after compilation-affecting code changes**; documentation-only
  changes (`.md`, `documentation/`, `.github/`, databases) do not require a build.
- **Every bug fix requires a host-side regression test** that fails before the fix
  and passes after it.
