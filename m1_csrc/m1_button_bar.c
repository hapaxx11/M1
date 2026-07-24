/* See COPYING.txt for license details. */

/**
 * @file   m1_button_bar.c
 * @brief  Standardized 3-column bottom bar renderer, shared across all M1
 *         scenes and modules.
 *
 * Extracted from m1_subghz_button_bar.c (Phase 3 of the UI/UX consistency
 * overhaul) since this renderer has no Sub-GHz-specific logic and is used by
 * non-Sub-GHz modules (WiFi, CAN, NFC, Infrared, etc.).
 */

#include <stdint.h>
#include <stdbool.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_button_bar.h"

/*============================================================================*/
/* Layout constants                                                           */
/*============================================================================*/

/* Bottom bar: 3-column layout on a 128px wide display.
 * Momentum style: three individual rounded-corner buttons with 1px gaps.
 *   42 + 1 + 42 + 1 + 42 = 128 px (exact fill, no remainder).
 */
#define BAR_Y_TOP      52   /* Top of bottom bar area (12px high) */
#define BAR_H          12   /* Height of bottom bar */
#define BAR_ICON_Y     54   /* Y for 8x8 icons (1px from bar top for centering) */
#define BAR_TEXT_Y     61   /* Y baseline for text */

/* Button geometry */
#define BTN_W          42   /* Width of each button */
#define BTN_GAP         1   /* Gap between buttons (background color shows through) */
#define BTN_R           2   /* Corner radius */
#define BTN_0_X         0                         /* Left button X */
#define BTN_1_X        (BTN_0_X + BTN_W + BTN_GAP) /* Center button X */
#define BTN_2_X        (BTN_1_X + BTN_W + BTN_GAP) /* Right button X */

/*============================================================================*/
/* Bottom bar                                                                 */
/*============================================================================*/

/* Draw one Momentum-style button: filled rounded box, white content inside.
 * The combined icon+text block is centred horizontally within the button.
 * icon_on_right: false = icon left of text (left/center slots),
 *                true  = text left of icon (right slot). */
static void draw_btn(uint8_t bx,
                     const uint8_t *icon, const char *text,
                     bool icon_on_right)
{
    /* Filled rounded box */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
    u8g2_DrawRBox(&m1_u8g2, bx, BAR_Y_TOP, BTN_W, BAR_H, BTN_R);
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_BG);

    /* Measure content and centre it within the button.
     * Use u8g2_uint_t to avoid uint8_t truncation when text is wide. */
    u8g2_uint_t text_w    = text ? u8g2_GetStrWidth(&m1_u8g2, text) : 0u;
    u8g2_uint_t icon_w    = icon ? 8u : 0u;
    u8g2_uint_t gap       = (icon && text) ? 2u : 0u;
    u8g2_uint_t content_w = icon_w + gap + text_w;
    u8g2_uint_t cx = (u8g2_uint_t)bx + (content_w < BTN_W ? (BTN_W - content_w) / 2u : 1u);

    if (!icon_on_right)
    {
        /* Left/center slot: icon first (left of text) */
        if (icon) u8g2_DrawXBMP(&m1_u8g2, (u8g2_uint_t)cx,              BAR_ICON_Y, 8, 8, icon);
        if (text) u8g2_DrawStr( &m1_u8g2, (u8g2_uint_t)(cx + icon_w + gap), BAR_TEXT_Y, text);
    }
    else
    {
        /* Right slot: text first, icon to its right */
        if (text) u8g2_DrawStr( &m1_u8g2, (u8g2_uint_t)cx,                     BAR_TEXT_Y, text);
        if (icon) u8g2_DrawXBMP(&m1_u8g2, (u8g2_uint_t)(cx + text_w + gap), BAR_ICON_Y, 8, 8, icon);
    }
}

void m1_button_bar_draw(
    const uint8_t *left_icon,   const char *left_text,
    const uint8_t *center_icon, const char *center_text,
    const uint8_t *right_icon,  const char *right_text)
{
    /* Use slim font for button labels */
    u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);

    /* Draw each occupied slot as an individual rounded button.
     * Positions are fixed so gaps always appear between buttons. */
    if (left_icon   || left_text)
        draw_btn(BTN_0_X, left_icon,   left_text,   false);
    if (center_icon || center_text)
        draw_btn(BTN_1_X, center_icon, center_text, false);
    if (right_icon  || right_text)
        draw_btn(BTN_2_X, right_icon,  right_text,  true);

    /* Restore draw color */
    u8g2_SetDrawColor(&m1_u8g2, M1_DISP_DRAW_COLOR_TXT);
}
