/* Minimal stream_buffer.h stub for host-side unit tests. */
#ifndef STREAM_BUFFER_H_STUB
#define STREAM_BUFFER_H_STUB

#include <stdint.h>

typedef void* StreamBufferHandle_t;
typedef void* QueueHandle_t;

/* portMAX_DELAY used by lfrfid.h */
#define portMAX_DELAY 0xFFFFFFFF
#define pdMS_TO_TICKS(x) (x)

#endif /* STREAM_BUFFER_H_STUB */
