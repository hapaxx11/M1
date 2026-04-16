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
# Stubs directory — provides minimal HAL/FatFS/RTOS stubs so firmware
# source files compile on the host without hardware dependencies.
# ------------------------------------------------------------------
set(STUBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/stubs")

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
# Module under test: subghz_protocol_registry (lookup functions)
# Stubs needed: all decoder functions + HAL stubs
# Bug context: .sub file replay used fragile strstr() chains —
# Cham_Code, Marantec24, and all new static protocols were missed.
# ------------------------------------------------------------------
add_executable(test_subghz_registry
    test_subghz_registry.c
    ${M1_ROOT}/Sub_Ghz/subghz_protocol_registry.c
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
# Module under test: subghz_raw_line_parser (RAW_Data line parsing)
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
# Pure function — linear interpolation of RGB channels by battery level.
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
if(TARGET sanitizers)
    target_link_libraries(test_led_color_ease PRIVATE sanitizers)
endif()
add_test(NAME led_color_ease COMMAND test_led_color_ease)

# ------------------------------------------------------------------
# Module under test: str_contains_icase (IR Universal Remote search helper)
# Pure function — case-insensitive substring search used by IRDB search.
# Uses a standalone copy because m1_ir_universal.c has HAL/RTOS dependencies.
# ------------------------------------------------------------------
add_executable(test_ir_search
    test_ir_search.c
    ${STUBS_DIR}/ir_search_impl.c
)
target_include_directories(test_ir_search PRIVATE
    ${M1_ROOT}/m1_csrc
)
target_link_libraries(test_ir_search PRIVATE unity)
if(TARGET sanitizers)
    target_link_libraries(test_ir_search PRIVATE sanitizers)
endif()
add_test(NAME ir_search COMMAND test_ir_search)

# ------------------------------------------------------------------
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
