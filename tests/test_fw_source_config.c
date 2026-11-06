#include "unity.h"
#include "m1_fw_source.h"
#include "m1_file_browser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char s_temp_dir[] = "/tmp/m1-fwsrc-XXXXXX";

uint8_t m1_fb_make_dir(const char *directory)
{
    (void)directory;
    return 0;
}

uint8_t m1_fb_check_existence(const char *filedir)
{
    return access(filedir, F_OK) == 0;
}

http_status_t http_get(const char *url, char *response_buf, uint16_t buf_size,
                       uint8_t timeout_sec)
{
    (void)url;
    (void)response_buf;
    (void)buf_size;
    (void)timeout_sec;
    return HTTP_OK;
}

void setUp(void)
{
    strcpy(s_temp_dir, "/tmp/m1-fwsrc-XXXXXX");
    TEST_ASSERT_NOT_NULL(mkdtemp(s_temp_dir));
    TEST_ASSERT_EQUAL_INT(0, chdir(s_temp_dir));
    TEST_ASSERT_EQUAL_INT(0, mkdir("0:", 0700));
    TEST_ASSERT_EQUAL_INT(0, mkdir("0:/System", 0700));
}

void tearDown(void)
{
    TEST_ASSERT_EQUAL_INT(0, chdir("/"));
    (void)unlink("0:/System/fw_sources.txt");
    (void)rmdir("0:/System");
    (void)rmdir("0:");
    (void)rmdir(s_temp_dir);
}

static void write_config(const char *text)
{
    FILE *fp = fopen(FW_SOURCES_CONFIG_PATH, "wb");
    TEST_ASSERT_NOT_NULL(fp);
    TEST_ASSERT_EQUAL_size_t(strlen(text), fwrite(text, 1, strlen(text), fp));
    TEST_ASSERT_EQUAL_INT(0, fclose(fp));
}

static void test_existing_sources_gain_hapax_and_prefer_it(void)
{
    static const char old_config[] =
        "Source: C3 SPI Brain\n"
        "Type: github_release\n"
        "Owner: bedge117\n"
        "Repo: m1-esp32-brain\n"
        "Asset_filter: factory_m1-esp32-brain.bin\n"
        "Category: esp32\n"
        "Max_releases: 5\n"
        "\n"
        "Source: Deauth ESP32 AT\n"
        "Type: github_release\n"
        "Owner: neddy299\n"
        "Repo: esp32-at-monstatek-m1\n"
        "Asset_filter: .bin\n"
        "Category: esp32\n"
        "Max_releases: 5\n";
    write_config(old_config);

    fw_source_t sources[FW_SOURCE_MAX];
    TEST_ASSERT_EQUAL_UINT8(3, fw_source_load_config_filtered(sources, "esp32"));
    TEST_ASSERT_EQUAL_STRING("Hapax SPI Brain", sources[0].name);
    TEST_ASSERT_EQUAL_STRING("hapaxx11", sources[0].github.owner);
    TEST_ASSERT_EQUAL_STRING("m1-esp32-brain", sources[0].github.repo);
    TEST_ASSERT_EQUAL_STRING("C3 SPI Brain", sources[1].name);
    TEST_ASSERT_EQUAL_STRING("Deauth ESP32 AT", sources[2].name);
}

static void test_missing_file_defaults_prefer_hapax(void)
{
    fw_source_t sources[FW_SOURCE_MAX];
    TEST_ASSERT_EQUAL_UINT8(3, fw_source_load_config_filtered(sources, "esp32"));
    TEST_ASSERT_EQUAL_STRING("Hapax SPI Brain", sources[0].name);
    TEST_ASSERT_EQUAL_STRING("C3 SPI Brain", sources[1].name);
    TEST_ASSERT_EQUAL_STRING("Deauth ESP32 AT", sources[2].name);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_existing_sources_gain_hapax_and_prefer_it);
    RUN_TEST(test_missing_file_defaults_prefer_hapax);
    return UNITY_END();
}
