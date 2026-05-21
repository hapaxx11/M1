/* See COPYING.txt for license details. */

#ifndef M1_WATCHDOG_BOOT_LOOP_H_
#define M1_WATCHDOG_BOOT_LOOP_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t runtime_ctr;
    uint32_t stored_ctr;
    bool disable_iwdg;
} m1_wdt_boot_loop_eval_t;

static inline uint32_t m1_wdt_boot_loop_build_sig_components(
    uint32_t fw_major,
    uint32_t fw_minor,
    uint32_t fw_build,
    uint32_t fw_rc,
    uint32_t hapax_revision)
{
    uint32_t sig = 0x57445431u; /* "WDT1" */
    sig = (sig * 33u) ^ (uint32_t)(fw_major & 0xFFu);
    sig = (sig * 33u) ^ (uint32_t)(fw_minor & 0xFFu);
    sig = (sig * 33u) ^ (uint32_t)(fw_build & 0xFFu);
    sig = (sig * 33u) ^ (uint32_t)(fw_rc & 0xFFu);
    sig = (sig * 33u) ^ (uint32_t)(hapax_revision & 0xFFu);
    return sig;
}

static inline m1_wdt_boot_loop_eval_t m1_wdt_boot_loop_evaluate(
    uint32_t stored_ctr,
    uint32_t stored_sig,
    bool was_wdt_reset,
    uint32_t expected_sig,
    uint32_t boot_loop_max)
{
    m1_wdt_boot_loop_eval_t out = {0u, 0u, false};
    uint32_t loop_ctr = stored_ctr;

    if((stored_sig != expected_sig) || (loop_ctr > boot_loop_max))
    {
        loop_ctr = 0u;
    }

    if(was_wdt_reset)
    {
        loop_ctr++;
    }
    else
    {
        loop_ctr = 0u;
    }

    out.runtime_ctr = loop_ctr;
    out.disable_iwdg = (loop_ctr >= boot_loop_max);
    out.stored_ctr = out.disable_iwdg ? 0u : loop_ctr;
    return out;
}

#endif /* M1_WATCHDOG_BOOT_LOOP_H_ */
