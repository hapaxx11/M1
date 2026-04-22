/* Minimal FatFS stub for host-side unit tests.
 * Provides the types referenced by m1_sdcard.h and flipper_file.h.
 *
 * This stub wraps stdio so that tests can read/write actual files on the host
 * filesystem (e.g. temp files in /tmp).  Tests that do not call ff_open()
 * or f_open() are unaffected. */

#ifndef FF_H_STUB
#define FF_H_STUB

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Basic FatFS primitive types */
typedef unsigned int    UINT;
typedef unsigned char   BYTE;
typedef uint32_t        DWORD;
typedef uint64_t        QWORD;
typedef DWORD           LBA_t;
typedef DWORD           FSIZE_t;

/* Minimal FRESULT */
typedef enum {
	FR_OK = 0,
	FR_DISK_ERR = 1
} FRESULT;

/* ff.h config constant needed by ff_gen_drv.h */
#ifndef FF_VOLUMES
#define FF_VOLUMES  1
#endif

/* Minimal FATFS filesystem object (opaque for tests) */
typedef struct { int dummy; } FATFS;

/* FIL: wraps a stdio FILE* for host-side test I/O */
typedef struct {
	FILE *f;
} FIL;

/* FILINFO for directory listing (opaque for tests) */
typedef struct {
	int dummy;
} FILINFO;

/* Mode flags referenced in flipper_file.c */
#define FA_READ          0x01
#define FA_WRITE         0x02
#define FA_CREATE_ALWAYS 0x08
#define FA_OPEN_EXISTING 0x00

static inline FRESULT f_open(FIL *fp, const char *path, unsigned char mode)
{
	if (!fp || !path) return FR_DISK_ERR;
	const char *m = ((mode & FA_WRITE) || (mode & FA_CREATE_ALWAYS)) ? "w" : "r";
	fp->f = fopen(path, m);
	return fp->f ? FR_OK : FR_DISK_ERR;
}

static inline FRESULT f_close(FIL *fp)
{
	if (fp && fp->f) { fclose(fp->f); fp->f = NULL; }
	return FR_OK;
}

static inline char *f_gets(char *buf, int len, FIL *fp)
{
	if (!fp || !fp->f) return NULL;
	return fgets(buf, len, fp->f);
}

static inline int f_puts(const char *str, FIL *fp)
{
	if (!fp || !fp->f) return -1;
	return fputs(str, fp->f) >= 0 ? (int)strlen(str) : -1;
}

static inline int f_printf(FIL *fp, const char *fmt, ...)
{
	if (!fp || !fp->f) return -1;
	va_list ap;
	va_start(ap, fmt);
	int r = vfprintf(fp->f, fmt, ap);
	va_end(ap);
	return r;
}

#endif /* FF_H_STUB */
