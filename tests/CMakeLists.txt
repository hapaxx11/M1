# Host-side unit test build
#
# Builds and runs tests on the host (x86/x64) using the Unity test framework.
# Only pure-logic modules with no hardware dependencies are included.
#
# Usage:
#   cmake -B build-tests -S tests
#   cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure

cmake_minimum_required(VERSION 3.16)
project(m1_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------
# Options
# ------------------------------------------------------------------
option(ENABLE_SANITIZERS "Enable ASan + UBSan" ON)

# ------------------------------------------------------------------
# Sanitizers (ASan + UBSan) — catches memory bugs at test time
# Uses an INTERFACE library so flags are applied per-target, not globally.
# ------------------------------------------------------------------
if(ENABLE_SANITIZERS AND NOT MSVC)
    add_library(sanitizers INTERFACE)
    target_compile_options(sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(sanitizers INTERFACE -fsanitize=address,undefined)
    message(STATUS "Address Sanitizer + UBSan enabled")
endif()

# Warnings (compiler-specific)
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)
endif()

# ------------------------------------------------------------------
# Unity test framework (vendored — single-file drop-in)
# ------------------------------------------------------------------
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC unity)

# ------------------------------------------------------------------
# Source root
# ------------------------------------------------------------------
set(M1_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")

# ------------------------------------------------------------------
# Module under test: bit_util
# Pure functions — CRC, parity, byte manipulation. Zero dependencies.
# ------------------------------------------------------------------
add_executable(test_bit_util
    test_bit_util.c
    ${M1_ROOT}/m1_csrc/bit_util.c
)
target_include_directories(test_bit_util PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_bit_util PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_bit_util PRIVATE sanitizers)
endif()

# ------------------------------------------------------------------
# CTest registration
# ------------------------------------------------------------------
enable_testing()
add_test(NAME bit_util COMMAND test_bit_util)

# ------------------------------------------------------------------
# Module under test: m1_esp32_caps (parse payload + capability bits + AT+CMD? parser)
#
# Tests the static-inline helpers in m1_esp32_caps.h which have zero hardware
# dependencies:
#   - m1_esp32_caps_parse_payload(): raw bytes → M1_ESP32_CAP_* bitmap + fw_name
#   - m1_esp32_caps_parse_at_cmd_list(): AT+CMD? response → M1_ESP32_CAP_* bits
#   - m1_esp32_caps_at_cmd_response_valid(): sanity-checks the AT+CMD? response
# No firmware source file is compiled — only the header is exercised.
# Verifies:
#   - Valid SiN360 payload parses correctly via binary path
#   - Short / wrong-proto-version payloads are rejected
#   - fw_name is always null-terminated
#   - All cap bit constants are unique powers-of-two
#   - Composite profile macros contain the expected bit sets
#   - Bitmap is decoded little-endian
#   - AT+CMD? response parsing: stock/bedge-dag/neddy299 responses,
#     quoted-substring anchoring, empty/NULL inputs, unordered commands,
#     validity detector
# ------------------------------------------------------------------
add_executable(test_esp32_caps
    test_esp32_caps.c
)
target_include_directories(test_esp32_caps PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_esp32_caps PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_esp32_caps PRIVATE sanitizers)
endif()
add_test(NAME esp32_caps COMMAND test_esp32_caps)

# ------------------------------------------------------------------
# Module under test: esp32_feature_map (Phase C — feature classifier)
#
# Pure-logic module: given a capability bitmap (uint64_t), classifies
# which firmware features are available and identifies the firmware class
# (SiN360 vs. AT).  Zero HAL / RTOS / display dependencies.
# Verifies:
#   - esp32_feature_required_caps(): each feature ID maps to correct M1_ESP32_CAP_* bit
#   - esp32_feature_label(): non-empty labels for all valid feature IDs
#   - esp32_feature_supported(): zero bitmap, all-ones, exact-required, missing-bit
#   - esp32_firmware_is_sin360(): SiN360 profile, AT profile, edge bitmaps
#   - ESP32_FEATURE_COUNT equals the current table size (regression guard)
# ------------------------------------------------------------------
add_executable(test_esp32_feature_map
    test_esp32_feature_map.c
    ${M1_ROOT}/m1_csrc/esp32_feature_map.c
)
target_include_directories(test_esp32_feature_map PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_esp32_feature_map PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_esp32_feature_map PRIVATE sanitizers)
endif()
add_test(NAME esp32_feature_map COMMAND test_esp32_feature_map)

# ------------------------------------------------------------------
# Module under test: battery_soc_estimate
# Pure piecewise-linear voltage → SoC mapping. Zero hardware deps.
# Bug context: "Asian M1" fuel gauge reports 0% SoC while device is
# still functional; voltage-based correction provides a realistic floor.
# ------------------------------------------------------------------
add_executable(test_battery_soc
    test_battery_soc.c
    ${M1_ROOT}/Battery/battery_soc_estimate.c
)
target_include_directories(test_battery_soc PRIVATE
    ${M1_ROOT}/Battery
)
target_link_libraries(test_battery_soc PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_battery_soc PRIVATE sanitizers)
endif()
add_test(NAME battery_soc COMMAND test_battery_soc)

# ------------------------------------------------------------------
# Stubs directory — provides minimal HAL/FatFS/RTOS stubs so firmware
# source files compile on the host without hardware dependencies.
# ------------------------------------------------------------------
set(STUBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stubs")

# ------------------------------------------------------------------
# Module under test: BLE spam packet-count helper
# Pure helper verifies the Google Fast Pair "All" cycle advert count
# stays aligned with the UI packet counter.
# ------------------------------------------------------------------
add_executable(test_bt_spam_counts
    test_bt_spam_counts.c
)
target_include_directories(test_bt_spam_counts PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_bt_spam_counts PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_bt_spam_counts PRIVATE sanitizers)
endif()
add_test(NAME bt_spam_counts COMMAND test_bt_spam_counts)

# ------------------------------------------------------------------
# Module under test: bq27421_init pure-logic helpers
#
# Regression for v0.9.0.166 crash: bq27421_init() entered the full
# CFGUPDATE path for the first time (taper rate change + new
# DEFAULT_DESIGN_CAP check), revealing two bugs:
#   1. Checksum comparison casts the wrong operand, always returning
#      false even when the block write succeeded.
#   2. BQ27421 left in CFGUPMODE indefinitely on the false-return path.
#
# The test does NOT link against m1_bq27421.c (too many I2C/HAL deps).
# It tests only the static inline pure helpers declared in m1_bq27421.h.
# ------------------------------------------------------------------
add_executable(test_bq27421_init
    test_bq27421_init.c
)
target_include_directories(test_bq27421_init PRIVATE
    ${M1_ROOT}/m1_csrc
    ${STUBS_DIR}
)
target_link_libraries(test_bq27421_init PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_bq27421_init PRIVATE sanitizers)
endif()
add_test(NAME bq27421_init COMMAND test_bq27421_init)

# ------------------------------------------------------------------
# Module under test: watchdog boot-loop backup-domain guard
# Pure helper logic in m1_watchdog_boot_loop.h; verifies stale-signature
# invalidation of DR2 counter and threshold behavior.
# ------------------------------------------------------------------
add_executable(test_watchdog_boot_loop
    test_watchdog_boot_loop.c
)
target_include_directories(test_watchdog_boot_loop PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_watchdog_boot_loop PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_watchdog_boot_loop PRIVATE sanitizers)
endif()
add_test(NAME watchdog_boot_loop COMMAND test_watchdog_boot_loop)

# ------------------------------------------------------------------
# Module under test: m1_json_mini
# Pure JSON parser — no hardware dependencies at all.
# Bug context: OTA firmware download always returned "No releases found"
# because the JSON parser had to handle GitHub API response format.
# ------------------------------------------------------------------
add_executable(test_json_mini
    test_json_mini.c
    ${M1_ROOT}/m1_csrc/m1_json_mini.c
)
target_include_directories(test_json_mini PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_json_mini PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_json_mini PRIVATE sanitizers)
endif()
add_test(NAME json_mini COMMAND test_json_mini)

# ------------------------------------------------------------------
# Module under test: flipper_subghz (preset + freq parsers)
# Stubs needed: m1_io_defs.h, m1_ring_buffer.h, stm32h5xx_hal.h, ff.h
# Bug context: 2FSK replay at all frequencies — case-insensitive preset
# matching was missing; ASK presets were unrecognized.
# ------------------------------------------------------------------
add_executable(test_flipper_subghz
    test_flipper_subghz.c
    ${M1_ROOT}/m1_csrc/flipper_subghz.c
    ${M1_ROOT}/m1_csrc/flipper_file.c
)
target_include_directories(test_flipper_subghz PRIVATE
    ${STUBS_DIR}            # ff.h, m1_io_defs.h, m1_ring_buffer.h stubs
    ${M1_ROOT}/m1_csrc
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_flipper_subghz PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_flipper_subghz PRIVATE sanitizers)
endif()
add_test(NAME flipper_subghz COMMAND test_flipper_subghz)

# ------------------------------------------------------------------
# Module under test: flipper_file (hex/int32 parsers + kv parser)
# Stubs needed: ff.h (FatFS types only — no actual I/O in test)
# Bug context: Flipper .sub/.rfid/.nfc/.ir file import had parsing
# edge cases with hex bytes, negative raw data values, whitespace.
# ------------------------------------------------------------------
add_executable(test_flipper_file
    test_flipper_file.c
    ${M1_ROOT}/m1_csrc/flipper_file.c
)
target_include_directories(test_flipper_file PRIVATE
    ${STUBS_DIR}            # ff.h stub
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_flipper_file PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_flipper_file PRIVATE sanitizers)
endif()
add_test(NAME flipper_file COMMAND test_flipper_file)

# ------------------------------------------------------------------
# Module under test: subghz_raw_capture_alloc
# Pure allocation-probe helper for Read Raw capture startup.
# Bug context: the capture ring buffer could greedily consume heap,
# leaving no room for the SD writer and causing "Record failed" even
# with a valid SD card.
# ------------------------------------------------------------------
add_executable(test_subghz_raw_capture_alloc
    test_subghz_raw_capture_alloc.c
    ${M1_ROOT}/Sub_Ghz/subghz_raw_capture_alloc.c
)
target_include_directories(test_subghz_raw_capture_alloc PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_raw_capture_alloc PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_raw_capture_alloc PRIVATE sanitizers)
endif()
add_test(NAME subghz_raw_capture_alloc COMMAND test_subghz_raw_capture_alloc)

# ------------------------------------------------------------------
# Source invariant: ESP32 transport must be quiesced around Sub-GHz.
# Bug context: SiN360 ESP32 tools can leave SPI3/EXTI active; Sub-GHz
# Read/Read Raw depend on timing-sensitive SI4463/TIM1 capture.
# ------------------------------------------------------------------
add_executable(test_esp32_subghz_lifecycle
    test_esp32_subghz_lifecycle.c
)
target_compile_definitions(test_esp32_subghz_lifecycle PRIVATE
    M1_ROOT="${M1_ROOT}"
)
target_link_libraries(test_esp32_subghz_lifecycle PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_esp32_subghz_lifecycle PRIVATE sanitizers)
endif()
add_test(NAME esp32_subghz_lifecycle COMMAND test_esp32_subghz_lifecycle)

# ------------------------------------------------------------------
# Source invariant: Phase E submenu-widget rollout.
# All BT, Settings, NFC, and WiFi scene menus must use m1_submenu_event
# + m1_submenu_draw (shared widget) and must NOT call m1_scene_menu_event
# (raw sel/scroll pair API).
# ------------------------------------------------------------------
add_executable(test_submenu_widget_rollout
    test_submenu_widget_rollout.c
)
target_compile_definitions(test_submenu_widget_rollout PRIVATE
    M1_ROOT="${M1_ROOT}"
)
target_link_libraries(test_submenu_widget_rollout PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_submenu_widget_rollout PRIVATE sanitizers)
endif()
add_test(NAME submenu_widget_rollout COMMAND test_submenu_widget_rollout)

# ------------------------------------------------------------------
# Source invariant: esp32_main_deinit() releases all heap held by the
# legacy AT-over-SPI plumbing (task stack, trans_data, stream buffer,
# semaphores, device handle).
# Bug context: Sub-GHz Read Raw failed after any ESP32-dependent feature
# (Bad-BT, Zigbee scan, FW update) because esp32_main_init() allocated
# ~17 KB that was never freed, leaving insufficient heap for the 128 KB
# capture buffer + 28 KB SD-writer reserve.
# ------------------------------------------------------------------
add_executable(test_esp32_main_deinit_releases_legacy_task
    test_esp32_main_deinit_releases_legacy_task.c
)
target_compile_definitions(test_esp32_main_deinit_releases_legacy_task PRIVATE
    M1_ROOT="${M1_ROOT}"
)
target_link_libraries(test_esp32_main_deinit_releases_legacy_task PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_esp32_main_deinit_releases_legacy_task PRIVATE sanitizers)
endif()
add_test(NAME esp32_main_deinit_releases_legacy_task
    COMMAND test_esp32_main_deinit_releases_legacy_task)

# ------------------------------------------------------------------
# Module under test: subghz_protocol_registry (lookup functions)
# Stubs needed: all decoder functions + HAL stubs
# Bug context: .sub file replay used fragile strstr() chains —
# Cham_Code, Marantec24, and all new static protocols were missed.
# Also links subghz_freq_presets.c so the registry test can verify
# that every protocol's band flags match the available freq presets.
# ------------------------------------------------------------------
add_executable(test_subghz_registry
    test_subghz_registry.c
    ${M1_ROOT}/Sub_Ghz/subghz_protocol_registry.c
    ${M1_ROOT}/Sub_Ghz/subghz_decode_try_fn.c
    ${M1_ROOT}/m1_csrc/subghz_freq_presets.c
    ${STUBS_DIR}/subghz_decode_stubs.c
)
target_include_directories(test_subghz_registry PRIVATE
    ${STUBS_DIR}            # HAL/FatFS stubs
    ${M1_ROOT}/m1_csrc
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_registry PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_registry PRIVATE sanitizers)
endif()
add_test(NAME subghz_registry COMMAND test_subghz_registry)

# ------------------------------------------------------------------
# Module under test: subghz_freq_presets
# Pure data — no HAL or RTOS dependencies; links nothing but the
# preset table itself.  Enforces structural invariants (count,
# sort order, range, no duplicates) and regression guards for
# specific frequencies (319.5 MHz, 315.0 MHz, 433.92 MHz, 868.35 MHz).
# Bug context: 319.5 MHz (Magellan/GE NA) was missing, causing
# M1 to receive nothing when Flipper emulated Magellan .sub files.
# ------------------------------------------------------------------
add_executable(test_subghz_freq_presets
    test_subghz_freq_presets.c
    ${M1_ROOT}/m1_csrc/subghz_freq_presets.c
)
target_include_directories(test_subghz_freq_presets PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_subghz_freq_presets PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_freq_presets PRIVATE sanitizers)
endif()
add_test(NAME subghz_freq_presets COMMAND test_subghz_freq_presets)

# ------------------------------------------------------------------
# Module under test: subghz_history (signal history ring buffer)
# Pure data structure — no hardware dependencies at all.
# Tests add/get ordering, duplicate detection, circular overflow,
# and boundary conditions.
# ------------------------------------------------------------------
add_executable(test_subghz_history
    test_subghz_history.c
    ${STUBS_DIR}/subghz_history_impl.c
)
target_include_directories(test_subghz_history PRIVATE
    ${STUBS_DIR}            # stm32h5xx_hal.h, m1_io_defs.h, m1_ring_buffer.h stubs
    ${M1_ROOT}/m1_csrc
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_history PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_history PRIVATE sanitizers)
endif()
add_test(NAME subghz_history COMMAND test_subghz_history)

# ------------------------------------------------------------------
# Module under test: subghz_key_encoder (KEY→RAW OOK PWM encoding)
# Extracted from sub_ghz_replay_flipper_file() — pure math + registry
# lookup.  Tests timing resolution, bit encoding, repetitions, and
# rejection of dynamic/weather/TPMS protocols.
# ------------------------------------------------------------------
add_executable(test_subghz_key_encoder
    test_subghz_key_encoder.c
    ${M1_ROOT}/Sub_Ghz/subghz_key_encoder.c
    ${M1_ROOT}/Sub_Ghz/subghz_protocol_registry.c
    ${M1_ROOT}/Sub_Ghz/subghz_decode_try_fn.c
    ${STUBS_DIR}/subghz_decode_stubs.c
)
target_include_directories(test_subghz_key_encoder PRIVATE
    ${STUBS_DIR}            # HAL stubs
    ${M1_ROOT}/m1_csrc
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_key_encoder PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_key_encoder PRIVATE sanitizers)
endif()
add_test(NAME subghz_key_encoder COMMAND test_subghz_key_encoder)

# ------------------------------------------------------------------
# Module under test: subghz_protocol_decode (encoder/decoder roundtrip)
# Tests verify that key encoder output is correctly decoded by the
# protocol-specific decoders:
#   - Magellan: inverted polarity + frame-header skip (PR fix)
#   - CAME 12-bit: standard 1:2 OOK PWM (verified correct)
#   - GateTX 24-bit: standard 1:2 OOK PWM (verified correct)
#   - Nice FLO 12-bit: standard 1:2 OOK PWM (verified correct)
# ------------------------------------------------------------------
add_executable(test_subghz_protocol_decode
    test_subghz_protocol_decode.c
    ${M1_ROOT}/Sub_Ghz/protocols/m1_magellan_decode.c
    ${M1_ROOT}/Sub_Ghz/protocols/m1_came_decode.c
    ${M1_ROOT}/Sub_Ghz/protocols/m1_gate_tx_decode.c
    ${M1_ROOT}/Sub_Ghz/protocols/m1_nice_flo_decode.c
    ${M1_ROOT}/Sub_Ghz/protocols/m1_princeton_decode.c
    ${M1_ROOT}/Sub_Ghz/protocols/m1_holtek_decode.c
    ${M1_ROOT}/Sub_Ghz/protocols/m1_ansonic_decode.c
    ${M1_ROOT}/m1_csrc/bit_util.c
    ${STUBS_DIR}/subghz_decode_impl.c
)
target_include_directories(test_subghz_protocol_decode PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
    ${M1_ROOT}/Sub_Ghz
    ${M1_ROOT}/Sub_Ghz/protocols
)
target_link_libraries(test_subghz_protocol_decode PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_protocol_decode PRIVATE sanitizers)
endif()
add_test(NAME subghz_protocol_decode COMMAND test_subghz_protocol_decode)

# ------------------------------------------------------------------
# Module under test: firecracker_cm17a (40-bit CM17A home-automation decoder)
# Tests validate header/footer recognition, house-code/unit/command decode,
# and rejection of invalid or truncated packets.
# Uses a minimal registry stub (one FireCracker entry at index 0) to avoid
# linking all ~100 production decoders.
# ------------------------------------------------------------------
add_executable(test_firecracker_cm17a
    test_firecracker_cm17a.c
    ${M1_ROOT}/Sub_Ghz/protocols/m1_firecracker_cm17a_decode.c
    ${M1_ROOT}/Sub_Ghz/subghz_block_generic.c
    ${STUBS_DIR}/subghz_firecracker_test_registry.c
    ${STUBS_DIR}/subghz_decode_impl.c
)
target_include_directories(test_firecracker_cm17a PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
    ${M1_ROOT}/Sub_Ghz
    ${M1_ROOT}/Sub_Ghz/protocols
)
target_link_libraries(test_firecracker_cm17a PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_firecracker_cm17a PRIVATE sanitizers)
endif()
add_test(NAME firecracker_cm17a COMMAND test_firecracker_cm17a)


# Extracted from sub_ghz_replay_flipper_file() — handles signed→absolute
# conversion, zero skipping, and cross-buffer leftover digit recombination.
# Pure string parsing, zero hardware dependencies.
# ------------------------------------------------------------------
add_executable(test_subghz_raw_line_parser
    test_subghz_raw_line_parser.c
    ${M1_ROOT}/Sub_Ghz/subghz_raw_line_parser.c
)
target_include_directories(test_subghz_raw_line_parser PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_raw_line_parser PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_raw_line_parser PRIVATE sanitizers)
endif()
add_test(NAME subghz_raw_line_parser COMMAND test_subghz_raw_line_parser)

# ------------------------------------------------------------------
# Module under test: subghz_raw_decoder (offline RAW→protocol decode)
# Extracted from m1_subghz_scene_saved.c::do_decode_raw() — reconstructs
# pulse packets from raw timing data using gap detection, then tries
# protocol decoders via callback.  Pure logic, zero hardware deps.
# ------------------------------------------------------------------
add_executable(test_subghz_raw_decoder
    test_subghz_raw_decoder.c
    ${M1_ROOT}/Sub_Ghz/subghz_raw_decoder.c
)
target_include_directories(test_subghz_raw_decoder PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_raw_decoder PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_raw_decoder PRIVATE sanitizers)
endif()
add_test(NAME subghz_raw_decoder COMMAND test_subghz_raw_decoder)

# ------------------------------------------------------------------
# Module under test: subghz_registry_decode_try_fn
# Canonical SubGhzRawDecodeTryFn bridge extracted from
# subghz_decode_try_fn.c.  Tests cover:
#   - pulse_buf copy into subghz_decenc_ctl (pulse_times + npulsecount)
#   - all decoders are scanned when none match
#   - continue scanning when decode() succeeds but subghz_decenc_read() fails
#   - correct result fields when both decode() and decenc_read() succeed
#   - early return (no further scan) after first successful match
#   - NULL user_ctx is accepted
#
# The test file provides a 3-entry mini registry + controllable stubs
# for subghz_decenc_ctl and subghz_decenc_read instead of the full
# production registry, so each control-flow branch can be driven in isolation.
# ------------------------------------------------------------------
add_executable(test_subghz_decode_try_fn
    test_subghz_decode_try_fn.c
    ${M1_ROOT}/Sub_Ghz/subghz_decode_try_fn.c
)
target_include_directories(test_subghz_decode_try_fn PRIVATE
    ${STUBS_DIR}            # stm32h5xx_hal.h, m1_io_defs.h
    ${M1_ROOT}/m1_csrc
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_decode_try_fn PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_decode_try_fn PRIVATE sanitizers)
endif()
add_test(NAME subghz_decode_try_fn COMMAND test_subghz_decode_try_fn)

# ------------------------------------------------------------------
# Module under test: subghz_playlist_parser (Flipper→M1 path remapping)
# Extracted from m1_subghz_scene_playlist.c — converts Flipper-style
# paths (/ext/subghz/...) to M1 convention (/SUBGHZ/...).
# Pure string manipulation, zero hardware dependencies.
# ------------------------------------------------------------------
add_executable(test_subghz_playlist_parser
    test_subghz_playlist_parser.c
    ${M1_ROOT}/Sub_Ghz/subghz_playlist_parser.c
)
target_include_directories(test_subghz_playlist_parser PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_playlist_parser PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_playlist_parser PRIVATE sanitizers)
endif()
add_test(NAME subghz_playlist_parser COMMAND test_subghz_playlist_parser)

# ------------------------------------------------------------------
# Module under test: subghz_block_decoder (bit accumulation helpers)
# Header-only — tests the inline functions used by ALL protocol decoders
# for bit-by-bit data accumulation (64-bit and 128-bit), plus dedup hash.
# ------------------------------------------------------------------
add_executable(test_subghz_block_decoder
    test_subghz_block_decoder.c
)
target_include_directories(test_subghz_block_decoder PRIVATE
    ${M1_ROOT}/Sub_Ghz
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_subghz_block_decoder PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_block_decoder PRIVATE sanitizers)
endif()
add_test(NAME subghz_block_decoder COMMAND test_subghz_block_decoder)

# ------------------------------------------------------------------
# Module under test: subghz_block_encoder (bit-array + upload gen)
# Header-only — tests MSB-first bit-array set/get and NRZ-style upload
# generation from bit arrays.  Depends on level_duration + blocks_math.
# ------------------------------------------------------------------
add_executable(test_subghz_block_encoder
    test_subghz_block_encoder.c
)
target_include_directories(test_subghz_block_encoder PRIVATE
    ${M1_ROOT}/Sub_Ghz
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_subghz_block_encoder PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_block_encoder PRIVATE sanitizers)
endif()
add_test(NAME subghz_block_encoder COMMAND test_subghz_block_encoder)

# ------------------------------------------------------------------
# Module under test: Manchester codec (decoder + encoder state machines)
# Header-only — tests the table-driven Manchester decoder and the
# Manchester encoder used by Marantec, Somfy, FAAC and other protocols.
# Includes round-trip tests (encode→decode).
# ------------------------------------------------------------------
add_executable(test_subghz_manchester_codec
    test_subghz_manchester_codec.c
)
target_include_directories(test_subghz_manchester_codec PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_manchester_codec PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_manchester_codec PRIVATE sanitizers)
endif()
add_test(NAME subghz_manchester_codec COMMAND test_subghz_manchester_codec)

# ------------------------------------------------------------------
# Module under test: datatypes_utils (hex/bin/decimal conversions)
# Pure string/numeric conversions used in Sub-GHz parsing and UI.
# ------------------------------------------------------------------
add_executable(test_datatypes_utils
    test_datatypes_utils.c
    ${M1_ROOT}/Sub_Ghz/datatypes_utils.c
)
target_include_directories(test_datatypes_utils PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_datatypes_utils PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_datatypes_utils PRIVATE sanitizers)
endif()
add_test(NAME datatypes_utils COMMAND test_datatypes_utils)

# ------------------------------------------------------------------
# Module under test: menu layout helpers (m1_menu_item_h, m1_menu_max_visible)
# Constants and M1_MENU_VIS() come from the production m1_scene.h header.
# Function implementations are inlined in the test (m1_scene.c has
# heavy HAL/RTOS deps).
# ------------------------------------------------------------------
add_executable(test_menu_layout
    test_menu_layout.c
)
target_include_directories(test_menu_layout PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_menu_layout PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_menu_layout PRIVATE sanitizers)
endif()
add_test(NAME menu_layout COMMAND test_menu_layout)

# ------------------------------------------------------------------
# ------------------------------------------------------------------
# Module under test: esp_queue (linked-list queue for SPI AT transport)
# Bug context: esp_queue_get() freed the dequeued node BEFORE clearing
# q->rear, creating a use-after-free race when the SPI transport task
# called esp_queue_put() concurrently.  BLE spam's rapid AT command
# cycling amplified the race window, causing heap corruption and crash.
# FreeRTOS stubs are provided so the module compiles on the host.
# ------------------------------------------------------------------
add_executable(test_esp_queue
    test_esp_queue.c
    ${M1_ROOT}/Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_queue.c
)
target_include_directories(test_esp_queue PRIVATE
    ${STUBS_DIR}            # FreeRTOS.h, task.h (no-op vTaskSuspendAll/xTaskResumeAll)
    ${M1_ROOT}/Esp_spi_at/examples/at_spi_master/spi/stm32/main
)
target_link_libraries(test_esp_queue PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_esp_queue PRIVATE sanitizers)
endif()
add_test(NAME esp_queue COMMAND test_esp_queue)

# ------------------------------------------------------------------
# Module under test: flipper_rfid (RFID protocol name mapping)
# Tests flipper_rfid_parse_protocol() — maps Flipper .rfid protocol
# names to M1 LFRFIDProtocol enum values.  Covers all 30+ protocols
# including aliases and case-insensitive matching.
# Stubs: ff.h, lfrfid.h (protocol enum only), FreeRTOS.h, queue.h,
#        stream_buffer.h, stm32h5xx_hal.h
# ------------------------------------------------------------------
add_executable(test_flipper_rfid
    test_flipper_rfid.c
    ${M1_ROOT}/m1_csrc/flipper_rfid.c
    ${M1_ROOT}/m1_csrc/flipper_file.c
)
target_include_directories(test_flipper_rfid PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_flipper_rfid PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_flipper_rfid PRIVATE sanitizers)
endif()
add_test(NAME flipper_rfid COMMAND test_flipper_rfid)

# ------------------------------------------------------------------
# Module under test: flipper_ir (IR protocol mapping + hex conversion)
# Tests flipper_ir_proto_to_irmp() and flipper_ir_irmp_to_proto() —
# bidirectional mapping between Flipper IR names and IRMP protocol IDs.
# Stubs: ff.h, irmp.h (protocol constants only)
# ------------------------------------------------------------------
add_executable(test_flipper_ir
    test_flipper_ir.c
    ${M1_ROOT}/m1_csrc/flipper_ir.c
    ${M1_ROOT}/m1_csrc/flipper_file.c
)
target_include_directories(test_flipper_ir PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_flipper_ir PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_flipper_ir PRIVATE sanitizers)
endif()
add_test(NAME flipper_ir COMMAND test_flipper_ir)

# ------------------------------------------------------------------
# Module under test: flipper_nfc (NFC device type mapping)
# Tests flipper_nfc_parse_type() — maps ~60 Flipper NFC device type
# strings (NTAG, Mifare Classic, DESFire, FeliCa, ISO15693, SLIX,
# ST25TB) to flipper_nfc_type_t enum values.
# Stubs: ff.h
# ------------------------------------------------------------------
add_executable(test_flipper_nfc
    test_flipper_nfc.c
    ${M1_ROOT}/m1_csrc/flipper_nfc.c
    ${M1_ROOT}/m1_csrc/flipper_file.c
)
target_include_directories(test_flipper_nfc PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_flipper_nfc PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_flipper_nfc PRIVATE sanitizers)
endif()
add_test(NAME flipper_nfc COMMAND test_flipper_nfc)

# ------------------------------------------------------------------
# Module under test: lfrfid_manchester (LFRFID Manchester decoder)
# Tests manch_init(), manch_push_bit(), manch_get_bit(), manch_is_full(),
# and manch_feed_events() — the generic Manchester decoder used by
# EM4100, Viking, FDX-B, Electra, Gallagher, etc.
# Stubs: lfrfid.h (evt_t + FRAME_CHUNK_SIZE only), app_freertos.h,
#        cmsis_os.h, main.h
# The real lfrfid_manchester.c includes real lfrfid.h (same dir) which
# chains to lfrfid_hal.h needing TIM_HandleTypeDef — force-include
# the stm32h5xx_hal.h stub to provide it.
# ------------------------------------------------------------------
add_executable(test_lfrfid_manchester
    test_lfrfid_manchester.c
    ${M1_ROOT}/lfrfid/lfrfid_manchester.c
)
target_include_directories(test_lfrfid_manchester PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/lfrfid
)
target_compile_options(test_lfrfid_manchester PRIVATE
    -include ${STUBS_DIR}/stm32h5xx_hal.h
)
target_link_libraries(test_lfrfid_manchester PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_lfrfid_manchester PRIVATE sanitizers)
endif()
add_test(NAME lfrfid_manchester COMMAND test_lfrfid_manchester)

# ------------------------------------------------------------------
# Module under test: fw_source_asset_matches_filter (OTA filter logic)
# Tests the asset name filtering function from m1_fw_source.c.
# Pure string matching — include suffix + space-separated exclude suffixes.
# Uses a standalone copy of the function to avoid HAL/HTTP/FatFS deps.
# ------------------------------------------------------------------
add_executable(test_fw_source_filter
    test_fw_source_filter.c
    ${STUBS_DIR}/fw_source_filter_impl.c
)
target_include_directories(test_fw_source_filter PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_fw_source_filter PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_fw_source_filter PRIVATE sanitizers)
endif()
add_test(NAME fw_source_filter COMMAND test_fw_source_filter)

# ------------------------------------------------------------------
# Module under test: RPC CRC-16/CCITT (frame integrity)
# Tests the table-driven CRC-16 used by the M1 RPC binary protocol.
# Uses a standalone copy of the CRC functions from m1_rpc.c to avoid
# HAL/RTOS/USB dependencies.
# ------------------------------------------------------------------
add_executable(test_rpc_crc16
    test_rpc_crc16.c
    ${STUBS_DIR}/rpc_crc16_impl.c
)
target_include_directories(test_rpc_crc16 PRIVATE
    ${STUBS_DIR}
)
target_link_libraries(test_rpc_crc16 PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_rpc_crc16 PRIVATE sanitizers)
endif()
add_test(NAME rpc_crc16 COMMAND test_rpc_crc16)

# ------------------------------------------------------------------
# Module under test: RPC frame parser state machine
# Tests byte-at-a-time frame parsing, CRC validation, length checks,
# garbage rejection, and recovery after bad frames.
# Uses standalone copies of the CRC and parser state machine.
# ------------------------------------------------------------------
add_executable(test_rpc_parse
    test_rpc_parse.c
    ${STUBS_DIR}/rpc_parse_impl.c
    ${STUBS_DIR}/rpc_crc16_impl.c
)
target_include_directories(test_rpc_parse PRIVATE
    ${STUBS_DIR}
)
target_link_libraries(test_rpc_parse PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_rpc_parse PRIVATE sanitizers)
endif()
add_test(NAME rpc_parse COMMAND test_rpc_parse)

# ------------------------------------------------------------------
# Module under test: file browser utilities (sorting, extension, path)
# Tests pure-logic functions from m1_file_browser.c: case conversion,
# directory-first qsort comparator, extension matching, file type
# classification, and path concatenation.
# Uses standalone copies to avoid u8g2/FatFS/HAL dependencies.
# ------------------------------------------------------------------
add_executable(test_file_browser
    test_file_browser.c
    ${STUBS_DIR}/file_browser_impl.c
)
target_include_directories(test_file_browser PRIVATE
    ${STUBS_DIR}
)
target_link_libraries(test_file_browser PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_file_browser PRIVATE sanitizers)
endif()
add_test(NAME file_browser COMMAND test_file_browser)

# ------------------------------------------------------------------
# Module under test: http_client URL parser (parse_url)
# Tests URL decomposition into host, port, path, and is_https flag.
# The is_https flag is critical for OTA: it triggers the SSL connection
# path (AT+CIPSTART="SSL") which requires SNTP + CIPSSLCCONF setup.
# Uses a standalone copy (http_client_parse_impl.c) because the
# production m1_http_client.c has ESP32 SPI and HAL dependencies.
# Bug context: OTA download "Connection failed" — SSL handshake failed
# because tcp_connect() did not configure SNTP or SSL cert verification.
# ------------------------------------------------------------------
add_executable(test_http_client_parse
    test_http_client_parse.c
    ${STUBS_DIR}/http_client_parse_impl.c
)
target_include_directories(test_http_client_parse PRIVATE
    ${M1_ROOT}/m1_csrc
    ${STUBS_DIR}
)
target_link_libraries(test_http_client_parse PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_http_client_parse PRIVATE sanitizers)
endif()
add_test(NAME http_client_parse COMMAND test_http_client_parse)

# ------------------------------------------------------------------
# Module under test: settings_parse_hex_color
# Pure function — parses "#RRGGBB" / "RRGGBB" hex string into RGB bytes.
# Uses a standalone copy because m1_settings.c has FatFS/u8g2 dependencies.
# ------------------------------------------------------------------
add_executable(test_settings_hex_color
    test_settings_hex_color.c
    ${STUBS_DIR}/settings_hex_color_extract.c
)
target_include_directories(test_settings_hex_color PRIVATE
    ${M1_ROOT}/m1_csrc
    ${STUBS_DIR}
)
target_link_libraries(test_settings_hex_color PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_settings_hex_color PRIVATE sanitizers)
endif()
add_test(NAME settings_hex_color COMMAND test_settings_hex_color)

# ------------------------------------------------------------------
# Module under test: m1_led_color_ease
# Pure function — Gaussian drop-off easing of RGB channels by battery level.
# Compiles directly from production source (no hardware dependencies).
# ------------------------------------------------------------------
add_executable(test_led_color_ease
    test_led_color_ease.c
    ${M1_ROOT}/m1_csrc/m1_led_color.c
)
target_include_directories(test_led_color_ease PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_led_color_ease PRIVATE unity)
if(NOT MSVC)
    target_link_libraries(test_led_color_ease PRIVATE m)
endif()
if(TARGET sanitizers)
    target_link_libraries(test_led_color_ease PRIVATE sanitizers)
endif()
add_test(NAME led_color_ease COMMAND test_led_color_ease)

# ------------------------------------------------------------------
# Module under test: m1_rgb_backlight
# Pure helpers — HSV/RGB conversion and animation math.
# ------------------------------------------------------------------
add_executable(test_rgb_backlight
    test_rgb_backlight.c
    ${M1_ROOT}/m1_csrc/m1_rgb_backlight.c
)
target_include_directories(test_rgb_backlight PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_rgb_backlight PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_rgb_backlight PRIVATE sanitizers)
endif()
add_test(NAME rgb_backlight COMMAND test_rgb_backlight)

# ------------------------------------------------------------------
# Module under test: ir_signal_record (IR Universal Remote pure-logic helpers)
# Extracted from m1_ir_universal.c (Phase F) — zero HAL/RTOS/FatFS deps.
# Covers:
#   ir_map_flipper_protocol()  — Flipper protocol name → IRMP protocol ID
#   ir_is_ir_file()            — ".ir" extension check
#   ir_path_append()           — path component append with buffer-size guard
#   ir_path_go_up()            — navigate one level up in a path
#   ir_str_contains_icase()    — case-insensitive substring search
# Stubs: irmp.h (protocol constants only)
# ------------------------------------------------------------------
add_executable(test_ir_signal_record
    test_ir_signal_record.c
    ${M1_ROOT}/m1_csrc/ir_signal_record.c
)
target_include_directories(test_ir_signal_record PRIVATE
    ${M1_ROOT}/m1_csrc
    ${STUBS_DIR}
)
target_link_libraries(test_ir_signal_record PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_ir_signal_record PRIVATE sanitizers)
endif()
add_test(NAME ir_signal_record COMMAND test_ir_signal_record)

# ------------------------------------------------------------------
# Module under test: ir_str_contains_icase (IR Universal Remote search helper)
# Now backed by the extracted ir_signal_record module (Phase F).
# ------------------------------------------------------------------
add_executable(test_ir_search
    test_ir_search.c
    ${M1_ROOT}/m1_csrc/ir_signal_record.c
)
target_include_directories(test_ir_search PRIVATE
    ${M1_ROOT}/m1_csrc
    ${STUBS_DIR}
)
target_link_libraries(test_ir_search PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_ir_search PRIVATE sanitizers)
endif()
add_test(NAME ir_search COMMAND test_ir_search)

# ------------------------------------------------------------------
# Module under test: ir_cmd_parse (Phase G — KV-reader vtable abstraction)
# Decouples parse_ir_signal_block() from FatFS via ir_block_reader_t.
# No FatFS or HAL stubs needed — tests use a string-backed reader.
# ------------------------------------------------------------------
add_executable(test_ir_cmd_parse
    test_ir_cmd_parse.c
    ${M1_ROOT}/m1_csrc/ir_signal_record.c
)
target_include_directories(test_ir_cmd_parse PRIVATE
    ${M1_ROOT}/m1_csrc
    ${STUBS_DIR}
)
target_link_libraries(test_ir_cmd_parse PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_ir_cmd_parse PRIVATE sanitizers)
endif()
add_test(NAME ir_cmd_parse COMMAND test_ir_cmd_parse)

# ------------------------------------------------------------------
# Module under test: system_dashboard
# Pure-logic helpers — uptime formatting, SD status text.
# Compiles m1_system_dashboard_helpers.c directly (no HAL dependencies).
# ------------------------------------------------------------------
add_executable(test_system_dashboard
    test_system_dashboard.c
    ${M1_ROOT}/m1_csrc/m1_system_dashboard_helpers.c
)
target_include_directories(test_system_dashboard PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_system_dashboard PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_system_dashboard PRIVATE sanitizers)
endif()
add_test(NAME system_dashboard COMMAND test_system_dashboard)
# Module under test: hex_viewer (format_row + ascii_preview)
# Pure functions — hex/ASCII formatting with no hardware dependencies.
# ------------------------------------------------------------------
add_executable(test_hex_viewer
    test_hex_viewer.c
    ${M1_ROOT}/m1_csrc/hex_viewer.c
)
target_include_directories(test_hex_viewer PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_hex_viewer PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_hex_viewer PRIVATE sanitizers)
endif()
add_test(NAME hex_viewer COMMAND test_hex_viewer)
# Module under test: nfc_poller (mfc_is_classic_sak)
# Pure function — SAK-to-MIFARE-Classic classifier.
# Compiles the production nfc_poller_helpers.c directly (single source of truth).
# ------------------------------------------------------------------
add_executable(test_nfc_poller
    test_nfc_poller.c
    ${M1_ROOT}/NFC/NFC_drv/legacy/nfc_poller_helpers.c
)
target_include_directories(test_nfc_poller PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/NFC/NFC_drv/legacy
)
target_link_libraries(test_nfc_poller PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_nfc_poller PRIVATE sanitizers)
endif()
add_test(NAME nfc_poller COMMAND test_nfc_poller)
# Module under test: m1_clock_util
# Pure functions — leap year, days-in-month, timezone offset.
# Compiles directly from production source (no hardware dependencies).
# ------------------------------------------------------------------
add_executable(test_clock
    test_clock.c
    ${M1_ROOT}/m1_csrc/m1_clock_util.c
)
target_include_directories(test_clock PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_clock PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_clock PRIVATE sanitizers)
endif()
add_test(NAME clock COMMAND test_clock)
# ------------------------------------------------------------------
# Module under test: subghz_secplus_v1_encoder
# Pure-logic ternary OOK packet encoder for Security+ 1.0 brute force.
# ------------------------------------------------------------------
add_executable(test_subghz_secplus_v1
    test_subghz_secplus_v1.c
    ${M1_ROOT}/Sub_Ghz/subghz_secplus_v1_encoder.c
    ${M1_ROOT}/m1_csrc/bit_util.c
)
target_include_directories(test_subghz_secplus_v1 PRIVATE
    ${M1_ROOT}/Sub_Ghz
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_subghz_secplus_v1 PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_secplus_v1 PRIVATE sanitizers)
endif()
add_test(NAME subghz_secplus_v1 COMMAND test_subghz_secplus_v1)
# ------------------------------------------------------------------
# Module under test: m1_esl_proto (ESL frame builder + barcode decoder)
# Pure logic — CRC16, barcode-to-PLID, frame construction.  Zero HAL
# dependencies; compiles directly on the host.
# ------------------------------------------------------------------
add_executable(test_esl_proto
    test_esl_proto.c
    ${M1_ROOT}/m1_csrc/m1_esl_proto.c
)
target_include_directories(test_esl_proto PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_esl_proto PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_esl_proto PRIVATE sanitizers)
endif()
add_test(NAME esl_proto COMMAND test_esl_proto)

# ------------------------------------------------------------------
# Module under test: cc1101_fec (CC1101 FEC encode / decode utility)
# Implements the rate-1/2 Viterbi convolutional codec + 4-byte block
# interleaver used by the TI CC1101 radio when FEC is enabled.
# Pure logic — only <stdint.h> and <string.h>; zero hardware dependencies.
# Tests: known encode/decode vector, encode→decode roundtrips, size helpers,
#        CRC-16/IBM, NULL-argument guards, edge cases.
# ------------------------------------------------------------------
add_executable(test_cc1101_fec
    test_cc1101_fec.c
    ${M1_ROOT}/Sub_Ghz/cc1101_fec.c
)
target_include_directories(test_cc1101_fec PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_cc1101_fec PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_cc1101_fec PRIVATE sanitizers)
endif()
add_test(NAME cc1101_fec COMMAND test_cc1101_fec)
# ------------------------------------------------------------------
# Module under test: subghz_keeloq (cipher + keystore + encoder)
# KeeLoq 528-round NLFSR cipher, normal/simple learning derivation,
# counter-mode hop increment, manufacturer key store (ff.h stubbed),
# and OOK PWM packet encoder for counter-mode replay.
# Pure logic — no HAL dependencies.
# ------------------------------------------------------------------
add_executable(test_subghz_keeloq
    test_subghz_keeloq.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq_mfkeys.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq_mfkeys_builtin.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq_encoder.c
    ${M1_ROOT}/m1_csrc/m1_crypto.c
    ${STUBS_DIR}/m1_crypto_stub.c
)
target_include_directories(test_subghz_keeloq PRIVATE
    ${M1_ROOT}/Sub_Ghz
    ${M1_ROOT}/m1_csrc
    ${STUBS_DIR}
)
target_compile_definitions(test_subghz_keeloq PRIVATE
    M1_CRYPTO_SKIP_UID_FUNCTIONS
)
target_link_libraries(test_subghz_keeloq PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_keeloq PRIVATE sanitizers)
endif()
add_test(NAME subghz_keeloq COMMAND test_subghz_keeloq)

# ------------------------------------------------------------------
# Module under test: keeloq_mfkeys_enc (encrypted keystore)
# Tests for keeloq_mfkeys_save_encrypted(), keeloq_mfkeys_load_encrypted(),
# and the auto-migration path in keeloq_mfkeys_load().
#
# m1_crypto.c is compiled with -DM1_CRYPTO_SKIP_UID_FUNCTIONS so the
# UID register accesses and HAL_GetTick() calls are excluded.
# m1_crypto_stub.c supplies deterministic replacements.
#
# File paths are overridden via compile definitions so that tests use
# /tmp/ rather than the production "0:/SUBGHZ/" SD card paths.
# ------------------------------------------------------------------
add_executable(test_keeloq_mfkeys_enc
    test_keeloq_mfkeys_enc.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq_mfkeys.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq_mfkeys_builtin.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq.c
    ${M1_ROOT}/m1_csrc/m1_crypto.c
    ${STUBS_DIR}/m1_crypto_stub.c
)
target_include_directories(test_keeloq_mfkeys_enc PRIVATE
    ${M1_ROOT}/Sub_Ghz
    ${M1_ROOT}/m1_csrc
    ${STUBS_DIR}
)
target_compile_definitions(test_keeloq_mfkeys_enc PRIVATE
    M1_CRYPTO_SKIP_UID_FUNCTIONS
    KEELOQ_MFKEYS_PATH="/tmp/keeloq_mfcodes_test"
    KEELOQ_MFKEYS_ENC_PATH="/tmp/keeloq_mfcodes_test.enc"
)
target_link_libraries(test_keeloq_mfkeys_enc PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_keeloq_mfkeys_enc PRIVATE sanitizers)
endif()
add_test(NAME keeloq_mfkeys_enc COMMAND test_keeloq_mfkeys_enc)

# ------------------------------------------------------------------
# Module under test: subghz_new_remote_gen (Bind New Remote key gen)
# Pure-logic random remote key generator: given a seed, produces a
# valid key, protocol name, frequency, bit count, TE, and filename
# base for each of the five OOK-PWM-replayable rolling-code protocols
# supported by the Bind New Remote wizard.
# Zero hardware dependencies — splitmix64 PRNG only.
# ------------------------------------------------------------------
add_executable(test_subghz_new_remote_gen
    test_subghz_new_remote_gen.c
    ${M1_ROOT}/Sub_Ghz/subghz_new_remote_gen.c
)
target_include_directories(test_subghz_new_remote_gen PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_new_remote_gen PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_new_remote_gen PRIVATE sanitizers)
endif()
add_test(NAME subghz_new_remote_gen COMMAND test_subghz_new_remote_gen)

# ------------------------------------------------------------------
# Module under test: subghz_create_proto (Phase 8a — create-from-
# scratch protocol catalog & field-schema descriptor).
# Pure-logic protocol catalog + editable-field bitmask + key-range
# masking helper.  Zero hardware dependencies.
# ------------------------------------------------------------------
add_executable(test_subghz_create_proto
    test_subghz_create_proto.c
    ${M1_ROOT}/Sub_Ghz/subghz_create_proto.c
)
target_include_directories(test_subghz_create_proto PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_create_proto PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_create_proto PRIVATE sanitizers)
endif()
add_test(NAME subghz_create_proto COMMAND test_subghz_create_proto)

# ------------------------------------------------------------------
# Module under test: subghz_hex_editor (Phase 8b-3 — SetKey scene
# backing model).  Pure-logic hex-digit editor with cursor + UP/DOWN
# cycling; reused by Phase 8c SetSerial / SetButton / SetCounter
# editor scenes.  Zero hardware dependencies.
# ------------------------------------------------------------------
add_executable(test_subghz_hex_editor
    test_subghz_hex_editor.c
    ${M1_ROOT}/Sub_Ghz/subghz_hex_editor.c
)
target_include_directories(test_subghz_hex_editor PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_hex_editor PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_hex_editor PRIVATE sanitizers)
endif()
add_test(NAME subghz_hex_editor COMMAND test_subghz_hex_editor)

# ------------------------------------------------------------------
# Module under test: subghz_keeloq_create (Phase 8c-3 — KeeLoq
# Create-from-scratch key assembler).  Pure-logic module that builds
# the 64-bit Flipper-format key from user-entered serial / button /
# counter and a picked manufacturer master key.  Depends on the
# subghz_keeloq cipher + learning primitives.  Zero hardware
# dependencies.
# ------------------------------------------------------------------
add_executable(test_subghz_keeloq_create
    test_subghz_keeloq_create.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq_create.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq.c
)
target_include_directories(test_subghz_keeloq_create PRIVATE
    ${M1_ROOT}/Sub_Ghz
    ${STUBS_DIR}
)
target_link_libraries(test_subghz_keeloq_create PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_keeloq_create PRIVATE sanitizers)
endif()
add_test(NAME subghz_keeloq_create COMMAND test_subghz_keeloq_create)

# ------------------------------------------------------------------
# Module under test: subghz_signal_fields (Phase 9a-1 — KeeLoq-family
# {serial, button, encrypted-hop} extract / assemble for the
# SignalSettings scene).  Pure-logic module; depends on
# subghz_keeloq_create (and through it subghz_keeloq) only for the
# cross-check tests.  Zero hardware dependencies.
# ------------------------------------------------------------------
add_executable(test_subghz_signal_fields
    test_subghz_signal_fields.c
    ${M1_ROOT}/Sub_Ghz/subghz_signal_fields.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq_create.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq.c
)
target_include_directories(test_subghz_signal_fields PRIVATE
    ${M1_ROOT}/Sub_Ghz
    ${STUBS_DIR}
)
target_link_libraries(test_subghz_signal_fields PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_signal_fields PRIVATE sanitizers)
endif()
add_test(NAME subghz_signal_fields COMMAND test_subghz_signal_fields)

# ------------------------------------------------------------------
# Module under test: subghz_signal_format (Phase 11-1)
# Polymorphic Info-screen renderers driven by the protocol registry's
# .get_string vtable slot.  Pure-logic; zero hardware dependencies.
# ------------------------------------------------------------------
add_executable(test_subghz_signal_format
    test_subghz_signal_format.c
    ${M1_ROOT}/Sub_Ghz/subghz_signal_format.c
    ${M1_ROOT}/Sub_Ghz/subghz_signal_fields.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq.c
)
target_include_directories(test_subghz_signal_format PRIVATE
    ${M1_ROOT}/Sub_Ghz
    ${M1_ROOT}/m1_csrc
    ${STUBS_DIR}
)
target_link_libraries(test_subghz_signal_format PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_signal_format PRIVATE sanitizers)
endif()
add_test(NAME subghz_signal_format COMMAND test_subghz_signal_format)

# ------------------------------------------------------------------
# Module under test: subghz_read_raw_state (Read Raw async TX helpers)
# Verifies the pure-logic dispatch helpers used by the Read Raw scene
# to handle Q_EVENT_SUBGHZ_TX completion events and BACK during TX:
#   - subghz_read_raw_state_is_tx()    classifies the four async TX states
#   - subghz_read_raw_state_after_tx() picks the post-TX return state
# These helpers gate the async TX driver (sub_ghz_replay_start_async/
# continue_async/abort) in the Read Raw scene.
# ------------------------------------------------------------------
add_executable(test_subghz_read_raw_state
    test_subghz_read_raw_state.c
)
target_include_directories(test_subghz_read_raw_state PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_subghz_read_raw_state PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_read_raw_state PRIVATE sanitizers)
endif()
add_test(NAME subghz_read_raw_state COMMAND test_subghz_read_raw_state)

# ------------------------------------------------------------------
# Source invariant: async replay abort must preserve the shared main
# queue, while the private blocking replay wrapper must still flush it
# before returning to Saved / Playlist / Remote / Bind Wizard callers.
# ------------------------------------------------------------------
add_executable(test_subghz_blocking_replay_contract
    test_subghz_blocking_replay_contract.c
)
target_compile_definitions(test_subghz_blocking_replay_contract PRIVATE
    M1_ROOT="${M1_ROOT}"
)
target_link_libraries(test_subghz_blocking_replay_contract PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_blocking_replay_contract PRIVATE sanitizers)
endif()
add_test(NAME subghz_blocking_replay_contract COMMAND test_subghz_blocking_replay_contract)

# ------------------------------------------------------------------
# Module under test: keeloq_mfkeys_builtin (build-time key embedding)
# Verifies that keeloq_mfkeys_load() uses compiled-in builtin keys
# (Flipper parity) and does NOT fall back to the SD card when builtin
# data is present.
#
# This test links with keeloq_builtin_test_data.c (a populated stub)
# instead of subghz_keeloq_mfkeys_builtin.c (the NULL stub in the
# public repo).  SD card paths are redirected to /tmp/ so the test
# can confirm the SD is not consulted when builtin is non-empty.
# ------------------------------------------------------------------
add_executable(test_keeloq_mfkeys_builtin
    test_keeloq_mfkeys_builtin.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq_mfkeys.c
    ${M1_ROOT}/Sub_Ghz/subghz_keeloq.c
    ${M1_ROOT}/m1_csrc/m1_crypto.c
    ${STUBS_DIR}/m1_crypto_stub.c
    ${STUBS_DIR}/keeloq_builtin_test_data.c
)
target_include_directories(test_keeloq_mfkeys_builtin PRIVATE
    ${M1_ROOT}/Sub_Ghz
    ${M1_ROOT}/m1_csrc
    ${STUBS_DIR}
)
target_compile_definitions(test_keeloq_mfkeys_builtin PRIVATE
    M1_CRYPTO_SKIP_UID_FUNCTIONS
    KEELOQ_MFKEYS_PATH="/tmp/keeloq_builtin_sd_test"
    KEELOQ_MFKEYS_ENC_PATH="/tmp/keeloq_builtin_sd_test.enc"
)
target_link_libraries(test_keeloq_mfkeys_builtin PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_keeloq_mfkeys_builtin PRIVATE sanitizers)
endif()
add_test(NAME keeloq_mfkeys_builtin COMMAND test_keeloq_mfkeys_builtin)

# ── BadUSB DuckyScript parser ──
add_executable(test_badusb_parser
    test_badusb_parser.c
    ${M1_ROOT}/m1_csrc/badusb_parser.c
)
target_include_directories(test_badusb_parser PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_badusb_parser PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_badusb_parser PRIVATE sanitizers)
endif()
add_test(NAME badusb_parser COMMAND test_badusb_parser)

# ── Sub-GHz autosave logic ──
add_executable(test_subghz_autosave
    test_subghz_autosave.c
    ${M1_ROOT}/m1_csrc/subghz_autosave.c
)
target_include_directories(test_subghz_autosave PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_subghz_autosave PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_autosave PRIVATE sanitizers)
endif()
add_test(NAME subghz_autosave COMMAND test_subghz_autosave)

# ── NFC NDEF encoder ──
add_executable(test_nfc_ndef_encode
    test_nfc_ndef_encode.c
    ${M1_ROOT}/m1_csrc/nfc_ndef_encode.c
)
target_include_directories(test_nfc_ndef_encode PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_nfc_ndef_encode PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_nfc_ndef_encode PRIVATE sanitizers)
endif()
add_test(NAME nfc_ndef_encode COMMAND test_nfc_ndef_encode)

# ------------------------------------------------------------------
# Module under test: nfc_card_info (manufacturer lookup, SAK classifier, UID helpers)
#
# Pure-logic functions extracted from m1_nfc.c with zero HAL/RTOS/display deps:
#   nfc_manufacturer_name()  — UID byte 0 → ISO/IEC 7816-6 manufacturer string
#   nfc_sak_type_str()       — SAK + ATQA → NFC-A card-type string
#   nfc_uid_fmt()            — raw UID bytes → "AA BB …" hex string
#   nfc_uid_step()           — UID big-endian increment / decrement (fuzzer helper)
# ------------------------------------------------------------------
add_executable(test_nfc_card_info
    test_nfc_card_info.c
    ${M1_ROOT}/m1_csrc/nfc_card_info.c
)
target_include_directories(test_nfc_card_info PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_nfc_card_info PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_nfc_card_info PRIVATE sanitizers)
endif()
add_test(NAME nfc_card_info COMMAND test_nfc_card_info)


add_executable(test_file_util
    test_file_util.c
    ${M1_ROOT}/m1_csrc/m1_file_util.c
)
target_include_directories(test_file_util PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_file_util PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_file_util PRIVATE sanitizers)
endif()
add_test(NAME file_util COMMAND test_file_util)

# ── Crypto AES-256-CBC round-trip ──
add_executable(test_crypto
    test_crypto.c
    ${M1_ROOT}/m1_csrc/m1_crypto.c
)
target_compile_definitions(test_crypto PRIVATE M1_CRYPTO_SKIP_UID_FUNCTIONS)
target_include_directories(test_crypto PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_crypto PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_crypto PRIVATE sanitizers)
endif()
add_test(NAME crypto COMMAND test_crypto)

# ── Music parser (FMF note parsing) ──
add_executable(test_music_parser
    test_music_parser.c
    ${M1_ROOT}/m1_csrc/music_parser.c
)
target_include_directories(test_music_parser PRIVATE
    ${STUBS_DIR}
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_music_parser PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_music_parser PRIVATE sanitizers)
endif()
add_test(NAME music_parser COMMAND test_music_parser)

# ── Sub-GHz TX/RX lifecycle state machine (Phase 1 of Momentum parity work) ──
add_executable(test_subghz_txrx_state
    test_subghz_txrx_state.c
    ${M1_ROOT}/Sub_Ghz/subghz_txrx_state.c
)
target_include_directories(test_subghz_txrx_state PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_txrx_state PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_txrx_state PRIVATE sanitizers)
endif()
add_test(NAME subghz_txrx_state COMMAND test_subghz_txrx_state)

# ── Per-scene state slots (Phase 2 of Momentum parity work) ──
add_executable(test_subghz_scene_state
    test_subghz_scene_state.c
    ${M1_ROOT}/Sub_Ghz/subghz_scene_state.c
)
target_include_directories(test_subghz_scene_state PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_scene_state PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_scene_state PRIVATE sanitizers)
endif()
add_test(NAME subghz_scene_state COMMAND test_subghz_scene_state)

# ── Submenu model (Phase 7a of Momentum parity work) ──
add_executable(test_subghz_submenu_model
    test_subghz_submenu_model.c
    ${M1_ROOT}/Sub_Ghz/subghz_submenu_model.c
)
target_include_directories(test_subghz_submenu_model PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_submenu_model PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_submenu_model PRIVATE sanitizers)
endif()
add_test(NAME subghz_submenu_model COMMAND test_subghz_submenu_model)

# ── TX repeat / endless-hold policy (Phase 3a of Momentum parity work) ──
add_executable(test_subghz_endless_tx
    test_subghz_endless_tx.c
    ${M1_ROOT}/Sub_Ghz/subghz_endless_tx.c
)
target_include_directories(test_subghz_endless_tx PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_endless_tx PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_endless_tx PRIVATE sanitizers)
endif()
add_test(NAME subghz_endless_tx COMMAND test_subghz_endless_tx)

# ----------------------------------------------------------------------------
# Sub-GHz scene manager polish primitives (Phase 10 — Momentum parity)
# ----------------------------------------------------------------------------
add_executable(test_subghz_scene_polish
    test_subghz_scene_polish.c
    ${M1_ROOT}/Sub_Ghz/subghz_scene_polish.c
)
target_include_directories(test_subghz_scene_polish PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_scene_polish PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_scene_polish PRIVATE sanitizers)
endif()
add_test(NAME subghz_scene_polish COMMAND test_subghz_scene_polish)

add_executable(test_subghz_transmitter_ctl
    test_subghz_transmitter_ctl.c
    ${M1_ROOT}/Sub_Ghz/subghz_transmitter_ctl.c
    ${M1_ROOT}/Sub_Ghz/subghz_endless_tx.c
)
target_include_directories(test_subghz_transmitter_ctl PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_transmitter_ctl PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_transmitter_ctl PRIVATE sanitizers)
endif()
add_test(NAME subghz_transmitter_ctl COMMAND test_subghz_transmitter_ctl)

# ------------------------------------------------------------------
# Sub-GHz button-cycling capability lookup (Phase 4a)
# ------------------------------------------------------------------
add_executable(test_subghz_button_caps
    test_subghz_button_caps.c
    ${M1_ROOT}/Sub_Ghz/subghz_button_caps.c
)
target_include_directories(test_subghz_button_caps PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_button_caps PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_button_caps PRIVATE sanitizers)
endif()
add_test(NAME subghz_button_caps COMMAND test_subghz_button_caps)

# ------------------------------------------------------------------
# Sub-GHz button-override key mutation (Phase 4c)
# ------------------------------------------------------------------
add_executable(test_subghz_button_override
    test_subghz_button_override.c
    ${M1_ROOT}/Sub_Ghz/subghz_button_override.c
)
target_include_directories(test_subghz_button_override PRIVATE
    ${M1_ROOT}/Sub_Ghz
)
target_link_libraries(test_subghz_button_override PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_subghz_button_override PRIVATE sanitizers)
endif()
add_test(NAME subghz_button_override COMMAND test_subghz_button_override)

# ------------------------------------------------------------------
# Module under test: memmgr (Momentum-style heap redirect shim)
#
# On the host the pvPort* macros map to libc, so the test exercises
# the __wrap_ delegation path (malloc/free/calloc/realloc + _r variants).
# ------------------------------------------------------------------
add_executable(test_memmgr
    test_memmgr.c
    ${M1_ROOT}/Core/Src/memmgr.c
)
target_include_directories(test_memmgr PRIVATE
    ${M1_ROOT}/Core/Inc
    stubs
)
target_link_libraries(test_memmgr PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_memmgr PRIVATE sanitizers)
endif()
add_test(NAME memmgr COMMAND test_memmgr)
# Allow ASan to return NULL for oversized allocations rather than aborting —
# this lets test_wrap_malloc_oom_returns_null verify the NULL-propagation path
# (AP-2 anti-pattern check) without triggering an allocator abort.
set_tests_properties(memmgr PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=allocator_may_return_null=1"
)

# ------------------------------------------------------------------
# ELF loader allocation budget (compile-time checks on budget constant)
# ------------------------------------------------------------------
add_executable(test_elf_alloc_budget
    test_elf_alloc_budget.c
)
target_include_directories(test_elf_alloc_budget PRIVATE
    ${M1_ROOT}/m1_csrc
    stubs
)
target_link_libraries(test_elf_alloc_budget PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_elf_alloc_budget PRIVATE sanitizers)
endif()
add_test(NAME elf_alloc_budget COMMAND test_elf_alloc_budget)

# ── WiFi AP record pure-logic extraction (Phase A — Firmware-wide Momentum parity) ──
add_executable(test_wifi_ap_record
    test_wifi_ap_record.c
    ${M1_ROOT}/m1_csrc/wifi_ap_record.c
)
target_include_directories(test_wifi_ap_record PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_wifi_ap_record PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_wifi_ap_record PRIVATE sanitizers)
endif()
add_test(NAME wifi_ap_record COMMAND test_wifi_ap_record)

