/* Minimal ff_gen_drv.h stub for host-side unit tests.
 * Provides the driver registration types used by m1_sdcard.h. */

#ifndef FF_GEN_DRV_H_STUB
#define FF_GEN_DRV_H_STUB

#include "diskio.h"
#include "ff.h"
#include <stdint.h>

typedef struct
{
    DSTATUS (*disk_initialize)(BYTE);
    DSTATUS (*disk_status)    (BYTE);
    DRESULT (*disk_read)      (BYTE, BYTE *, DWORD, UINT);
    DRESULT (*disk_write)     (BYTE, const BYTE *, DWORD, UINT);
    DRESULT (*disk_ioctl)     (BYTE, BYTE, void *);
} Diskio_drvTypeDef;

typedef struct
{
    uint8_t                  is_initialized[FF_VOLUMES];
    const Diskio_drvTypeDef *drv[FF_VOLUMES];
    uint8_t                  lun[FF_VOLUMES];
    volatile uint8_t         nbr;
} Disk_drvTypeDef;

static inline uint8_t FATFS_LinkDriver(const Diskio_drvTypeDef *d, char *p)   { (void)d; (void)p; return 0; }
static inline uint8_t FATFS_UnLinkDriver(char *p)                              { (void)p; return 0; }
static inline uint8_t FATFS_GetAttachedDriversNbr(void)                        { return 0; }

#endif /* FF_GEN_DRV_H_STUB */
