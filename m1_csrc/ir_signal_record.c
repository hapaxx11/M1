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
