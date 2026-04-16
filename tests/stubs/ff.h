/* Minimal FatFS stub for host-side unit tests.
 * Provides the types referenced by m1_sdcard.h and flipper_file.h. */

#ifndef FF_H_STUB
#define FF_H_STUB

#include <stdint.h>
#include <stddef.h>

/* Basic FatFS primitive types */
typedef unsigned int    UINT;
typedef unsigned char   BYTE;
typedef uint32_t        DWORD;
typedef uint64_t        QWORD;
typedef DWORD           LBA_t;
typedef DWORD           FSIZE_t;

/* Minimal FRESULT */
typedef enum {
	FR_OK = 0
} FRESULT;

/* ff.h config constant needed by ff_gen_drv.h */
#ifndef FF_VOLUMES
#define FF_VOLUMES  1
#endif

/* Minimal FATFS filesystem object (opaque for tests) */
typedef struct { int dummy; } FATFS;

/* Minimal FIL (opaque for tests) */
typedef struct {
	int dummy;
} FIL;

/* FILINFO for directory listing (opaque for tests) */
typedef struct {
	int dummy;
} FILINFO;

/* Stub declarations so flipper_file.c compiles.
 * These are never called by the utility functions under test. */
static inline FRESULT f_open(FIL *fp, const char *path, unsigned char mode) { (void)fp; (void)path; (void)mode; return FR_OK; }
static inline FRESULT f_close(FIL *fp) { (void)fp; return FR_OK; }
static inline char *f_gets(char *buf, int len, FIL *fp) { (void)buf; (void)len; (void)fp; return NULL; }
static inline int f_printf(FIL *fp, const char *fmt, ...) { (void)fp; (void)fmt; return 0; }

/* Mode flags referenced in flipper_file.c */
#define FA_READ         0x01
#define FA_WRITE        0x02
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_EXISTING 0x00

#endif /* FF_H_STUB */
