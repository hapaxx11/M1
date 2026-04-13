# Firmware Testing Strategy

Host-side testing strategy for M1 firmware (Hapax fork).

---

## Tier 1 — Host-Side Unit Tests (CI ✅, Low Effort)

**Status: ✅ Active — 22 suites, runs on every push**

Pure-logic C functions tested on x86 with Unity + ASan/UBSan.
Uses the stub-based extraction pattern: minimal stubs in `tests/stubs/`
provide the types and constants each source file needs without any real
HAL, RTOS, or FatFS.

### Current suites (22)

| Suite | Module under test |
|-------|-------------------|
| `test_bit_util` | CRC-4/7/8/16, parity, reverse, whitening, LFSR |
| `test_json_mini` | JSON parser/builder |
| `test_flipper_subghz` | Flipper `.sub` file parser |
| `test_flipper_file` | Flipper generic file parser |
| `test_flipper_ir` | Flipper `.ir` file import |
| `test_flipper_nfc` | Flipper `.nfc` file import |
| `test_flipper_rfid` | Flipper `.rfid` file import |
| `test_subghz_registry` | Protocol registry lookups |
| `test_subghz_history` | Signal history buffer management |
| `test_subghz_key_encoder` | KEY→RAW OOK PWM encoder |
| `test_subghz_raw_line_parser` | RAW_Data line parser |
| `test_subghz_raw_decoder` | Offline RAW→protocol decoder |
| `test_subghz_playlist_parser` | Flipper→M1 path remapping |
| `test_subghz_block_decoder` | Block-code protocol decoders |
| `test_subghz_block_encoder` | Block-code protocol encoders |
| `test_subghz_manchester_codec` | Manchester encode/decode |
| `test_datatypes_utils` | Generic data type helpers |
| `test_menu_layout` | Menu scroll/selection math |
| `test_lfrfid_manchester` | LFRFID Manchester decoder |
| `test_fw_source_filter` | OTA asset name filter |
| `test_rpc_crc16` | RPC CRC-16 + frame builder/validator |
| `test_file_browser` | Case-insensitive sort, ext matching, path concat |

### Candidates for new suites

| Candidate | What to test | Notes |
|-----------|-------------|-------|
| Virtual keyboard logic | Cursor movement, character selection, text length limits, ENTER-when-empty guard | `m1_virtual_kb.c` — pure index math, no display deps |
| Config parser (`fw_sources.txt`) | Multi-source parsing, category filtering, malformed input handling | `m1_fw_source.c` — already has `fw_source_filter` suite for the OTA filter; extend to full config parser |
| NFC data formatters | UID formatting, ATQA/SAK display strings, sector dump pretty-print | Scattered across `m1_nfc.c` — extract pure formatting helpers |
| RFID encoding logic | T5577 block calculation, modulation encoding, protocol-specific bit layouts | `lfrfid/` — some Manchester coverage exists, extend to full encode |
| IR protocol tables | NEC/Samsung/RC5 encode/decode round-trips | `m1_csrc/m1_ir_nec.c` etc. — pure bit math |
| Scene stack math | Push/pop/replace index arithmetic, depth limits | `m1_scene.c` — if stack logic is separable from draw callbacks |
| Ring buffer logic | SubGhz raw recording ring buffer fill/drain edge cases | `m1_sub_ghz.c` ring buffer — extract if feasible |

### How to add a new suite

```bash
# 1. Create tests/test_<module>.c with Unity test functions
# 2. Add stubs to tests/stubs/ if the source file needs HAL/RTOS types
# 3. Add CMake target + CTest registration in tests/CMakeLists.txt:
#
#    add_executable(test_<module>
#        test_<module>.c
#        ${M1_ROOT}/m1_csrc/<module>.c
#    )
#    target_include_directories(test_<module> PRIVATE
#        ${STUBS_DIR}
#        ${M1_ROOT}/m1_csrc
#    )
#    target_link_libraries(test_<module> PRIVATE unity)
#    if(TARGET sanitizers)
#        target_link_libraries(test_<module> PRIVATE sanitizers)
#    endif()
#    add_test(NAME <module> COMMAND test_<module>)
#
# 4. Build + run:
#    cmake -B build-tests -S tests && cmake --build build-tests
#    ctest --test-dir build-tests --output-on-failure
```

### CI workflow

`.github/workflows/tests.yml` — triggers on PRs and pushes to `main`
when `m1_csrc/`, `Sub_Ghz/`, or `tests/` change.  Runs on
`ubuntu-24.04` with ASan + UBSan.

---

## Tier 2 — Host-Side Scene Harness with u8g2 Virtual Display (CI ✅, High Effort)

**Status: 🔮 Future — not yet implemented**

Compile the scene manager and draw code on the host with a u8g2
virtual framebuffer.  This enables testing UI flows (menu navigation,
scene stack, scroll behavior) without physical hardware, in CI.

### Architecture

```
┌──────────────────────────────────────────────────┐
│  Test code (Unity)                               │
│  ┌──────────┐  ┌──────────┐  ┌───────────────┐  │
│  │ inject   │  │ call     │  │ capture       │  │
│  │ button   │──│ scene    │──│ framebuffer   │  │
│  │ events   │  │ tick()   │  │ (128×64 buf)  │  │
│  └──────────┘  └──────────┘  └───────────────┘  │
└──────────────────────┬───────────────────────────┘
                       │
    ┌──────────────────┼───────────────────────┐
    │  Host stubs      │                       │
    │  ┌───────────┐   │   ┌────────────────┐  │
    │  │ FreeRTOS  │   │   │ u8g2 virtual   │  │
    │  │ queue     │   │   │ display (RAM   │  │
    │  │ stub      │   │   │ framebuffer)   │  │
    │  └───────────┘   │   └────────────────┘  │
    │  ┌───────────┐   │   ┌────────────────┐  │
    │  │ HAL GPIO  │   │   │ Font data      │  │
    │  │ stub      │   │   │ (compiled for  │  │
    │  │ (no-op)   │   │   │ host)          │  │
    │  └───────────┘   │   └────────────────┘  │
    └──────────────────┴───────────────────────┘
```

### Key challenges

| Challenge | Mitigation |
|-----------|-----------|
| u8g2 global state (`m1_u8g2`) | Initialize with virtual display backend (u8g2 supports this natively) |
| ~30 font references in draw code | Compile u8g2 font data for host (same `.c` files, just different target) |
| `xQueueReceive()` in event loop | Replace with test-driven `inject_event(button)` function |
| HAL GPIO reads (e.g., `HAL_GPIO_ReadPin`) | Stub to return configurable test values |
| Display macros (`M1_LCD_WIDTH`, `M1_LCD_HEIGHT`) | Already `#define` constants — work as-is |
| Scene callbacks reference global arrays | Compile the full scene table for host |

### What you'd test

- **Scene stack**: push/pop/replace with depth limits
- **Menu navigation**: UP/DOWN scroll, selection highlight, page transitions
- **Button bar rendering**: correct icons for each scene
- **Info screens**: multi-line text layout, scroll position
- **Config scenes**: LEFT/RIGHT value changes, item count
- **Visual regression**: pixel-exact golden PNG comparison after each draw()

### Implementation steps

1. **Add u8g2 virtual display setup** — `u8g2_Setup_128x64_1(...)` with
   null byte/GPIO callbacks, or use `u8g2_SetupBuffer_Utf8()` for text-mode
2. **Create `tests/stubs/u8g2_host.c`** — thin wrapper that calls
   `u8g2_SetupDisplay()` with a RAM-only display callback
3. **Stub FreeRTOS queue** — `xQueueReceive()` reads from a test-driven
   FIFO of injected button events
4. **Compile scene modules** — start with one scene (e.g., main menu),
   add more incrementally
5. **Framebuffer capture** — after `draw()`, read `u8g2_GetBufferPtr()`
   and compare against golden bitmaps
6. **CMake integration** — new test target that links u8g2 + scene code +
   stubs, registered with CTest

### Effort estimate

This is a **multi-day project** requiring:
- u8g2 library compiled for x86 (~20 source files)
- Comprehensive FreeRTOS/HAL stub layer
- Per-scene dependency analysis (which globals each scene touches)
- Golden bitmap generation and maintenance

The ROI is high for catching UI regressions, but the initial setup cost
is significant.  Consider implementing after Tier 1 coverage is
comprehensive.

---

## Hardware-in-the-Loop Tests

RPC-based integration tests (Python + pytest + physical M1 device) live in
a dedicated repository: **[hapaxx11/m1-hil-tests](https://github.com/hapaxx11/m1-hil-tests)**.

See that repo's README and CLAUDE.md for setup instructions.

---

## Decision Criteria for New Tests

```
Is the function pure logic (no HAL/display/RTOS)?
  → Tier 1: Host-side unit test (stub-based extraction)

Does it require the full firmware running on hardware?
  → hapaxx11/m1-hil-tests: RPC integration test (pytest + M1Client)

Does it involve UI rendering that should be tested in CI?
  → Tier 2: u8g2 scene harness (when available)
```
