/* See COPYING.txt for license details. */

/*
 *
 * esp32_idle.c
 *
 * Pure-logic idle-timeout state machine for the ESP32-C6 coprocessor.
 * See esp32_idle.h for the rationale.  HARDWARE-INDEPENDENT — host-tested.
 *
 * M1 Project
 *
 */

/*************************** I N C L U D E S **********************************/

#include <stddef.h>
#include "esp32_idle.h"

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/


/*============================================================================*/
/*
 * @brief  Initialise an idle context with the given power-off window.
 */
/*============================================================================*/
void esp32_idle_ctx_init(esp32_idle_ctx_t *ctx, uint32_t timeout_ms)
{
    if (ctx == NULL)
        return;

    ctx->timeout_ms    = timeout_ms;
    ctx->idle_since_ms = 0u;
    ctx->idle_active   = false;
}


/*============================================================================*/
/*
 * @brief  Advance the idle-timeout state machine.
 * @retval ESP32_IDLE_ACTION_POWER_OFF once the idle window elapses, else NONE.
 */
/*============================================================================*/
esp32_idle_action_t esp32_idle_poll(esp32_idle_ctx_t *ctx,
                                    bool powered,
                                    bool busy,
                                    uint32_t now_ms)
{
    if (ctx == NULL)
        return ESP32_IDLE_ACTION_NONE;

    /* Not powered, or actively in use: no idle window is open. */
    if (!powered || busy)
    {
        ctx->idle_active = false;
        return ESP32_IDLE_ACTION_NONE;
    }

    /* Powered but idle: open the window on the first such poll. */
    if (!ctx->idle_active)
    {
        ctx->idle_active   = true;
        ctx->idle_since_ms = now_ms;
        return ESP32_IDLE_ACTION_NONE;
    }

    /* Window open: power off once it has elapsed (unsigned wrap-safe). */
    if ((uint32_t)(now_ms - ctx->idle_since_ms) >= ctx->timeout_ms)
    {
        ctx->idle_active = false;   /* consume — do not re-fire */
        return ESP32_IDLE_ACTION_POWER_OFF;
    }

    return ESP32_IDLE_ACTION_NONE;
}
