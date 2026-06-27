/* Minimal stub for logger.h used by host-side unit tests.
 * Replaces the real logger.h which includes rfal_platform.h (STM32 HAL).
 * mfc_crypto1.c includes logger.h but does not call any logging functions
 * from the pure-logic cipher core — only from mfc_auth() which is not tested
 * on the host. */
#ifndef LOGGER_H
#define LOGGER_H

#include <stddef.h>

#define LOGGER_ON  1
#define LOGGER_OFF 0

static inline void logUsartInit(void *husart) { (void)husart; }
static inline int  logUsart(const char *fmt, ...) { (void)fmt; return 0; }
static inline char *hex2Str(unsigned char *data, size_t len)
{
    static char hex_stub_buf[2] = {0};
    (void)data;
    (void)len;
    return hex_stub_buf;
}

#endif /* LOGGER_H */
