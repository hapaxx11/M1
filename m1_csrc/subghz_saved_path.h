/* See COPYING.txt for license details. */

#ifndef SUBGHZ_SAVED_PATH_H
#define SUBGHZ_SAVED_PATH_H

#include <stdbool.h>
#include <stddef.h>

#define SUBGHZ_SAVED_PATH_MAX 128U
#define SUBGHZ_SAVED_FULL_PATH_MAX (SUBGHZ_SAVED_PATH_MAX + 3U)

bool subghz_saved_path_build(char *output,
                             size_t output_size,
                             const char *directory,
                             const char *filename);
bool subghz_saved_path_rename_build(char *output,
                                    size_t output_size,
                                    const char *existing_path,
                                    const char *new_filename);
bool subghz_saved_path_recovered_build(char *output,
                                       size_t output_size,
                                       const char *source_path);

#endif
