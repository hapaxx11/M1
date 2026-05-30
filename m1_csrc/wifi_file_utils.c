/* See COPYING.txt for license details. */

/**
 * @file   wifi_file_utils.c
 * @brief  Pure-logic WiFi file type validators.
 *
 * See wifi_file_utils.h for API documentation.
 */

#include "wifi_file_utils.h"
#include <string.h>

uint8_t wifi_ascii_lower(uint8_t c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

bool wifi_ext_is_ap_cache(const char *name)
{
    if (!name) return false;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;

    char ext[5] = {0};
    for (uint8_t i = 0; i < 4 && dot[i]; i++)
    {
        ext[i] = (char)wifi_ascii_lower((uint8_t)dot[i]);
    }

    return (strcmp(ext, ".tsv") == 0 || strcmp(ext, ".txt") == 0);
}

bool wifi_ext_is_html(const char *name)
{
    if (!name) return false;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;

    char ext[6] = {0};
    for (uint8_t i = 0; i < 5 && dot[i]; i++)
    {
        ext[i] = (char)wifi_ascii_lower((uint8_t)dot[i]);
    }

    return (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0);
}

bool wifi_ext_is_ssid_list(const char *name)
{
    if (!name) return false;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;

    char ext[5] = {0};
    for (uint8_t i = 0; i < 4 && dot[i]; i++)
    {
        ext[i] = (char)wifi_ascii_lower((uint8_t)dot[i]);
    }

    return (strcmp(ext, ".txt") == 0 || strcmp(ext, ".lst") == 0);
}
