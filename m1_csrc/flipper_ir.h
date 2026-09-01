/* See COPYING.txt for license details. */

/*
 * flipper_ir.h
 *
 * Flipper Zero .ir file format parser for IR signals
 *
 * M1 Project
 */

#ifndef FLIPPER_IR_H_
#define FLIPPER_IR_H_

#include "flipper_file.h"
#include "irmp.h"
#include "ir_signal_record.h"

#define FLIPPER_IR_RAW_MAX_SAMPLES  512
#define FLIPPER_IR_NAME_MAX_LEN     32

typedef enum {
	FLIPPER_IR_SIGNAL_PARSED = 0,
	FLIPPER_IR_SIGNAL_RAW
} flipper_ir_signal_type_t;

typedef struct {
	char name[FLIPPER_IR_NAME_MAX_LEN];
	flipper_ir_signal_type_t type;
	bool valid;
	union {
		struct {
			uint8_t  protocol;     /* IRMP protocol ID */
			uint16_t address;
			uint16_t command;
			uint8_t  flags;
		} parsed;
		struct {
			uint32_t frequency;    /* Hz, e.g. 38000 */
			float    duty_cycle;   /* e.g. 0.33 */
			int32_t  samples[FLIPPER_IR_RAW_MAX_SAMPLES];
			uint16_t sample_count;
		} raw;
	};
} flipper_ir_signal_t;

/* Open a .ir file and validate header */
bool flipper_ir_open(flipper_file_t *ctx, const char *path);

/* Read next signal from an open .ir file. Returns false at EOF */
bool flipper_ir_read_signal(flipper_file_t *ctx, flipper_ir_signal_t *out);

/**
 * Parse one IR signal block using an ir_block_reader_t vtable.
 *
 * This is the FatFS-free extraction of the flipper_ir_read_signal() body.
 * Any source (FatFS file, in-memory string, …) can supply the key-value
 * pairs by providing an ir_block_reader_t adapter.
 *
 * @param ops  Non-NULL reader vtable.
 * @param ctx  Opaque context pointer forwarded to every vtable call.
 * @param out  Output signal structure; zeroed on entry.
 * @return true if a valid signal was parsed, false at EOF or error.
 */
bool flipper_ir_parse_block(const ir_block_reader_t *ops, void *ctx, flipper_ir_signal_t *out);

/* Write a .ir file header */
bool flipper_ir_write_header(flipper_file_t *ctx);

/* Write a signal to .ir file */
bool flipper_ir_write_signal(flipper_file_t *ctx, const flipper_ir_signal_t *sig);

/* Map Flipper protocol name string to IRMP protocol ID */
uint8_t flipper_ir_proto_to_irmp(const char *name);

/* Map IRMP protocol ID to Flipper protocol name string */
const char *flipper_ir_irmp_to_proto(uint8_t irmp_id);

/* Count signals in a .ir file without loading them all */
uint16_t flipper_ir_count_signals(const char *path);

/**
 * @brief  Rename one signal (by index) in-place.
 *
 * Streams the file through a temp copy, changing the name of the signal
 * at @p idx.  The original file is replaced atomically.
 *
 * @param path      Full FatFS path to the .ir file (e.g. "0:/IR/Custom/tv.ir").
 * @param idx       Zero-based index of the signal to rename.
 * @param new_name  New name string; must be non-empty, ≤ FLIPPER_IR_NAME_MAX_LEN-1 chars.
 * @retval true   on success.
 * @retval false  if @p idx is out of range, or any I/O error occurs.
 */
bool flipper_ir_rename_signal(const char *path, uint16_t idx, const char *new_name);

/**
 * @brief  Delete one signal (by index) in-place.
 *
 * Streams the file through a temp copy, omitting the signal at @p idx.
 * The original file is replaced atomically.
 *
 * @param path  Full FatFS path to the .ir file.
 * @param idx   Zero-based index of the signal to delete.
 * @retval true   on success.
 * @retval false  if @p idx is out of range, or any I/O error occurs.
 */
bool flipper_ir_delete_signal(const char *path, uint16_t idx);

/**
 * @brief  Append one signal to a .ir file.
 *
 * Streams the existing content through a temp copy then appends @p sig.
 * The original file is replaced atomically.  Works on empty files (header
 * only) as well as files that already contain signals.
 *
 * @param path  Full FatFS path to the .ir file.
 * @param sig   Signal to append; must be valid (sig->valid == true).
 * @retval true   on success.
 * @retval false  on any I/O error.
 */
bool flipper_ir_append_signal(const char *path, const flipper_ir_signal_t *sig);

/* ---- Raw-signal accumulator -------------------------------------------- */

/**
 * @brief  Accumulator that builds a RAW-type flipper_ir_signal_t from
 *         successive mark/space edge durations captured by the IR hardware.
 *
 * Usage:
 *   flipper_ir_raw_feed_t f;
 *   flipper_ir_raw_feed_init(&f, "Power", 38000, 0.33f);
 *   while (edge_available())
 *       flipper_ir_raw_feed_push(&f, next_edge_us());
 *   if (flipper_ir_raw_feed_finish(&f))
 *       flipper_ir_write_signal(&ff, &f.sig);
 */
typedef struct {
    flipper_ir_signal_t sig;      /**< Output signal being assembled. */
    bool                overflow; /**< Set when sample_count exceeded capacity. */
} flipper_ir_raw_feed_t;

/** Initialise the accumulator (must be called before push/finish). */
void flipper_ir_raw_feed_init(flipper_ir_raw_feed_t *f, const char *name,
                               uint32_t freq_hz, float duty_cycle);

/**
 * @brief  Push one mark/space sample (µs, positive = mark, negative = space).
 * @retval true   sample accepted.
 * @retval false  overflow — sample count exceeded FLIPPER_IR_RAW_MAX_SAMPLES.
 */
bool flipper_ir_raw_feed_push(flipper_ir_raw_feed_t *f, int32_t sample_us);

/**
 * @brief  Finalise the accumulator and mark the signal as valid.
 * @retval true   signal is valid and ready to write.
 * @retval false  accumulator is empty, overflowed, or invalid.
 */
bool flipper_ir_raw_feed_finish(flipper_ir_raw_feed_t *f);

#endif /* FLIPPER_IR_H_ */
