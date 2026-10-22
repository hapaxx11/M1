/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder for the ProtoPirate "Fiat V1" automotive keyfob
 * protocol.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate protocols/fiat_v1.c
 * (GPLv3): Manchester-cell OOK, 104 wire bits (208 half-bit cells, 13 bytes),
 * 102 logical bits.  Two timing variants are tracked in parallel, exactly as
 * upstream: variant A = te 250/500 us (delta 100) with a 2033 us lead-in and
 * 3252 us frame gap, variant B = te 100/200 us (delta 50) with an 850 us
 * lead-in and 1230 us frame gap.  The TX encoder lives in
 * Sub_Ghz/subghz_proto_pirate.c (it emits variant A); this file implements
 * RX identification only.
 *
 * Wire layout (MSB first):
 *   raw[0]      = 0x00 marker, raw[1] = 0x01 family marker
 *   raw[2..5]   = UID (serial, big-endian)
 *   raw[6]      = button nibble (1/2/4/8) | control[9:6]
 *   raw[7]      = control[5:0] << 2 | authenticator[31:30]
 *   raw[8..11]  = authenticator[29:0] << 2 | tail bits (raw[11] & 0x03)
 *   raw[12]     = XOR8 checksum (seed 0x01, over raw[0..11])
 *
 * Commit policy (mirrors upstream): a frame commits STRUCTURALLY whenever the
 * 13-byte layout validates (markers, XOR8, button in {1,2,4,8}, UID neither 0
 * nor UINT32_MAX) — no cipher key is needed to identify a capture.  In
 * addition the 32-bit authenticator is checked against the nine
 * publicly-known 6-byte BCM keys shipped in the upstream source (epoch 0,
 * decoded button first, then any valid button — as upstream's
 * fiat_v1_resolve_hitag2_key does on the RX path); a match sets the
 * known-key flag and surfaces the key.  The BCM authenticator construction
 * (fiat_v1_bcm_generate_authenticator) is ported verbatim and is
 * self-contained in this file.
 *
 * n32_key2 layout (uint64_t) — covers the upstream file fields that do not
 * fit the standard M1 commit slots (Raw, Tail Bits, XOR, Hitag2 Key; Hitag2
 * Epoch is always 0 on the RX path and Type is the TE variant):
 *   bits 47:0   matched Hitag2 BCM key (key[0] in bits 47:40); 0 when invalid
 *   bit  48     TE variant (0 = A 250/500 us, 1 = B 100/200 us)
 *   bits 50:49  tail bits (raw[11] & 0x03)
 *   bit  51     known-key match flag
 *   bits 59:52  frame XOR byte (raw[12])
 *   bits 63:60  zero
 * Together with n64_decodedvalue (serial << 32 | authenticator),
 * n32_serialnumber (UID), n32_rollingcode (10-bit control counter) and
 * n8_buttonid this carries every field upstream serializes.
 *
 * Adaptation notes vs the upstream event-driven decoder:
 *  - Stateless per call: the per-variant sliding 208-cell windows live on the
 *    stack and the whole capture buffer is re-run on each call.  Upstream's
 *    last_raw cross-call dedup is dropped — M1 commits once per buffer and
 *    returns.
 *  - A variant-A frame gap (3252 us) is included in the buffer by the M1
 *    pulse handler (it re-opens the capture below 5 ms), so the upstream
 *    boundary push (a trailing low >= 800 us supplies the final low cell when
 *    the frame ends in a 1 bit) works unchanged.
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"

#define FIAT_V1_TE_A_SHORT 250u
#define FIAT_V1_TE_A_LONG  500u
#define FIAT_V1_TE_A_DELTA 100u
#define FIAT_V1_TE_B_SHORT 100u
#define FIAT_V1_TE_B_LONG  200u
#define FIAT_V1_TE_B_DELTA 50u

#define FIAT_V1_WIRE_BITS    104u
#define FIAT_V1_WIRE_BYTES   13u
#define FIAT_V1_WIRE_CELLS   (FIAT_V1_WIRE_BITS * 2u)
#define FIAT_V1_LOGICAL_BITS 102u
#define FIAT_V1_BOUNDARY_MIN_US 800u

#define FIAT_V1_VARIANT_A     0u
#define FIAT_V1_VARIANT_B     1u
#define FIAT_V1_VARIANT_COUNT 2u

#define FIAT_V1_KNOWN_KEY_COUNT 9u

/* n32_key2 bit layout (see header comment). */
#define FIAT_V1_KEY2_VARIANT_SHIFT 48u
#define FIAT_V1_KEY2_TAIL_SHIFT    49u
#define FIAT_V1_KEY2_VALID_SHIFT   51u
#define FIAT_V1_KEY2_XOR_SHIFT     52u

typedef enum {
    FiatV1StepReset = 0,
    FiatV1StepData,
} fiat_v1_step_t;

typedef struct {
    fiat_v1_step_t step;
    uint8_t cells[FIAT_V1_VARIANT_COUNT][FIAT_V1_WIRE_CELLS];
    uint16_t cell_count[FIAT_V1_VARIANT_COUNT];
} fiat_v1_state_t;

/*--------------------------------------------------------------------*/
/* Timing classification                                               */
/*--------------------------------------------------------------------*/

static bool fiat_v1_is_short(uint8_t variant, uint16_t duration)
{
    const uint16_t te = (variant == FIAT_V1_VARIANT_A) ? FIAT_V1_TE_A_SHORT
                                                       : FIAT_V1_TE_B_SHORT;
    const uint16_t delta = (variant == FIAT_V1_VARIANT_A) ? FIAT_V1_TE_A_DELTA
                                                          : FIAT_V1_TE_B_DELTA;
    return get_diff(duration, te) < delta;
}

static bool fiat_v1_is_long(uint8_t variant, uint16_t duration)
{
    const uint16_t te = (variant == FIAT_V1_VARIANT_A) ? FIAT_V1_TE_A_LONG
                                                       : FIAT_V1_TE_B_LONG;
    const uint16_t delta = (variant == FIAT_V1_VARIANT_A) ? FIAT_V1_TE_A_DELTA
                                                          : FIAT_V1_TE_B_DELTA;
    return get_diff(duration, te) < delta;
}

static bool fiat_v1_is_pulse(uint16_t duration)
{
    for (uint8_t variant = 0; variant < FIAT_V1_VARIANT_COUNT; variant++)
    {
        if (fiat_v1_is_short(variant, duration) ||
            fiat_v1_is_long(variant, duration))
            return true;
    }
    return false;
}

/*--------------------------------------------------------------------*/
/* Pure frame helpers (exported for host tests)                        */
/*--------------------------------------------------------------------*/

static uint8_t fiat_v1_compute_xor(const uint8_t raw[FIAT_V1_WIRE_BYTES])
{
    uint8_t value = 0x01u;
    for (uint8_t i = 0; i < FIAT_V1_WIRE_BYTES - 1u; i++)
        value ^= raw[i];
    return value;
}

static bool fiat_v1_button_valid(uint8_t button)
{
    return button == 0x1u || button == 0x2u || button == 0x4u || button == 0x8u;
}

static uint32_t fiat_v1_uid(const uint8_t raw[FIAT_V1_WIRE_BYTES])
{
    return ((uint32_t)raw[2] << 24) | ((uint32_t)raw[3] << 16) |
           ((uint32_t)raw[4] << 8) | raw[5];
}

static uint32_t fiat_v1_counter(const uint8_t raw[FIAT_V1_WIRE_BYTES])
{
    return ((uint32_t)(raw[6] & 0x0Fu) << 6) | (raw[7] >> 2);
}

static uint32_t fiat_v1_hop(const uint8_t raw[FIAT_V1_WIRE_BYTES])
{
    return ((uint32_t)(raw[7] & 0x03u) << 30) | ((uint32_t)raw[8] << 22) |
           ((uint32_t)raw[9] << 14) | ((uint32_t)raw[10] << 6) |
           (raw[11] >> 2);
}

bool m1_fiat_v1_frame_valid(const uint8_t raw[FIAT_V1_WIRE_BYTES])
{
    if (raw == NULL)
        return false;
    if (raw[0] != 0x00u || raw[1] != 0x01u)
        return false;
    if (fiat_v1_compute_xor(raw) != raw[12])
        return false;
    if (!fiat_v1_button_valid((uint8_t)(raw[6] >> 4)))
        return false;
    const uint32_t uid = fiat_v1_uid(raw);
    return uid != 0u && uid != UINT32_MAX;
}

void m1_fiat_v1_parse_frame(const uint8_t raw[FIAT_V1_WIRE_BYTES],
                            uint32_t *uid, uint8_t *button, uint32_t *control,
                            uint32_t *hop, uint8_t *tail_bits,
                            uint8_t *frame_xor)
{
    if (uid)       *uid       = fiat_v1_uid(raw);
    if (button)    *button    = (uint8_t)(raw[6] >> 4);
    if (control)   *control   = fiat_v1_counter(raw);
    if (hop)       *hop       = fiat_v1_hop(raw);
    if (tail_bits) *tail_bits = (uint8_t)(raw[11] & 0x03u);
    if (frame_xor) *frame_xor = raw[12];
}

/*--------------------------------------------------------------------*/
/* BCM authenticator (Hitag2-filter construction, verbatim port)       */
/*--------------------------------------------------------------------*/

static uint8_t fiat_v1_truth(uint32_t table, uint8_t index)
{
    return (uint8_t)((table >> index) & 1u);
}

static uint8_t fiat_v1_filter_index(uint8_t a, uint8_t b, uint8_t c, uint8_t d)
{
    return (uint8_t)((a << 3u) | (b << 2u) | (c << 1u) | d);
}

static uint8_t fiat_v1_byte_bit(uint8_t byte, uint8_t bit)
{
    return (uint8_t)((byte >> bit) & 1u);
}

static uint8_t fiat_v1_bcm_hitag2_filter(const uint8_t state[6])
{
    uint8_t group = 0u;
    group |= fiat_v1_truth(
        0x2c79u,
        fiat_v1_filter_index(
            fiat_v1_byte_bit(state[0], 1u), fiat_v1_byte_bit(state[0], 2u),
            fiat_v1_byte_bit(state[0], 4u), fiat_v1_byte_bit(state[0], 5u)));
    group |= (uint8_t)(fiat_v1_truth(
                           0x6671u,
                           fiat_v1_filter_index(
                               fiat_v1_byte_bit(state[1], 0u),
                               fiat_v1_byte_bit(state[1], 1u),
                               fiat_v1_byte_bit(state[1], 3u),
                               fiat_v1_byte_bit(state[1], 7u)))
                       << 1u);
    group |= (uint8_t)(fiat_v1_truth(
                           0x6671u,
                           fiat_v1_filter_index(
                               fiat_v1_byte_bit(state[3], 5u),
                               fiat_v1_byte_bit(state[2], 0u),
                               fiat_v1_byte_bit(state[2], 2u),
                               fiat_v1_byte_bit(state[2], 6u)))
                       << 2u);
    group |= (uint8_t)(fiat_v1_truth(
                           0x6671u,
                           fiat_v1_filter_index(
                               fiat_v1_byte_bit(state[4], 6u),
                               fiat_v1_byte_bit(state[3], 0u),
                               fiat_v1_byte_bit(state[3], 2u),
                               fiat_v1_byte_bit(state[3], 3u)))
                       << 3u);
    group |= (uint8_t)(fiat_v1_truth(
                           0x2c79u,
                           fiat_v1_filter_index(
                               fiat_v1_byte_bit(state[5], 1u),
                               fiat_v1_byte_bit(state[5], 3u),
                               fiat_v1_byte_bit(state[5], 4u),
                               fiat_v1_byte_bit(state[4], 5u)))
                       << 4u);
    return fiat_v1_truth(0x7907287bUL, group);
}

static uint8_t fiat_v1_parity8(uint8_t value)
{
    value ^= (uint8_t)(value >> 4u);
    value ^= (uint8_t)(value >> 2u);
    value ^= (uint8_t)(value >> 1u);
    return value & 1u;
}

static uint8_t fiat_v1_bcm_hitag2_feedback(const uint8_t state[6])
{
    static const uint8_t masks[6] = {0xb3u, 0x80u, 0x83u, 0x22u, 0x00u, 0x73u};
    uint8_t feedback = 0u;
    for (uint8_t i = 0; i < 6u; i++)
        feedback ^= fiat_v1_parity8((uint8_t)(state[i] & masks[i]));
    return feedback & 1u;
}

static void fiat_v1_bcm_hitag2_shift(uint8_t state[6], uint8_t input)
{
    for (uint8_t i = 0; i < 5u; i++)
        state[i] = (uint8_t)((state[i] << 1u) | (state[i + 1u] >> 7u));
    state[5] = (uint8_t)((state[5] << 1u) | (input & 1u));
}

static uint8_t fiat_v1_input_bit_u32_be(uint32_t value, uint8_t index)
{
    return (uint8_t)((value >> (31u - index)) & 1u);
}

static uint8_t fiat_v1_input_bit_bytes_be(const uint8_t *bytes, uint8_t index)
{
    return (uint8_t)((bytes[index >> 3u] >> (7u - (index & 7u))) & 1u);
}

uint32_t m1_fiat_v1_bcm_generate_authenticator(uint32_t uid, uint8_t button,
                                               uint16_t control,
                                               const uint8_t key[6],
                                               uint32_t epoch)
{
    uint8_t state[6] = {
        (uint8_t)(uid >> 24u),
        (uint8_t)(uid >> 16u),
        (uint8_t)(uid >> 8u),
        (uint8_t)uid,
        key[4],
        key[5],
    };

    const uint32_t iv = ((epoch & 0x3FFFFu) << 14u) |
                        (((uint32_t)control & 0x03FFu) << 4u) |
                        ((uint32_t)button & 0x0Fu);

    for (uint8_t i = 0; i < 32u; i++)
    {
        const uint8_t input = (uint8_t)(fiat_v1_input_bit_u32_be(iv, i) ^
                                        fiat_v1_input_bit_bytes_be(key, i) ^
                                        fiat_v1_bcm_hitag2_filter(state));
        fiat_v1_bcm_hitag2_shift(state, input);
    }

    uint32_t authenticator = 0u;
    for (uint8_t i = 0; i < 32u; i++)
    {
        authenticator = (authenticator << 1u) | fiat_v1_bcm_hitag2_filter(state);
        fiat_v1_bcm_hitag2_shift(state, fiat_v1_bcm_hitag2_feedback(state));
    }
    return authenticator;
}

/*--------------------------------------------------------------------*/
/* Known-key validation                                                */
/*--------------------------------------------------------------------*/

/* Nine publicly-known 6-byte BCM keys, as shipped in the upstream source. */
static const uint8_t fiat_v1_known_keys[FIAT_V1_KNOWN_KEY_COUNT][6] = {
    {0xB7u, 0x92u, 0x80u, 0xAEu, 0xCCu, 0x37u},
    {0xD4u, 0x24u, 0x28u, 0xF7u, 0xD9u, 0x66u},
    {0x4Du, 0x34u, 0x3Fu, 0xD4u, 0xE7u, 0xB6u},
    {0x6Du, 0x6Bu, 0xF2u, 0x1Du, 0x3Au, 0x1Au},
    {0xA3u, 0xF3u, 0xACu, 0xF7u, 0xB9u, 0x10u},
    {0x4Du, 0x49u, 0x4Bu, 0x52u, 0x4Fu, 0x4Eu},
    {0xCDu, 0x49u, 0x4Bu, 0x52u, 0x4Fu, 0x4Eu},
    {0x33u, 0xFAu, 0x2Fu, 0xCDu, 0xC3u, 0x3Bu},
    {0xF6u, 0x1Au, 0xEFu, 0x9Cu, 0xD0u, 0x1Bu},
};

/* Try the nine known keys (epoch 0): the decoded button first, then any
 * valid button — mirroring upstream's fiat_v1_resolve_hitag2_key(NULL, ...)
 * RX path. */
bool m1_fiat_v1_find_known_key(uint32_t uid, uint8_t button, uint16_t control,
                               uint32_t hop, uint8_t key_out[6])
{
    static const uint8_t buttons[] = {0x1u, 0x2u, 0x4u, 0x8u};

    control = (uint16_t)(control & 0x03FFu);
    for (uint8_t i = 0; i < FIAT_V1_KNOWN_KEY_COUNT; i++)
    {
        if (m1_fiat_v1_bcm_generate_authenticator(uid, button, control,
                                                  fiat_v1_known_keys[i],
                                                  0u) == hop)
        {
            if (key_out)
                memcpy(key_out, fiat_v1_known_keys[i], 6u);
            return true;
        }
        for (uint8_t j = 0; j < (uint8_t)(sizeof(buttons) / sizeof(buttons[0]));
             j++)
        {
            if (buttons[j] == button)
                continue;
            if (m1_fiat_v1_bcm_generate_authenticator(uid, buttons[j], control,
                                                      fiat_v1_known_keys[i],
                                                      0u) == hop)
            {
                if (key_out)
                    memcpy(key_out, fiat_v1_known_keys[i], 6u);
                return true;
            }
        }
    }
    return false;
}

/* Pack the auxiliary fields into the n32_key2 word (layout in the header). */
uint64_t m1_fiat_v1_pack_key2(bool key_valid, const uint8_t key[6],
                              uint8_t te_variant, uint8_t tail_bits,
                              uint8_t frame_xor)
{
    uint64_t value = 0;
    if (key_valid && key != NULL)
    {
        for (uint8_t i = 0; i < 6u; i++)
            value = (value << 8) | key[i];
    }
    if (te_variant == FIAT_V1_VARIANT_B)
        value |= (uint64_t)1u << FIAT_V1_KEY2_VARIANT_SHIFT;
    value |= (uint64_t)(tail_bits & 0x03u) << FIAT_V1_KEY2_TAIL_SHIFT;
    if (key_valid)
        value |= (uint64_t)1u << FIAT_V1_KEY2_VALID_SHIFT;
    value |= (uint64_t)frame_xor << FIAT_V1_KEY2_XOR_SHIFT;
    return value;
}

/*--------------------------------------------------------------------*/
/* Commit                                                              */
/*--------------------------------------------------------------------*/

static bool fiat_v1_commit(const uint8_t raw[FIAT_V1_WIRE_BYTES],
                           uint8_t te_variant, uint16_t protocol_index)
{
    if (!m1_fiat_v1_frame_valid(raw))
        return false;

    uint32_t uid = 0, control = 0, hop = 0;
    uint8_t button = 0, tail_bits = 0, frame_xor = 0;
    m1_fiat_v1_parse_frame(raw, &uid, &button, &control, &hop, &tail_bits,
                           &frame_xor);

    uint8_t key[6] = {0};
    const bool key_valid =
        m1_fiat_v1_find_known_key(uid, button, (uint16_t)control, hop, key);

    subghz_decenc_ctl.n64_decodedvalue = ((uint64_t)uid << 32) | hop;
    subghz_decenc_ctl.n32_serialnumber = uid;
    subghz_decenc_ctl.n32_rollingcode = control;
    subghz_decenc_ctl.n32_key2 =
        m1_fiat_v1_pack_key2(key_valid, key, te_variant, tail_bits, frame_xor);
    subghz_decenc_ctl.n8_buttonid = button;
    subghz_decenc_ctl.ndecodedbitlength = FIAT_V1_LOGICAL_BITS;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

/*--------------------------------------------------------------------*/
/* Cell window decode                                                  */
/*--------------------------------------------------------------------*/

static bool fiat_v1_try_window(fiat_v1_state_t *state, uint8_t variant,
                               bool invert, uint16_t protocol_index)
{
    if (state->cell_count[variant] != FIAT_V1_WIRE_CELLS)
        return false;

    const uint8_t *cells = state->cells[variant];
    uint8_t raw[FIAT_V1_WIRE_BYTES] = {0};
    for (uint8_t bit_index = 0; bit_index < FIAT_V1_WIRE_BITS; bit_index++)
    {
        const uint8_t first = cells[bit_index * 2u];
        const uint8_t second = cells[bit_index * 2u + 1u];
        if (first == second)
            return false;

        bool bit = first != 0u;
        if (invert)
            bit = !bit;
        if (bit)
            raw[bit_index >> 3u] |= (uint8_t)(1u << (7u - (bit_index & 7u)));
    }

    return fiat_v1_commit(raw, variant, protocol_index);
}

static bool fiat_v1_push_cell(fiat_v1_state_t *state, uint8_t variant,
                              bool level, uint16_t protocol_index)
{
    uint8_t *cells = state->cells[variant];
    if (state->cell_count[variant] < FIAT_V1_WIRE_CELLS)
    {
        cells[state->cell_count[variant]++] = level ? 1u : 0u;
    }
    else
    {
        memmove(cells, &cells[1], FIAT_V1_WIRE_CELLS - 1u);
        cells[FIAT_V1_WIRE_CELLS - 1u] = level ? 1u : 0u;
    }

    if (fiat_v1_try_window(state, variant, false, protocol_index))
        return true;
    return fiat_v1_try_window(state, variant, true, protocol_index);
}

static void fiat_v1_clear_variant(fiat_v1_state_t *state, uint8_t variant)
{
    state->cell_count[variant] = 0;
}

static bool fiat_v1_feed_data_pulse(fiat_v1_state_t *state, bool level,
                                    uint16_t duration, uint16_t protocol_index,
                                    bool *committed)
{
    bool matched = false;

    for (uint8_t variant = 0; variant < FIAT_V1_VARIANT_COUNT; variant++)
    {
        if (fiat_v1_is_short(variant, duration))
        {
            if (fiat_v1_push_cell(state, variant, level, protocol_index))
                *committed = true;
            matched = true;
        }
        else if (fiat_v1_is_long(variant, duration))
        {
            if (fiat_v1_push_cell(state, variant, level, protocol_index))
                *committed = true;
            if (fiat_v1_push_cell(state, variant, level, protocol_index))
                *committed = true;
            matched = true;
        }
        else
        {
            /* Frame boundary: a trailing low >= 800 us supplies the final
             * low cell when the frame ends in a 1 bit (the last low cell
             * merges with the frame gap on air). */
            if (!level && duration >= FIAT_V1_BOUNDARY_MIN_US)
            {
                if (fiat_v1_push_cell(state, variant, false, protocol_index))
                    *committed = true;
            }
            fiat_v1_clear_variant(state, variant);
        }
    }

    return matched;
}

uint8_t subghz_decode_fiat_v1(uint16_t protocol_index, uint16_t pulse_count)
{
    fiat_v1_state_t state;
    memset(&state, 0, sizeof(state));
    state.step = FiatV1StepReset;

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        const bool level = (i & 1u) == 0u;
        const uint16_t duration = subghz_decenc_ctl.pulse_times[i];
        bool committed = false;

        switch (state.step)
        {
            case FiatV1StepReset:
                if (fiat_v1_is_pulse(duration))
                {
                    fiat_v1_clear_variant(&state, FIAT_V1_VARIANT_A);
                    fiat_v1_clear_variant(&state, FIAT_V1_VARIANT_B);
                    state.step = FiatV1StepData;
                    (void)fiat_v1_feed_data_pulse(&state, level, duration,
                                                  protocol_index, &committed);
                }
                break;

            case FiatV1StepData:
                if (!fiat_v1_feed_data_pulse(&state, level, duration,
                                             protocol_index, &committed))
                    state.step = FiatV1StepReset;
                break;

            default:
                state.step = FiatV1StepReset;
                break;
        }

        if (committed)
            return 0;
    }

    return 1;
}
