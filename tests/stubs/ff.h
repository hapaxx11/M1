/* Minimal FatFS stub for host-side unit tests.
 * Only provides the types referenced by flipper_file.h — no actual
 * filesystem operations are available. */

#ifndef FF_H_STUB
#define FF_H_STUB

#include <stdint.h>
#include <stddef.h>

/* Minimal FRESULT */
typedef enum {
	FR_OK = 0
} FRESULT;

/* Minimal FIL (opaque for tests) */
typedef struct {
	int dummy;
} FIL;

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
