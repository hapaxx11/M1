<!-- See COPYING.txt for license details. -->

# RAM Reduction Plan (issue #747)

> **Status:** Phase 1 landed in PR #737. Phases 2+ are proposals for follow-up
> PRs, pending owner review. This is the "plan + phased checklist" requested in
> issue #747.

## 1. Problem

The STM32H573VITX `RAM` region is **640 KiB (655360 bytes)**. Before this work
the firmware linked at **99.98% RAM used — only 144 bytes free**:

```
Memory region         Used Size  Region Size  %age Used
             RAM:      655216 B       640 KB     99.98%
           FLASH:      828296 B      1023 KB     79.07%
```

There is essentially no headroom: almost any new `static`/global buffer
overflows the link (see PR #737). FLASH, by contrast, is only ~79% used — there
is **~200 KiB of flash headroom**, which is the key lever exploited in Phase 1.

### How the 640 KiB breaks down (measured, Release build)

| Consumer | Bytes | Notes |
|----------|------:|-------|
| FreeRTOS heap `ucHeap[]` (`configTOTAL_HEAP_SIZE`) | 262144 | static `.bss` pool, `Core/Inc/FreeRTOSConfig.h` |
| ESP32 flasher-stub table (`esp_stub[]`) | ~92912 | **Phase 1 — moved to flash** |
| All other `.data` + `.bss` (buffers, scene state, task-owned statics) | remainder | see "Largest remaining consumers" below |

Method: `arm-none-eabi-nm --print-size --size-sort` on the linked `.elf`,
cross-referenced with the `.map` file. Reproduce with the build in
`.github/skills/memory-heap/SKILL.md`.

## 2. Phase 1 — ESP32 flasher stubs: RAM → flash (DONE)

**Root cause.** `Esp32_serial_flasher/src/esp_stubs.c` defines
`const esp_stub_t esp_stub[ESP_MAX_CHIP]`, but each stub payload was written as
a **non-`const`** compound literal:

```c
.data = (uint8_t[]){ 0x0, 0xc5, ... },   /* ~91 KB across all chips */
```

A non-`const` compound literal has static storage in the **writable** `.data`
section (RAM), even though the enclosing array is `const`. All 14 payloads
(`__compound_literal.0..13`, from `esp_stubs.c` per the `.map`) therefore
consumed **~92,912 bytes of RAM**.

**Fix (PR #737).** Make the payloads `const` so they live in flash (`.rodata`):

- `Esp32_serial_flasher/include/esp_loader.h` — `esp_loader_bin_segment_t.data`
  is now `const uint8_t *`.
- `Esp32_serial_flasher/src/esp_stubs.c` — every payload is
  `.data = (const uint8_t[]){ ... }`.
- `Esp32_serial_flasher/src/protocol_uart.c`,
  `Esp32_serial_flasher/app/common/app_common.c` — the reader pointers are now
  `const uint8_t *` (stub/segment data is transmit-only; it is never written —
  `esp_loader.c` reads only `.addr`/`.size`, and the data is passed to
  `esp_loader_mem_write(const void *, ...)`).

**Measured result:**

```
             RAM:      562304 B       640 KB     85.80%   (was 99.98%)
           FLASH:      828292 B      1023 KB     79.07%   (unchanged)
```

**~92,912 bytes (~90.7 KiB) freed. Free RAM: 144 B → ~93 KiB.** This alone
clears the critical condition. Flash is unchanged because the initialised
`.data` copy already occupied flash; it now stays there instead of being copied
to RAM at boot.

**Guardrails added:**
- `tests/test_esp_stub_rodata.c` — host regression test that fails if the
  `const` qualifiers are ever dropped (e.g. by regenerating `esp_stubs.c` from
  upstream). Fails before the fix, passes after.
- Registered in the vendored-deps **Local Modification Registry**
  (`.github/skills/vendored-deps/SKILL.md`) with in-file "do not remove"
  markers, because `esp_stubs.c` is auto-generated.
- `Esp32_serial_flasher/**` added to the CI test path filter
  (`.github/workflows/tests.yml`).

## 3. Largest remaining consumers (post-Phase-1 targets)

Measured `static`/global symbols after Phase 1 (excluding the 256 KiB heap):

| Symbol | Bytes | Location | Phase |
|--------|------:|----------|-------|
| `s_api_buf` | 32768 | `m1_csrc/m1_fw_source.c` (`API_RESPONSE_BUF_SIZE`) | 3 |
| `fb_sorted` | 24672 | `m1_csrc/m1_file_browser.c` (`FILE_BROWSER_MAX_FILES`) | 3 |
| `saved_signal` | 16544 | `m1_csrc/m1_subghz_scene_saved_menu.c` | 2 |
| `s_signal` | 16544 | `m1_csrc/m1_sub_ghz.c` | 2 |
| `s_sig` | 16544 | `m1_csrc/m1_subghz_scene_decode_raw.c` | 2 |
| `s_tx_buf` / `s_rx_frame` / `s_deferred_frame` | ~8196 ×3 | `m1_csrc/m1_rpc.c` (`RPC_MAX_FRAME_SIZE`) | 3 |
| `snoop_buf` / `resp` / `out_buf` | 8192 ×3 | `m1_rpc.c` / `m1_sub_ghz.c` | 3 |
| various AT/search/NFC buffers | 2–4 KiB each | `m1_wifi.c`, `m1_nfc.c`, ESP32 cmd | 3 |

## 4. Proposed follow-up phases (pending review)

### Phase 2 — Consolidate mutually-exclusive Sub-GHz signal buffers (~33 KiB)
`saved_signal`, `s_signal`, `s_sig` are three `flipper_subghz_signal_t`
instances (~16.5 KiB each) owned by **different, never-simultaneously-active
Sub-GHz scenes**. This is the exact use case for the **shared scratch union**
pattern already used by Peer-Link scenes
(`m1_espnow_scene_scratch_t`, see the `memory-heap` skill). Collapsing the two
scene-local copies onto one shared backing buffer recovers ~16–33 KiB. Requires
verifying the scenes are provably mutually exclusive (each `memset`s its own
state on entry). **Load `subghz-protocols` + `memory-heap` skills first.**

### Phase 3 — Right-size / share large tool buffers (~30–50 KiB)
Audit `s_api_buf` (32 KiB), `fb_sorted` (24 KiB), and the RPC frame buffers
(3 × 8 KiB). Options: derive sizes from the actual largest real payload rather
than round numbers; move request-lifetime buffers to heap (`malloc`/`free`);
share the RPC TX/RX/deferred frames where lifetimes don't overlap. Each change
needs a host regression test per repo policy.

### Phase 4 — Right-size the FreeRTOS heap (potential tens of KiB)
`configTOTAL_HEAP_SIZE` = 256 KiB is a **static** reservation regardless of
runtime use. Instrument `xPortGetMinimumEverFreeHeapSize()` across the
heaviest paths (Sub-GHz Read Raw allocates ~128 KiB ring buffers; SD write
buffer; ESP32 OTA) to find the true high-water mark, then trim the pool to
`high_water + safety_margin`. **High risk** — must exercise every large
allocation path on hardware before trimming. Do **not** shrink blindly.

### Phase 5 — Unused-asset audit (flash first, then RAM)
Scan embedded assets (`m1_csrc/m1_display_data.c` bitmaps/fonts) and const
tables for unreferenced entries; retain-but-build-exclude anything
"maybe-useful-later" per the issue. Primary win is flash, but any const data
that is copied to RAM (non-`const` tables, like the Phase-1 bug) is a direct RAM
win — grep for the same `(type[]){...}`-in-`.data` anti-pattern elsewhere.

### Phase 6 — Externalise lesser-used features as `.m1app`
The firmware already has an ELF32 app loader
(`m1_csrc/m1_elf_loader.c`, `m1_app_manager.c`, `m1_app_api.c`; enabled via
`M1_APP_APPS_ENABLE` in `m1_compile_cfg.h`; apps in `0:/apps`, `.m1app`
extension). Strong externalisation candidates (self-contained, low core
coupling): **Games** and standalone **tools** (hex viewer, RGB backlight). Note:
externalising code primarily reclaims **flash**; it only reclaims RAM for the
`static` state those modules hold. For a new `.m1apps` repo, follow Flipper
**Momentum** app UX conventions (not CD3/Monstatek); the bedge117
[m1os-sdk](https://github.com/bedge117/m1os-sdk) is a reference. Apps must meet
documented M1 UX standards (`ui-scene-architecture` skill).

### Phase 7 — Guardrails
Once headroom exists, tighten `tools/check_ram_budget.sh` (currently a
non-blocking warning at 98%) — consider a blocking gate at a chosen ceiling, and
keep the Static RAM Budget rule in the `memory-heap` skill authoritative.

## 5. Verification (Phase 1)

```bash
# Firmware — confirm RAM dropped to ~85.8%
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel "$(nproc)"      # reads "RAM: ... 85.80%"

# Host regression guard
cmake -B build-tests -S tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tests
ctest --test-dir build-tests -R esp_stub_rodata --output-on-failure
```
