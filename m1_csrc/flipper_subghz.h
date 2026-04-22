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
	/* Set to true when the file was loaded from an M1 native .sgh format
	 * (not a Flipper .sub file).  Used to select the direct replay path
	 * which feeds the original file into the streaming engine without any
	 * temp-file conversion. */
	bool     is_m1_native;
} flipper_subghz_signal_t;

/*
 * Lightweight header probe — reads only the first few header lines of a
 * .sgh/.sub file to determine format and extract frequency/modulation without
 * loading any raw sample data.  Used by the playlist scene to dispatch each
 * playlist entry without the 16 KB overhead of a full flipper_subghz_signal_t.
 */
typedef struct {
	bool    is_m1_native;   /* true = M1 native .sgh format */
	bool    is_noise;       /* true = RAW/NOISE recording, false = PACKET/Key */
	uint32_t frequency;     /* Hz, from header */
	uint8_t modulation;     /* MODULATION_OOK / MODULATION_FSK / MODULATION_ASK */
} flipper_subghz_probe_t;

/* Probe just the header of a .sgh/.sub file (no Data: lines loaded).
 * Returns false if the file cannot be opened or frequency is missing. */
bool flipper_subghz_probe(const char *path, flipper_subghz_probe_t *out);

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

/*
 * Emulate replay path selector — pure dispatch logic.
 *
 * Encoding the decision as a named function (rather than an inline
 * comparison) makes the dispatch gate testable in isolation, documents
 * the contract precisely, and prevents silent regressions: if anyone
 * changes the dispatch rule the corresponding unit test will fail.
 *
 * DIRECT   → sub_ghz_replay_datafile()    — feeds the original .sgh file
 *             straight into the streaming engine; no conversion, no temp file.
 *             Used for M1 native NOISE recordings from Hapax, C3.12, SiN360.
 *
 * CONVERT  → sub_ghz_replay_flipper_file() — converts to temp .sgh before
 *             streaming.  Used for Flipper .sub RAW files (need sign-strip)
 *             and all PACKET/key files (need the key encoder path).
 */
typedef enum {
	FLIPPER_SUBGHZ_EMULATE_DIRECT,  /* sub_ghz_replay_datafile()      */
	FLIPPER_SUBGHZ_EMULATE_CONVERT, /* sub_ghz_replay_flipper_file()  */
} flipper_subghz_emulate_path_t;

/**
 * @brief  Choose the replay path for a loaded signal.
 *
 * @param  is_raw       true when the loaded file is a RAW/NOISE recording
 *                      (FLIPPER_SUBGHZ_TYPE_RAW), false for PACKET/key files.
 * @param  is_m1_native true when flipper_subghz_load() set is_m1_native
 *                      (M1 native .sgh format), false for Flipper .sub files.
 * @return DIRECT when the file can stream without conversion; CONVERT otherwise.
 */
static inline flipper_subghz_emulate_path_t
flipper_subghz_emulate_path(bool is_raw, bool is_m1_native)
{
	/* Only M1 native NOISE files skip conversion.
	 * PACKET files (even native) always need the key encoder path.
	 * Flipper .sub RAW files always need the sign-strip conversion. */
	return (is_raw && is_m1_native)
	       ? FLIPPER_SUBGHZ_EMULATE_DIRECT
	       : FLIPPER_SUBGHZ_EMULATE_CONVERT;
}

#endif /* FLIPPER_SUBGHZ_H_ */
