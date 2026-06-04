/* See COPYING.txt for license details. */

/**
 * @file   wifi_status_msg.c
 * @brief  Pure-logic WiFi status message model.
 *
 * See wifi_status_msg.h for API documentation.
 */

#include "wifi_status_msg.h"
#include <string.h>

/* Safe string copy: always NUL-terminates, handles NULL src. */
static void safe_copy(char *dst, size_t dst_len, const char *src)
{
    if (!dst || dst_len == 0) return;
    if (!src)
    {
        dst[0] = '\0';
        return;
    }
    size_t i;
    for (i = 0; i < dst_len - 1 && src[i] != '\0'; i++)
    {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

void wifi_status_msg_set(wifi_status_msg_t *msg,
                         const char *title,
                         const char *line1,
                         const char *line2,
                         uint32_t display_ms)
{
    if (!msg) return;

    safe_copy(msg->title, sizeof(msg->title), title);
    safe_copy(msg->line1, sizeof(msg->line1), line1);
    safe_copy(msg->line2, sizeof(msg->line2), line2);
    msg->display_ms = (display_ms == 0) ? WIFI_STATUS_MSG_DEFAULT_MS : display_ms;
    msg->active = true;
    /* start_ms intentionally NOT set here — caller must set from system tick */
}

void wifi_status_msg_clear(wifi_status_msg_t *msg)
{
    if (!msg) return;
    memset(msg, 0, sizeof(*msg));
}

bool wifi_status_msg_active(const wifi_status_msg_t *msg)
{
    if (!msg) return false;
    return msg->active;
}

bool wifi_status_msg_expired(const wifi_status_msg_t *msg, uint32_t now_ms)
{
    if (!msg || !msg->active) return true;
    /* Unsigned subtraction handles wraparound correctly. */
    uint32_t elapsed = now_ms - msg->start_ms;
    return elapsed >= msg->display_ms;
}
