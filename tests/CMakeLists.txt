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
