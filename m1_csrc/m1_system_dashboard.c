/* See COPYING.txt for license details. */

/**
 * @file   m1_system_dashboard.c
 * @brief  3-page at-a-glance system status dashboard.
 *
 * Ported from dagnazty/M1_T-1000 (app_system_dashboard.c), adapted for
 * Hapax firmware:
 *   - Replaced m1_t1000_version.h with Hapax versioning macros
 *   - Replaced hardcoded fonts with m1_menu_font() helper
 *   - Replaced m1_draw_header_bar/m1_draw_content_frame with inline drawing
 *   - Page indicator at bottom (no "Back" button bar per Hapax UI rules)
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "stm32h5xx_hal.h"
#include "m1_system_dashboard.h"
#include "m1_games.h"
#include "m1_sdcard.h"
#include "m1_system_dashboard_helpers.h"
#include "m1_esp32_hal.h"
#include "m1_usb_cdc_msc.h"
#include "m1_fw_update_bl.h"
#include "m1_system.h"
#include "m1_display.h"
#include "m1_scene.h"
#include "battery.h"
/* #include "esp_app_main.h"  -- replaced by binary SPI protocol (m1_esp32_cmd.h) */

/*************************** D E F I N E S ************************************/

#define DASHBOARD_PAGE_COUNT   3U
#define DASHBOARD_POLL_MS      200U

/************************** S T R U C T U R E S *******************************/

typedef enum
{
    DASHBOARD_PAGE_OVERVIEW = 0,
    DASHBOARD_PAGE_IO,
    DASHBOARD_PAGE_SYSTEM
} dashboard_page_t;

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

static const char *dashboard_orientation_text(void);
static void dashboard_draw_page(dashboard_page_t page);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

static const char *dashboard_orientation_text(void)
{
    switch (m1_screen_orientation)
    {
        case M1_ORIENT_SOUTHPAW:
            return "Southpaw";
        case M1_ORIENT_REMOTE:
            return "Remote";
        case M1_ORIENT_NORMAL:
        default:
            return "Normal";
    }
}

static void dashboard_draw_page(dashboard_page_t page)
{
    char badge[8];
    char line1[40];
    char line2[40];
    char line3[40];
    char line4[40];
    S_M1_Power_Status_t pwr;
    S_M1_SDCard_Access_Status sd_status;
    S_M1_SDCard_Info *sd_info;
    m1_time_t now;
    const char *usb_mode;
    const char *usb_link;
    uint32_t battery_voltage_tenths;

    battery_power_status_get(&pwr);
    m1_get_localtime(&now);
    battery_voltage_tenths = ((uint32_t)(pwr.battery_voltage + 50.0f)) / 100U;
    sd_status = m1_sdcard_get_status();
    sd_info = (sd_status == SD_access_OK) ? m1_sdcard_get_info() : NULL;

#ifdef M1_APP_BADUSB_ENABLE
    if (m1_usb_get_current_mode() == M1_USB_MODE_HID)
    {
        usb_mode = "HID";
    }
    else
#endif
    {
        usb_mode = (m1_usbcdc_mode == CDC_MODE_VCP) ? "VCP" : "CLI";
    }
    usb_link = (hUsbDeviceFS.dev_state == USBD_STATE_CONFIGURED) ? "Linked" : "Idle";

    snprintf(badge, sizeof(badge), "%u/%u",
             (unsigned)page + 1U, (unsigned)DASHBOARD_PAGE_COUNT);

    line1[0] = '\0';
    line2[0] = '\0';
    line3[0] = '\0';
    line4[0] = '\0';

    if (page == DASHBOARD_PAGE_OVERVIEW)
    {
        char uptime[20];
        dashboard_format_uptime(HAL_GetTick(), uptime, sizeof(uptime));
        snprintf(line1, sizeof(line1), "%02u:%02u:%02u  %02u/%02u/%02u",
                 now.hour, now.minute, now.second,
                 now.month, now.day, now.year % 100U);
        snprintf(line2, sizeof(line2), "Battery %u%%  %uC",
                 pwr.battery_level, pwr.battery_temp);
        snprintf(line3, sizeof(line3), "VBAT %u.%uV  %dmA",
                 (unsigned)(battery_voltage_tenths / 10U),
                 (unsigned)(battery_voltage_tenths % 10U),
                 pwr.consumption_current);
        snprintf(line4, sizeof(line4), "Uptime %s", uptime);
    }
    else if (page == DASHBOARD_PAGE_IO)
    {
        if (sd_info != NULL)
        {
            snprintf(line1, sizeof(line1), "SD %s  %luG free",
                     dashboard_sd_status_text(sd_status),
                     (unsigned long)(sd_info->free_cap_kb / 1024UL / 1024UL));
            snprintf(line2, sizeof(line2), "Card %s",
                     sd_info->vol_label[0] ? sd_info->vol_label : "No label");
        }
        else
        {
            snprintf(line1, sizeof(line1), "SD %s",
                     dashboard_sd_status_text(sd_status));
            snprintf(line2, sizeof(line2), "Use Mount or check card");
        }
        snprintf(line3, sizeof(line3), "USB %s  %s", usb_mode, usb_link);
        snprintf(line4, sizeof(line4), "ESP32 %s / Binary SPI",
                 m1_esp32_get_init_status() ? "HAL" : "Off");
    }
    else /* DASHBOARD_PAGE_SYSTEM */
    {
        snprintf(line1, sizeof(line1), "Hapax %d.%d.%d.%d-H.%d",
                 m1_device_stat.config.fw_version_major,
                 m1_device_stat.config.fw_version_minor,
                 m1_device_stat.config.fw_version_build,
                 m1_device_stat.config.fw_version_rc,
                 M1_HAPAX_REVISION);
        snprintf(line2, sizeof(line2), "Bank %d  %s",
                 (m1_device_stat.active_bank == BANK1_ACTIVE) ? 1 : 2,
                 dashboard_orientation_text());
        snprintf(line3, sizeof(line3), "Buzz %s  LED %s",
                 m1_buzzer_on ? "On" : "Off",
                 m1_led_notify_on ? "On" : "Off");
        line4[0] = '\0'; /* no fourth line on system page */
    }

    /* --- Drawing --- */
    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title bar with page badge */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    u8g2_DrawStr(&m1_u8g2, 2, 9, "Dashboard");
    {
        u8g2_uint_t bw = u8g2_GetStrWidth(&m1_u8g2, badge);
        u8g2_DrawStr(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - bw - 2, 9, badge);
    }
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    /* Content frame */
    u8g2_DrawFrame(&m1_u8g2, 2, 13, 124, 38);

    /* Content lines */
    u8g2_SetFont(&m1_u8g2, m1_menu_font());
    m1_draw_text(&m1_u8g2, 6, 23, 116, line1, TEXT_ALIGN_LEFT);
    m1_draw_text(&m1_u8g2, 6, 32, 116, line2, TEXT_ALIGN_LEFT);
    m1_draw_text(&m1_u8g2, 6, 41, 116, line3, TEXT_ALIGN_LEFT);
    if (line4[0] != '\0')
    {
        m1_draw_text(&m1_u8g2, 6, 50, 116, line4, TEXT_ALIGN_LEFT);
    }

    /* Page indicator at bottom — matches About screen pattern */
    {
        char page_ind[12];
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        snprintf(page_ind, sizeof(page_ind), "< %u/%u >",
                 (unsigned)page + 1U, (unsigned)DASHBOARD_PAGE_COUNT);
        u8g2_uint_t tw = u8g2_GetStrWidth(&m1_u8g2, page_ind);
        u8g2_DrawStr(&m1_u8g2, (M1_LCD_DISPLAY_WIDTH - tw) / 2, 62, page_ind);
    }

    m1_u8g2_nextpage();
}


void system_dashboard_run(void)
{
    dashboard_page_t page = DASHBOARD_PAGE_OVERVIEW;
    game_button_t btn;

    for (;;)
    {
        dashboard_draw_page(page);
        btn = game_poll_button(DASHBOARD_POLL_MS);

        if (btn == GAME_BTN_BACK)
        {
            return;
        }
        if (btn == GAME_BTN_LEFT || btn == GAME_BTN_UP)
        {
            page = (page == DASHBOARD_PAGE_OVERVIEW)
                ? (dashboard_page_t)(DASHBOARD_PAGE_COUNT - 1U)
                : (dashboard_page_t)(page - 1U);
        }
        else if (btn == GAME_BTN_RIGHT || btn == GAME_BTN_DOWN
                 || btn == GAME_BTN_OK)
        {
            page = (dashboard_page_t)((page + 1U) % DASHBOARD_PAGE_COUNT);
        }
    }
}
