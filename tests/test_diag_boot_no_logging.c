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

static void read_text(const char *relative, char *out, size_t out_size)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", M1_ROOT, relative);
    FILE *fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);
    size_t len = fread(out, 1, out_size - 1u, fp);
    fclose(fp);
    TEST_ASSERT_LESS_THAN(out_size - 1u, len);
    out[len] = '\0';
}

void test_captured_faults_take_precedence_over_rpc_activity(void)
{
    static char source[32768];
    read_text("m1_csrc/m1_diag.c", source, sizeof(source));
    char *assertion = strstr(source, "prev_fault == M1_DIAG_FAULT_ASSERT");
    char *other_fault = strstr(source, "prev_fault != M1_DIAG_FAULT_NONE");
    char *origin = strstr(source, "prev_origin >= M1_DIAG_DETAIL_RPC_NOTIFY");
    TEST_ASSERT_NOT_NULL(assertion);
    TEST_ASSERT_NOT_NULL(other_fault);
    TEST_ASSERT_NOT_NULL(origin);
    TEST_ASSERT_LESS_THAN(origin, assertion);
    TEST_ASSERT_LESS_THAN(origin, other_fault);
}

void test_hardfault_uses_naked_entry_and_c_helper(void)
{
    static char source[65536];
    read_text("Core/Src/stm32h5xx_it.c", source, sizeof(source));
    TEST_ASSERT_NOT_NULL(strstr(source,
        "__attribute__((naked)) void HardFault_Handler(void)"));
    TEST_ASSERT_NOT_NULL(strstr(source, "B HardFault_Handler_C"));
    TEST_ASSERT_NOT_NULL(strstr(source,
        "void HardFault_Handler_C(uint32_t *stack_frame, uint32_t exc_return)"));
}

void test_rpc_watchdog_debug_calls_are_feature_guarded(void)
{
    static char source[65536];
    read_text("m1_csrc/m1_watchdog.c", source, sizeof(source));
    char *guard = strstr(source, "#ifdef M1_APP_RPC_ENABLE");
    char *capture = strstr(source, "m1_diag_wdt_capture(");
    char *fallback = strstr(source,
        "m1_diag_wdt_capture(wdt_id, wdt_rt, UINT32_MAX, 0u, 0);");
    TEST_ASSERT_NOT_NULL(guard);
    TEST_ASSERT_NOT_NULL(capture);
    TEST_ASSERT_NOT_NULL(fallback);
    TEST_ASSERT_LESS_THAN(capture, guard);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_boot_report_does_not_call_logging_subsystem);
    RUN_TEST(test_previous_boot_snapshot_precedes_all_stage_updates);
    RUN_TEST(test_captured_faults_take_precedence_over_rpc_activity);
    RUN_TEST(test_hardfault_uses_naked_entry_and_c_helper);
    RUN_TEST(test_rpc_watchdog_debug_calls_are_feature_guarded);
    return UNITY_END();
}
