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
#include "m1_esp32_caps.h"
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_scene.h"

/*************************** D E F I N E S ************************************/

#define M1_LOGDB_TAG            "BLE"

#define M1_GUI_ROW_SPACING      1

#define BLE_SCAN_TIMEOUT_MS     8000
#define BLE_CMD_TIMEOUT_MS      2000
#define BLE_NEXT_TIMEOUT_MS     1000
#define BLE_GATT_TIMEOUT_MS     22000

#define BLE_DEV_MAX             32
#define BLE_GATT_MAX            64
#define BLE_ADV_NAME_MAX        29
#define BLE_CFG_FILE            "bt.cfg"
#define BLE_LOG_DIR             "bt"
#define BLE_GATT_FILE           "bt/gatt.csv"
#define BLE_GATT_NOTIFY_FILE    "bt/gatt_notify.csv"

/* NimBLE GATT characteristic property bits (see SiN360 firmware). */
#define BLE_GATT_CHR_F_READ     0x02
#define BLE_GATT_CHR_F_WRITE_NR 0x04
#define BLE_GATT_CHR_F_WRITE    0x08
#define BLE_GATT_CHR_F_NOTIFY   0x10
#define BLE_GATT_CHR_F_INDICATE 0x20

/* Flags byte for CMD_BLE_GATT_WRITE: bit 0 = write-without-response. */
#define BLE_GATT_WRITE_NO_RSP   0x01

//************************** S T R U C T U R E S *******************************

typedef struct {
    uint8_t  addr[6];
    uint8_t  addr_type;
    int8_t   rssi;
    char     name[30];
    char     addr_str[18]; /* "XX:XX:XX:XX:XX:XX" */
} ble_dev_t;

/* One row of the GATT discovery result table.
 *  type      1 = service, 2 = characteristic, 3 = descriptor.
 *  handle1   service start handle (svc) / characteristic def handle (chr) /
 *            descriptor handle (dsc).
 *  handle2   service end handle (svc) / characteristic value handle (chr) /
 *            owning characteristic value handle (dsc).
 *  props     characteristic property bits (BLE_GATT_CHR_F_*); valid only when
 *            type == 2.
 *  uuid      raw UUID bytes (LE) — 2, 4, or 16 bytes.
 *  value     short read-cache of the characteristic / descriptor value (up to
 *            16 bytes) when the peer responded to a read during discovery. */
typedef struct {
    uint8_t  type;
    uint16_t handle1;
    uint16_t handle2;
    uint8_t  props;
    uint8_t  uuid_len;
    uint8_t  uuid[16];
    uint8_t  value_len;
    uint8_t  value[16];
} ble_gatt_entry_t;

/* One notification / indication delivered by CMD_BLE_GATT_NOTIF. */
typedef struct {
    uint16_t handle;
    uint8_t  indication; /* 0 = notify, 1 = indicate */
    uint8_t  value_len;
    uint8_t  value[55];
} ble_gatt_notify_t;

/***************************** V A R I A B L E S ******************************/

static ble_dev_t *ble_list = NULL;
static uint16_t ble_count = 0;
static uint16_t ble_view_idx = 0;
static char ble_adv_name[BLE_ADV_NAME_MAX + 1] = "SiN360-M1";
static bool ble_adv_name_loaded = false;
static ble_gatt_entry_t ble_gatt_list[BLE_GATT_MAX];
static uint16_t ble_gatt_count = 0;
static uint16_t ble_gatt_view_idx = 0;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

void menu_bluetooth_init(void);
void bluetooth_config(void);
void bluetooth_scan(void);
void bluetooth_advertise(void);
void ble_gatt_discovery(void);

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


/*============================================================================*/
/*  GATT Discovery (NimBLE binary SPI client)                                 */
/*                                                                            */
/*  Imported from sincere360/M1_SiN360_ESP32 reference firmware.              */
/*  Uses CMD_BLE_GATT_START/NEXT/STOP/WRITE/SUB/NOTIF opcodes.  Runs as a     */
/*  blocking delegate behind the BtSceneGattDiscovery scene wrapper, which    */
/*  also handles ESP32 init/deinit and M1_ESP32_CAP_BLE_GATT gating.          */
/*============================================================================*/

/* -------- Hex / CSV helpers -------- */

static void ble_hex_encode(char *out, size_t out_len, const uint8_t *data, uint8_t data_len)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t pos = 0;

    if (!out || out_len == 0) return;

    for (uint8_t i = 0; i < data_len && pos + 2 < out_len; i++)
    {
        out[pos++] = hex[(data[i] >> 4) & 0x0F];
        out[pos++] = hex[data[i] & 0x0F];
    }
    out[pos] = '\0';
}

static int ble_hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static uint8_t ble_hex_parse(const char *in, uint8_t *out, uint8_t out_len)
{
    int high = -1;
    uint8_t count = 0;

    if (!in || !out || out_len == 0) return 0;

    for (size_t i = 0; in[i] && count < out_len; i++)
    {
        int v = ble_hex_value(in[i]);
        if (v < 0) continue;

        if (high < 0)
        {
            high = v;
        }
        else
        {
            out[count++] = (uint8_t)((high << 4) | v);
            high = -1;
        }
    }

    return count;
}

static void ble_csv_quote_field(char *dst, const char *src, size_t dst_len)
{
    size_t pos = 0;

    if (!dst || dst_len == 0) return;
    if (dst_len < 3)
    {
        dst[0] = '\0';
        return;
    }

    dst[pos++] = '"';

    if (src)
    {
        for (size_t i = 0; src[i] && pos + 2 < dst_len; i++)
        {
            if (src[i] == '"')
            {
                dst[pos++] = '"';
                if (pos + 1 >= dst_len) break;
            }
            dst[pos++] = src[i];
        }
    }

    if (pos + 1 < dst_len)
    {
        dst[pos++] = '"';
    }
    else
    {
        pos = dst_len - 2;
        dst[pos++] = '"';
    }
    dst[pos] = '\0';
}

static bool ble_append_header_if_new(FIL *fil, const char *path, const char *header)
{
    FILINFO info;
    UINT bw;
    bool new_file = (f_stat(path, &info) != FR_OK || info.fsize == 0);

    if (f_open(fil, path, FA_WRITE | FA_OPEN_APPEND) != FR_OK)
    {
        return false;
    }

    if (new_file && header)
    {
        f_write(fil, header, strlen(header), &bw);
    }

    return true;
}

/* -------- Wait-for-BACK helper (used by detail / pending screens) -------- */

static void ble_gatt_wait_back(void)
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

/* -------- GATT helpers -------- */

static void ble_uuid_to_text(const ble_gatt_entry_t *e, char *out, size_t out_len)
{
    if (!out || out_len == 0) return;

    if (e->uuid_len == 2)
    {
        uint16_t v = e->uuid[0] | ((uint16_t)e->uuid[1] << 8);
        snprintf(out, out_len, "UUID:%04X", v);
    }
    else if (e->uuid_len == 4)
    {
        uint32_t v = e->uuid[0] | ((uint32_t)e->uuid[1] << 8) |
            ((uint32_t)e->uuid[2] << 16) | ((uint32_t)e->uuid[3] << 24);
        snprintf(out, out_len, "UUID:%08lX", (unsigned long)v);
    }
    else if (e->uuid_len == 16)
    {
        snprintf(out, out_len, "UUID:%02X%02X%02X%02X...",
            e->uuid[15], e->uuid[14], e->uuid[13], e->uuid[12]);
    }
    else
    {
        snprintf(out, out_len, "UUID:unknown");
    }
}

static void ble_gatt_props_text(uint8_t props, char *out, size_t out_len)
{
    if (!out || out_len == 0) return;
    out[0] = '\0';
    if ((props & BLE_GATT_CHR_F_READ) && strlen(out) + 1 < out_len) strcat(out, "R");
    if ((props & BLE_GATT_CHR_F_WRITE) && strlen(out) + 1 < out_len) strcat(out, "W");
    if ((props & BLE_GATT_CHR_F_WRITE_NR) && strlen(out) + 1 < out_len) strcat(out, "w");
    if ((props & BLE_GATT_CHR_F_NOTIFY) && strlen(out) + 1 < out_len) strcat(out, "N");
    if ((props & BLE_GATT_CHR_F_INDICATE) && strlen(out) + 1 < out_len) strcat(out, "I");
    if (out[0] == '\0')
    {
        strncpy(out, "-", out_len - 1);
        out[out_len - 1] = '\0';
    }
}

static void ble_gatt_export(const ble_dev_t *dev)
{
    FIL fil;
    UINT bw;
    char line[210];
    char name[70];
    char uuid_hex[33];
    char value_hex[33];

    if (!dev || ble_gatt_count == 0) return;

    f_mkdir(BLE_LOG_DIR);
    if (!ble_append_header_if_new(&fil, BLE_GATT_FILE,
        "target_addr,target_name,type,handle1,handle2,props,uuid_hex,value_hex\r\n"))
    {
        return;
    }

    ble_csv_quote_field(name, dev->name[0] ? dev->name : "*no name*", sizeof(name));
    for (uint16_t i = 0; i < ble_gatt_count; i++)
    {
        const ble_gatt_entry_t *e = &ble_gatt_list[i];
        const char *type = (e->type == 1) ? "svc" : (e->type == 2) ? "chr" : "dsc";
        ble_hex_encode(uuid_hex, sizeof(uuid_hex), e->uuid, e->uuid_len);
        ble_hex_encode(value_hex, sizeof(value_hex), e->value, e->value_len);
        int len = snprintf(line, sizeof(line),
            "%s,%s,%s,%04X,%04X,%02X,%s,%s\r\n",
            dev->addr_str, name, type, e->handle1, e->handle2,
            e->props, uuid_hex, value_hex);
        if (len > 0 && len < (int)sizeof(line))
        {
            f_write(&fil, line, len, &bw);
        }
    }

    f_close(&fil);
}

static void ble_gatt_disconnect(void)
{
    m1_resp_t resp;
    m1_esp32_simple_cmd(CMD_BLE_GATT_STOP, &resp, BLE_CMD_TIMEOUT_MS);
}

static uint16_t ble_gatt_fetch(const ble_dev_t *dev)
{
    m1_cmd_t cmd;
    m1_resp_t resp;
    int ret;
    uint16_t expected;

    ble_gatt_count = 0;
    ble_gatt_view_idx = 0;

    if (!dev) return 0;

    m1_u8g2_firstpage();
    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 6, 15, "GATT Discovery");
    u8g2_DrawStr(&m1_u8g2, 6, 30, "Connecting...");
    u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 18 / 2,
        M1_LCD_DISPLAY_HEIGHT / 2 - 2, 18, 32, hourglass_18x32);
    m1_u8g2_nextpage();

    memset(&cmd, 0, sizeof(cmd));
    cmd.magic = M1_CMD_MAGIC;
    cmd.cmd_id = CMD_BLE_GATT_START;
    cmd.payload[0] = dev->addr_type;
    memcpy(&cmd.payload[1], dev->addr, 6);
    cmd.payload_len = 7;

    ret = m1_esp32_send_cmd(&cmd, &resp, BLE_GATT_TIMEOUT_MS);
    if (ret != 0 || resp.status != RESP_OK || resp.payload_len < 2)
    {
        ble_show_pending("GATT Discovery", "Connect/disc failed", "Try another device");
        return 0;
    }

    expected = resp.payload[0] | ((uint16_t)resp.payload[1] << 8);
    if (expected > BLE_GATT_MAX) expected = BLE_GATT_MAX;

    for (uint16_t i = 0; i < expected; i++)
    {
        ret = m1_esp32_simple_cmd(CMD_BLE_GATT_NEXT, &resp, BLE_NEXT_TIMEOUT_MS);
        if (ret != 0 || resp.status != RESP_OK || resp.payload_len < 7) break;
        if (resp.payload[0] == 0) break;

        ble_gatt_entry_t *e = &ble_gatt_list[ble_gatt_count];
        memset(e, 0, sizeof(*e));
        e->type = resp.payload[0];
        e->handle1 = resp.payload[1] | ((uint16_t)resp.payload[2] << 8);
        e->handle2 = resp.payload[3] | ((uint16_t)resp.payload[4] << 8);
        e->props = resp.payload[5];
        e->uuid_len = resp.payload[6];
        if (e->uuid_len > 16) e->uuid_len = 16;
        if (resp.payload_len >= 7 + e->uuid_len)
        {
            memcpy(e->uuid, &resp.payload[7], e->uuid_len);
        }
        uint8_t pos = 7 + e->uuid_len;
        if (resp.payload_len > pos)
        {
            e->value_len = resp.payload[pos++];
            if (e->value_len > sizeof(e->value)) e->value_len = sizeof(e->value);
            if (resp.payload_len >= pos + e->value_len)
            {
                memcpy(e->value, &resp.payload[pos], e->value_len);
            }
        }
        ble_gatt_count++;
    }

    ble_gatt_export(dev);
    if (ble_gatt_count == 0)
    {
        ble_gatt_disconnect();
    }
    return ble_gatt_count;
}

static bool ble_gatt_write_value(uint16_t handle, uint8_t flags,
    const uint8_t *value, uint8_t value_len)
{
    m1_cmd_t cmd;
    m1_resp_t resp;

    if (handle == 0 || !value || value_len == 0 || value_len > 56) return false;

    memset(&cmd, 0, sizeof(cmd));
    cmd.magic = M1_CMD_MAGIC;
    cmd.cmd_id = CMD_BLE_GATT_WRITE;
    cmd.payload[0] = handle & 0xFF;
    cmd.payload[1] = (handle >> 8) & 0xFF;
    cmd.payload[2] = flags;
    cmd.payload[3] = value_len;
    memcpy(&cmd.payload[4], value, value_len);
    cmd.payload_len = 4 + value_len;

    return m1_esp32_send_cmd(&cmd, &resp, BLE_CMD_TIMEOUT_MS + 3000) == 0 &&
        resp.status == RESP_OK;
}

static bool ble_gatt_subscribe_handle(uint16_t handle, bool enable, uint8_t mode)
{
    m1_cmd_t cmd;
    m1_resp_t resp;

    if (handle == 0) return false;

    memset(&cmd, 0, sizeof(cmd));
    cmd.magic = M1_CMD_MAGIC;
    cmd.cmd_id = CMD_BLE_GATT_SUB;
    cmd.payload[0] = handle & 0xFF;
    cmd.payload[1] = (handle >> 8) & 0xFF;
    cmd.payload[2] = enable ? 1 : 0;
    cmd.payload[3] = mode;
    cmd.payload_len = 4;

    return m1_esp32_send_cmd(&cmd, &resp, BLE_CMD_TIMEOUT_MS + 2000) == 0 &&
        resp.status == RESP_OK;
}

static bool ble_gatt_notify_next(ble_gatt_notify_t *notif)
{
    m1_resp_t resp;

    if (!notif) return false;
    memset(notif, 0, sizeof(*notif));

    if (m1_esp32_simple_cmd(CMD_BLE_GATT_NOTIF, &resp, BLE_NEXT_TIMEOUT_MS) != 0 ||
        resp.status != RESP_OK || resp.payload_len < 1 || resp.payload[0] == 0)
    {
        return false;
    }

    if (resp.payload_len < 5) return false;

    notif->handle = resp.payload[1] | ((uint16_t)resp.payload[2] << 8);
    notif->indication = resp.payload[3];
    notif->value_len = resp.payload[4];
    if (notif->value_len > resp.payload_len - 5) notif->value_len = resp.payload_len - 5;
    if (notif->value_len > sizeof(notif->value)) notif->value_len = sizeof(notif->value);
    memcpy(notif->value, &resp.payload[5], notif->value_len);
    return true;
}

static void ble_gatt_log_notify(const ble_dev_t *dev, const ble_gatt_notify_t *notif)
{
    FIL fil;
    UINT bw;
    char line[170];
    char name[70];
    char hex[111];

    if (!dev || !notif) return;

    f_mkdir(BLE_LOG_DIR);
    if (!ble_append_header_if_new(&fil, BLE_GATT_NOTIFY_FILE,
        "target_addr,target_name,handle,type,value_hex\r\n"))
    {
        return;
    }

    ble_csv_quote_field(name, dev->name[0] ? dev->name : "*no name*", sizeof(name));
    ble_hex_encode(hex, sizeof(hex), notif->value, notif->value_len);
    int len = snprintf(line, sizeof(line), "%s,%s,%04X,%s,%s\r\n",
        dev->addr_str, name, notif->handle,
        notif->indication ? "indicate" : "notify", hex);
    if (len > 0 && len < (int)sizeof(line))
    {
        f_write(&fil, line, len, &bw);
    }
    f_close(&fil);
}

static void ble_gatt_show_detail(const ble_gatt_entry_t *e)
{
    char uuid[24];
    char props[12];
    char ln[26];
    char value_hex[25];

    if (!e) return;
    ble_uuid_to_text(e, uuid, sizeof(uuid));
    ble_gatt_props_text(e->props, props, sizeof(props));
    ble_hex_encode(value_hex, sizeof(value_hex), e->value, e->value_len);

    m1_u8g2_firstpage();
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 9, "GATT Detail");
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);
    snprintf(ln, sizeof(ln), "H:%04X V:%04X", e->handle1, e->handle2);
    u8g2_DrawStr(&m1_u8g2, 2, 24, ln);
    snprintf(ln, sizeof(ln), "Props:%s Raw:%02X", props, e->props);
    u8g2_DrawStr(&m1_u8g2, 2, 34, ln);
    u8g2_DrawStr(&m1_u8g2, 2, 44, uuid);
    if (e->value_len > 0) u8g2_DrawStr(&m1_u8g2, 2, 54, value_hex);
    u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK] Exit");
    m1_u8g2_nextpage();

    ble_gatt_wait_back();
}

static uint8_t ble_gatt_write_flags(const ble_gatt_entry_t *e)
{
    if (!e) return 0;
    if ((e->props & BLE_GATT_CHR_F_WRITE) == 0 &&
        (e->props & BLE_GATT_CHR_F_WRITE_NR) != 0)
    {
        return BLE_GATT_WRITE_NO_RSP;
    }
    return 0;
}

static void ble_gatt_write_text_tool(const ble_gatt_entry_t *e)
{
    char text[41] = {0};
    char ln[26];
    bool ok;

    if (!e || m1_vkb_get_text("GATT text:", "", text, sizeof(text) - 1) == 0)
    {
        return;
    }

    ok = ble_gatt_write_value(e->handle2, ble_gatt_write_flags(e),
        (const uint8_t *)text, (uint8_t)strlen(text));
    if (ok) snprintf(ln, sizeof(ln), "Wrote %u bytes", (unsigned)strlen(text));
    else snprintf(ln, sizeof(ln), "Write failed");
    ble_show_pending("GATT Write", ln, NULL);
}

static void ble_gatt_write_hex_tool(const ble_gatt_entry_t *e)
{
    char hex[57] = {0};
    uint8_t data[28];
    uint8_t len;
    char ln[26];
    bool ok;

    if (!e || m1_vkb_get_text("GATT hex:", "", hex, sizeof(hex) - 1) == 0)
    {
        return;
    }

    len = ble_hex_parse(hex, data, sizeof(data));
    if (len == 0)
    {
        ble_show_pending("GATT Write", "No hex bytes", NULL);
        return;
    }

    ok = ble_gatt_write_value(e->handle2, ble_gatt_write_flags(e), data, len);
    if (ok) snprintf(ln, sizeof(ln), "Wrote %u bytes", len);
    else snprintf(ln, sizeof(ln), "Write failed");
    ble_show_pending("GATT Write", ln, NULL);
}

static void ble_gatt_notify_view(const ble_dev_t *dev, const ble_gatt_entry_t *e)
{
    S_M1_Buttons_Status btn;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    ble_gatt_notify_t notif;
    uint32_t count = 0;
    char ln[26];
    char hex[25] = "";
    uint16_t last_handle = e ? e->handle2 : 0;
    uint8_t last_len = 0;
    uint8_t mode = (e && (e->props & BLE_GATT_CHR_F_INDICATE)) ? 2 : 1;

    if (!e || !ble_gatt_subscribe_handle(e->handle2, true, mode))
    {
        ble_show_pending("GATT Notify", "Subscribe failed", NULL);
        return;
    }

    while (1)
    {
        while (ble_gatt_notify_next(&notif))
        {
            count++;
            last_handle = notif.handle;
            last_len = notif.value_len;
            ble_hex_encode(hex, sizeof(hex), notif.value,
                notif.value_len > 12 ? 12 : notif.value_len);
            ble_gatt_log_notify(dev, &notif);
        }

        m1_u8g2_firstpage();
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 2, 9,
            mode == 2 ? "GATT Indicate" : "GATT Notify");
        u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);
        snprintf(ln, sizeof(ln), "Count:%lu Len:%u", (unsigned long)count, last_len);
        u8g2_DrawStr(&m1_u8g2, 2, 24, ln);
        snprintf(ln, sizeof(ln), "Handle:%04X", last_handle);
        u8g2_DrawStr(&m1_u8g2, 2, 34, ln);
        if (hex[0]) u8g2_DrawStr(&m1_u8g2, 2, 46, hex);
        else u8g2_DrawStr(&m1_u8g2, 2, 46, "Waiting...");
        u8g2_DrawStr(&m1_u8g2, 2, 63, "[BACK] Stop");
        m1_u8g2_nextpage();

        ret = xQueueReceive(main_q_hdl, &q_item, pdMS_TO_TICKS(500));
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

    ble_gatt_subscribe_handle(e->handle2, false, mode);
}

static bool ble_gatt_entry_has_tools(const ble_gatt_entry_t *e)
{
    if (!e || e->type != 2) return false;
    return (e->props & (BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NR |
        BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE)) != 0;
}

static void ble_gatt_tools(const ble_dev_t *dev, const ble_gatt_entry_t *e)
{
    S_M1_Buttons_Status btn;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    const char *labels[5];
    uint8_t actions[5];
    uint8_t count = 0;
    uint8_t sel = 0;
    char ln[26];

    if (!e || e->type != 2)
    {
        ble_gatt_show_detail(e);
        return;
    }

    labels[count] = "Detail"; actions[count++] = 0;
    if (e->props & (BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NR))
    {
        labels[count] = "Write Text"; actions[count++] = 1;
        labels[count] = "Write Hex"; actions[count++] = 2;
    }
    if (e->props & (BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_INDICATE))
    {
        labels[count] = "Notify View"; actions[count++] = 3;
        labels[count] = "Notify Off"; actions[count++] = 4;
    }

    while (1)
    {
        m1_u8g2_firstpage();
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 2, 9, "GATT Tools");
        u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);
        snprintf(ln, sizeof(ln), "Value:%04X", e->handle2);
        u8g2_DrawStr(&m1_u8g2, 2, 24, ln);
        for (uint8_t i = 0; i < count && i < 3; i++)
        {
            uint8_t idx = (sel + i) % count;
            snprintf(ln, sizeof(ln), "%c%s", i == 0 ? '>' : ' ', labels[idx]);
            u8g2_DrawStr(&m1_u8g2, 2, 36 + i * 9, ln);
        }
        u8g2_DrawStr(&m1_u8g2, 2, 63, "[OK]Run [BACK]");
        m1_u8g2_nextpage();

        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE || q_item.q_evt_type != Q_EVENT_KEYPAD) continue;
        xQueueReceive(button_events_q_hdl, &btn, 0);

        if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            xQueueReset(main_q_hdl);
            break;
        }
        else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (sel == 0) sel = count - 1;
            else sel--;
        }
        else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel++;
            if (sel >= count) sel = 0;
        }
        else if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            switch (actions[sel])
            {
            case 0:
                ble_gatt_show_detail(e);
                break;
            case 1:
                ble_gatt_write_text_tool(e);
                break;
            case 2:
                ble_gatt_write_hex_tool(e);
                break;
            case 3:
                ble_gatt_notify_view(dev, e);
                break;
            case 4:
                ble_show_pending("GATT Notify",
                    ble_gatt_subscribe_handle(e->handle2, false,
                        (e->props & BLE_GATT_CHR_F_INDICATE) ? 2 : 1) ?
                        "Notify disabled" : "Disable failed",
                    NULL);
                break;
            default:
                break;
            }
        }
    }
}

static uint16_t ble_gatt_print(bool up_dir)
{
    char ln[26];
    char uuid[24];
    char props[12];

    if (ble_gatt_count == 0) return 0;

    if (up_dir)
    {
        if (ble_gatt_view_idx == 0) ble_gatt_view_idx = ble_gatt_count - 1;
        else ble_gatt_view_idx--;
    }
    else
    {
        ble_gatt_view_idx++;
        if (ble_gatt_view_idx >= ble_gatt_count) ble_gatt_view_idx = 0;
    }

    ble_gatt_entry_t *e = &ble_gatt_list[ble_gatt_view_idx];
    ble_uuid_to_text(e, uuid, sizeof(uuid));
    ble_gatt_props_text(e->props, props, sizeof(props));

    m1_u8g2_firstpage();
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 9, "GATT Discovery");
    snprintf(ln, sizeof(ln), "%d/%d", ble_gatt_view_idx + 1, ble_gatt_count);
    u8g2_DrawStr(&m1_u8g2, 98, 9, ln);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);
    if (e->type == 1)
    {
        snprintf(ln, sizeof(ln), "SVC %04X-%04X", e->handle1, e->handle2);
        u8g2_DrawStr(&m1_u8g2, 2, 26, ln);
    }
    else if (e->type == 2)
    {
        snprintf(ln, sizeof(ln), "CHR %04X Val:%04X", e->handle1, e->handle2);
        u8g2_DrawStr(&m1_u8g2, 2, 26, ln);
        snprintf(ln, sizeof(ln), "Props:%s %02X", props, e->props);
        u8g2_DrawStr(&m1_u8g2, 2, 36, ln);
    }
    else
    {
        snprintf(ln, sizeof(ln), "DSC %04X Chr:%04X", e->handle1, e->handle2);
        u8g2_DrawStr(&m1_u8g2, 2, 26, ln);
    }

    if (e->value_len > 0)
    {
        char value_hex[25];
        ble_hex_encode(value_hex, sizeof(value_hex), e->value, e->value_len);
        u8g2_DrawStr(&m1_u8g2, 2, 46, value_hex);
    }
    else
    {
        u8g2_DrawStr(&m1_u8g2, 2, 48, uuid);
    }
    if (ble_gatt_entry_has_tools(e)) u8g2_DrawStr(&m1_u8g2, 2, 63, "[OK]Tools [BACK]");
    else u8g2_DrawStr(&m1_u8g2, 2, 63, "[OK]Detail [BACK]");
    m1_u8g2_nextpage();

    return ble_gatt_count;
}

/*============================================================================*/
/**
  * @brief BLE GATT Discovery: scan, pick a device, enumerate services /
  *        characteristics / descriptors, write or subscribe to notifications.
  *
  * Blocking delegate — runs its own button event loop until BACK pops back.
  * The scene wrapper (BtSceneGattDiscovery) handles ESP32 init/deinit and the
  * M1_ESP32_CAP_BLE_GATT capability gate.
  */
/*============================================================================*/
void ble_gatt_discovery(void)
{
    S_M1_Buttons_Status btn;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    bool showing_gatt = false;
    uint16_t list_count;

    ble_ensure_esp32_ready();

    u8g2_SetFont(&m1_u8g2, M1_DISP_MAIN_MENU_FONT_N);
    m1_u8g2_firstpage();
    u8g2_DrawStr(&m1_u8g2, 6, 15, "GATT Discovery");
    u8g2_DrawStr(&m1_u8g2, 6, 30, "Scanning BLE...");
    u8g2_DrawXBMP(&m1_u8g2, M1_LCD_DISPLAY_WIDTH / 2 - 18 / 2,
        M1_LCD_DISPLAY_HEIGHT / 2 - 2, 18, 32, hourglass_18x32);
    m1_u8g2_nextpage();

    list_count = ble_do_scan();
    if (list_count == 0)
    {
        ble_show_pending("GATT Discovery", "No devices found", NULL);
        return;
    }

    ble_view_idx = 1;
    ble_list_print(true);

    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE || q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

        xQueueReceive(button_events_q_hdl, &btn, 0);
        if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (showing_gatt)
            {
                ble_gatt_disconnect();
            }
            ble_list_free();
            xQueueReset(main_q_hdl);
            break;
        }
        else if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (showing_gatt) ble_gatt_print(true);
            else ble_list_print(true);
        }
        else if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (showing_gatt) ble_gatt_print(false);
            else ble_list_print(false);
        }
        else if (!showing_gatt && btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (ble_gatt_fetch(&ble_list[ble_view_idx]) > 0)
            {
                showing_gatt = true;
                ble_gatt_view_idx = 1;
                ble_gatt_print(true);
            }
        }
        else if (showing_gatt && btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            ble_gatt_tools(&ble_list[ble_view_idx], &ble_gatt_list[ble_gatt_view_idx]);
            if (ble_gatt_count > 0)
            {
                ble_gatt_view_idx++;
                if (ble_gatt_view_idx >= ble_gatt_count) ble_gatt_view_idx = 0;
            }
            ble_gatt_print(true);
        }
    }
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
/*  Google Fast Pair — Android device pairing notifications                  */
/*============================================================================*/

static const uint8_t google_fastpair_payloads[][31] = {
    { 0x02, 0x01, 0x06, 0x03, 0x03, 0x2C, 0xFE,
      0x06, 0x16, 0x2C, 0xFE, 0xCD, 0x82, 0x1C },
    { 0x02, 0x01, 0x06, 0x03, 0x03, 0x2C, 0xFE,
      0x06, 0x16, 0x2C, 0xFE, 0xD9, 0x46, 0x3C },
    { 0x02, 0x01, 0x06, 0x03, 0x03, 0x2C, 0xFE,
      0x06, 0x16, 0x2C, 0xFE, 0xF0, 0x00, 0x02 },
    { 0x02, 0x01, 0x06, 0x03, 0x03, 0x2C, 0xFE,
      0x06, 0x16, 0x2C, 0xFE, 0x72, 0xFB, 0x00 },
    { 0x02, 0x01, 0x06, 0x03, 0x03, 0x2C, 0xFE,
      0x06, 0x16, 0x2C, 0xFE, 0xA7, 0xA6, 0xB8 },
    { 0x02, 0x01, 0x06, 0x03, 0x03, 0x2C, 0xFE,
      0x06, 0x16, 0x2C, 0xFE, 0x05, 0xA9, 0x63 },
};
static const uint8_t google_fastpair_lens[] = { 14, 14, 14, 14, 14, 14 };

void ble_spam_google_fastpair(void)
{
    ble_ensure_esp32_ready();
    ble_spam_run_loop("GOOGLE FP",
        google_fastpair_payloads, google_fastpair_lens, 6);
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
        uint8_t sent = ble_spam_payload_count(sent_type);

        switch (type_idx)
        {
        case 0:
            for (uint8_t i = 0; i < sent; i++)
                ble_raw_adv_send(sour_apple_payloads[i], sour_apple_lens[i]);
            break;
        case 1:
            for (uint8_t i = 0; i < sent; i++)
                ble_raw_adv_send(swiftpair_payloads[i], swiftpair_lens[i]);
            break;
        case 2:
            for (uint8_t i = 0; i < sent; i++)
                ble_raw_adv_send(samsung_payloads[i], samsung_lens[i]);
            break;
        case 3:
            for (uint8_t i = 0; i < sent; i++)
                ble_raw_adv_send(flipper_payloads[i], flipper_lens[i]);
            break;
        case 4:
            for (uint8_t i = 0; i < sent; i++)
                ble_raw_adv_send(google_fastpair_payloads[i], google_fastpair_lens[i]);
            break;
        }
        type_idx = (type_idx + 1) % 5;
        pkt_count += sent;

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

        const char *names[] = {"Apple", "Windows", "Samsung", "Flipper", "Google"};
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
/* Legacy AT-layer stubs — runtime-gated via M1_ESP32_CAP_* bits.          */
/* DELEGATE_CAPPED in m1_bt_scene.c calls m1_esp32_require_cap() before    */
/* reaching these; the second check here is redundant but harmless.         */
/*==========================================================================*/

void bluetooth_saved_devices(void)
{
    m1_esp32_require_cap(M1_ESP32_CAP_BT_MANAGE, "Saved Devices");
}

void bluetooth_info(void)
{
    m1_esp32_require_cap(M1_ESP32_CAP_BT_MANAGE, "BT Info");
}

bt_connection_state_t *bt_get_connection_state(void)
{
	/* SiN360 binary-SPI firmware does not support legacy AT BT management.
	 * Return a stub state that is always disconnected so the home-screen
	 * BT icon remains hidden. */
	static bt_connection_state_t state = { .connected = false };
	return &state;
}

void bluetooth_set_badbt_name(void)
{
    m1_esp32_require_cap(M1_ESP32_CAP_BLE_HID, "BT Name");
}
