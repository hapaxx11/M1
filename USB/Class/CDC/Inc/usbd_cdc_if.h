/**
  ******************************************************************************
  * @file    usbd_cdc_if_template.h
  * @author  MCD Application Team
  * @brief   Header for usbd_cdc_if_template.c file.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2015 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_CDC_IF_TEMPLATE_H
#define __USBD_CDC_IF_TEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/

extern USBD_CDC_ItfTypeDef  USBD_CDC_Interface_fops;

/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */
uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);

/* Abort a stuck IN transfer (flush EP + clear TxState) so a shared TX buffer is
 * safe to reuse after the host stops reading. Guards NULL pClassData. */
void CDC_TxAbort(void);

/* True while the IN endpoint still owns a previous transfer (TxState != 0).
 * Guards NULL pClassData (returns not-busy when not yet enumerated). */
uint8_t CDC_Transmit_Busy(void);

/* Re-arm the CDC OUT (RX) endpoint from task context, forcing the CDC instance
 * on a composite device. hUsbDeviceFS.classId is mutable and shared with the
 * MSC class; a prior MSC transfer can leave it pointing at MSC, so re-arming
 * without forcing the CDC instance can arm the wrong endpoint and leave the
 * serial port deaf. Returns USBD_OK only when the rearm was accepted; callers
 * must clear any "paused" flag ONLY on USBD_OK. */
uint8_t CDC_RearmRx(void);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CDC_IF_TEMPLATE_H */

