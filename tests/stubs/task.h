/* Minimal task.h stub for host-side unit tests.
 * Provides no-op scheduler suspend/resume so esp_queue.c compiles
 * without pulling in the full FreeRTOS kernel.
 */
#ifndef TASK_H_STUB
#define TASK_H_STUB

static inline void vTaskSuspendAll(void) {}
static inline int  xTaskResumeAll(void)  { return 1; }

#endif /* TASK_H_STUB */
