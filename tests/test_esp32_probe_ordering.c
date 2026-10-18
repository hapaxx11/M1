/* See COPYING.txt for license details. */

/*
 * test_esp32_probe_ordering.c
 *
 * Source-level regression checks for the supported ESP32 capability probes
 * and M1 Link transaction serialization.
 *
 * Neither m1_esp32_caps_init() (m1_csrc/m1_esp32_caps.c) nor m1link_hal_xfer()
 * (Esp_spi_at/.../esp_app_main.c) can be exercised on the host: both call
 * concrete HAL/FreeRTOS/hardware entry points with no injectable seam. As
 * with the existing esp32_main_deinit() heap-leak regression test, these
 * checks verify the source-code invariants of the fix directly instead:
 *
 *   1. Capability probing uses only the SiN360 binary PING and M1_RPC; it must
 *      not start or issue commands through the retired AT task.
 *   2. The shared SPI mutex serializes complete M1 Link request/reply sequences.
 */

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    return buf;
}

static void assert_contains(const char *content, const char *needle)
{
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(content, needle), needle);
}

static void assert_ordered(const char *content, const char *first,
                           const char *second)
{
    const char *p_first = strstr(content, first);
    const char *p_second = strstr(content, second);

    TEST_ASSERT_NOT_NULL_MESSAGE(p_first, first);
    TEST_ASSERT_NOT_NULL_MESSAGE(p_second, second);
    TEST_ASSERT_TRUE_MESSAGE(p_first < p_second, first);
}

void setUp(void)    {}
void tearDown(void) {}

/* ----- m1_esp32_caps.c: supported transports only ----------------------- */

void test_caps_probe_has_no_at_transport_path(void)
{
    char *content = read_file("m1_csrc/m1_esp32_caps.c");

    assert_contains(content, "m1_esp32_simple_cmd(CMD_PING");
    assert_contains(content, "spi_m1link_send_recv_bin(tx64");
    TEST_ASSERT_NULL(strstr(content, "spi_AT_send_recv"));
    TEST_ASSERT_NULL(strstr(content, "AT+CMD?"));
    TEST_ASSERT_NULL(strstr(content, "esp32_main_init"));

    free(content);
}

void test_caps_init_still_falls_back_to_unknown(void)
{
    char *content = read_file("m1_csrc/m1_esp32_caps.c");

    assert_contains(content, "\"Unknown (fallback)\"");
    assert_ordered(content, "spi_m1link_send_recv_bin(tx64",
                   "strncpy(s_fw_name, \"Unknown (fallback)\",");

    free(content);
}

/* ----- esp_app_main.c: mutex held for the full M1 Link sequence ------------ */

void test_spi_m1link_send_recv_bin_locks_spi_mutex_for_full_sequence(void)
{
    char *content = read_file(
        "Esp_spi_at/examples/at_spi_master/spi/stm32/main/esp_app_main.c");

    /* Anchor all checks to the body of spi_m1link_send_recv_bin so that
     * the HAL_SPI_Abort call inside m1link_hal_xfer (earlier in the file)
     * and any mention of m1_esp32_m1link_send_recv_timed in comments do not
     * pollute the ordered checks.  Anchor to the C function definition (which
     * has a return type) rather than the comment references to the name. */
    const char *fn_start = strstr(content, "uint8_t spi_m1link_send_recv_bin(");
    TEST_ASSERT_NOT_NULL_MESSAGE(fn_start, "spi_m1link_send_recv_bin definition not found");

    /* The full-sequence lock must wrap the abort AND the timed exchange so
     * another SPI user cannot interpose between request and reply polls. */
    const char *lock_call  = strstr(fn_start, "esp32_m1link_spi_lock())");
    const char *abort_call = strstr(fn_start, "HAL_SPI_Abort(&hspi_esp);");
    const char *timed_call = strstr(fn_start, "m1_esp32_m1link_send_recv_timed(");
    const char *unlock_call= timed_call != NULL
                                 ? strstr(timed_call, "esp32_m1link_spi_unlock();")
                                 : NULL;

    TEST_ASSERT_NOT_NULL(lock_call);
    TEST_ASSERT_NOT_NULL(abort_call);
    TEST_ASSERT_NOT_NULL(timed_call);
    TEST_ASSERT_NOT_NULL(unlock_call);

    TEST_ASSERT_TRUE_MESSAGE(lock_call  < abort_call,  "lock must precede abort");
    TEST_ASSERT_TRUE_MESSAGE(abort_call < timed_call,  "abort must precede timed exchange");
    TEST_ASSERT_TRUE_MESSAGE(timed_call < unlock_call, "timed exchange must precede unlock");

    /* m1link_hal_xfer must NOT take the per-transfer lock: doing so would
     * serialise individual SPI transactions but still permit another operation
     * between the request unlock and the next IDLE poll. Search
     * only within m1link_hal_xfer's text range (between its definition and
     * the start of spi_m1link_send_recv_bin) to avoid false positives. */
    const char *xfer_fn = strstr(content, "static int m1link_hal_xfer(");
    TEST_ASSERT_NOT_NULL(xfer_fn);
    TEST_ASSERT_TRUE_MESSAGE(fn_start > xfer_fn,
        "spi_m1link_send_recv_bin must come after m1link_hal_xfer");
    const char *xfer_lock = strstr(xfer_fn, "esp32_m1link_spi_lock())");
    /* If the call is found at all, it must be in spi_m1link_send_recv_bin
     * (at or after fn_start), not inside m1link_hal_xfer (before fn_start). */
    TEST_ASSERT_TRUE_MESSAGE(xfer_lock == NULL || xfer_lock >= fn_start,
        "m1link_hal_xfer must not take per-transfer SPI mutex");

    free(content);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_caps_probe_has_no_at_transport_path);
    RUN_TEST(test_caps_init_still_falls_back_to_unknown);
    RUN_TEST(test_spi_m1link_send_recv_bin_locks_spi_mutex_for_full_sequence);
    return UNITY_END();
}
