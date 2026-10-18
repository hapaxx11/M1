/* See COPYING.txt for license details. */

/**
 * @file   subghz_upload_entry.h
 * @brief  Shared upload-waveform entry type for the ProtoPirate encoders.
 *
 * One upload entry: a constant level held for `duration` microseconds.
 * Shared by the Renault V1 and PSA encoders so scene/TX code can handle both
 * waveforms through a single type.
 *
 * M1 Project — Hapax fork
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t duration;
    bool     level;  /* true = mark (high), false = space (low) */
} subghz_upload_entry_t;
