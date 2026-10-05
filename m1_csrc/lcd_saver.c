/* See COPYING.txt for license details. */

/*
 *
 * lcd_saver.c
 *
 * Pure-logic decision state machine for the LCD backlight screen-saver.
 * See lcd_saver.h for the rationale. HARDWARE-INDEPENDENT — host-tested.
 *
 * M1 Project
 *
 */

/*************************** I N C L U D E S **********************************/

#include <stddef.h>
#include "lcd_saver.h"

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/


/*============================================================================*/
/*
 * @brief  Initialise a saver context to the "backlight on" state.
 */
/*============================================================================*/
void lcd_saver_ctx_init(lcd_saver_ctx_t *ctx)
{
    if (ctx == NULL)
        return;

    ctx->backlight_off = false;
}


/*============================================================================*/
/*
 * @brief  Advance the screen-saver state machine.
 * @retval LCD_SAVER_ACTION_ON/OFF on a transition, else LCD_SAVER_ACTION_NONE.
 */
/*============================================================================*/
lcd_saver_action_t lcd_saver_poll(lcd_saver_ctx_t *ctx,
                                    uint32_t now_ms,
                                    uint32_t active_timestamp,
                                    uint32_t timeout_ms)
{
    uint32_t delta;

    if (ctx == NULL)
        return LCD_SAVER_ACTION_NONE;

    /* Timeout of 0 means "never dim": keep the backlight on. */
    if (timeout_ms == 0u)
    {
        if (ctx->backlight_off)
        {
            ctx->backlight_off = false;
            return LCD_SAVER_ACTION_ON;
        }
        return LCD_SAVER_ACTION_NONE;
    }

    delta = now_ms - active_timestamp;  /* unsigned: wrap-safe */

    if (ctx->backlight_off)
    {
        /* Backlight is off: wake once activity is fresh again. */
        if (delta < timeout_ms)
        {
            ctx->backlight_off = false;
            return LCD_SAVER_ACTION_ON;
        }
    }
    else
    {
        /* Backlight is on: dim once the inactivity window elapses. */
        if (delta >= timeout_ms)
        {
            ctx->backlight_off = true;
            return LCD_SAVER_ACTION_OFF;
        }
    }

    return LCD_SAVER_ACTION_NONE;
}
