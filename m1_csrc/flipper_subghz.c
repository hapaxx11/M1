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

/*************************** D E F I N E S ************************************/

#define FLIPPER_SUBGHZ_RAW_FILETYPE      "Flipper SubGhz RAW File"
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
		else if (subghz_strcasecmp(ff_get_key(ctx), "TE") == 0)
		{
			out->te = (uint32_t)strtoul(ff_get_value(ctx), NULL, 10);
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
 * "Bit:"), and a "Payload:" field.  Only Frequency, Modulation, Protocol, and
 * Bits are extracted; Payload is intentionally skipped because it uses a
 * different binary encoding than Flipper's hex Key field.
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
		/* Note: M1 "Payload:", "BT:", "IT:" are not mapped to out->key here;
		 * the emulate action uses sub_ghz_replay_flipper_file() which reads
		 * the file directly in its native format. */
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

	const char *version_val = ff_get_value(&ff);

	/* Determine file format from filetype + version strings */
	if (strcmp(version_val, "1") == 0 || strcmp(version_val, "2") == 0)
	{
		/* Flipper .sub format (version 1 or 2) — re-open to check filetype */
		ff_close(&ff);

		if (!ff_open(&ff, path))
			return false;

		if (!ff_read_line(&ff) || !ff_parse_kv(&ff))
		{
			ff_close(&ff);
			return false;
		}

		if (strstr(ff_get_value(&ff), "RAW") != NULL)
		{
			/* Skip Version line */
			if (!ff_read_line(&ff) || !ff_parse_kv(&ff))
			{
				ff_close(&ff);
				return false;
			}
			result = subghz_parse_raw(&ff, out);
		}
		else
		{
			/* Skip Version line */
			if (!ff_read_line(&ff) || !ff_parse_kv(&ff))
			{
				ff_close(&ff);
				return false;
			}
			result = subghz_parse_key(&ff, out);
		}
	}
	else if (strstr(filetype_val, M1_SUBGHZ_FILETYPE_PREFIX) != NULL)
	{
		/* M1 native .sgh format — version is firmware major.minor (e.g. "0.8",
		 * "0.9").  File type (RAW vs PACKET) is determined by the filetype value,
		 * not the version.  The file descriptor is already positioned after the
		 * Version: line, so body parsing continues from here. */
		if (strstr(filetype_val, M1_SUBGHZ_NOISE_TYPE) != NULL)
		{
			result = m1sgh_parse_raw(&ff, out);
		}
		else if (strstr(filetype_val, M1_SUBGHZ_PACKET_TYPE) != NULL)
		{
			result = m1sgh_parse_packet(&ff, out);
		}
		/* else: unknown M1 format variant — result stays false */
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
 * @brief  Save a .sub file
 * @param  path  file path on FatFs filesystem
 * @param  sig   signal structure to save
 * @return true on success
 */
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
