/* See COPYING.txt for license details. */

/*
 * m1_fw_source.h
 *
 * Firmware download source configuration and management.
 * Reads user-configurable source list from SD card and dispatches
 * to source-type handlers (GitHub releases, direct URLs, etc.)
 *
 * M1 Project
 */

#ifndef M1_FW_SOURCE_H_
#define M1_FW_SOURCE_H_

#include <stdint.h>
#include <stdbool.h>
#include "m1_http_client.h"

/* Maximum number of configured sources */
#define FW_SOURCE_MAX           8

/* Maximum releases to fetch per source */
#define FW_RELEASE_MAX          5

/* String lengths */
#define FW_SOURCE_NAME_LEN      48
#define FW_SOURCE_OWNER_LEN     32
#define FW_SOURCE_REPO_LEN      32
#define FW_SOURCE_FILTER_LEN    32
#define FW_SOURCE_EXCLUDE_LEN   64
#define FW_SOURCE_CATEGORY_LEN  16
#define FW_RELEASE_TAG_LEN      24
#define FW_RELEASE_NAME_LEN     48
#define FW_RELEASE_URL_LEN      192
#define FW_RELEASE_ASSET_LEN    64

/* SD card path for source configuration */
#define FW_SOURCES_CONFIG_PATH  "0:/System/fw_sources.txt"

/* Source type identifiers (extensible) */
typedef enum {
	FW_SOURCE_GITHUB_RELEASE = 0,
	/* Future: FW_SOURCE_DIRECT_URL, FW_SOURCE_GITLAB, etc. */
	FW_SOURCE_TYPE_COUNT
} fw_source_type_t;

/* GitHub release-specific parameters */
typedef struct {
	char owner[FW_SOURCE_OWNER_LEN];
	char repo[FW_SOURCE_REPO_LEN];
} fw_github_params_t;

/* A configured firmware source */
typedef struct {
	char name[FW_SOURCE_NAME_LEN];
	fw_source_type_t type;
	fw_github_params_t github;      /* type-specific params */
	char asset_filter[FW_SOURCE_FILTER_LEN];    /* suffix match, e.g., "_wCRC.bin" */
	char exclude_filter[FW_SOURCE_EXCLUDE_LEN]; /* space-separated exclusions */
	char category[FW_SOURCE_CATEGORY_LEN];      /* "firmware" (default) or "esp32" */
	uint8_t max_releases;
} fw_source_t;

/* A single release entry */
typedef struct {
	char tag[FW_RELEASE_TAG_LEN];
	char name[FW_RELEASE_NAME_LEN];
	char asset_name[FW_RELEASE_ASSET_LEN];
	char download_url[FW_RELEASE_URL_LEN];
	uint32_t asset_size;
} fw_release_t;

/*
 * Load firmware sources from the config file on SD card.
 * If the file doesn't exist, creates it with default sources.
 *
 * sources:   Array to fill (must have space for FW_SOURCE_MAX entries)
 * Returns:   Number of sources loaded (0 on error)
 */
uint8_t fw_source_load_config(fw_source_t *sources);

/*
 * Load firmware sources filtered by category.
 * Only sources whose category matches the given string are returned.
 * Sources without a Category field default to "firmware".
 *
 * sources:   Array to fill (must have space for FW_SOURCE_MAX entries)
 * category:  Category string to match (e.g., "firmware" or "esp32")
 * Returns:   Number of matching sources loaded (0 on error or no match)
 */
uint8_t fw_source_load_config_filtered(fw_source_t *sources, const char *category);

/*
 * Create the default source configuration file on SD card.
 * Called automatically by fw_source_load_config if file is missing.
 */
bool fw_source_create_defaults(void);

/*
 * Check if an asset name matches the include/exclude filter criteria.
 * include_filter: suffix that asset name must end with (e.g., "_wCRC.bin")
 * exclude_filter: space-separated suffixes that disqualify the asset
 * Returns true if the asset should be included.
 */
bool fw_source_asset_matches_filter(const char *asset_name,
                                     const char *include_filter,
                                     const char *exclude_filter);

/*
 * Fetch available releases for a given source.
 * Queries the remote server (e.g., GitHub API) and populates the release list.
 *
 * source:      Source configuration
 * releases:    Array to fill (must have space for FW_RELEASE_MAX entries)
 * out_status:  Optional — receives the http_status_t result for error diagnostics
 *              (e.g. HTTP_ERR_TIMEOUT, HTTP_ERR_CONNECT_FAIL, HTTP_OK).
 *              This is NOT the HTTP response code (200/404/etc.).
 * Returns:     Number of releases found (0 on error or no releases)
 */
uint8_t fw_source_fetch_releases(const fw_source_t *source, fw_release_t *releases,
                                  http_status_t *out_status);

#endif /* M1_FW_SOURCE_H_ */
