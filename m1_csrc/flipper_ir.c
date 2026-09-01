/* See COPYING.txt for license details. */

/*
 * flipper_ir.c
 *
 * Flipper Zero .ir file format parser for IR signals
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
#include "flipper_ir.h"

/*************************** D E F I N E S ************************************/

#define FLIPPER_IR_FILETYPE     "IR signals file"
#define FLIPPER_IR_MIN_VERSION  1

//************************** S T R U C T U R E S *******************************

/**
 * @brief  Protocol mapping entry: Flipper name <-> IRMP protocol ID
 */
typedef struct {
	const char *flipper_name;
	uint8_t     irmp_id;
} ir_proto_map_t;

/***************************** V A R I A B L E S ******************************/

/**
 * Protocol mapping table between Flipper Zero protocol names and IRMP IDs.
 * The IRMP protocol IDs come from irmpprotocols.h.
 */
static const ir_proto_map_t ir_proto_table[] = {
	{ "NEC",        IRMP_NEC_PROTOCOL },         /* 2  */
	{ "NECext",     IRMP_NEC_PROTOCOL },          /* 2  (extended addressing mode) */
	{ "NEC42",      IRMP_NEC42_PROTOCOL },        /* 28 */
	{ "NEC42ext",   IRMP_NEC42_PROTOCOL },        /* 28 (extended addressing mode) */
	{ "NEC16",      IRMP_NEC16_PROTOCOL },        /* 27 */
	{ "Samsung32",  IRMP_SAMSUNG32_PROTOCOL },    /* 10 */
	{ "RC5",        IRMP_RC5_PROTOCOL },          /* 7  */
	{ "RC5X",       IRMP_RC5_PROTOCOL },          /* 7  (RC5 extended, same decoder) */
	{ "RC6",        IRMP_RC6_PROTOCOL },          /* 9  */
	{ "SIRC",       IRMP_SIRCS_PROTOCOL },        /* 1  */
	{ "SIRC15",     IRMP_SIRCS_PROTOCOL },        /* 1  (15-bit mode) */
	{ "SIRC20",     IRMP_SIRCS_PROTOCOL },        /* 1  (20-bit mode) */
	{ "Kaseikyo",   IRMP_KASEIKYO_PROTOCOL },     /* 5  */
	{ "RCA",        IRMP_RCCAR_PROTOCOL },         /* 19 */
	{ "Pioneer",    IRMP_NEC_PROTOCOL },           /* Pioneer uses NEC encoding */
	{ "Denon",      IRMP_DENON_PROTOCOL },         /* 8  */
	{ "JVC",        IRMP_JVC_PROTOCOL },           /* 20 */
	{ "Sharp",      IRMP_DENON_PROTOCOL },         /* 8  (Sharp uses same as Denon) */
	{ "Panasonic",  IRMP_KASEIKYO_PROTOCOL },      /* 5  (Panasonic uses Kaseikyo) */
	{ "LG",         IRMP_LGAIR_PROTOCOL },         /* 40 */
	{ "Samsung",    IRMP_SAMSUNG32_PROTOCOL },     /* 10 (Flipper has no separate Samsung; all are Samsung32) */
	{ "Apple",      IRMP_APPLE_PROTOCOL },         /* 11 */
	{ "Nokia",      IRMP_NOKIA_PROTOCOL },         /* 16 */
	{ "Bose",       IRMP_BOSE_PROTOCOL },          /* 31 */
	{ "Samsung48",  IRMP_SAMSUNG48_PROTOCOL },    /* 41 */
	{ "RCMM",       IRMP_RCMM32_PROTOCOL },       /* 36 */
	{ NULL,         IRMP_UNKNOWN_PROTOCOL }
};

/********************* F U N C T I O N   P R O T O T Y P E S ******************/

static int ff_strcasecmp(const char *a, const char *b);
static uint16_t ff_hex_bytes_to_uint16_le(const uint8_t *bytes, uint8_t count);
static bool ff_next_wrap(void *ctx);
static bool ff_is_sep_wrap(void *ctx);
static bool ff_parse_kv_wrap(void *ctx);
static const char *ff_get_key_wrap(void *ctx);
static const char *ff_get_val_wrap(void *ctx);

/*************** F U N C T I O N   I M P L E M E N T A T I O N ****************/

/*============================================================================*/
/**
 * @brief  Case-insensitive string comparison (portable)
 */
static int ff_strcasecmp(const char *a, const char *b)
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
 * @brief  Convert Flipper hex bytes (little-endian) to uint16.
 *         Flipper format: "07 00 00 00" means value 0x0007
 * @param  bytes  parsed byte array
 * @param  count  number of bytes
 * @return uint16 value (first two bytes, little-endian)
 */
static uint16_t ff_hex_bytes_to_uint16_le(const uint8_t *bytes, uint8_t count)
{
	uint16_t val = 0;

	if (count >= 1)
		val = bytes[0];
	if (count >= 2)
		val |= (uint16_t)((uint16_t)bytes[1] << 8);

	return val;
}

/*============================================================================*/
/**
 * @brief  Map Flipper protocol name to IRMP protocol ID
 * @param  name  Flipper protocol name string (e.g., "NEC", "Samsung32")
 * @return IRMP protocol ID, or IRMP_UNKNOWN_PROTOCOL if not found
 */
uint8_t flipper_ir_proto_to_irmp(const char *name)
{
	const ir_proto_map_t *entry;

	if (name == NULL)
		return IRMP_UNKNOWN_PROTOCOL;

	for (entry = ir_proto_table; entry->flipper_name != NULL; entry++)
	{
		if (ff_strcasecmp(name, entry->flipper_name) == 0)
			return entry->irmp_id;
	}

	return IRMP_UNKNOWN_PROTOCOL;
}

/*============================================================================*/
/**
 * @brief  Map IRMP protocol ID to Flipper protocol name string
 * @param  irmp_id  IRMP protocol ID
 * @return Flipper protocol name, or "Unknown" if not found
 */
const char *flipper_ir_irmp_to_proto(uint8_t irmp_id)
{
	const ir_proto_map_t *entry;

	for (entry = ir_proto_table; entry->flipper_name != NULL; entry++)
	{
		if (entry->irmp_id == irmp_id)
			return entry->flipper_name;
	}

	return "Unknown";
}

/*============================================================================*/
/**
 * @brief  Open a .ir file and validate the header
 * @param  ctx   flipper file context
 * @param  path  file path
 * @return true if file opened and header is valid
 */
bool flipper_ir_open(flipper_file_t *ctx, const char *path)
{
	if (!ff_open(ctx, path))
		return false;

	if (!ff_validate_header(ctx, FLIPPER_IR_FILETYPE, FLIPPER_IR_MIN_VERSION))
	{
		ff_close(ctx);
		return false;
	}

	return true;
}

/*============================================================================*/
/**
 * @brief  Parse one IR signal block using an ir_block_reader_t vtable.
 *
 *         Parsed signal format:
 *           name: Power
 *           type: parsed
 *           protocol: NEC
 *           address: 07 00 00 00
 *           command: 02 00 00 00
 *
 *         Raw signal format:
 *           name: Power
 *           type: raw
 *           frequency: 38000
 *           duty_cycle: 0.330000
 *           data: 9024 4512 579 552 ...
 *
 * @param  ops  Non-NULL reader vtable.
 * @param  ctx  Opaque context pointer forwarded to every vtable call.
 * @param  out  Output signal structure; zeroed on entry.
 * @return true if a valid signal was parsed, false at EOF or error.
 */
bool flipper_ir_parse_block(const ir_block_reader_t *ops, void *ctx, flipper_ir_signal_t *out)
{
	bool got_name = false;
	bool got_type = false;
	uint8_t hex_buf[4];
	uint8_t hex_count;

	if (ops == NULL || ctx == NULL || out == NULL)
		return false;

	memset(out, 0, sizeof(flipper_ir_signal_t));
	out->valid = false;

	/* Scan for the start of a signal block */
	while (ops->next(ctx))
	{
		/* Skip separator lines */
		if (ops->is_sep(ctx))
			continue;

		if (!ops->parse_kv(ctx))
			continue;

		/* Look for "name:" to start a signal block */
		if (!got_name)
		{
			if (ff_strcasecmp(ops->get_key(ctx), "name") == 0)
			{
				strncpy(out->name, ops->get_value(ctx), FLIPPER_IR_NAME_MAX_LEN - 1);
				out->name[FLIPPER_IR_NAME_MAX_LEN - 1] = '\0';
				got_name = true;
			}
			continue;
		}

		/* After name, expect type */
		if (!got_type)
		{
			if (ff_strcasecmp(ops->get_key(ctx), "type") == 0)
			{
				if (ff_strcasecmp(ops->get_value(ctx), "parsed") == 0)
					out->type = FLIPPER_IR_SIGNAL_PARSED;
				else if (ff_strcasecmp(ops->get_value(ctx), "raw") == 0)
					out->type = FLIPPER_IR_SIGNAL_RAW;
				else
					return false;  /* Unknown type */

				got_type = true;
			}
			continue;
		}

		/* Parse type-specific fields */
		if (out->type == FLIPPER_IR_SIGNAL_PARSED)
		{
			if (ff_strcasecmp(ops->get_key(ctx), "protocol") == 0)
			{
				out->parsed.protocol = flipper_ir_proto_to_irmp(ops->get_value(ctx));
			}
			else if (ff_strcasecmp(ops->get_key(ctx), "address") == 0)
			{
				hex_count = ir_parse_hex_bytes(ops->get_value(ctx), hex_buf, 4);
				out->parsed.address = ff_hex_bytes_to_uint16_le(hex_buf, hex_count);
			}
			else if (ff_strcasecmp(ops->get_key(ctx), "command") == 0)
			{
				hex_count = ir_parse_hex_bytes(ops->get_value(ctx), hex_buf, 4);
				out->parsed.command = ff_hex_bytes_to_uint16_le(hex_buf, hex_count);
				out->parsed.flags = 0;
				out->valid = true;
				return true;  /* Parsed signal complete */
			}
		}
		else /* FLIPPER_IR_SIGNAL_RAW */
		{
			if (ff_strcasecmp(ops->get_key(ctx), "frequency") == 0)
			{
				out->raw.frequency = (uint32_t)strtoul(ops->get_value(ctx), NULL, 10);
			}
			else if (ff_strcasecmp(ops->get_key(ctx), "duty_cycle") == 0)
			{
				/* Parse float manually for embedded compatibility */
				const char *p = ops->get_value(ctx);
				int whole = 0;
				int frac = 0;
				int frac_div = 1;
				char *dot;

				whole = (int)strtol(p, NULL, 10);
				dot = strchr(p, '.');
				if (dot != NULL)
				{
					const char *fp = dot + 1;
					while (*fp >= '0' && *fp <= '9')
					{
						frac = frac * 10 + (*fp - '0');
						frac_div *= 10;
						fp++;
					}
				}
				out->raw.duty_cycle = (float)whole + (float)frac / (float)frac_div;
			}
			else if (ff_strcasecmp(ops->get_key(ctx), "data") == 0)
			{
				out->raw.sample_count = ir_parse_int32_array(
					ops->get_value(ctx),
					out->raw.samples,
					FLIPPER_IR_RAW_MAX_SAMPLES
				);
				out->valid = (out->raw.sample_count > 0);
				return true;  /* Raw signal complete */
			}
		}
	}

	/* If we got a partial signal at EOF, check if it is valid */
	if (got_name && got_type && out->valid)
		return true;

	return false;
}

/*============================================================================*/
/* FatFS adapter — thin wrappers so flipper_ir_read_signal() can delegate to  */
/* flipper_ir_parse_block() without exposing FatFS details to the pure parser. */
/*============================================================================*/
static bool ff_next_wrap(void *ctx)     { return ff_read_line((flipper_file_t *)ctx); }
static bool ff_is_sep_wrap(void *ctx)   { return ff_is_separator((flipper_file_t *)ctx); }
static bool ff_parse_kv_wrap(void *ctx) { return ff_parse_kv((flipper_file_t *)ctx); }
static const char *ff_get_key_wrap(void *ctx)   { return ff_get_key((flipper_file_t *)ctx); }
static const char *ff_get_val_wrap(void *ctx)   { return ff_get_value((flipper_file_t *)ctx); }

static const ir_block_reader_t s_ff_reader = {
	ff_next_wrap, ff_is_sep_wrap, ff_parse_kv_wrap, ff_get_key_wrap, ff_get_val_wrap
};

bool flipper_ir_read_signal(flipper_file_t *ctx, flipper_ir_signal_t *out)
{
	if (ctx == NULL)
		return false;
	return flipper_ir_parse_block(&s_ff_reader, ctx, out);
}


/*============================================================================*/
/**
 * @brief  Write the .ir file header
 */
bool flipper_ir_write_header(flipper_file_t *ctx)
{
	if (ctx == NULL)
		return false;

	if (!ff_write_kv_str(ctx, "Filetype", FLIPPER_IR_FILETYPE))
		return false;
	if (!ff_write_kv_uint32(ctx, "Version", 1))
		return false;

	return true;
}

/*============================================================================*/
/**
 * @brief  Write a single IR signal to a .ir file
 */
bool flipper_ir_write_signal(flipper_file_t *ctx, const flipper_ir_signal_t *sig)
{
	if (ctx == NULL || sig == NULL)
		return false;

	/* Write separator before each signal */
	if (!ff_write_separator(ctx))
		return false;

	/* Write name */
	if (!ff_write_kv_str(ctx, "name", sig->name))
		return false;

	if (sig->type == FLIPPER_IR_SIGNAL_PARSED)
	{
		uint8_t addr_bytes[4];
		uint8_t cmd_bytes[4];

		if (!ff_write_kv_str(ctx, "type", "parsed"))
			return false;

		if (!ff_write_kv_str(ctx, "protocol", flipper_ir_irmp_to_proto(sig->parsed.protocol)))
			return false;

		/* Convert uint16 address to 4-byte little-endian hex */
		addr_bytes[0] = (uint8_t)(sig->parsed.address & 0xFF);
		addr_bytes[1] = (uint8_t)((sig->parsed.address >> 8) & 0xFF);
		addr_bytes[2] = 0;
		addr_bytes[3] = 0;
		if (!ff_write_kv_hex(ctx, "address", addr_bytes, 4))
			return false;

		/* Convert uint16 command to 4-byte little-endian hex */
		cmd_bytes[0] = (uint8_t)(sig->parsed.command & 0xFF);
		cmd_bytes[1] = (uint8_t)((sig->parsed.command >> 8) & 0xFF);
		cmd_bytes[2] = 0;
		cmd_bytes[3] = 0;
		if (!ff_write_kv_hex(ctx, "command", cmd_bytes, 4))
			return false;
	}
	else /* FLIPPER_IR_SIGNAL_RAW */
	{
		uint16_t i;
		int pos;
		char buf[FF_VALUE_MAX_LEN];

		if (!ff_write_kv_str(ctx, "type", "raw"))
			return false;

		if (!ff_write_kv_uint32(ctx, "frequency", sig->raw.frequency))
			return false;

		if (!ff_write_kv_float(ctx, "duty_cycle", sig->raw.duty_cycle))
			return false;

		/* Write data as space-separated int32 values */
		pos = 0;
		for (i = 0; i < sig->raw.sample_count; i++)
		{
			int written = snprintf(&buf[pos], sizeof(buf) - (size_t)pos,
			                       "%s%ld",
			                       (i > 0) ? " " : "",
			                       (long)sig->raw.samples[i]);
			if (written < 0 || (pos + written) >= (int)(sizeof(buf) - 1))
				break;
			pos += written;
		}
		buf[pos] = '\0';

		if (!ff_write_kv_str(ctx, "data", buf))
			return false;
	}

	return true;
}

/*============================================================================*/
/**
 * @brief  Count the number of IR signals in a .ir file without loading all data
 * @param  path  file path
 * @return signal count, or 0 on error
 */
uint16_t flipper_ir_count_signals(const char *path)
{
	flipper_file_t ff;
	uint16_t count = 0;

	if (!flipper_ir_open(&ff, path))
		return 0;

	while (ff_read_line(&ff))
	{
		if (ff_is_separator(&ff))
			continue;

		if (ff_parse_kv(&ff))
		{
			if (ff_strcasecmp(ff_get_key(&ff), "name") == 0)
				count++;
		}
	}

	ff_close(&ff);
	return count;
}

/*============================================================================*/
/* Internal streaming-rewrite helper                                          */
/*============================================================================*/

/* Max FatFS path length handled by the rewrite helpers */
#define FLIPPER_IR_PATH_MAX  120

/**
 * @brief  Streaming rewrite: copy all signals to a temp file, optionally
 *         skipping or renaming one signal at @p target_idx.
 *
 * When @p skip is true, the signal at @p target_idx is omitted (delete).
 * When @p skip is false and @p rename_to is non-NULL, the signal name is
 * replaced with @p rename_to (rename).
 * When @p append_sig is non-NULL, it is written after all existing signals.
 *
 * On success, the original file is replaced by the temp file.
 * On failure, the temp file is deleted and the original is left untouched.
 *
 * @param path       FatFS path to the original .ir file.
 * @param target_idx Signal index to skip/rename.  UINT16_MAX = "no target"
 *                   (used for pure-append; skip and rename are ignored).
 * @param skip       Omit the target signal when true.
 * @param rename_to  New name for the target signal (NULL = keep name).
 * @param append_sig Signal to append after all existing signals (NULL = none).
 * @retval true   on success.
 * @retval false  on I/O error or out-of-range index.
 */
static bool flipper_ir_stream_rewrite(const char *path,
                                       uint16_t    target_idx,
                                       bool        skip,
                                       const char *rename_to,
                                       const flipper_ir_signal_t *append_sig)
{
	char tmp[FLIPPER_IR_PATH_MAX + 2];
	flipper_file_t src, dst;
	flipper_ir_signal_t sig;
	uint16_t n = 0;
	bool target_seen = false;
	bool ok = true;

	if (snprintf(tmp, sizeof(tmp), "%s~", path) >= (int)(sizeof(tmp) - 1))
		return false;

	if (!flipper_ir_open(&src, path))
		return false;

	if (!ff_open_write(&dst, tmp))
	{
		ff_close(&src);
		return false;
	}

	if (!flipper_ir_write_header(&dst))
	{
		ok = false;
		goto done;
	}

	memset(&sig, 0, sizeof(sig));
	while (flipper_ir_read_signal(&src, &sig))
	{
		if (n == target_idx)
		{
			target_seen = true;
			if (!skip)
			{
				if (rename_to != NULL)
					snprintf(sig.name, FLIPPER_IR_NAME_MAX_LEN, "%s", rename_to);
				if (!flipper_ir_write_signal(&dst, &sig))
					ok = false;
			}
		}
		else
		{
			if (!flipper_ir_write_signal(&dst, &sig))
				ok = false;
		}
		n++;
		if (!ok)
			break;
		memset(&sig, 0, sizeof(sig));
	}

	/* target_idx == UINT16_MAX means "no target" (pure append) */
	if (target_idx != UINT16_MAX && !target_seen)
		ok = false;  /* idx was out of range */

	/* Append extra signal if requested */
	if (ok && append_sig != NULL)
	{
		if (!flipper_ir_write_signal(&dst, append_sig))
			ok = false;
	}

done:
	ff_close(&src);
	ff_close(&dst);

	if (ok)
	{
		char bak[FLIPPER_IR_PATH_MAX + 6];

		/* Use a backup name so a failed rename can't lose the original file. */
		if (snprintf(bak, sizeof(bak), "%s.bak", path) >= (int)(sizeof(bak) - 1))
		{
			ok = false;
			(void)f_unlink(tmp);
		}
		else
		{
			(void)f_unlink(bak);
			if (f_rename(path, bak) != FR_OK)
			{
				ok = false;
				(void)f_unlink(tmp);
			}
			else if (f_rename(tmp, path) != FR_OK)
			{
				(void)f_rename(bak, path); /* best-effort rollback */
				ok = false;
				(void)f_unlink(tmp);
			}

			if (ok)
				(void)f_unlink(bak);
		}
	}
	else
	{
		f_unlink(tmp);
	}
	return ok;
}

/*============================================================================*/

bool flipper_ir_rename_signal(const char *path, uint16_t idx, const char *new_name)
{
	if (path == NULL || new_name == NULL || new_name[0] == '\0')
		return false;
	return flipper_ir_stream_rewrite(path, idx, false, new_name, NULL);
}

bool flipper_ir_delete_signal(const char *path, uint16_t idx)
{
	if (path == NULL)
		return false;
	return flipper_ir_stream_rewrite(path, idx, true, NULL, NULL);
}

bool flipper_ir_append_signal(const char *path, const flipper_ir_signal_t *sig)
{
	if (path == NULL || sig == NULL || !sig->valid)
		return false;
	return flipper_ir_stream_rewrite(path, UINT16_MAX, false, NULL, sig);
}

/*============================================================================*/
/* Raw-signal accumulator                                                     */
/*============================================================================*/

void flipper_ir_raw_feed_init(flipper_ir_raw_feed_t *f, const char *name,
                               uint32_t freq_hz, float duty_cycle)
{
	if (f == NULL)
		return;
	memset(f, 0, sizeof(*f));
	snprintf(f->sig.name, FLIPPER_IR_NAME_MAX_LEN, "%s", name ? name : "Signal");
	f->sig.type           = FLIPPER_IR_SIGNAL_RAW;
	f->sig.raw.frequency  = freq_hz;
	f->sig.raw.duty_cycle = duty_cycle;
}

bool flipper_ir_raw_feed_push(flipper_ir_raw_feed_t *f, int32_t sample_us)
{
	if (f == NULL || f->overflow)
		return false;
	if (f->sig.raw.sample_count >= FLIPPER_IR_RAW_MAX_SAMPLES)
	{
		f->overflow = true;
		return false;
	}
	f->sig.raw.samples[f->sig.raw.sample_count++] = sample_us;
	return true;
}

bool flipper_ir_raw_feed_finish(flipper_ir_raw_feed_t *f)
{
	if (f == NULL || f->overflow || f->sig.raw.sample_count == 0)
		return false;
	/* Ensure the final sample is a space (negative) for proper demodulator
	 * termination.  If the last pushed sample is a mark, append a trailing
	 * silence that is long enough to end the frame. */
	if (f->sig.raw.samples[f->sig.raw.sample_count - 1] > 0)
	{
		if (f->sig.raw.sample_count < FLIPPER_IR_RAW_MAX_SAMPLES)
			f->sig.raw.samples[f->sig.raw.sample_count++] = -9000;
		else
		{
			f->overflow = true;
			return false;
		}
	}
	f->sig.valid = true;
	return true;
}
