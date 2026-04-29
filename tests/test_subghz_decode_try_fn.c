/* SPDX-License-Identifier: GPL-2.0-or-later */
/* See COPYING.txt for license details. */

/*
 * test_subghz_decode_try_fn.c
 *
 * Unit tests for subghz_registry_decode_try_fn() — the canonical bridge
 * between subghz_decode_raw_offline() and the global protocol registry.
 *
 * Strategy
 * --------
 * subghz_registry_decode_try_fn() has three external dependencies:
 *   1. subghz_protocol_registry[]  — the global registry array
 *   2. subghz_decenc_ctl           — the global decode-state struct
 *   3. subghz_decenc_read()        — reads decoded result after a decode call
 *
 * This test file provides:
 *   - A minimal 3-entry registry (fail / succeed-then-decenc_fail / always-fail)
 *     so we can exercise every branch in the function.
 *   - subghz_decenc_ctl as a zero-initialised global.
 *   - A controllable subghz_decenc_read() whose return value and output are
 *     set per-test via g_decenc_read_result / g_decenc_read_info.
 *   - Decoder stubs that check a per-index g_decoder_result[] flag.
 *
 * Tests cover
 * -----------
 *   1. Pulse copy — pulse_buf contents appear in subghz_decenc_ctl.pulse_times
 *      and npulsecount is set, even when no decoder matches.
 *   2. No match — all decoders fail → function returns false.
 *   3. Decode match, decenc_read fails → function continues to next decoder,
 *      eventually returns false (no other decoder matches).
 *   4. Decode match, decenc_read succeeds → function returns true with the
 *      correct fields copied from the SubGHz_Dec_Info_t.
 *   5. First decoder matches and succeeds → returns true immediately without
 *      scanning the rest of the registry.
 *   6. user_ctx is ignored → NULL user_ctx is accepted without crash.
 *
 * Build:
 *   cmake -B tests/build-tests -S tests && cmake --build tests/build-tests
 *   ctest --test-dir tests/build-tests -R subghz_decode_try_fn --output-on-failure
 */

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "unity.h"
#include "subghz_protocol_registry.h"
#include "m1_sub_ghz_decenc.h"
#include "subghz_raw_decoder.h"

/*============================================================================*/
/* Controllable test state                                                    */
/*============================================================================*/

/* Result that each decoder stub returns.  0 = success, 1 = failure. */
static int g_decoder_result[3];

/* Controls what subghz_decenc_read() returns and fills in. */
static bool g_decenc_read_result;
static SubGHz_Dec_Info_t g_decenc_read_info;

/* Track how many times each decoder was called. */
static int g_decoder_call_count[3];

static void reset_state(void)
{
    memset(g_decoder_result, 1, sizeof(g_decoder_result));  /* all fail */
    g_decenc_read_result = false;
    memset(&g_decenc_read_info, 0, sizeof(g_decenc_read_info));
    memset(g_decoder_call_count, 0, sizeof(g_decoder_call_count));
    memset(&subghz_decenc_ctl, 0, sizeof(subghz_decenc_ctl));
}

/*============================================================================*/
/* Required by subghz_decode_try_fn.c                                        */
/*============================================================================*/

SubGHz_DecEnc_t subghz_decenc_ctl;

bool subghz_decenc_read(SubGHz_Dec_Info_t *out, bool raw)
{
    (void)raw;
    if (g_decenc_read_result && out)
        *out = g_decenc_read_info;
    return g_decenc_read_result;
}

uint16_t get_diff(uint16_t a, uint16_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

const SubGHz_Weather_Data_t *subghz_get_weather_data(void) { return NULL; }

/*============================================================================*/
/* Minimal 3-entry test registry                                             */
/*============================================================================*/

static uint8_t test_decode_0(uint16_t p, uint16_t n) { (void)p; (void)n; g_decoder_call_count[0]++; return (uint8_t)g_decoder_result[0]; }
static uint8_t test_decode_1(uint16_t p, uint16_t n) { (void)p; (void)n; g_decoder_call_count[1]++; return (uint8_t)g_decoder_result[1]; }
static uint8_t test_decode_2(uint16_t p, uint16_t n) { (void)p; (void)n; g_decoder_call_count[2]++; return (uint8_t)g_decoder_result[2]; }

const SubGhzProtocolDef subghz_protocol_registry[] = {
    [0] = { .name = "TestProto0", .type = SubGhzProtocolTypeStatic,
            .timing = { .te_short = 350, .te_long = 700, .te_delta = 100,
                        .min_count_bit_for_found = 24 },
            .decode = test_decode_0,
            .flags = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_Send },
    [1] = { .name = "TestProto1", .type = SubGhzProtocolTypeStatic,
            .timing = { .te_short = 500, .te_long = 1000, .te_delta = 100,
                        .min_count_bit_for_found = 24 },
            .decode = test_decode_1,
            .flags = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_Send },
    [2] = { .name = "TestProto2", .type = SubGhzProtocolTypeStatic,
            .timing = { .te_short = 600, .te_long = 1200, .te_delta = 100,
                        .min_count_bit_for_found = 24 },
            .decode = test_decode_2,
            .flags = SubGhzProtocolFlag_433 | SubGhzProtocolFlag_Send },
};
const uint16_t subghz_protocol_registry_count = 3;

/* Lookup stubs required by the linker but not called in these tests. */
int16_t subghz_protocol_find_by_name(const char *name) { (void)name; return -1; }
const SubGhzProtocolDef *subghz_protocol_get(uint16_t i)
{
    return (i < subghz_protocol_registry_count) ? &subghz_protocol_registry[i] : NULL;
}
const char *subghz_protocol_get_name(uint16_t i)
{
    return (i < subghz_protocol_registry_count) ? subghz_protocol_registry[i].name : NULL;
}

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

/* Build a small pulse buffer with predictable values. */
#define TEST_PULSE_COUNT  4
static const uint16_t TEST_PULSES[TEST_PULSE_COUNT] = { 111, 222, 333, 444 };

void setUp(void)   { reset_state(); }
void tearDown(void) {}

/*============================================================================*/
/* Tests                                                                      */
/*============================================================================*/

/* 1. Pulse copy — even when all decoders fail, the pulse data must land in
 *    subghz_decenc_ctl before any decoder is called. */
void test_pulse_buf_copied_to_decenc_ctl(void)
{
    SubGhzRawDecodeResult result;
    memset(&result, 0, sizeof(result));

    /* All decoders fail (default). */
    bool ret = subghz_registry_decode_try_fn(TEST_PULSES, TEST_PULSE_COUNT, &result, NULL);

    TEST_ASSERT_FALSE(ret);
    TEST_ASSERT_EQUAL_UINT16(TEST_PULSE_COUNT, subghz_decenc_ctl.npulsecount);
    TEST_ASSERT_EQUAL_MEMORY(TEST_PULSES, subghz_decenc_ctl.pulse_times,
                             TEST_PULSE_COUNT * sizeof(uint16_t));
}

/* 2. No match — all decoders return 1 → function returns false. */
void test_no_decoder_match_returns_false(void)
{
    SubGhzRawDecodeResult result;
    memset(&result, 0xFF, sizeof(result));

    bool ret = subghz_registry_decode_try_fn(TEST_PULSES, TEST_PULSE_COUNT, &result, NULL);

    TEST_ASSERT_FALSE(ret);
}

/* 3. All decoders are scanned — each decoder is called exactly once when
 *    no match is found. */
void test_all_decoders_scanned_on_no_match(void)
{
    SubGhzRawDecodeResult result;
    memset(&result, 0, sizeof(result));

    subghz_registry_decode_try_fn(TEST_PULSES, TEST_PULSE_COUNT, &result, NULL);

    TEST_ASSERT_EQUAL_INT(1, g_decoder_call_count[0]);
    TEST_ASSERT_EQUAL_INT(1, g_decoder_call_count[1]);
    TEST_ASSERT_EQUAL_INT(1, g_decoder_call_count[2]);
}

/* 4. Decode match but decenc_read fails → continue scanning.
 *    decoder[1] returns 0 (success at pulse level) but decenc_read returns
 *    false.  The function should NOT return early; it should continue to
 *    decoder[2] which also fails.  Final result: false. */
void test_decode_match_decenc_fail_continues_scanning(void)
{
    g_decoder_result[1] = 0;     /* decoder[1] reports pulse-level match */
    g_decenc_read_result = false; /* but decenc_read() can't produce a result */

    SubGhzRawDecodeResult result;
    memset(&result, 0, sizeof(result));

    bool ret = subghz_registry_decode_try_fn(TEST_PULSES, TEST_PULSE_COUNT, &result, NULL);

    TEST_ASSERT_FALSE(ret);
    /* All three decoders must have been called (function did not stop early). */
    TEST_ASSERT_EQUAL_INT(1, g_decoder_call_count[0]);
    TEST_ASSERT_EQUAL_INT(1, g_decoder_call_count[1]);
    TEST_ASSERT_EQUAL_INT(1, g_decoder_call_count[2]);
}

/* 5. Decode match and decenc_read succeeds → returns true with correct fields.
 *    decoder[1] succeeds and decenc_read fills in recognisable field values.
 *    decoder[2] must NOT be called (early return). */
void test_decode_match_decenc_succeed_returns_true(void)
{
    g_decoder_result[1] = 0;    /* decoder[1] pulse-level match */
    g_decenc_read_result = true;
    g_decenc_read_info.protocol      = 42;
    g_decenc_read_info.key           = 0xDEADBEEFCAFEBABEULL;
    g_decenc_read_info.bit_len       = 24;
    g_decenc_read_info.te            = 350;
    g_decenc_read_info.serial_number = 0x12345678;
    g_decenc_read_info.rolling_code  = 0xAABB;
    g_decenc_read_info.button_id     = 7;

    SubGhzRawDecodeResult result;
    memset(&result, 0, sizeof(result));

    bool ret = subghz_registry_decode_try_fn(TEST_PULSES, TEST_PULSE_COUNT, &result, NULL);

    TEST_ASSERT_TRUE(ret);
    TEST_ASSERT_EQUAL_UINT16(42, result.protocol);
    TEST_ASSERT_EQUAL_UINT64(0xDEADBEEFCAFEBABEULL, result.key);
    TEST_ASSERT_EQUAL_UINT16(24, result.bit_len);
    TEST_ASSERT_EQUAL_UINT16(350, result.te);
    TEST_ASSERT_EQUAL_UINT32(0x12345678, result.serial_number);
    TEST_ASSERT_EQUAL_UINT32(0xAABB, result.rolling_code);
    TEST_ASSERT_EQUAL_UINT8(7, result.button_id);

    /* decoder[2] must NOT have been called after the early return. */
    TEST_ASSERT_EQUAL_INT(0, g_decoder_call_count[2]);
}

/* 6. First decoder matches and decenc_read succeeds → returns true and
 *    subsequent decoders are NOT called. */
void test_first_decoder_match_no_further_scan(void)
{
    g_decoder_result[0] = 0;
    g_decenc_read_result = true;
    g_decenc_read_info.protocol = 1;
    g_decenc_read_info.key      = 0xABCD;

    SubGhzRawDecodeResult result;
    memset(&result, 0, sizeof(result));

    bool ret = subghz_registry_decode_try_fn(TEST_PULSES, TEST_PULSE_COUNT, &result, NULL);

    TEST_ASSERT_TRUE(ret);
    TEST_ASSERT_EQUAL_INT(1, g_decoder_call_count[0]);
    TEST_ASSERT_EQUAL_INT(0, g_decoder_call_count[1]);
    TEST_ASSERT_EQUAL_INT(0, g_decoder_call_count[2]);
}

/* 7. NULL user_ctx is accepted (the function must ignore it). */
void test_null_user_ctx_accepted(void)
{
    SubGhzRawDecodeResult result;
    memset(&result, 0, sizeof(result));
    /* Should not crash regardless of decoders or decenc_read result. */
    subghz_registry_decode_try_fn(TEST_PULSES, TEST_PULSE_COUNT, &result, NULL);
    TEST_PASS();
}

/* 8. Single pulse copy — verify with 1 pulse (boundary condition). */
void test_single_pulse_copy(void)
{
    const uint16_t one_pulse = 999;
    SubGhzRawDecodeResult result;
    memset(&result, 0, sizeof(result));

    subghz_registry_decode_try_fn(&one_pulse, 1, &result, NULL);

    TEST_ASSERT_EQUAL_UINT16(1, subghz_decenc_ctl.npulsecount);
    TEST_ASSERT_EQUAL_UINT16(999, subghz_decenc_ctl.pulse_times[0]);
}

/*============================================================================*/
/* main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_pulse_buf_copied_to_decenc_ctl);
    RUN_TEST(test_no_decoder_match_returns_false);
    RUN_TEST(test_all_decoders_scanned_on_no_match);
    RUN_TEST(test_decode_match_decenc_fail_continues_scanning);
    RUN_TEST(test_decode_match_decenc_succeed_returns_true);
    RUN_TEST(test_first_decoder_match_no_further_scan);
    RUN_TEST(test_null_user_ctx_accepted);
    RUN_TEST(test_single_pulse_copy);

    return UNITY_END();
}
