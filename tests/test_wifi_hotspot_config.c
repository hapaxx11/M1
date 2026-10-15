/* See COPYING.txt for license details. */

#include "unity.h"
#include "wifi_hotspot_config.h"

#include <string.h>

void setUp(void) {}
void tearDown(void) {}

void test_validate_accepts_boundary_values(void)
{
    char ssid[WIFI_HOTSPOT_SSID_MAX_LEN + 1u];
    char password[WIFI_HOTSPOT_PASSWORD_MAX_LEN + 1u];
    memset(ssid, 's', sizeof(ssid) - 1u);
    ssid[sizeof(ssid) - 1u] = '\0';
    memset(password, 'p', sizeof(password) - 1u);
    password[sizeof(password) - 1u] = '\0';

    TEST_ASSERT_TRUE(wifi_hotspot_config_validate("M1", ""));
    TEST_ASSERT_TRUE(wifi_hotspot_config_validate(ssid, password));
}

void test_validate_rejects_invalid_lengths_and_control_characters(void)
{
    char long_ssid[WIFI_HOTSPOT_SSID_MAX_LEN + 2u];
    memset(long_ssid, 's', sizeof(long_ssid) - 1u);
    long_ssid[sizeof(long_ssid) - 1u] = '\0';

    TEST_ASSERT_FALSE(wifi_hotspot_config_validate("", "password"));
    TEST_ASSERT_FALSE(wifi_hotspot_config_validate(long_ssid, "password"));
    TEST_ASSERT_FALSE(wifi_hotspot_config_validate("M1", "short"));
    TEST_ASSERT_FALSE(wifi_hotspot_config_validate("M1", "bad\npass"));
    TEST_ASSERT_FALSE(wifi_hotspot_config_validate("bad\rssid", "password"));
    TEST_ASSERT_FALSE(wifi_hotspot_config_validate(NULL, "password"));
    TEST_ASSERT_FALSE(wifi_hotspot_config_validate("M1", NULL));
}

void test_format_and_parse_round_trip(void)
{
    char text[WIFI_HOTSPOT_CONFIG_TEXT_MAX + 1u];
    char ssid[WIFI_HOTSPOT_SSID_MAX_LEN + 1u];
    char password[WIFI_HOTSPOT_PASSWORD_MAX_LEN + 1u];
    size_t len = wifi_hotspot_config_format(text, sizeof(text),
                                            "M1=Hotspot", "pass,word123");

    TEST_ASSERT_GREATER_THAN_UINT(0u, len);
    TEST_ASSERT_TRUE(wifi_hotspot_config_parse(text, len, ssid, sizeof(ssid),
                                               password, sizeof(password)));
    TEST_ASSERT_EQUAL_STRING("M1=Hotspot", ssid);
    TEST_ASSERT_EQUAL_STRING("pass,word123", password);
}

void test_parse_accepts_crlf_and_empty_open_password(void)
{
    static const char config[] = "ssid=M1\r\npassword=\r\n";
    char ssid[WIFI_HOTSPOT_SSID_MAX_LEN + 1u];
    char password[WIFI_HOTSPOT_PASSWORD_MAX_LEN + 1u];

    TEST_ASSERT_TRUE(wifi_hotspot_config_parse(config, sizeof(config) - 1u,
                                               ssid, sizeof(ssid),
                                               password, sizeof(password)));
    TEST_ASSERT_EQUAL_STRING("M1", ssid);
    TEST_ASSERT_EQUAL_STRING("", password);
}

void test_parse_rejects_duplicate_missing_unknown_and_truncated_fields(void)
{
    static const char duplicate[] =
        "ssid=M1\nssid=M2\npassword=password\n";
    static const char missing[] = "ssid=M1\n";
    static const char unknown[] = "ssid=M1\npassword=password\nother=x\n";
    char ssid[WIFI_HOTSPOT_SSID_MAX_LEN + 1u];
    char password[WIFI_HOTSPOT_PASSWORD_MAX_LEN + 1u];

    TEST_ASSERT_FALSE(wifi_hotspot_config_parse(duplicate, sizeof(duplicate) - 1u,
                                                ssid, sizeof(ssid),
                                                password, sizeof(password)));
    TEST_ASSERT_FALSE(wifi_hotspot_config_parse(missing, sizeof(missing) - 1u,
                                                ssid, sizeof(ssid),
                                                password, sizeof(password)));
    TEST_ASSERT_FALSE(wifi_hotspot_config_parse(unknown, sizeof(unknown) - 1u,
                                                ssid, sizeof(ssid),
                                                password, sizeof(password)));
    TEST_ASSERT_FALSE(wifi_hotspot_config_parse("ssid=Long\npassword=password\n",
                                                27u, ssid, 4u,
                                                password, sizeof(password)));
    TEST_ASSERT_EQUAL_STRING("", ssid);
    TEST_ASSERT_EQUAL_STRING("", password);
}

void test_parse_rejects_embedded_nul(void)
{
    static const char config[] = {
        's', 's', 'i', 'd', '=', 'M', '1', '\n',
        'p', 'a', 's', 's', 'w', 'o', 'r', 'd', '=',
        'p', 'a', 's', 's', '\0', 'w', 'o', 'r', 'd', '\n'
    };
    char ssid[WIFI_HOTSPOT_SSID_MAX_LEN + 1u];
    char password[WIFI_HOTSPOT_PASSWORD_MAX_LEN + 1u];

    TEST_ASSERT_FALSE(wifi_hotspot_config_parse(config, sizeof(config),
                                                ssid, sizeof(ssid),
                                                password, sizeof(password)));
}

void test_format_rejects_invalid_or_short_output(void)
{
    char text[8];
    TEST_ASSERT_EQUAL_UINT(0u,
        wifi_hotspot_config_format(text, sizeof(text), "M1", "password"));
    TEST_ASSERT_EQUAL_CHAR('\0', text[0]);
    TEST_ASSERT_EQUAL_UINT(0u,
        wifi_hotspot_config_format(NULL, 0u, "M1", "password"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_validate_accepts_boundary_values);
    RUN_TEST(test_validate_rejects_invalid_lengths_and_control_characters);
    RUN_TEST(test_format_and_parse_round_trip);
    RUN_TEST(test_parse_accepts_crlf_and_empty_open_password);
    RUN_TEST(test_parse_rejects_duplicate_missing_unknown_and_truncated_fields);
    RUN_TEST(test_parse_rejects_embedded_nul);
    RUN_TEST(test_format_rejects_invalid_or_short_output);
    return UNITY_END();
}
