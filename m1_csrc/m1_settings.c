/* See COPYING.txt for license details. */

/*
 *
 *  m1_settings.c
 *
 *  M1 RFID functions
 *
 * M1 Project
 *
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_settings.h"
#include "m1_buzzer.h"
#include "m1_lcd.h"
#include "m1_lp5814.h"
#include "m1_display.h"
#include "ff.h"
#include "m1_log_debug.h"
#include "m1_fw_update_bl.h"
#include "m1_system.h"
#include "m1_file_util.h"
#include "m1_scene.h"
#include "m1_virtual_kb.h"
#include "m1_clock_util.h"

/*************************** D E F I N E S ************************************/

#define SETTINGS_TAG              "SETT"
#define SETTINGS_FILE_PATH        "0:/System/settings.cfg"
#define SETTINGS_FILE_MAX_SIZE    512

/* (ABOUT_BOX defines and SETTING_ABOUT_CHOICES_MAX removed — About screen
 * now uses settings_about_draw_page() with full-screen redraw per page.) */

/* LCD & Notifications menu items */
#define LCD_SETTINGS_ITEMS   10
#define LCD_SET_BRIGHTNESS   0
#define LCD_SET_BUZZER       1
#define LCD_SET_LED          2
#define LCD_SET_LED_COLOR    3
#define LCD_SET_LOWBATT_CLR  4
#define LCD_SET_ORIENT       5
#define LCD_SET_SLEEP        6
#define LCD_SET_TEXT_SIZE    7
#define LCD_SET_DARK_MODE    8
#define LCD_SET_TZ_OFFSET    9

//************************** S T R U C T U R E S *******************************\n
/***************************** V A R I A B L E S ******************************/

static const uint8_t s_brightness_values[] = { 0, 64, 128, 192, 255 };
static const char *s_brightness_text[] = { "Off", "Low", "Med", "High", "Max" };
static const char *s_orient_text[] = { "Normal", "Southpaw", "Remote" };
static const char *s_sleep_text[] = { "30s", "1 min", "5 min", "10 min", "15 min", "Never" };
static const char *s_text_size_text[] = { "Small", "Medium", "Large" };

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

void menu_settings_init(void);
void menu_settings_exit(void);
void settings_about(void);
void settings_save_to_sd(void);

/* Sub-GHz save format accessors (defined in m1_sub_ghz.c) */
extern uint8_t subghz_get_save_fmt_ext(void);
extern void    subghz_set_save_fmt_ext(uint8_t fmt);

/* Sub-GHz radio config accessors (defined in m1_sub_ghz.c) */
extern uint8_t subghz_get_freq_idx_ext(void);
extern void    subghz_set_freq_idx_ext(uint8_t idx);
extern uint8_t subghz_get_mod_idx_ext(void);
extern void    subghz_set_mod_idx_ext(uint8_t idx);
extern bool    subghz_get_hopping_ext(void);
extern void    subghz_set_hopping_ext(bool v);
extern bool    subghz_get_sound_ext(void);
extern void    subghz_set_sound_ext(bool v);
extern uint8_t subghz_get_tx_power_idx_ext(void);
extern void    subghz_set_tx_power_idx_ext(uint8_t idx);
extern int8_t  subghz_get_rssi_threshold_ext(void);
extern void    subghz_set_rssi_threshold_ext(int8_t v);
extern uint32_t subghz_get_user_custom_freq_ext(void);
extern void     subghz_set_user_custom_freq_ext(uint32_t hz);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/


/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void menu_settings_init(void)
{
	;
} // void menu_settings_init(void)


/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void menu_settings_exit(void)
{
	;
} // void menu_settings_exit(void)


/*============================================================================*/
/**
  * @brief  Apply screen orientation and sync m1_southpaw_mode
  */
/*============================================================================*/
void settings_apply_orientation(uint8_t orient)
{
    m1_screen_orientation = orient;
    m1_southpaw_mode = (orient == M1_ORIENT_SOUTHPAW) ? 1 : 0;

    if (orient == M1_ORIENT_SOUTHPAW)
        m1_lcd_set_rotation(U8G2_R0);
    else if (orient == M1_ORIENT_REMOTE)
        m1_lcd_set_rotation(U8G2_R1);
    else
        m1_lcd_set_rotation(U8G2_R2);
}

/*============================================================================*/
/**
  * @brief  LCD & Notifications settings — config-style menu matching the
  *         SubGhz Config scene layout: no bottom bar, < > arrows on the
  *         selected item to indicate L/R cycling, 8px rows from y=12.
  */
/*============================================================================*/

/* Layout constants — aligned with SubGhz Config scene */
#define LCD_CFG_AREA_TOP     12   /* Y below title + separator line      */
#define LCD_CFG_TEXT_W      124   /* Highlight / text area width           */
#define LCD_CFG_SCROLLBAR_X 125   /* Scrollbar left edge (3px wide)       */
#define LCD_CFG_SCROLLBAR_W   3   /* Scrollbar track width                */

static const char *const lcd_cfg_labels[LCD_SETTINGS_ITEMS] = {
    "Brightness:",
    "Buzzer:",
    "LED Notify:",
    "LED Color:",
    "LowBatt Clr:",
    "Orientation:",
    "Sleep After:",
    "Text Size:",
    "Dark Mode:",
    "Local TZ:",
};

static char led_color_buf[8]; /* "#RRGGBB" + NUL */
static char lowbatt_color_buf[8]; /* "#RRGGBB" + NUL */
static char tz_label_buf[8]; /* "UTC+14" + NUL */

static const char *lcd_cfg_get_value(uint8_t item)
{
    switch (item)
    {
    case LCD_SET_BRIGHTNESS: return s_brightness_text[m1_brightness_level];
    case LCD_SET_BUZZER:     return m1_buzzer_on ? "On" : "Off";
    case LCD_SET_LED:        return m1_led_notify_on ? "On" : "Off";
    case LCD_SET_LED_COLOR:
        snprintf(led_color_buf, sizeof(led_color_buf), "#%02X%02X%02X",
                 m1_led_color_r, m1_led_color_g, m1_led_color_b);
        return led_color_buf;
    case LCD_SET_LOWBATT_CLR:
        snprintf(lowbatt_color_buf, sizeof(lowbatt_color_buf), "#%02X%02X%02X",
                 m1_led_lowbatt_r, m1_led_lowbatt_g, m1_led_lowbatt_b);
        return lowbatt_color_buf;
    case LCD_SET_ORIENT:     return s_orient_text[m1_screen_orientation];
    case LCD_SET_SLEEP:      return s_sleep_text[m1_sleep_timeout_idx];
    case LCD_SET_TEXT_SIZE:
        switch (m1_menu_style)
        {
        case 1:  return s_text_size_text[1];
        case 2:  return s_text_size_text[2];
        case 0:
        default: return s_text_size_text[0];
        }
    case LCD_SET_DARK_MODE:  return m1_dark_mode ? "On" : "Off";
    case LCD_SET_TZ_OFFSET:
        clock_tz_label(m1_clock_tz_offset, tz_label_buf, sizeof(tz_label_buf));
        return tz_label_buf;
    default:                 return "";
    }
}

/*============================================================================*/
/**
  * @brief  Parse a hex color string "#RRGGBB" or "RRGGBB" into r, g, b.
  * @retval 1 on success, 0 on invalid input.
  */
/*============================================================================*/
uint8_t settings_parse_hex_color(const char *str, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (str == NULL || r == NULL || g == NULL || b == NULL) return 0;
    if (str[0] == '#') str++;
    if (strlen(str) < 6) return 0;

    uint32_t val = 0;
    for (uint8_t i = 0; i < 6; i++)
    {
        char c = str[i];
        uint8_t nib;
        if (c >= '0' && c <= '9')      nib = c - '0';
        else if (c >= 'A' && c <= 'F')  nib = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f')  nib = c - 'a' + 10;
        else return 0;
        val = (val << 4) | nib;
    }
    *r = (val >> 16) & 0xFF;
    *g = (val >> 8) & 0xFF;
    *b = val & 0xFF;
    return 1;
}


void settings_lcd_and_notifications(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;

    uint8_t sel = 0;
    uint8_t scroll = 0;
    uint8_t needs_redraw = 1;

    while (1)
    {
        if (needs_redraw)
        {
            needs_redraw = 0;

            const uint8_t item_h   = m1_menu_item_h();
            const uint8_t text_ofs = item_h - 1;
            const uint8_t visible  = M1_MENU_VIS(LCD_SETTINGS_ITEMS);

            m1_u8g2_firstpage();
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

            /* Title */
            u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
            m1_draw_text(&m1_u8g2, 2, 9, 120, "LCD & Notifications", TEXT_ALIGN_CENTER);

            /* Separator line */
            u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

            /* Config items */
            u8g2_SetFont(&m1_u8g2, m1_menu_font());

            /* Compute value x-position so the longest label never abuts
             * its value — in medium/large fonts labels are wider, but
             * keep it inside the text area so long values and arrows
             * still fit before the scrollbar. */
            u8g2_uint_t max_lw = 0;
            for (uint8_t j = 0; j < LCD_SETTINGS_ITEMS; j++)
            {
                u8g2_uint_t w = u8g2_GetStrWidth(&m1_u8g2, lcd_cfg_labels[j]);
                if (w > max_lw) max_lw = w;
            }
            u8g2_uint_t val_x_pref = 4 + max_lw + 4;   /* label start + max width + gap */
            u8g2_uint_t val_x_max  = LCD_CFG_TEXT_W - 18; /* keep value/arrows within text area */
            u8g2_uint_t val_x      = (val_x_pref < val_x_max) ? val_x_pref : val_x_max;
            u8g2_uint_t arrow_x    = val_x - 6;         /* "<" one glyph-cell left       */

            for (uint8_t v = 0; v < visible && (scroll + v) < LCD_SETTINGS_ITEMS; v++)
            {
                uint8_t i = scroll + v;
                uint8_t y = LCD_CFG_AREA_TOP + v * item_h;
                const char *val = lcd_cfg_get_value(i);

                if (i == sel)
                {
                    /* Highlight selected row */
                    u8g2_DrawBox(&m1_u8g2, 0, y, LCD_CFG_TEXT_W, item_h);
                    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
                }

                /* Label on left */
                u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, lcd_cfg_labels[i]);

                /* Value on right with < > arrows for selected item */
                if (i == sel)
                {
                    u8g2_DrawStr(&m1_u8g2, arrow_x, y + text_ofs, "<");
                    u8g2_DrawStr(&m1_u8g2, val_x, y + text_ofs, val);
                    u8g2_uint_t vw = u8g2_GetStrWidth(&m1_u8g2, val);
                    u8g2_DrawStr(&m1_u8g2, val_x + vw + 2, y + text_ofs, ">");
                }
                else
                {
                    u8g2_DrawStr(&m1_u8g2, val_x, y + text_ofs, val);
                }

                u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
            }

            /* Scrollbar — only when items exceed visible area */
            if (LCD_SETTINGS_ITEMS > visible)
            {
                uint8_t sb_area_h   = visible * item_h;
                uint8_t sb_handle_h = sb_area_h / LCD_SETTINGS_ITEMS;
                if (sb_handle_h < 3) sb_handle_h = 3;
                uint8_t sb_handle_y = LCD_CFG_AREA_TOP +
                    (uint8_t)((uint16_t)sb_area_h * sel / LCD_SETTINGS_ITEMS);

                u8g2_DrawFrame(&m1_u8g2, LCD_CFG_SCROLLBAR_X, LCD_CFG_AREA_TOP,
                               LCD_CFG_SCROLLBAR_W, sb_area_h);
                u8g2_DrawBox(&m1_u8g2, LCD_CFG_SCROLLBAR_X, sb_handle_y,
                             LCD_CFG_SCROLLBAR_W, sb_handle_h);
            }

            m1_u8g2_nextpage();
        }

        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE) continue;
        if (q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

        ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
        if (ret != pdTRUE) continue;

        /* Back — save and exit */
        if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            settings_save_to_sd();
            xQueueReset(main_q_hdl);
            break;
        }

        /* LED Color / Low Batt Color editor — allow OK/LEFT/RIGHT so UI cues match behavior */
        if ((sel == LCD_SET_LED_COLOR || sel == LCD_SET_LOWBATT_CLR) &&
            (this_button_status.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK ||
             this_button_status.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK ||
             this_button_status.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK))
        {
            uint8_t *cr, *cg, *cb;
            const char *prompt;
            if (sel == LCD_SET_LED_COLOR)
            {
                cr = &m1_led_color_r; cg = &m1_led_color_g; cb = &m1_led_color_b;
                prompt = "LED Color (hex)";
            }
            else
            {
                cr = &m1_led_lowbatt_r; cg = &m1_led_lowbatt_g; cb = &m1_led_lowbatt_b;
                prompt = "LowBatt Clr (hex)";
            }
            char cur_hex[8];
            char new_hex[8];
            snprintf(cur_hex, sizeof(cur_hex), "%02X%02X%02X", *cr, *cg, *cb);
            if (m1_vkb_get_text(prompt, cur_hex, new_hex, sizeof(new_hex)))
            {
                uint8_t r, g, b;
                if (settings_parse_hex_color(new_hex, &r, &g, &b))
                {
                    *cr = r; *cg = g; *cb = b;
                }
            }
            this_button_status.event[BUTTON_OK_KP_ID] = BUTTON_EVENT_IDLE;
            this_button_status.event[BUTTON_LEFT_KP_ID] = BUTTON_EVENT_IDLE;
            this_button_status.event[BUTTON_RIGHT_KP_ID] = BUTTON_EVENT_IDLE;
            needs_redraw = 1;
            continue;
        }

        /* Up/Down — navigate with scroll */
        if (this_button_status.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel = (sel == 0) ? (LCD_SETTINGS_ITEMS - 1) : (sel - 1);
            if (sel < scroll)
                scroll = sel;
            if (sel == LCD_SETTINGS_ITEMS - 1)
            {
                uint8_t vis = M1_MENU_VIS(LCD_SETTINGS_ITEMS);
                scroll = (LCD_SETTINGS_ITEMS > vis) ? LCD_SETTINGS_ITEMS - vis : 0;
            }
            needs_redraw = 1;
        }
        if (this_button_status.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel = (sel + 1) % LCD_SETTINGS_ITEMS;
            {
                uint8_t vis = M1_MENU_VIS(LCD_SETTINGS_ITEMS);
                if (sel >= scroll + vis)
                    scroll = sel - vis + 1;
            }
            if (sel == 0)
                scroll = 0;
            needs_redraw = 1;
        }

        /* Left — decrement selected setting */
        if (this_button_status.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (sel == LCD_SET_BRIGHTNESS)
            {
                m1_brightness_level = (m1_brightness_level == 0) ? 4 : (m1_brightness_level - 1);
                lp5814_backlight_on(s_brightness_values[m1_brightness_level]);
            }
            else if (sel == LCD_SET_BUZZER)
                m1_buzzer_on = !m1_buzzer_on;
            else if (sel == LCD_SET_LED)
                m1_led_notify_on = !m1_led_notify_on;
            else if (sel == LCD_SET_ORIENT)
                settings_apply_orientation((m1_screen_orientation == 0) ? 2 : (m1_screen_orientation - 1));
            else if (sel == LCD_SET_SLEEP)
                m1_sleep_timeout_idx = (m1_sleep_timeout_idx == 0) ? 5 : (m1_sleep_timeout_idx - 1);
            else if (sel == LCD_SET_TEXT_SIZE)
            {
                uint8_t menu_style = (m1_menu_style <= 2) ? m1_menu_style : 0;
                m1_menu_style = (menu_style == 0) ? 2 : (menu_style - 1);
                uint8_t vis = M1_MENU_VIS(LCD_SETTINGS_ITEMS);
                if (sel >= scroll + vis)
                    scroll = sel - vis + 1;
                else if (LCD_SETTINGS_ITEMS <= vis)
                    scroll = 0;
            }
            else if (sel == LCD_SET_DARK_MODE)
                m1_lcd_set_dark_mode(!m1_dark_mode);
            else if (sel == LCD_SET_TZ_OFFSET)
                m1_clock_tz_offset = (m1_clock_tz_offset <= -12) ? 14 : (int8_t)(m1_clock_tz_offset - 1);
            needs_redraw = 1;
        }

        /* Right — increment selected setting */
        if (this_button_status.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (sel == LCD_SET_BRIGHTNESS)
            {
                m1_brightness_level = (m1_brightness_level >= 4) ? 0 : (m1_brightness_level + 1);
                lp5814_backlight_on(s_brightness_values[m1_brightness_level]);
            }
            else if (sel == LCD_SET_BUZZER)
            {
                m1_buzzer_on = !m1_buzzer_on;
                if (m1_buzzer_on) m1_buzzer_notification();
            }
            else if (sel == LCD_SET_LED)
                m1_led_notify_on = !m1_led_notify_on;
            else if (sel == LCD_SET_ORIENT)
                settings_apply_orientation((m1_screen_orientation + 1) % 3);
            else if (sel == LCD_SET_SLEEP)
                m1_sleep_timeout_idx = (m1_sleep_timeout_idx >= 5) ? 0 : (m1_sleep_timeout_idx + 1);
            else if (sel == LCD_SET_TEXT_SIZE)
            {
                m1_menu_style = (m1_menu_style >= 2) ? 0 : (m1_menu_style + 1);
                uint8_t vis = M1_MENU_VIS(LCD_SETTINGS_ITEMS);
                if (sel >= scroll + vis)
                    scroll = sel - vis + 1;
                else if (LCD_SETTINGS_ITEMS <= vis)
                    scroll = 0;
            }
            else if (sel == LCD_SET_DARK_MODE)
                m1_lcd_set_dark_mode(!m1_dark_mode);
            else if (sel == LCD_SET_TZ_OFFSET)
                m1_clock_tz_offset = (m1_clock_tz_offset >= 14) ? -12 : (int8_t)(m1_clock_tz_offset + 1);
            needs_redraw = 1;
        }
    }
}


/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void settings_buzzer(void)
{
	//buzzer_demo_play();
} // void settings_sound(void)


/*============================================================================*/
/**
  * @brief
  * @param
  * @retval
  */
/*============================================================================*/
void settings_power(void)
{
	;
} // void settings_power(void)


/*============================================================================*/
/**
  * @brief  About screen — paginated info view without bottom bar.
  *         L/R navigation is intuitive; a small page indicator replaces
  *         the old Prev/Next bar to reclaim screen space.
  */
/*============================================================================*/

#define ABOUT_PAGES  3   /* FW info, Company info, M1 image */

static void settings_about_draw_page(uint8_t choice)
{
    char buf[48];

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    /* Title */
    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "About", TEXT_ALIGN_CENTER);

    /* Separator line */
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    /* Page content — starts at y=20, full height available */
    switch (choice)
    {
    case 0: /* FW info */
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_B);
        u8g2_DrawStr(&m1_u8g2, 4, 22, "M1 by Hapax");
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        snprintf(buf, sizeof(buf), "%d.%d.%d.%d-Hapax.%d",
                 m1_device_stat.config.fw_version_major,
                 m1_device_stat.config.fw_version_minor,
                 m1_device_stat.config.fw_version_build,
                 m1_device_stat.config.fw_version_rc,
                 M1_HAPAX_REVISION);
        u8g2_DrawStr(&m1_u8g2, 4, 34, buf);
        snprintf(buf, sizeof(buf), "Active bank: %d",
                 (m1_device_stat.active_bank == BANK1_ACTIVE) ? 1 : 2);
        u8g2_DrawStr(&m1_u8g2, 4, 46, buf);
        break;

    case 1: /* Company info */
        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 4, 22, "MonstaTek Inc.");
        u8g2_DrawStr(&m1_u8g2, 4, 34, "San Jose, CA, USA");
        break;

    default: /* M1 device image */
        u8g2_DrawXBMP(&m1_u8g2, 23, 14, 82, 36, m1_device_82x36);
        break;
    }

    /* Page indicator "< N/M >" at bottom right — no inverted bar */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    snprintf(buf, sizeof(buf), "< %d/%d >", choice + 1, ABOUT_PAGES);
    uint8_t tw = u8g2_GetStrWidth(&m1_u8g2, buf);
    u8g2_DrawStr(&m1_u8g2, (M1_LCD_DISPLAY_WIDTH - tw) / 2, 62, buf);

    m1_u8g2_nextpage();
}

void settings_about(void)
{
    S_M1_Buttons_Status this_button_status;
    S_M1_Main_Q_t q_item;
    BaseType_t ret;
    uint8_t choice = 0;

    settings_about_draw_page(choice);

    while (1)
    {
        ret = xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY);
        if (ret != pdTRUE) continue;
        if (q_item.q_evt_type != Q_EVENT_KEYPAD) continue;

        ret = xQueueReceive(button_events_q_hdl, &this_button_status, 0);
        if (ret != pdTRUE) continue;

        if (this_button_status.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            xQueueReset(main_q_hdl);
            break;
        }
        else if (this_button_status.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK)
        {
            choice = (choice == 0) ? (ABOUT_PAGES - 1) : (choice - 1);
            settings_about_draw_page(choice);
        }
        else if (this_button_status.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
        {
            choice = (choice + 1) % ABOUT_PAGES;
            settings_about_draw_page(choice);
        }
    }
}


/*============================================================================*/
/**
  * @brief  Save user settings to SD card (0:/System/settings.cfg)
  */
/*============================================================================*/
void settings_save_to_sd(void)
{
    FIL fp;
    FRESULT fres;
    UINT bw;
    char buf[64];

    /* Ensure System directory exists */
    f_mkdir("0:/System");

    fres = f_open(&fp, SETTINGS_FILE_PATH, FA_WRITE | FA_CREATE_ALWAYS);
    if (fres != FR_OK)
    {
        M1_LOG_W(SETTINGS_TAG, "Save failed (err=%d)\r\n", fres);
        return;
    }

    snprintf(buf, sizeof(buf), "brightness=%d\n", m1_brightness_level);
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "buzzer=%d\n", m1_buzzer_on);
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "led_notify=%d\n", m1_led_notify_on);
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "led_color=%02X%02X%02X\n",
             m1_led_color_r, m1_led_color_g, m1_led_color_b);
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "led_lowbatt=%02X%02X%02X\n",
             m1_led_lowbatt_r, m1_led_lowbatt_g, m1_led_lowbatt_b);
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "orientation=%d\n", m1_screen_orientation);
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "sleep_timeout=%d\n", m1_sleep_timeout_idx);
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "menu_style=%d\n", m1_menu_style);
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "dark_mode=%d\n", m1_dark_mode);
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "ism_region=%d\n", m1_device_stat.config.ism_band_region);
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "subghz_save_fmt=%d\n", subghz_get_save_fmt_ext());
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "subghz_freq_idx=%d\n", subghz_get_freq_idx_ext());
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "subghz_mod_idx=%d\n", subghz_get_mod_idx_ext());
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "subghz_hopping=%d\n", subghz_get_hopping_ext() ? 1 : 0);
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "subghz_sound=%d\n", subghz_get_sound_ext() ? 1 : 0);
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "subghz_tx_power=%d\n", subghz_get_tx_power_idx_ext());
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "subghz_rssi_threshold=%d\n", (int)subghz_get_rssi_threshold_ext());
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "subghz_custom_freq=%lu\n", (unsigned long)subghz_get_user_custom_freq_ext());
    f_write(&fp, buf, strlen(buf), &bw);

    snprintf(buf, sizeof(buf), "clock_tz_offset=%d\n", (int)m1_clock_tz_offset);
    f_write(&fp, buf, strlen(buf), &bw);

#ifdef M1_APP_BADBT_ENABLE
    snprintf(buf, sizeof(buf), "badbt_name=%s\n", m1_badbt_name);
    f_write(&fp, buf, strlen(buf), &bw);
#endif

    f_close(&fp);
}


/*============================================================================*/
/**
  * @brief  Load user settings from SD card (0:/System/settings.cfg)
  *         Sets m1_southpaw_mode and applies display rotation.
  */
/*============================================================================*/
void settings_load_from_sd(void)
{
    FIL fp;
    FRESULT fres;
    UINT br;
    char buf[SETTINGS_FILE_MAX_SIZE];
    char *p;
    int val;

    fres = f_open(&fp, SETTINGS_FILE_PATH, FA_READ);
    if (fres != FR_OK)
        goto apply;  /* No settings file yet — apply defaults */

    fres = f_read(&fp, buf, sizeof(buf) - 1, &br);
    f_close(&fp);

    if (fres != FR_OK || br == 0)
        goto apply;

    buf[br] = '\0';

    /* Parse "brightness=X" */
    p = strstr(buf, "brightness=");
    if (p != NULL)
    {
        val = (int)(*(p + 11) - '0');
        if (val >= 0 && val <= 4)
            m1_brightness_level = (uint8_t)val;
    }

    /* Parse "buzzer=X" */
    p = strstr(buf, "buzzer=");
    if (p != NULL)
    {
        val = (int)(*(p + 7) - '0');
        if (val == 0 || val == 1)
            m1_buzzer_on = (uint8_t)val;
    }

    /* Parse "led_notify=X" */
    p = strstr(buf, "led_notify=");
    if (p != NULL)
    {
        val = (int)(*(p + 11) - '0');
        if (val == 0 || val == 1)
            m1_led_notify_on = (uint8_t)val;
    }

    /* Parse "led_color=RRGGBB" */
    p = strstr(buf, "led_color=");
    if (p != NULL)
    {
        uint8_t r, g, b;
        if (settings_parse_hex_color(p + 10, &r, &g, &b))
        {
            m1_led_color_r = r;
            m1_led_color_g = g;
            m1_led_color_b = b;
        }
    }

    /* Parse "led_lowbatt=RRGGBB" */
    p = strstr(buf, "led_lowbatt=");
    if (p != NULL)
    {
        uint8_t r, g, b;
        if (settings_parse_hex_color(p + 12, &r, &g, &b))
        {
            m1_led_lowbatt_r = r;
            m1_led_lowbatt_g = g;
            m1_led_lowbatt_b = b;
        }
    }

    /* Parse "orientation=X" */
    p = strstr(buf, "orientation=");
    if (p != NULL)
    {
        val = (int)(*(p + 12) - '0');
        if (val >= 0 && val <= 2)
            m1_screen_orientation = (uint8_t)val;
    }

    /* Parse "sleep_timeout=X" */
    p = strstr(buf, "sleep_timeout=");
    if (p != NULL)
    {
        val = (int)(*(p + 14) - '0');
        if (val >= 0 && val <= 5)
            m1_sleep_timeout_idx = (uint8_t)val;
    }

    /* Parse "menu_style=X" */
    p = strstr(buf, "menu_style=");
    if (p != NULL)
    {
        val = (int)(*(p + 11) - '0');
        if (val >= 0 && val <= 2)
            m1_menu_style = (uint8_t)val;
    }

    /* Parse "dark_mode=X" */
    p = strstr(buf, "dark_mode=");
    if (p != NULL)
    {
        val = (int)(*(p + 10) - '0');
        if (val == 0 || val == 1)
            m1_dark_mode = (uint8_t)val;
    }

    /* Parse "ism_region=X" */
    p = strstr(buf, "ism_region=");
    if (p != NULL)
    {
        val = (int)(*(p + 11) - '0');
        if (val >= 0 && val <= 3)
            m1_device_stat.config.ism_band_region = (uint8_t)val;
    }

    /* Parse "subghz_save_fmt=X" */
    p = strstr(buf, "subghz_save_fmt=");
    if (p != NULL)
    {
        val = (int)(*(p + 16) - '0');
        if (val == 0 || val == 1)
            subghz_set_save_fmt_ext((uint8_t)val);
    }

    /* Parse "subghz_freq_idx=NN" (0–63, including Custom sentinel) */
    p = strstr(buf, "subghz_freq_idx=");
    if (p != NULL)
    {
        char *end;
        long lval = strtol(p + 16, &end, 10);
        if (end != p + 16 && (*end == '\n' || *end == '\r' || *end == '\0' || *end == ' '))
            subghz_set_freq_idx_ext((uint8_t)lval);  /* bounds enforced by setter */
    }

    /* Parse "subghz_mod_idx=X" (0–3) */
    p = strstr(buf, "subghz_mod_idx=");
    if (p != NULL)
    {
        val = (int)(*(p + 15) - '0');
        if (val >= 0 && val <= 3)
            subghz_set_mod_idx_ext((uint8_t)val);
    }

    /* Parse "subghz_hopping=X" (0 or 1) */
    p = strstr(buf, "subghz_hopping=");
    if (p != NULL)
    {
        val = (int)(*(p + 15) - '0');
        if (val == 0 || val == 1)
            subghz_set_hopping_ext(val == 1);
    }

    /* Parse "subghz_sound=X" (0 or 1) */
    p = strstr(buf, "subghz_sound=");
    if (p != NULL)
    {
        val = (int)(*(p + 13) - '0');
        if (val == 0 || val == 1)
            subghz_set_sound_ext(val == 1);
    }

    /* Parse "subghz_tx_power=X" (0–3) */
    p = strstr(buf, "subghz_tx_power=");
    if (p != NULL)
    {
        val = (int)(*(p + 16) - '0');
        if (val >= 0 && val <= 3)
            subghz_set_tx_power_idx_ext((uint8_t)val);
    }

    /* Parse "subghz_rssi_threshold=N" (-50 to -100) */
    p = strstr(buf, "subghz_rssi_threshold=");
    if (p != NULL)
    {
        char *end;
        long lval = strtol(p + 22, &end, 10);
        if (end != p + 22 &&
            (*end == '\n' || *end == '\r' || *end == '\0') &&
            lval >= -100 && lval <= -50)
        {
            subghz_set_rssi_threshold_ext((int8_t)lval);
        }
    }

    /* Parse "subghz_custom_freq=N" (Hz, 300000000–930000000) */
    p = strstr(buf, "subghz_custom_freq=");
    if (p != NULL)
    {
        char *end;
        unsigned long uval = strtoul(p + 19, &end, 10);
        if (end != p + 19 &&
            (*end == '\n' || *end == '\r' || *end == '\0') &&
            uval >= 300000000UL && uval <= 930000000UL)
        {
            subghz_set_user_custom_freq_ext((uint32_t)uval);
        }
    }

    /* Parse "clock_tz_offset=N" (-12..+14) */
    p = strstr(buf, "clock_tz_offset=");
    if (p != NULL)
    {
        char *end;
        long lval = strtol(p + 16, &end, 10);
        if (end != p + 16 &&
            (*end == '\n' || *end == '\0') &&
            lval >= -12 && lval <= 14)
        {
            m1_clock_tz_offset = (int8_t)lval;
        }
    }

    /* Legacy: migrate "southpaw=1" if no orientation key found */
    if (strstr(buf, "orientation=") == NULL)
    {
        p = strstr(buf, "southpaw=");
        if (p != NULL && *(p + 9) == '1')
            m1_screen_orientation = M1_ORIENT_SOUTHPAW;
    }

#ifdef M1_APP_BADBT_ENABLE
    /* Parse "badbt_name=XYZ" */
    p = strstr(buf, "badbt_name=");
    if (p != NULL)
    {
        p += 11;  /* skip "badbt_name=" */
        char *end = strchr(p, '\n');
        if (!end) end = p + strlen(p);
        uint8_t len = end - p;
        if (len > BADBT_NAME_MAX_LEN) len = BADBT_NAME_MAX_LEN;
        if (len > 0)
        {
            memcpy(m1_badbt_name, p, len);
            m1_badbt_name[len] = '\0';
        }
    }
#endif

apply:
    /* Apply brightness */
    lp5814_backlight_on(s_brightness_values[m1_brightness_level]);

    /* Apply dark mode — sends the ST7567 hardware inverse display command
     * so the LCD matches the persisted m1_dark_mode value on boot. */
    m1_lcd_set_dark_mode(m1_dark_mode);

    /* Apply orientation */
    settings_apply_orientation(m1_screen_orientation);
}


/*============================================================================*/
/**
  * @brief  Ensure all module SD card directories exist
  *         Call once after SD card is mounted (e.g. after settings_load_from_sd)
  */
/*============================================================================*/
void settings_ensure_sd_folders(void)
{
    static const char * const dirs[] = {
        "0:/NFC",
        "0:/RFID",
        "0:/SUBGHZ",
        "0:/IR",
        "0:/BadUSB",
        "0:/BT",
        "0:/System",
        "0:/apps"
    };

    for (uint8_t i = 0; i < sizeof(dirs) / sizeof(dirs[0]); i++)
    {
        fs_directory_ensure(dirs[i]);
    }
}