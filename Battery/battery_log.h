/* See COPYING.txt for license details. */

/*
*
* battery_log.h
*
* Battery log functions for M1
*
* M1 Project
*
*/

#ifndef BATTERY_LOG_H_
#define BATTERY_LOG_H_

#include <stdbool.h>
#include <stdint.h>
#include "ff.h"
#include "m1_bq27421.h"
#include "battery.h"

void        battery_log_Enable(bool ctrl);
bool        battery_log_get(void);
bool        battery_log_OpenNewFile(const S_M1_Power_Status_t *log);
bool        battery_log_Save(const S_M1_Power_Status_t *log);
void        battery_log_Close(void);
const char *battery_log_GetFileName(void);

int WriteOneBlock(FIL *fp, const BQ27421_DataBlock *pBlock);
int WriteGoldenImage(const char *filename, const BQ27421_DataBlock *table, uint16_t count);

#endif /* BATTERY_LOG_H_ */
