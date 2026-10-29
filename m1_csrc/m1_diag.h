/* See COPYING.txt for license details. */

/*
 * m1_diag.h — Reset-cause diagnostics with .noinit RAM persistence
 *
 * Stores a T5577 write-phase breadcrumb, startup milestone, reset cause,
 * and captured fault registers in .noinit RAM, surviving brownout/watchdog
 * resets.  The early home screen shows the previous boot stage/fault without
 * requiring button input.  The RFID Utilities diagnostics screen shows the
 * reset cause, write phase, and fault registers.
 *
 * Useful for debugging silent resets and startup freezes without a serial
 * adapter.
 *
 * Ported from da-pingwing / dagnazty M1_T-1000 (GPLv3, license-compatible).
 *
 * Usage:
 *   1. Call m1_diag_boot_report() early in startup (after logdb init).
 *   2. Call m1_diag_set_phase(M1_DIAG_PHASE_xxx) before each write step.
 *   3. Call m1_diag_set_phase(M1_DIAG_PHASE_DONE) when write completes cleanly.
 *   4. From RFID Utilities "RFID Diagnostics", call m1_diag_screen().
 */

#ifndef M1_DIAG_H_
#define M1_DIAG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Write phase breadcrumb enum
 * Stored in .noinit RAM so a watchdog/brownout reset during t5577_execute_write
 * leaves behind a record of which step was in progress.
 * ------------------------------------------------------------------------- */
typedef enum {
    M1_DIAG_PHASE_NONE      = 0,    /* No write in progress */
    M1_DIAG_PHASE_START     = 1,    /* t5577_write_start()     */
    M1_DIAG_PHASE_WRITE_BIT = 2,    /* t5577_write_block_data() */
    M1_DIAG_PHASE_VERIFY    = 3,    /* Post-write verify read   */
    M1_DIAG_PHASE_DONE      = 4,    /* Write completed cleanly  */
} M1DiagWritePhase;

typedef enum {
    M1_DIAG_BOOT_MAIN = 0,
    M1_DIAG_BOOT_EARLY,
    M1_DIAG_BOOT_HAL,
    M1_DIAG_BOOT_CLOCK,
    M1_DIAG_BOOT_MPU,
    M1_DIAG_BOOT_GPIO,
    M1_DIAG_BOOT_IWDG,
    M1_DIAG_BOOT_SPI1,
    M1_DIAG_BOOT_ICACHE,
    M1_DIAG_BOOT_SPI2,
    M1_DIAG_BOOT_I2C,
    M1_DIAG_BOOT_RTC,
    M1_DIAG_BOOT_CRC,
    M1_DIAG_BOOT_KERNEL,
    M1_DIAG_BOOT_SYSINIT,
    M1_DIAG_BOOT_IOHAL,
    M1_DIAG_BOOT_BATTERY,
    M1_DIAG_BOOT_LCD,
    M1_DIAG_BOOT_SDCARD,
    M1_DIAG_BOOT_SDMMC,
    M1_DIAG_BOOT_SD_CARD,
    M1_DIAG_BOOT_LOGDB,
    M1_DIAG_BOOT_UART,
    M1_DIAG_BOOT_DMA_RX,
    M1_DIAG_BOOT_DMA_TX,
    M1_DIAG_BOOT_LOG_RTOS,
    M1_DIAG_BOOT_LOG_MUTEX,
    M1_DIAG_BOOT_LOG_QUEUE,
    M1_DIAG_BOOT_LOG_DONE,
    M1_DIAG_BOOT_DIAG,
    M1_DIAG_BOOT_DIAG_RCC,
    M1_DIAG_BOOT_DIAG_SCB,
    M1_DIAG_BOOT_DIAG_MAGIC,
    M1_DIAG_BOOT_DIAG_STAGE,
    M1_DIAG_BOOT_DIAG_FAULT,
    M1_DIAG_BOOT_DIAG_ORIGIN,
    M1_DIAG_BOOT_DIAG_SNAPSHOT,
    M1_DIAG_BOOT_DIAG_REGS,
    M1_DIAG_BOOT_DIAG_CLEAR,
    M1_DIAG_BOOT_DIAG_TEXT,
    M1_DIAG_BOOT_DIAG_LCD,
    M1_DIAG_BOOT_DIAG_DONE,
    M1_DIAG_BOOT_HOME_START,
    M1_DIAG_BOOT_HOME_RTC,
    M1_DIAG_BOOT_HOME_PAGE,
    M1_DIAG_BOOT_HOME_TIME,
    M1_DIAG_BOOT_HOME_LOGO,
    M1_DIAG_BOOT_HOME_BATT,
    M1_DIAG_BOOT_HOME_ICONS,
    M1_DIAG_BOOT_HOME_TEXT,
    M1_DIAG_BOOT_HOME_SEND,
    M1_DIAG_BOOT_HOME_BACKLIGHT,
    M1_DIAG_BOOT_NEXT_SEND,
    M1_DIAG_BOOT_NEXT_REFRESH,
    M1_DIAG_BOOT_NEXT_RPC,
    M1_DIAG_BOOT_NEXT_DONE,
    M1_DIAG_BOOT_LCD_SPI_ENTER,
    M1_DIAG_BOOT_LCD_SPI_EXIT,
    M1_DIAG_BOOT_BL_START,
    M1_DIAG_BOOT_BL_PWM,
    M1_DIAG_BOOT_BL_READ,
    M1_DIAG_BOOT_BL_CONFIG,
    M1_DIAG_BOOT_BL_UPDATE,
    M1_DIAG_BOOT_I2C_WAIT,
    M1_DIAG_BOOT_I2C_XFER,
    M1_DIAG_BOOT_I2C_DONE,
    M1_DIAG_BOOT_I2C_RELEASED,
    M1_DIAG_BOOT_LP_WRITE_DONE,
    M1_DIAG_BOOT_SDMMC_IRQ,
    M1_DIAG_BOOT_LOG_IRQ,
    M1_DIAG_BOOT_WDT_ENTRY,
    M1_DIAG_BOOT_WDT_BKUP,
    M1_DIAG_BOOT_WDT_WRITE,
    M1_DIAG_BOOT_WDT_ARM,
    M1_DIAG_BOOT_WDT_TASK,
    M1_DIAG_BOOT_WATCHDOG,
    M1_DIAG_BOOT_TASKS,
    M1_DIAG_BOOT_CONFIG,
    M1_DIAG_BOOT_SETTINGS,
    M1_DIAG_BOOT_MENU,
} M1DiagBootStage;

typedef enum {
    M1_DIAG_FAULT_NONE = 0,
    M1_DIAG_FAULT_ERROR,
    M1_DIAG_FAULT_HARD,
    M1_DIAG_FAULT_MEMMANAGE,
    M1_DIAG_FAULT_BUS,
    M1_DIAG_FAULT_USAGE,
    M1_DIAG_FAULT_NMI,
    M1_DIAG_FAULT_ASSERT,
    M1_DIAG_FAULT_WDT_TASK,
} M1DiagFaultKind;

typedef enum {
    M1_DIAG_RESET_NONE = 0,
    M1_DIAG_RESET_GENERIC,
    M1_DIAG_RESET_RPC,
    M1_DIAG_RESET_POWER,
    M1_DIAG_RESET_BANK,
    M1_DIAG_RESET_DFU,
    M1_DIAG_RESET_FLASH_OB,
    M1_DIAG_DETAIL_RPC_NOTIFY = 16,
    M1_DIAG_DETAIL_RPC_DEFERRED,
    M1_DIAG_DETAIL_RPC_RELAY,
    M1_DIAG_DETAIL_RPC_OWNER,
    M1_DIAG_DETAIL_RPC_ESP_INIT,
    M1_DIAG_DETAIL_RPC_TRANSPORT,
    M1_DIAG_DETAIL_RPC_STATUS,
    M1_DIAG_DETAIL_RPC_BLE_ADV,
    M1_DIAG_DETAIL_RPC_QMON,
    M1_DIAG_DETAIL_RPC_FEED,
    M1_DIAG_DETAIL_RPC_STREAM,
    M1_DIAG_DETAIL_RPC_EPOCH,
} M1DiagResetOrigin;

/* -------------------------------------------------------------------------
 * .noinit diagnostic block — survives all soft resets
 * Magic guards against stale RAM and diagnostic-layout changes.
 * ------------------------------------------------------------------------- */
#define M1_DIAG_MAGIC  0xD1A90C26UL

typedef struct {
    uint32_t        magic;           /* M1_DIAG_MAGIC when valid */
    M1DiagWritePhase write_phase;    /* Write phase at last reset */
    uint32_t        reset_cause;     /* RCC->RSR snapshot        */
    uint32_t        cfsr;            /* SCB->CFSR (fault status) */
    uint32_t        hfsr;            /* SCB->HFSR (hard fault)   */
    uint32_t        bfar;            /* SCB->BFAR (bus fault addr)*/
    M1DiagBootStage boot_stage;      /* Last startup milestone reached */
    M1DiagFaultKind fault_kind;      /* Fault captured before reset loop */
    M1DiagResetOrigin reset_origin;  /* Software reset call site */
    uint32_t        pc;              /* Stacked PC at fault */
    uint32_t        wdt_run_time;    /* Accumulated task time at WDT failure */
    uint32_t        wdt_task_state;  /* eTaskState at WDT failure */
    uint32_t        wdt_stack_high_water; /* Free task stack, in words */
    int32_t         wdt_wake_delta;  /* Task wake tick minus current tick */
    uint32_t        boot_completed; /* Menu reached during this boot */
} M1DiagBlock;

/* The diagnostic block lives in .noinit so it is never zeroed at startup. */
extern volatile M1DiagBlock g_m1_diag;

/* -------------------------------------------------------------------------
 * Inline setters — called from t5577.c write steps and startup milestones.
 * Always safe to call: writes to globals are atomic enough for diagnostics.
 * ------------------------------------------------------------------------- */
static inline uint32_t m1_diag_boot_completion_sticky(
    uint32_t completed, M1DiagBootStage stage)
{
    return completed || stage == M1_DIAG_BOOT_MENU ? 1u : 0u;
}

static inline void m1_diag_set_phase(M1DiagWritePhase phase)
{
    g_m1_diag.magic       = M1_DIAG_MAGIC;
    g_m1_diag.write_phase = phase;
}

static inline void m1_diag_set_boot_stage(M1DiagBootStage stage)
{
    g_m1_diag.magic      = M1_DIAG_MAGIC;
    g_m1_diag.boot_stage = stage;
    g_m1_diag.boot_completed =
        m1_diag_boot_completion_sticky(g_m1_diag.boot_completed, stage);
}

static inline void m1_diag_set_reset_origin(M1DiagResetOrigin origin)
{
    g_m1_diag.magic        = M1_DIAG_MAGIC;
    g_m1_diag.reset_origin = origin;
}

static inline void m1_diag_set_detail(M1DiagResetOrigin detail)
{
    g_m1_diag.magic        = M1_DIAG_MAGIC;
    g_m1_diag.reset_origin = detail;
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/* Preserve the previous boot before main() writes any new diagnostic state. */
void m1_diag_capture_previous(void);

/* Read RCC->RSR, report the preserved boot state, clear reset flags.
 * Must be called once early in boot (after logdb init). */
void m1_diag_boot_report(void);

/* Fault-context-safe register capture; no allocation, logging, or RTOS calls. */
void m1_diag_fault_capture(M1DiagFaultKind kind);
void m1_diag_fault_capture_with_pc(M1DiagFaultKind kind, uint32_t pc);
void m1_diag_wdt_capture(uint32_t task_id, uint32_t run_time,
                         uint32_t task_state, uint32_t stack_high_water,
                         int32_t wake_delta);
void m1_diag_assert_capture(const char *file, uint32_t line);
void m1_diag_system_reset(M1DiagResetOrigin origin);

/* Blocking diagnostics screen — shows reset cause, write phase, fault regs.
 * Exit on any button press.  Called from RFID Utilities "RFID Diagnostics". */
void m1_diag_screen(void);

#ifdef __cplusplus
}
#endif

#endif /* M1_DIAG_H_ */
