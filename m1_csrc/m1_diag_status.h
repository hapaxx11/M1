/* See COPYING.txt for license details. */

#ifndef M1_DIAG_STATUS_H_
#define M1_DIAG_STATUS_H_

#include <stdbool.h>
#include <stdio.h>
#include "m1_diag.h"

static inline void m1_diag_copy_saved(M1DiagBlock *dest,
                                      const volatile M1DiagBlock *src)
{
    dest->magic = src->magic;
    dest->write_phase = src->write_phase;
    dest->reset_cause = src->reset_cause;
    dest->cfsr = src->cfsr;
    dest->hfsr = src->hfsr;
    dest->bfar = src->bfar;
    dest->boot_stage = src->boot_stage;
    dest->fault_kind = src->fault_kind;
    dest->reset_origin = src->reset_origin;
    dest->pc = src->pc;
    dest->wdt_run_time = src->wdt_run_time;
    dest->wdt_task_state = src->wdt_task_state;
    dest->wdt_stack_high_water = src->wdt_stack_high_water;
    dest->wdt_wake_delta = src->wdt_wake_delta;
    dest->boot_completed = src->boot_completed;
}

static inline void m1_diag_wdt_status_text(uint32_t task_id,
                                           uint32_t run_time,
                                           uint32_t task_state,
                                           int32_t wake_delta,
                                           char *out,
                                           size_t out_size)
{
    const char *state = "?";
    switch (task_state)
    {
    case 0: state = "RUN"; break;
    case 1: state = "RDY"; break;
    case 2: state = "BLK"; break;
    case 3: state = "SUS"; break;
    case 4: state = "DEL"; break;
    default: break;
    }
    snprintf(out, out_size, "W%lu R%lu %s D%ld",
             (unsigned long)task_id, (unsigned long)run_time, state,
             (long)wake_delta);
}

static inline uint32_t m1_diag_stacked_pc(const uint32_t *frame,
                                          uint32_t exc_return)
{
    (void)exc_return;
    return frame != NULL ? frame[6u] : 0u;
}

static inline bool m1_diag_boot_report_visible(bool valid,
                                               bool boot_completed,
                                               M1DiagFaultKind fault,
                                               bool watchdog_reset)
{
    if (watchdog_reset || fault != M1_DIAG_FAULT_NONE)
        return true;
    return valid && !boot_completed;
}

static inline bool m1_diag_reset_origin_status_visible(
    M1DiagFaultKind fault, M1DiagResetOrigin origin)
{
    return fault == M1_DIAG_FAULT_NONE && origin != M1_DIAG_RESET_NONE;
}

static inline const char *m1_diag_reset_origin_text(M1DiagResetOrigin origin)
{
    switch (origin)
    {
    case M1_DIAG_RESET_GENERIC:  return "GENERIC";
    case M1_DIAG_RESET_RPC:      return "RPC";
    case M1_DIAG_RESET_POWER:    return "POWER";
    case M1_DIAG_RESET_BANK:     return "BANK";
    case M1_DIAG_RESET_DFU:      return "DFU";
    case M1_DIAG_RESET_FLASH_OB: return "FLASHOB";
    case M1_DIAG_DETAIL_RPC_NOTIFY: return "NOTIFY";
    case M1_DIAG_DETAIL_RPC_DEFERRED: return "DEFERRED";
    case M1_DIAG_DETAIL_RPC_RELAY: return "RELAY";
    case M1_DIAG_DETAIL_RPC_OWNER: return "OWNER";
    case M1_DIAG_DETAIL_RPC_ESP_INIT: return "ESPINIT";
    case M1_DIAG_DETAIL_RPC_TRANSPORT: return "TRANSPORT";
    case M1_DIAG_DETAIL_RPC_STATUS: return "STATUS";
    case M1_DIAG_DETAIL_RPC_BLE_ADV: return "BLEADV";
    case M1_DIAG_DETAIL_RPC_QMON: return "QMON";
    case M1_DIAG_DETAIL_RPC_FEED: return "FEED";
    case M1_DIAG_DETAIL_RPC_STREAM: return "STREAM";
    case M1_DIAG_DETAIL_RPC_EPOCH: return "EPOCH";
    default:                     return "UNKNOWN";
    }
}

static inline void m1_diag_boot_status_text(M1DiagBootStage stage,
                                            M1DiagFaultKind fault,
                                            char *out,
                                            size_t out_size)
{
    const char *text;

    switch (fault)
    {
    case M1_DIAG_FAULT_NONE:      text = NULL;      break;
    case M1_DIAG_FAULT_ERROR:     text = "ERROR";   break;
    case M1_DIAG_FAULT_HARD:      text = "HARD";    break;
    case M1_DIAG_FAULT_MEMMANAGE: text = "MEM";     break;
    case M1_DIAG_FAULT_BUS:       text = "BUS";     break;
    case M1_DIAG_FAULT_USAGE:     text = "USAGE";   break;
    case M1_DIAG_FAULT_NMI:       text = "NMI";     break;
    case M1_DIAG_FAULT_ASSERT:    text = "ASSERT";  break;
    case M1_DIAG_FAULT_WDT_TASK:  text = "WDT TASK"; break;
    default:                      text = "UNKNOWN"; break;
    }

    if (text != NULL)
    {
        snprintf(out, out_size, "FAULT: %s", text);
        return;
    }

    switch (stage)
    {
    case M1_DIAG_BOOT_MAIN:     text = "MAIN";     break;
    case M1_DIAG_BOOT_EARLY:    text = "EARLY";    break;
    case M1_DIAG_BOOT_HAL:      text = "HAL";      break;
    case M1_DIAG_BOOT_CLOCK:    text = "CLOCK";    break;
    case M1_DIAG_BOOT_MPU:      text = "MPU";      break;
    case M1_DIAG_BOOT_GPIO:     text = "GPIO";     break;
    case M1_DIAG_BOOT_IWDG:     text = "IWDG";     break;
    case M1_DIAG_BOOT_SPI1:     text = "SPI1";     break;
    case M1_DIAG_BOOT_ICACHE:   text = "ICACHE";   break;
    case M1_DIAG_BOOT_SPI2:     text = "SPI2";     break;
    case M1_DIAG_BOOT_I2C:      text = "I2C";      break;
    case M1_DIAG_BOOT_RTC:      text = "RTC";      break;
    case M1_DIAG_BOOT_CRC:      text = "CRC";      break;
    case M1_DIAG_BOOT_KERNEL:   text = "KERNEL";   break;
    case M1_DIAG_BOOT_SYSINIT:  text = "SYSINIT";  break;
    case M1_DIAG_BOOT_IOHAL:    text = "IOHAL";    break;
    case M1_DIAG_BOOT_BATTERY:  text = "BATTERY";  break;
    case M1_DIAG_BOOT_LCD:      text = "LCD";      break;
    case M1_DIAG_BOOT_SDCARD:   text = "SDCARD";   break;
    case M1_DIAG_BOOT_SDMMC:    text = "SDMMC";    break;
    case M1_DIAG_BOOT_SD_CARD:  text = "SDINIT";   break;
    case M1_DIAG_BOOT_LOGDB:    text = "LOGDB";    break;
    case M1_DIAG_BOOT_UART:     text = "UART";     break;
    case M1_DIAG_BOOT_DMA_RX:   text = "DMARX";    break;
    case M1_DIAG_BOOT_DMA_TX:   text = "DMATX";    break;
    case M1_DIAG_BOOT_LOG_RTOS:  text = "LOGRTOS";  break;
    case M1_DIAG_BOOT_LOG_MUTEX: text = "LOGMUTEX"; break;
    case M1_DIAG_BOOT_LOG_QUEUE: text = "LOGQSTATIC"; break;
    case M1_DIAG_BOOT_LOG_DONE:  text = "LOGQDONE";  break;
    case M1_DIAG_BOOT_DIAG:         text = "DIAG";      break;
    case M1_DIAG_BOOT_DIAG_RCC:     text = "DRCC";      break;
    case M1_DIAG_BOOT_DIAG_SCB:     text = "DSCB";      break;
    case M1_DIAG_BOOT_DIAG_MAGIC:   text = "DMAGIC";    break;
    case M1_DIAG_BOOT_DIAG_STAGE:   text = "DSTAGE";    break;
    case M1_DIAG_BOOT_DIAG_FAULT:   text = "DFAULT";    break;
    case M1_DIAG_BOOT_DIAG_ORIGIN:  text = "DORIGIN";   break;
    case M1_DIAG_BOOT_DIAG_SNAPSHOT: text = "DSNAP";    break;
    case M1_DIAG_BOOT_DIAG_REGS:    text = "DREGS";     break;
    case M1_DIAG_BOOT_DIAG_CLEAR: text = "DCLEAR";    break;
    case M1_DIAG_BOOT_DIAG_TEXT:  text = "DTEXT";     break;
    case M1_DIAG_BOOT_DIAG_LCD:   text = "DLCD";      break;
    case M1_DIAG_BOOT_DIAG_DONE:  text = "DIAGDONE";  break;
    case M1_DIAG_BOOT_HOME_START: text = "HSTART";    break;
    case M1_DIAG_BOOT_HOME_RTC:   text = "HRTC";      break;
    case M1_DIAG_BOOT_HOME_PAGE:  text = "HPAGE";     break;
    case M1_DIAG_BOOT_HOME_TIME:  text = "HTIME";     break;
    case M1_DIAG_BOOT_HOME_LOGO:  text = "HLOGO";     break;
    case M1_DIAG_BOOT_HOME_BATT:  text = "HBATT";     break;
    case M1_DIAG_BOOT_HOME_ICONS: text = "HICONS";    break;
    case M1_DIAG_BOOT_HOME_TEXT:  text = "HTEXT";     break;
    case M1_DIAG_BOOT_HOME_SEND:  text = "HSEND";     break;
    case M1_DIAG_BOOT_HOME_BACKLIGHT: text = "HBL";   break;
    case M1_DIAG_BOOT_NEXT_SEND: text = "NSEND";       break;
    case M1_DIAG_BOOT_NEXT_REFRESH: text = "NREFRESH"; break;
    case M1_DIAG_BOOT_NEXT_RPC: text = "NRPC";         break;
    case M1_DIAG_BOOT_NEXT_DONE: text = "NDONE";       break;
    case M1_DIAG_BOOT_LCD_SPI_ENTER: text = "SPIENTER"; break;
    case M1_DIAG_BOOT_LCD_SPI_EXIT: text = "SPIEXIT";  break;
    case M1_DIAG_BOOT_BL_START: text = "BLSTART";      break;
    case M1_DIAG_BOOT_BL_PWM: text = "BLPWM";          break;
    case M1_DIAG_BOOT_BL_READ: text = "BLREAD";        break;
    case M1_DIAG_BOOT_BL_CONFIG: text = "BLCFG";       break;
    case M1_DIAG_BOOT_BL_UPDATE: text = "BLUPDATE";    break;
    case M1_DIAG_BOOT_I2C_WAIT: text = "I2CWAIT";      break;
    case M1_DIAG_BOOT_I2C_XFER: text = "I2CXFER";      break;
    case M1_DIAG_BOOT_I2C_DONE: text = "I2CDONE";      break;
    case M1_DIAG_BOOT_I2C_RELEASED: text = "I2CREL";   break;
    case M1_DIAG_BOOT_LP_WRITE_DONE: text = "LPWDONE"; break;
    case M1_DIAG_BOOT_SDMMC_IRQ:  text = "SDIRQ";     break;
    case M1_DIAG_BOOT_LOG_IRQ:    text = "LOGIRQ";    break;
    case M1_DIAG_BOOT_WDT_ENTRY:  text = "WDTENTER";  break;
    case M1_DIAG_BOOT_WDT_BKUP:   text = "WDTBKUP";   break;
    case M1_DIAG_BOOT_WDT_WRITE:  text = "WDTWRITE";  break;
    case M1_DIAG_BOOT_WDT_ARM:    text = "WDTARM";    break;
    case M1_DIAG_BOOT_WDT_TASK:   text = "WDTTASK";   break;
    case M1_DIAG_BOOT_WATCHDOG:   text = "WDT";       break;
    case M1_DIAG_BOOT_TASKS:    text = "TASKS";    break;
    case M1_DIAG_BOOT_CONFIG:   text = "CONFIG";   break;
    case M1_DIAG_BOOT_SETTINGS: text = "SETTINGS"; break;
    case M1_DIAG_BOOT_MENU:     text = "MENU";     break;
    default:                    text = "UNKNOWN";  break;
    }
    snprintf(out, out_size, "LAST: %s", text);
}

#endif /* M1_DIAG_STATUS_H_ */
