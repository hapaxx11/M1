# Phase Checklist — Queens puzzle game

## PR Metadata
- **PR Title**: Add Queens puzzle game with procedural generation, timed high scores
- **PR Description**: Adds a LinkedIn-Queens-style puzzle game to the Games menu: procedurally generated puzzles (unique-solution verified) at 4 difficulty levels (6x6–9x9), monochrome pattern-shaded regions, per-difficulty top-3 timed high scores with initials/date/no-mistakes flag persisted to SD, auto-exclude assist option, pause/resume/give-up flow with solution reveal.

## Phases

### Phase 1 — Pure-logic module + host tests
- **Description**: `m1_csrc/queens_logic.c/h` — RNG, puzzle generator (queen placement + region growth + uniqueness repair + pattern assignment), solver, conflict/win/auto-exclude checks, high-score table + line serialize/parse. Host tests in `tests/test_queens_logic.c`, registered in `tests/CMakeLists.txt`. 17 tests pass via WSL ctest (ASan/UBSan).
- **Status**: ✅ Complete
- **Commit**: `Add Queens game pure-logic module with host tests`

### Phase 2 — Game UI + integration + firmware build
- **Description**: `m1_csrc/game_queens.c` (start screen with options+scores, play screen with pattern board/timer/cursor, pause screen, initials entry, end screens with solution/high scores), registered in `m1_games.h` / `m1_games_scene.c`, sources in `cmake/m1_01/CMakeLists.txt`, SD persistence of options+scores (`0:/apps/queens.dat`). Firmware builds clean; RAM 86.82% (context is heap-allocated, no new static buffers). README games list updated.
- **Status**: ✅ Complete
- **Commit**: `Add Queens game UI, menu entry, and SD high-score persistence`

### Phase 3 — Changelog + cleanup
- **Description**: Add `.changelog/queens-game.added.md` fragment, re-run host tests + firmware build, remove this checklist.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
