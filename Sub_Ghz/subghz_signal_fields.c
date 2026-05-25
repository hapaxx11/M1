/* See COPYING.txt for license details. */

/**
 * @file   subghz_signal_fields.c
 * @brief  KeeLoq-family field extract / assemble — Phase 9a-1.
 *
 * See subghz_signal_fields.h for the bit layouts and contract.
 *
 * M1 Project — Hapax fork
 */

#include "subghz_signal_fields.h"
#include "subghz_keeloq.h"

#include <ctype.h>
#include <stddef.h>

/*============================================================================*/
/* Bit-layout constants                                                        */
/*============================================================================*/

#define SUBGHZ_SF_SERIAL_MASK_28  0x0FFFFFFFU
#define SUBGHZ_SF_BUTTON_MASK_4   0x0FU

/*============================================================================*/
/* Protocol-name matching                                                      */
/*============================================================================*/

/* Case-insensitive prefix match terminated by NUL or space.  Matches the
 * convention used by Sub_Ghz/subghz_keeloq_create.c::is_supported_keeloq()
 * and Sub_Ghz/subghz_keeloq_encoder.c::keeloq_is_keeloq_protocol(). */
static bool name_matches(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b))
    {
        ++a; ++b;
    }
    return (*b == '\0') && (*a == '\0' || *a == ' ');
}

static bool is_star_line(const char *protocol)
{
    return name_matches(protocol, "Star Line");
}

bool subghz_signal_fields_is_keeloq_family(const char *protocol)
{
    if (!protocol) return false;
    static const char *const names[] = { "KeeLoq", "Star Line", "Jarolift" };
    for (size_t i = 0; i < sizeof(names)/sizeof(names[0]); ++i)
    {
        if (name_matches(protocol, names[i]))
            return true;
    }
    return false;
}

/*============================================================================*/
/* Extract                                                                     */
/*============================================================================*/

bool subghz_signal_fields_keeloq_extract(const char            *protocol,
                                          uint64_t               key,
                                          subghz_keeloq_fields_t *out)
{
    if (!out)
        return false;

    /* Pre-clear so callers see a clean state on failure. */
    out->serial  = 0U;
    out->button  = 0U;
    out->enc_hop = 0U;

    if (!subghz_signal_fields_is_keeloq_family(protocol))
        return false;

    if (is_star_line(protocol))
    {
        /* [63:32] encrypted HOP, [31:4] serial[27:0], [3:0] button[3:0] */
        out->enc_hop = (uint32_t)(key >> 32);
        out->serial  = (uint32_t)((key >> 4) & SUBGHZ_SF_SERIAL_MASK_28);
        out->button  = (uint8_t)(key & SUBGHZ_SF_BUTTON_MASK_4);
    }
    else
    {
        /* KeeLoq / Jarolift:
         *   [63:60] button, [59:32] serial[27:0], [31:0] encrypted HOP */
        out->enc_hop = (uint32_t)(key & 0xFFFFFFFFULL);
        out->serial  = (uint32_t)((key >> 32) & SUBGHZ_SF_SERIAL_MASK_28);
        out->button  = (uint8_t)((key >> 60) & SUBGHZ_SF_BUTTON_MASK_4);
    }
    return true;
}

/*============================================================================*/
/* Assemble                                                                    */
/*============================================================================*/

bool subghz_signal_fields_keeloq_assemble(const char                   *protocol,
                                           const subghz_keeloq_fields_t *fields,
                                           uint64_t                     *key_out)
{
    if (!key_out)
        return false;
    *key_out = 0ULL;

    if (!fields)
        return false;
    if (!subghz_signal_fields_is_keeloq_family(protocol))
        return false;

    const uint32_t s = fields->serial & SUBGHZ_SF_SERIAL_MASK_28;
    const uint8_t  b = fields->button & SUBGHZ_SF_BUTTON_MASK_4;
    const uint32_t h = fields->enc_hop;

    if (is_star_line(protocol))
    {
        *key_out = ((uint64_t)h << 32) |
                   ((uint64_t)s << 4)  |
                   (uint64_t)b;
    }
    else
    {
        *key_out = ((uint64_t)b << 60) |
                   ((uint64_t)s << 32) |
                   (uint64_t)h;
    }
    return true;
}

/*============================================================================*/
/* Counter decode / encode (Phase 9c-1)                                        */
/*============================================================================*/

uint16_t subghz_signal_fields_keeloq_counter_decode(uint32_t enc_hop,
                                                    uint64_t device_key)
{
    const uint32_t plain = keeloq_decrypt(enc_hop, device_key);
    return (uint16_t)(plain >> 16);
}

uint32_t subghz_signal_fields_keeloq_counter_encode(uint32_t enc_hop,
                                                    uint16_t new_counter,
                                                    uint64_t device_key)
{
    /* Decrypt to recover the lower 16 plaintext bits
     * (button / VLOW / discriminant / overflow counter), substitute the
     * 16-bit rolling counter, and re-encrypt with the same device key.
     * This mirrors keeloq_increment_hop()'s structure exactly. */
    const uint32_t plain     = keeloq_decrypt(enc_hop, device_key);
    const uint32_t new_plain = ((uint32_t)new_counter << 16) |
                               (plain & 0x0000FFFFU);
    return keeloq_encrypt(new_plain, device_key);
}

/*============================================================================*/
/* Counter-edit capability probe (Phase 9e-1)                                  */
/*============================================================================*/

/* Static deferred-reason strings — pointed to by callers; never freed. */
static const char SF_REASON_EMPTY[]      = "";
static const char SF_REASON_FLOR_S[]     = "Nice FloR-S: HCS perm. table req.";
static const char SF_REASON_ATOMO[]      = "CAME Atomo: cipher decode req.";
static const char SF_REASON_ALUTECH[]    = "Alutech: AES key recovery req.";
static const char SF_REASON_PHOENIX_V2[] = "Phoenix V2: checksum recompute req.";

/* Phase 9e-2..5 protocols — counter editing is on the roadmap but the
 * required decode/encode path is not yet implemented.  Each entry cites
 * the specific blocker documented in the Phase 9e checklist:
 *
 *   - Nice FloR-S    : 1080-byte Microchip-HCS-style permutation lookup
 *                      table + inverse perm to substitute the counter.
 *   - CAME Atomo     : XTEA-family fixed-key cipher decode chain to
 *                      expose the counter bits inside the 62-bit code.
 *   - Alutech AT-4N  : Per-device AES-128-like cipher key extracted
 *                      from the encrypted payload before counter
 *                      substitution.
 *   - Phoenix V2     : Counter bit-field is at a known offset in the
 *                      52-bit code but the trailing discriminant /
 *                      checksum must be recomputed after editing.
 */
typedef struct {
    const char *name;
    const char *reason;
} sf_deferred_entry_t;

static const sf_deferred_entry_t SF_DEFERRED[] = {
    { "Nice FloR-S",   SF_REASON_FLOR_S    },
    { "CAME Atomo",    SF_REASON_ATOMO     },
    { "Alutech AT-4N", SF_REASON_ALUTECH   },
    { "Phoenix_V2",    SF_REASON_PHOENIX_V2 },
};

subghz_counter_edit_status_t
subghz_signal_fields_counter_edit_status(const char  *protocol,
                                         const char **out_reason)
{
    if (subghz_signal_fields_is_keeloq_family(protocol))
    {
        if (out_reason)
            *out_reason = SF_REASON_EMPTY;
        return SUBGHZ_COUNTER_EDIT_SUPPORTED;
    }

    if (protocol)
    {
        for (size_t i = 0; i < sizeof(SF_DEFERRED) / sizeof(SF_DEFERRED[0]); ++i)
        {
            if (name_matches(protocol, SF_DEFERRED[i].name))
            {
                if (out_reason)
                    *out_reason = SF_DEFERRED[i].reason;
                return SUBGHZ_COUNTER_EDIT_DEFERRED;
            }
        }
    }

    if (out_reason)
        *out_reason = SF_REASON_EMPTY;
    return SUBGHZ_COUNTER_EDIT_UNSUPPORTED;
}
