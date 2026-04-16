/* Minimal diskio.h stub for host-side unit tests.
 * Provides the types from diskio.h needed by ff_gen_drv.h and m1_sdcard.h. */

#ifndef DISKIO_H_STUB
#define DISKIO_H_STUB

#include "ff.h"

/* Disk status type (same as BYTE) */
typedef BYTE DSTATUS;

/* Disk result type */
typedef enum {
    RES_OK = 0,
    RES_ERROR,
    RES_WRPRT,
    RES_NOTRDY,
    RES_PARERR
} DRESULT;

/* Disk status bits */
#define STA_NOINIT   0x01
#define STA_NODISK   0x02
#define STA_PROTECT  0x04

#endif /* DISKIO_H_STUB */
