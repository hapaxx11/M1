/* See COPYING.txt for license details. */

/*
 * lfrfid_uid_copy.h — pure clamp helper for copying decoded .rfid "HData" hex
 * bytes into the fixed-size LFRFIDTagInfo.uid[] buffer.
 *
 * Hardware-independent: no HAL/RTOS/FatFs includes, safe to unit test directly.
 */

#ifndef LFRFID_UID_COPY_H_
#define LFRFID_UID_COPY_H_

#include <stddef.h>

/*
 * @brief Clamp a source length to a destination buffer's capacity.
 * @param src_len Number of bytes decoded from the file (untrusted, may exceed dst_cap).
 * @param dst_cap Size in bytes of the destination buffer.
 * @return The number of bytes safe to copy into the destination (<= dst_cap).
 */
size_t lfrfid_uid_copy_len(size_t src_len, size_t dst_cap);

#endif /* LFRFID_UID_COPY_H_ */
