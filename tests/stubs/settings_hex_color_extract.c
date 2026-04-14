/*
 * settings_hex_color_extract.c — Host-test implementation of settings_parse_hex_color()
 *
 * This file intentionally implements the documented "#RRGGBB"/"RRGGBB" format
 * directly for host tests, rather than mirroring the production parser line by
 * line. Keeping the test helper specification-based avoids silent divergence
 * caused by maintaining two hand-copied implementations.
 */

#include <stdint.h>
#include <string.h>
#include "m1_settings.h"

uint8_t settings_parse_hex_color(const char *str, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (str == NULL || r == NULL || g == NULL || b == NULL) return 0;
    if (str[0] == '#') str++;
    if (strlen(str) < 6) return 0;

    uint32_t val = 0;
    for (uint8_t i = 0; i < 6; i++)
    {
        char c = str[i];
        uint8_t nib;
        if (c >= '0' && c <= '9')      nib = c - '0';
        else if (c >= 'A' && c <= 'F') nib = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') nib = c - 'a' + 10;
        else return 0;
        val = (val << 4) | nib;
    }

    *r = (uint8_t)((val >> 16) & 0xFF);
    *g = (uint8_t)((val >> 8) & 0xFF);
    *b = (uint8_t)(val & 0xFF);
    return 1;
}
