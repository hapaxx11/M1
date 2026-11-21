/* See COPYING.txt for license details. */

/*
*
* m1_ring_buffer.h
*
* Header for library for ring buffer
*
* M1 Project
*
*/

#ifndef M1_RING_BUFFER_H_
#define M1_RING_BUFFER_H_

typedef struct
{
    uint8_t *pdata; // Pointer to the buffer
    uint32_t end_index; // Last index in the buffer
    uint32_t len; // Length of buffer data
    uint8_t data_size; // Data size
    volatile uint32_t tail; // Index in the buffer for reading from
    volatile uint32_t head; // Index in the buffer for for writing to
} S_M1_RingBuffer;

extern S_M1_RingBuffer esp32_rb_hdl;

void m1_ringbuffer_init(S_M1_RingBuffer *prb_handle, uint8_t *ring_buffer, uint16_t n_elements, uint8_t data_size);
void m1_ringbuffer_reset(S_M1_RingBuffer *prb_handle);
uint16_t m1_ringbuffer_get_read_len(S_M1_RingBuffer *prb_handle);
uint8_t *m1_ringbuffer_get_read_address(S_M1_RingBuffer *prb_handle);
uint16_t m1_ringbuffer_write(S_M1_RingBuffer *prb_handle, uint8_t *indata, uint16_t n_slots);
uint16_t m1_ringbuffer_insert(S_M1_RingBuffer *prb_handle, uint8_t *indata);
uint16_t m1_ringbuffer_read(S_M1_RingBuffer *prb_handle, uint8_t *outdata, uint16_t bytes_n);
uint16_t m1_ringbuffer_peek(S_M1_RingBuffer *prb_handle, uint8_t *outdata, uint16_t n_slots);
uint16_t m1_ringbuffer_advance_read(S_M1_RingBuffer *prb_handle, uint16_t bytes_n);
uint8_t m1_ringbuffer_check_empty_state(S_M1_RingBuffer *prb_handle);
uint32_t ringbuffer_get_empty_slots(S_M1_RingBuffer *prb_handle);
uint32_t ringbuffer_get_data_slots(S_M1_RingBuffer *prb_handle);

#endif /* M1_RING_BUFFER_H_ */
