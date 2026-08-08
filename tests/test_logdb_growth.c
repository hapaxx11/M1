/* See COPYING.txt for license details. */

/*
 * test_logdb_growth.c
 *
 * Unit tests for m1_logdb_next_alloc_size() (m1_logdb_growth.h), the pure
 * buffer-growth decision used by m1_logdb_dyn_vsprintf() to size the
 * dynamic log-message buffer.
 *
 * Regression coverage: m1_logdb_dyn_vsprintf() used to store the growth
 * decision in a `uint8_t mem_size`. Any log message long enough that
 * `ret_n + 20` exceeded 255 wrapped back into the valid range, so the
 * loop kept reallocating a too-small buffer forever instead of giving up
 * — an infinite spin / crash on any log line >= ~235 bytes. Fixed by
 * using a plain `int` for the size arithmetic. test_grow_wraps_with_narrow_type
 * below reproduces the old bug using an explicit uint8_t cast to prove the
 * fixed (int-based) logic no longer wraps.
 *
 * Build with the host-side CMake:
 *   cmake -B build-tests -S tests && cmake --build build-tests
 *   ctest --test-dir build-tests --output-on-failure
 */

#include "unity.h"
#include "m1_logdb_growth.h"
#include <stdint.h>

#define M1_LOGDB_MESSAGE_SIZE 80
#define MAX_SIZE (2 * M1_LOGDB_MESSAGE_SIZE) /* 160, matches m1_log_debug.c */

void setUp(void) {}
void tearDown(void) {}

/* ===================================================================
 * DONE: vsnprintf() reported a length that fit in the current buffer.
 * =================================================================== */

void test_grow_done_when_ret_fits(void)
{
	int new_size = -1;
	m1_logdb_grow_action_t action = m1_logdb_next_alloc_size(40, M1_LOGDB_MESSAGE_SIZE, MAX_SIZE, &new_size);
	TEST_ASSERT_EQUAL(M1_LOGDB_GROW_DONE, action);
}

void test_grow_done_when_ret_is_cur_size_minus_one(void)
{
	int new_size = -1;
	m1_logdb_grow_action_t action = m1_logdb_next_alloc_size(M1_LOGDB_MESSAGE_SIZE - 1, M1_LOGDB_MESSAGE_SIZE, MAX_SIZE, &new_size);
	TEST_ASSERT_EQUAL(M1_LOGDB_GROW_DONE, action);
}

/* ===================================================================
 * RETRY: message doesn't fit yet, but a bigger buffer stays under max.
 * =================================================================== */

void test_grow_retry_computes_ret_plus_20(void)
{
	int new_size = -1;
	m1_logdb_grow_action_t action = m1_logdb_next_alloc_size(100, M1_LOGDB_MESSAGE_SIZE, MAX_SIZE, &new_size);
	TEST_ASSERT_EQUAL(M1_LOGDB_GROW_RETRY, action);
	TEST_ASSERT_EQUAL_INT(120, new_size); /* 100 + 20 */
}

void test_grow_retry_just_under_max(void)
{
	int new_size = -1;
	/* ret_n + 20 == MAX_SIZE exactly -> still a valid retry size */
	m1_logdb_grow_action_t action = m1_logdb_next_alloc_size(MAX_SIZE - 20, M1_LOGDB_MESSAGE_SIZE, MAX_SIZE, &new_size);
	TEST_ASSERT_EQUAL(M1_LOGDB_GROW_RETRY, action);
	TEST_ASSERT_EQUAL_INT(MAX_SIZE, new_size);
}

/* ===================================================================
 * GIVE_UP: requested size exceeds max, or vsnprintf() itself failed.
 * =================================================================== */

void test_grow_give_up_when_over_max(void)
{
	int new_size = -1;
	m1_logdb_grow_action_t action = m1_logdb_next_alloc_size(MAX_SIZE, M1_LOGDB_MESSAGE_SIZE, MAX_SIZE, &new_size);
	TEST_ASSERT_EQUAL(M1_LOGDB_GROW_GIVE_UP, action);
}

void test_grow_give_up_on_vsnprintf_error(void)
{
	int new_size = -1;
	m1_logdb_grow_action_t action = m1_logdb_next_alloc_size(-1, M1_LOGDB_MESSAGE_SIZE, MAX_SIZE, &new_size);
	TEST_ASSERT_EQUAL(M1_LOGDB_GROW_GIVE_UP, action);
}

/* ===================================================================
 * Regression: the historical uint8_t overflow bug.
 *
 * With the old `uint8_t mem_size`, a message needing (ret_n + 20) > 255
 * wraps modulo 256 back into the "still <= max_size" range, so the
 * buggy code kept retrying with a too-small size forever. The int-based
 * m1_logdb_next_alloc_size() must correctly GIVE_UP instead.
 * =================================================================== */

void test_grow_large_message_gives_up_without_wrap(void)
{
	int new_size = -1;
	/* ret_n = 250 -> ret_n + 20 = 270, which overflows uint8_t (270 % 256 = 14). */
	int ret_n = 250;
	m1_logdb_grow_action_t action = m1_logdb_next_alloc_size(ret_n, M1_LOGDB_MESSAGE_SIZE, MAX_SIZE, &new_size);

	/* The fixed logic must GIVE_UP because 270 > MAX_SIZE (160). */
	TEST_ASSERT_EQUAL(M1_LOGDB_GROW_GIVE_UP, action);

	/* Demonstrate the historical bug directly: truncating the same
	   arithmetic to uint8_t wraps 270 down to 14, which is < MAX_SIZE
	   and would have been misread as a valid (too-small) retry size. */
	uint8_t buggy_narrow_size = (uint8_t)(ret_n + 20);
	TEST_ASSERT_EQUAL_UINT8(14, buggy_narrow_size);
	TEST_ASSERT_TRUE(buggy_narrow_size <= MAX_SIZE); /* would have passed the buggy check */
}

int main(void)
{
	UNITY_BEGIN();

	RUN_TEST(test_grow_done_when_ret_fits);
	RUN_TEST(test_grow_done_when_ret_is_cur_size_minus_one);
	RUN_TEST(test_grow_retry_computes_ret_plus_20);
	RUN_TEST(test_grow_retry_just_under_max);
	RUN_TEST(test_grow_give_up_when_over_max);
	RUN_TEST(test_grow_give_up_on_vsnprintf_error);
	RUN_TEST(test_grow_large_message_gives_up_without_wrap);

	return UNITY_END();
}
