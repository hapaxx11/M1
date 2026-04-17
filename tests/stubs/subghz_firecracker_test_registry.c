/* Minimal SubGhz protocol registry stub for test_firecracker_cm17a.
 *
 * Provides a one-entry registry at index 0 with the timing parameters
 * used by the FireCracker CM17A decoder (te_short=562, te_long=1688,
 * te_delta=100).  The test calls subghz_decode_firecracker_cm17a(0, ...)
 * which reads subghz_protocol_registry[0] for timing — this stub satisfies
 * that lookup without linking the entire production registry (which would
 * require stubs for all ~100 other decoder functions).
 *
 * Also provides subghz_block_generic_commit_to_m1() — the FireCracker
 * decoder calls this on successful decode to commit results into
 * subghz_decenc_ctl.  The real implementation (subghz_block_generic.c)
 * has no extra dependencies so we include it directly via the CMake target;
 * this stub is NOT needed for that function — see CMakeLists.txt.
 */

#include "subghz_protocol_registry.h"
#include <stdint.h>

/* One-entry registry: FireCracker at index 0, matching production timing. */
const SubGhzProtocolDef subghz_protocol_registry[] = {
    [0] = {
        .name   = "FireCracker",
        .type   = SubGhzProtocolTypeStatic,
        .timing = { .te_short = 562, .te_long = 1688, .te_delta = 100,
                    .min_count_bit_for_found = 40 },
        .decode = NULL,  /* not used in decode path */
    },
};

const uint16_t subghz_protocol_registry_count = 1;

/* Provide lookup stubs required by the linker (not called by decoder). */
int16_t subghz_protocol_find_by_name(const char *name)
{
    (void)name;
    return -1;
}

const SubGhzProtocolDef* subghz_protocol_get(uint16_t index)
{
    if (index < subghz_protocol_registry_count)
        return &subghz_protocol_registry[index];
    return NULL;
}

const char* subghz_protocol_get_name(uint16_t index)
{
    if (index < subghz_protocol_registry_count)
        return subghz_protocol_registry[index].name;
    return NULL;
}
