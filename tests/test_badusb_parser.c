/* See COPYING.txt for license details. */

/*
 * test_badusb_parser.c
 *
 * Host-side unit tests for the DuckyScript parser (badusb_parser.c/h).
 * Tests key-name lookup, modifier parsing, ASCII-to-HID mapping,
 * line classification, and line counting for core DuckyScript commands.
 * Advanced commands (ALTCHAR, HOLD/RELEASE, MOUSE, MEDIA,
 * WAIT_FOR_BUTTON_PRESS, DEFINE) are not yet implemented.
 */

#include "unity.h"
#include "badusb_parser.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/*═══════════════ Key name lookup ═══════════════*/

void test_key_name_enter(void)
{
    TEST_ASSERT_EQUAL(BUSB_KEY_ENTER, busb_parse_key_name("ENTER"));
    TEST_ASSERT_EQUAL(BUSB_KEY_ENTER, busb_parse_key_name("RETURN"));
}

void test_key_name_tab(void)
{
    TEST_ASSERT_EQUAL(BUSB_KEY_TAB, busb_parse_key_name("TAB"));
}

void test_key_name_escape_aliases(void)
{
    TEST_ASSERT_EQUAL(BUSB_KEY_ESCAPE, busb_parse_key_name("ESCAPE"));
    TEST_ASSERT_EQUAL(BUSB_KEY_ESCAPE, busb_parse_key_name("ESC"));
}

void test_key_name_arrow_keys(void)
{
    TEST_ASSERT_EQUAL(BUSB_KEY_UP,    busb_parse_key_name("UP"));
    TEST_ASSERT_EQUAL(BUSB_KEY_UP,    busb_parse_key_name("UPARROW"));
    TEST_ASSERT_EQUAL(BUSB_KEY_DOWN,  busb_parse_key_name("DOWN"));
    TEST_ASSERT_EQUAL(BUSB_KEY_DOWN,  busb_parse_key_name("DOWNARROW"));
    TEST_ASSERT_EQUAL(BUSB_KEY_LEFT,  busb_parse_key_name("LEFT"));
    TEST_ASSERT_EQUAL(BUSB_KEY_LEFT,  busb_parse_key_name("LEFTARROW"));
    TEST_ASSERT_EQUAL(BUSB_KEY_RIGHT, busb_parse_key_name("RIGHT"));
    TEST_ASSERT_EQUAL(BUSB_KEY_RIGHT, busb_parse_key_name("RIGHTARROW"));
}

void test_key_name_function_keys(void)
{
    TEST_ASSERT_EQUAL(BUSB_KEY_F1,  busb_parse_key_name("F1"));
    TEST_ASSERT_EQUAL(BUSB_KEY_F4,  busb_parse_key_name("F4"));
    TEST_ASSERT_EQUAL(BUSB_KEY_F12, busb_parse_key_name("F12"));
}

void test_key_name_delete_aliases(void)
{
    TEST_ASSERT_EQUAL(BUSB_KEY_DELETE, busb_parse_key_name("DELETE"));
    TEST_ASSERT_EQUAL(BUSB_KEY_DELETE, busb_parse_key_name("DEL"));
}

void test_key_name_pause_aliases(void)
{
    TEST_ASSERT_EQUAL(BUSB_KEY_PAUSE, busb_parse_key_name("PAUSE"));
    TEST_ASSERT_EQUAL(BUSB_KEY_PAUSE, busb_parse_key_name("BREAK"));
}

void test_key_name_menu_aliases(void)
{
    TEST_ASSERT_EQUAL(BUSB_KEY_MENU, busb_parse_key_name("MENU"));
    TEST_ASSERT_EQUAL(BUSB_KEY_MENU, busb_parse_key_name("APP"));
}

void test_key_name_single_char(void)
{
    TEST_ASSERT_EQUAL(BUSB_KEY_A, busb_parse_key_name("a"));
    TEST_ASSERT_EQUAL(BUSB_KEY_A, busb_parse_key_name("A"));
    TEST_ASSERT_EQUAL(BUSB_KEY_1, busb_parse_key_name("1"));
    TEST_ASSERT_EQUAL(BUSB_KEY_SPACE, busb_parse_key_name(" "));
}

void test_key_name_unknown(void)
{
    TEST_ASSERT_EQUAL(BUSB_KEY_NONE, busb_parse_key_name("FOOBAR"));
    TEST_ASSERT_EQUAL(BUSB_KEY_NONE, busb_parse_key_name(""));
}

/*═══════════════ Modifier parsing ═══════════════*/

void test_modifier_ctrl(void)
{
    const char *rest = NULL;
    TEST_ASSERT_EQUAL(BUSB_MOD_LCTRL, busb_parse_modifier("CTRL x", &rest));
    TEST_ASSERT_NOT_NULL(rest);
    TEST_ASSERT_EQUAL_STRING("x", rest);
}

void test_modifier_control_alias(void)
{
    const char *rest = NULL;
    TEST_ASSERT_EQUAL(BUSB_MOD_LCTRL, busb_parse_modifier("CONTROL c", &rest));
    TEST_ASSERT_NOT_NULL(rest);
    TEST_ASSERT_EQUAL_STRING("c", rest);
}

void test_modifier_alt(void)
{
    const char *rest = NULL;
    TEST_ASSERT_EQUAL(BUSB_MOD_LALT, busb_parse_modifier("ALT F4", &rest));
    TEST_ASSERT_EQUAL_STRING("F4", rest);
}

void test_modifier_gui_aliases(void)
{
    const char *rest = NULL;
    TEST_ASSERT_EQUAL(BUSB_MOD_LGUI, busb_parse_modifier("GUI r", &rest));
    TEST_ASSERT_EQUAL_STRING("r", rest);

    TEST_ASSERT_EQUAL(BUSB_MOD_LGUI, busb_parse_modifier("WINDOWS r", &rest));
    TEST_ASSERT_EQUAL_STRING("r", rest);

    TEST_ASSERT_EQUAL(BUSB_MOD_LGUI, busb_parse_modifier("COMMAND r", &rest));
    TEST_ASSERT_EQUAL_STRING("r", rest);
}

void test_modifier_dash_separator(void)
{
    const char *rest = NULL;
    TEST_ASSERT_EQUAL(BUSB_MOD_LCTRL, busb_parse_modifier("CTRL-x", &rest));
    TEST_ASSERT_EQUAL_STRING("x", rest);
}

void test_modifier_plus_separator(void)
{
    const char *rest = NULL;
    TEST_ASSERT_EQUAL(BUSB_MOD_LCTRL, busb_parse_modifier("CTRL+x", &rest));
    TEST_ASSERT_EQUAL_STRING("x", rest);
}

void test_modifier_alone(void)
{
    const char *rest = NULL;
    TEST_ASSERT_EQUAL(BUSB_MOD_LGUI, busb_parse_modifier("GUI", &rest));
    TEST_ASSERT_NULL(rest);
}

void test_modifier_not_a_modifier(void)
{
    const char *rest = NULL;
    TEST_ASSERT_EQUAL(0, busb_parse_modifier("ENTER", &rest));
    TEST_ASSERT_NULL(rest);
}

/*═══════════════ ASCII-to-HID mapping ═══════════════*/

void test_ascii_lowercase_a(void)
{
    const busb_ascii_hid_map_t *m = busb_ascii_to_hid('a');
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL(BUSB_KEY_A, m->keycode);
    TEST_ASSERT_EQUAL(0, m->shift);
}

void test_ascii_uppercase_a(void)
{
    const busb_ascii_hid_map_t *m = busb_ascii_to_hid('A');
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL(BUSB_KEY_A, m->keycode);
    TEST_ASSERT_EQUAL(1, m->shift);
}

void test_ascii_digit(void)
{
    const busb_ascii_hid_map_t *m = busb_ascii_to_hid('5');
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL(BUSB_KEY_5, m->keycode);
    TEST_ASSERT_EQUAL(0, m->shift);
}

void test_ascii_exclamation(void)
{
    const busb_ascii_hid_map_t *m = busb_ascii_to_hid('!');
    TEST_ASSERT_NOT_NULL(m);
    TEST_ASSERT_EQUAL(BUSB_KEY_1, m->keycode);
    TEST_ASSERT_EQUAL(1, m->shift);
}

void test_ascii_out_of_range(void)
{
    TEST_ASSERT_NULL(busb_ascii_to_hid('\t'));
    TEST_ASSERT_NULL(busb_ascii_to_hid(0x7F));
    TEST_ASSERT_NULL(busb_ascii_to_hid('\0'));
}

/*═══════════════ Line classification ═══════════════*/

void test_classify_empty(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_EMPTY, out.type);

    TEST_ASSERT_TRUE(busb_classify_line("   ", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_EMPTY, out.type);

    TEST_ASSERT_TRUE(busb_classify_line("\n", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_EMPTY, out.type);
}

void test_classify_comment(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("REM this is a comment", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_COMMENT, out.type);

    TEST_ASSERT_TRUE(busb_classify_line("REM", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_COMMENT, out.type);
}

void test_classify_delay(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("DELAY 500", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_DELAY, out.type);
    TEST_ASSERT_EQUAL(500, out.u.delay_ms);
}

void test_classify_default_delay_underscore(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("DEFAULT_DELAY 100", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_DEFAULT_DELAY, out.type);
    TEST_ASSERT_EQUAL(100, out.u.delay_ms);
}

void test_classify_default_delay_nounderscore(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("DEFAULTDELAY 200", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_DEFAULT_DELAY, out.type);
    TEST_ASSERT_EQUAL(200, out.u.delay_ms);
}

void test_classify_string(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("STRING Hello World", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_STRING, out.type);
    TEST_ASSERT_EQUAL_STRING("Hello World", out.u.string_text);
}

void test_classify_repeat(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("REPEAT 5", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_REPEAT, out.type);
    TEST_ASSERT_EQUAL(5, out.u.repeat_count);
}

void test_classify_modifier_with_key(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("CTRL c", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_MODIFIER_KEY, out.type);
    TEST_ASSERT_EQUAL(BUSB_MOD_LCTRL, out.u.key.modifiers);
    TEST_ASSERT_EQUAL(BUSB_KEY_C, out.u.key.keycode);
}

void test_classify_multi_modifier(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("CTRL ALT DELETE", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_MODIFIER_KEY, out.type);
    TEST_ASSERT_EQUAL(BUSB_MOD_LCTRL | BUSB_MOD_LALT, out.u.key.modifiers);
    TEST_ASSERT_EQUAL(BUSB_KEY_DELETE, out.u.key.keycode);
}

void test_classify_gui_r(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("GUI r", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_MODIFIER_KEY, out.type);
    TEST_ASSERT_EQUAL(BUSB_MOD_LGUI, out.u.key.modifiers);
    TEST_ASSERT_EQUAL(BUSB_KEY_R, out.u.key.keycode);
}

void test_classify_standalone_key(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("ENTER", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_STANDALONE_KEY, out.type);
    TEST_ASSERT_EQUAL(BUSB_KEY_ENTER, out.u.key.keycode);
}

void test_classify_standalone_f5(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("F5", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_STANDALONE_KEY, out.type);
    TEST_ASSERT_EQUAL(BUSB_KEY_F5, out.u.key.keycode);
}

void test_classify_unknown(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_FALSE(busb_classify_line("GOBBLEDYGOOK", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_UNKNOWN, out.type);
}

void test_classify_leading_whitespace(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("   DELAY 100", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_DELAY, out.type);
    TEST_ASSERT_EQUAL(100, out.u.delay_ms);
}

void test_classify_trailing_crlf(void)
{
    busb_parsed_line_t out;
    TEST_ASSERT_TRUE(busb_classify_line("ENTER\r\n", &out));
    TEST_ASSERT_EQUAL(BUSB_LINE_STANDALONE_KEY, out.type);
    TEST_ASSERT_EQUAL(BUSB_KEY_ENTER, out.u.key.keycode);
}

/*═══════════════ Line counting ═══════════════*/

void test_count_lines_normal(void)
{
    const char *buf = "REM test\nDELAY 100\nSTRING hello\n";
    TEST_ASSERT_EQUAL(3, busb_count_lines(buf, (uint32_t)strlen(buf)));
}

void test_count_lines_no_trailing_newline(void)
{
    const char *buf = "LINE1\nLINE2";
    TEST_ASSERT_EQUAL(2, busb_count_lines(buf, (uint32_t)strlen(buf)));
}

void test_count_lines_empty(void)
{
    TEST_ASSERT_EQUAL(0, busb_count_lines("", 0));
}

void test_count_lines_single(void)
{
    TEST_ASSERT_EQUAL(1, busb_count_lines("hello", 5));
}

/*═══════════════ Main ═══════════════*/

int main(void)
{
    UNITY_BEGIN();

    /* Key name lookup */
    RUN_TEST(test_key_name_enter);
    RUN_TEST(test_key_name_tab);
    RUN_TEST(test_key_name_escape_aliases);
    RUN_TEST(test_key_name_arrow_keys);
    RUN_TEST(test_key_name_function_keys);
    RUN_TEST(test_key_name_delete_aliases);
    RUN_TEST(test_key_name_pause_aliases);
    RUN_TEST(test_key_name_menu_aliases);
    RUN_TEST(test_key_name_single_char);
    RUN_TEST(test_key_name_unknown);

    /* Modifier parsing */
    RUN_TEST(test_modifier_ctrl);
    RUN_TEST(test_modifier_control_alias);
    RUN_TEST(test_modifier_alt);
    RUN_TEST(test_modifier_gui_aliases);
    RUN_TEST(test_modifier_dash_separator);
    RUN_TEST(test_modifier_plus_separator);
    RUN_TEST(test_modifier_alone);
    RUN_TEST(test_modifier_not_a_modifier);

    /* ASCII-to-HID */
    RUN_TEST(test_ascii_lowercase_a);
    RUN_TEST(test_ascii_uppercase_a);
    RUN_TEST(test_ascii_digit);
    RUN_TEST(test_ascii_exclamation);
    RUN_TEST(test_ascii_out_of_range);

    /* Line classification */
    RUN_TEST(test_classify_empty);
    RUN_TEST(test_classify_comment);
    RUN_TEST(test_classify_delay);
    RUN_TEST(test_classify_default_delay_underscore);
    RUN_TEST(test_classify_default_delay_nounderscore);
    RUN_TEST(test_classify_string);
    RUN_TEST(test_classify_repeat);
    RUN_TEST(test_classify_modifier_with_key);
    RUN_TEST(test_classify_multi_modifier);
    RUN_TEST(test_classify_gui_r);
    RUN_TEST(test_classify_standalone_key);
    RUN_TEST(test_classify_standalone_f5);
    RUN_TEST(test_classify_unknown);
    RUN_TEST(test_classify_leading_whitespace);
    RUN_TEST(test_classify_trailing_crlf);

    /* Line counting */
    RUN_TEST(test_count_lines_normal);
    RUN_TEST(test_count_lines_no_trailing_newline);
    RUN_TEST(test_count_lines_empty);
    RUN_TEST(test_count_lines_single);

    return UNITY_END();
}
