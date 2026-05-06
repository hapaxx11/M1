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

/* Forward declarations — implemented in m1_esp32_hal.c and esp_app_main.c */
extern uint8_t m1_esp32_get_init_status(void);
extern bool    get_esp32_main_init_status(void);
extern uint8_t spi_AT_send_recv(const char *at_cmd, char *out_buf,
                                int out_buf_size, int timeout_sec);

/*************************** D E F I N E S ************************************/

#define CAPS_QUERY_TIMEOUT_MS  500u   /* Brief: ESP32 is already running */

/*************************** V A R I A B L E S ********************************/

static uint64_t s_bitmap          = 0u;
static bool     s_queried         = false;
static char     s_fw_name[32]     = "Unknown";

/*************** I N T E R N A L   H E L P E R S *****************************/

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
        /* Binary CMD_GET_STATUS failed or timed out.
         *
         * SiN360 binary-SPI firmware has supported CMD_GET_STATUS since its
         * initial Hapax integration and always self-reports above.  A failure
         * here therefore indicates an AT firmware variant (bedge117, neddy299,
         * dag) that may or may not implement the feat/cmd_get_status extension.
         *
         * Attempt a secondary probe via the AT text command "AT+GETSTATUSHEX"
         * (AT-Custom-Status).  AT firmware variants that implement this
         * extension return a hex-encoded m1_esp32_status_payload_t so the
         * same capability bitmap and firmware name can be extracted without
         * a binary SPI opcode response. */
        if (get_esp32_main_init_status())
        {
            char at_resp[100] = {0};
            uint8_t decoded[sizeof(m1_esp32_status_payload_t)] = {0};

            if (spi_AT_send_recv("AT+GETSTATUSHEX\r\n", at_resp,
                                 (int)sizeof(at_resp), 2) == 0 &&
                m1_esp32_caps_parse_at_hex(at_resp, decoded,
                                           (uint8_t)sizeof(decoded)) &&
                m1_esp32_caps_parse_payload(decoded,
                                            (uint8_t)sizeof(m1_esp32_status_payload_t),
                                            &bitmap, fw_name))
            {
                s_bitmap = bitmap;
                strncpy(s_fw_name, fw_name, sizeof(s_fw_name) - 1);
                s_fw_name[sizeof(s_fw_name) - 1] = '\0';
                s_queried = true;
                return;
            }
        }

        /* Neither binary CMD_GET_STATUS nor AT+GETSTATUSHEX succeeded.
         * Apply the AT baseline fallback so Bad-BT and 802.15.4 remain
         * accessible on AT firmware variants without the feat/cmd_get_status
         * extension. */
        s_bitmap = M1_ESP32_CAP_PROFILE_AT_BEDGE117;
        strncpy(s_fw_name, "Unknown AT (fallback)", sizeof(s_fw_name) - 1);
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
