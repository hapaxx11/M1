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

/* Case-insensitive exact comparison of the full file extension (including the
 * leading '.') against a lowercase candidate.  Requires the whole extension to
 * match so suffixes like ".tsvx" or ".html5" are rejected. */
static bool wifi_ext_equals_icase(const char *ext, const char *want)
{
    uint8_t i = 0;
    for (; ext[i] && want[i]; i++)
    {
        if (wifi_ascii_lower((uint8_t)ext[i]) != (uint8_t)want[i]) return false;
    }
    return ext[i] == '\0' && want[i] == '\0';
}

bool wifi_ext_is_ap_cache(const char *name)
{
    if (!name) return false;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;

    return (wifi_ext_equals_icase(dot, ".tsv") || wifi_ext_equals_icase(dot, ".txt"));
}

bool wifi_ext_is_html(const char *name)
{
    if (!name) return false;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;

    return (wifi_ext_equals_icase(dot, ".html") || wifi_ext_equals_icase(dot, ".htm"));
}

bool wifi_ext_is_ssid_list(const char *name)
{
    if (!name) return false;
    const char *dot = strrchr(name, '.');
    if (!dot) return false;

    return (wifi_ext_equals_icase(dot, ".txt") || wifi_ext_equals_icase(dot, ".lst"));
}
