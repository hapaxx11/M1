/* See COPYING.txt for license details. */

/*
 * m1_diag.c — Reset-cause diagnostics with .noinit RAM persistence
 *
 * Ported from da-pingwing / dagnazty M1_T-1000 (GPLv3, license-compatible).
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "m1_diag.h"
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_lcd.h"
#include "m1_display.h"
#include "m1_tasks.h"
#include "m1_log_debug.h"

/*===========================================================================*/
/* .noinit diagnostic block (never zeroed by startup)                        */
/*===========================================================================*/

__attribute__((section(".noinit")))
M1DiagBlock g_m1_diag;

/*===========================================================================*/
/* Logging                                                                   */
/*===========================================================================*/

#define M1_LOGDB_TAG "Diag"

/*===========================================================================*/
/* m1_diag_boot_report()                                                     */
/*===========================================================================*/

void m1_diag_boot_report(void)
{
    uint32_t rsr = RCC->RSR;

    /* Capture fault registers for this boot (they come from the CPU) */
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    uint32_t bfar = SCB->BFAR;

    /* Decode the previous boot's write phase if the magic is valid */
    bool prev_valid = (g_m1_diag.magic == M1_DIAG_MAGIC);
    M1DiagWritePhase prev_phase  = prev_valid ? g_m1_diag.write_phase  : M1_DIAG_PHASE_NONE;
    uint32_t         prev_cause  = prev_valid ? g_m1_diag.reset_cause  : 0;
    uint32_t         prev_cfsr   = prev_valid ? g_m1_diag.cfsr          : 0;
    (void)prev_cause; (void)prev_cfsr;

    /* Log reset cause */
    const char *cause_str = "POR";
    if      (rsr & RCC_RSR_IWDGRSTF) cause_str = "IWDG";
    else if (rsr & RCC_RSR_WWDGRSTF) cause_str = "WWDG";
    else if (rsr & RCC_RSR_SFTRSTF)  cause_str = "SFT";
    else if (rsr & RCC_RSR_BORRSTF)  cause_str = "BOR";
    else if (rsr & RCC_RSR_PINRSTF)  cause_str = "PIN";

    if (prev_valid && prev_phase != M1_DIAG_PHASE_NONE
                   && prev_phase != M1_DIAG_PHASE_DONE) {
        M1_LOG_W(M1_LOGDB_TAG,
                 "Reset(%s) during RFID write phase %d! RSR=0x%08lX\r\n",
                 cause_str, (int)prev_phase, (unsigned long)prev_cause);
    } else {
        M1_LOG_I(M1_LOGDB_TAG, "Boot reset cause: %s (RSR=0x%08lX)\r\n",
                 cause_str, (unsigned long)rsr);
    }

    /* Save current boot state */
    g_m1_diag.magic       = M1_DIAG_MAGIC;
    g_m1_diag.reset_cause = rsr;
    g_m1_diag.write_phase = M1_DIAG_PHASE_NONE;
    g_m1_diag.cfsr        = cfsr;
    g_m1_diag.hfsr        = hfsr;
    g_m1_diag.bfar        = bfar;

    /* Clear reset flags so next boot gets a clean slate */
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

/*===========================================================================*/
/* m1_diag_screen()                                                          */
/*===========================================================================*/

void m1_diag_screen(void)
{
    /* Build display strings from the saved diagnostic block */
    bool     valid  = (g_m1_diag.magic == M1_DIAG_MAGIC);
    uint32_t rsr    = valid ? g_m1_diag.reset_cause  : 0;
    uint32_t phase  = valid ? (uint32_t)g_m1_diag.write_phase : 0;
    uint32_t cfsr   = valid ? g_m1_diag.cfsr          : 0;
    uint32_t hfsr   = valid ? g_m1_diag.hfsr          : 0;

    const char *cause_str = "POR/Unknown";
    if      (rsr & RCC_RSR_IWDGRSTF) cause_str = "IWDG watchdog";
    else if (rsr & RCC_RSR_WWDGRSTF) cause_str = "WWDG watchdog";
    else if (rsr & RCC_RSR_SFTRSTF)  cause_str = "Software reset";
    else if (rsr & RCC_RSR_BORRSTF)  cause_str = "Brown-out";
    else if (rsr & RCC_RSR_PINRSTF)  cause_str = "PIN reset";

    static const char *const phase_names[] = {
        "None", "Start", "Write", "Verify", "Done"
    };
    const char *phase_str = (phase < 5) ? phase_names[phase] : "???";

    char line_cause[32], line_phase[32], line_cfsr[32], line_hfsr[32];
    snprintf(line_cause, sizeof(line_cause), "Reset: %s", cause_str);
    snprintf(line_phase, sizeof(line_phase), "RFID phase: %s", phase_str);
    snprintf(line_cfsr,  sizeof(line_cfsr),  "CFSR: %08lX", (unsigned long)cfsr);
    snprintf(line_hfsr,  sizeof(line_hfsr),  "HFSR: %08lX", (unsigned long)hfsr);

    /* Drain any stale button events */
    S_M1_Buttons_Status bs;
    while (xQueueReceive(button_events_q_hdl, &bs, 0) == pdTRUE) { }

    bool running = true;
    while (running) {
        /* Draw */
        m1_u8g2_firstpage();
        u8g2_SetFont(&m1_u8g2, M1_DISP_FUNC_MENU_FONT_N);

        u8g2_DrawStr(&m1_u8g2, 0, 10, "RFID Diagnostics");
        u8g2_DrawHLine(&m1_u8g2, 0, 11, 128);

        if (!valid) {
            u8g2_DrawStr(&m1_u8g2, 0, 26, "No diag data");
            u8g2_DrawStr(&m1_u8g2, 0, 38, "(first boot?)");
        } else {
            u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
            u8g2_DrawStr(&m1_u8g2, 0, 23, line_cause);
            u8g2_DrawStr(&m1_u8g2, 0, 33, line_phase);
            u8g2_DrawStr(&m1_u8g2, 0, 43, line_cfsr);
            u8g2_DrawStr(&m1_u8g2, 0, 53, line_hfsr);
        }

        u8g2_SetFont(&m1_u8g2, M1_DISP_SUB_MENU_FONT_N);
        u8g2_DrawStr(&m1_u8g2, 34, 63, "BACK: exit");
        m1_u8g2_nextpage();

        /* Wait for any button */
        S_M1_Main_Q_t q;
        if (xQueueReceive(main_q_hdl, &q, pdMS_TO_TICKS(200)) == pdTRUE) {
            if (q.q_evt_type == Q_EVENT_KEYPAD) {
                xQueueReceive(button_events_q_hdl, &bs, 0);
                running = false;
            }
        }
    }
}
