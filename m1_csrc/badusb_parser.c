/* See COPYING.txt for license details. */

/*
 * badusb_parser.c
 *
 * Pure-logic DuckyScript parser helpers — key name lookup, modifier parsing,
 * ASCII-to-HID mapping, and line-type classification.
 *
 * Extracted from m1_badusb.c for host-side testability.
 * Hardware-independent: no RTOS, no USB, no display.
 *
 * M1 Project
 */

#include "badusb_parser.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/*──────────── ASCII-to-HID mapping table (US layout, 0x20-0x7E) ────────────*/

static const busb_ascii_hid_map_t s_ascii_to_hid[] =
{
    /* 0x20 ' '  */ {BUSB_KEY_SPACE,      0},
    /* 0x21 '!'  */ {BUSB_KEY_1,          1},
    /* 0x22 '"'  */ {BUSB_KEY_APOSTROPHE, 1},
    /* 0x23 '#'  */ {BUSB_KEY_3,          1},
    /* 0x24 '$'  */ {BUSB_KEY_4,          1},
    /* 0x25 '%'  */ {BUSB_KEY_5,          1},
    /* 0x26 '&'  */ {BUSB_KEY_7,          1},
    /* 0x27 '\'' */ {BUSB_KEY_APOSTROPHE, 0},
    /* 0x28 '('  */ {BUSB_KEY_9,          1},
    /* 0x29 ')'  */ {BUSB_KEY_0,          1},
    /* 0x2A '*'  */ {BUSB_KEY_8,          1},
    /* 0x2B '+'  */ {BUSB_KEY_EQUAL,      1},
    /* 0x2C ','  */ {BUSB_KEY_COMMA,      0},
    /* 0x2D '-'  */ {BUSB_KEY_MINUS,      0},
    /* 0x2E '.'  */ {BUSB_KEY_DOT,        0},
    /* 0x2F '/'  */ {BUSB_KEY_SLASH,      0},
    /* 0x30 '0'  */ {BUSB_KEY_0,          0},
    /* 0x31 '1'  */ {BUSB_KEY_1,          0},
    /* 0x32 '2'  */ {BUSB_KEY_2,          0},
    /* 0x33 '3'  */ {BUSB_KEY_3,          0},
    /* 0x34 '4'  */ {BUSB_KEY_4,          0},
    /* 0x35 '5'  */ {BUSB_KEY_5,          0},
    /* 0x36 '6'  */ {BUSB_KEY_6,          0},
    /* 0x37 '7'  */ {BUSB_KEY_7,          0},
    /* 0x38 '8'  */ {BUSB_KEY_8,          0},
    /* 0x39 '9'  */ {BUSB_KEY_9,          0},
    /* 0x3A ':'  */ {BUSB_KEY_SEMICOLON,  1},
    /* 0x3B ';'  */ {BUSB_KEY_SEMICOLON,  0},
    /* 0x3C '<'  */ {BUSB_KEY_COMMA,      1},
    /* 0x3D '='  */ {BUSB_KEY_EQUAL,      0},
    /* 0x3E '>'  */ {BUSB_KEY_DOT,        1},
    /* 0x3F '?'  */ {BUSB_KEY_SLASH,      1},
    /* 0x40 '@'  */ {BUSB_KEY_2,          1},
    /* 0x41-0x5A: A-Z (shift) */
    {BUSB_KEY_A, 1}, {BUSB_KEY_B, 1}, {BUSB_KEY_C, 1}, {BUSB_KEY_D, 1},
    {BUSB_KEY_E, 1}, {BUSB_KEY_F, 1}, {BUSB_KEY_G, 1}, {BUSB_KEY_H, 1},
    {BUSB_KEY_I, 1}, {BUSB_KEY_J, 1}, {BUSB_KEY_K, 1}, {BUSB_KEY_L, 1},
    {BUSB_KEY_M, 1}, {BUSB_KEY_N, 1}, {BUSB_KEY_O, 1}, {BUSB_KEY_P, 1},
    {BUSB_KEY_Q, 1}, {BUSB_KEY_R, 1}, {BUSB_KEY_S, 1}, {BUSB_KEY_T, 1},
    {BUSB_KEY_U, 1}, {BUSB_KEY_V, 1}, {BUSB_KEY_W, 1}, {BUSB_KEY_X, 1},
    {BUSB_KEY_Y, 1}, {BUSB_KEY_Z, 1},
    /* 0x5B '['  */ {BUSB_KEY_LEFTBRACE,  0},
    /* 0x5C '\\' */ {BUSB_KEY_BACKSLASH,  0},
    /* 0x5D ']'  */ {BUSB_KEY_RIGHTBRACE, 0},
    /* 0x5E '^'  */ {BUSB_KEY_6,          1},
    /* 0x5F '_'  */ {BUSB_KEY_MINUS,      1},
    /* 0x60 '`'  */ {BUSB_KEY_GRAVE,      0},
    /* 0x61-0x7A: a-z (no shift) */
    {BUSB_KEY_A, 0}, {BUSB_KEY_B, 0}, {BUSB_KEY_C, 0}, {BUSB_KEY_D, 0},
    {BUSB_KEY_E, 0}, {BUSB_KEY_F, 0}, {BUSB_KEY_G, 0}, {BUSB_KEY_H, 0},
    {BUSB_KEY_I, 0}, {BUSB_KEY_J, 0}, {BUSB_KEY_K, 0}, {BUSB_KEY_L, 0},
    {BUSB_KEY_M, 0}, {BUSB_KEY_N, 0}, {BUSB_KEY_O, 0}, {BUSB_KEY_P, 0},
    {BUSB_KEY_Q, 0}, {BUSB_KEY_R, 0}, {BUSB_KEY_S, 0}, {BUSB_KEY_T, 0},
    {BUSB_KEY_U, 0}, {BUSB_KEY_V, 0}, {BUSB_KEY_W, 0}, {BUSB_KEY_X, 0},
    {BUSB_KEY_Y, 0}, {BUSB_KEY_Z, 0},
    /* 0x7B '{'  */ {BUSB_KEY_LEFTBRACE,  1},
    /* 0x7C '|'  */ {BUSB_KEY_BACKSLASH,  1},
    /* 0x7D '}'  */ {BUSB_KEY_RIGHTBRACE, 1},
    /* 0x7E '~'  */ {BUSB_KEY_GRAVE,      1},
};

/*──────────── busb_ascii_to_hid ────────────*/

const busb_ascii_hid_map_t *busb_ascii_to_hid(char c)
{
    if (c < 0x20 || c > 0x7E)
        return NULL;
    return &s_ascii_to_hid[(unsigned char)c - 0x20];
}

/*──────────── busb_parse_key_name ────────────*/

uint8_t busb_parse_key_name(const char *name)
{
    if (strcmp(name, "ENTER") == 0 || strcmp(name, "RETURN") == 0)  return BUSB_KEY_ENTER;
    if (strcmp(name, "TAB") == 0)           return BUSB_KEY_TAB;
    if (strcmp(name, "ESCAPE") == 0 || strcmp(name, "ESC") == 0)    return BUSB_KEY_ESCAPE;
    if (strcmp(name, "SPACE") == 0)         return BUSB_KEY_SPACE;
    if (strcmp(name, "BACKSPACE") == 0)     return BUSB_KEY_BACKSPACE;
    if (strcmp(name, "DELETE") == 0 || strcmp(name, "DEL") == 0)    return BUSB_KEY_DELETE;
    if (strcmp(name, "INSERT") == 0)        return BUSB_KEY_INSERT;
    if (strcmp(name, "HOME") == 0)          return BUSB_KEY_HOME;
    if (strcmp(name, "END") == 0)           return BUSB_KEY_END;
    if (strcmp(name, "PAGEUP") == 0)        return BUSB_KEY_PAGEUP;
    if (strcmp(name, "PAGEDOWN") == 0)      return BUSB_KEY_PAGEDOWN;
    if (strcmp(name, "UP") == 0 || strcmp(name, "UPARROW") == 0)    return BUSB_KEY_UP;
    if (strcmp(name, "DOWN") == 0 || strcmp(name, "DOWNARROW") == 0) return BUSB_KEY_DOWN;
    if (strcmp(name, "LEFT") == 0 || strcmp(name, "LEFTARROW") == 0) return BUSB_KEY_LEFT;
    if (strcmp(name, "RIGHT") == 0 || strcmp(name, "RIGHTARROW") == 0) return BUSB_KEY_RIGHT;
    if (strcmp(name, "CAPSLOCK") == 0)      return BUSB_KEY_CAPSLOCK;
    if (strcmp(name, "NUMLOCK") == 0)       return BUSB_KEY_NUMLOCK;
    if (strcmp(name, "SCROLLLOCK") == 0)    return BUSB_KEY_SCROLLLOCK;
    if (strcmp(name, "PRINTSCREEN") == 0)   return BUSB_KEY_PRINTSCREEN;
    if (strcmp(name, "PAUSE") == 0 || strcmp(name, "BREAK") == 0)   return BUSB_KEY_PAUSE;
    if (strcmp(name, "MENU") == 0 || strcmp(name, "APP") == 0)      return BUSB_KEY_MENU;
    if (strcmp(name, "F1") == 0)   return BUSB_KEY_F1;
    if (strcmp(name, "F2") == 0)   return BUSB_KEY_F2;
    if (strcmp(name, "F3") == 0)   return BUSB_KEY_F3;
    if (strcmp(name, "F4") == 0)   return BUSB_KEY_F4;
    if (strcmp(name, "F5") == 0)   return BUSB_KEY_F5;
    if (strcmp(name, "F6") == 0)   return BUSB_KEY_F6;
    if (strcmp(name, "F7") == 0)   return BUSB_KEY_F7;
    if (strcmp(name, "F8") == 0)   return BUSB_KEY_F8;
    if (strcmp(name, "F9") == 0)   return BUSB_KEY_F9;
    if (strcmp(name, "F10") == 0)  return BUSB_KEY_F10;
    if (strcmp(name, "F11") == 0)  return BUSB_KEY_F11;
    if (strcmp(name, "F12") == 0)  return BUSB_KEY_F12;

    /* Single printable character */
    if (strlen(name) == 1 && name[0] >= 0x20 && name[0] <= 0x7E)
        return s_ascii_to_hid[(unsigned char)name[0] - 0x20].keycode;

    return BUSB_KEY_NONE;
}

/*──────────── busb_parse_modifier ────────────*/

uint8_t busb_parse_modifier(const char *name, const char **remainder)
{
    *remainder = NULL;

    static const struct {
        const char *keyword;
        uint8_t     mod;
    } modifiers[] = {
        {"CTRL",    BUSB_MOD_LCTRL},
        {"CONTROL", BUSB_MOD_LCTRL},
        {"ALT",     BUSB_MOD_LALT},
        {"SHIFT",   BUSB_MOD_LSHIFT},
        {"GUI",     BUSB_MOD_LGUI},
        {"WINDOWS", BUSB_MOD_LGUI},
        {"COMMAND", BUSB_MOD_LGUI},
    };

    for (int i = 0; i < (int)(sizeof(modifiers) / sizeof(modifiers[0])); i++)
    {
        size_t klen = strlen(modifiers[i].keyword);
        if (strncmp(name, modifiers[i].keyword, klen) == 0)
        {
            char sep = name[klen];
            if (sep == ' ' || sep == '-' || sep == '+' || sep == '\0')
            {
                if (sep != '\0')
                    *remainder = name + klen + 1;
                else
                    *remainder = NULL;
                return modifiers[i].mod;
            }
        }
    }

    return 0;
}

/*──────────── busb_classify_line ────────────*/

/* Helper: trim trailing whitespace/CR/LF into a stack buffer. */
static void trim_tail(const char *src, char *dst, size_t dstsz)
{
    size_t len = strlen(src);
    if (len >= dstsz) len = dstsz - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
    while (len > 0 && (dst[len - 1] == '\r' || dst[len - 1] == '\n' ||
                       dst[len - 1] == ' '  || dst[len - 1] == '\t'))
        dst[--len] = '\0';
}

bool busb_classify_line(const char *line, busb_parsed_line_t *out)
{
    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;

    /* Empty line */
    if (*line == '\0' || *line == '\n' || *line == '\r')
    {
        out->type = BUSB_LINE_EMPTY;
        return true;
    }

    /* REM — comment */
    if (strncmp(line, "REM ", 4) == 0 || strncmp(line, "REM\r", 4) == 0 ||
        strncmp(line, "REM\n", 4) == 0 || strcmp(line, "REM") == 0)
    {
        out->type = BUSB_LINE_COMMENT;
        return true;
    }

    /* DELAY <ms> */
    if (strncmp(line, "DELAY ", 6) == 0)
    {
        out->type = BUSB_LINE_DELAY;
        out->u.delay_ms = (uint32_t)atoi(line + 6);
        return true;
    }

    /* DEFAULT_DELAY / DEFAULTDELAY */
    if (strncmp(line, "DEFAULT_DELAY ", 14) == 0 ||
        strncmp(line, "DEFAULTDELAY ", 13) == 0)
    {
        out->type = BUSB_LINE_DEFAULT_DELAY;
        const char *p = line + (line[7] == '_' ? 14 : 13);
        out->u.delay_ms = (uint32_t)atoi(p);
        return true;
    }

    /* STRING <text> */
    if (strncmp(line, "STRING ", 7) == 0)
    {
        out->type = BUSB_LINE_STRING;
        out->u.string_text = line + 7;
        return true;
    }

    /* REPEAT <n> */
    if (strncmp(line, "REPEAT ", 7) == 0)
    {
        out->type = BUSB_LINE_REPEAT;
        out->u.repeat_count = atoi(line + 7);
        return true;
    }

    /* Try as modifier combo */
    {
        uint8_t mod_accum = 0;
        const char *cur = line;
        const char *rest = NULL;

        while (cur && *cur)
        {
            uint8_t m = busb_parse_modifier(cur, &rest);
            if (m == 0)
                break;
            mod_accum |= m;
            cur = rest;
        }

        if (mod_accum != 0)
        {
            out->type = BUSB_LINE_MODIFIER_KEY;
            out->u.key.modifiers = mod_accum;

            if (cur != NULL && *cur != '\0')
            {
                char keybuf[32];
                trim_tail(cur, keybuf, sizeof(keybuf));
                uint8_t kc = busb_parse_key_name(keybuf);
                if (kc != BUSB_KEY_NONE)
                {
                    /* Add shift if needed for printable char */
                    if (strlen(keybuf) == 1 && keybuf[0] >= 0x20 && keybuf[0] <= 0x7E)
                    {
                        if (s_ascii_to_hid[(unsigned char)keybuf[0] - 0x20].shift)
                            out->u.key.modifiers |= BUSB_MOD_LSHIFT;
                    }
                }
                out->u.key.keycode = kc;
            }
            else
            {
                out->u.key.keycode = BUSB_KEY_NONE;
            }
            return true;
        }
    }

    /* Try as standalone key name */
    {
        char keybuf[32];
        trim_tail(line, keybuf, sizeof(keybuf));
        uint8_t kc = busb_parse_key_name(keybuf);
        if (kc != BUSB_KEY_NONE)
        {
            out->type = BUSB_LINE_STANDALONE_KEY;
            out->u.key.modifiers = 0;
            out->u.key.keycode = kc;
            return true;
        }
    }

    out->type = BUSB_LINE_UNKNOWN;
    return false;
}

/*──────────── busb_count_lines ────────────*/

uint16_t busb_count_lines(const char *buf, uint32_t len)
{
    uint16_t count = 0;
    for (uint32_t i = 0; i < len; i++)
    {
        if (buf[i] == '\n')
            count++;
    }
    if (len > 0 && buf[len - 1] != '\n')
        count++;
    return count;
}
