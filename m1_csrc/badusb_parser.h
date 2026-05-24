/* See COPYING.txt for license details. */

/*
 * badusb_parser.h
 *
 * Pure-logic DuckyScript parser helpers — key name lookup, modifier parsing,
 * ASCII-to-HID mapping, and line-type classification.
 *
 * Extracted from m1_badusb.c for host-side testability.
 * Hardware-independent: no RTOS, no USB, no display.
 *
 * M1 Project
 */

#ifndef BADUSB_PARSER_H_
#define BADUSB_PARSER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*─────────────── HID scancodes ───────────────*/

#define BUSB_KEY_NONE         0x00
#define BUSB_KEY_A            0x04
#define BUSB_KEY_B            0x05
#define BUSB_KEY_C            0x06
#define BUSB_KEY_D            0x07
#define BUSB_KEY_E            0x08
#define BUSB_KEY_F            0x09
#define BUSB_KEY_G            0x0A
#define BUSB_KEY_H            0x0B
#define BUSB_KEY_I            0x0C
#define BUSB_KEY_J            0x0D
#define BUSB_KEY_K            0x0E
#define BUSB_KEY_L            0x0F
#define BUSB_KEY_M            0x10
#define BUSB_KEY_N            0x11
#define BUSB_KEY_O            0x12
#define BUSB_KEY_P            0x13
#define BUSB_KEY_Q            0x14
#define BUSB_KEY_R            0x15
#define BUSB_KEY_S            0x16
#define BUSB_KEY_T            0x17
#define BUSB_KEY_U            0x18
#define BUSB_KEY_V            0x19
#define BUSB_KEY_W            0x1A
#define BUSB_KEY_X            0x1B
#define BUSB_KEY_Y            0x1C
#define BUSB_KEY_Z            0x1D
#define BUSB_KEY_1            0x1E
#define BUSB_KEY_2            0x1F
#define BUSB_KEY_3            0x20
#define BUSB_KEY_4            0x21
#define BUSB_KEY_5            0x22
#define BUSB_KEY_6            0x23
#define BUSB_KEY_7            0x24
#define BUSB_KEY_8            0x25
#define BUSB_KEY_9            0x26
#define BUSB_KEY_0            0x27
#define BUSB_KEY_ENTER        0x28
#define BUSB_KEY_ESCAPE       0x29
#define BUSB_KEY_BACKSPACE    0x2A
#define BUSB_KEY_TAB          0x2B
#define BUSB_KEY_SPACE        0x2C
#define BUSB_KEY_MINUS        0x2D
#define BUSB_KEY_EQUAL        0x2E
#define BUSB_KEY_LEFTBRACE    0x2F
#define BUSB_KEY_RIGHTBRACE   0x30
#define BUSB_KEY_BACKSLASH    0x31
#define BUSB_KEY_SEMICOLON    0x33
#define BUSB_KEY_APOSTROPHE   0x34
#define BUSB_KEY_GRAVE        0x35
#define BUSB_KEY_COMMA        0x36
#define BUSB_KEY_DOT          0x37
#define BUSB_KEY_SLASH        0x38
#define BUSB_KEY_CAPSLOCK     0x39
#define BUSB_KEY_F1           0x3A
#define BUSB_KEY_F2           0x3B
#define BUSB_KEY_F3           0x3C
#define BUSB_KEY_F4           0x3D
#define BUSB_KEY_F5           0x3E
#define BUSB_KEY_F6           0x3F
#define BUSB_KEY_F7           0x40
#define BUSB_KEY_F8           0x41
#define BUSB_KEY_F9           0x42
#define BUSB_KEY_F10          0x43
#define BUSB_KEY_F11          0x44
#define BUSB_KEY_F12          0x45
#define BUSB_KEY_PRINTSCREEN  0x46
#define BUSB_KEY_SCROLLLOCK   0x47
#define BUSB_KEY_PAUSE        0x48
#define BUSB_KEY_INSERT       0x49
#define BUSB_KEY_HOME         0x4A
#define BUSB_KEY_PAGEUP       0x4B
#define BUSB_KEY_DELETE       0x4C
#define BUSB_KEY_END          0x4D
#define BUSB_KEY_PAGEDOWN     0x4E
#define BUSB_KEY_RIGHT        0x4F
#define BUSB_KEY_LEFT         0x50
#define BUSB_KEY_DOWN         0x51
#define BUSB_KEY_UP           0x52
#define BUSB_KEY_NUMLOCK      0x53
#define BUSB_KEY_MENU         0x65

/*─────────────── HID modifiers ───────────────*/

#define BUSB_MOD_LCTRL   0x01
#define BUSB_MOD_LSHIFT  0x02
#define BUSB_MOD_LALT    0x04
#define BUSB_MOD_LGUI    0x08

/*─────────────── ASCII-to-HID map entry ───────────────*/

typedef struct
{
    uint8_t keycode;
    uint8_t shift;   /* 1 = shift required */
} busb_ascii_hid_map_t;

/*─────────────── DuckyScript line types ───────────────*/

typedef enum
{
    BUSB_LINE_EMPTY,           /* blank / whitespace only */
    BUSB_LINE_COMMENT,         /* REM ... */
    BUSB_LINE_DELAY,           /* DELAY <ms> */
    BUSB_LINE_DEFAULT_DELAY,   /* DEFAULT_DELAY / DEFAULTDELAY */
    BUSB_LINE_STRING,          /* STRING <text> */
    BUSB_LINE_REPEAT,          /* REPEAT <n> */
    BUSB_LINE_MODIFIER_KEY,    /* CTRL x, GUI r, ALT F4, etc. */
    BUSB_LINE_STANDALONE_KEY,  /* ENTER, ESC, F5, etc. */
    BUSB_LINE_UNKNOWN,         /* unrecognized */
} busb_line_type_t;

/*─────────────── Parsed line result ───────────────*/

typedef struct
{
    busb_line_type_t type;
    union {
        uint32_t delay_ms;        /* DELAY / DEFAULT_DELAY */
        const char *string_text;  /* STRING (points into input) */
        int repeat_count;         /* REPEAT */
        struct {
            uint8_t modifiers;    /* accumulated modifier bits */
            uint8_t keycode;      /* final key scancode */
        } key;
    } u;
} busb_parsed_line_t;

/*─────────────── API ───────────────*/

/**
 * Parse a named key string to HID scancode.
 * Recognizes ENTER, TAB, ESCAPE, F1-F12, arrow keys, etc.
 * Single printable characters are also accepted.
 * @return HID keycode or BUSB_KEY_NONE if not recognized.
 */
uint8_t busb_parse_key_name(const char *name);

/**
 * Parse a modifier keyword at the start of the string.
 * Recognizes CTRL, ALT, SHIFT, GUI, WINDOWS, COMMAND, CONTROL.
 * @param name     input string
 * @param remainder  output: pointer past the modifier+separator, or NULL
 * @return modifier bitmask, or 0 if not a modifier keyword
 */
uint8_t busb_parse_modifier(const char *name, const char **remainder);

/**
 * Classify and parse a single DuckyScript line (pure logic, no side effects).
 * @param line   null-terminated line of script text
 * @param out    parsed result
 * @return true if the line was recognized, false if unknown
 */
bool busb_classify_line(const char *line, busb_parsed_line_t *out);

/**
 * Map an ASCII character (0x20–0x7E) to its HID scancode and shift flag.
 * @return pointer to the map entry, or NULL if out of range
 */
const busb_ascii_hid_map_t *busb_ascii_to_hid(char c);

/**
 * Count lines in a text buffer.
 */
uint16_t busb_count_lines(const char *buf, uint32_t len);

#endif /* BADUSB_PARSER_H_ */
