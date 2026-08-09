/* See COPYING.txt for license details. */

/*
 * lfrfid_uid_copy.c — see lfrfid_uid_copy.h.
 */

#include "lfrfid_uid_copy.h"

size_t lfrfid_uid_copy_len(size_t src_len, size_t dst_cap)
{
    return (src_len > dst_cap) ? dst_cap : src_len;
}
