/* See COPYING.txt for license details. */

/*
 * file_browser_impl.h — host-testable interface for file browser
 *                        pure-logic functions.
 */

#ifndef FILE_BROWSER_IMPL_H_
#define FILE_BROWSER_IMPL_H_

#include <stdint.h>

/* Mirrors FatFS AM_DIR attribute bit */
#define FB_AM_DIR  0x10

/* Max filename length — mirrors FF_LFN_BUF */
#define FB_FNAME_MAX  255

/* Sorted entry — mirrors m1_file_browser.c::fb_sorted_entry_t */
typedef struct {
    char    fname[FB_FNAME_MAX + 1];
    uint8_t fattrib;
} fb_sorted_entry_t;

/* File extension classification — mirrors S_M1_file_browser_ext */
typedef enum {
    FB_F_EXT_DATA = 0,
    FB_F_EXT_OTHER
} fb_file_ext_t;

/**
 * @brief  Convert ASCII uppercase to lowercase.
 */
int fb_char_lower(int c);

/**
 * @brief  Comparator for qsort: directories first, then alphabetical
 *         (case-insensitive).
 */
int fb_entry_cmp(const void *a, const void *b);

/**
 * @brief  Check if file_name has an extension found in file_ext.
 */
uint8_t fb_find_ext(char *file_name, const char *file_ext);

/**
 * @brief  Classify a filename by its extension.
 */
fb_file_ext_t fb_get_file_type(char *filename);

/**
 * @brief  Concatenate num strings with '/' separator.
 */
uint8_t fb_dyn_strcat(char *buffer, uint8_t num, const char *format, ...);

#endif /* FILE_BROWSER_IMPL_H_ */
