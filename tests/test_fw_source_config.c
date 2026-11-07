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
    TEST_ASSERT_EQUAL_STRING("factory_ESP32C6-SPI.bin", sources[0].asset_filter);
    TEST_ASSERT_EQUAL_STRING("C3 SPI Brain", sources[1].name);
    TEST_ASSERT_EQUAL_STRING("Deauth ESP32 AT", sources[2].name);
}

static void test_missing_file_defaults_prefer_hapax(void)
{
    fw_source_t sources[FW_SOURCE_MAX];
    TEST_ASSERT_EQUAL_UINT8(3, fw_source_load_config_filtered(sources, "esp32"));
    TEST_ASSERT_EQUAL_STRING("Hapax SPI Brain", sources[0].name);
    TEST_ASSERT_EQUAL_STRING("factory_ESP32C6-SPI.bin", sources[0].asset_filter);
    TEST_ASSERT_EQUAL_STRING("C3 SPI Brain", sources[1].name);
    TEST_ASSERT_EQUAL_STRING("Deauth ESP32 AT", sources[2].name);
}

static void write_custom_sources(uint8_t count, bool hapax_last)
{
    char text[8192] = {0};
    for (uint8_t i = 0; i < count; i++) {
        char block[384];
        if (hapax_last && i == count - 1) {
            snprintf(block, sizeof(block),
                "Source: Hapax SPI Brain\n"
                "Type: github_release\n"
                "Owner: hapaxx11\n"
                "Repo: m1-esp32-brain\n"
                "Asset_filter: factory_m1-esp32-brain.bin\n"
                "Category: esp32\n"
                "Max_releases: 5\n\n");
        } else {
            snprintf(block, sizeof(block),
                "Source: Custom %u\n"
                "Type: github_release\n"
                "Owner: custom-owner\n"
                "Repo: custom-repo\n"
                "Asset_filter: .bin\n"
                "Category: esp32\n"
                "Max_releases: 5\n\n", i);
        }
        strncat(text, block, sizeof(text) - strlen(text) - 1);
    }
    write_config(text);
}

static void assert_source_occurrences(const char *name, int expected)
{
    FILE *fp = fopen(FW_SOURCES_CONFIG_PATH, "rb");
    TEST_ASSERT_NOT_NULL(fp);
    char text[8192];
    size_t len = fread(text, 1, sizeof(text) - 1, fp);
    fclose(fp);
    text[len] = '\0';
    int count = 0;
    for (const char *p = text; (p = strstr(p, name)) != NULL; p++) count++;
    TEST_ASSERT_EQUAL_INT(expected, count);
}

static void test_seven_sources_migrate_only_once(void)
{
    write_custom_sources(7, false);
    for (int pass = 0; pass < 2; pass++) {
        fw_source_t sources[FW_SOURCE_MAX];
        uint8_t loaded = fw_source_load_config_filtered(sources, "esp32");
        TEST_ASSERT_EQUAL_UINT8(8, loaded);
        TEST_ASSERT_EQUAL_STRING("Hapax SPI Brain", sources[0].name);
        assert_source_occurrences("Source: Hapax SPI Brain", 1);
        assert_source_occurrences("Source: Custom ", 7);
    }
}

static void test_full_custom_catalog_is_not_discarded_or_duplicated(void)
{
    write_custom_sources(8, false);
    for (int pass = 0; pass < 2; pass++) {
        fw_source_t sources[FW_SOURCE_MAX];
        TEST_ASSERT_EQUAL_UINT8(8, fw_source_load_config_filtered(sources, "esp32"));
        assert_source_occurrences("Source: Hapax SPI Brain", 0);
        assert_source_occurrences("Source: Custom ", 8);
    }
}

static void test_full_catalog_detects_hapax_beyond_loaded_sources(void)
{
    write_custom_sources(9, true);
    for (int pass = 0; pass < 2; pass++) {
        fw_source_t sources[FW_SOURCE_MAX];
        TEST_ASSERT_EQUAL_UINT8(8, fw_source_load_config_filtered(sources, "esp32"));
        TEST_ASSERT_EQUAL_STRING("Custom 0", sources[0].name);
        assert_source_occurrences("Source: Hapax SPI Brain", 1);
        assert_source_occurrences("Source: Custom ", 8);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_existing_sources_gain_hapax_and_prefer_it);
    RUN_TEST(test_missing_file_defaults_prefer_hapax);
    RUN_TEST(test_seven_sources_migrate_only_once);
    RUN_TEST(test_full_custom_catalog_is_not_discarded_or_duplicated);
    RUN_TEST(test_full_catalog_detects_hapax_beyond_loaded_sources);
    return UNITY_END();
}
