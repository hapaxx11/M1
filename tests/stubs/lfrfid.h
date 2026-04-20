/* Minimal lfrfid.h stub for host-side unit tests.
 * Provides only the types and constants needed by lfrfid_manchester.h
 * and flipper_rfid.h — no actual RTOS or HAL dependencies. */

#ifndef LFRFID_H_STUB
#define LFRFID_H_STUB

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Edge types for LF RFID timing events */
typedef enum {
	LFRFID_EDGE_UNKNOWN = 0,
	LFRFID_EDGE_RISE,
	LFRFID_EDGE_FALL
} lfrfid_edge_t;

/* Timing event from the capture timer ISR.
 * Must match lfrfid/lfrfid.h exactly — edge is uint16_t in production. */
typedef struct {
	uint16_t t_us;   /* period in microseconds */
	uint16_t edge;   /* 0=falling, 1=rising    */
} lfrfid_evt_t;

/* Batch size for edge buffering */
#define LFR_BATCH_ITEMS   60
#define FRAME_CHUNK_SIZE  (LFR_BATCH_ITEMS)

/* Tag info placeholder (not used by manchester/parser tests) */
typedef struct {
	uint8_t  uid[5];
	uint8_t  protocol;
	uint8_t  bitrate;
	uint8_t  modulation;
	uint8_t  encoding;
	uint8_t  card_format;
	uint8_t  status;
	char     filename[32];
	char     filepath[64];
} LFRFID_TAG_INFO;
typedef LFRFID_TAG_INFO* PLFRFID_TAG_INFO;

/* Include protocol enum definitions */
#include "lfrfid_protocol_stub.h"

#endif /* LFRFID_H_STUB */
