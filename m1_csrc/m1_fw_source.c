/* See COPYING.txt for license details. */

/*
 * m1_fw_source.c
 *
 * Firmware download source configuration and management.
 *
 * M1 Project
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "stm32h5xx_hal.h"
#include "main.h"
#include "m1_fw_source.h"
#include "m1_http_client.h"
#include "m1_json_mini.h"
#include "m1_file_browser.h"
#include "m1_log_debug.h"
#include "flipper_file.h"

#define FW_SRC_TAG "FW_SRC"

/* GitHub API URL template — releases list (returns JSON array) */
#define GITHUB_API_RELEASES_FMT "https://api.github.com/repos/%s/%s/releases?per_page=%u"

/*
 * GitHub API response buffer.
 * A single GitHub release JSON object is typically 2-4 KB; five releases
 * can reach 15-20 KB.  12 KB comfortably holds 3-5 releases for the
 * Hapax/C3/Monstatek repos whose release notes are CI-generated and brief.
 */
#define API_RESPONSE_BUF_SIZE  12288
static char s_api_buf[API_RESPONSE_BUF_SIZE];

/*
 * Default source configuration written when fw_sources.txt is missing.
 */
static const char *s_default_config =
	"# M1 Firmware Download Sources\n"
	"# Edit this file to add or modify download sources.\n"
	"# Supported Type values: github_release\n"
	"# Category: firmware (M1 STM32 firmware) or esp32 (ESP32-C6 AT firmware)\n"
	"#\n"
	"# Each source block starts with 'Source:' and ends at the next 'Source:' or EOF.\n"
	"# Fields: Source, Type, Owner, Repo, Asset_filter, Exclude_filter, Category, Max_releases\n"
	"\n"
	"Source: Monstatek Official\n"
	"Type: github_release\n"
	"Owner: Monstatek\n"
	"Repo: M1\n"
	"Asset_filter: .bin\n"
	"Exclude_filter:\n"
	"Category: firmware\n"
	"Max_releases: 5\n"
	"\n"
	"Source: Hapax\n"
	"Type: github_release\n"
	"Owner: hapaxx11\n"
	"Repo: M1\n"
	"Asset_filter: _wCRC.bin\n"
	"Exclude_filter: .elf .hex\n"
	"Category: firmware\n"
	"Max_releases: 5\n"
	"\n"
	"Source: C3\n"
	"Type: github_release\n"
	"Owner: bedge117\n"
	"Repo: M1\n"
	"Asset_filter: _wCRC.bin\n"
	"Exclude_filter:\n"
	"Category: firmware\n"
	"Max_releases: 5\n"
	"\n"
	"Source: C3 ESP32 AT\n"
	"Type: github_release\n"
	"Owner: bedge117\n"
	"Repo: esp32-at-monstatek-m1\n"
	"Asset_filter: .bin\n"
	"Exclude_filter:\n"
	"Category: esp32\n"
	"Max_releases: 5\n"
	"\n"
	"Source: Deauth ESP32 AT\n"
	"Type: github_release\n"
	"Owner: neddy299\n"
	"Repo: esp32-at-monstatek-m1\n"
	"Asset_filter: .bin\n"
	"Exclude_filter:\n"
	"Category: esp32\n"
	"Max_releases: 5\n";

bool fw_source_create_defaults(void)
{
	FIL file;
	FRESULT fr;
	UINT bw;

	/* Ensure System directory exists */
	m1_fb_make_dir("0:/System");

	fr = f_open(&file, FW_SOURCES_CONFIG_PATH, FA_CREATE_ALWAYS | FA_WRITE);
	if (fr != FR_OK)
		return false;

	uint16_t len = strlen(s_default_config);
	fr = f_write(&file, s_default_config, len, &bw);
	f_close(&file);

	return (fr == FR_OK && bw == len);
}

uint8_t fw_source_load_config(fw_source_t *sources)
{
	flipper_file_t ff;
	uint8_t count = 0;
	fw_source_t *cur = NULL;

	if (!sources)
		return 0;

	memset(sources, 0, sizeof(fw_source_t) * FW_SOURCE_MAX);

	/* Check if config file exists, create defaults if not */
	if (!m1_fb_check_existence(FW_SOURCES_CONFIG_PATH))
	{
		if (!fw_source_create_defaults())
			return 0;
	}

	if (!ff_open(&ff, FW_SOURCES_CONFIG_PATH))
		return 0;

	while (ff_read_line(&ff) && count < FW_SOURCE_MAX)
	{
		if (!ff_parse_kv(&ff))
			continue;

		const char *key = ff_get_key(&ff);
		const char *val = ff_get_value(&ff);

		if (strcmp(key, "Source") == 0)
		{
			/* Finalize previous source: default category if empty */
			if (cur && !cur->category[0])
				strncpy(cur->category, "firmware", FW_SOURCE_CATEGORY_LEN - 1);
			/* Start a new source entry */
			cur = &sources[count];
			memset(cur, 0, sizeof(fw_source_t));
			strncpy(cur->name, val, FW_SOURCE_NAME_LEN - 1);
			cur->max_releases = FW_RELEASE_MAX;
			count++;
		}
		else if (cur)
		{
			if (strcmp(key, "Type") == 0)
			{
				if (strcmp(val, "github_release") == 0)
					cur->type = FW_SOURCE_GITHUB_RELEASE;
			}
			else if (strcmp(key, "Owner") == 0)
				strncpy(cur->github.owner, val, FW_SOURCE_OWNER_LEN - 1);
			else if (strcmp(key, "Repo") == 0)
				strncpy(cur->github.repo, val, FW_SOURCE_REPO_LEN - 1);
			else if (strcmp(key, "Asset_filter") == 0)
				strncpy(cur->asset_filter, val, FW_SOURCE_FILTER_LEN - 1);
			else if (strcmp(key, "Exclude_filter") == 0)
				strncpy(cur->exclude_filter, val, FW_SOURCE_EXCLUDE_LEN - 1);
			else if (strcmp(key, "Category") == 0)
				strncpy(cur->category, val, FW_SOURCE_CATEGORY_LEN - 1);
			else if (strcmp(key, "Max_releases") == 0)
			{
				int n = atoi(val);
				if (n > 0 && n <= FW_RELEASE_MAX)
					cur->max_releases = (uint8_t)n;
			}
		}
	}

	/* Finalize last source: default category if empty */
	if (cur && !cur->category[0])
		strncpy(cur->category, "firmware", FW_SOURCE_CATEGORY_LEN - 1);

	ff_close(&ff);
	return count;
}

uint8_t fw_source_load_config_filtered(fw_source_t *sources, const char *category)
{
	fw_source_t all[FW_SOURCE_MAX];
	uint8_t total, out = 0;

	if (!sources || !category)
		return 0;

	total = fw_source_load_config(all);

	for (uint8_t i = 0; i < total && out < FW_SOURCE_MAX; i++)
	{
		if (strcmp(all[i].category, category) == 0)
		{
			memcpy(&sources[out], &all[i], sizeof(fw_source_t));
			out++;
		}
	}

	return out;
}

/*
 * Check if an asset name matches the filter criteria.
 * Returns true if the asset should be included.
 */
static bool asset_matches_filter(const char *asset_name,
                                  const char *include_filter,
                                  const char *exclude_filter)
{
	size_t alen = strlen(asset_name);

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

/*
 * Fetch releases from GitHub Releases API.
 * Parses the JSON response to extract release metadata and matching assets.
 */
static uint8_t github_fetch_releases(const fw_source_t *source, fw_release_t *releases)
{
	char url[256];
	uint8_t count = 0;
	http_status_t status;

	snprintf(url, sizeof(url), GITHUB_API_RELEASES_FMT,
	         source->github.owner, source->github.repo, source->max_releases);

	/* GitHub API requires Accept header — AT+HTTPCLIENT adds it automatically */
	status = http_get(url, s_api_buf, sizeof(s_api_buf), 30);
	if (status != HTTP_OK)
		return 0;

	/* The response is a JSON array: [{...}, {...}, ...] */
	/* Skip leading whitespace and '[' */
	const char *p = s_api_buf;
	while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
	if (*p != '[')
		return 0;
	p++;

	/* Iterate over release objects in the array */
	while (*p && count < source->max_releases)
	{
		/* Skip whitespace */
		while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
		if (*p == ']') break;
		if (*p == ',') { p++; continue; }
		if (*p != '{') break;

		/* Find the end of this release object */
		const char *obj_end = json_object_end(p);
		if (!obj_end) break;

		fw_release_t *rel = &releases[count];
		memset(rel, 0, sizeof(fw_release_t));

		/* Extract tag_name and name */
		json_get_string(p, "tag_name", rel->tag, FW_RELEASE_TAG_LEN);
		json_get_string(p, "name", rel->name, FW_RELEASE_NAME_LEN);

		/* Find assets array and look for matching asset */
		const char *assets = json_find_array(p, "assets");
		if (assets)
		{
			const char *ap = assets;
			while (ap && ap < obj_end)
			{
				if (*ap != '{')
				{
					ap = json_array_next(ap);
					continue;
				}

				char aname[FW_RELEASE_ASSET_LEN];
				aname[0] = '\0';
				json_get_string(ap, "name", aname, sizeof(aname));

				if (aname[0] && asset_matches_filter(aname, source->asset_filter, source->exclude_filter))
				{
					strncpy(rel->asset_name, aname, FW_RELEASE_ASSET_LEN - 1);
					rel->asset_name[FW_RELEASE_ASSET_LEN - 1] = '\0';
					json_get_string(ap, "browser_download_url", rel->download_url, FW_RELEASE_URL_LEN);
					int32_t size_val = 0;
					if (json_get_int(ap, "size", &size_val))
						rel->asset_size = (uint32_t)size_val;
					break; /* Use first matching asset */
				}

				ap = json_array_next(ap);
			}
		}

		/* Only include release if we found a matching asset */
		if (rel->download_url[0])
			count++;

		p = obj_end;
	}

	return count;
}

uint8_t fw_source_fetch_releases(const fw_source_t *source, fw_release_t *releases)
{
	if (!source || !releases)
		return 0;

	memset(releases, 0, sizeof(fw_release_t) * FW_RELEASE_MAX);

	switch (source->type)
	{
		case FW_SOURCE_GITHUB_RELEASE:
			return github_fetch_releases(source, releases);

		default:
			return 0;
	}
}
