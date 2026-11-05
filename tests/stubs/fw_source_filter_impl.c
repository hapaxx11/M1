/* Standalone build of fw_source_asset_matches_filter() for host-side testing.
 *
 * The production implementation lives in m1_fw_source.c which has heavy
 * HAL/HTTP/FatFS dependencies.  This file provides an identical copy that
 * can be compiled without any firmware headers, enabling host-side testing
 * of the pure-logic filter function.
 *
 * When the production code changes, this file must be updated to match.
 * The test suite validates that this implementation behaves correctly. */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "m1_fw_source.h"

bool fw_source_asset_matches_filter(const char *asset_name,
                                     const char *include_filter,
                                     const char *exclude_filter)
{
	size_t alen;

	if (!asset_name || !asset_name[0])
		return false;

	alen = strlen(asset_name);

	/* Must match include filter (suffix) */
	if (include_filter && include_filter[0])
	{
		size_t flen = strlen(include_filter);
		if (alen < flen)
			return false;
		if (strcmp(asset_name + alen - flen, include_filter) != 0)
			return false;
	}

	/* Must not match any exclude filter (space-separated suffixes) */
	if (exclude_filter && exclude_filter[0])
	{
		char excl_copy[FW_SOURCE_EXCLUDE_LEN];
		strncpy(excl_copy, exclude_filter, sizeof(excl_copy) - 1);
		excl_copy[sizeof(excl_copy) - 1] = '\0';

		char *tok = strtok(excl_copy, " ");
		while (tok)
		{
			size_t tlen = strlen(tok);
			if (alen >= tlen && strcmp(asset_name + alen - tlen, tok) == 0)
				return false;
			tok = strtok(NULL, " ");
		}
	}

	return true;
}

bool fw_source_asset_matches_source(const fw_source_t *source,
                                    const char *asset_name)
{
	if (!source)
		return false;
	if (fw_source_asset_matches_filter(asset_name, source->asset_filter,
	                                   source->exclude_filter))
		return true;

	if (strcmp(source->github.owner, "bedge117") == 0 &&
	    strcmp(source->github.repo, "m1-esp32-brain") == 0 &&
	    strcmp(source->asset_filter, "factory_m1-esp32-brain.bin") == 0)
	{
		return fw_source_asset_matches_filter(asset_name, "factory_ESP32C6-SPI.bin",
		                                      source->exclude_filter);
	}
	return false;
}
