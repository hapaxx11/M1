/* See COPYING.txt for license details. */

/**
 * @file   test_subghz_signal_format.c
 * @brief  Unity tests for the Phase 11-1 polymorphic Info-screen renderer.
 *
 * Covers `subghz_signal_format_keeloq_info()`:
 *   - Buffer always NUL-terminated (including on NULL/zero inputs).
 *   - KeeLoq layout extraction: Serial / Button / EncHop fields.
 *   - Star Line layout extraction (different bit ordering).
 *   - Jarolift layout (mirrors KeeLoq).
 *   - Case-insensitive + prefix-terminated protocol match
 *     (e.g. "KeeLoq 433" classifies as KeeLoq).
 *   - Unknown protocol → falls back to a raw key dump (regression
 *     guard against a registry mis-wiring).
 *   - NULL view safety.
 *   - Truncation safety (buflen too small never overflows).
 */

#include "unity.h"
#include "subghz_signal_format.h"
#include "subghz_protocol_registry.h"

#include <string.h>

void setUp(void)    {}
void tearDown(void) {}

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

/* Build a KeeLoq-layout 64-bit Flipper key: [63:60]=button, [59:32]=serial,
 * [31:0]=enc_hop.  Mirrors subghz_signal_fields.c::extract_keeloq(). */
static uint64_t build_keeloq_key(uint32_t serial, uint8_t button, uint32_t hop)
{
    return ((uint64_t)(button & 0x0F) << 60) |
           ((uint64_t)(serial & 0x0FFFFFFFU) << 32) |
           (uint64_t)hop;
}

/* Build a Star Line layout key: [63:32]=enc_hop, [31:4]=serial, [3:0]=button. */
static uint64_t build_starline_key(uint32_t serial, uint8_t button, uint32_t hop)
{
    return ((uint64_t)hop << 32) |
           ((uint64_t)(serial & 0x0FFFFFFFU) << 4) |
           (uint64_t)(button & 0x0F);
}

/*============================================================================*/
/* Buffer / NUL safety                                                        */
/*============================================================================*/

void test_null_view_safely_nul_terminates_buffer(void)
{
    char buf[16] = "garbage";
    subghz_signal_format_keeloq_info(NULL, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
}

void test_null_buffer_is_a_noop(void)
{
    /* Must not crash.  No buffer to inspect. */
    SubGhzSignalView view = { .protocol = "KeeLoq", .key = 0, .bit_count = 64, .te = 400 };
    subghz_signal_format_keeloq_info(&view, NULL, 32);
}

void test_zero_length_buffer_is_a_noop(void)
{
    char buf[4] = "abcd";
    SubGhzSignalView view = { .protocol = "KeeLoq", .key = 0, .bit_count = 64, .te = 400 };
    subghz_signal_format_keeloq_info(&view, buf, 0);
    /* No write should have happened; original contents preserved. */
    TEST_ASSERT_EQUAL_CHAR('a', buf[0]);
}

void test_buflen_1_writes_only_nul(void)
{
    char buf[2] = { 'X', 'Y' };
    SubGhzSignalView view = { .protocol = "KeeLoq", .key = 0, .bit_count = 64, .te = 400 };
    subghz_signal_format_keeloq_info(&view, buf, 1);
    TEST_ASSERT_EQUAL_CHAR('\0', buf[0]);
}

/*============================================================================*/
/* KeeLoq layout extraction                                                   */
/*============================================================================*/

void test_keeloq_extracts_serial_button_enchop(void)
{
    uint64_t key = build_keeloq_key(0x0123456, 0xA, 0xDEADBEEFU);
    SubGhzSignalView view = { .protocol = "KeeLoq", .key = key, .bit_count = 64, .te = 400 };

    char buf[128];
    subghz_signal_format_keeloq_info(&view, buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "Proto: KeeLoq"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Serial: 0x0123456"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Button: 0xA"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "EncHop: 0xDEADBEEF"));
}

void test_jarolift_uses_same_layout_as_keeloq(void)
{
    uint64_t key = build_keeloq_key(0x0AABBCC, 0x5, 0x12345678U);
    SubGhzSignalView view = { .protocol = "Jarolift", .key = key, .bit_count = 72, .te = 400 };

    char buf[128];
    subghz_signal_format_keeloq_info(&view, buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "Proto: Jarolift"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Serial: 0x0AABBCC"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Button: 0x5"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "EncHop: 0x12345678"));
}

/*============================================================================*/
/* Star Line layout extraction (different bit ordering)                       */
/*============================================================================*/

void test_starline_uses_reversed_layout(void)
{
    uint64_t key = build_starline_key(0x07654321U, 0x3, 0xCAFEBABEU);
    SubGhzSignalView view = { .protocol = "Star Line", .key = key, .bit_count = 64, .te = 400 };

    char buf[128];
    subghz_signal_format_keeloq_info(&view, buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "Proto: Star Line"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Serial: 0x7654321"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Button: 0x3"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "EncHop: 0xCAFEBABE"));
}

/*============================================================================*/
/* Case-insensitive + prefix-terminated protocol match                        */
/*============================================================================*/

void test_protocol_match_is_case_insensitive(void)
{
    uint64_t key = build_keeloq_key(0x0111111, 0x1, 0x22222222U);
    SubGhzSignalView view = { .protocol = "keeloq", .key = key, .bit_count = 64, .te = 400 };

    char buf[128];
    subghz_signal_format_keeloq_info(&view, buf, sizeof(buf));

    /* Field extractor should still recognise lowercase "keeloq" as KeeLoq. */
    TEST_ASSERT_NOT_NULL(strstr(buf, "Serial: 0x0111111"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Button: 0x1"));
}

void test_protocol_match_is_prefix_terminated_by_space(void)
{
    /* "KeeLoq 433" classifies as KeeLoq — mirrors is_keeloq_family's behaviour. */
    uint64_t key = build_keeloq_key(0x0FACEBE, 0x7, 0xABCDEF00U);
    SubGhzSignalView view = { .protocol = "KeeLoq 433", .key = key, .bit_count = 64, .te = 400 };

    char buf[128];
    subghz_signal_format_keeloq_info(&view, buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "Serial: 0x0FACEBE"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Button: 0x7"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "EncHop: 0xABCDEF00"));
}

/*============================================================================*/
/* Unknown protocol — fallback path                                           */
/*============================================================================*/

void test_unknown_protocol_falls_back_to_raw_key_dump(void)
{
    SubGhzSignalView view = {
        .protocol = "Princeton",   /* not a KeeLoq family member */
        .key      = 0x1122334455667788ULL,
        .bit_count = 24,
        .te        = 250,
    };

    char buf[128];
    subghz_signal_format_keeloq_info(&view, buf, sizeof(buf));

    /* Header still shows the protocol name verbatim... */
    TEST_ASSERT_NOT_NULL(strstr(buf, "Proto: Princeton"));
    /* ...but Serial/Button/EncHop are absent — fallback writes a "Key:" dump. */
    TEST_ASSERT_NULL(strstr(buf, "Serial:"));
    TEST_ASSERT_NULL(strstr(buf, "Button:"));
    TEST_ASSERT_NULL(strstr(buf, "EncHop:"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Key: 0x1122334455667788"));
}

void test_null_protocol_falls_back_safely(void)
{
    SubGhzSignalView view = {
        .protocol = NULL,
        .key      = 0xAAAABBBBCCCCDDDDULL,
        .bit_count = 64,
        .te        = 400,
    };

    char buf[128];
    subghz_signal_format_keeloq_info(&view, buf, sizeof(buf));

    TEST_ASSERT_NOT_NULL(strstr(buf, "Proto: ?"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Key: 0xAAAABBBBCCCCDDDD"));
}

/*============================================================================*/
/* Truncation safety                                                          */
/*============================================================================*/

void test_short_buffer_never_overflows_and_is_nul_terminated(void)
{
    /* Sentinels around a 16-byte working buffer let us detect any overrun. */
    char before  = 0x5A;
    char buf[16];
    char after   = 0xA5;
    memset(buf, 0x55, sizeof(buf));

    uint64_t key = build_keeloq_key(0x0123456, 0xA, 0xDEADBEEFU);
    SubGhzSignalView view = { .protocol = "KeeLoq", .key = key, .bit_count = 64, .te = 400 };

    subghz_signal_format_keeloq_info(&view, buf, sizeof(buf));

    /* Always NUL-terminated within the buffer. */
    bool found_nul = false;
    for (size_t i = 0; i < sizeof(buf); i++)
    {
        if (buf[i] == '\0') { found_nul = true; break; }
    }
    TEST_ASSERT_TRUE(found_nul);

    /* Sentinels untouched. */
    TEST_ASSERT_EQUAL_CHAR(0x5A, before);
    TEST_ASSERT_EQUAL_CHAR((char)0xA5, after);
}

/*============================================================================*/
/* Registry vtable slot wiring (smoke test)                                   */
/*============================================================================*/

/* Verify that the SubGhzGetStringFn typedef and SubGhzSignalView struct are
 * exposed by the registry header and that the renderer matches the signature.
 * If the typedef ever drifts, this assignment fails to compile. */
void test_renderer_matches_vtable_signature(void)
{
    SubGhzGetStringFn fn = subghz_signal_format_keeloq_info;
    TEST_ASSERT_NOT_NULL((void *)fn);

    /* Round-trip via the typedef: same observable behaviour. */
    uint64_t key = build_keeloq_key(0x0123456, 0xA, 0xDEADBEEFU);
    SubGhzSignalView view = { .protocol = "KeeLoq", .key = key, .bit_count = 64, .te = 400 };
    char buf[128];
    fn(&view, buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Serial: 0x0123456"));
}

/*============================================================================*/
/* Test runner                                                                */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_null_view_safely_nul_terminates_buffer);
    RUN_TEST(test_null_buffer_is_a_noop);
    RUN_TEST(test_zero_length_buffer_is_a_noop);
    RUN_TEST(test_buflen_1_writes_only_nul);

    RUN_TEST(test_keeloq_extracts_serial_button_enchop);
    RUN_TEST(test_jarolift_uses_same_layout_as_keeloq);
    RUN_TEST(test_starline_uses_reversed_layout);

    RUN_TEST(test_protocol_match_is_case_insensitive);
    RUN_TEST(test_protocol_match_is_prefix_terminated_by_space);

    RUN_TEST(test_unknown_protocol_falls_back_to_raw_key_dump);
    RUN_TEST(test_null_protocol_falls_back_safely);

    RUN_TEST(test_short_buffer_never_overflows_and_is_nul_terminated);
    RUN_TEST(test_renderer_matches_vtable_signature);

    return UNITY_END();
}
