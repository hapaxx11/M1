# Phase Checklist — Fork Feature Assessment (July 2026) implementation

## PR Metadata
- **PR Title**: ESP32-C6 idle auto power-off + July 2026 fork review
- **PR Description**: Implements the highest-value missing ESP32 feature from the
  July 2026 fork assessment — automatic ESP32-C6 power-off after 60 s of idle
  (battery savings), backed by a host-tested pure-logic timing core. Records the
  fork review in the forks-tracker skill, noting the plan items already present in
  Hapax (SPI-AT re-init leak fix, USB-UART bridge, 2048 game).

## Audit Result (verified in tree before implementing)
- 1A ESP32-C6 idle auto power-off — **MISSING** (`esp32_disable()` is commented out
  in `m1_esp32_deinit`, so the C6 stays powered after use) → implement.
- 1B ESP32 SPI-AT re-init leak fix — **ALREADY PRESENT** (`esp32_main_deinit()`
  task-join barrier + `esp32_main_init_done` guard in `esp_app_main.c`).
- 1C ESP32 boot readiness handshake — **PARTIALLY PRESENT** (handshake-pin
  injection in `esp32_main_init()`); deferred, no blind-delay race remaining that
  is safely fixable without hardware.
- 2A/2B LF RFID fixes — TIM3/TIM5 clock-enable-before-deinit fix **ALREADY PRESENT**
  (`lfrfid_read_hw_deinit`); ICFilter/write-from-saved changes require the exact
  upstream diff + hardware validation → deferred.
- 3A GPIO USB-UART bridge — **ALREADY PRESENT** (`m1_gpio_uart.c`).
- 3B GPIO Pin Map — MISSING; deferred (UI-only, medium effort, lower priority).
- 3C 2048 game — **ALREADY PRESENT** (`game_2048.c`, wired into games scene).

## Phases

### Phase 1 — ESP32-C6 idle auto power-off (pure-logic core + host test)
- **Description**: New `m1_csrc/esp32_idle.c/h` pure-logic idle-timeout state machine
  with `tests/test_esp32_idle.c` host coverage; registered in firmware + test CMake.
- **Status**: ✅ Complete
- **Commit**: `Add ESP32-C6 idle power-off pure-logic core + host test`

### Phase 2 — Wire idle power-off into the ESP32 HAL + periodic task
- **Description**: Add `m1_esp32_idle_poll()` in `m1_esp32_hal.c` (reads EN pin +
  init state, calls `esp32_disable()` on timeout) and call it from
  `system_periodic_task()` inside the non-firmware-update guard.
- **Status**: ✅ Complete
- **Commit**: `Wire ESP32-C6 idle power-off into HAL and system periodic task`

### Phase 3 — Docs: record the July 2026 fork review
- **Description**: Update `.github/skills/forks-tracker/SKILL.md` review dates/notes.
- **Status**: 🔲 Not started
- **Commit**: _(pending)_
