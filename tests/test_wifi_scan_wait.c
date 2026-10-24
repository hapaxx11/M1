/* See COPYING.txt for license details. */

/*
 * test_wifi_scan_wait.c
 *
 * Regression coverage for the brain-CD3 async WiFi scan wait in
 * wifi_do_scan() (m1_wifi.c):
 *
 *   - Pure-logic tests for wifi_scan_wait_decide() (m1_csrc/wifi_scan_wait.h),
 *     which encodes the safety rule that scan-job resources may only be
 *     freed once the worker has signalled completion.
 *   - Source-level invariants for the orphaned-job path: a worker that
 *     outlives the cancel+drain window must have its job orphaned (kept
 *     alive) rather than freed under it (use-after-free), and orphans are
 *     reaped on a later scan only after the worker signals.
 *
 * A worker genuinely blocked past the drain deadline cannot be reproduced
 * on the host (the test stubs run tasks synchronously), so the
 * blocked-worker case is pinned by the decision table plus the
 * source-level orphan/reap invariants below.
 */

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wifi_scan_wait.h"

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

static char *read_file(const char *relpath)
{
    char path[512];
    FILE *fp;
    long size;
    char *buf;

    snprintf(path, sizeof(path), "%s/%s", M1_ROOT, relpath);
    fp = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, path);

    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_END));
    size = ftell(fp);
    TEST_ASSERT_GREATER_THAN_INT(0, size);
    TEST_ASSERT_EQUAL_INT(0, fseek(fp, 0, SEEK_SET));

    buf = (char *)malloc((size_t)size + 1U);
    TEST_ASSERT_NOT_NULL(buf);
    TEST_ASSERT_EQUAL_size_t((size_t)size, fread(buf, 1U, (size_t)size, fp));
    buf[size] = '\0';
    fclose(fp);

    /* Normalise CRLF -> LF (Windows core.autocrlf=true checkouts). */
    {
        char *rd = buf;
        char *wr = buf;
        while (*rd != '\0') {
            if (*rd != '\r')
                *wr++ = *rd;
            rd++;
        }
        *wr = '\0';
    }
    return buf;
}

void setUp(void)    {}
void tearDown(void) {}

/* ----- wifi_scan_wait_decide(): all four input combinations -------------- */

void test_decide_completed_within_bound(void)
{
    TEST_ASSERT_EQUAL(WIFI_SCAN_WAIT_COMPLETED,
                      wifi_scan_wait_decide(true, false));
}

void test_decide_completed_within_bound_and_drain(void)
{
    /* done_in_drain is never evaluated when the bound succeeded, but the
     * decision must stay COMPLETED regardless. */
    TEST_ASSERT_EQUAL(WIFI_SCAN_WAIT_COMPLETED,
                      wifi_scan_wait_decide(true, true));
}

void test_decide_completed_during_drain(void)
{
    TEST_ASSERT_EQUAL(WIFI_SCAN_WAIT_COMPLETED,
                      wifi_scan_wait_decide(false, true));
}

void test_decide_orphaned_when_worker_never_signals(void)
{
    /* The blocked-past-drain-deadline case: resources must stay alive. */
    TEST_ASSERT_EQUAL(WIFI_SCAN_WAIT_ORPHANED,
                      wifi_scan_wait_decide(false, false));
}

/* ----- m1_wifi.c: orphan path frees nothing under a live worker ----------- */

void test_orphan_path_frees_no_worker_resources(void)
{
    char *content = read_file("m1_csrc/m1_wifi.c");

    /* Slice the orphan branch: from the decision check to the end of
     * wifi_do_scan()'s RPC section. */
    const char *branch = strstr(content, "WIFI_SCAN_WAIT_ORPHANED) {");
    TEST_ASSERT_NOT_NULL_MESSAGE(branch, "orphan branch not found");
    const char *branch_end = strstr(branch, "return 0u;");
    TEST_ASSERT_NOT_NULL(branch_end);

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(branch, "wifi_scan_job_orphan(job);"),
        "drain-expired job must be orphaned, not freed");

    /* Nothing in the orphan branch may release worker-referenced memory. */
    for (const char *p = branch; p < branch_end; p++) {
        TEST_ASSERT_FALSE_MESSAGE(strncmp(p, "free(", 5) == 0,
            "orphan branch must not free job resources");
        TEST_ASSERT_FALSE_MESSAGE(strncmp(p, "m1_esp32_rpc_wifi_scan_async_cleanup", 36) == 0,
            "orphan branch must not delete the worker semaphore");
    }

    free(content);
}

void test_orphans_are_reaped_only_after_worker_signals(void)
{
    char *content = read_file("m1_csrc/m1_wifi.c");

    /* The reaper must poll for the completion signal BEFORE cleaning up,
     * and wifi_do_scan() must call it before starting a new scan. */
    const char *reaper = strstr(content, "wifi_scan_job_reap_orphans(void)");
    TEST_ASSERT_NOT_NULL(reaper);
    const char *poll    = strstr(reaper, "m1_esp32_rpc_wifi_scan_async_poll(&job->ctx)");
    const char *cleanup = strstr(reaper, "m1_esp32_rpc_wifi_scan_async_cleanup(&job->ctx)");
    TEST_ASSERT_NOT_NULL(poll);
    TEST_ASSERT_NOT_NULL(cleanup);
    TEST_ASSERT_TRUE_MESSAGE(poll < cleanup,
        "reaper must wait for the worker signal before freeing");

    const char *scan_fn = strstr(content, "static uint16_t wifi_do_scan(void)");
    const char *reap_call = scan_fn
        ? strstr(scan_fn, "wifi_scan_job_reap_orphans();") : NULL;
    const char *job_alloc = scan_fn
        ? strstr(scan_fn, "malloc(sizeof(wifi_scan_job_t))") : NULL;
    TEST_ASSERT_NOT_NULL(scan_fn);
    TEST_ASSERT_NOT_NULL(reap_call);
    TEST_ASSERT_NOT_NULL(job_alloc);
    TEST_ASSERT_TRUE_MESSAGE(reap_call < job_alloc,
        "orphans must be reaped before a new scan job is allocated");

    free(content);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_decide_completed_within_bound);
    RUN_TEST(test_decide_completed_within_bound_and_drain);
    RUN_TEST(test_decide_completed_during_drain);
    RUN_TEST(test_decide_orphaned_when_worker_never_signals);
    RUN_TEST(test_orphan_path_frees_no_worker_resources);
    RUN_TEST(test_orphans_are_reaped_only_after_worker_signals);
    return UNITY_END();
}
