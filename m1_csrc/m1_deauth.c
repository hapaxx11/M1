/* See COPYING.txt for license details. */

/*
 * m1_deauth.c
 *
 * WiFi deauthentication tool — blocking delegate for the WiFi scene manager.
 *
 * Ported from neddy299/M1 (initial deauth commit + advanced mode tweaks),
 * adapted for Hapax conventions:
 *   - m1_esp32_deinit() called on ALL exit paths
 *   - No "Back" button bar labels (hardware BACK is self-explanatory)
 *   - Uses existing Hapax display / buzzer / ESP32 APIs
 *
 * M1 Project
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_app_main.h"
#include "m1_log_debug.h"
#include "m1_compile_cfg.h"
#include "m1_display.h"
#include "m1_esp32_hal.h"
#include "m1_deauth.h"
#include "m1_lcd.h"
#include "m1_buzzer.h"
#include "m1_games.h"
#include "m1_lib.h"
#include "m1_tasks.h"

#define TAG "DEAUTH"

/************************** D E F I N E S *************************************/

#define AT_RESP_BUF_SIZE         512
#define AT_LARGE_RESP_BUF_SIZE   8192
#define MULTI_AT_CMD_INTERVAL_MS 50

#define MAX_LIST_ITEMS   64
#define MAX_SIZE_ADDRESS 18
#define MAX_SIZE_NAME    32
#define MAX_SIZE_TITLE   64

#define LIST_HEADER_HEIGHT  12
#define LIST_ITEM_HEIGHT    9
#define LIST_START_Y        (LIST_HEADER_HEIGHT + 2)
#define LIST_VISIBLE_ITEMS  5

#define INPUT_POLL_DELAY    200

#define TITLE_DEFAULT       "Deauther"
#define TITLE_ERROR         "Deauther Error"
#define TITLE_SYSTEM_CHECK  "Deauth System Check"

/************************** S T R U C T U R E S *******************************/

typedef enum {
    STATE_NONE,
    STATE_PREFLIGHT,
    STATE_MENU,
    STATE_AP_SELECT,
    STATE_AP_CONF,
    STATE_STA_SELECT,
    STATE_STA_CONF,
    STATE_ATTACK,
    STATE_COMPLETE
} state_t;

typedef struct {
    char    address[MAX_SIZE_ADDRESS];
    char    name[MAX_SIZE_NAME];
    uint8_t channel;
} list_item_t;

/************************** S T A T I C S *************************************/

#define MAIN_MENU_OPTS 3
static char s_main_menu_options[MAIN_MENU_OPTS][32] = {
    "Discover Target",
    "Manual Target",
    "Reset ESP32"
};

static char       s_selected_sta[MAX_SIZE_ADDRESS];
static list_item_t s_selected_ap;

static list_item_t s_list_items[MAX_LIST_ITEMS];
static uint16_t    s_list_count = 0;

static state_t s_state          = STATE_NONE;
static state_t s_state_previous = STATE_NONE;

static bool    s_stascan_active = false;
static bool    s_deauth_active  = false;
static bool    s_redraw         = false;

static uint8_t s_deauth_enable_mode = 1;

/* Deauth status as returned by AT+DEAUTH? */
static uint8_t s_at_deauth_channel        = 0;
static uint8_t s_at_deauth_enable_mode    = 0;
static uint8_t s_at_deauth_enable_num_modes = 0;

#define PREFLIGHT_AT_CHECKS 2
static char PREFLIGHT_AT_COMMANDS[PREFLIGHT_AT_CHECKS][MAX_SIZE_NAME] = {
    "AT+DEAUTH?",
    "AT+STASCAN?"
};
static uint8_t s_failed_preflight_tests = 0;

/* Advanced-mode unlock: UP UP DOWN DOWN */
#define KEYSEQ_MAX 4
static game_button_t s_keyseq_map[KEYSEQ_MAX] = {
    GAME_BTN_UP, GAME_BTN_UP, GAME_BTN_DOWN, GAME_BTN_DOWN
};
static uint8_t s_keyseq_level    = 0;
static bool    s_keyseq_unlocked = false;

/* Large AT response buffer — file-scope static to avoid 8 KB stack allocation
 * inside wifi_deauth().  Safe because only one blocking delegate runs at a time. */
static char    s_resp_buf[AT_LARGE_RESP_BUF_SIZE];

/********************* H E L P E R S ****************************************/

static void check_keyseq(game_button_t btn)
{
    if (s_keyseq_unlocked)
        return;

    if (btn == s_keyseq_map[s_keyseq_level]) {
        if (s_keyseq_level == KEYSEQ_MAX - 1) {
            s_keyseq_unlocked = true;
            s_redraw = true;
            m1_buzzer_notification();
        } else {
            s_keyseq_level++;
        }
    } else {
        s_keyseq_level = 0;
    }
}

static void add_ap(const char *name, const char *address, uint8_t channel)
{
    if (s_list_count >= MAX_LIST_ITEMS) {
        M1_LOG_W(TAG, "too many APs discovered (%d), discarding\r\n", s_list_count);
        return;
    }

    if (strlen(name) == 0) {
        snprintf(s_list_items[s_list_count].name, MAX_SIZE_NAME, "hidden %s", address);
    } else {
        strncpy(s_list_items[s_list_count].name, name, MAX_SIZE_NAME - 1);
        s_list_items[s_list_count].name[MAX_SIZE_NAME - 1] = '\0';
    }

    strncpy(s_list_items[s_list_count].address, address, MAX_SIZE_ADDRESS - 1);
    s_list_items[s_list_count].address[MAX_SIZE_ADDRESS - 1] = '\0';
    s_list_items[s_list_count].channel = channel;

    s_list_count++;
    s_redraw = true;
}

static void add_station(const char *address)
{
    if (s_list_count >= MAX_LIST_ITEMS) {
        M1_LOG_W(TAG, "too many stations discovered (%d), discarding\r\n", s_list_count);
        return;
    }

    strncpy(s_list_items[s_list_count].address, address, MAX_SIZE_ADDRESS - 1);
    s_list_items[s_list_count].address[MAX_SIZE_ADDRESS - 1] = '\0';
    s_list_items[s_list_count].name[0]    = '\0';
    s_list_items[s_list_count].channel    = 0;

    s_list_count++;
    s_redraw = true;
}

/*
 * Parse a +CWLAP line and add to AP list.
 *   Requires preceding AT command: "AT+CWLAPOPT=,26"
 * Format: +CWLAP:("<ssid>","<bssid>",<channel>)
 */
static void parse_apscan(const char *line)
{
    char name[MAX_SIZE_NAME]       = "";
    char address[MAX_SIZE_ADDRESS] = "";
    int  channel = 0;

    const char *p = strstr(line, "+CWLAP:");
    if (!p) return;
    p = strchr(p + 7, '"'); /* find opening " of ssid after +CWLAP: */
    if (!p) return;
    p++; /* skip the opening " */

    /* Parse ssid */
    const char *delim = strchr(p, '"');
    if (!delim) return;
    int len = (int)(delim - p);
    if (len >= (int)sizeof(name))
        len = (int)sizeof(name) - 1;
    memcpy(name, p, len);
    name[len] = '\0';
    p = delim + 1;

    /* Skip to bssid */
    delim = strchr(p, '"');
    if (!delim) return;
    p = delim + 1;

    /* Parse bssid */
    delim = strchr(p, '"');
    if (!delim || (delim - p) >= (int)sizeof(address)) return;
    memcpy(address, p, delim - p);
    address[delim - p] = '\0';
    p = delim + 2;

    /* Parse channel */
    channel = (int)strtol(p, NULL, 10);

    if (channel != 0) {
        add_ap(name, address, (uint8_t)channel);
    }
}

/*
 * Parse a +STASCAN line and add to station list.
 * Format: +STASCAN:("<mac>")
 */
static void parse_stascan(const char *line)
{
    char address[MAX_SIZE_ADDRESS] = "";

    const char *p = strstr(line, "+STASCAN:");
    if (!p) return;
    p = strchr(p + 9, '"'); /* find opening " of mac after +STASCAN:( */
    if (!p) return;
    p++; /* skip the opening " */

    const char *delim = strchr(p, '"');
    if (!delim) return;
    int len = (int)(delim - p);
    if (len >= (int)sizeof(address))
        len = (int)sizeof(address) - 1;
    memcpy(address, p, len);
    address[len] = '\0';

    add_station(address);
}

/*
 * Parse AT+DEAUTH? response.
 * Example: +DEAUTH:(1,1,3,9)
 */
static void parse_query_deauth(const char *line)
{
    const char *p = strstr(line, "+DEAUTH:");
    if (!p) return;
    p += 9; /* skip "+DEAUTH:(" */

    int active = (int)strtol(p, NULL, 10);
    s_deauth_active = (active > 0);

    p = strchr(p, ',');
    if (!p) return;
    p++;
    s_at_deauth_channel = (uint8_t)strtol(p, NULL, 10);

    p = strchr(p, ',');
    if (!p) return;
    p++;
    s_at_deauth_enable_mode = (uint8_t)strtol(p, NULL, 10);

    p = strchr(p, ',');
    if (!p) return;
    p++;
    s_at_deauth_enable_num_modes = (uint8_t)strtol(p, NULL, 10);
}

static void parse_query_stascan(const char *line)
{
    const char *p = strstr(line, "+STASCAN:");
    if (!p) return;
    p += 10; /* skip "+STASCAN:(" */

    int active = (int)strtol(p, NULL, 10);
    s_stascan_active = (active > 0);
}

/*
 * Process a buffer that may contain multiple +CWLAP lines.
 */
static void process_apscan_response_buffer(const char *buf)
{
    const char *p = buf;
    while ((p = strstr(p, "+CWLAP:")) != NULL) {
        const char *eol = strchr(p, '\n');
        if (!eol) eol = p + strlen(p);

        char line[400];
        int llen = (int)(eol - p);
        if (llen >= (int)sizeof(line))
            llen = (int)sizeof(line) - 1;
        memcpy(line, p, llen);
        line[llen] = '\0';

        parse_apscan(line);
        p = eol;
    }
}

/*
 * Process a buffer that may contain multiple +STASCAN lines.
 */
static void process_stascan_response_buffer(const char *buf)
{
    const char *p = buf;
    while ((p = strstr(p, "+STASCAN:")) != NULL) {
        const char *eol = strchr(p, '\n');
        if (!eol) eol = p + strlen(p);

        char line[400];
        int llen = (int)(eol - p);
        if (llen >= (int)sizeof(line))
            llen = (int)sizeof(line) - 1;
        memcpy(line, p, llen);
        line[llen] = '\0';

        parse_stascan(line);
        p = eol;
    }
}

/********************* D I S P L A Y ****************************************/

static void draw_title_bar(const char *title)
{
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(&m1_u8g2, 2, 10, title);
    u8g2_DrawHLine(&m1_u8g2, 0, LIST_HEADER_HEIGHT, 128);
}

static void show_message(const char *title, const char *line1,
                         const char *line2, uint16_t delay_ms)
{
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    draw_title_bar(title);
    if (line1)
        u8g2_DrawStr(&m1_u8g2, 2, 28, line1);
    if (line2)
        u8g2_DrawStr(&m1_u8g2, 2, 40, line2);
    m1_u8g2_nextpage();
    if (delay_ms)
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

static void draw_attack(u8g2_t *u8g2)
{
    char prn_msg[128];

    m1_u8g2_firstpage();
    draw_title_bar(TITLE_DEFAULT);

    u8g2_SetFont(u8g2, M1_DISP_SUB_MENU_FONT_N);
    m1_draw_text(u8g2, 4, 28, 72, "Deauth Attack:", TEXT_ALIGN_LEFT);

    u8g2_SetFont(u8g2, M1_DISP_SUB_MENU_FONT_B);
    m1_draw_text(u8g2, 78, 28, 46, s_deauth_active ? "ON" : "OFF", TEXT_ALIGN_LEFT);

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    if (s_keyseq_unlocked) {
        snprintf(prn_msg, sizeof(prn_msg), "Mode: %u (Up/Down)", s_deauth_enable_mode);
        m1_draw_text(u8g2, 4, 38, 120, prn_msg, TEXT_ALIGN_CENTER);
    }

    m1_draw_text(u8g2, 4, 48, 120, "Press OK to toggle", TEXT_ALIGN_CENTER);
    m1_draw_bottom_bar(u8g2, NULL, NULL, "End", arrowright_8x8);
    m1_u8g2_nextpage();

    s_redraw = false;
}

static void draw_complete(u8g2_t *u8g2)
{
    m1_u8g2_firstpage();
    draw_title_bar(TITLE_DEFAULT);

    u8g2_SetFont(u8g2, M1_DISP_SUB_MENU_FONT_N);
    m1_draw_text(u8g2, 4, 28, 72, "Attack complete", TEXT_ALIGN_CENTER);

    m1_draw_bottom_bar(u8g2, NULL, NULL, "Menu", arrowright_8x8);
    m1_u8g2_nextpage();

    s_redraw = false;
}

static void draw_confirmation(u8g2_t *u8g2)
{
    char prn_msg[128];

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(u8g2, M1_DISP_DRAW_COLOR_TXT);

    if (s_state == STATE_AP_CONF)
        snprintf(prn_msg, sizeof(prn_msg), "Confirm Access Point");
    else
        snprintf(prn_msg, sizeof(prn_msg), "Confirm Station");
    draw_title_bar(prn_msg);

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    snprintf(prn_msg, sizeof(prn_msg), "Name: %s", s_selected_ap.name);
    u8g2_DrawStr(u8g2, 4, 21, prn_msg);
    snprintf(prn_msg, sizeof(prn_msg), "Channel: %u", s_selected_ap.channel);
    u8g2_DrawStr(u8g2, 4, 31, prn_msg);
    snprintf(prn_msg, sizeof(prn_msg), "BSSID: %s", s_selected_ap.address);
    u8g2_DrawStr(u8g2, 4, 41, prn_msg);
    if (s_state == STATE_STA_CONF) {
        snprintf(prn_msg, sizeof(prn_msg), "MAC:   %s", s_selected_sta);
        u8g2_DrawStr(u8g2, 4, 51, prn_msg);
    }

    m1_draw_bottom_bar(u8g2, NULL, NULL, "Ok", arrowright_8x8);
    m1_u8g2_nextpage();

    s_redraw = false;
}

static void draw_list(u8g2_t *u8g2, char *title, uint16_t count, uint16_t selection)
{
    uint16_t i;
    uint16_t start_idx;
    uint16_t visible;
    uint8_t  y;

    /* Calculate which items are visible */
    if (selection < LIST_VISIBLE_ITEMS)
        start_idx = 0;
    else
        start_idx = selection - LIST_VISIBLE_ITEMS + 1;

    visible = count - start_idx;
    if (visible > LIST_VISIBLE_ITEMS)
        visible = LIST_VISIBLE_ITEMS;

    m1_u8g2_firstpage();
    draw_title_bar(title);

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);

    if (count > 0 && (s_state == STATE_AP_SELECT || s_state == STATE_STA_SELECT)) {
        for (i = 0; i < visible; i++) {
            uint16_t idx = start_idx + i;
            y = LIST_START_Y + (i * LIST_ITEM_HEIGHT);

            if (idx == selection) {
                u8g2_SetDrawColor(u8g2, M1_DISP_DRAW_COLOR_TXT);
                u8g2_DrawBox(u8g2, 0, y, 128, LIST_ITEM_HEIGHT);
                u8g2_SetDrawColor(u8g2, M1_DISP_DRAW_COLOR_BG);
                if (s_state == STATE_AP_SELECT)
                    u8g2_DrawStr(u8g2, 4, y + 8, s_list_items[idx].name);
                else
                    u8g2_DrawStr(u8g2, 4, y + 8, s_list_items[idx].address);
                u8g2_SetDrawColor(u8g2, M1_DISP_DRAW_COLOR_TXT);
            } else {
                if (s_state == STATE_AP_SELECT)
                    u8g2_DrawStr(u8g2, 4, y + 8, s_list_items[idx].name);
                else
                    u8g2_DrawStr(u8g2, 4, y + 8, s_list_items[idx].address);
            }
        }

        /* Scroll indicator */
        if (count > LIST_VISIBLE_ITEMS) {
            uint8_t bar_height = (LIST_VISIBLE_ITEMS * LIST_ITEM_HEIGHT * LIST_VISIBLE_ITEMS)
                                 / count;
            if (bar_height < 4)
                bar_height = 4;

            uint8_t bar_y = LIST_START_Y
                          + (start_idx * (LIST_VISIBLE_ITEMS * LIST_ITEM_HEIGHT - bar_height))
                            / (count - LIST_VISIBLE_ITEMS);
            u8g2_DrawBox(u8g2, 126, bar_y, 2, bar_height);
        }
    }

    m1_u8g2_nextpage();

    s_redraw = false;
}

static void draw_menu(u8g2_t *u8g2, uint16_t selection)
{
    uint8_t y;

    m1_u8g2_firstpage();
    draw_title_bar(TITLE_DEFAULT);

    for (int i = 0; i < MAIN_MENU_OPTS; i++) {
        y = LIST_START_Y + (i * (LIST_ITEM_HEIGHT + 2));

        if (i == (int)selection) {
            u8g2_SetDrawColor(u8g2, M1_DISP_DRAW_COLOR_TXT);
            u8g2_DrawBox(u8g2, 0, y, 128, LIST_ITEM_HEIGHT);
            u8g2_SetDrawColor(u8g2, M1_DISP_DRAW_COLOR_BG);
            u8g2_DrawStr(u8g2, 4, y + 8, s_main_menu_options[i]);
            u8g2_SetDrawColor(u8g2, M1_DISP_DRAW_COLOR_TXT);
        } else {
            u8g2_DrawStr(u8g2, 4, y + 8, s_main_menu_options[i]);
        }
    }

    m1_u8g2_nextpage();

    s_redraw = false;
}

static void draw_preflight_failed(u8g2_t *u8g2)
{
    char    prn_msg[128];
    uint8_t tests = s_failed_preflight_tests;

    m1_u8g2_firstpage();

    snprintf(prn_msg, sizeof(prn_msg), "Failed Preflight Chk: %d", tests);
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2, 2, 10, prn_msg);
    u8g2_DrawHLine(u8g2, 0, LIST_HEADER_HEIGHT, 128);

    u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
    u8g2_DrawStr(u8g2, 4, 22, "Missing AT Cmds: update ESP32");

    int i = 0;
    int start_y = 32;
    for (int c = 0; c < PREFLIGHT_AT_CHECKS; c++) {
        if (tests & 1) {
            u8g2_DrawStr(u8g2, 4, start_y + (i * LIST_ITEM_HEIGHT), PREFLIGHT_AT_COMMANDS[c]);
            i++;
        }
        tests = tests >> 1;
    }

    m1_draw_text(u8g2, 4, 62, 120, "Press back to exit", TEXT_ALIGN_CENTER);
    m1_u8g2_nextpage();

    s_redraw = false;
}

/********************* A T   C O M M A N D S ********************************/

static void deauth_start(uint8_t channel, const char *bssid, const char *mac)
{
    char resp_buf[AT_RESP_BUF_SIZE];
    char at_cmd[128];

    snprintf(at_cmd, sizeof(at_cmd),
             "AT+DEAUTH=%u,%d,\"%s\",\"%s\"\r\n",
             s_deauth_enable_mode, channel, mac, bssid);
    resp_buf[0] = '\0';
    spi_AT_send_recv(at_cmd, resp_buf, sizeof(resp_buf), 3);

    s_deauth_active = true;
}

static void deauth_stop(void)
{
    char resp_buf[AT_RESP_BUF_SIZE];
    spi_AT_send_recv("AT+DEAUTH=0\r\n", resp_buf, sizeof(resp_buf), 1);
    s_deauth_active = false;
}

static void stascan_stop(void)
{
    char resp_buf[AT_RESP_BUF_SIZE];
    spi_AT_send_recv("AT+STASCAN=0\r\n", resp_buf, sizeof(resp_buf), 1);
    s_stascan_active = false;
}

/********************* M A I N   E N T R Y **********************************/

void wifi_deauth(void)
{
    bool           esp_at_error = false;
    char          *resp_buf = s_resp_buf;
    char           at_cmd[150];
    char           temp_title[MAX_SIZE_TITLE];
    game_button_t  btn = GAME_BTN_NONE;
    uint16_t       selection = 0;

    resp_buf[0]    = '\0';
    temp_title[0]  = '\0';

    /* Reset state for fresh entry */
    s_state = s_state_previous = STATE_NONE;
    s_deauth_active  = false;
    s_stascan_active = false;
    s_redraw         = true;
    s_keyseq_level   = 0;
    s_keyseq_unlocked = false;
    s_deauth_enable_mode = 1;

    /* Init ESP32 if needed */
    show_message(TITLE_SYSTEM_CHECK, "Checking ESP32 init status", NULL, 0);
    if (!m1_esp32_get_init_status())
        m1_esp32_init();
    if (!get_esp32_main_init_status())
    {
        show_message(TITLE_SYSTEM_CHECK, "Initializing ESP32", NULL, 0);
        esp32_main_init();
    }

    if (!get_esp32_main_init_status())
    {
        show_message(TITLE_SYSTEM_CHECK, "ESP32 not ready!", "Press Back", 0);
        goto wait_exit;
    }

    s_redraw = true;

    while (1)
    {
        switch (s_state)
        {
        /* --------------------------------------------- */
        case STATE_NONE:
            s_state = STATE_PREFLIGHT;
            continue;

        /* --------------------------------------------- */
        case STATE_PREFLIGHT:
            if (s_state_previous != s_state) {
                s_state_previous = s_state;
                s_redraw = true;

                show_message(TITLE_DEFAULT, "Starting Preflight", NULL, 0);

                s_failed_preflight_tests = 0;
                for (int i = 0; i < PREFLIGHT_AT_CHECKS; i++) {
                    snprintf(at_cmd, sizeof(at_cmd), "%s\r\n", PREFLIGHT_AT_COMMANDS[i]);
                    spi_AT_send_recv(at_cmd, resp_buf, AT_LARGE_RESP_BUF_SIZE, 1);

                    if (strstr(resp_buf, "TIMEOUT") || strstr(resp_buf, "SEND_ERR")) {
                        esp_at_error = true;
                    } else if (strstr(resp_buf, "ERROR")) {
                        s_failed_preflight_tests |= (1 << i);
                    }

                    vTaskDelay(pdMS_TO_TICKS(MULTI_AT_CMD_INTERVAL_MS));
                }
            }

            if (esp_at_error) {
                if (s_redraw) {
                    show_message(TITLE_ERROR, "ESP32 Comm Failure",
                                 "Press back to exit", 0);
                    s_redraw = false;
                }
            } else if (s_failed_preflight_tests != 0) {
                if (s_redraw)
                    draw_preflight_failed(&m1_u8g2);
            } else {
                s_state++;
                continue;
            }

            btn = game_poll_button(INPUT_POLL_DELAY);
            if (btn == GAME_BTN_BACK)
                break;
            break;

        /* --------------------------------------------- */
        case STATE_MENU:
            if (s_state_previous != s_state) {
                s_state_previous = s_state;
                selection = 0;
                s_redraw = true;

                show_message(TITLE_DEFAULT, "Starting menu", NULL, 0);

                resp_buf[0] = '\0';
                snprintf(at_cmd, sizeof(at_cmd), "AT+DEAUTH?\r\n");
                spi_AT_send_recv(at_cmd, resp_buf, AT_LARGE_RESP_BUF_SIZE, 5);
                parse_query_deauth(resp_buf);

                vTaskDelay(pdMS_TO_TICKS(MULTI_AT_CMD_INTERVAL_MS));

                resp_buf[0] = '\0';
                snprintf(at_cmd, sizeof(at_cmd), "AT+STASCAN?\r\n");
                spi_AT_send_recv(at_cmd, resp_buf, AT_LARGE_RESP_BUF_SIZE, 5);
                parse_query_stascan(resp_buf);

                if (s_deauth_active)
                    deauth_stop();
                if (s_stascan_active)
                    stascan_stop();
            }

            if (s_redraw)
                draw_menu(&m1_u8g2, selection);

            btn = game_poll_button(INPUT_POLL_DELAY);
            if (btn == GAME_BTN_BACK) {
                break;
            } else if (btn == GAME_BTN_UP) {
                if (selection > 0)
                    selection--;
                else
                    selection = MAIN_MENU_OPTS - 1;
                s_redraw = true;
            } else if (btn == GAME_BTN_DOWN) {
                if (selection < MAIN_MENU_OPTS - 1)
                    selection++;
                else
                    selection = 0;
                s_redraw = true;
            } else if (btn == GAME_BTN_LEFT) {
                btn = GAME_BTN_BACK;
                break;
            } else if (btn == GAME_BTN_RIGHT || btn == GAME_BTN_OK) {
                if (selection == 0) {
                    s_state++;
                } else if (selection == 2) {
                    show_message(TITLE_DEFAULT, "Resetting ESP32", NULL, 0);
                    esp32_disable();
                    m1_hard_delay(1);
                    esp32_enable();
                    m1_hard_delay(200);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    s_redraw = true;
                } else {
                    m1_buzzer_notification();
                    show_message(TITLE_DEFAULT, "Feature not",
                                 "implemented yet", 1500);
                    s_redraw = true;
                }
            }
            break;

        /* --------------------------------------------- */
        case STATE_AP_SELECT:
            if (s_state_previous != s_state) {
                s_state_previous = s_state;
                s_list_count = 0;
                selection = 0;
                s_redraw = true;

                show_message(TITLE_DEFAULT, "Scanning APs...", NULL, 0);

                snprintf(at_cmd, sizeof(at_cmd), "AT+CWMODE=1\r\n");
                spi_AT_send_recv(at_cmd, resp_buf, AT_LARGE_RESP_BUF_SIZE, 1);
                vTaskDelay(pdMS_TO_TICKS(MULTI_AT_CMD_INTERVAL_MS));

                snprintf(at_cmd, sizeof(at_cmd), "AT+CWLAPOPT=,26\r\n");
                spi_AT_send_recv(at_cmd, resp_buf, AT_LARGE_RESP_BUF_SIZE, 1);
                vTaskDelay(pdMS_TO_TICKS(MULTI_AT_CMD_INTERVAL_MS));

                resp_buf[0] = '\0';
                snprintf(at_cmd, sizeof(at_cmd), "AT+CWLAP\r\n");
                spi_AT_send_recv(at_cmd, resp_buf, AT_LARGE_RESP_BUF_SIZE, 5);
                process_apscan_response_buffer(resp_buf);
            }

            if (s_redraw) {
                snprintf(temp_title, MAX_SIZE_TITLE, "Access Points (%d)", s_list_count);
                draw_list(&m1_u8g2, temp_title, s_list_count, selection);
            }

            btn = game_poll_button(INPUT_POLL_DELAY);
            if (btn == GAME_BTN_BACK) {
                break;
            } else if (btn == GAME_BTN_UP) {
                if (selection > 0)
                    selection--;
                else if (s_list_count != 0)
                    selection = s_list_count - 1;
                s_redraw = true;
            } else if (btn == GAME_BTN_DOWN) {
                if (s_list_count > 0 && selection < s_list_count - 1)
                    selection++;
                else
                    selection = 0;
                s_redraw = true;
            } else if (btn == GAME_BTN_LEFT) {
                s_state--;
            } else if (btn == GAME_BTN_RIGHT || btn == GAME_BTN_OK) {
                if (s_list_count > 0) {
                    s_selected_ap.channel = s_list_items[selection].channel;
                    strncpy(s_selected_ap.address, s_list_items[selection].address,
                            MAX_SIZE_ADDRESS - 1);
                    s_selected_ap.address[MAX_SIZE_ADDRESS - 1] = '\0';
                    strncpy(s_selected_ap.name, s_list_items[selection].name,
                            MAX_SIZE_NAME - 1);
                    s_selected_ap.name[MAX_SIZE_NAME - 1] = '\0';
                    s_selected_sta[0] = '\0';
                    s_state++;
                }
            }
            break;

        /* --------------------------------------------- */
        case STATE_AP_CONF:
            if (s_state_previous != s_state) {
                s_state_previous = s_state;
                s_redraw = true;
            }

            if (s_redraw)
                draw_confirmation(&m1_u8g2);

            btn = game_poll_button(INPUT_POLL_DELAY);
            if (btn == GAME_BTN_BACK) {
                break;
            } else if (btn == GAME_BTN_LEFT) {
                s_state--;
            } else if (btn == GAME_BTN_RIGHT || btn == GAME_BTN_OK) {
                s_state++;
            }
            break;

        /* --------------------------------------------- */
        case STATE_STA_SELECT:
            if (s_state_previous != s_state) {
                s_state_previous = s_state;
                s_list_count = 0;
                selection = 0;
                s_redraw = true;

                show_message(TITLE_DEFAULT, "Scanning stations...", NULL, 0);

                snprintf(at_cmd, sizeof(at_cmd),
                         "AT+STASCAN=1,%d,\"%s\"\r\n",
                         s_selected_ap.channel, s_selected_ap.address);
                resp_buf[0] = '\0';
                spi_AT_send_recv(at_cmd, resp_buf, AT_LARGE_RESP_BUF_SIZE, 1);
            }

            /* Poll for unsolicited +STASCAN responses */
            resp_buf[0] = '\0';
            spi_AT_send_recv("AT\r\n", resp_buf, AT_LARGE_RESP_BUF_SIZE, 1);
            if (resp_buf[0] != '\0')
                process_stascan_response_buffer(resp_buf);

            if (s_redraw) {
                snprintf(temp_title, MAX_SIZE_TITLE, "Select Targets (%d)", s_list_count);
                draw_list(&m1_u8g2, temp_title, s_list_count, selection);
            }

            btn = game_poll_button(INPUT_POLL_DELAY);
            if (btn == GAME_BTN_BACK) {
                break;
            } else if (btn == GAME_BTN_UP) {
                if (selection > 0)
                    selection--;
                else if (s_list_count != 0)
                    selection = s_list_count - 1;
                s_redraw = true;
            } else if (btn == GAME_BTN_DOWN) {
                if (s_list_count > 0 && selection < s_list_count - 1)
                    selection++;
                else
                    selection = 0;
                s_redraw = true;
            } else if (btn == GAME_BTN_LEFT) {
                s_state--;
                stascan_stop();
            } else if (btn == GAME_BTN_RIGHT || btn == GAME_BTN_OK) {
                if (s_list_count > 0) {
                    strncpy(s_selected_sta, s_list_items[selection].address,
                            MAX_SIZE_ADDRESS - 1);
                    s_selected_sta[MAX_SIZE_ADDRESS - 1] = '\0';
                    s_state++;
                    stascan_stop();
                }
            }
            break;

        /* --------------------------------------------- */
        case STATE_STA_CONF:
            if (s_state_previous != s_state) {
                s_state_previous = s_state;
                s_redraw = true;
            }

            if (s_redraw)
                draw_confirmation(&m1_u8g2);

            btn = game_poll_button(INPUT_POLL_DELAY);
            if (btn == GAME_BTN_BACK) {
                break;
            } else if (btn == GAME_BTN_LEFT) {
                s_state--;
            } else if (btn == GAME_BTN_RIGHT || btn == GAME_BTN_OK) {
                s_state++;
            }
            break;

        /* --------------------------------------------- */
        case STATE_ATTACK:
            if (s_state_previous != s_state) {
                s_state_previous = s_state;
                s_redraw = true;
                deauth_start(s_selected_ap.channel, s_selected_ap.address,
                             s_selected_sta);
            }

            if (s_redraw)
                draw_attack(&m1_u8g2);

            btn = game_poll_button(INPUT_POLL_DELAY);
            if (btn == GAME_BTN_BACK) {
                break;
            } else if (btn == GAME_BTN_UP) {
                if (s_keyseq_unlocked && s_at_deauth_enable_num_modes > 0) {
                    if (s_deauth_enable_mode < s_at_deauth_enable_num_modes)
                        s_deauth_enable_mode++;
                    else
                        s_deauth_enable_mode = 1;
                    s_redraw = true;
                    deauth_start(s_selected_ap.channel, s_selected_ap.address,
                                 s_selected_sta);
                } else {
                    check_keyseq(GAME_BTN_UP);
                }
            } else if (btn == GAME_BTN_DOWN) {
                if (s_keyseq_unlocked && s_at_deauth_enable_num_modes > 0) {
                    if (s_deauth_enable_mode > 1)
                        s_deauth_enable_mode--;
                    else
                        s_deauth_enable_mode = s_at_deauth_enable_num_modes;
                    s_redraw = true;
                    deauth_start(s_selected_ap.channel, s_selected_ap.address,
                                 s_selected_sta);
                } else {
                    check_keyseq(GAME_BTN_DOWN);
                }
            } else if (btn == GAME_BTN_LEFT) {
                s_state--;
                deauth_stop();
            } else if (btn == GAME_BTN_OK) {
                s_redraw = true;
                if (s_deauth_active)
                    deauth_stop();
                else
                    deauth_start(s_selected_ap.channel, s_selected_ap.address,
                                 s_selected_sta);
            } else if (btn == GAME_BTN_RIGHT) {
                s_state++;
                deauth_stop();
            }
            break;

        /* --------------------------------------------- */
        case STATE_COMPLETE:
            if (s_state_previous != s_state) {
                s_state_previous = s_state;
                s_redraw = true;
                deauth_stop();
            }

            if (s_redraw)
                draw_complete(&m1_u8g2);

            btn = game_poll_button(INPUT_POLL_DELAY);
            if (btn == GAME_BTN_BACK) {
                break;
            } else if (btn == GAME_BTN_LEFT) {
                s_state--;
            } else if (btn == GAME_BTN_RIGHT || btn == GAME_BTN_OK) {
                s_state = STATE_MENU;
            }
            break;
        }

        /* Exit on BACK */
        if (btn == GAME_BTN_BACK)
            break;
    }

    /* Cleanup — always stop any active operations */
    deauth_stop();
    vTaskDelay(pdMS_TO_TICKS(MULTI_AT_CMD_INTERVAL_MS));
    stascan_stop();

    /* Tear down ESP32 SPI transport (Hapax convention) */
    m1_esp32_deinit();
    return;

wait_exit:
    {
        S_M1_Buttons_Status this_button_status;
        S_M1_Main_Q_t      q_item;
        BaseType_t          ret;

        while (1)
        {
            ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
            if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
            {
                xQueueReceive(button_events_q_hdl, &this_button_status, 0);
                if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK
                 || this_button_status.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK
                 || this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK
                 || this_button_status.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
                    break;
            }
        }
        xQueueReset(main_q_hdl);
    }

    /* Tear down ESP32 SPI transport even on error path */
    m1_esp32_deinit();
}
