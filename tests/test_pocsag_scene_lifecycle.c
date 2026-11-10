/* See COPYING.txt for license details. */

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

void setUp(void) {}
void tearDown(void) {}

void test_pocsag_pulse_drain_is_bounded(void)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/m1_csrc/m1_subghz_scene.c", M1_ROOT);
    FILE *fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_END));
    long size = ftell(fp);
    TEST_ASSERT_GREATER_THAN_INT(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_SET));
    char content[32768];
    TEST_ASSERT_LESS_THAN_INT(sizeof(content), size);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(content, 1U, (size_t)size, fp));
    content[size] = '\0';
    fclose(fp);

    const char *scene = strstr(content, "if (subghz_scene_current(&app) == SubGhzScenePocsag)");
    TEST_ASSERT_NOT_NULL(scene);
    TEST_ASSERT_NOT_NULL(strstr(content, "SUBGHZ_POCSAG_PULSE_BATCH_LIMIT 64U"));
    TEST_ASSERT_NOT_NULL(strstr(scene, "for (uint16_t pulse_count = 0;"));
    TEST_ASSERT_NOT_NULL(strstr(scene,
        "pulse_count < SUBGHZ_POCSAG_PULSE_BATCH_LIMIT"));
    TEST_ASSERT_NOT_NULL(strstr(scene,
        "if (pulse_count >= SUBGHZ_POCSAG_PULSE_BATCH_LIMIT)"));
    TEST_ASSERT_NOT_NULL(strstr(scene, "vTaskDelay(pdMS_TO_TICKS(1U));"));
    TEST_ASSERT_NULL(strstr(scene, "} while (true);"));
}

void test_recording_stop_drains_and_allocation_failure_cleans_up(void)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/m1_csrc/m1_sub_ghz.c", M1_ROOT);
    FILE *fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_END));
    long size = ftell(fp);
    TEST_ASSERT_GREATER_THAN_INT(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_SET));
    char content[300000];
    TEST_ASSERT_LESS_THAN_INT(sizeof(content), size);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(content, 1U, (size_t)size, fp));
    content[size] = '\0';
    fclose(fp);

    const char *flush = strstr(content, "uint32_t sub_ghz_raw_recording_flush_ext(void)");
    TEST_ASSERT_NOT_NULL(flush);
    TEST_ASSERT_NOT_NULL(strstr(flush, "if (sub_ghz_rx_raw_save(false, false) != 0U)"));
    TEST_ASSERT_NOT_NULL(strstr(flush, "return 0U;"));

    const char *stop = strstr(content, "void sub_ghz_raw_recording_stop_ext(void)");
    TEST_ASSERT_NOT_NULL(stop);
    TEST_ASSERT_NOT_NULL(strstr(stop, "while (sub_ghz_raw_recording_flush_ext() > 0U)"));
    TEST_ASSERT_NOT_NULL(strstr(stop, "sub_ghz_rx_raw_save(false, true);"));
    TEST_ASSERT_NOT_NULL(strstr(stop, "m1_sdm_task_stop();"));
    TEST_ASSERT_NOT_NULL(strstr(stop, "m1_sdm_task_deinit();"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pocsag_pulse_drain_is_bounded);
    RUN_TEST(test_recording_stop_drains_and_allocation_failure_cleans_up);
    return UNITY_END();
}
