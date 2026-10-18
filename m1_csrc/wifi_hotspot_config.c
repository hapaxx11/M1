/* See COPYING.txt for license details. */

#include "wifi_hotspot_config.h"

#include <stdio.h>
#include <string.h>

wifi_hotspot_path_t wifi_hotspot_select_path(esp32_transport_t transport)
{
    switch (transport) {
    case ESP32_TRANSPORT_RPC:
        return WIFI_HOTSPOT_PATH_RPC;
    default:
        return WIFI_HOTSPOT_PATH_UNSUPPORTED;
    }
}

static bool has_line_break_or_control(const char *value)
{
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p < 0x20u || *p == 0x7fu)
            return true;
    }
    return false;
}

bool wifi_hotspot_config_validate(const char *ssid, const char *password)
{
    if (!ssid || !password)
        return false;

    size_t ssid_len = strlen(ssid);
    size_t password_len = strlen(password);
    if (ssid_len == 0u || ssid_len > WIFI_HOTSPOT_SSID_MAX_LEN ||
        password_len > WIFI_HOTSPOT_PASSWORD_MAX_LEN ||
        (password_len != 0u && password_len < 8u))
        return false;

    return !has_line_break_or_control(ssid) &&
           !has_line_break_or_control(password);
}

bool wifi_hotspot_config_parse(const char *text, size_t text_len,
                               char *ssid, size_t ssid_capacity,
                               char *password, size_t password_capacity)
{
    if (!text || !ssid || !password || ssid_capacity == 0u ||
        password_capacity == 0u)
        return false;

    ssid[0] = '\0';
    password[0] = '\0';
    bool have_ssid = false;
    bool have_password = false;
    size_t pos = 0u;

    while (pos < text_len) {
        size_t start = pos;
        while (pos < text_len && text[pos] != '\n') {
            if (text[pos] == '\0')
                goto invalid;
            pos++;
        }
        size_t line_len = pos - start;
        if (pos < text_len)
            pos++;
        if (line_len > 0u && text[start + line_len - 1u] == '\r')
            line_len--;
        if (line_len == 0u)
            continue;

        const char *value;
        size_t value_len;
        char *destination;
        size_t capacity;
        bool *seen;
        if (line_len >= 5u && memcmp(text + start, "ssid=", 5u) == 0) {
            destination = ssid;
            capacity = ssid_capacity;
            seen = &have_ssid;
            value = text + start + 5u;
            value_len = line_len - 5u;
        } else if (line_len >= 9u &&
                   text[start] == 'p' && text[start + 1u] == 'a' &&
                   text[start + 2u] == 's' && text[start + 3u] == 's' &&
                   text[start + 4u] == 'w' && text[start + 5u] == 'o' &&
                   text[start + 6u] == 'r' && text[start + 7u] == 'd' &&
                   text[start + 8u] == '=') {
            destination = password;
            capacity = password_capacity;
            seen = &have_password;
            value = text + start + 9u;
            value_len = line_len - 9u;
        } else {
            goto invalid;
        }

        if (*seen || value_len >= capacity)
            goto invalid;
        for (size_t i = 0u; i < value_len; i++) {
            unsigned char ch = (unsigned char)value[i];
            if (ch < 0x20u || ch == 0x7fu)
                goto invalid;
        }
        memcpy(destination, value, value_len);
        destination[value_len] = '\0';
        *seen = true;
    }

    if (!have_ssid || !have_password ||
        !wifi_hotspot_config_validate(ssid, password))
        goto invalid;
    return true;

invalid:
    ssid[0] = '\0';
    password[0] = '\0';
    return false;
}

size_t wifi_hotspot_config_format(char *out, size_t capacity,
                                  const char *ssid, const char *password)
{
    if (!out || capacity == 0u ||
        !wifi_hotspot_config_validate(ssid, password)) {
        if (out && capacity > 0u)
            out[0] = '\0';
        return 0u;
    }

    int written = snprintf(out, capacity, "ssid=%s\npassword=%s\n",
                           ssid, password);
    if (written < 0 || (size_t)written >= capacity) {
        out[0] = '\0';
        return 0u;
    }
    return (size_t)written;
}
