/* Minimal m1_ring_buffer.h stub for host-side unit tests.
 * Provides empty definitions so m1_sub_ghz.h compiles. */

#ifndef M1_RING_BUFFER_H_STUB
#define M1_RING_BUFFER_H_STUB

#include <stdint.h>

typedef struct {
	uint8_t *pdata;
	uint32_t end_index;
	uint32_t len;
	uint8_t data_size;
	volatile uint32_t tail;
	volatile uint32_t head;
} S_M1_RingBuffer;

#endif /* M1_RING_BUFFER_H_STUB */
