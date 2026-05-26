/* Minimal FreeRTOS.h stub for host-side unit tests. */
#ifndef FREERTOS_H_STUB
#define FREERTOS_H_STUB

#include <stdint.h>
#include <stddef.h>

typedef uint32_t TickType_t;
typedef void *   TaskHandle_t;

#ifndef portMAX_DELAY
#define portMAX_DELAY 0xFFFFFFFF
#endif

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(x) (x)
#endif

/* Map FreeRTOS heap API to libc so host-side tests compile and run
 * against firmware source that uses pvPortMalloc / vPortFree. */
#include <stdlib.h>
#ifndef pvPortMalloc
#define pvPortMalloc(sz) malloc(sz)
#endif
#ifndef vPortFree
#define vPortFree(p)     free(p)
#endif

#endif /* FREERTOS_H_STUB */
