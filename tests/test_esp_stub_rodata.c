/* See COPYING.txt for license details. */
/*
 * Regression guard for the ESP32 flasher-stub RAM reduction (issue #747).
 *
 * The ~91 KB `esp_stub[]` table in Esp32_serial_flasher/src/esp_stubs.c must
 * live in flash (.rodata), NOT in RAM (.data).  It only lands in flash when
 * BOTH of the following hold:
 *
 *   1. `esp_loader_bin_segment_t.data` is declared `const uint8_t *`
 *      (Esp32_serial_flasher/include/esp_loader.h), and
 *   2. every stub payload compound literal is const-qualified —
 *      `.data = (const uint8_t[]){ ... }` (esp_stubs.c).
 *
 * If a future regeneration of the auto-generated esp_stubs.c drops `const`
 * from the compound literals, or the header field loses `const`, the stub
 * payloads silently move back into .data and RAM usage jumps ~91 KB (from
 * ~85.8% back to ~99.98%, overflowing the STM32H573 RAM budget).
 *
 * This host-side test parses the two source files as text and fails if either
 * const qualifier is missing.  It intentionally does NOT compile the stub
 * table (it is ~140 KB of target-specific data with heavy vendored deps); a
 * text-level guard is sufficient because the const qualifiers are exactly what
 * controls the .rodata-vs-.data placement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "unity.h"

#ifndef ESP_STUBS_SRC_PATH
#error "ESP_STUBS_SRC_PATH must be defined by the build"
#endif
#ifndef ESP_LOADER_HDR_PATH
#error "ESP_LOADER_HDR_PATH must be defined by the build"
#endif

void setUp(void) {}
void tearDown(void) {}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL_MESSAGE(f, path);
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    TEST_ASSERT_GREATER_THAN_INT(0, (int)len);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)len + 1);
    TEST_ASSERT_NOT_NULL(buf);
    size_t got = fread(buf, 1, (size_t)len, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

/* The stub payloads must be const compound literals so they land in flash. */
static void test_stub_payloads_are_const(void)
{
    char *src = read_file(ESP_STUBS_SRC_PATH);
    bool has_const = strstr(src, ".data = (const uint8_t[]){") != NULL;
    bool has_nonconst = strstr(src, ".data = (uint8_t[]){") != NULL;
    free(src);

    /* Every payload assignment must be `.data = (const uint8_t[])`. */
    TEST_ASSERT_TRUE_MESSAGE(
        has_const, "expected const-qualified stub payload compound literals");

    /* A non-const `.data = (uint8_t[])` would place ~91 KB in RAM. */
    TEST_ASSERT_FALSE_MESSAGE(
        has_nonconst,
        "esp_stubs.c has a non-const stub payload — this pushes ~91 KB into "
        "RAM (.data) and overflows the STM32H573 RAM budget");
}

/* The segment `data` field must be const for the compound literals to bind
 * without an implicit const-discard that would defeat the .rodata placement. */
static void test_segment_data_field_is_const(void)
{
    char *hdr = read_file(ESP_LOADER_HDR_PATH);
    bool field_is_const = strstr(hdr, "const uint8_t *data;") != NULL;
    free(hdr);

    TEST_ASSERT_TRUE_MESSAGE(
        field_is_const,
        "esp_loader_bin_segment_t.data must be `const uint8_t *` so the stub "
        "table stays in flash (.rodata)");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_stub_payloads_are_const);
    RUN_TEST(test_segment_data_field_is_const);
    return UNITY_END();
}
