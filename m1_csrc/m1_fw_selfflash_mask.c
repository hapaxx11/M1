/* See COPYING.txt for license details. */

/*
 * m1_fw_selfflash_mask.c
 *
 * See m1_fw_selfflash_mask.h for background. Pure logic, no hardware access.
 *
 * M1 Project
 */

#include "m1_fw_selfflash_mask.h"

void fw_selfflash_mask_init(fw_selfflash_mask_state_t *st)
{
    st->masked = false;
}

void fw_selfflash_mask_begin(fw_selfflash_mask_state_t *st)
{
    st->masked = true;
}

bool fw_selfflash_mask_end(fw_selfflash_mask_state_t *st)
{
    if (st->masked)
    {
        st->masked = false;
        return true;
    }
    return false;
}
