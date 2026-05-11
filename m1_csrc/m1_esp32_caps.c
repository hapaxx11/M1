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
#include <stddef.h>
#include <string.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "FreeRTOS.h"
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

/* Buffer for the AT+CMD? response.  Stock ESP-AT lists ~150-200 commands at
 * roughly 30-50 bytes per line, so ~8KB is generous enough to capture the
 * full list before spi_AT_send_recv() stops at "OK\r\n".  Allocated from the
 * FreeRTOS heap to avoid stacking ~8KB inside a caller's task stack and to
 * sidestep the newlib heap (which has been observed too small for incidental
 * allocations post-SiN360 integration). */
#define AT_CMD_RESP_BUF_SZ     8192u

/* AT+CMD? probe needs a few seconds for the ESP32 to assemble and stream the
 * full command list.  5 s is the standard "long" timeout used elsewhere in
 * the AT bridge (e.g. spi_AT_send_recv 5 s in m1_http_client / m1_802154). */
#define AT_CMD_PROBE_TIMEOUT_S 5

/*************************** V A R I A B L E S ********************************/

static uint64_t s_bitmap          = 0u;
static bool     s_queried         = false;
static char     s_fw_name[32]     = "Unknown";

/* =========================================================================
 * AT command → capability bit mapping table
 *
 * Consulted by m1_esp32_caps_parse_at_cmd_list() against the response of a
 * stock ESP-AT `AT+CMD?` probe.  Each entry maps the full AT command name
 * (including the "AT+" prefix) to the M1_ESP32_CAP_* bit that should be set
 * when the connected ESP32 firmware advertises that command.
 *
 * Adding support for a new AT-side feature is a single-line edit to this
 * table — no curated fallback profile macros are required.
 *
 * Currently mapped commands:
 *   - "AT+CWJAP"      — stock ESP-AT, WiFi station join → CAP_WIFI_JOIN
 *   - "AT+BLEHIDINIT" — stock ESP-AT (BLE HID example) → CAP_BLE_HID
 *   - "AT+ZIGSNIFF"   — bedge117/dag/neddy299 custom    → CAP_802154
 *   - "AT+DEAUTH"     — dag/neddy299 at_custom_deauth   → CAP_DEAUTH
 *   - "AT+STASCAN"    — neddy299 at_custom_stascan      → CAP_STA_SCAN
 * =========================================================================*/
static const m1_esp32_at_cmd_cap_entry_t s_at_cmd_cap_map[] = {
    { "AT+CWJAP",      M1_ESP32_CAP_WIFI_JOIN },
    { "AT+BLEHIDINIT", M1_ESP32_CAP_BLE_HID   },
    { "AT+ZIGSNIFF",   M1_ESP32_CAP_802154    },
    { "AT+DEAUTH",     M1_ESP32_CAP_DEAUTH    },
    { "AT+STASCAN",    M1_ESP32_CAP_STA_SCAN  },
};

#define S_AT_CMD_CAP_MAP_N \
    (sizeof(s_at_cmd_cap_map) / sizeof(s_at_cmd_cap_map[0]))

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
     * fallback, potentially mis-attributing capabilities. */
    if (!m1_esp32_get_init_status())
        return;

    /* Probe 1: binary CMD_GET_STATUS (0x02).  SiN360 binary-SPI firmware
     * always responds here.  AT firmware that implements the binary extension
     * (e.g. hapaxx11/esp32-at-monstatek-m1) also responds here.  Unextended
     * AT firmware (bedge117, dag) will return RESP_ERR or time out. */
    ret = m1_esp32_simple_cmd(CMD_GET_STATUS, &resp, CAPS_QUERY_TIMEOUT_MS);

    if (ret == 0 && resp.status == RESP_OK &&
        m1_esp32_caps_parse_payload(resp.payload, resp.payload_len,
                                    &bitmap, fw_name))
    {
        s_bitmap = bitmap;
        strncpy(s_fw_name, fw_name, sizeof(s_fw_name) - 1);
        s_fw_name[sizeof(s_fw_name) - 1] = '\0';
        s_queried = true;
        return;
    }

    /* Probe 2: stock ESP-AT `AT+CMD?`.  This command is part of the basic
     * ESP-AT command set and is supported by all tracked AT firmware
     * variants (bedge117, dag, neddy299) without requiring any custom
     * extension on the ESP32 side.  The response lists every AT command the
     * firmware understands; we OR in capability bits for each command our
     * mapping table recognises.
     *
     * The response can be several KB, so the buffer is allocated from the
     * FreeRTOS heap rather than the caller's stack.  Allocation failure
     * is treated identically to probe failure — fall through to the
     * fail-closed default below. */
    if (get_esp32_main_init_status())
    {
        char *at_resp = (char *)pvPortMalloc(AT_CMD_RESP_BUF_SZ);
        if (at_resp)
        {
            at_resp[0] = '\0';
            (void)spi_AT_send_recv("AT+CMD?\r\n", at_resp,
                                   (int)AT_CMD_RESP_BUF_SZ,
                                   AT_CMD_PROBE_TIMEOUT_S);

            if (m1_esp32_caps_at_cmd_response_valid(at_resp))
            {
                /* Probe succeeded — OR in every tracked AT command the
                 * firmware advertised.  A response that contains "+CMD:"
                 * lines but none of our tracked names is still a successful
                 * probe; the bitmap simply reflects that the firmware has
                 * no features we currently recognise. */
                s_bitmap = m1_esp32_caps_parse_at_cmd_list(
                    at_resp, s_at_cmd_cap_map, S_AT_CMD_CAP_MAP_N);
                strncpy(s_fw_name, "AT (probed)", sizeof(s_fw_name) - 1);
                s_fw_name[sizeof(s_fw_name) - 1] = '\0';
                s_queried = true;
                vPortFree(at_resp);
                return;
            }

            vPortFree(at_resp);
        }
        /* If pvPortMalloc returned NULL (FreeRTOS heap exhausted), we
         * intentionally fall through to the fail-closed default below.
         * Recording a partial bitmap based on an unprobed firmware would
         * be worse than reporting "Unknown (fallback)".  Because s_queried
         * is not set, the next call to m1_esp32_caps_init() will retry
         * automatically. */
    }

    /* Both probes failed — fail closed.  Feature gates that check specific
     * M1_ESP32_CAP_* bits will all return false and the "Feature not
     * supported" UI will appear.  This is intentional: granting capabilities
     * we cannot verify risks crashing on firmware that does not implement
     * the underlying command. */
    s_bitmap = 0u;
    strncpy(s_fw_name, "Unknown (fallback)", sizeof(s_fw_name) - 1);
    s_fw_name[sizeof(s_fw_name) - 1] = '\0';
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
        /* Don't probe when the ESP32 HAL has not been initialised.  Probing
         * an uninitialised transport would time out and potentially cache a
         * fail-closed result that prevents legitimate capabilities later. */
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
