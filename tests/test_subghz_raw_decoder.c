/* See COPYING.txt for license details. */

/*
 * test_subghz_raw_decoder.c
 *
 * Unit tests for subghz_raw_decoder — the offline RAW→protocol decode
 * engine extracted from m1_subghz_scene_saved.c::do_decode_raw().
 *
 * Tests cover:
 *   - Empty / NULL input handling
 *   - Noise filtering (below PACKET_PULSE_TIME_MIN)
 *   - Gap-based packet segmentation
 *   - Successful decode via mock callback
 *   - Deduplication of same protocol+key
 *   - Trailing packet (no final gap)
 *   - Pulse buffer overflow (> PACKET_PULSE_COUNT_MAX)
 *   - Max results clamping
 *   - Negative raw values (absolute value used)
 *   - Mixed noise + valid packets
 *   - Zero-key rejection
 *   - Frequency and RSSI pass-through
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include <string.h>
#include "unity.h"
#include "subghz_raw_decoder.h"

void setUp(void) {}
void tearDown(void) {}

/*============================================================================*/
/* Mock decode callback                                                       */
/*============================================================================*/

/** Mock context — controls what the mock callback returns */
typedef struct {
    uint16_t match_protocol;    /**< Protocol index to report on match */
    uint64_t match_key;         /**< Key value to report on match */
    uint16_t match_bit_len;     /**< Bit length to report */
    uint16_t match_te;          /**< TE to report */
    uint16_t min_pulses;        /**< Minimum pulses to "decode" */
    bool     should_match;      /**< Whether callback should report success */
    uint8_t  call_count;        /**< Number of times callback was invoked */
} MockDecodeCtx;

static MockDecodeCtx make_mock(bool should_match, uint16_t protocol,
                                uint64_t key, uint16_t min_pulses)
{
    MockDecodeCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.should_match  = should_match;
    ctx.match_protocol = protocol;
    ctx.match_key      = key;
    ctx.match_bit_len  = 24;
    ctx.match_te       = 350;
    ctx.min_pulses     = min_pulses;
    return ctx;
}

static bool mock_try_decode(const uint16_t *pulse_buf,
                             uint16_t pulse_count,
                             SubGhzRawDecodeResult *out,
                             void *user_ctx)
{
    MockDecodeCtx *ctx = (MockDecodeCtx *)user_ctx;
    ctx->call_count++;

    if (!ctx->should_match)
        return false;

    if (pulse_count < ctx->min_pulses)
        return false;

    out->protocol = ctx->match_protocol;
    out->key      = ctx->match_key;
    out->bit_len  = ctx->match_bit_len;
    out->te       = ctx->match_te;
    /* frequency and rssi are set by the caller, not the callback */
    (void)pulse_buf;
    return true;
}

/*============================================================================*/
/* Multi-protocol mock — returns different results for different packets       */
/*============================================================================*/

typedef struct {
    uint16_t protocol;
    uint64_t key;
    uint16_t min_pulses;
} MockProtoEntry;

typedef struct {
    MockProtoEntry entries[8];
    uint8_t entry_count;
    uint8_t call_count;
    uint8_t next_entry;  /**< Round-robin through entries */
} MultiMockCtx;

static bool multi_mock_try_decode(const uint16_t *pulse_buf,
                                   uint16_t pulse_count,
                                   SubGhzRawDecodeResult *out,
                                   void *user_ctx)
{
    MultiMockCtx *ctx = (MultiMockCtx *)user_ctx;
    ctx->call_count++;
    (void)pulse_buf;

    if (ctx->next_entry >= ctx->entry_count)
        return false;

    MockProtoEntry *e = &ctx->entries[ctx->next_entry];
    if (pulse_count < e->min_pulses)
        return false;

    out->protocol = e->protocol;
    out->key      = e->key;
    out->bit_len  = 24;
    out->te       = 350;
    ctx->next_entry++;
    return true;
}

/*============================================================================*/
/* Zero-key mock — simulates a decoder that returns key=0                      */
/*============================================================================*/

static bool zero_key_try_decode(const uint16_t *pulse_buf,
                                 uint16_t pulse_count,
                                 SubGhzRawDecodeResult *out,
                                 void *user_ctx)
{
    (void)pulse_buf;
    (void)pulse_count;
    (void)user_ctx;
    out->protocol = 1;
    out->key      = 0;   /* Zero key — should be rejected */
    out->bit_len  = 24;
    out->te       = 350;
    return true;
}

/*============================================================================*/
/* Helper — build a raw data array with valid pulses separated by gaps         */
/*============================================================================*/

/**
 * Build a raw data array with `n_packets` packets, each containing
 * `pulses_per_packet` pulses of duration `pulse_dur`, separated by
 * gaps of `gap_dur`.
 */
static uint16_t build_raw_data(int16_t *buf, uint16_t buf_size,
                                uint8_t n_packets, uint16_t pulses_per_packet,
                                int16_t pulse_dur, int16_t gap_dur)
{
    uint16_t idx = 0;
    for (uint8_t pkt = 0; pkt < n_packets; pkt++)
    {
        for (uint16_t p = 0; p < pulses_per_packet; p++)
        {
            if (idx >= buf_size) return idx;
            /* Alternate sign to simulate mark/space */
            buf[idx++] = (p % 2 == 0) ? pulse_dur : (int16_t)(-pulse_dur);
        }
        /* Inter-packet gap */
        if (idx < buf_size)
            buf[idx++] = gap_dur;
    }
    return idx;
}

/*============================================================================*/
/* Tests: NULL and empty input                                                */
/*============================================================================*/

void test_null_raw_data_returns_zero(void)
{
    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(true, 1, 0x123, 40);
    uint8_t count = subghz_decode_raw_offline(
        NULL, 0, 433920000, results, 4, mock_try_decode, &ctx);
    TEST_ASSERT_EQUAL_UINT8(0, count);
}

void test_empty_raw_data_returns_zero(void)
{
    int16_t raw[1] = {0};
    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(true, 1, 0x123, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, 0, 433920000, results, 4, mock_try_decode, &ctx);
    TEST_ASSERT_EQUAL_UINT8(0, count);
}

void test_null_results_returns_zero(void)
{
    int16_t raw[50];
    MockDecodeCtx ctx = make_mock(true, 1, 0x123, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, 50, 433920000, NULL, 4, mock_try_decode, &ctx);
    TEST_ASSERT_EQUAL_UINT8(0, count);
}

void test_null_callback_returns_zero(void)
{
    int16_t raw[50] = {0};
    SubGhzRawDecodeResult results[4];
    uint8_t count = subghz_decode_raw_offline(
        raw, 50, 433920000, results, 4, NULL, NULL);
    TEST_ASSERT_EQUAL_UINT8(0, count);
}

/*============================================================================*/
/* Tests: Noise filtering                                                     */
/*============================================================================*/

void test_all_noise_no_decode(void)
{
    /* All samples below PACKET_PULSE_TIME_MIN (120) */
    int16_t raw[60];
    for (int i = 0; i < 60; i++)
        raw[i] = (int16_t)((i % 2 == 0) ? 50 : -50);

    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(true, 1, 0x123, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, 60, 433920000, results, 4, mock_try_decode, &ctx);
    TEST_ASSERT_EQUAL_UINT8(0, count);
    TEST_ASSERT_EQUAL_UINT8(0, ctx.call_count);
}

void test_noise_resets_accumulator(void)
{
    /* 20 valid pulses, then noise, then 20 more valid + gap.
     * Total = 20 valid pulses per segment — below PACKET_PULSE_COUNT_MIN (40).
     * Should NOT decode even though 40 total pulses were sent. */
    int16_t raw[80];
    uint16_t idx = 0;
    /* 20 valid pulses */
    for (int i = 0; i < 20; i++)
        raw[idx++] = (int16_t)((i % 2 == 0) ? 300 : -300);
    /* 1 noise pulse */
    raw[idx++] = 50;  /* below minimum */
    /* 20 more valid pulses */
    for (int i = 0; i < 20; i++)
        raw[idx++] = (int16_t)((i % 2 == 0) ? 300 : -300);
    /* Gap */
    raw[idx++] = 2000;

    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(true, 1, 0x123, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, idx, 433920000, results, 4, mock_try_decode, &ctx);
    TEST_ASSERT_EQUAL_UINT8(0, count);
}

/*============================================================================*/
/* Tests: Successful decode                                                   */
/*============================================================================*/

void test_single_packet_decode(void)
{
    /* 48 valid pulses + gap → one packet with 49 pulses (gap included) */
    int16_t raw[100];
    uint16_t len = build_raw_data(raw, 100, 1, 48, 300, 2000);

    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(true, 5, 0xABCDEF, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, len, 433920000, results, 4, mock_try_decode, &ctx);

    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(5, results[0].protocol);
    TEST_ASSERT_EQUAL_HEX64(0xABCDEF, results[0].key);
    TEST_ASSERT_EQUAL_UINT32(433920000, results[0].frequency);
    TEST_ASSERT_EQUAL_INT16(0, results[0].rssi);
}

void test_two_different_packets_decode(void)
{
    /* Two packets with gap between them */
    int16_t raw[200];
    uint16_t idx = 0;
    /* Packet 1: 48 pulses + gap */
    for (int i = 0; i < 48; i++)
        raw[idx++] = (int16_t)((i % 2 == 0) ? 300 : -300);
    raw[idx++] = 2000;
    /* Packet 2: 48 pulses + gap */
    for (int i = 0; i < 48; i++)
        raw[idx++] = (int16_t)((i % 2 == 0) ? 400 : -400);
    raw[idx++] = 3000;

    SubGhzRawDecodeResult results[4];
    MultiMockCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.entries[0] = (MockProtoEntry){.protocol = 1, .key = 0x111, .min_pulses = 40};
    ctx.entries[1] = (MockProtoEntry){.protocol = 2, .key = 0x222, .min_pulses = 40};
    ctx.entry_count = 2;

    uint8_t count = subghz_decode_raw_offline(
        raw, idx, 315000000, results, 4, multi_mock_try_decode, &ctx);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_UINT16(1, results[0].protocol);
    TEST_ASSERT_EQUAL_HEX64(0x111, results[0].key);
    TEST_ASSERT_EQUAL_UINT16(2, results[1].protocol);
    TEST_ASSERT_EQUAL_HEX64(0x222, results[1].key);
    TEST_ASSERT_EQUAL_UINT32(315000000, results[0].frequency);
    TEST_ASSERT_EQUAL_UINT32(315000000, results[1].frequency);
}

/*============================================================================*/
/* Tests: Deduplication                                                       */
/*============================================================================*/

void test_duplicate_protocol_key_deduplicated(void)
{
    /* Two identical packets — should only get one result */
    int16_t raw[200];
    uint16_t len = build_raw_data(raw, 200, 2, 48, 300, 2000);

    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(true, 3, 0xDEAD, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, len, 433920000, results, 4, mock_try_decode, &ctx);

    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(3, results[0].protocol);
    /* Callback was invoked twice (once per packet) */
    TEST_ASSERT_EQUAL_UINT8(2, ctx.call_count);
}

/*============================================================================*/
/* Tests: Trailing packet (no final gap)                                      */
/*============================================================================*/

void test_trailing_packet_without_gap(void)
{
    /* 48 pulses with NO trailing gap — should still be decoded */
    int16_t raw[48];
    for (int i = 0; i < 48; i++)
        raw[i] = (int16_t)((i % 2 == 0) ? 300 : -300);

    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(true, 7, 0xBEEF, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, 48, 433920000, results, 4, mock_try_decode, &ctx);

    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT16(7, results[0].protocol);
    TEST_ASSERT_EQUAL_HEX64(0xBEEF, results[0].key);
}

/*============================================================================*/
/* Tests: Pulse overflow                                                      */
/*============================================================================*/

void test_pulse_overflow_resets(void)
{
    /* More than PACKET_PULSE_COUNT_MAX (256) pulses without a gap
     * should reset the accumulator and not crash */
    int16_t raw[300];
    for (int i = 0; i < 280; i++)
        raw[i] = (int16_t)((i % 2 == 0) ? 300 : -300);
    /* Add a gap at the end */
    raw[280] = 2000;

    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(true, 1, 0x123, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, 281, 433920000, results, 4, mock_try_decode, &ctx);

    /* After overflow reset, the gap pulse starts a new packet of 1 pulse
     * which is below PACKET_PULSE_COUNT_MIN → no decode */
    TEST_ASSERT_EQUAL_UINT8(0, count);
}

/*============================================================================*/
/* Tests: Max results clamping                                                */
/*============================================================================*/

void test_max_results_clamped(void)
{
    /* 5 packets, but max_results = 2 → only 2 decoded */
    int16_t raw[500];
    uint16_t idx = 0;
    for (int pkt = 0; pkt < 5; pkt++)
    {
        for (int p = 0; p < 48; p++)
            raw[idx++] = (int16_t)((p % 2 == 0) ? 300 : -300);
        raw[idx++] = 2000;
    }

    SubGhzRawDecodeResult results[2];
    MultiMockCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    for (int i = 0; i < 5; i++)
        ctx.entries[i] = (MockProtoEntry){
            .protocol = (uint16_t)(i + 1),
            .key = (uint64_t)(0x100 + i),
            .min_pulses = 40};
    ctx.entry_count = 5;

    uint8_t count = subghz_decode_raw_offline(
        raw, idx, 433920000, results, 2, multi_mock_try_decode, &ctx);

    TEST_ASSERT_EQUAL_UINT8(2, count);
}

/*============================================================================*/
/* Tests: Negative raw values                                                 */
/*============================================================================*/

void test_negative_values_absolute(void)
{
    /* All negative values — should be treated as absolute */
    int16_t raw[50];
    for (int i = 0; i < 48; i++)
        raw[i] = -300;
    raw[48] = -2000;  /* gap (negative) */

    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(true, 1, 0xCAFE, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, 49, 433920000, results, 4, mock_try_decode, &ctx);

    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_HEX64(0xCAFE, results[0].key);
}

/*============================================================================*/
/* Tests: Zero key rejection                                                  */
/*============================================================================*/

void test_zero_key_rejected(void)
{
    /* Callback returns key=0 → should be rejected */
    int16_t raw[100];
    uint16_t len = build_raw_data(raw, 100, 1, 48, 300, 2000);

    SubGhzRawDecodeResult results[4];
    uint8_t count = subghz_decode_raw_offline(
        raw, len, 433920000, results, 4, zero_key_try_decode, NULL);

    TEST_ASSERT_EQUAL_UINT8(0, count);
}

/*============================================================================*/
/* Tests: Callback failure                                                    */
/*============================================================================*/

void test_callback_failure_no_decode(void)
{
    int16_t raw[100];
    uint16_t len = build_raw_data(raw, 100, 1, 48, 300, 2000);

    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(false, 0, 0, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, len, 433920000, results, 4, mock_try_decode, &ctx);

    TEST_ASSERT_EQUAL_UINT8(0, count);
    TEST_ASSERT_GREATER_THAN(0, ctx.call_count);
}

/*============================================================================*/
/* Tests: Too few pulses                                                      */
/*============================================================================*/

void test_too_few_pulses_no_decode(void)
{
    /* Only 20 pulses (below PACKET_PULSE_COUNT_MIN of 40) + gap */
    int16_t raw[30];
    for (int i = 0; i < 20; i++)
        raw[i] = (int16_t)((i % 2 == 0) ? 300 : -300);
    raw[20] = 2000;

    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(true, 1, 0x123, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, 21, 433920000, results, 4, mock_try_decode, &ctx);

    TEST_ASSERT_EQUAL_UINT8(0, count);
    /* Callback should NOT be invoked when pulse count is too low */
    TEST_ASSERT_EQUAL_UINT8(0, ctx.call_count);
}

/*============================================================================*/
/* Tests: Mixed valid + noise + valid                                         */
/*============================================================================*/

void test_mixed_noise_and_valid(void)
{
    /* Packet 1 (valid: 48 pulses + gap),
     * noise segment,
     * Packet 2 (valid: 48 pulses + gap) */
    int16_t raw[200];
    uint16_t idx = 0;

    /* Packet 1 */
    for (int i = 0; i < 48; i++)
        raw[idx++] = (int16_t)((i % 2 == 0) ? 300 : -300);
    raw[idx++] = 2000;

    /* Noise */
    for (int i = 0; i < 10; i++)
        raw[idx++] = 50;

    /* Packet 2 */
    for (int i = 0; i < 48; i++)
        raw[idx++] = (int16_t)((i % 2 == 0) ? 400 : -400);
    raw[idx++] = 3000;

    SubGhzRawDecodeResult results[4];
    MultiMockCtx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.entries[0] = (MockProtoEntry){.protocol = 10, .key = 0xAAA, .min_pulses = 40};
    ctx.entries[1] = (MockProtoEntry){.protocol = 20, .key = 0xBBB, .min_pulses = 40};
    ctx.entry_count = 2;

    uint8_t count = subghz_decode_raw_offline(
        raw, idx, 433920000, results, 4, multi_mock_try_decode, &ctx);

    TEST_ASSERT_EQUAL_UINT8(2, count);
    TEST_ASSERT_EQUAL_UINT16(10, results[0].protocol);
    TEST_ASSERT_EQUAL_UINT16(20, results[1].protocol);
}

/*============================================================================*/
/* Tests: Frequency pass-through                                              */
/*============================================================================*/

void test_frequency_passed_through(void)
{
    int16_t raw[100];
    uint16_t len = build_raw_data(raw, 100, 1, 48, 300, 2000);

    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(true, 1, 0x999, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, len, 868350000, results, 4, mock_try_decode, &ctx);

    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_UINT32(868350000, results[0].frequency);
    TEST_ASSERT_EQUAL_INT16(0, results[0].rssi);
}

/*============================================================================*/
/* Tests: Gap included in pulse count                                         */
/*============================================================================*/

void test_gap_pulse_included_in_count(void)
{
    /* 39 normal pulses + 1 gap pulse = 40 total.
     * The gap pulse is accumulated before triggering decode,
     * so 40 >= PACKET_PULSE_COUNT_MIN → should decode. */
    int16_t raw[41];
    for (int i = 0; i < 39; i++)
        raw[i] = (int16_t)((i % 2 == 0) ? 300 : -300);
    raw[39] = 2000;  /* Gap — this is the 40th pulse */

    SubGhzRawDecodeResult results[4];
    MockDecodeCtx ctx = make_mock(true, 1, 0x40, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, 40, 433920000, results, 4, mock_try_decode, &ctx);

    TEST_ASSERT_EQUAL_UINT8(1, count);
    TEST_ASSERT_EQUAL_HEX64(0x40, results[0].key);
}

/*============================================================================*/
/* Tests: max_results=0                                                       */
/*============================================================================*/

void test_zero_max_results(void)
{
    int16_t raw[100];
    uint16_t len = build_raw_data(raw, 100, 1, 48, 300, 2000);

    SubGhzRawDecodeResult results[1];
    MockDecodeCtx ctx = make_mock(true, 1, 0x123, 40);
    uint8_t count = subghz_decode_raw_offline(
        raw, len, 433920000, results, 0, mock_try_decode, &ctx);

    TEST_ASSERT_EQUAL_UINT8(0, count);
}

/*============================================================================*/
/* Main                                                                       */
/*============================================================================*/

int main(void)
{
    UNITY_BEGIN();

    /* NULL/empty input */
    RUN_TEST(test_null_raw_data_returns_zero);
    RUN_TEST(test_empty_raw_data_returns_zero);
    RUN_TEST(test_null_results_returns_zero);
    RUN_TEST(test_null_callback_returns_zero);

    /* Noise filtering */
    RUN_TEST(test_all_noise_no_decode);
    RUN_TEST(test_noise_resets_accumulator);

    /* Successful decode */
    RUN_TEST(test_single_packet_decode);
    RUN_TEST(test_two_different_packets_decode);

    /* Deduplication */
    RUN_TEST(test_duplicate_protocol_key_deduplicated);

    /* Trailing packet */
    RUN_TEST(test_trailing_packet_without_gap);

    /* Overflow */
    RUN_TEST(test_pulse_overflow_resets);

    /* Clamping */
    RUN_TEST(test_max_results_clamped);
    RUN_TEST(test_zero_max_results);

    /* Negative values */
    RUN_TEST(test_negative_values_absolute);

    /* Zero key rejection */
    RUN_TEST(test_zero_key_rejected);

    /* Callback failure */
    RUN_TEST(test_callback_failure_no_decode);

    /* Too few pulses */
    RUN_TEST(test_too_few_pulses_no_decode);

    /* Mixed noise + valid */
    RUN_TEST(test_mixed_noise_and_valid);

    /* Pass-through */
    RUN_TEST(test_frequency_passed_through);

    /* Gap inclusion */
    RUN_TEST(test_gap_pulse_included_in_count);

    return UNITY_END();
}
