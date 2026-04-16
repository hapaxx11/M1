/* Standalone build of str_contains_icase() for host-side testing.
 *
 * The production implementation lives in m1_ir_universal.c, which has
 * heavy HAL/FatFS/RTOS dependencies.  This file provides an identical
 * copy that compiles without any firmware headers, enabling host-side
 * unit testing of the pure-logic case-insensitive substring matcher.
 *
 * When the production code changes, this file must be updated to match. */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

bool str_contains_icase(const char *haystack, const char *needle)
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
