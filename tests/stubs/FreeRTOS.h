/* Minimal FreeRTOS.h stub for host-side unit tests. */
#ifndef FREERTOS_H_STUB
#define FREERTOS_H_STUB

#include <stdint.h>
#include <stddef.h>

typedef uint32_t  TickType_t;
typedef void *    TaskHandle_t;
typedef long      BaseType_t;
typedef void *    SemaphoreHandle_t;

#define pdTRUE    ((BaseType_t)1)
#define pdFALSE   ((BaseType_t)0)
#define pdPASS    pdTRUE
#define pdFAIL    pdFALSE

#ifndef portMAX_DELAY
#define portMAX_DELAY ((TickType_t)0xFFFFFFFF)
#endif

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#endif

/* Map FreeRTOS heap API to libc so host-side tests compile and run
 * against firmware source that uses pvPortMalloc / vPortFree. */
#include <stdlib.h>
#include <string.h>
#ifndef pvPortMalloc
#define pvPortMalloc(sz) malloc(sz)
#endif
#ifndef vPortFree
#define vPortFree(p)     free(p)
#endif
#ifndef pvPortCalloc
#define pvPortCalloc(n, sz) calloc((n), (sz))
#endif
#ifndef pvPortRealloc
#define pvPortRealloc(p, sz) realloc((p), (sz))
#endif

/* Scheduler / task stubs: no-ops on the host. */
static inline void vTaskDelay(TickType_t t) { (void)t; }

/* Semaphore stubs: single-element flag backed by malloc. */
#include <stdbool.h>
static inline SemaphoreHandle_t xSemaphoreCreateBinary(void)
{
    bool *p = (bool *)malloc(sizeof(bool));
    if (p) *p = false;
    return (SemaphoreHandle_t)p;
}
static inline void vSemaphoreDelete(SemaphoreHandle_t s)  { free(s); }
static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t s)
{
    if (s) { *(bool *)s = true; return pdTRUE; }
    return pdFALSE;
}
static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t t)
{
    (void)t;
    if (s && *(bool *)s) { *(bool *)s = false; return pdTRUE; }
    return pdFALSE;
}

/* Task creation stub: immediately executes the task function synchronously
 * (no pre-emption on host), then returns pdPASS. */
typedef void (*TaskFunction_t)(void *);
static inline BaseType_t xTaskCreate(TaskFunction_t fn, const char *name,
                                     uint16_t stack, void *param,
                                     uint32_t prio, TaskHandle_t *handle)
{
    (void)name; (void)stack; (void)prio;
    if (handle) *handle = NULL;
    if (fn) fn(param);
    return pdPASS;
}
static inline void vTaskDelete(TaskHandle_t h) { (void)h; }

#endif /* FREERTOS_H_STUB */
