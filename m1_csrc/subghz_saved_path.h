/* See COPYING.txt for license details. */

#ifndef SUBGHZ_SAVED_PATH_H
#define SUBGHZ_SAVED_PATH_H

#include <stdbool.h>
#include <stddef.h>

bool subghz_saved_path_build(char *output,
                             size_t output_size,
                             const char *directory,
                             const char *filename);

#endif
