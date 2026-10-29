/* See COPYING.txt for license details. */

/*
 * m1_diag.c — Reset-cause diagnostics with .noinit RAM persistence
 *
 * Ported from da-pingwing / dagnazty M1_T-1000 (GPLv3, license-compatible).
 */

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdbool.h>
#include "m1_diag.h"
#include "m1_diag_status.h"
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_lcd.h"
#include "m1_display.h"
#include "m1_tasks.h"
#include "m1_system.h"

/*===========================================================================*/
/* .noinit diagnostic block (never zeroed by startup)                        */
/*===========================================================================*/

__attribute__((section(".noinit")))
volatile M1DiagBlock g_m1_diag;

static M1DiagBlock previous_diag;

void m1_diag_capture_previous(void)
{
    m1_diag_copy_saved(&previous_diag, &g_m1_diag);
}

/*===========================================================================*/
/* m1_diag_boot_report()                                                     */
/*===========================================================================*/

void m1_diag_boot_report(void)
{
    m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG);
    uint32_t rsr = RCC->RSR;
    m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG_RCC);
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;
    uint32_t bfar = SCB->BFAR;
    m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG_SCB);
    bool prev_valid = (previous_diag.magic == M1_DIAG_MAGIC);
    m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG_MAGIC);
    M1DiagBootStage prev_stage = prev_valid ? previous_diag.boot_stage : M1_DIAG_BOOT_EARLY;
    m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG_STAGE);
    M1DiagFaultKind prev_fault = prev_valid ? previous_diag.fault_kind : M1_DIAG_FAULT_NONE;
    m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG_FAULT);
    M1DiagResetOrigin prev_origin = prev_valid ? previous_diag.reset_origin : M1_DIAG_RESET_NONE;
    uint32_t prev_pc = prev_valid ? previous_diag.pc : 0;
    uint32_t prev_wdt_run_time = prev_valid ? previous_diag.wdt_run_time : 0;
    uint32_t prev_wdt_task_state = prev_valid ? previous_diag.wdt_task_state : UINT32_MAX;
    int32_t prev_wdt_wake_delta = prev_valid ? previous_diag.wdt_wake_delta : 0;
    m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG_ORIGIN);
    m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG_SNAPSHOT);
    m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG_REGS);
    char stage_status[32];
    char status[32];
    const char *cause_str = "POR";

    if      (rsr & RCC_RSR_IWDGRSTF) cause_str = "IWDG";
    else if (rsr & RCC_RSR_WWDGRSTF) cause_str = "WWDG";
    else if (rsr & RCC_RSR_SFTRSTF)  cause_str = "SFT";
    else if (rsr & RCC_RSR_BORRSTF)  cause_str = "BOR";
    else if (rsr & RCC_RSR_PINRSTF)  cause_str = "PIN";

    g_m1_diag.magic       = M1_DIAG_MAGIC;
    g_m1_diag.reset_cause = rsr;
    g_m1_diag.write_phase = M1_DIAG_PHASE_NONE;
    g_m1_diag.cfsr        = prev_valid ? previous_diag.cfsr : cfsr;
    g_m1_diag.hfsr        = prev_valid ? previous_diag.hfsr : hfsr;
    g_m1_diag.bfar        = prev_valid ? previous_diag.bfar : bfar;
    g_m1_diag.fault_kind  = M1_DIAG_FAULT_NONE;
    g_m1_diag.reset_origin = M1_DIAG_RESET_NONE;
    g_m1_diag.pc          = prev_pc;
    g_m1_diag.wdt_run_time = 0;
    g_m1_diag.wdt_task_state = UINT32_MAX;
    g_m1_diag.wdt_stack_high_water = 0;
    g_m1_diag.wdt_wake_delta = 0;
    g_m1_diag.boot_completed = false;

    __HAL_RCC_CLEAR_RESET_FLAGS();
    m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG_CLEAR);

    if (prev_fault == M1_DIAG_FAULT_WDT_TASK)
        m1_diag_wdt_status_text(prev_pc, prev_wdt_run_time,
                                prev_wdt_task_state, prev_wdt_wake_delta,
                                stage_status, sizeof(stage_status));
    else if (prev_fault == M1_DIAG_FAULT_ASSERT && prev_pc != 0)
        snprintf(stage_status, sizeof(stage_status), "ASSERT: %lu",
                 (unsigned long)prev_pc);
    else if (prev_fault != M1_DIAG_FAULT_NONE && prev_pc != 0)
        snprintf(stage_status, sizeof(stage_status), "PC: %08lX",
                 (unsigned long)prev_pc);
    else if (prev_fault != M1_DIAG_FAULT_NONE)
        m1_diag_boot_status_text(prev_stage, prev_fault, stage_status, sizeof(stage_status));
    else if (prev_origin >= M1_DIAG_DETAIL_RPC_NOTIFY)
        snprintf(stage_status, sizeof(stage_status), "RPC: %s",
                 m1_diag_reset_origin_text(prev_origin));
    else if (prev_origin != M1_DIAG_RESET_NONE)
        snprintf(stage_status, sizeof(stage_status), "RST: %s",
                 m1_diag_reset_origin_text(prev_origin));
    else
        m1_diag_boot_status_text(prev_stage, prev_fault, stage_status, sizeof(stage_status));
    m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG_TEXT);
    snprintf(status, sizeof(status), "%s: %.24s", cause_str, stage_status);
    if (m1_diag_boot_report_visible(
            prev_valid, previous_diag.boot_completed, prev_fault,
            (rsr & (RCC_RSR_IWDGRSTF | RCC_RSR_WWDGRSTF)) != 0))
    {
        m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG_LCD);
        startup_info_screen_display(status);
    }
    m1_diag_set_boot_stage(M1_DIAG_BOOT_DIAG_DONE);
}

void m1_diag_system_reset(M1DiagResetOrigin origin)
{
    m1_diag_set_reset_origin(origin);
    NVIC_SystemReset();
}

void m1_diag_fault_capture(M1DiagFaultKind kind)
{
    m1_diag_fault_capture_with_pc(kind, 0);
}

void m1_diag_fault_capture_with_pc(M1DiagFaultKind kind, uint32_t pc)
{
    g_m1_diag.magic      = M1_DIAG_MAGIC;
    g_m1_diag.fault_kind = kind;
    g_m1_diag.cfsr       = SCB->CFSR;
    g_m1_diag.hfsr       = SCB->HFSR;
    g_m1_diag.bfar       = SCB->BFAR;
    g_m1_diag.pc         = pc;
}

void m1_diag_wdt_capture(uint32_t task_id, uint32_t run_time,
                         uint32_t task_state, uint32_t stack_high_water,
                         int32_t wake_delta)
{
    m1_diag_fault_capture_with_pc(M1_DIAG_FAULT_WDT_TASK, task_id);
    g_m1_diag.wdt_run_time = run_time;
    g_m1_diag.wdt_task_state = task_state;
    g_m1_diag.wdt_stack_high_water = stack_high_water;
    g_m1_diag.wdt_wake_delta = wake_delta;
}

void m1_diag_assert_capture(const char *file, uint32_t line)
{
    (void)file;
    m1_diag_fault_capture_with_pc(M1_DIAG_FAULT_ASSERT, line);
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
