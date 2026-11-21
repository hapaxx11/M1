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

void m1_ringbuffer_init(S_M1_RingBuffer *prb_handle, uint8_t *ring_buffer, uint16_t n_elements, uint8_t data_size);
void m1_ringbuffer_reset(S_M1_RingBuffer *prb_handle);
uint16_t m1_ringbuffer_get_read_len(S_M1_RingBuffer *prb_handle);
uint8_t *m1_ringbuffer_get_read_address(S_M1_RingBuffer *prb_handle);
uint16_t m1_ringbuffer_write(S_M1_RingBuffer *prb_handle, uint8_t *indata, uint16_t n_slots);
uint16_t m1_ringbuffer_insert(S_M1_RingBuffer *prb_handle, uint8_t *indata);
uint16_t m1_ringbuffer_read(S_M1_RingBuffer *prb_handle, uint8_t *outdata, uint16_t n_slots);
uint16_t m1_ringbuffer_peek(S_M1_RingBuffer *prb_handle, uint8_t *outdata, uint16_t n_slots);
uint16_t m1_ringbuffer_advance_read(S_M1_RingBuffer *prb_handle, uint16_t n_slots);
uint8_t m1_ringbuffer_check_empty_state(S_M1_RingBuffer *prb_handle);
uint32_t ringbuffer_get_empty_slots(S_M1_RingBuffer *prb_handle);
uint32_t ringbuffer_get_data_slots(S_M1_RingBuffer *prb_handle);

#endif /* M1_RING_BUFFER_H_STUB */
