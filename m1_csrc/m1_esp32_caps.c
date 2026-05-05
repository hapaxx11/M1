/* See COPYING.txt for license details. */

/*
 * m1_esp32_caps.c
 *
 * Runtime capability descriptor for the ESP32-C6 coprocessor.
 *
 * See m1_esp32_caps.h for full documentation.
 *
 * M1 Project
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_esp32_caps.h"
#include "m1_esp32_cmd.h"
#include "m1_compile_cfg.h"
#include "m1_display.h"
#include "m1_lcd.h"

/* Forward declaration — implemented in m1_esp32_hal.c */
extern uint8_t m1_esp32_get_init_status(void);

/*************************** D E F I N E S ************************************/

#define CAPS_QUERY_TIMEOUT_MS  500u   /* Brief: ESP32 is already running */

/*************************** V A R I A B L E S ********************************/

static uint64_t s_bitmap          = 0u;
static bool     s_queried         = false;
static char     s_fw_name[32]     = "Unknown";

/*************** I N T E R N A L   H E L P E R S *****************************/

/**
 * Derive a capability bitmap from compile-time feature flags.
 *
 * This is the fallback used when the ESP32 firmware does not implement
 * CMD_GET_STATUS.  It exactly mirrors the feature set that was compiled in,
 * preserving the behaviour that existed before the runtime cap layer was
 * added — no feature silently disappears.
 *
 * SiN360 binary-SPI features are always assumed available (they form the
 * base of the current build).  AT-layer features are included only when
 * the corresponding compile flag was set.
 */
static uint64_t caps_build_fallback_bitmap(void)
{
    uint64_t caps = 0u;

    /* SiN360 binary-SPI capabilities — present in all current builds */
    caps |= M1_ESP32_CAP_WIFI_SCAN;
    caps |= M1_ESP32_CAP_WIFI_STA_SCAN;
    caps |= M1_ESP32_CAP_WIFI_SNIFF;
    caps |= M1_ESP32_CAP_WIFI_ATTACK;
    caps |= M1_ESP32_CAP_WIFI_NETSCAN;
    caps |= M1_ESP32_CAP_WIFI_EVIL_PORTAL;
    caps |= M1_ESP32_CAP_BLE_SCAN;
    caps |= M1_ESP32_CAP_BLE_ADV;
    caps |= M1_ESP32_CAP_BLE_SPAM;
    caps |= M1_ESP32_CAP_BLE_SNIFF;

    /* AT-layer capabilities — only when compile flags say they are built in */
#ifdef M1_APP_WIFI_CONNECT_ENABLE
    caps |= M1_ESP32_CAP_WIFI_CONNECT;
#endif
#ifdef M1_APP_BADBT_ENABLE
    caps |= M1_ESP32_CAP_BLE_HID;
#endif
#ifdef M1_APP_BT_MANAGE_ENABLE
    caps |= M1_ESP32_CAP_BT_MANAGE;
#endif

    return caps;
}

/*************** P U B L I C   A P I ******************************************/

void m1_esp32_caps_init(void)
{
    m1_resp_t resp;
    int       ret;
    uint64_t  bitmap        = 0u;
    char      fw_name[32]   = {0};

    if (s_queried)
        return;  /* Already cached from a previous call */

    /* Require the SPI HAL transport to be active before sending CMD_GET_STATUS.
     * If the ESP32 has not been initialised yet (or was deinitialized), return
     * without caching so the next call retries once the transport is ready.
     * Probing an uninitialised transport would time out and cache the
     * compile-flag fallback, potentially granting capabilities that the
     * connected firmware does not actually support. */
    if (!m1_esp32_get_init_status())
        return;

    /* Attempt to query the ESP32 for its capability descriptor */
    ret = m1_esp32_simple_cmd(CMD_GET_STATUS, &resp, CAPS_QUERY_TIMEOUT_MS);

    if (ret == 0 && resp.status == RESP_OK &&
        m1_esp32_caps_parse_payload(resp.payload, resp.payload_len,
                                    &bitmap, fw_name))
    {
        /* Successful handshake — use the firmware-reported capabilities */
        s_bitmap = bitmap;
        strncpy(s_fw_name, fw_name, sizeof(s_fw_name) - 1);
        s_fw_name[sizeof(s_fw_name) - 1] = '\0';
    }
    else
    {
        /* No CMD_GET_STATUS support or timeout — use compile-flag fallback.
         * This covers older SiN360 firmware that predates CMD_GET_STATUS. */
        s_bitmap = caps_build_fallback_bitmap();
        strncpy(s_fw_name, "Unknown (fallback)", sizeof(s_fw_name) - 1);
        s_fw_name[sizeof(s_fw_name) - 1] = '\0';
    }

    s_queried = true;
}

void m1_esp32_caps_reset(void)
{
    s_bitmap     = 0u;
    s_queried    = false;
    s_fw_name[0] = '\0';
}

bool m1_esp32_has_cap(uint64_t cap)
{
    if (!s_queried)
    {
        /* Don't probe (and don't cache the fallback) when the ESP32 HAL
         * has not been initialised.  Probing an uninitialised transport
         * would time out, cache the compile-flag fallback, and potentially
         * grant capabilities the connected firmware does not support. */
        if (!m1_esp32_get_init_status())
            return false;
        m1_esp32_caps_init();
    }
    return (s_bitmap & cap) != 0u;
}

const char *m1_esp32_caps_fw_name(void)
{
    if (!s_queried)
    {
        /* Return "offline" immediately — without probing or caching —
         * when the ESP32 HAL transport is not active.  This keeps the
         * RPC device-info response accurate for disconnected devices. */
        if (!m1_esp32_get_init_status())
            return "offline";
        m1_esp32_caps_init();
    }
    return s_fw_name[0] != '\0' ? s_fw_name : "Unknown";
}

bool m1_esp32_require_cap(uint64_t cap, const char *feature_name)
{
    char fw_line[22];  /* fits on 128px display with main-menu font */

    if (m1_esp32_has_cap(cap))
        return true;

    /* Draw the "not supported" screen and hold for 2 seconds so the user
     * can read it before the delegate pops the scene. */
    snprintf(fw_line, sizeof(fw_line), "%.21s", m1_esp32_caps_fw_name());

    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);

    m1_u8g2_firstpage();
    do
    {
        /* Title */
        u8g2_DrawStr(&m1_u8g2, 0, 10, feature_name);
        u8g2_DrawHLine(&m1_u8g2, 0, 11, 128);

        /* Body */
        u8g2_DrawStr(&m1_u8g2, 0, 25, "Not supported by");
        u8g2_DrawStr(&m1_u8g2, 0, 37, fw_line);
        u8g2_DrawStr(&m1_u8g2, 0, 52, "Flash compatible");
        u8g2_DrawStr(&m1_u8g2, 0, 62, "ESP32 firmware");
    }
    while (u8g2_NextPage(&m1_u8g2));

    HAL_Delay(2000);

    return false;
}
