/* See COPYING.txt for license details. */

/*
 * m1_diag.h — Reset-cause diagnostics with .noinit RAM persistence
 *
 * Stores a T5577 write-phase breadcrumb and reset cause in .noinit RAM,
 * surviving brownout/watchdog resets.  A diagnostics screen accessible
 * from the RFID Utilities menu shows the last reset cause (BOR/IWDG/
 * SFT/POR), which write phase was in progress, and fault registers.
 *
 * Very useful for debugging silent resets during RFID writes without a
 * serial adapter.
 *
 * Ported from da-pingwing / dagnazty M1_T-1000 (GPLv3, license-compatible).
 *
 * Usage:
 *   1. Call m1_diag_boot_report() early in startup (after logdb init).
 *   2. Call m1_diag_set_phase(M1_DIAG_PHASE_xxx) before each write step.
 *   3. Call m1_diag_set_phase(M1_DIAG_PHASE_NONE) when write completes cleanly.
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

/* -------------------------------------------------------------------------
 * .noinit diagnostic block — survives all soft resets
 * Magic guards against stale RAM at first power-up.
 * ------------------------------------------------------------------------- */
#define M1_DIAG_MAGIC  0xD1A90C0EUL

typedef struct {
    uint32_t        magic;           /* M1_DIAG_MAGIC when valid */
    M1DiagWritePhase write_phase;    /* Write phase at last reset */
    uint32_t        reset_cause;     /* RCC->RSR snapshot        */
    uint32_t        cfsr;            /* SCB->CFSR (fault status) */
    uint32_t        hfsr;            /* SCB->HFSR (hard fault)   */
    uint32_t        bfar;            /* SCB->BFAR (bus fault addr)*/
} M1DiagBlock;

/* The diagnostic block lives in .noinit so it is never zeroed at startup. */
extern M1DiagBlock g_m1_diag;

/* -------------------------------------------------------------------------
 * Inline setter — called from t5577.c write steps (zero overhead).
 * Always safe to call: a write to a global is atomic enough for diagnostics.
 * ------------------------------------------------------------------------- */
static inline void m1_diag_set_phase(M1DiagWritePhase phase)
{
    g_m1_diag.magic       = M1_DIAG_MAGIC;
    g_m1_diag.write_phase = phase;
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/* Read RCC->RSR, snapshot previous boot state, clear reset flags.
 * Must be called once early in boot (after logdb init). */
void m1_diag_boot_report(void);

/* Blocking diagnostics screen — shows reset cause, write phase, fault regs.
 * Exit on any button press.  Called from RFID Utilities "RFID Diagnostics". */
void m1_diag_screen(void);

#ifdef __cplusplus
}
#endif

#endif /* M1_DIAG_H_ */
