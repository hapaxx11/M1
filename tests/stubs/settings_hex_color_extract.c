/*
 * settings_hex_color_extract.c — Host-test implementation of settings_parse_hex_color()
 *
 * This file intentionally implements the documented "#RRGGBB"/"RRGGBB" format
 * directly for host tests, rather than mirroring the production parser line by
 * line. Keeping the test helper specification-based avoids silent divergence
 * caused by maintaining two hand-copied implementations.
 */

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "m1_settings.h"

uint8_t settings_parse_hex_color(const char *str, uint8_t *r, uint8_t *g, uint8_t *b)
{
    unsigned long val;

    if (str == NULL || r == NULL || g == NULL || b == NULL) return 0;
    if (str[0] == '#') str++;
    if (strlen(str) < 6) return 0;

    for (uint8_t i = 0; i < 6; i++)
    {
        if (!isxdigit((unsigned char)str[i])) return 0;
    }

    char buf[7];
    memcpy(buf, str, 6);
    buf[6] = '\0';

    val = strtoul(buf, NULL, 16);
    if (val > 0xFFFFFFUL) return 0;

    *r = (uint8_t)((val >> 16) & 0xFF);
    *g = (uint8_t)((val >> 8) & 0xFF);
    *b = (uint8_t)(val & 0xFF);
    return 1;
}