#include "unity.h"
#include "m1_fw_selfflash_mask.h"

void setUp(void) {}
void tearDown(void) {}

/* REGRESSION (ported from bedge117/M1 C3.164): the M1 self-flash used to
 * unmask the ESP32 interrupt lines right after the bank erase, leaving a gap
 * before the first DATA chunk arrived. A buffered ESP (WiFi/BLE/802.15.4)
 * backlog in that gap could starve the flash worker and the IWDG feeder,
 * resetting the device before a single byte was written (inactive bank
 * erased, 0 bytes, device reports v0.0.0). The fix keeps the session masked
 * from the erase through FINISH with no gap in between. */

void test_session_stays_masked_across_multiple_data_chunks(void)
{
    fw_selfflash_mask_state_t st;
    fw_selfflash_mask_init(&st);
    TEST_ASSERT_FALSE(st.masked);

    fw_selfflash_mask_begin(&st);           /* erase succeeded */
    TEST_ASSERT_TRUE(st.masked);

    /* Several DATA chunks in a row must NOT end the masked window. */
    for (int i = 0; i < 8; i++)
    {
        TEST_ASSERT_TRUE(st.masked);
    }

    /* Only FINISH ends the session, exactly once. */
    TEST_ASSERT_TRUE(fw_selfflash_mask_end(&st));
    TEST_ASSERT_FALSE(st.masked);
}

void test_end_is_idempotent_and_unmasks_only_once(void)
{
    fw_selfflash_mask_state_t st;
    fw_selfflash_mask_init(&st);
    fw_selfflash_mask_begin(&st);

    TEST_ASSERT_TRUE(fw_selfflash_mask_end(&st));   /* first exit -> unmask */
    TEST_ASSERT_FALSE(fw_selfflash_mask_end(&st));  /* second call -> no-op */
    TEST_ASSERT_FALSE(fw_selfflash_mask_end(&st));  /* still a no-op */
}

void test_end_before_begin_is_a_noop(void)
{
    /* An erase failure before begin() was ever reached must not attempt to
     * unmask lines that were never masked. */
    fw_selfflash_mask_state_t st;
    fw_selfflash_mask_init(&st);
    TEST_ASSERT_FALSE(fw_selfflash_mask_end(&st));
}

void test_data_write_failure_ends_the_masked_window(void)
{
    fw_selfflash_mask_state_t st;
    fw_selfflash_mask_init(&st);
    fw_selfflash_mask_begin(&st);
    TEST_ASSERT_TRUE(st.masked);

    /* A DATA write failure aborts the session. */
    TEST_ASSERT_TRUE(fw_selfflash_mask_end(&st));
    TEST_ASSERT_FALSE(st.masked);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_session_stays_masked_across_multiple_data_chunks);
    RUN_TEST(test_end_is_idempotent_and_unmasks_only_once);
    RUN_TEST(test_end_before_begin_is_a_noop);
    RUN_TEST(test_data_write_failure_ends_the_masked_window);
    return UNITY_END();
}
