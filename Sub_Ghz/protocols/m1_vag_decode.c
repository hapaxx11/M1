/* See COPYING.txt for license details. */

/*
 * m1_vag_decode.c
 *
 * M1 Sub-GHz decoder for the ProtoPirate "VAG" automotive keyfob protocol
 * (Volkswagen / Audi / Škoda / SEAT).
 *
 * Reference implementation: RocketGod-git/ProtoPirate protocols/vag.c
 * (OOK/AM, Manchester).  VAG ships two on-air formats; this decoder handles the
 * "T12" format (reference types 1 & 2: te_short=300µs, te_long=600µs), which is
 * identified by a 15-bit plaintext prefix:
 *   0x2F3F → type 1 (AUT64),   0x2F1C → type 2 (TEA/XTEA)
 *
 * Type 1 AUT64 keys are supplied by the optional VAG keystore build secret.
 * Type 2 uses the fixed TEA schedule from ProtoPirate. If type 1 keys are
 * unavailable, the decoder still identifies the prefix and dispatch button.
 *
 * Unlike the name-only placeholder automotive entries in this directory, this
 * decoder actually parses the on-air waveform so M1 can identify VAG keyfobs and
 * their button from a capture.
 *
 * Wire frame (T12): 15-bit prefix + 80-bit payload (64-bit Key1 + 16-bit Key2),
 * all Manchester.  The payload is stored as the one's-complement of the received
 * bits (prefix is received raw); the dual-polarity window decode below recovers
 * the correct orientation.
 *
 * Frame parsing and decryption are pure and host-testable.
 *
 * M1 Project -- Hapax fork
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"
#include "m1_aut64.h"
#include "subghz_protopirate_keys_builtin.h"

/* Timing (µs) — T12 format (ProtoPirate reference types 1/2). */
#define VAG_TE_SHORT  300u
#define VAG_TE_LONG   600u
#define VAG_TE_DELTA  120u

/* Frame geometry. */
#define VAG_PREFIX_BITS 15u
#define VAG_KEY1_BITS   64u
#define VAG_KEY2_BITS   16u
#define VAG_WIRE_BITS   (VAG_PREFIX_BITS + VAG_KEY1_BITS + VAG_KEY2_BITS) /* 95 */
#define VAG_WIRE_CELLS  (VAG_WIRE_BITS * 2u)   /* 190 Manchester cells */

/* Prefix / dispatch constants (ProtoPirate vag.c). */
#define VAG_PREFIX_T1   0x2F3Fu
#define VAG_PREFIX_T2   0x2F1Cu
#define VAG_DISPATCH_LOCK   0x2Au
#define VAG_DISPATCH_UNLOCK 0x1Cu
#define VAG_DISPATCH_BOOT   0x46u
/* Button codes (ProtoPirate vag_button_name): 1=Unlock, 2=Lock, 4=Boot. */
#define VAG_BTN_UNLOCK  0x1u
#define VAG_BTN_LOCK    0x2u
#define VAG_BTN_BOOT    0x4u

/*============================================================================*/
/* Pure frame parsing (host-testable)                                          */
/*============================================================================*/

static bool vag_dispatch_to_button(uint8_t dispatch, uint8_t *button)
{
    switch (dispatch)
    {
    case VAG_DISPATCH_LOCK:   if (button) *button = VAG_BTN_LOCK;   return true;
    case VAG_DISPATCH_UNLOCK: if (button) *button = VAG_BTN_UNLOCK; return true;
    case VAG_DISPATCH_BOOT:   if (button) *button = VAG_BTN_BOOT;   return true;
    default:                  return false;
    }
}

/**
 * Validate and parse a VAG T12 frame from its three raw (as-received, before
 * one's-complement) wire fields.
 *
 * @p prefix        the 15-bit plaintext prefix (identifies type 1 or 2)
 * @p key1_raw      the 64-bit Key1 as received (stored inverted on air)
 * @p key2_raw      the 16-bit Key2 as received (stored inverted on air)
 *
 * Returns true if the prefix is a known VAG type and the plaintext dispatch byte
 * is a valid button code, filling any non-NULL output pointers.  @p type reports
 * 1 (AUT64) or 2 (TEA).  Note: serial/counter are NOT recovered (encrypted).
 *
 * Pure function — no global side effects; unit-tested on the host.
 */
bool m1_vag_t12_parse(uint16_t prefix, uint64_t key1_raw, uint16_t key2_raw,
                      uint8_t *type, uint8_t *button, uint64_t *key1_out)
{
    uint8_t t;
    if (prefix == VAG_PREFIX_T1)
        t = 1u;
    else if (prefix == VAG_PREFIX_T2)
        t = 2u;
    else
        return false;

    const uint64_t key1 = ~key1_raw;             /* payload stored inverted */
    const uint16_t key2 = (uint16_t)(~key2_raw); /* 16-bit */
    const uint8_t  dispatch = (uint8_t)(key2 & 0xFFu);

    if (!vag_dispatch_to_button(dispatch, button))
        return false;

    if (type)     *type     = t;
    if (key1_out) *key1_out = key1;
    return true;
}

static uint32_t vag_read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

static bool vag_decrypted_button_matches(const uint8_t block[8], uint8_t dispatch)
{
    const uint8_t expected = (uint8_t)(dispatch >> 4);
    const uint8_t decoded = (uint8_t)(block[7] >> 4);
    return decoded == expected || (block[7] == 0u && expected == VAG_BTN_LOCK);
}

bool m1_vag_t12_decrypt(uint8_t type, uint64_t key1, uint16_t key2,
                        const uint8_t packed_keys[M1_VAG_AUT64_KEY_BYTES],
                        bool keys_available, uint32_t *serial,
                        uint32_t *counter, uint8_t *button)
{
    uint8_t block[8] = {
        (uint8_t)(key1 >> 48), (uint8_t)(key1 >> 40),
        (uint8_t)(key1 >> 32), (uint8_t)(key1 >> 24),
        (uint8_t)(key1 >> 16), (uint8_t)(key1 >> 8),
        (uint8_t)key1, (uint8_t)(key2 >> 8),
    };
    const uint8_t dispatch = (uint8_t)key2;
    uint8_t decoded_button = 0;
    if (!vag_dispatch_to_button(dispatch, &decoded_button))
        return false;

    bool found = false;
    if (type == 1u && keys_available && packed_keys != NULL) {
        for (uint8_t i = 0; i < 3u; i++) {
            m1_aut64_key_t key;
            uint8_t candidate[sizeof(block)];
            if (!m1_aut64_unpack(&key, &packed_keys[i * 16u]))
                continue;
            memcpy(candidate, block, sizeof(candidate));
            m1_aut64_decrypt(&key, candidate);
            if (vag_decrypted_button_matches(candidate, dispatch)) {
                memcpy(block, candidate, sizeof(block));
                found = true;
                break;
            }
        }
    } else if (type == 2u) {
        uint32_t left = vag_read_be32(block);
        uint32_t right = vag_read_be32(&block[4]);
        uint32_t sum = 0x9E3779B9u * 32u;
        static const uint32_t tea_key[4] = {
            0x0B46502Du, 0x5E253718u, 0x2BF93A19u, 0x622C1206u,
        };
        for (uint8_t round = 0; round < 32u; round++) {
            right -= (((left << 4) ^ (left >> 5)) + left) ^
                     (sum + tea_key[(sum >> 11) & 3u]);
            sum -= 0x9E3779B9u;
            left -= (((right << 4) ^ (right >> 5)) + right) ^
                    (sum + tea_key[sum & 3u]);
        }
        block[0] = (uint8_t)(left >> 24);
        block[1] = (uint8_t)(left >> 16);
        block[2] = (uint8_t)(left >> 8);
        block[3] = (uint8_t)left;
        block[4] = (uint8_t)(right >> 24);
        block[5] = (uint8_t)(right >> 16);
        block[6] = (uint8_t)(right >> 8);
        block[7] = (uint8_t)right;
        found = vag_decrypted_button_matches(block, dispatch);
    }

    if (!found)
        return false;
    if (serial != NULL)
        *serial = vag_read_be32(block);
    if (counter != NULL)
        *counter = (uint32_t)block[4] | ((uint32_t)block[5] << 8) |
                   ((uint32_t)block[6] << 16);
    if (button != NULL)
        *button = decoded_button;
    return true;
}

/*============================================================================*/
/* Manchester cell helpers                                                     */
/*============================================================================*/

static inline bool vag_is_short(uint16_t d)
{
    return get_diff(d, VAG_TE_SHORT) < VAG_TE_DELTA;
}

static inline bool vag_is_long(uint16_t d)
{
    return get_diff(d, VAG_TE_LONG) < VAG_TE_DELTA;
}

static bool vag_try_window(const uint8_t *cells, uint16_t cell_count,
                           uint16_t protocol_index, bool invert)
{
    if (cell_count < VAG_WIRE_CELLS)
        return false;

    const uint8_t *w = &cells[cell_count - VAG_WIRE_CELLS];

    uint16_t prefix = 0;
    uint64_t key1_raw = 0;
    uint16_t key2_raw = 0;

    for (uint8_t b = 0; b < VAG_WIRE_BITS; b++)
    {
        const uint8_t first  = w[b * 2u];
        const uint8_t second = w[b * 2u + 1u];
        if (first == second)
            return false;

        bool bit = (first != 0u);
        if (invert)
            bit = !bit;

        if (b < VAG_PREFIX_BITS)
            prefix = (uint16_t)((prefix << 1) | (bit ? 1u : 0u));
        else if (b < VAG_PREFIX_BITS + VAG_KEY1_BITS)
            key1_raw = (key1_raw << 1) | (bit ? 1u : 0u);
        else
            key2_raw = (uint16_t)((key2_raw << 1) | (bit ? 1u : 0u));
    }

    uint8_t  type = 0, button = 0;
    uint64_t key1 = 0;
    if (!m1_vag_t12_parse(prefix, key1_raw, key2_raw, &type, &button, &key1))
        return false;

    uint32_t serial = 0, counter = 0;
    if (m1_vag_t12_decrypt(type, key1, (uint16_t)~key2_raw,
                           m1_vag_aut64_keys_builtin,
                           m1_vag_aut64_keys_builtin_available,
                           &serial, &counter, &button)) {
        subghz_decenc_ctl.n32_serialnumber = serial;
        subghz_decenc_ctl.n32_rollingcode = counter;
    } else {
        /* Keep the previous stable identifier when type-1 keys are absent. */
        subghz_decenc_ctl.n32_serialnumber = (uint32_t)(key1 >> 32);
        subghz_decenc_ctl.n32_rollingcode = 0;
    }
    subghz_decenc_ctl.n64_decodedvalue  = key1;
    subghz_decenc_ctl.n8_buttonid       = button;
    subghz_decenc_ctl.ndecodedbitlength = (uint16_t)VAG_WIRE_BITS;
    subghz_decenc_ctl.ndecodeddelay     = 0;
    subghz_decenc_ctl.ndecodedprotocol  = protocol_index;
    return true;
}

/*============================================================================*/
/* M1 registry decode entry                                                     */
/*============================================================================*/

uint8_t subghz_decode_vag(uint16_t p, uint16_t pulsecount)
{
    uint8_t  cells[VAG_WIRE_CELLS];
    uint16_t cell_count = 0;

    for (uint16_t i = 0; i < pulsecount; i++)
    {
        const uint16_t d = subghz_decenc_ctl.pulse_times[i];
        const uint8_t  level = (uint8_t)((i & 1u) == 0u);

        uint8_t push = 0;
        if (vag_is_short(d))
            push = 1;
        else if (vag_is_long(d))
            push = 2;
        else
        {
            cell_count = 0;
            continue;
        }

        for (uint8_t c = 0; c < push; c++)
        {
            if (cell_count < VAG_WIRE_CELLS)
            {
                cells[cell_count++] = level;
            }
            else
            {
                memmove(cells, &cells[1], VAG_WIRE_CELLS - 1u);
                cells[VAG_WIRE_CELLS - 1u] = level;
            }

            if (cell_count == VAG_WIRE_CELLS)
            {
                if (vag_try_window(cells, cell_count, p, false) ||
                    vag_try_window(cells, cell_count, p, true))
                {
                    return 0;
                }
            }
        }
    }

    return 1; /* no valid VAG frame found */
}
