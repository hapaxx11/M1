/* See COPYING.txt for license details. */

/*
 * test_esp32_probe_ordering.c
 *
 * Source-level regression checks for the supported ESP32 capability probes
 * and M1 Link transaction serialization.
 *
 * Neither m1_esp32_caps_init() (m1_csrc/m1_esp32_caps.c) nor m1link_hal_xfer()
 * (m1_csrc/m1_esp32_m1link.c) can be exercised on the host: both call concrete
 * HAL/FreeRTOS/hardware entry points with no injectable seam. These checks
 * verify the source-code invariants directly instead:
 *
 *   1. Capability probing uses only the SiN360 binary PING and M1_RPC.
 *   2. The RPC transaction mutex serializes capability probes and feature RPCs.
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

/* ----- Shared transaction mutex protects M1 Link callers ------------------- */

void test_m1link_callers_use_shared_rpc_transaction_mutex(void)
{
    char *rpc = read_file("m1_csrc/m1_esp32_rpc.c");
    char *caps = read_file("m1_csrc/m1_esp32_caps.c");
    char *transport = read_file("m1_csrc/m1_esp32_m1link.c");

    const char *rpc_fn = strstr(rpc, "m1_esp32_rpc_status_t m1_esp32_rpc_call(");
    const char *caps_fn = strstr(caps, "void m1_esp32_caps_init(void)");
    TEST_ASSERT_NOT_NULL(rpc_fn);
    TEST_ASSERT_NOT_NULL(caps_fn);
    assert_ordered(rpc_fn, "if (!m1_esp32_rpc_transaction_lock())",
                   "m1_esp32_rpc_call_unlocked(");
    assert_ordered(caps_fn, "if (!m1_esp32_rpc_transaction_lock())",
                   "m1_esp32_caps_init_locked();");
    assert_contains(transport, "m1_esp32_m1link_send_recv_timed(");
    assert_contains(transport, "!m1_esp32_get_init_status()");

    free(transport);
    free(caps);
    free(rpc);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_caps_probe_has_no_at_transport_path);
    RUN_TEST(test_caps_init_still_falls_back_to_unknown);
    RUN_TEST(test_m1link_callers_use_shared_rpc_transaction_mutex);
    return UNITY_END();
}
