/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_button_bar.c
 * @brief  Sub-GHz-specific status/RSSI bars, plus a backward-compatible
 *         forwarding wrapper for the shared bottom-bar renderer.
 *
 * The generic 3-column bottom bar renderer moved to m1_button_bar.c/h (Phase 3
 * of the UI/UX consistency overhaul) since it has no Sub-GHz-specific logic
 * and is used by non-Sub-GHz modules too.  `subghz_button_bar_draw()` is kept
 * here as a thin forwarding wrapper so existing Sub-GHz call sites do not need
 * to change.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_button_bar.h"
#include "m1_subghz_button_bar.h"

/*============================================================================*/
/* Bottom bar (forwards to the shared m1_button_bar module)                  */
/*============================================================================*/

void subghz_button_bar_draw(
    const uint8_t *left_icon,   const char *left_text,
    const uint8_t *center_icon, const char *center_text,
    const uint8_t *right_icon,  const char *right_text)
{
    m1_button_bar_draw(left_icon,   left_text,
                       center_icon, center_text,
                       right_icon,  right_text);
}

/*============================================================================*/
/* Layout constants (status bar / RSSI bar only)                             */
/*============================================================================*/

/* Status bar */
#define STATUS_BAR_H   10   /* Height of top status bar */
#define STATUS_BAR_Y    0   /* Y position */

/* RSSI bar */
#define RSSI_BAR_Y     11   /* Just below status bar */
#define RSSI_BAR_H      3   /* Height of RSSI bar */
#define RSSI_MIN     -110   /* dBm mapped to 0 pixels */
#define RSSI_MAX      -30   /* dBm mapped to full width (128px) */

/*============================================================================*/
/* Status bar                                                                 */
/*============================================================================*/

void subghz_status_bar_draw(
    const char *freq_text,
    const char *mod_text,
    const char *state_text,
    bool hopping)
{
    /* Black text on white background — matches Momentum/Flipper header style */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

    /* Left side: frequency or "Hopping" */
    if (hopping)
        u8g2_DrawStr(&m1_u8g2, 2, 8, "HOP");
    else if (freq_text)
        u8g2_DrawStr(&m1_u8g2, 2, 8, freq_text);

    /* Center: modulation */
    if (mod_text)
    {
        /* Centre the modulation text — use u8g2_uint_t to avoid truncation */
        u8g2_uint_t tw = u8g2_GetStrWidth(&m1_u8g2, mod_text);
        u8g2_uint_t cx = tw < M1_LCD_DISPLAY_WIDTH ? (M1_LCD_DISPLAY_WIDTH - tw) / 2u : 0u;
        u8g2_DrawStr(&m1_u8g2, cx, 8, mod_text);
    }

    /* Right side: state indicator */
    if (state_text)
    {
        u8g2_uint_t tw = u8g2_GetStrWidth(&m1_u8g2, state_text);
        u8g2_uint_t x  = tw + 2u < M1_LCD_DISPLAY_WIDTH ? M1_LCD_DISPLAY_WIDTH - tw - 2u : 0u;
        u8g2_DrawStr(&m1_u8g2, x, 8, state_text);
    }

    /* Separator line at bottom of status bar */
    u8g2_DrawHLine(&m1_u8g2, 0, STATUS_BAR_H, M1_LCD_DISPLAY_WIDTH);
}

/*============================================================================*/
/* RSSI bar                                                                   */
/*============================================================================*/

void subghz_rssi_bar_draw(int16_t rssi_dbm)
{
    char rssi_str[8];

    /* Clamp RSSI to displayable range */
    if (rssi_dbm < RSSI_MIN) rssi_dbm = RSSI_MIN;
    if (rssi_dbm > RSSI_MAX) rssi_dbm = RSSI_MAX;

    /* Map to 0-108 pixels (leave 20px for text) */
    uint8_t bar_max_w = M1_LCD_DISPLAY_WIDTH - 20;
    uint8_t bar_w = (uint8_t)((uint32_t)(rssi_dbm - RSSI_MIN) * bar_max_w / (RSSI_MAX - RSSI_MIN));

    /* Clear RSSI bar area */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
    u8g2_DrawBox(&m1_u8g2, 0, RSSI_BAR_Y, M1_LCD_DISPLAY_WIDTH, RSSI_BAR_H + 1);

    /* Draw filled bar */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    if (bar_w > 0)
        u8g2_DrawBox(&m1_u8g2, 0, RSSI_BAR_Y, bar_w, RSSI_BAR_H);

    /* Draw RSSI text */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
    snprintf(rssi_str, sizeof(rssi_str), "%d", rssi_dbm);
    u8g2_DrawStr(&m1_u8g2, bar_max_w + 2, RSSI_BAR_Y + RSSI_BAR_H, rssi_str);
}
