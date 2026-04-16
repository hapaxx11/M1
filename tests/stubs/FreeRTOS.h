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

#endif /* FREERTOS_H_STUB */
