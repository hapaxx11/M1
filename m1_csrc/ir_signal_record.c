/* See COPYING.txt for license details. */

/**
 * @file   ir_signal_record.c
 * @brief  Pure-logic IR signal record helpers — no HAL, RTOS, FatFS, or display deps.
 *
 * All functions are hardware-independent and testable on the host.
 *
 * M1 Project
 */

#include "ir_signal_record.h"

#include <string.h>

/* =========================================================================
 * Protocol mapping
 * =========================================================================*/

uint8_t ir_map_flipper_protocol(const char *name)
{
	if (name == NULL)
		return 0;

	/* Common Flipper protocol names mapped to IRMP IDs.
	 * Must stay in sync with flipper_ir.c ir_proto_table[]. */
	if (strcmp(name, "NEC") == 0 || strcmp(name, "NECext") == 0)
		return IRMP_NEC_PROTOCOL;
	if (strcmp(name, "Samsung48") == 0)
		return IRMP_SAMSUNG48_PROTOCOL;
	if (strcmp(name, "Samsung32") == 0 || strcmp(name, "Samsung") == 0)
		return IRMP_SAMSUNG32_PROTOCOL;
	if (strcmp(name, "RC5") == 0 || strcmp(name, "RC5X") == 0)
		return IRMP_RC5_PROTOCOL;
	if (strcmp(name, "RC6") == 0)
		return IRMP_RC6_PROTOCOL;
	if (strcmp(name, "Sony12") == 0 || strcmp(name, "Sony15") == 0 || strcmp(name, "Sony20") == 0 ||
	    strcmp(name, "SIRC") == 0 || strcmp(name, "SIRC15") == 0 || strcmp(name, "SIRC20") == 0)
		return IRMP_SIRCS_PROTOCOL;
	if (strcmp(name, "Kaseikyo") == 0 || strcmp(name, "Panasonic") == 0)
		return IRMP_KASEIKYO_PROTOCOL;
	if (strcmp(name, "NEC42") == 0 || strcmp(name, "NEC42ext") == 0)
		return IRMP_NEC42_PROTOCOL;
	if (strcmp(name, "Denon") == 0 || strcmp(name, "Sharp") == 0)
		return IRMP_DENON_PROTOCOL;
	if (strcmp(name, "JVC") == 0)
		return IRMP_JVC_PROTOCOL;
	if (strcmp(name, "LG") == 0)
		return IRMP_LGAIR_PROTOCOL;
	if (strcmp(name, "Pioneer") == 0)
		return IRMP_NEC_PROTOCOL;
	if (strcmp(name, "Apple") == 0)
		return IRMP_APPLE_PROTOCOL;
	if (strcmp(name, "Bose") == 0)
		return IRMP_BOSE_PROTOCOL;
	if (strcmp(name, "Nokia") == 0)
		return IRMP_NOKIA_PROTOCOL;
	if (strcmp(name, "RCA") == 0)
		return IRMP_RCCAR_PROTOCOL;
	if (strcmp(name, "RCMM") == 0)
		return IRMP_RCMM32_PROTOCOL;

	return 0; /* Unknown protocol */
}

/* =========================================================================
 * File-name helpers
 * =========================================================================*/

bool ir_is_ir_file(const char *fname)
{
	size_t len;

	if (fname == NULL)
		return false;

	len = strlen(fname);
	if (len < 4)
		return false;

	return (strcmp(&fname[len - 3], ".ir") == 0);
}

/* =========================================================================
 * Path manipulation
 * =========================================================================*/

void ir_path_append(char *base, const char *item, size_t base_size)
{
	size_t len;

	if (base == NULL || item == NULL || base_size == 0)
		return;

	len = strlen(base);

	if (len > 0 && base[len - 1] != '/')
	{
		if (len + 1 < base_size)
		{
			base[len] = '/';
			base[len + 1] = '\0';
			len++;
		}
	}

	strncat(base, item, base_size - len - 1);
	base[base_size - 1] = '\0';
}

void ir_path_go_up(char *path)
{
	size_t len;
	int i;

	if (path == NULL)
		return;

	len = strlen(path);
	if (len == 0)
		return;

	/* Remove trailing slash */
	if (path[len - 1] == '/')
	{
		path[len - 1] = '\0';
		len--;
	}

	/* Find the last slash and truncate */
	for (i = (int)len - 1; i >= 0; i--)
	{
		if (path[i] == '/')
		{
			path[i] = '\0';
			return;
		}
	}

	/* No slash found - clear the path */
	path[0] = '\0';
}

/* =========================================================================
 * String helpers
 * =========================================================================*/

bool ir_str_contains_icase(const char *haystack, const char *needle)
{
	size_t hl, nl, i, j;
	char h, n;

	if (needle == NULL || needle[0] == '\0')
		return true;
	if (haystack == NULL)
		return false;

	hl = strlen(haystack);
	nl = strlen(needle);
	if (nl > hl)
		return false;

	for (i = 0; i <= hl - nl; i++)
	{
		bool match = true;
		for (j = 0; j < nl; j++)
		{
			h = haystack[i + j];
			n = needle[j];
			if (h >= 'A' && h <= 'Z')
				h = (char)(h + 32);
			if (n >= 'A' && n <= 'Z')
				n = (char)(n + 32);
			if (h != n)
			{
				match = false;
				break;
			}
		}
		if (match)
			return true;
	}
	return false;
}

/* =========================================================================
 * Hex byte parser
 * =========================================================================*/

#include <ctype.h>
#include <stdlib.h>

uint8_t ir_parse_hex_bytes(const char *str, uint8_t *out, uint8_t max_len)
{
	uint8_t count = 0;
	const char *p = str;

	if (str == NULL || out == NULL || max_len == 0)
		return 0;

	while (*p != '\0' && count < max_len)
	{
		while (*p == ' ' || *p == '\t')
			p++;

		if (*p == '\0')
			break;

		if (isxdigit((unsigned char)p[0]) && isxdigit((unsigned char)p[1]))
		{
			unsigned int val = 0;
			char hex[3] = { p[0], p[1], '\0' };
			val = (unsigned int)strtoul(hex, NULL, 16);
			out[count++] = (uint8_t)val;
			p += 2;
		}
		else
		{
			break;
		}
	}

	return count;
}

/* =========================================================================
 * KV-reader block parser
 * =========================================================================*/

bool ir_cmd_parse(const ir_block_reader_t *ops, void *ctx, ir_universal_cmd_t *cmd)
{
	bool got_name    = false;
	bool got_type    = false;
	bool is_parsed   = false;
	bool is_raw_type = false;

	if (ops == NULL || ctx == NULL || cmd == NULL)
		return false;

	memset(cmd, 0, sizeof(ir_universal_cmd_t));

	while (ops->next(ctx))
	{
		if (ops->is_sep(ctx))
			break; /* end of this signal block */

		if (!ops->parse_kv(ctx))
			continue;

		const char *k = ops->get_key(ctx);
		const char *v = ops->get_value(ctx);

		if (strcmp(k, "name") == 0)
		{
			strncpy(cmd->name, v, IR_CMD_NAME_MAX_LEN - 1);
			cmd->name[IR_CMD_NAME_MAX_LEN - 1] = '\0';
			got_name = true;
		}
		else if (strcmp(k, "type") == 0)
		{
			got_type = true;
			if (strcmp(v, "parsed") == 0)
			{
				is_parsed   = true;
				is_raw_type = false;
			}
			else if (strcmp(v, "raw") == 0)
			{
				is_parsed   = false;
				is_raw_type = true;
			}
		}
		else if (strcmp(k, "protocol") == 0 && is_parsed)
		{
			cmd->protocol = ir_map_flipper_protocol(v);
		}
		else if (strcmp(k, "address") == 0 && is_parsed)
		{
			uint8_t hex_buf[4];
			uint8_t n = ir_parse_hex_bytes(v, hex_buf, 4);
			cmd->address = (n >= 2) ? (uint16_t)(hex_buf[0] | ((uint16_t)hex_buf[1] << 8))
			                        : (uint16_t)hex_buf[0];
		}
		else if (strcmp(k, "command") == 0 && is_parsed)
		{
			uint8_t hex_buf[4];
			uint8_t n = ir_parse_hex_bytes(v, hex_buf, 4);
			cmd->command = (n >= 2) ? (uint16_t)(hex_buf[0] | ((uint16_t)hex_buf[1] << 8))
			                        : (uint16_t)hex_buf[0];
		}
		else if (strcmp(k, "frequency") == 0 && is_raw_type)
		{
			cmd->raw_freq = (uint32_t)strtoul(v, NULL, 10);
		}
		else if (strcmp(k, "data") == 0 && is_raw_type)
		{
			const char *p = v;
			uint16_t count = 0;
			while (*p)
			{
				while (*p == ' ')
					p++;
				if (*p == '\0')
					break;
				count++;
				while (*p && *p != ' ')
					p++;
			}
			cmd->raw_count = count;
		}
	}

	if (got_name && got_type)
	{
		if (is_parsed && cmd->protocol != 0)
		{
			cmd->is_raw = false;
			cmd->flags  = 0;
			cmd->valid  = true;
			return true;
		}
		if (is_raw_type && cmd->raw_freq > 0)
		{
			cmd->is_raw = true;
			cmd->valid  = true;
			return true;
		}
	}

	return false;
}
