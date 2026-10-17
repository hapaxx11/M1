/* See COPYING.txt for license details. */

#ifndef SUBGHZ_RAW_PAIRS_WRITER_H
#define SUBGHZ_RAW_PAIRS_WRITER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "subghz_key_encoder.h"

typedef bool (*subghz_raw_pairs_write_line_fn)(void *context, const char *line);

bool subghz_raw_pairs_write_data(const SubGhzRawPair *pairs, uint32_t pair_count,
                                uint32_t pairs_per_rep, char *line,
                                size_t line_capacity,
                                subghz_raw_pairs_write_line_fn write_line,
                                void *context);

#endif
