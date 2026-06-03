/* See COPYING.txt for license details. */

#ifndef M1_ESP32_SCRATCH_H_
#define M1_ESP32_SCRATCH_H_

#include <stddef.h>
#include <stdbool.h>

/*
 * esp32_fw_ensure_scratch() - pure, host-testable guard for the scratch
 * buffers used by setting_esp32_image_file().
 *
 * That function calls storage_browse() to pick the image file; entering the
 * file-browser scene pops the ESP32-update scene and runs setting_esp32_exit(),
 * which frees pfullpath / pfilename_md5.  Using them afterwards without
 * re-allocation writes to a NULL pointer and HardFaults.
 *
 * This helper re-allocates any buffer that is NULL and returns true only when
 * BOTH are valid afterwards; the caller MUST bail (not crash) when it returns
 * false.  The allocator is injected so the logic is unit-testable on the host.
 */
typedef void *(*esp32_scratch_alloc_fn)(size_t);

static inline bool esp32_fw_ensure_scratch(char **pfullpath,
                                           char **pfilename_md5,
                                           size_t path_sz,
                                           size_t name_sz,
                                           esp32_scratch_alloc_fn alloc)
{
    if (*pfullpath == NULL)
        *pfullpath = (char *)alloc(path_sz);
    if (*pfilename_md5 == NULL)
        *pfilename_md5 = (char *)alloc(name_sz);
    return (*pfullpath != NULL) && (*pfilename_md5 != NULL);
}

#endif /* M1_ESP32_SCRATCH_H_ */
