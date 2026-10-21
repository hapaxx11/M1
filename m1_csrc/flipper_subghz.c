/* See COPYING.txt for license details. */

/*
 * flipper_subghz.c
 *
 * Flipper Zero .sub file format parser for Sub-GHz signals
 *
 * M1 Project
 */

/*************************** I N C L U D E S **********************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "flipper_subghz.h"
#include "m1_sub_ghz.h"
#include "subghz_psa_bruteforce.h"
#include "subghz_renault_v1_bruteforce.h"

/*************************** D E F I N E S ************************************/

/* FLIPPER_SUBGHZ_RAW_FILETYPE is defined in flipper_subghz.h */
#define FLIPPER_SUBGHZ_KEY_FILETYPE      "Flipper SubGhz Key File"
#define FLIPPER_SUBGHZ_MIN_VERSION       1

/* M1 native .sgh format — filetype prefix and type keywords */
#define M1_SUBGHZ_FILETYPE_PREFIX        "M1 SubGHz"
#define M1_SUBGHZ_NOISE_TYPE             "NOISE"    /* RAW recording */
#define M1_SUBGHZ_PACKET_TYPE            "PACKET"   /* Decoded/key file */

/* Maximum number of int32 samples to parse from a single RAW_Data line */
#define SUBGHZ_LINE_SAMPLES_MAX          256

//************************** S T R U C T U R E S *******************************

/***************************** V A R I A B L E S ******************************/

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

static int subghz_strcasecmp(const char *a, const char *b);
static bool subghz_parse_raw(flipper_file_t *ctx, flipper_subghz_signal_t *out);
static bool subghz_parse_key(flipper_file_t *ctx, flipper_subghz_signal_t *out);
static bool m1sgh_parse_raw(flipper_file_t *ctx, flipper_subghz_signal_t *out);
static bool m1sgh_parse_packet(flipper_file_t *ctx, flipper_subghz_signal_t *out);
static const char *stristr(const char *haystack, const char *needle);

static bool subghz_parse_key_2(const char *value, uint64_t *key_2,
                               bool full_width)
{
	uint8_t key_bytes[9];
	uint8_t count;
	uint8_t i;
	uint64_t v = 0;

	if (key_2 == NULL)
		return false;

	/* ProtoPirate auxiliary fields are 2..8 bytes; keep all bytes big-endian. */
	count = ff_parse_hex_bytes(value, key_bytes, sizeof(key_bytes));
	if (count < 2 || count > 8)
		return false;

	for (i = 0; i < count; i++)
		v = (v << 8) | key_bytes[i];
	*key_2 = full_width ? v : (v & UINT32_MAX);
	return true;
}

/* Format the low key_2_len bytes of key_2 as "AA BB[ ...]" (big-endian). */
static void subghz_format_key_2(char *buf, size_t size, uint64_t key_2,
                                uint8_t key_2_len)
{
	uint8_t n = 0;

	if (key_2_len > 8)
		key_2_len = 8;
	for (uint8_t i = 0; i < key_2_len; i++)
	{
		int w = snprintf(buf + n, size - n, "%s%02X",
		                 i == 0 ? "" : " ",
		                 (unsigned int)((key_2 >> (8 * (key_2_len - 1 - i))) & 0xFF));
		if (w <= 0 || (size_t)w >= size - n)
			break;
		n += (uint8_t)w;
	}
}

static void subghz_parse_protocol_type(const char *value,
                                      flipper_subghz_signal_t *out)
{
	char *end;
	unsigned long parsed;

	if (value == NULL || out == NULL)
		return;

	parsed = strtoul(value, &end, 0);
	if (end != value && *end == '\0' && parsed <= UINT32_MAX)
	{
		out->protocol_type = (uint32_t)parsed;
		out->has_protocol_type = true;
	}
}

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/*============================================================================*/
/**
 * @brief  Case-insensitive string comparison (portable)
 */
static int subghz_strcasecmp(const char *a, const char *b)
{
	while (*a && *b)
	{
		int ca = tolower((unsigned char)*a);
		int cb = tolower((unsigned char)*b);
		if (ca != cb)
			return ca - cb;
		a++;
		b++;
	}
	return (unsigned char)*a - (unsigned char)*b;
}

/*============================================================================*/
/**
 * @brief  Parse a raw Sub-GHz file body after header validation
 */
static bool subghz_parse_raw(flipper_file_t *ctx, flipper_subghz_signal_t *out)
{
	int32_t line_samples[SUBGHZ_LINE_SAMPLES_MAX];
	uint16_t line_count;
	uint16_t i;

	out->type = FLIPPER_SUBGHZ_TYPE_RAW;
	out->raw_count = 0;

	while (ff_read_line(ctx))
	{
		if (ff_is_separator(ctx))
			continue;

		if (!ff_parse_kv(ctx))
			continue;

		if (subghz_strcasecmp(ff_get_key(ctx), "Frequency") == 0)
		{
			out->frequency = (uint32_t)strtoul(ff_get_value(ctx), NULL, 10);
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Preset") == 0)
		{
			strncpy(out->preset, ff_get_value(ctx), FLIPPER_SUBGHZ_PRESET_MAX_LEN - 1);
			out->preset[FLIPPER_SUBGHZ_PRESET_MAX_LEN - 1] = '\0';
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Protocol") == 0)
		{
			strncpy(out->protocol, ff_get_value(ctx), FLIPPER_SUBGHZ_PROTO_MAX_LEN - 1);
			out->protocol[FLIPPER_SUBGHZ_PROTO_MAX_LEN - 1] = '\0';
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "RAW_Data") == 0)
		{
			/* Parse the int32 samples from this line and append to raw_data */
			line_count = ff_parse_int32_array(ff_get_value(ctx),
			                                  line_samples,
			                                  SUBGHZ_LINE_SAMPLES_MAX);

			for (i = 0; i < line_count && out->raw_count < FLIPPER_SUBGHZ_RAW_MAX_SAMPLES; i++)
			{
				/* Clamp int32 values to int16 range */
				int32_t val = line_samples[i];
				if (val > 32767)
					val = 32767;
				else if (val < -32768)
					val = -32768;

				out->raw_data[out->raw_count++] = (int16_t)val;
			}
		}
	}

	return (out->raw_count > 0);
}

/*============================================================================*/
/**
 * @brief  Parse a key/parsed Sub-GHz file body after header validation
 */
static bool subghz_parse_key(flipper_file_t *ctx, flipper_subghz_signal_t *out)
{
	out->type = FLIPPER_SUBGHZ_TYPE_PARSED;

	while (ff_read_line(ctx))
	{
		if (ff_is_separator(ctx))
			continue;

		if (!ff_parse_kv(ctx))
			continue;

		if (subghz_strcasecmp(ff_get_key(ctx), "Frequency") == 0)
		{
			out->frequency = (uint32_t)strtoul(ff_get_value(ctx), NULL, 10);
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Preset") == 0)
		{
			strncpy(out->preset, ff_get_value(ctx), FLIPPER_SUBGHZ_PRESET_MAX_LEN - 1);
			out->preset[FLIPPER_SUBGHZ_PRESET_MAX_LEN - 1] = '\0';
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Protocol") == 0)
		{
			strncpy(out->protocol, ff_get_value(ctx), FLIPPER_SUBGHZ_PROTO_MAX_LEN - 1);
			out->protocol[FLIPPER_SUBGHZ_PROTO_MAX_LEN - 1] = '\0';
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Bit") == 0)
		{
			out->bit_count = (uint32_t)strtoul(ff_get_value(ctx), NULL, 10);
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Key") == 0)
		{
			/* Key is in format "00 00 00 00 07 04 D0 03" (8 hex bytes, big-endian) */
			uint8_t key_bytes[8];
			uint8_t count;
			uint8_t i;

			memset(key_bytes, 0, sizeof(key_bytes));
			count = ff_parse_hex_bytes(ff_get_value(ctx), key_bytes, 8);

			out->key = 0;
			for (i = 0; i < count && i < 8; i++)
			{
				out->key = (out->key << 8) | key_bytes[i];
			}
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Key_2") == 0 ||
		         subghz_strcasecmp(ff_get_key(ctx), "Key2") == 0)
		{
			/* Accept 2–8 bytes; retain all 64 bits only for Ford V1. */
			if (subghz_parse_key_2(ff_get_value(ctx), &out->key_2,
				                   subghz_strcasecmp(out->protocol, "Ford V1") == 0))
				out->has_key_2 = true;
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Type") == 0)
		{
			subghz_parse_protocol_type(ff_get_value(ctx), out);
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "TE") == 0)
		{
			out->te = (uint32_t)strtoul(ff_get_value(ctx), NULL, 10);
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Manufacture") == 0)
		{
			/* KeeLoq-family manufacturer-key name — carried
			 * through load → SignalSettings → save_key_with_manufacture
			 * so the saved file's Manufacture line is preserved across
			 * round-trip edits. */
			strncpy(out->manufacture, ff_get_value(ctx),
			        FLIPPER_SUBGHZ_MANUF_MAX_LEN - 1);
			out->manufacture[FLIPPER_SUBGHZ_MANUF_MAX_LEN - 1] = '\0';
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "CounterMode") == 0)
		{
			/* Optional KeeLoq-family replay policy override.
			 * "Static" → STATIC replay (re-emit captured hop verbatim).
			 * Anything else (including missing field, empty string, or
			 * an unknown value such as "Bogus") falls through to the
			 * INCREMENT default that was already set by memset() at the
			 * caller — matching the historical behaviour for every
			 * existing .sub file. */
			if (subghz_strcasecmp(ff_get_value(ctx), "Static") == 0)
				out->counter_mode = FLIPPER_SUBGHZ_COUNTER_MODE_STATIC;
			else
				out->counter_mode = FLIPPER_SUBGHZ_COUNTER_MODE_INCREMENT;
		}
	}

	return (out->frequency > 0 && out->protocol[0] != '\0');
}

/*============================================================================*/
/**
 * @brief  Parse an M1 native .sgh RAW (NOISE) file body.
 *
 * M1 native RAW files store unsigned pulse durations in "Data:" lines.
 * Values are converted to alternating signed format (mark/space) so the
 * offline decode engine (which expects Flipper-style alternating int16_t)
 * can process them.  The first sample is assumed to be a mark (positive).
 */
static bool m1sgh_parse_raw(flipper_file_t *ctx, flipper_subghz_signal_t *out)
{
	int32_t line_samples[SUBGHZ_LINE_SAMPLES_MAX];
	uint16_t line_count;
	uint16_t i;
	bool positive = true; /* first pulse assumed to be mark (high) */

	out->type = FLIPPER_SUBGHZ_TYPE_RAW;
	out->raw_count = 0;

	while (ff_read_line(ctx))
	{
		if (ff_is_separator(ctx))
			continue;

		if (!ff_parse_kv(ctx))
			continue;

		if (subghz_strcasecmp(ff_get_key(ctx), "Frequency") == 0)
		{
			out->frequency = (uint32_t)strtoul(ff_get_value(ctx), NULL, 10);
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Modulation") == 0)
		{
			/* Store modulation in preset field for info screen display */
			strncpy(out->preset, ff_get_value(ctx), FLIPPER_SUBGHZ_PRESET_MAX_LEN - 1);
			out->preset[FLIPPER_SUBGHZ_PRESET_MAX_LEN - 1] = '\0';
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Data") == 0)
		{
			/* Parse unsigned pulse durations; convert to alternating signed */
			line_count = ff_parse_int32_array(ff_get_value(ctx),
			                                  line_samples,
			                                  SUBGHZ_LINE_SAMPLES_MAX);

			for (i = 0; i < line_count && out->raw_count < FLIPPER_SUBGHZ_RAW_MAX_SAMPLES; i++)
			{
				int32_t val = line_samples[i];
				if (val < 0) val = -val;   /* ensure positive (M1 stores unsigned) */
				if (val > 32767) val = 32767;

				/* Alternate sign: positive = mark (high), negative = space (low) */
				out->raw_data[out->raw_count++] = positive ? (int16_t)val : -(int16_t)val;
				positive = !positive;
			}
		}
	}

	return (out->raw_count > 0);
}

/*============================================================================*/
/**
 * @brief  Parse an M1 native .sgh PACKET file body.
 *
 * M1 packet files use "Modulation:" (not Flipper's "Preset:"), "Bits:" (not
 * "Bit:"), and a "Payload:" field (0x-prefixed hex key) and "BT:" (bit time).
 * All fields are extracted so callers can use key and te from the result.
 */
static bool m1sgh_parse_packet(flipper_file_t *ctx, flipper_subghz_signal_t *out)
{
	out->type = FLIPPER_SUBGHZ_TYPE_PARSED;

	while (ff_read_line(ctx))
	{
		if (ff_is_separator(ctx))
			continue;

		if (!ff_parse_kv(ctx))
			continue;

		if (subghz_strcasecmp(ff_get_key(ctx), "Frequency") == 0)
		{
			out->frequency = (uint32_t)strtoul(ff_get_value(ctx), NULL, 10);
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Modulation") == 0)
		{
			/* M1 uses "Modulation:" where Flipper uses "Preset:" */
			strncpy(out->preset, ff_get_value(ctx), FLIPPER_SUBGHZ_PRESET_MAX_LEN - 1);
			out->preset[FLIPPER_SUBGHZ_PRESET_MAX_LEN - 1] = '\0';
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Protocol") == 0)
		{
			strncpy(out->protocol, ff_get_value(ctx), FLIPPER_SUBGHZ_PROTO_MAX_LEN - 1);
			out->protocol[FLIPPER_SUBGHZ_PROTO_MAX_LEN - 1] = '\0';
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Bits") == 0)
		{
			/* M1 uses "Bits:" where Flipper uses "Bit:" */
			out->bit_count = (uint32_t)strtoul(ff_get_value(ctx), NULL, 10);
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Payload") == 0)
		{
			/* 0x-prefixed big-endian hex key, e.g. "0x0000000052A12E00" */
			out->key = (uint64_t)strtoull(ff_get_value(ctx), NULL, 16);
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "BT") == 0)
		{
			/* M1 uses "BT:" (bit time) where Flipper uses "TE:" */
			out->te = (uint32_t)strtoul(ff_get_value(ctx), NULL, 10);
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Key_2") == 0 ||
		         subghz_strcasecmp(ff_get_key(ctx), "Key2") == 0)
		{
			if (subghz_parse_key_2(ff_get_value(ctx), &out->key_2,
				                   subghz_strcasecmp(out->protocol, "Ford V1") == 0))
				out->has_key_2 = true;
		}
		else if (subghz_strcasecmp(ff_get_key(ctx), "Type") == 0)
		{
			subghz_parse_protocol_type(ff_get_value(ctx), out);
		}
	}

	return (out->frequency > 0);
}

/*============================================================================*/
/**
 * @brief  Load a .sub file (either RAW or Key format)
 * @param  path  file path on FatFs filesystem
 * @param  out   output signal structure
 * @return true on success
 */
bool flipper_subghz_load(const char *path, flipper_subghz_signal_t *out)
{
	flipper_file_t ff;
	bool result = false;
	char filetype_val[48]; /* saved before version read overwrites ff.value */
	char version_val[16];  /* saved before next read may overwrite it */

	if (path == NULL || out == NULL)
		return false;

	memset(out, 0, sizeof(flipper_subghz_signal_t));

	if (!ff_open(&ff, path))
		return false;

	/* Read and check Filetype line */
	if (!ff_read_line(&ff) || !ff_parse_kv(&ff))
	{
		ff_close(&ff);
		return false;
	}

	if (strcmp(ff_get_key(&ff), "Filetype") != 0)
	{
		ff_close(&ff);
		return false;
	}

	/* Save filetype value — the next read will overwrite ff.value */
	strncpy(filetype_val, ff_get_value(&ff), sizeof(filetype_val) - 1);
	filetype_val[sizeof(filetype_val) - 1] = '\0';

	/* Check Version line */
	if (!ff_read_line(&ff) || !ff_parse_kv(&ff))
	{
		ff_close(&ff);
		return false;
	}

	if (strcmp(ff_get_key(&ff), "Version") != 0)
	{
		ff_close(&ff);
		return false;
	}

	/* Save version value — ff.value may be overwritten on next read */
	strncpy(version_val, ff_get_value(&ff), sizeof(version_val) - 1);
	version_val[sizeof(version_val) - 1] = '\0';

	/* Determine file format from filetype + version strings.
	 * The cursor is now positioned after Version:, ready for body parsers.
	 * No close/reopen needed — filetype_val already carries the sub-type. */
	if (strcmp(version_val, "1") == 0 || strcmp(version_val, "2") == 0)
	{
		/* Flipper .sub format — RAW vs Key is encoded in filetype_val */
		if (strstr(filetype_val, "RAW") != NULL)
			result = subghz_parse_raw(&ff, out);
		else
			result = subghz_parse_key(&ff, out);
	}
	else if (flipper_subghz_is_m1_native_header(filetype_val, version_val))
	{
		/* M1 native .sgh format — sub-type (NOISE/PACKET) is in filetype_val */
		if (strstr(filetype_val, M1_SUBGHZ_NOISE_TYPE) != NULL)
			result = m1sgh_parse_raw(&ff, out);
		else if (strstr(filetype_val, M1_SUBGHZ_PACKET_TYPE) != NULL)
			result = m1sgh_parse_packet(&ff, out);
		/* else: unknown M1 format variant — result stays false */

		/* Mark as native so callers can bypass the conversion/temp-file path */
		if (result)
			out->is_m1_native = true;
	}

	ff_close(&ff);
	return result;
}

/*============================================================================*/
/**
 * @brief  Test whether header strings indicate an M1 native .sgh file.
 *
 * This is a pure-logic helper extracted for testability.  Flipper .sub files
 * use integer version strings ("1", "2"); M1 native .sgh files use the
 * firmware version (e.g., "0.8", "0.9") and always contain "M1 SubGHz" in
 * the Filetype value.
 *
 * @param  filetype_val  Value string from the Filetype: line
 * @param  version_val   Value string from the Version: line
 * @return true if the header indicates M1 native .sgh format
 */
bool flipper_subghz_is_m1_native_header(const char *filetype_val,
                                         const char *version_val)
{
	if (filetype_val == NULL || version_val == NULL)
		return false;

	/* Flipper format uses integer version strings "1" or "2" */
	if (strcmp(version_val, "1") == 0 || strcmp(version_val, "2") == 0)
		return false;

	/* M1 native format always has "M1 SubGHz" in the filetype */
	return strstr(filetype_val, M1_SUBGHZ_FILETYPE_PREFIX) != NULL;
}

/*============================================================================*/
/**
 * @brief  Lightweight header probe — reads only the first few header lines.
 *
 * Opens the file, reads Filetype/Version (to detect M1-native vs Flipper),
 * then scans subsequent lines for Frequency and Preset/Modulation.  Closes
 * the file immediately after; no Data: lines are loaded.
 *
 * This is used by the playlist scene so that each entry can be classified
 * without the 16 KB overhead of a full flipper_subghz_signal_t.
 *
 * @param  path  file path on FatFs filesystem
 * @param  out   probe result (set to zero on failure)
 * @return true if the file was opened and a valid frequency was found
 */
bool flipper_subghz_probe(const char *path, flipper_subghz_probe_t *out)
{
	flipper_file_t ff;
	char filetype_val[48];
	char version_val[16];
	bool done_freq = false;
	bool done_mod  = false;

	if (path == NULL || out == NULL)
		return false;

	memset(out, 0, sizeof(*out));
	out->modulation = MODULATION_OOK; /* safe default */

	if (!ff_open(&ff, path))
		return false;

	/* Filetype line */
	if (!ff_read_line(&ff) || !ff_parse_kv(&ff) ||
	    strcmp(ff_get_key(&ff), "Filetype") != 0)
	{
		ff_close(&ff);
		return false;
	}
	strncpy(filetype_val, ff_get_value(&ff), sizeof(filetype_val) - 1);
	filetype_val[sizeof(filetype_val) - 1] = '\0';

	/* Version line */
	if (!ff_read_line(&ff) || !ff_parse_kv(&ff) ||
	    strcmp(ff_get_key(&ff), "Version") != 0)
	{
		ff_close(&ff);
		return false;
	}
	strncpy(version_val, ff_get_value(&ff), sizeof(version_val) - 1);
	version_val[sizeof(version_val) - 1] = '\0';

	out->is_m1_native = flipper_subghz_is_m1_native_header(filetype_val, version_val);
	/* NOISE: M1 "NOISE" or Flipper "RAW" */
	out->is_noise = (strstr(filetype_val, M1_SUBGHZ_NOISE_TYPE) != NULL ||
	                 strstr(filetype_val, "RAW") != NULL);

	/* Scan remaining header lines for Frequency and Preset/Modulation.
	 * Stop at the first Data/RAW_Data/Key line (body data). */
	while (ff_read_line(&ff) && !(done_freq && done_mod))
	{
		if (ff_is_separator(&ff))
			continue;

		if (!ff_parse_kv(&ff))
			continue;

		const char *key = ff_get_key(&ff);
		const char *val = ff_get_value(&ff);

		if (!done_freq && subghz_strcasecmp(key, "Frequency") == 0)
		{
			out->frequency = (uint32_t)strtoul(val, NULL, 10);
			done_freq = true;
		}
		else if (!done_mod && (subghz_strcasecmp(key, "Preset") == 0 ||
		                       subghz_strcasecmp(key, "Modulation") == 0))
		{
			out->modulation = flipper_subghz_preset_to_modulation(val);
			if (out->modulation == MODULATION_UNKNOWN)
				out->modulation = MODULATION_OOK;
			done_mod = true;
		}
		else if (subghz_strcasecmp(key, "Key") == 0 ||
		         subghz_strcasecmp(key, "Data") == 0 ||
		         subghz_strcasecmp(key, "RAW_Data") == 0)
		{
			break; /* entered body — stop scanning */
		}
	}

	ff_close(&ff);
	return (out->frequency > 0);
}


bool flipper_subghz_save(const char *path, const flipper_subghz_signal_t *sig)
{
	flipper_file_t ff;
	bool result = true;

	if (path == NULL || sig == NULL)
		return false;

	if (!ff_open_write(&ff, path))
		return false;

	/* Write header */
	if (sig->type == FLIPPER_SUBGHZ_TYPE_RAW)
	{
		result = ff_write_kv_str(&ff, "Filetype", FLIPPER_SUBGHZ_RAW_FILETYPE);
	}
	else
	{
		result = ff_write_kv_str(&ff, "Filetype", FLIPPER_SUBGHZ_KEY_FILETYPE);
	}

	if (result)
		result = ff_write_kv_uint32(&ff, "Version", 1);

	if (result)
		result = ff_write_kv_uint32(&ff, "Frequency", sig->frequency);

	if (result && sig->preset[0] != '\0')
		result = ff_write_kv_str(&ff, "Preset", sig->preset);

	if (result && sig->protocol[0] != '\0')
		result = ff_write_kv_str(&ff, "Protocol", sig->protocol);

	if (result && sig->type == FLIPPER_SUBGHZ_TYPE_RAW)
	{
		/* Write RAW_Data lines, splitting at reasonable line lengths */
		uint16_t i = 0;
		char buf[FF_VALUE_MAX_LEN];

		while (i < sig->raw_count && result)
		{
			int pos = 0;
			uint16_t line_start = i;

			while (i < sig->raw_count)
			{
				int written = snprintf(&buf[pos], sizeof(buf) - (size_t)pos,
				                       "%s%d",
				                       (i > line_start) ? " " : "",
				                       (int)sig->raw_data[i]);

				if (written < 0 || (pos + written) >= (int)(sizeof(buf) - 8))
					break;

				pos += written;
				i++;
			}
			buf[pos] = '\0';

			result = ff_write_kv_str(&ff, "RAW_Data", buf);
		}
	}
	else if (result && sig->type == FLIPPER_SUBGHZ_TYPE_PARSED)
	{
		char key_str[32];

		if (result)
			result = ff_write_kv_uint32(&ff, "Bit", sig->bit_count);

		/* Write Key as 8-byte big-endian hex */
		if (result)
		{
			snprintf(key_str, sizeof(key_str),
			         "%02X %02X %02X %02X %02X %02X %02X %02X",
			         (unsigned int)((sig->key >> 56) & 0xFF),
			         (unsigned int)((sig->key >> 48) & 0xFF),
			         (unsigned int)((sig->key >> 40) & 0xFF),
			         (unsigned int)((sig->key >> 32) & 0xFF),
			         (unsigned int)((sig->key >> 24) & 0xFF),
			         (unsigned int)((sig->key >> 16) & 0xFF),
			         (unsigned int)((sig->key >> 8) & 0xFF),
			         (unsigned int)(sig->key & 0xFF));
			result = ff_write_kv_str(&ff, "Key", key_str);
		}

		if (result && sig->te > 0)
			result = ff_write_kv_uint32(&ff, "TE", sig->te);
	}

	ff_close(&ff);
	return result;
}

/*============================================================================*/
/**
 * @brief  Save an M1 native .sgh PACKET file for a parsed (key) signal.
 *
 * Writes a file that can be fully loaded by flipper_subghz_load() (which now
 * maps Payload→key and BT→te) and replayed via sub_ghz_replay_flipper_file().
 * The format uses M1-specific field names (Bits/Payload/BT) so that legacy
 * M1 tools can also read it.
 *
 * @param  path  file path on FatFs filesystem
 * @param  sig   signal structure to save (must be FLIPPER_SUBGHZ_TYPE_PARSED
 *               with bit_count > 0)
 * @return true on success
 */
bool flipper_subghz_save_m1native(const char *path, const flipper_subghz_signal_t *sig)
{
	flipper_file_t ff;
	bool result = true;
	char buf[32];

	if (path == NULL || sig == NULL)
		return false;

	/* PACKET saves are only valid for parsed/key signals with payload bits. */
	if (sig->type != FLIPPER_SUBGHZ_TYPE_PARSED || sig->bit_count == 0)
		return false;

	if (!ff_open_write(&ff, path))
		return false;

	/* Header */
	result = ff_write_kv_str(&ff, "Filetype", "M1 SubGHz PACKET");

	if (result)
		result = ff_write_kv_str(&ff, "Version", "0.9");

	if (result)
		result = ff_write_kv_uint32(&ff, "Frequency", sig->frequency);

	/* Derive modulation label from Flipper preset name */
	if (result)
	{
		const char *mod;
		if (stristr(sig->preset, "FSK"))
			mod = "FSK";
		else if (stristr(sig->preset, "ASK"))
			mod = "ASK";
		else
			mod = "OOK";   /* Default: OOK for most 433 MHz remotes */
		result = ff_write_kv_str(&ff, "Modulation", mod);
	}

	if (result && sig->protocol[0] != '\0')
		result = ff_write_kv_str(&ff, "Protocol", sig->protocol);

	if (result)
		result = ff_write_kv_uint32(&ff, "Bits", sig->bit_count);

	/* Key stored as 0x-prefixed hex (big-endian 8-byte) */
	if (result)
	{
		snprintf(buf, sizeof(buf), "0x%016llX",
		         (unsigned long long)sig->key);
		result = ff_write_kv_str(&ff, "Payload", buf);
	}

	if (result && sig->te > 0)
		result = ff_write_kv_uint32(&ff, "BT", sig->te);

	ff_close(&ff);
	return result;
}

/*============================================================================*/
/**
 * @brief  Lightweight .sub KEY save — writes from individual fields.
 *
 * Equivalent to flipper_subghz_save() for a PARSED signal but avoids the
 * need for a flipper_subghz_signal_t at the call site (and its ~16 KB
 * raw_data[] array).  All four save-path callers deal only with decoded
 * key signals and never need the raw buffer, so this is the preferred helper.
 *
 * @param  path      FatFs path for the output file
 * @param  frequency Carrier frequency in Hz
 * @param  preset    Flipper preset name (e.g. "FuriHalSubGhzPresetOok650Async")
 * @param  protocol  Protocol name string
 * @param  bit_count Number of data bits
 * @param  key       Decoded key value
 * @param  te        Timing element (0 to omit)
 * @return true on success
 */
bool flipper_subghz_save_key(const char *path, uint32_t frequency,
                              const char *preset, const char *protocol,
                              uint32_t bit_count, uint64_t key, uint32_t te)
{
	flipper_file_t ff;
	bool result = true;
	char key_str[32];

	if (path == NULL)
		return false;

	if (!ff_open_write(&ff, path))
		return false;

	result = ff_write_kv_str(&ff, "Filetype", FLIPPER_SUBGHZ_KEY_FILETYPE);

	if (result)
		result = ff_write_kv_uint32(&ff, "Version", 1);

	if (result)
		result = ff_write_kv_uint32(&ff, "Frequency", frequency);

	if (result && preset && preset[0] != '\0')
		result = ff_write_kv_str(&ff, "Preset", preset);

	if (result && protocol && protocol[0] != '\0')
		result = ff_write_kv_str(&ff, "Protocol", protocol);

	if (result)
		result = ff_write_kv_uint32(&ff, "Bit", bit_count);

	if (result)
	{
		snprintf(key_str, sizeof(key_str),
		         "%02X %02X %02X %02X %02X %02X %02X %02X",
		         (unsigned int)((key >> 56) & 0xFF),
		         (unsigned int)((key >> 48) & 0xFF),
		         (unsigned int)((key >> 40) & 0xFF),
		         (unsigned int)((key >> 32) & 0xFF),
		         (unsigned int)((key >> 24) & 0xFF),
		         (unsigned int)((key >> 16) & 0xFF),
		         (unsigned int)((key >>  8) & 0xFF),
		         (unsigned int)( key        & 0xFF));
		result = ff_write_kv_str(&ff, "Key", key_str);
	}

	if (result && te > 0)
		result = ff_write_kv_uint32(&ff, "TE", te);

	ff_close(&ff);
	return result;
}

/*============================================================================*/
/**
 * @brief  Variant of flipper_subghz_save_key() that also writes the
 *         `Key_2:` field — the 16-bit second key word carried by PSA
 *         captures (required to seed the PSA TEA brute-force search).
 *
 * The Key_2 line is written immediately after Key, matching the field order
 * used by ProtoPirate captures.
 */
bool flipper_subghz_save_key_with_key2(const char *path,
                                        uint32_t    frequency,
                                        const char *preset,
                                        const char *protocol,
                                        uint32_t    bit_count,
                                        uint64_t    key,
                                        uint32_t    key_2,
                                        uint8_t     key_2_len,
                                        uint32_t    te)
{
	flipper_file_t ff;
	bool result = true;
	char key_str[32];

	if (path == NULL)
		return false;

	if (!ff_open_write(&ff, path))
		return false;

	result = ff_write_kv_str(&ff, "Filetype", FLIPPER_SUBGHZ_KEY_FILETYPE);

	if (result)
		result = ff_write_kv_uint32(&ff, "Version", 1);

	if (result)
		result = ff_write_kv_uint32(&ff, "Frequency", frequency);

	if (result && preset && preset[0] != '\0')
		result = ff_write_kv_str(&ff, "Preset", preset);

	if (result && protocol && protocol[0] != '\0')
		result = ff_write_kv_str(&ff, "Protocol", protocol);

	if (result)
		result = ff_write_kv_uint32(&ff, "Bit", bit_count);

	if (result)
	{
		snprintf(key_str, sizeof(key_str),
		         "%02X %02X %02X %02X %02X %02X %02X %02X",
		         (unsigned int)((key >> 56) & 0xFF),
		         (unsigned int)((key >> 48) & 0xFF),
		         (unsigned int)((key >> 40) & 0xFF),
		         (unsigned int)((key >> 32) & 0xFF),
		         (unsigned int)((key >> 24) & 0xFF),
		         (unsigned int)((key >> 16) & 0xFF),
		         (unsigned int)((key >>  8) & 0xFF),
		         (unsigned int)( key        & 0xFF));
		result = ff_write_kv_str(&ff, "Key", key_str);
	}

	if (result)
	{
		subghz_format_key_2(key_str, sizeof(key_str), key_2, key_2_len);
		result = ff_write_kv_str(&ff, "Key_2", key_str);
	}

	if (result && te > 0)
		result = ff_write_kv_uint32(&ff, "TE", te);

	ff_close(&ff);
	return result;
}

/*============================================================================*/
/**
 * @brief  Save a PSA capture patched with the fields recovered by the TEA
 *         brute-force search (subghz_psa_bruteforce.c).
 *
 * Writes a Flipper Key file carrying the original Key/Key_2 plus the
 * recovered Serial, Btn, Cnt, Type (0x36), CRC and Seed fields — the same
 * field set the ProtoPirate reference applies to a capture after a
 * successful search.
 */
bool flipper_subghz_save_psa_recovered(const char *path,
                                        uint32_t    frequency,
                                        const char *preset,
                                        uint32_t    bit_count,
                                        uint64_t    key,
                                        uint32_t    key_2,
                                        uint32_t    te,
                                        const subghz_psa_bf_result_t *result)
{
	flipper_file_t ff;
	bool ok = true;
	char key_str[40];

	if (path == NULL || result == NULL)
		return false;

	if (!ff_open_write(&ff, path))
		return false;

	ok = ff_write_kv_str(&ff, "Filetype", FLIPPER_SUBGHZ_KEY_FILETYPE);

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Version", 1);

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Frequency", frequency);

	if (ok && preset && preset[0] != '\0')
		ok = ff_write_kv_str(&ff, "Preset", preset);

	if (ok)
		ok = ff_write_kv_str(&ff, "Protocol", "PSA");

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Bit", bit_count);

	if (ok)
	{
		snprintf(key_str, sizeof(key_str),
		         "%02X %02X %02X %02X %02X %02X %02X %02X",
		         (unsigned int)((key >> 56) & 0xFF),
		         (unsigned int)((key >> 48) & 0xFF),
		         (unsigned int)((key >> 40) & 0xFF),
		         (unsigned int)((key >> 32) & 0xFF),
		         (unsigned int)((key >> 24) & 0xFF),
		         (unsigned int)((key >> 16) & 0xFF),
		         (unsigned int)((key >>  8) & 0xFF),
		         (unsigned int)( key        & 0xFF));
		ok = ff_write_kv_str(&ff, "Key", key_str);
	}

	if (ok)
	{
		snprintf(key_str, sizeof(key_str), "%02X %02X",
		         (unsigned int)((key_2 >> 8) & 0xFF),
		         (unsigned int)( key_2       & 0xFF));
		ok = ff_write_kv_str(&ff, "Key_2", key_str);
	}

	if (ok && te > 0)
		ok = ff_write_kv_uint32(&ff, "TE", te);

	/* Recovered fields (ProtoPirate write-back set). */
	if (ok)
		ok = ff_write_kv_uint32(&ff, "Serial", result->serial);

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Btn", result->button);

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Cnt", result->counter);

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Type", result->type);

	if (ok)
		ok = ff_write_kv_uint32(&ff, "CRC", result->crc);

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Seed", result->seed);

	ff_close(&ff);
	return ok;
}

/*============================================================================*/
/**
 * @brief  Save a VAG capture with the ProtoPirate field set needed to
 *         re-encrypt frames for TX (Sub_Ghz/subghz_vag_encoder.c): Key (the
 *         logical Key1, whose top byte is the manufacturer tag), `Key2:`
 *         (note: not `Key_2:` � upstream VAG uses the no-underscore name),
 *         plus Serial/Btn/Cnt/Type.
 */
bool flipper_subghz_save_vag(const char *path,
                              uint32_t    frequency,
                              const char *preset,
                              uint32_t    bit_count,
                              uint64_t    key1,
                              uint16_t    key2,
                              uint32_t    te,
                              uint32_t    serial,
                              uint8_t     button,
                              uint32_t    counter,
                              uint8_t     type)
{
	flipper_file_t ff;
	bool result = true;
	char buf[40];

	if (path == NULL)
		return false;

	if (!ff_open_write(&ff, path))
		return false;

	result = ff_write_kv_str(&ff, "Filetype", FLIPPER_SUBGHZ_KEY_FILETYPE);

	if (result)
		result = ff_write_kv_uint32(&ff, "Version", 1);

	if (result)
		result = ff_write_kv_uint32(&ff, "Frequency", frequency);

	if (result && preset && preset[0] != ' ')
		result = ff_write_kv_str(&ff, "Preset", preset);

	if (result)
		result = ff_write_kv_str(&ff, "Protocol", "VAG");

	if (result)
		result = ff_write_kv_uint32(&ff, "Bit", bit_count);

	if (result)
	{
		snprintf(buf, sizeof(buf),
		         "%02X %02X %02X %02X %02X %02X %02X %02X",
		         (unsigned int)((key1 >> 56) & 0xFF),
		         (unsigned int)((key1 >> 48) & 0xFF),
		         (unsigned int)((key1 >> 40) & 0xFF),
		         (unsigned int)((key1 >> 32) & 0xFF),
		         (unsigned int)((key1 >> 24) & 0xFF),
		         (unsigned int)((key1 >> 16) & 0xFF),
		         (unsigned int)((key1 >>  8) & 0xFF),
		         (unsigned int)( key1        & 0xFF));
		result = ff_write_kv_str(&ff, "Key", buf);
	}

	if (result)
	{
		snprintf(buf, sizeof(buf), "%02X %02X",
		         (unsigned int)((key2 >> 8) & 0xFF),
		         (unsigned int)( key2       & 0xFF));
		result = ff_write_kv_str(&ff, "Key_2", buf);
	}

	if (result && te > 0)
		result = ff_write_kv_uint32(&ff, "TE", te);

	if (result)
		result = ff_write_kv_uint32(&ff, "Serial", serial);

	if (result)
		result = ff_write_kv_uint32(&ff, "Btn", button);

	if (result)
		result = ff_write_kv_uint32(&ff, "Cnt", counter);

	if (result)
		result = ff_write_kv_uint32(&ff, "Type", type);

	ff_close(&ff);
	return result;
}

/*============================================================================*/
/**
 * @brief  Save a Kia V6 capture with the fields needed to re-encrypt frames
 *         for TX (subghz_kia_v6_encoder.c): Key (serial<<32|counter per the
 *         decoder layout) plus Btn and Fx.
 */
bool flipper_subghz_save_kia_v6(const char *path,
                                 uint32_t    frequency,
                                 const char *preset,
                                 uint32_t    bit_count,
                                 uint64_t    key,
                                 uint32_t    te,
                                 uint8_t     button,
                                 uint8_t     fx)
{
	flipper_file_t ff;
	bool result = true;
	char key_str[32];

	if (path == NULL)
		return false;

	if (!ff_open_write(&ff, path))
		return false;

	result = ff_write_kv_str(&ff, "Filetype", FLIPPER_SUBGHZ_KEY_FILETYPE);

	if (result)
		result = ff_write_kv_uint32(&ff, "Version", 1);

	if (result)
		result = ff_write_kv_uint32(&ff, "Frequency", frequency);

	if (result && preset && preset[0] != ' ')
		result = ff_write_kv_str(&ff, "Preset", preset);

	if (result)
		result = ff_write_kv_str(&ff, "Protocol", "Kia V6");

	if (result)
		result = ff_write_kv_uint32(&ff, "Bit", bit_count);

	if (result)
	{
		snprintf(key_str, sizeof(key_str),
		         "%02X %02X %02X %02X %02X %02X %02X %02X",
		         (unsigned int)((key >> 56) & 0xFF),
		         (unsigned int)((key >> 48) & 0xFF),
		         (unsigned int)((key >> 40) & 0xFF),
		         (unsigned int)((key >> 32) & 0xFF),
		         (unsigned int)((key >> 24) & 0xFF),
		         (unsigned int)((key >> 16) & 0xFF),
		         (unsigned int)((key >>  8) & 0xFF),
		         (unsigned int)( key        & 0xFF));
		result = ff_write_kv_str(&ff, "Key", key_str);
	}

	if (result && te > 0)
		result = ff_write_kv_uint32(&ff, "TE", te);

	if (result && (uint32_t)(key >> 32) != 0)
	{
		result = ff_write_kv_uint32(&ff, "Serial", (uint32_t)(key >> 32));
		if (result)
			result = ff_write_kv_uint32(&ff, "Btn", button);
		if (result)
			result = ff_write_kv_uint32(&ff, "Cnt", (uint32_t)key);
	}
	else if (result)
	{
		result = ff_write_kv_uint32(&ff, "Btn", button);
	}

	if (result)
		result = ff_write_kv_uint32(&ff, "Fx", fx);

	ff_close(&ff);
	return result;
}

/*============================================================================*/
/**
 * @brief  Save a Renault V1 capture patched with the seed (IV) recovered by
 *         the Hitag2 brute-force search (subghz_renault_v1_bruteforce.c).
 *
 * Writes a Flipper Key file carrying the original Key/Key_2 plus Serial, Btn,
 * Cnt (full counter reconstructed from the seed), Hop, Seed (4 hex bytes) and
 * Recovered: 01 � the field set the ProtoPirate reference applies after a
 * successful search.  The plaintext fields are sliced back out of the
 * captured frame itself (the Renault V1 wire format is cipher-free except
 * for the hop word).
 */
bool flipper_subghz_save_renault_v1_recovered(const char *path,
                                               uint32_t    frequency,
                                               const char *preset,
                                               uint32_t    bit_count,
                                               uint64_t    key,
                                               uint32_t    key_2,
                                               uint32_t    te,
                                               uint32_t    seed)
{
	flipper_file_t ff;
	bool ok = true;
	char buf[40];

	if (path == NULL)
		return false;

	uint8_t frame[11];
	subghz_renault_v1_bf_frame_from_capture(frame, key, key_2 & 0xFFFFFFu);

	uint32_t serial = 0, hop = 0;
	uint16_t cnt10 = 0;
	uint8_t  btn = 0;
	subghz_renault_v1_bf_unpack_frame(frame, &serial, &btn, &cnt10, &hop);

	if (!ff_open_write(&ff, path))
		return false;

	ok = ff_write_kv_str(&ff, "Filetype", FLIPPER_SUBGHZ_KEY_FILETYPE);

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Version", 1);

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Frequency", frequency);

	if (ok && preset && preset[0] != '\0')
		ok = ff_write_kv_str(&ff, "Preset", preset);

	if (ok)
		ok = ff_write_kv_str(&ff, "Protocol", "Renault V1");

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Bit", bit_count);

	if (ok)
	{
		snprintf(buf, sizeof(buf),
		         "%02X %02X %02X %02X %02X %02X %02X %02X",
		         (unsigned int)((key >> 56) & 0xFF),
		         (unsigned int)((key >> 48) & 0xFF),
		         (unsigned int)((key >> 40) & 0xFF),
		         (unsigned int)((key >> 32) & 0xFF),
		         (unsigned int)((key >> 24) & 0xFF),
		         (unsigned int)((key >> 16) & 0xFF),
		         (unsigned int)((key >>  8) & 0xFF),
		         (unsigned int)( key        & 0xFF));
		ok = ff_write_kv_str(&ff, "Key", buf);
	}

	if (ok)
	{
		subghz_format_key_2(buf, sizeof(buf), key_2, 3u);
		ok = ff_write_kv_str(&ff, "Key_2", buf);
	}

	if (ok && te > 0)
		ok = ff_write_kv_uint32(&ff, "TE", te);

	/* Recovered write-back set (ProtoPirate field formats). */
	if (ok)
		ok = ff_write_kv_uint32(&ff, "Serial", serial);

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Btn", btn);

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Cnt",
		                        subghz_renault_v1_bf_counter_from_seed(seed));

	if (ok)
		ok = ff_write_kv_uint32(&ff, "Hop", hop);

	if (ok)
	{
		snprintf(buf, sizeof(buf), "%02X %02X %02X %02X",
		         (unsigned int)((seed >> 24) & 0xFF),
		         (unsigned int)((seed >> 16) & 0xFF),
		         (unsigned int)((seed >>  8) & 0xFF),
		         (unsigned int)( seed        & 0xFF));
		ok = ff_write_kv_str(&ff, "Seed", buf);
	}

	if (ok)
		ok = ff_write_kv_str(&ff, "Recovered", "01");

	ff_close(&ff);
	return ok;
}

/*============================================================================*/
/**
 * @brief  Variant of flipper_subghz_save_key() that also writes the
 *         `Manufacture:` field so the saved key can be replayed via the
 *         KeeLoq counter-mode encoder.
 *
 * Used by the create-from-scratch KeeLoq-family flow.  When
 * @p manufacture is NULL or empty the function behaves identically to
 * flipper_subghz_save_key() (no Manufacture line written).
 */
bool flipper_subghz_save_key_with_manufacture(const char *path,
                                               uint32_t    frequency,
                                               const char *preset,
                                               const char *protocol,
                                               uint32_t    bit_count,
                                               uint64_t    key,
                                               uint32_t    te,
                                               const char *manufacture)
{
	/* Thin wrapper kept for backwards-compatibility with earlier
	 * callers — defaults CounterMode to INCREMENT so no new line is
	 * written to existing files unless they go through the
	 * SignalSettings save path. */
	return flipper_subghz_save_key_full(path, frequency, preset, protocol,
	                                    bit_count, key, te, manufacture,
	                                    FLIPPER_SUBGHZ_COUNTER_MODE_INCREMENT);
}

/*============================================================================*/
/**
 * @brief  Write a Flipper Key file with both optional
 *         `Manufacture:` and `CounterMode:` fields.
 *
 * The `CounterMode:` line is only emitted when @p counter_mode is non-default
 * (i.e. STATIC).  This keeps existing saved files byte-identical
 * across re-saves that did not touch the counter-mode toggle.
 */
bool flipper_subghz_save_key_full(const char *path,
                                   uint32_t    frequency,
                                   const char *preset,
                                   const char *protocol,
                                   uint32_t    bit_count,
                                   uint64_t    key,
                                   uint32_t    te,
                                   const char *manufacture,
                                   flipper_subghz_counter_mode_t counter_mode)
{
	flipper_file_t ff;
	bool result = true;
	char key_str[32];

	if (path == NULL)
		return false;

	if (!ff_open_write(&ff, path))
		return false;

	result = ff_write_kv_str(&ff, "Filetype", FLIPPER_SUBGHZ_KEY_FILETYPE);

	if (result)
		result = ff_write_kv_uint32(&ff, "Version", 1);

	if (result)
		result = ff_write_kv_uint32(&ff, "Frequency", frequency);

	if (result && preset && preset[0] != '\0')
		result = ff_write_kv_str(&ff, "Preset", preset);

	if (result && protocol && protocol[0] != '\0')
		result = ff_write_kv_str(&ff, "Protocol", protocol);

	if (result)
		result = ff_write_kv_uint32(&ff, "Bit", bit_count);

	if (result)
	{
		snprintf(key_str, sizeof(key_str),
		         "%02X %02X %02X %02X %02X %02X %02X %02X",
		         (unsigned int)((key >> 56) & 0xFF),
		         (unsigned int)((key >> 48) & 0xFF),
		         (unsigned int)((key >> 40) & 0xFF),
		         (unsigned int)((key >> 32) & 0xFF),
		         (unsigned int)((key >> 24) & 0xFF),
		         (unsigned int)((key >> 16) & 0xFF),
		         (unsigned int)((key >>  8) & 0xFF),
		         (unsigned int)( key        & 0xFF));
		result = ff_write_kv_str(&ff, "Key", key_str);
	}

	if (result && te > 0)
		result = ff_write_kv_uint32(&ff, "TE", te);

	if (result && manufacture && manufacture[0] != '\0')
		result = ff_write_kv_str(&ff, "Manufacture", manufacture);

	if (result && counter_mode == FLIPPER_SUBGHZ_COUNTER_MODE_STATIC)
		result = ff_write_kv_str(&ff, "CounterMode", "Static");

	ff_close(&ff);
	return result;
}

static uint16_t subghz_ford_v1_crc16(const flipper_subghz_key_capture_t *capture)
{
	uint8_t raw[15] = {0};
	uint16_t remainder = 0;

	for (uint8_t i = 0; i < 7; i++)
		raw[i] = (uint8_t)(capture->key >> (48u - i * 8u));
	for (uint8_t i = 0; i < 8; i++)
		raw[7u + i] = (uint8_t)(capture->key_2 >> (56u - i * 8u));

	for (uint8_t byte = 3; byte < 15; byte++)
	{
		remainder ^= (uint16_t)raw[byte] << 8;
		for (uint8_t bit = 0; bit < 8; bit++)
		{
			remainder = (remainder & 0x8000u)
			                ? (uint16_t)((remainder << 1) ^ 0x1021u)
			                : (uint16_t)(remainder << 1);
		}
	}
	return remainder;
}

static uint32_t subghz_ford_v2_tail31(uint64_t tail_raw)
{
	uint8_t raw[5];
	for (uint8_t i = 0; i < sizeof(raw); i++)
		raw[i] = (uint8_t)(tail_raw >> (32u - i * 8u));
	return (((uint32_t)(raw[1] & 0x7Fu)) << 24) |
	       ((uint32_t)raw[2] << 16) | ((uint32_t)raw[3] << 8) | raw[4];
}

/*============================================================================*/
/**
 * @brief  Lightweight M1 native .sgh PACKET save — writes from individual fields.
 *
 * Equivalent to flipper_subghz_save_m1native() but avoids the need for a
 * flipper_subghz_signal_t at the call site.
 *
 * @param  path      FatFs path for the output file
 * @param  frequency Carrier frequency in Hz
 * @param  preset    Flipper preset name (used to derive OOK/ASK/FSK label)
 * @param  protocol  Protocol name string
 * @param  bit_count Number of data bits
 * @param  key       Decoded key value
 * @param  te        Timing element (0 to omit)
 * @return true on success
 */
static bool subghz_save_m1native_key(const char *path, uint32_t frequency,
                                      const char *preset, const char *protocol,
                                      uint32_t bit_count, uint64_t key,
                                      uint64_t key_2, uint8_t key_2_len,
                                      uint32_t te,
                                      const flipper_subghz_key_capture_t *capture)
{
	flipper_file_t ff;
	bool result = true;
	char buf[32];
	const bool is_vag = capture != NULL && protocol != NULL &&
	                    strcmp(protocol, "VAG") == 0;
	const bool is_kia_v6 = capture != NULL && protocol != NULL &&
	                       strcmp(protocol, "Kia V6") == 0;
	const bool is_ford_v1 = capture != NULL && protocol != NULL &&
	                        strcmp(protocol, "Ford V1") == 0;
	const bool is_ford_v2 = capture != NULL && protocol != NULL &&
	                        strcmp(protocol, "Ford V2") == 0;

	if (path == NULL || bit_count == 0)
		return false;

	if (!ff_open_write(&ff, path))
		return false;

	result = ff_write_kv_str(&ff, "Filetype", "M1 SubGHz PACKET");

	if (result)
		result = ff_write_kv_str(&ff, "Version", "0.9");

	if (result)
		result = ff_write_kv_uint32(&ff, "Frequency", frequency);

	if (result)
	{
		const char *mod;
		if (preset && stristr(preset, "FSK"))
			mod = "FSK";
		else if (preset && stristr(preset, "ASK"))
			mod = "ASK";
		else
			mod = "OOK";
		result = ff_write_kv_str(&ff, "Modulation", mod);
	}

	if (result && protocol && protocol[0] != '\0')
		result = ff_write_kv_str(&ff, "Protocol", protocol);

	if (result)
		result = ff_write_kv_uint32(&ff, "Bits", bit_count);

	if (result)
	{
		snprintf(buf, sizeof(buf), "0x%016llX", (unsigned long long)key);
		result = ff_write_kv_str(&ff, "Payload", buf);
	}

	if (result && is_vag)
	{
		subghz_format_key_2(buf, sizeof(buf), key_2, 2u);
		result = ff_write_kv_str(&ff, "Key_2", buf);
	}
	else if (result && is_ford_v1)
	{
		const uint16_t crc = subghz_ford_v1_crc16(capture);
		subghz_format_key_2(buf, sizeof(buf), key_2, 8u);
		result = ff_write_kv_str(&ff, "Key_2", buf);
		if (result)
		{
			subghz_format_key_2(buf, sizeof(buf), crc, 4u);
			result = ff_write_kv_str(&ff, "Key_3", buf);
		}
	}
	else if (result && is_ford_v2)
	{
		result = ff_write_kv_uint32(&ff, "Tail31",
		                            subghz_ford_v2_tail31(key_2));
		if (result)
		{
			subghz_format_key_2(buf, sizeof(buf), key_2, 5u);
			result = ff_write_kv_str(&ff, "TailRaw", buf);
		}
	}
	else if (result && key_2_len > 0)
	{
		subghz_format_key_2(buf, sizeof(buf), key_2, key_2_len);
		result = ff_write_kv_str(&ff, "Key_2", buf);
	}

	if (result && te > 0)
		result = ff_write_kv_uint32(&ff, "BT", te);

	if (result && capture && (is_vag || capture->serial != 0))
	{
		result = ff_write_kv_uint32(&ff, "Serial", capture->serial);
		if (result)
			result = ff_write_kv_uint32(&ff, "Btn", capture->button);
		if (result)
			result = ff_write_kv_uint32(&ff, "Cnt", capture->counter);
	}
	if (result && is_vag)
		result = ff_write_kv_uint32(&ff, "Type", capture->type);
	if (result && is_kia_v6 && capture->serial == 0)
		result = ff_write_kv_uint32(&ff, "Btn", capture->button);
	if (result && is_kia_v6)
		result = ff_write_kv_uint32(&ff, "Fx", capture->fx);

	ff_close(&ff);
	return result;
}

bool flipper_subghz_save_m1native_key(const char *path, uint32_t frequency,
                                       const char *preset, const char *protocol,
                                       uint32_t bit_count, uint64_t key, uint32_t te)
{
	return subghz_save_m1native_key(path, frequency, preset, protocol,
	                                bit_count, key, 0, 0, te, NULL);
}

bool flipper_subghz_save_m1native_key_with_key2(
    const char *path, uint32_t frequency, const char *preset,
    const char *protocol, uint32_t bit_count, uint64_t key,
    uint32_t key_2, uint8_t key_2_len, uint32_t te)
{
	return subghz_save_m1native_key(path, frequency, preset, protocol,
	                                bit_count, key, key_2, key_2_len, te, NULL);
}

static bool subghz_save_flipper_key_capture(
    const char *path, const flipper_subghz_key_capture_t *capture)
{
	flipper_file_t ff;
	bool result = true;
	char key_str[32];
	const bool is_ford_v1 = strcmp(capture->protocol, "Ford V1") == 0;
	const bool is_ford_v2 = strcmp(capture->protocol, "Ford V2") == 0;

	if (!ff_open_write(&ff, path))
		return false;

	result = ff_write_kv_str(&ff, "Filetype", FLIPPER_SUBGHZ_KEY_FILETYPE);
	if (result)
		result = ff_write_kv_uint32(&ff, "Version", 1);
	if (result)
		result = ff_write_kv_uint32(&ff, "Frequency", capture->frequency);
	if (result && capture->preset && capture->preset[0] != '\0')
		result = ff_write_kv_str(&ff, "Preset", capture->preset);
	if (result)
		result = ff_write_kv_str(&ff, "Protocol", capture->protocol);
	if (result)
		result = ff_write_kv_uint32(&ff, "Bit", capture->bit_count);
	if (result)
	{
		snprintf(key_str, sizeof(key_str),
		         "%02X %02X %02X %02X %02X %02X %02X %02X",
		         (unsigned int)((capture->key >> 56) & 0xFF),
		         (unsigned int)((capture->key >> 48) & 0xFF),
		         (unsigned int)((capture->key >> 40) & 0xFF),
		         (unsigned int)((capture->key >> 32) & 0xFF),
		         (unsigned int)((capture->key >> 24) & 0xFF),
		         (unsigned int)((capture->key >> 16) & 0xFF),
		         (unsigned int)((capture->key >>  8) & 0xFF),
		         (unsigned int)( capture->key        & 0xFF));
		result = ff_write_kv_str(&ff, "Key", key_str);
	}
	if (result && is_ford_v1)
	{
		const uint16_t crc = subghz_ford_v1_crc16(capture);
		subghz_format_key_2(key_str, sizeof(key_str), capture->key_2, 8u);
		result = ff_write_kv_str(&ff, "Key_2", key_str);
		if (result)
		{
			subghz_format_key_2(key_str, sizeof(key_str), crc, 4u);
			result = ff_write_kv_str(&ff, "Key_3", key_str);
		}
	}
	else if (result && is_ford_v2)
	{
		result = ff_write_kv_uint32(&ff, "Tail31",
		                            subghz_ford_v2_tail31(capture->key_2));
		if (result)
		{
			subghz_format_key_2(key_str, sizeof(key_str), capture->key_2, 5u);
			result = ff_write_kv_str(&ff, "TailRaw", key_str);
		}
	}
	else if (result && capture->key_2_len > 0)
	{
		subghz_format_key_2(key_str, sizeof(key_str), capture->key_2,
		                    capture->key_2_len);
		result = ff_write_kv_str(&ff, "Key_2", key_str);
	}
	if (result && capture->te > 0)
		result = ff_write_kv_uint32(&ff, "TE", capture->te);
	if (result && capture->serial != 0)
	{
		result = ff_write_kv_uint32(&ff, "Serial", capture->serial);
		if (result)
			result = ff_write_kv_uint32(&ff, "Btn", capture->button);
		if (result)
			result = ff_write_kv_uint32(&ff, "Cnt", capture->counter);
	}

	ff_close(&ff);
	return result;
}

bool flipper_subghz_save_key_capture(
    const char *path, bool m1_native,
    const flipper_subghz_key_capture_t *capture)
{
	if (path == NULL || capture == NULL || capture->protocol == NULL)
		return false;

	const bool is_vag = strcmp(capture->protocol, "VAG") == 0;
	const bool is_kia_v6 = strcmp(capture->protocol, "Kia V6") == 0;
	if (m1_native)
		return subghz_save_m1native_key(
		    path, capture->frequency, capture->preset, capture->protocol,
		    capture->bit_count, capture->key, capture->key_2,
		    capture->key_2_len, capture->te, capture);
	if (is_vag)
		return flipper_subghz_save_vag(
		    path, capture->frequency, capture->preset, capture->bit_count,
		    capture->key, (uint16_t)capture->key_2, capture->te,
		    capture->serial, (uint8_t)capture->button, capture->counter,
		    capture->type);
	if (is_kia_v6)
		return flipper_subghz_save_kia_v6(
		    path, capture->frequency, capture->preset, capture->bit_count,
		    capture->key, capture->te, (uint8_t)capture->button, capture->fx);
	return subghz_save_flipper_key_capture(path, capture);
}

/*============================================================================*/
/** @brief  Case-insensitive substring search (like POSIX strcasestr). */
static const char *stristr(const char *haystack, const char *needle)
{
	if (!needle[0]) return haystack;
	for (; *haystack; haystack++)
	{
		const char *h = haystack, *n = needle;
		while (*h && *n && (tolower((unsigned char)*h) == tolower((unsigned char)*n)))
		{
			h++;
			n++;
		}
		if (!*n) return haystack;
	}
	return NULL;
}

/*============================================================================*/
/**
 * @brief  Map Flipper preset name to M1 modulation type
 * @param  preset  Flipper preset string (e.g., "FuriHalSubGhzPresetOok650Async")
 * @return M1 modulation type (S_M1_SubGHz_Modulation enum value)
 */
uint8_t flipper_subghz_preset_to_modulation(const char *preset)
{
	if (preset == NULL)
		return (uint8_t)MODULATION_UNKNOWN;

	/* OOK presets — e.g. FuriHalSubGhzPresetOok650Async */
	if (stristr(preset, "OOK"))
		return (uint8_t)MODULATION_OOK;

	/* ASK presets */
	if (stristr(preset, "ASK"))
		return (uint8_t)MODULATION_ASK;

	/* FSK presets — matches 2FSK, GFSK, 4FSK, etc.
	 * e.g. FuriHalSubGhzPreset2FSKDev238Async */
	if (stristr(preset, "FSK"))
		return (uint8_t)MODULATION_FSK;

	return (uint8_t)MODULATION_UNKNOWN;
}

/*============================================================================*/
/**
 * @brief  Convert frequency in Hz to closest M1 SI4463 band enum
 * @param  freq_hz  frequency in Hz (e.g., 433920000)
 * @return M1 Sub-GHz band enum value (S_M1_SubGHz_Band)
 */
uint8_t flipper_subghz_freq_to_band(uint32_t freq_hz)
{
	/* Convert to MHz for comparison */
	uint32_t freq_mhz = freq_hz / 1000000;

	if (freq_mhz <= 305)
		return (uint8_t)SUB_GHZ_BAND_300;
	else if (freq_mhz <= 312)
		return (uint8_t)SUB_GHZ_BAND_310;
	else if (freq_mhz <= 320)
		return (uint8_t)SUB_GHZ_BAND_315;
	else if (freq_mhz <= 358)
		return (uint8_t)SUB_GHZ_BAND_345;
	else if (freq_mhz <= 381)
		return (uint8_t)SUB_GHZ_BAND_372;
	else if (freq_mhz <= 412)
		return (uint8_t)SUB_GHZ_BAND_390;
	else if (freq_mhz <= 433)
		return (uint8_t)SUB_GHZ_BAND_433;
	else if (freq_mhz <= 500)
		return (uint8_t)SUB_GHZ_BAND_433_92;
	else
		return (uint8_t)SUB_GHZ_BAND_915;
}
