#include "unity.h"
#include "notify.h"

static uint32_t s_tick;
static uint16_t s_tone_frequency[8];
static uint16_t s_tone_duration[8];
static size_t s_tone_count;
static size_t s_led_on_count;
static size_t s_led_off_count;

void setUp(void) {
    s_tick = 0;
    s_tone_count = 0;
    s_led_on_count = 0;
    s_led_off_count = 0;
}

void tearDown(void) {}

uint32_t m1app_get_tick(void) {
    return s_tick;
}

void m1_buzzer_set(uint16_t frequency, uint16_t duration_ms) {
    if(s_tone_count < sizeof(s_tone_frequency) / sizeof(s_tone_frequency[0])) {
        s_tone_frequency[s_tone_count] = frequency;
        s_tone_duration[s_tone_count] = duration_ms;
    }
    s_tone_count++;
}

void lp5814_led_on_Blue(uint8_t pwm) {
    TEST_ASSERT_EQUAL_UINT8(255, pwm);
    s_led_on_count++;
}

void lp5814_all_off_RGB(void) {
    s_led_off_count++;
}

void test_receive_notification_advances_without_blocking(void) {
    ZeroMeshApp app = {0};
    app.notify_vibro = true;
    app.notify_led = true;
    app.notify_ringtone = RingtoneShort;

    notify_rx_message(&app);
    TEST_ASSERT_EQUAL_UINT(0, s_tone_count);

    notify_poll();
    TEST_ASSERT_EQUAL_UINT(1, s_tone_count);
    TEST_ASSERT_EQUAL_UINT(200, s_tone_frequency[0]);
    TEST_ASSERT_EQUAL_UINT(120, s_tone_duration[0]);

    notify_poll();
    TEST_ASSERT_EQUAL_UINT(1, s_tone_count);
    TEST_ASSERT_EQUAL_UINT(0, s_led_on_count);

    s_tick = 126;
    notify_poll();
    TEST_ASSERT_EQUAL_UINT(1, s_led_on_count);
    s_tick = 376;
    notify_poll();
    TEST_ASSERT_EQUAL_UINT(1, s_led_off_count);
    s_tick = 476;
    notify_poll();
    TEST_ASSERT_EQUAL_UINT(2, s_led_on_count);
    s_tick = 726;
    notify_poll();
    TEST_ASSERT_EQUAL_UINT(2, s_led_off_count);
    s_tick = 826;
    notify_poll();
    TEST_ASSERT_EQUAL_UINT(2, s_tone_count);
    TEST_ASSERT_EQUAL_UINT(800, s_tone_frequency[1]);
    TEST_ASSERT_EQUAL_UINT(100, s_tone_duration[1]);
    s_tick = 932;
    notify_poll();
    TEST_ASSERT_EQUAL_UINT(2, s_tone_count);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_receive_notification_advances_without_blocking);
    return UNITY_END();
}
