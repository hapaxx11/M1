#include "unity.h"
#include "ble_findmy.h"

void setUp(void) { }
void tearDown(void) { }

static void test_findmy_layout_and_address(void)
{
    uint8_t key[BLE_FINDMY_KEY_LEN];
    uint8_t adv[BLE_FINDMY_ADV_LEN];
    uint8_t addr[6];
    for (uint8_t i = 0; i < sizeof(key); i++) key[i] = i;

    ble_findmy_build(key, adv, addr);

    const uint8_t prefix[] = { 0x1E, 0xFF, 0x4C, 0x00, 0x12, 0x19, 0x00 };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(prefix, adv, sizeof(prefix));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(&key[6], &adv[7], 22);
    TEST_ASSERT_EQUAL_UINT8(key[0] >> 6, adv[29]);
    TEST_ASSERT_EQUAL_UINT8(0, adv[30]);
    const uint8_t expected_addr[] = { 5, 4, 3, 2, 1, 0xC0 };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected_addr, addr, 6);
}

static void test_findmy_preserves_random_static_bits(void)
{
    uint8_t key[BLE_FINDMY_KEY_LEN] = {0};
    uint8_t adv[BLE_FINDMY_ADV_LEN];
    uint8_t addr[6];
    key[0] = 0xFF;
    ble_findmy_build(key, adv, addr);
    TEST_ASSERT_EQUAL_UINT8(0, addr[0]);
    TEST_ASSERT_EQUAL_UINT8(0xFF, addr[5]);
    TEST_ASSERT_EQUAL_UINT8(3, adv[29]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_findmy_layout_and_address);
    RUN_TEST(test_findmy_preserves_random_static_bits);
    return UNITY_END();
}
