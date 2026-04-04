/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_button_bar.c
 * @brief  Standardized bottom bar, status bar, and RSSI bar for Sub-GHz scenes.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_subghz_button_bar.h"

/*============================================================================*/
/* Layout constants                                                           */
/*============================================================================*/

/* Bottom bar: 3-column layout on a 128px wide display */
#define BAR_Y_TOP      52   /* Top of bottom bar area (12px high) */
#define BAR_H          12   /* Height of bottom bar */
#define BAR_ICON_Y     53   /* Y for 8x8 icons */
#define BAR_TEXT_Y     61   /* Y baseline for text */

#define COL_LEFT_X      2   /* Left column icon X */
#define COL_LEFT_TX    12   /* Left column text X */
#define COL_CENTER_X   48   /* Center column icon X */
#define COL_CENTER_TX  58   /* Center column text X */
#define COL_RIGHT_X    96   /* Right column icon X */
#define COL_RIGHT_TX  106   /* Right column text X */

/* Status bar */
#define STATUS_BAR_H   10   /* Height of top status bar */
#define STATUS_BAR_Y    0   /* Y position */

/* RSSI bar */
#define RSSI_BAR_Y     11   /* Just below status bar */
#define RSSI_BAR_H      3   /* Height of RSSI bar */
#define RSSI_MIN     -110   /* dBm mapped to 0 pixels */
#define RSSI_MAX      -30   /* dBm mapped to full width (128px) */

/*============================================================================*/
/* Bottom bar                                                                 */
/*============================================================================*/

void subghz_button_bar_draw(
    const uint8_t *left_icon,   const char *left_text,
    const uint8_t *center_icon, const char *center_text,
    const uint8_t *right_icon,  const char *right_text)
{
    /* Draw inverted background bar */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_DrawBox(&m1_u8g2, 0, BAR_Y_TOP, M1_LCD_DISPLAY_WIDTH, BAR_H);
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);

    /* Left column */
    if (left_icon)
        u8g2_DrawXBMP(&m1_u8g2, COL_LEFT_X, BAR_ICON_Y, 8, 8, left_icon);
    if (left_text)
        u8g2_DrawStr(&m1_u8g2, COL_LEFT_TX, BAR_TEXT_Y, left_text);

    /* Center column */
    if (center_icon)
        u8g2_DrawXBMP(&m1_u8g2, COL_CENTER_X, BAR_ICON_Y, 8, 8, center_icon);
    if (center_text)
        u8g2_DrawStr(&m1_u8g2, COL_CENTER_TX, BAR_TEXT_Y, center_text);

    /* Right column */
    if (right_icon)
        u8g2_DrawXBMP(&m1_u8g2, COL_RIGHT_X, BAR_ICON_Y, 8, 8, right_icon);
    if (right_text)
        u8g2_DrawStr(&m1_u8g2, COL_RIGHT_TX, BAR_TEXT_Y, right_text);

    /* Restore draw color */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
}

/*============================================================================*/
/* Status bar                                                                 */
/*============================================================================*/

void subghz_status_bar_draw(
    const char *freq_text,
    const char *mod_text,
    const char *state_text,
    bool hopping)
{
    char line[32];

    /* Draw inverted background */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_DrawBox(&m1_u8g2, 0, STATUS_BAR_Y, M1_LCD_DISPLAY_WIDTH, STATUS_BAR_H);
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);
    u8g2_SetFont(&m1_u8g2, M1_DISP_RUN_MENU_FONT_B);

    /* Left side: frequency or "Hopping" */
    if (hopping)
        u8g2_DrawStr(&m1_u8g2, 2, 8, "HOP");
    else if (freq_text)
        u8g2_DrawStr(&m1_u8g2, 2, 8, freq_text);

    /* Center: modulation */
    if (mod_text)
    {
        /* Center the modulation text */
        uint8_t tw = u8g2_GetStrWidth(&m1_u8g2, mod_text);
        uint8_t cx = (M1_LCD_DISPLAY_WIDTH - tw) / 2;
        u8g2_DrawStr(&m1_u8g2, cx, 8, mod_text);
    }

    /* Right side: state indicator */
    if (state_text)
    {
        uint8_t tw = u8g2_GetStrWidth(&m1_u8g2, state_text);
        u8g2_DrawStr(&m1_u8g2, M1_LCD_DISPLAY_WIDTH - tw - 2, 8, state_text);
    }

    /* Restore draw color */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
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
