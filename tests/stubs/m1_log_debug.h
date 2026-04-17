/* Minimal m1_log_debug.h stub for host-side unit tests.
 *
 * Several protocol decoders include m1_log_debug.h for the M1_LOGDB_TAG
 * macro even though they never call any logging function.  This stub
 * satisfies those includes without pulling in the real UART/DMA/CMSIS
 * dependencies from the target firmware.
 */

#ifndef M1_LOG_DEBUG_H_STUB
#define M1_LOG_DEBUG_H_STUB

#include <stdint.h>

/* Minimal level type used by the real m1_logdb_printf declaration */
typedef enum {
    LOG_DEBUG_LEVEL_NONE  = 0,
    LOG_DEBUG_LEVEL_ERROR = 1,
    LOG_DEBUG_LEVEL_WARN  = 2,
    LOG_DEBUG_LEVEL_INFO  = 3,
    LOG_DEBUG_LEVEL_DEBUG = 4,
    LOG_DEBUG_LEVEL_TRACE = 5,
} S_M1_LogDebugLevel_t;

/* Suppress unused-attribute warning on GCC/Clang */
#ifdef __GNUC__
#  define _ATTRIBUTE(x) __attribute__(x)
#else
#  define _ATTRIBUTE(x)
#endif

/* No-op logging macros — no output, no side effects */
#define M1_LOG_N(tag, format, ...)  ((void)0)
#define M1_LOG_E(tag, format, ...)  ((void)0)
#define M1_LOG_W(tag, format, ...)  ((void)0)
#define M1_LOG_I(tag, format, ...)  ((void)0)
#define M1_LOG_D(tag, format, ...)  ((void)0)
#define M1_LOG_T(tag, format, ...)  ((void)0)

/* Satisfy the m1_logdb_printf declaration in the real header without
 * defining the full UART/DMA machinery.  Decoders that include this stub
 * but never call M1_LOG_* will never reference this symbol, so the linker
 * won't complain even if no definition is provided. */
void m1_logdb_printf(S_M1_LogDebugLevel_t level, const char *tag,
                     const char *format, ...)
    _ATTRIBUTE((__format__(__printf__, 3, 4)));

#endif /* M1_LOG_DEBUG_H_STUB */
