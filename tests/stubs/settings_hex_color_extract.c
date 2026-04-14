/*
 * settings_hex_color_extract.c — Test-only extraction of settings_parse_hex_color()
 *
 * This duplicates the pure-logic hex parser from m1_settings.c so it can be
 * compiled on the host without any firmware dependencies.  Keep in sync with
 * the production implementation in m1_settings.c.
 */

#include <stdint.h>
#include <string.h>

uint8_t settings_parse_hex_color(const char *str, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (str[0] == '#') str++;
    if (strlen(str) < 6) return 0;

    uint32_t val = 0;
    for (uint8_t i = 0; i < 6; i++)
    {
        char c = str[i];
        uint8_t nib;
        if (c >= '0' && c <= '9')      nib = c - '0';
        else if (c >= 'A' && c <= 'F')  nib = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')  nib = c - 'a' + 10;
        else return 0;
        val = (val << 4) | nib;
    }
    *r = (val >> 16) & 0xFF;
    *g = (val >> 8) & 0xFF;
    *b = val & 0xFF;
    return 1;
}
