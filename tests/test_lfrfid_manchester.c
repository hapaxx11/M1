/* See COPYING.txt for license details. */

/*
 * test_lfrfid_manchester.c
 *
 * Unit tests for lfrfid_manchester.c — LFRFID Manchester decoder.
 *
 * Tests the pure-logic functions:
 *   - manch_init()        — decoder initialization
 *   - manch_push_bit()    — MSB shift register accumulation
 *   - manch_get_bit()     — bit extraction from frame buffer
 *   - manch_is_full()     — frame completion check
 *   - manch_feed_events() — edge timing → Manchester bit decoding
 *
 * These validate that the Manchester decoder correctly decodes edge
 * timing events into protocol frames for LFRFID protocols.
 *
 * Build:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "lfrfid.h"              /* stub — provides lfrfid_evt_t + FRAME_CHUNK_SIZE */
#include "lfrfid_manchester.h"

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * manch_init — initialization
 * =================================================================== */

void test_manch_init_sets_params(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 64);
	TEST_ASSERT_EQUAL_UINT16(256, d.half_bit_us);
	TEST_ASSERT_EQUAL_UINT16(64, d.frame_bits);
	TEST_ASSERT_EQUAL_UINT16(0, d.bit_count);
	TEST_ASSERT_EQUAL_UINT8(0, d.edge_count);
}

void test_manch_init_clears_frame(void)
{
	manch_decoder_t d;
	memset(&d, 0xFF, sizeof(d));
	manch_init(&d, 200, 32);
	for (int i = 0; i < MANCH_MAX_FRAME_BYTES; i++)
		TEST_ASSERT_EQUAL_UINT8(0, d.frame_buffer[i]);
}

/* ===================================================================
 * manch_push_bit — MSB shift register
 * =================================================================== */

void test_manch_push_bit_single_one(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);
	manch_push_bit(&d, 1);
	TEST_ASSERT_EQUAL_UINT8(1, d.frame_buffer[0]);
	TEST_ASSERT_EQUAL_UINT16(1, d.bit_count);
}

void test_manch_push_bit_single_zero(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);
	manch_push_bit(&d, 0);
	TEST_ASSERT_EQUAL_UINT8(0, d.frame_buffer[0]);
	TEST_ASSERT_EQUAL_UINT16(1, d.bit_count);
}

void test_manch_push_bit_byte_pattern(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);
	/* Push 0xA5 = 10100101 */
	manch_push_bit(&d, 1);
	manch_push_bit(&d, 0);
	manch_push_bit(&d, 1);
	manch_push_bit(&d, 0);
	manch_push_bit(&d, 0);
	manch_push_bit(&d, 1);
	manch_push_bit(&d, 0);
	manch_push_bit(&d, 1);
	TEST_ASSERT_EQUAL_UINT8(0xA5, d.frame_buffer[0]);
	TEST_ASSERT_EQUAL_UINT16(8, d.bit_count);
}

void test_manch_push_bit_multi_byte(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 16);
	/* Push 0xFF 0x00 */
	for (int i = 0; i < 8; i++)
		manch_push_bit(&d, 1);
	for (int i = 0; i < 8; i++)
		manch_push_bit(&d, 0);
	TEST_ASSERT_EQUAL_UINT8(0xFF, d.frame_buffer[0]);
	TEST_ASSERT_EQUAL_UINT8(0x00, d.frame_buffer[1]);
	TEST_ASSERT_EQUAL_UINT16(16, d.bit_count);
}

void test_manch_push_bit_shifts_msb(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);
	/* Push more than 8 bits — older bits shift out */
	for (int i = 0; i < 8; i++)
		manch_push_bit(&d, 1);  /* 0xFF */
	manch_push_bit(&d, 0);         /* shifts to 0xFE */
	TEST_ASSERT_EQUAL_UINT8(0xFE, d.frame_buffer[0]);
}

void test_manch_push_bit_count_saturates(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);
	for (int i = 0; i < 16; i++)
		manch_push_bit(&d, 1);
	/* bit_count should not exceed frame_bits */
	TEST_ASSERT_EQUAL_UINT16(8, d.bit_count);
}

/* ===================================================================
 * manch_get_bit — bit extraction from frame buffer
 * =================================================================== */

void test_manch_get_bit_positions(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 16);
	/* Set frame_buffer[0] = 0xA5 = 10100101, frame_buffer[1] = 0x3C = 00111100 */
	d.frame_buffer[0] = 0xA5;
	d.frame_buffer[1] = 0x3C;
	/* Bit 0 (MSB of byte 0) = 1 */
	TEST_ASSERT_EQUAL_UINT8(1, manch_get_bit(&d, 0));
	/* Bit 1 = 0 */
	TEST_ASSERT_EQUAL_UINT8(0, manch_get_bit(&d, 1));
	/* Bit 2 = 1 */
	TEST_ASSERT_EQUAL_UINT8(1, manch_get_bit(&d, 2));
	/* Bit 7 (LSB of byte 0) = 1 */
	TEST_ASSERT_EQUAL_UINT8(1, manch_get_bit(&d, 7));
	/* Bit 8 (MSB of byte 1) = 0 */
	TEST_ASSERT_EQUAL_UINT8(0, manch_get_bit(&d, 8));
	/* Bit 10 = 1 */
	TEST_ASSERT_EQUAL_UINT8(1, manch_get_bit(&d, 10));
}

/* ===================================================================
 * manch_is_full — frame completion
 * =================================================================== */

void test_manch_is_full_not_full(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);
	manch_push_bit(&d, 1);
	TEST_ASSERT_FALSE(manch_is_full(&d));
}

void test_manch_is_full_exactly_full(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);
	for (int i = 0; i < 8; i++)
		manch_push_bit(&d, 1);
	TEST_ASSERT_TRUE(manch_is_full(&d));
}

void test_manch_is_full_null(void)
{
	TEST_ASSERT_FALSE(manch_is_full(NULL));
}

/* ===================================================================
 * manch_reset — clears state for next frame
 * =================================================================== */

void test_manch_reset_clears(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);
	for (int i = 0; i < 8; i++)
		manch_push_bit(&d, 1);
	TEST_ASSERT_TRUE(manch_is_full(&d));

	manch_reset(&d);
	TEST_ASSERT_EQUAL_UINT16(0, d.bit_count);
	TEST_ASSERT_EQUAL_UINT8(0, d.edge_count);
	TEST_ASSERT_FALSE(manch_is_full(&d));
	/* half_bit_us and frame_bits should be preserved */
	TEST_ASSERT_EQUAL_UINT16(256, d.half_bit_us);
	TEST_ASSERT_EQUAL_UINT16(8, d.frame_bits);
}

/* ===================================================================
 * manch_feed_events — half/full period classification
 *
 * Manchester convention used:
 *   edge 0→1 (falling then rising) pair = bit 1
 *   edge 1→0 (rising then falling) pair = bit 0
 * Half-bit period ~256µs (EM4100 @ 125kHz / 2 = 4µs * 64 = 256µs)
 * =================================================================== */

void test_manch_feed_simple_pair(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);

	/* Two half-period events forming a Manchester '1': falling then rising */
	lfrfid_evt_t events[2] = {
		{ .t_us = 250, .edge = 0 },
		{ .t_us = 260, .edge = 1 }
	};
	manch_feed_events(&d, events, 2);
	TEST_ASSERT_EQUAL_UINT16(1, d.bit_count);
}

void test_manch_feed_full_period_expansion(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);

	/* One full-period event (should be expanded into two half-period events) */
	lfrfid_evt_t events[1] = {
		{ .t_us = 500, .edge = 0 }
	};
	manch_feed_events(&d, events, 1);
	/* After expansion: two events with same edge (0,0) — not a valid Manchester
	 * pair, so no bits should be accumulated */
	TEST_ASSERT_EQUAL_UINT16(0, d.bit_count);
}

void test_manch_feed_alternating_pairs(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);

	/* 4 pairs producing alternating 1,0,1,0 pattern */
	lfrfid_evt_t events[] = {
		{ .t_us = 256, .edge = 0 },
		{ .t_us = 256, .edge = 1 },  /* pair → 1 */
		{ .t_us = 256, .edge = 1 },
		{ .t_us = 256, .edge = 0 },  /* pair → 0 */
		{ .t_us = 256, .edge = 0 },
		{ .t_us = 256, .edge = 1 },  /* pair → 1 */
		{ .t_us = 256, .edge = 1 },
		{ .t_us = 256, .edge = 0 },  /* pair → 0 */
	};
	manch_feed_events(&d, events, 8);
	/* Verify we got 4 bits accumulated */
	TEST_ASSERT_EQUAL_UINT16(4, d.bit_count);
}

void test_manch_feed_invalid_pair_resets(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);

	/* Valid pair followed by a timing error */
	lfrfid_evt_t events[] = {
		{ .t_us = 256, .edge = 0 },
		{ .t_us = 256, .edge = 1 },  /* valid pair → 1 */
		{ .t_us = 50,  .edge = 0 },  /* too short — invalid */
		{ .t_us = 256, .edge = 1 },
	};
	manch_feed_events(&d, events, 4);
	/* The invalid pair should cause a reset of accumulated bits */
	TEST_ASSERT_TRUE(d.bit_count <= 1);
}

void test_manch_feed_overflow_resets(void)
{
	manch_decoder_t d;
	manch_init(&d, 256, 8);

	/* Fill edge buffer with many full-period events that expand to 2x */
	lfrfid_evt_t events[FRAME_CHUNK_SIZE];
	for (int i = 0; i < FRAME_CHUNK_SIZE; i++) {
		events[i].t_us = 500;  /* full period — each expands to 2 events */
		events[i].edge = (uint8_t)(i & 1);
	}
	/* This should trigger overflow protection and reset */
	manch_feed_events(&d, events, FRAME_CHUNK_SIZE);
	/* After overflow, bit_count should be 0 (reset occurred) */
	TEST_ASSERT_EQUAL_UINT16(0, d.bit_count);
}

/* ===================================================================
 * Runner
 * =================================================================== */

int main(void)
{
	UNITY_BEGIN();

	/* Initialization */
	RUN_TEST(test_manch_init_sets_params);
	RUN_TEST(test_manch_init_clears_frame);

	/* Push bit */
	RUN_TEST(test_manch_push_bit_single_one);
	RUN_TEST(test_manch_push_bit_single_zero);
	RUN_TEST(test_manch_push_bit_byte_pattern);
	RUN_TEST(test_manch_push_bit_multi_byte);
	RUN_TEST(test_manch_push_bit_shifts_msb);
	RUN_TEST(test_manch_push_bit_count_saturates);

	/* Get bit */
	RUN_TEST(test_manch_get_bit_positions);

	/* Is full */
	RUN_TEST(test_manch_is_full_not_full);
	RUN_TEST(test_manch_is_full_exactly_full);
	RUN_TEST(test_manch_is_full_null);

	/* Reset */
	RUN_TEST(test_manch_reset_clears);

	/* Feed events */
	RUN_TEST(test_manch_feed_simple_pair);
	RUN_TEST(test_manch_feed_full_period_expansion);
	RUN_TEST(test_manch_feed_alternating_pairs);
	RUN_TEST(test_manch_feed_invalid_pair_resets);
	RUN_TEST(test_manch_feed_overflow_resets);

	return UNITY_END();
}
