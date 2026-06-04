/* See COPYING.txt for license details. */

/*
 * test_badusb_migration.c
 *
 * Source-level regression checks for Phase J: m1_badusb.c migration to
 * badusb_parser.h. Verifies that:
 *
 *   - m1_badusb.c includes badusb_parser.h
 *   - The old KEY_* defines block is gone
 *   - The ascii_hid_map_t typedef is gone
 *   - The static ascii_to_hid[] table is gone
 *   - The duplicate static helpers are gone (parse_key_name,
 *     parse_modifier, count_lines)
 *   - The new badusb_parser API calls are present (busb_classify_line,
 *     busb_count_lines, busb_ascii_to_hid)
 *   - badusb_parser.h declares the expected API surface
 */

#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M1_ROOT
#error "M1_ROOT must be defined by CMake"
#endif

void setUp(void) {}
void tearDown(void) {}

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

static void assert_absent(const char *content, const char *needle)
{
    TEST_ASSERT_NULL_MESSAGE(strstr(content, needle), needle);
}

/* ------------------------------------------------------------------ */
/* m1_badusb.c migration checks                                        */
/* ------------------------------------------------------------------ */

void test_m1_badusb_includes_badusb_parser_header(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "badusb_parser.h");
    free(c);
}

void test_m1_badusb_key_defines_removed(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    /* Old KEY_* scancode defines should be gone */
    assert_absent(c, "#define KEY_ENTER");
    assert_absent(c, "#define KEY_TAB");
    assert_absent(c, "#define KEY_ESCAPE");
    assert_absent(c, "#define KEY_NONE");
    assert_absent(c, "#define KEY_A ");
    free(c);
}

void test_m1_badusb_ascii_hid_map_typedef_removed(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    /* Old typedef closing brace should be gone; busb_ascii_hid_map_t is fine */
    assert_absent(c, "} ascii_hid_map_t;");
    free(c);
}

void test_m1_badusb_ascii_to_hid_table_removed(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    /* The static table definition should be gone */
    assert_absent(c, "ascii_to_hid[]");
    free(c);
}

void test_m1_badusb_duplicate_parse_key_name_removed(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_absent(c, "badusb_parse_key_name");
    free(c);
}

void test_m1_badusb_duplicate_parse_modifier_removed(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_absent(c, "badusb_parse_modifier");
    free(c);
}

void test_m1_badusb_duplicate_count_lines_removed(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_absent(c, "badusb_count_lines");
    free(c);
}

void test_m1_badusb_uses_busb_classify_line(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "busb_classify_line");
    free(c);
}

void test_m1_badusb_uses_busb_count_lines(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "busb_count_lines");
    free(c);
}

void test_m1_badusb_uses_busb_ascii_to_hid(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "busb_ascii_to_hid");
    free(c);
}

void test_m1_badusb_uses_busb_key_constants(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "BUSB_KEY_ENTER");
    assert_contains(c, "BUSB_KEY_TAB");
    free(c);
}

void test_m1_badusb_uses_busb_mod_constants(void)
{
    char *c = read_file("m1_csrc/m1_badusb.c");
    assert_contains(c, "BUSB_MOD_LSHIFT");
    free(c);
}

/* ------------------------------------------------------------------ */
/* badusb_parser.h API surface checks                                  */
/* ------------------------------------------------------------------ */

void test_badusb_parser_header_declares_classify_line(void)
{
    char *h = read_file("m1_csrc/badusb_parser.h");
    assert_contains(h, "busb_classify_line");
    free(h);
}

void test_badusb_parser_header_declares_parse_key_name(void)
{
    char *h = read_file("m1_csrc/badusb_parser.h");
    assert_contains(h, "busb_parse_key_name");
    free(h);
}

void test_badusb_parser_header_declares_parse_modifier(void)
{
    char *h = read_file("m1_csrc/badusb_parser.h");
    assert_contains(h, "busb_parse_modifier");
    free(h);
}

void test_badusb_parser_header_declares_count_lines(void)
{
    char *h = read_file("m1_csrc/badusb_parser.h");
    assert_contains(h, "busb_count_lines");
    free(h);
}

void test_badusb_parser_header_declares_ascii_to_hid(void)
{
    char *h = read_file("m1_csrc/badusb_parser.h");
    assert_contains(h, "busb_ascii_to_hid");
    free(h);
}

void test_badusb_parser_header_defines_busb_key_enter(void)
{
    char *h = read_file("m1_csrc/badusb_parser.h");
    assert_contains(h, "BUSB_KEY_ENTER");
    free(h);
}

void test_badusb_parser_header_defines_busb_mod_lctrl(void)
{
    char *h = read_file("m1_csrc/badusb_parser.h");
    assert_contains(h, "BUSB_MOD_LCTRL");
    free(h);
}

/* ------------------------------------------------------------------ */
/* badusb_parser.c implements all API functions                        */
/* ------------------------------------------------------------------ */

void test_badusb_parser_c_implements_classify_line(void)
{
    char *c = read_file("m1_csrc/badusb_parser.c");
    assert_contains(c, "busb_classify_line");
    free(c);
}

void test_badusb_parser_c_implements_count_lines(void)
{
    char *c = read_file("m1_csrc/badusb_parser.c");
    assert_contains(c, "busb_count_lines");
    free(c);
}

void test_badusb_parser_c_implements_ascii_to_hid(void)
{
    char *c = read_file("m1_csrc/badusb_parser.c");
    assert_contains(c, "busb_ascii_to_hid");
    free(c);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_m1_badusb_includes_badusb_parser_header);
    RUN_TEST(test_m1_badusb_key_defines_removed);
    RUN_TEST(test_m1_badusb_ascii_hid_map_typedef_removed);
    RUN_TEST(test_m1_badusb_ascii_to_hid_table_removed);
    RUN_TEST(test_m1_badusb_duplicate_parse_key_name_removed);
    RUN_TEST(test_m1_badusb_duplicate_parse_modifier_removed);
    RUN_TEST(test_m1_badusb_duplicate_count_lines_removed);
    RUN_TEST(test_m1_badusb_uses_busb_classify_line);
    RUN_TEST(test_m1_badusb_uses_busb_count_lines);
    RUN_TEST(test_m1_badusb_uses_busb_ascii_to_hid);
    RUN_TEST(test_m1_badusb_uses_busb_key_constants);
    RUN_TEST(test_m1_badusb_uses_busb_mod_constants);
    RUN_TEST(test_badusb_parser_header_declares_classify_line);
    RUN_TEST(test_badusb_parser_header_declares_parse_key_name);
    RUN_TEST(test_badusb_parser_header_declares_parse_modifier);
    RUN_TEST(test_badusb_parser_header_declares_count_lines);
    RUN_TEST(test_badusb_parser_header_declares_ascii_to_hid);
    RUN_TEST(test_badusb_parser_header_defines_busb_key_enter);
    RUN_TEST(test_badusb_parser_header_defines_busb_mod_lctrl);
    RUN_TEST(test_badusb_parser_c_implements_classify_line);
    RUN_TEST(test_badusb_parser_c_implements_count_lines);
    RUN_TEST(test_badusb_parser_c_implements_ascii_to_hid);
    return UNITY_END();
}
