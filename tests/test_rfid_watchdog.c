/* See COPYING.txt for license details. */

#include "unity.h"
#include "m1_rfid_watchdog.h"
#include <stddef.h>

void setUp(void) {}
void tearDown(void) {}

static unsigned int kick_count;

static void record_kick(void)
{
	kick_count++;
}

void test_processed_scan_message_requires_watchdog_kick(void)
{
	kick_count = 0;
	m1_rfid_scan_watchdog_kick(true, record_kick);
	TEST_ASSERT_EQUAL_UINT(1, kick_count);
}

void test_idle_scan_iteration_does_not_require_watchdog_kick(void)
{
	kick_count = 0;
	m1_rfid_scan_watchdog_kick(false, record_kick);
	TEST_ASSERT_EQUAL_UINT(0, kick_count);
}

void test_missing_kick_callback_is_safe(void)
{
	m1_rfid_scan_watchdog_kick(true, NULL);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_processed_scan_message_requires_watchdog_kick);
	RUN_TEST(test_idle_scan_iteration_does_not_require_watchdog_kick);
	RUN_TEST(test_missing_kick_callback_is_safe);
	return UNITY_END();
}
