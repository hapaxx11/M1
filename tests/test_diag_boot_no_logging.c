#include "unity.h"
#include <stdio.h>
#include <string.h>

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

void setUp(void) {}
void tearDown(void) {}

void test_boot_report_does_not_call_logging_subsystem(void)
{
    char path[512];
    char source[32768];
    FILE *fp;
    size_t len;
    char *fn;
    char *end;
    char *log_call;

    snprintf(path, sizeof(path), "%s/m1_csrc/m1_diag.c", M1_ROOT);
    fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    len = fread(source, 1, sizeof(source) - 1u, fp);
    fclose(fp);
    TEST_ASSERT_LESS_THAN(sizeof(source) - 1u, len);
    source[len] = '\0';

    fn = strstr(source, "void m1_diag_boot_report(void)");
    TEST_ASSERT_NOT_NULL(fn);
    end = strstr(fn, "void m1_diag_fault_capture(");
    TEST_ASSERT_NOT_NULL(end);
    log_call = strstr(fn, "M1_LOG_");
    TEST_ASSERT_TRUE(log_call == NULL || log_call >= end);
}

void test_previous_boot_snapshot_precedes_all_stage_updates(void)
{
    char path[512];
    char source[65536];
    FILE *fp;
    size_t len;
    char *snapshot;
    char *stage;

    snprintf(path, sizeof(path), "%s/Core/Src/main.c", M1_ROOT);
    fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    len = fread(source, 1, sizeof(source) - 1u, fp);
    fclose(fp);
    TEST_ASSERT_LESS_THAN(sizeof(source) - 1u, len);
    source[len] = '\0';

    snapshot = strstr(source, "m1_diag_capture_previous();");
    stage = strstr(source, "m1_diag_set_boot_stage(M1_DIAG_BOOT_MAIN);");
    TEST_ASSERT_NOT_NULL(snapshot);
    TEST_ASSERT_NOT_NULL(stage);
    TEST_ASSERT_LESS_THAN(stage, snapshot);
}

void test_hardfault_entry_captures_stack_before_c_handler(void)
{
    char path[512];
    char source[32768];
    FILE *fp;
    size_t len;
    char *handler;
    char *end;

    snprintf(path, sizeof(path), "%s/Core/Src/stm32h5xx_it.c", M1_ROOT);
    fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    len = fread(source, 1, sizeof(source) - 1u, fp);
    fclose(fp);
    TEST_ASSERT_LESS_THAN(sizeof(source) - 1u, len);
    source[len] = '\0';

    handler = strstr(source, "__attribute__((naked)) void HardFault_Handler(void)");
    TEST_ASSERT_NOT_NULL(handler);
    end = strstr(handler, "void MemManage_Handler(void)");
    TEST_ASSERT_NOT_NULL(end);
    TEST_ASSERT_NOT_NULL(strstr(handler, "MRSEQ R0, MSP"));
    TEST_ASSERT_NOT_NULL(strstr(handler, "MRSNE R0, PSP"));
    TEST_ASSERT_NOT_NULL(strstr(handler, "MOV R1, LR"));
    TEST_ASSERT_NOT_NULL(strstr(handler, "B m1_diag_hardfault_capture"));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_boot_report_does_not_call_logging_subsystem);
    RUN_TEST(test_previous_boot_snapshot_precedes_all_stage_updates);
    RUN_TEST(test_hardfault_entry_captures_stack_before_c_handler);
    return UNITY_END();
}
