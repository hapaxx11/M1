/* See COPYING.txt for license details. */

/*
 * app_rgb_backlight.c
 *
 * User-facing RGB backlight settings app for the M1 RGB Mod (SK6805-1515,
 * 2 LEDs on PD3).  Hardware presence is auto-detected at init; the Installed
 * row reflects the last detected state and may be toggled manually for the
 * current session.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "m1_compile_cfg.h"

#if M1_HAS_RGB_BACKLIGHT

#include "app_freertos.h"
#include "m1_tasks.h"
#include "m1_system.h"
#include "m1_display.h"
#include "m1_scene.h"
#include "m1_rgb_backlight.h"

#define RGB_APP_ITEMS 4U

enum {
    RGB_APP_ITEM_MODE = 0,
    RGB_APP_ITEM_BRIGHTNESS,
    RGB_APP_ITEM_COLOR,
    RGB_APP_ITEM_INSTALLED,
};

static const char *const s_labels[RGB_APP_ITEMS] = {
    "Mode:",
    "Brightness:",
    "Color:",
    "Installed:",
};

static const rgb_backlight_color_t s_palette[] = {
    {0xFF, 0xA0, 0x40},
    {0xFF, 0x40, 0x40},
    {0x40, 0xFF, 0x60},
    {0x40, 0x80, 0xFF},
    {0xFF, 0x40, 0xD0},
    {0xFF, 0xFF, 0xFF},
};

static const char *mode_name(rgb_backlight_mode_t mode)
{
    switch (mode)
    {
    case RGB_BACKLIGHT_MODE_STATIC:    return "Static";
    case RGB_BACKLIGHT_MODE_BREATHING: return "Breathing";
    case RGB_BACKLIGHT_MODE_RAINBOW:   return "Rainbow";
    case RGB_BACKLIGHT_MODE_OFF:
    default:                           return "Off";
    }
}

static uint8_t palette_index_for_color(rgb_backlight_color_t c)
{
    for (uint8_t i = 0; i < (uint8_t)(sizeof(s_palette) / sizeof(s_palette[0])); i++)
    {
        if (s_palette[i].r == c.r && s_palette[i].g == c.g && s_palette[i].b == c.b)
        {
            return i;
        }
    }
    return 0U;
}

static void draw_menu(uint8_t sel,
                      uint8_t scroll,
                      rgb_backlight_mode_t mode,
                      uint8_t brightness,
                      uint8_t palette_idx,
                      bool installed)
{
    char value_buf[16];
    const uint8_t item_h = m1_menu_item_h();
    const uint8_t text_ofs = item_h - 1U;
    const uint8_t visible = M1_MENU_VIS(RGB_APP_ITEMS);

    m1_u8g2_firstpage();
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);

    u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);
    m1_draw_text(&m1_u8g2, 2, 9, 120, "RGB Backlight", TEXT_ALIGN_CENTER);
    u8g2_DrawHLine(&m1_u8g2, 0, 10, M1_LCD_DISPLAY_WIDTH);

    u8g2_SetFont(&m1_u8g2, m1_menu_font());

    for (uint8_t v = 0; v < visible && (scroll + v) < RGB_APP_ITEMS; v++)
    {
        uint8_t i = scroll + v;
        uint8_t y = M1_MENU_AREA_TOP + (uint8_t)(v * item_h);

        if (i == sel)
        {
            u8g2_DrawRBox(&m1_u8g2, 0, y, M1_MENU_TEXT_W, item_h, 2);
            u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
        }

        u8g2_DrawStr(&m1_u8g2, 4, y + text_ofs, s_labels[i]);

        switch (i)
        {
        case RGB_APP_ITEM_MODE:
            snprintf(value_buf, sizeof(value_buf), "%s", mode_name(mode));
            break;
        case RGB_APP_ITEM_BRIGHTNESS:
            snprintf(value_buf, sizeof(value_buf), "%u", (unsigned)brightness);
            break;
        case RGB_APP_ITEM_COLOR:
            snprintf(value_buf, sizeof(value_buf), "#%02X%02X%02X",
                     s_palette[palette_idx].r,
                     s_palette[palette_idx].g,
                     s_palette[palette_idx].b);
            break;
        case RGB_APP_ITEM_INSTALLED:
            snprintf(value_buf, sizeof(value_buf), "%s", installed ? "Yes" : "No");
            break;
        default:
            value_buf[0] = '\0';
            break;
        }

        u8g2_DrawStr(&m1_u8g2, 74, y + text_ofs, value_buf);

        if (i == sel)
        {
            u8g2_DrawStr(&m1_u8g2, 67, y + text_ofs, "<");
            u8g2_uint_t vw = u8g2_GetStrWidth(&m1_u8g2, value_buf);
            u8g2_DrawStr(&m1_u8g2, 74 + (uint8_t)vw + 2U, y + text_ofs, ">");
        }

        u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    }

    if (RGB_APP_ITEMS > visible)
    {
        uint8_t sb_area_h = visible * item_h;
        uint8_t sb_handle_h = sb_area_h / RGB_APP_ITEMS;
        if (sb_handle_h < 6U) sb_handle_h = 6U;
        uint8_t sb_travel_h = (sb_area_h > sb_handle_h) ? (sb_area_h - sb_handle_h) : 0U;
        uint8_t sb_handle_y = M1_MENU_AREA_TOP;
        if (RGB_APP_ITEMS > 1U)
        {
            sb_handle_y += (uint8_t)((uint16_t)sb_travel_h * sel / (RGB_APP_ITEMS - 1U));
        }

        u8g2_DrawVLine(&m1_u8g2, M1_MENU_SCROLLBAR_X + (M1_MENU_SCROLLBAR_W / 2U),
                       M1_MENU_AREA_TOP, sb_area_h);
        u8g2_DrawRBox(&m1_u8g2, M1_MENU_SCROLLBAR_X, sb_handle_y,
                      M1_MENU_SCROLLBAR_W, sb_handle_h, 1);
    }

    m1_u8g2_nextpage();
}

static void apply_state(rgb_backlight_mode_t mode,
                        uint8_t brightness,
                        uint8_t palette_idx,
                        bool installed)
{
    rgb_backlight_set_installed(installed);
    rgb_backlight_set_mode(mode);
    rgb_backlight_set_brightness(brightness);
    rgb_backlight_set_color(s_palette[palette_idx].r,
                            s_palette[palette_idx].g,
                            s_palette[palette_idx].b);
    rgb_backlight_update();
}

void app_rgb_backlight_run(void)
{
    S_M1_Main_Q_t q_item;
    S_M1_Buttons_Status btn;

    uint8_t sel = 0U;
    uint8_t scroll = 0U;
    bool redraw = true;

    rgb_backlight_mode_t mode = rgb_backlight_get_mode();
    uint8_t brightness = rgb_backlight_get_brightness();
    bool installed = rgb_backlight_is_installed();
    uint8_t palette_idx = palette_index_for_color(rgb_backlight_get_color());

    apply_state(mode, brightness, palette_idx, installed);

    while (true)
    {
        if (redraw)
        {
            redraw = false;
            draw_menu(sel, scroll, mode, brightness, palette_idx, installed);
        }

        if (xQueueReceive(main_q_hdl, &q_item, portMAX_DELAY) != pdTRUE) continue;
        if (q_item.q_evt_type != Q_EVENT_KEYPAD) continue;
        if (xQueueReceive(button_events_q_hdl, &btn, 0) != pdTRUE) continue;

        if (btn.event[BUTTON_BACK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            xQueueReset(main_q_hdl);
            return;
        }

        if (btn.event[BUTTON_UP_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel = (sel == 0U) ? (RGB_APP_ITEMS - 1U) : (uint8_t)(sel - 1U);
            if (sel < scroll) scroll = sel;
            if (sel == (RGB_APP_ITEMS - 1U))
            {
                uint8_t vis = M1_MENU_VIS(RGB_APP_ITEMS);
                scroll = (RGB_APP_ITEMS > vis) ? (RGB_APP_ITEMS - vis) : 0U;
            }
            redraw = true;
            continue;
        }

        if (btn.event[BUTTON_DOWN_KP_ID] == BUTTON_EVENT_CLICK)
        {
            sel = (uint8_t)((sel + 1U) % RGB_APP_ITEMS);
            uint8_t vis = M1_MENU_VIS(RGB_APP_ITEMS);
            if (sel >= (uint8_t)(scroll + vis))
            {
                scroll = (uint8_t)(sel - vis + 1U);
            }
            if (sel == 0U) scroll = 0U;
            redraw = true;
            continue;
        }

        if (btn.event[BUTTON_LEFT_KP_ID] == BUTTON_EVENT_CLICK ||
            btn.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK)
        {
            bool inc = (btn.event[BUTTON_RIGHT_KP_ID] == BUTTON_EVENT_CLICK);

            if (sel == RGB_APP_ITEM_MODE)
            {
                if (inc)
                {
                    mode = (mode >= (RGB_BACKLIGHT_MODE_COUNT - 1))
                           ? RGB_BACKLIGHT_MODE_OFF
                           : (rgb_backlight_mode_t)(mode + 1);
                }
                else
                {
                    mode = (mode == RGB_BACKLIGHT_MODE_OFF)
                           ? (rgb_backlight_mode_t)(RGB_BACKLIGHT_MODE_COUNT - 1)
                           : (rgb_backlight_mode_t)(mode - 1);
                }
            }
            else if (sel == RGB_APP_ITEM_BRIGHTNESS)
            {
                if (inc)
                {
                    brightness = (brightness > 245U) ? 255U : (uint8_t)(brightness + 10U);
                }
                else
                {
                    brightness = (brightness < 10U) ? 0U : (uint8_t)(brightness - 10U);
                }
            }
            else if (sel == RGB_APP_ITEM_COLOR)
            {
                uint8_t palette_count = (uint8_t)(sizeof(s_palette) / sizeof(s_palette[0]));
                if (inc)
                {
                    palette_idx = (uint8_t)((palette_idx + 1U) % palette_count);
                }
                else
                {
                    palette_idx = (palette_idx == 0U) ? (uint8_t)(palette_count - 1U)
                                                      : (uint8_t)(palette_idx - 1U);
                }
            }
            else if (sel == RGB_APP_ITEM_INSTALLED)
            {
                installed = !installed;
                if (!installed)
                {
                    mode = RGB_BACKLIGHT_MODE_OFF;
                }
            }

            apply_state(mode, brightness, palette_idx, installed);
            redraw = true;
            continue;
        }

        if (btn.event[BUTTON_OK_KP_ID] == BUTTON_EVENT_CLICK)
        {
            if (sel == RGB_APP_ITEM_INSTALLED)
            {
                installed = !installed;
                if (!installed) mode = RGB_BACKLIGHT_MODE_OFF;
                apply_state(mode, brightness, palette_idx, installed);
                redraw = true;
            }
        }
    }
}

#endif /* M1_HAS_RGB_BACKLIGHT */
