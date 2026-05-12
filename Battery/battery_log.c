/* See COPYING.txt for license details. */

/*
*
* battery_log.c
*
* Battery log functions for M1
*
* M1 Project
*
*/

#include "ff.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "m1_bq27421.h"
#include "battery.h"
#include "battery_log.h"

#define LOG_FILE_PREFIX   "0:battery_log_"
#define LOG_FILE_EXT      ".csv"
#define LOG_FILE_MAX_IDX  1000

static FIL  s_logFile;
static bool s_logOpened = false;
static char s_logFileName[64] = {0};
static int  s_logCount = 0;
static bool s_logENable = false;

void battery_log_Enable(bool ctrl)
{
        s_logENable = ctrl;
}

bool battery_log_get(void)
{
        return s_logENable;
}

static bool battery_log_GenerateFileName(char *out, size_t len)
{
    FILINFO fno;

    for (int i = 0; i < LOG_FILE_MAX_IDX; i++)
    {
        snprintf(out, len, "%s%03d%s", LOG_FILE_PREFIX, i, LOG_FILE_EXT);

        FRESULT fr = f_stat(out, &fno);
        if (fr == FR_NO_FILE || fr == FR_NO_PATH)
        {
            return true;
        }
        if (fr != FR_OK)
        {
            /* SD error (not-ready, disk error, etc.) — abort search */
            return false;
        }
    }

    return false;
}

static FRESULT battery_log_WriteHeader(FIL *fp, const S_M1_Power_Status_t *log)
{
	char line[240];
    UINT bw;

    const char *header =
        "No,CtrlStatus,Flags,Temperature,Voltage,RemCap,FullChgCap,AvgCurrent,SOHStat,SOH,SOC\r\n";

    int len = snprintf(line, sizeof(line),
                       "Design Capacity = %d, Period time 2sec \r\n\r\n", log->designCapacity_mAh);
    if (len <= 0 || len >= (int)sizeof(line))
        return FR_INT_ERR;

    size_t header_len = strlen(header);
    if ((size_t)len + header_len >= sizeof(line))
        return FR_INT_ERR;
    memcpy(line + len, header, header_len + 1u); /* +1 copies the NUL */

    s_logCount = 1;

    size_t total = (size_t)len + header_len;
    FRESULT fr = f_write(fp, line, (UINT)total, &bw);
    if (fr != FR_OK)
        return fr;
    if (bw != (UINT)total)
        return FR_DENIED;
    return FR_OK;
}

static FRESULT battery_log_WriteLine(FIL *fp, const S_M1_Power_Status_t *log)
{
    char line[160];
    UINT bw;

    int len = snprintf(line, sizeof(line),
                       "%u,0x%04X,0x%04X,%u,%d,%u,%u,%+d,%u,%u,%u\r\n",
                       (unsigned)s_logCount++,
                       log->status,
                       log->flags,
                       log->battery_temp,
                       (int)(log->battery_voltage),
                       log->remainingCapacity_mAh,
                       log->fullChargeCapacity_mAh,
                       log->consumption_current,
                       log->soh_state,
                       log->battery_health,
                       log->battery_level);

    if (len <= 0 || len >= (int)sizeof(line))
        return FR_INT_ERR;

    FRESULT fr = f_write(fp, line, (UINT)len, &bw);
    if (fr != FR_OK)
        return fr;
    if (bw != (UINT)len)
        return FR_DENIED;
    return FR_OK;
}

bool battery_log_OpenNewFile(const S_M1_Power_Status_t *log)
{
    FRESULT fr;

    if (log == NULL)
        return false;

    if (s_logOpened)
        return true;

    if (!battery_log_GenerateFileName(s_logFileName, sizeof(s_logFileName)))
        return false;

    fr = f_open(&s_logFile, s_logFileName, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
        return false;

    fr = battery_log_WriteHeader(&s_logFile, log);
    if (fr != FR_OK)
    {
        f_close(&s_logFile);
        return false;
    }

    f_sync(&s_logFile);
    s_logOpened = true;
    return true;
}

bool battery_log_Save(const S_M1_Power_Status_t *log)
{
    FRESULT fr;

    if(s_logENable == false)
    	return false;

    if (log == NULL)
        return false;

    if (!s_logOpened)
    {
        if (!battery_log_OpenNewFile(log))
            return false;
    }

    fr = battery_log_WriteLine(&s_logFile, log);
    if (fr != FR_OK)
        return false;

    f_sync(&s_logFile);
    return true;
}

void battery_log_Close(void)
{
    if (!s_logOpened)
        return;

    f_sync(&s_logFile);
    f_close(&s_logFile);
    s_logOpened = false;
    s_logFileName[0] = '\0';
    s_logCount = 0;
}

const char *battery_log_GetFileName(void)
{
    return s_logFileName;
}


static FIL  s_giFile;

int WriteOneBlock(FIL *fp, const BQ27421_DataBlock *pBlock)
{
    int i;

    if ((fp == NULL) || (pBlock == NULL)) {
        return -1;
    }

    f_printf(fp, "    {0x%02X, 0x%02X, {\n        ", pBlock->subclass, pBlock->block);

    for (i = 0; i < 16; i++) {
    	f_printf(fp, "0x%02X", pBlock->data[i]);
        if (i < 15) {
            f_printf(fp, ",");
        }
    }

    f_printf(fp, ",\n        ");

    for (i = 16; i < 32; i++) {
    	f_printf(fp, "0x%02X", pBlock->data[i]);
        if (i < 31) {
        	f_printf(fp, ",");
        }
    }

    f_printf(fp, "\n    }},\n\n");

    return 0;
}

int WriteGoldenImage(const char *filename,
                     const BQ27421_DataBlock *table,
                     uint16_t count)
{
	FRESULT fr;
    uint16_t i;

    if (!filename || !table || count == 0) return -1;

    fr = f_open(&s_giFile, filename, FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
        return -1;

    f_printf(&s_giFile, "static const BQ27421_DataBlock bq27421_golden_image[] = {\n");

    for (i = 0; i < count; i++) {
        WriteOneBlock(&s_giFile, &table[i]);
    }

    f_printf(&s_giFile, "};\n");

    f_close(&s_giFile);
    return 0;
}
