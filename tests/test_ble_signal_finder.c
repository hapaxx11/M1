#include "unity.h"
#include "ble_signal_finder.h"

void setUp(void) { }
void tearDown(void) { }

void test_matches_same_address_and_type(void)
{
    const uint8_t target[6] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
    const uint8_t candidate[6] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};

    TEST_ASSERT_TRUE(ble_signal_finder_matches(target, 1, candidate, 1));
}

void test_does_not_match_different_address_or_type(void)
{
    const uint8_t target[6] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB};
    const uint8_t different_address[6] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAC};

    TEST_ASSERT_FALSE(ble_signal_finder_matches(target, 1, different_address, 1));
    TEST_ASSERT_FALSE(ble_signal_finder_matches(target, 1, target, 0));
    TEST_ASSERT_FALSE(ble_signal_finder_matches(NULL, 1, target, 1));
    TEST_ASSERT_FALSE(ble_signal_finder_matches(target, 1, NULL, 1));
}

void test_strength_maps_rssi_thresholds(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, ble_signal_finder_strength(-96));
    TEST_ASSERT_EQUAL_UINT8(1, ble_signal_finder_strength(-95));
    TEST_ASSERT_EQUAL_UINT8(1, ble_signal_finder_strength(-81));
    TEST_ASSERT_EQUAL_UINT8(2, ble_signal_finder_strength(-80));
    TEST_ASSERT_EQUAL_UINT8(2, ble_signal_finder_strength(-68));
    TEST_ASSERT_EQUAL_UINT8(3, ble_signal_finder_strength(-67));
    TEST_ASSERT_EQUAL_UINT8(3, ble_signal_finder_strength(-56));
    TEST_ASSERT_EQUAL_UINT8(4, ble_signal_finder_strength(-55));
    TEST_ASSERT_EQUAL_UINT8(4, ble_signal_finder_strength(0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_matches_same_address_and_type);
    RUN_TEST(test_does_not_match_different_address_or_type);
    RUN_TEST(test_strength_maps_rssi_thresholds);
    return UNITY_END();
}
