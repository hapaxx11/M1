/* See COPYING.txt for license details. */

/*
*
*  m1_can.h
*
*  M1 CAN bus (FDCAN1) functions — CAN Commander
*
*  Requires external CAN transceiver on J7 (X10) connector.
*  Recommended: Waveshare SN65HVD230 CAN Board (3.3 V).
*
* M1 Project
*
*/

#ifndef M1_CAN_H_
#define M1_CAN_H_

#include <stdint.h>
#include <stdbool.h>
#include "stm32h5xx_hal.h"
#include "m1_compile_cfg.h"

#ifdef M1_APP_CAN_ENABLE

/*============================================================================*/
/*                         D E F I N E S                                      */
/*============================================================================*/

#define CAN_MSG_BUF_SIZE        64   /* Circular buffer depth for sniffer      */
#define CAN_DLC_MAX             8    /* Classic CAN max payload                */

/* Supported baud rates (index into can_baud_table[]) */
typedef enum {
    CAN_BAUD_125K = 0,
    CAN_BAUD_250K,
    CAN_BAUD_500K,
    CAN_BAUD_1M,
    CAN_BAUD_NUM
} m1_can_baud_t;

/*============================================================================*/
/*                     S T R U C T U R E S                                    */
/*============================================================================*/

/* One captured CAN frame */
typedef struct {
    uint32_t    id;                    /* Standard or extended arbitration ID   */
    uint8_t     dlc;                   /* Data length code (0-8)               */
    uint8_t     data[CAN_DLC_MAX];     /* Payload bytes                        */
    bool        is_extended;           /* true = 29-bit ID, false = 11-bit     */
    bool        is_rtr;                /* Remote transmission request flag     */
    uint32_t    timestamp;             /* Tick count when received             */
} m1_can_msg_t;

/*============================================================================*/
/*               F U N C T I O N   P R O T O T Y P E S                       */
/*============================================================================*/

/* Menu entry points (called from m1_menu.c via subfunc_handler_task) */
void can_sniffer(void);
void can_send(void);
void can_saved(void);

/* Menu init/deinit hooks */
void menu_can_init(void);
void menu_can_deinit(void);

/* Low-level helpers */
bool m1_can_start(m1_can_baud_t baud);
void m1_can_stop(void);
bool m1_can_transmit(uint32_t id, bool extended, const uint8_t *data, uint8_t dlc);

#endif /* M1_APP_CAN_ENABLE */

#endif /* M1_CAN_H_ */
