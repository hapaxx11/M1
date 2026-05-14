**Documentation: Sub-GHz async TX architecture** — Updated CLAUDE.md and DEVELOPMENT.md
  to reflect the completed async TX state machine from PR #470. Replaced the open
  "async-conversion" TODO section with completed architecture docs covering the four
  Momentum-aligned TX states, the new async prepare/start/continue/abort API, scene-local
  temp-file ownership, and TIM1 sharing rules. Updated the replay API table, emulate
  dispatch descriptions, emulation code rules, and SI4463 radio management section to
  remove stale references to the intentionally-blocking Read Raw TX path.
