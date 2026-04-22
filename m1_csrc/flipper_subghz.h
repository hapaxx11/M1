/* See COPYING.txt for license details. */

/*
 * flipper_subghz.h
 *
 * Flipper Zero .sub file format parser for Sub-GHz signals
 *
 * M1 Project
 */

#ifndef FLIPPER_SUBGHZ_H_
#define FLIPPER_SUBGHZ_H_

#include "flipper_file.h"

#define FLIPPER_SUBGHZ_RAW_MAX_SAMPLES   8192
#define FLIPPER_SUBGHZ_PROTO_MAX_LEN     32
#define FLIPPER_SUBGHZ_PRESET_MAX_LEN    48

typedef enum {
	FLIPPER_SUBGHZ_TYPE_RAW = 0,
	FLIPPER_SUBGHZ_TYPE_PARSED
} flipper_subghz_type_t;

typedef struct {
	flipper_subghz_type_t type;
	uint32_t frequency;                 /* Hz, e.g. 433920000 */
	char     preset[FLIPPER_SUBGHZ_PRESET_MAX_LEN];
	char     protocol[FLIPPER_SUBGHZ_PROTO_MAX_LEN];
	/* For parsed protocols */
	uint32_t bit_count;
	uint64_t key;
	uint32_t te;                        /* Timing element */
	/* For raw data */
	int16_t  raw_data[FLIPPER_SUBGHZ_RAW_MAX_SAMPLES];
	uint16_t raw_count;
} flipper_subghz_signal_t;

/* Load a .sub file (Flipper format) or M1 native .sgh file */
bool flipper_subghz_load(const char *path, flipper_subghz_signal_t *out);

/* Save a Flipper-compatible .sub file */
bool flipper_subghz_save(const char *path, const flipper_subghz_signal_t *sig);

/* Save an M1 native .sgh PACKET file */
bool flipper_subghz_save_m1native(const char *path, const flipper_subghz_signal_t *sig);

/*
 * Lightweight key-only save helpers — write a parsed (decoded) signal directly
 * from its individual fields without requiring a flipper_subghz_signal_t on the
 * caller's stack.  Use these in any save path that deals with key/protocol
 * signals (never RAW); doing so avoids a ~16 KB stack or BSS allocation.
 */
bool flipper_subghz_save_key(const char *path, uint32_t frequency,
                              const char *preset, const char *protocol,
                              uint32_t bit_count, uint64_t key, uint32_t te);

bool flipper_subghz_save_m1native_key(const char *path, uint32_t frequency,
                                       const char *preset, const char *protocol,
                                       uint32_t bit_count, uint64_t key, uint32_t te);

/* Map Flipper preset name to M1 modulation type */
uint8_t flipper_subghz_preset_to_modulation(const char *preset);

/* Convert frequency in Hz to closest M1 SI4463 band */
uint8_t flipper_subghz_freq_to_band(uint32_t freq_hz);

/* Pure-logic helper: returns true if filetype/version header strings indicate
 * an M1 native .sgh file (as opposed to a Flipper .sub file).
 * Exposed for unit testing without file I/O dependencies. */
bool flipper_subghz_is_m1_native_header(const char *filetype_val,
                                         const char *version_val);

#endif /* FLIPPER_SUBGHZ_H_ */
