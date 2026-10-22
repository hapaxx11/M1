/* See COPYING.txt for license details. */

/*
 * M1 Sub-GHz decoder core shared by the ProtoPirate "Kia V3" and
 * "Kia V4" automotive keyfob protocols.
 *
 * Decode algorithm derived from RocketGod-git/ProtoPirate
 * protocols/kia_v3_v4.c (GPLv3), where one decoder handles both wire
 * polarities.  Here the state machine is shared and the two registry
 * wrappers (m1_kia_v3_decode.c / m1_kia_v4_decode.c) pass an expected
 * polarity so the "Kia V3/V4" (KIA_V3) registry entry never claims V4
 * frames and vice versa.  The TX encoder lives in
 * Sub_Ghz/subghz_proto_pirate.c; this file implements RX identification
 * only.
 *
 * Wire format (both versions): te 400/800 us, delta 150; 12 pairs of
 * 400/400 preamble; sync 400+1200; 64 PWM data bits (LSB-first on air)
 * plus 4 CRC bits; end marker 800+800 (~164 pulses per burst).
 * Polarity: V4 = low-then-high (sync low 400 then high 1200);
 *           V3 = high-then-low (sync high 400 then low 1200).
 * Only the HIGH pulses carry data; for V3 the collected bytes are
 * inverted before parsing (upstream is_v3_sync path).
 *
 * KeeLoq gate: the raw 64-bit on-air word is committed structurally
 * whenever the frame validates by timing/structure (enables identify +
 * save + bitwise replay).  When the built-in Kia manufacturer key
 * (ProtoPirate keystore type 10, KIA_KEY1, m1_kia_mf_key_builtin) is
 * available, the encrypted 32-bit word is additionally decrypted
 * directly with that key (no learning mode) and the plaintext layout
 * 0xBSSSCCCC is validated: the plaintext button nibble must equal the
 * on-air button and plaintext bits 16..23 must equal the low serial
 * byte; serial/button/counter are then filled from the validated frame.
 * Mirroring upstream, a frame that fails this validation is rejected
 * outright (no commit).  When the key is not compiled in, the frame
 * still commits with serial/button/counter zeroed.
 *
 * The trailing 4-bit CRC nibble is extracted but never gates the
 * commit: upstream's encoder sweeps all 16 CRC values per transmission,
 * so RX treats the CRC as non-gating.
 *
 * Adaptation notes vs the upstream event-driven decoder:
 *  - M1 delivers one capture buffer per packet (the pulse handler splits
 *    at >= INTERPACKET_GAP_MIN), so the upstream state machine is re-run
 *    per buffer.
 *  - Upstream commits on the next burst's sync pulse or the trailing
 *    inter-burst gap.  As a documented relaxation, a complete (>=68 bit)
 *    collection is also committed when the capture buffer simply ends in
 *    the data state (M1 may split off the terminating gap).
 *
 * M1 Project -- Hapax fork
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "m1_sub_ghz_decenc.h"
#include "subghz_keeloq.h"
#include "subghz_protopirate_keys_builtin.h"
#include "bit_util.h"

#define KIA_V3_V4_TE_SHORT 400u
#define KIA_V3_V4_TE_LONG  800u
#define KIA_V3_V4_TE_DELTA 150u

/* 64 data bits + 4 CRC bits (upstream min_count_bit_for_found = 68). */
#define KIA_V3_V4_BIT_COUNT 68u

/* Upstream sync window: duration > 1000 && duration < 1500 (1200 us). */
#define KIA_V3_V4_SYNC_MIN 1000u
#define KIA_V3_V4_SYNC_MAX 1500u

/* Upstream requires header_count >= 8 at the sync pulse. */
#define KIA_V3_V4_MIN_PREAMBLE_COUNT 8u

#define KIA_V3_V4_RAW_MAX_BITS 256u

/* Upstream version numbering: 0 = V4 polarity, 1 = V3 polarity. */
#define M1_KIA_V3_V4_VERSION_V4 0u
#define M1_KIA_V3_V4_VERSION_V3 1u

typedef enum {
    KiaV3V4StepReset = 0,
    KiaV3V4StepCheckPreamble,
    KiaV3V4StepCollectRawBits,
} kia_v3_v4_step_t;

typedef struct {
    kia_v3_v4_step_t step;
    uint16_t te_last;
    uint16_t header_count;
    uint8_t raw_bits[KIA_V3_V4_RAW_MAX_BITS / 8u];
    uint16_t raw_bit_count;
    bool is_v3_sync;
} kia_v3_v4_state_t;

/*--------------------------------------------------------------------*/
/* Pure frame parser (exported for host tests)                         */
/*--------------------------------------------------------------------*/

/* Parse and validate one collected frame.  raw_bits holds the collected
 * bits packed MSB-first per byte, exactly as the state machine gathered
 * them (pre-inversion for V3).  The KeeLoq key and its availability are
 * parameters so host tests can exercise every gating branch. */
bool m1_kia_v3_v4_parse_frame(const uint8_t raw_bits[9], uint16_t raw_bit_count,
                              uint8_t version, uint64_t mf_key, bool key_available,
                              uint64_t *raw, uint32_t *serial,
                              uint8_t *button, uint16_t *counter)
{
    if (raw_bit_count < KIA_V3_V4_BIT_COUNT)
        return false;

    uint8_t b[9];
    memcpy(b, raw_bits, sizeof(b));

    if (version == M1_KIA_V3_V4_VERSION_V3)
    {
        /* Upstream inverts (raw_bit_count + 7) / 8 bytes; only the first
         * 9 carry fields, so inverting those is equivalent. */
        for (uint8_t i = 0; i < sizeof(b); i++)
            b[i] = (uint8_t)~b[i];
    }

    /* CRC nibble: (b[8] >> 4) & 0xF — extracted by upstream for display
     * only; never compared on RX (the upstream encoder sweeps all 16
     * values), so it is non-gating here as well. */

    const uint32_t encrypted =
        ((uint32_t)reverse8(b[3]) << 24) | ((uint32_t)reverse8(b[2]) << 16) |
        ((uint32_t)reverse8(b[1]) << 8) | (uint32_t)reverse8(b[0]);

    const uint32_t ser =
        ((uint32_t)reverse8((uint8_t)(b[7] & 0xF0u)) << 24) |
        ((uint32_t)reverse8(b[6]) << 16) |
        ((uint32_t)reverse8(b[5]) << 8) | (uint32_t)reverse8(b[4]);

    const uint8_t btn = (uint8_t)(reverse8(b[7]) >> 4);

    *raw = ((uint64_t)b[0] << 56) | ((uint64_t)b[1] << 48) |
           ((uint64_t)b[2] << 40) | ((uint64_t)b[3] << 32) |
           ((uint64_t)b[4] << 24) | ((uint64_t)b[5] << 16) |
           ((uint64_t)b[6] << 8) | (uint64_t)b[7];

    if (key_available)
    {
        /* Direct manufacturer-key decryption (upstream
         * subghz_protocol_keeloq_common_decrypt(encrypted, KIA_KEY1)) —
         * no learning mode.  Plaintext layout 0xBSSSCCCC. */
        const uint32_t decrypted = keeloq_decrypt(encrypted, mf_key);
        if ((uint8_t)((decrypted >> 28) & 0x0Fu) != btn)
            return false; /* upstream: decrypt mismatch -> reject */
        if ((uint8_t)((decrypted >> 16) & 0xFFu) != (uint8_t)(ser & 0xFFu))
            return false;
        *serial = ser;
        *button = btn;
        *counter = (uint16_t)(decrypted & 0xFFFFu);
    }
    else
    {
        /* No built-in key: commit the raw word with zeroed fields. */
        *serial = 0;
        *button = 0;
        *counter = 0;
    }
    return true;
}

/*--------------------------------------------------------------------*/
/* Commit                                                              */
/*--------------------------------------------------------------------*/

static bool kia_v3_v4_try_commit(kia_v3_v4_state_t *state,
                                 uint16_t protocol_index,
                                 uint8_t expected_version)
{
    const uint8_t version =
        state->is_v3_sync ? M1_KIA_V3_V4_VERSION_V3 : M1_KIA_V3_V4_VERSION_V4;
    uint64_t raw = 0;
    uint32_t serial = 0;
    uint8_t button = 0;
    uint16_t counter = 0;

    /* Polarity filter: each registry wrapper claims only its own version. */
    if (version != expected_version)
        return false;

    if (!m1_kia_v3_v4_parse_frame(state->raw_bits, state->raw_bit_count, version,
                                  m1_kia_mf_key_builtin,
                                  m1_kia_mf_key_builtin_available,
                                  &raw, &serial, &button, &counter))
        return false;

    subghz_decenc_ctl.n64_decodedvalue = raw;
    subghz_decenc_ctl.n32_serialnumber = serial;
    subghz_decenc_ctl.n32_rollingcode = counter;
    subghz_decenc_ctl.n8_buttonid = button;
    subghz_decenc_ctl.ndecodedbitlength = KIA_V3_V4_BIT_COUNT;
    subghz_decenc_ctl.ndecodeddelay = 0;
    subghz_decenc_ctl.ndecodedprotocol = protocol_index;
    return true;
}

/*--------------------------------------------------------------------*/
/* Waveform state machine (ported from upstream feed())                */
/*--------------------------------------------------------------------*/

static void kia_v3_v4_state_clear(kia_v3_v4_state_t *state)
{
    memset(state, 0, sizeof(*state));
}

static bool kia_v3_v4_is_short(uint16_t duration)
{
    return get_diff(duration, KIA_V3_V4_TE_SHORT) < KIA_V3_V4_TE_DELTA;
}

static bool kia_v3_v4_is_long(uint16_t duration)
{
    return get_diff(duration, KIA_V3_V4_TE_LONG) < KIA_V3_V4_TE_DELTA;
}

static bool kia_v3_v4_is_sync(uint16_t duration)
{
    return duration > KIA_V3_V4_SYNC_MIN && duration < KIA_V3_V4_SYNC_MAX;
}

static void kia_v3_v4_add_raw_bit(kia_v3_v4_state_t *state, bool bit)
{
    if (state->raw_bit_count < KIA_V3_V4_RAW_MAX_BITS)
    {
        const uint16_t byte_idx = state->raw_bit_count / 8u;
        const uint8_t bit_idx = (uint8_t)(7u - (state->raw_bit_count % 8u));
        if (bit)
            state->raw_bits[byte_idx] |= (uint8_t)(1u << bit_idx);
        else
            state->raw_bits[byte_idx] &= (uint8_t)~(1u << bit_idx);
        state->raw_bit_count++;
    }
}

static void kia_v3_v4_start_collect(kia_v3_v4_state_t *state, bool is_v3)
{
    state->step = KiaV3V4StepCollectRawBits;
    state->raw_bit_count = 0;
    state->is_v3_sync = is_v3;
    memset(state->raw_bits, 0, sizeof(state->raw_bits));
}

static bool kia_v3_v4_feed(kia_v3_v4_state_t *state, bool level,
                           uint16_t duration, uint16_t protocol_index,
                           uint8_t expected_version)
{
    switch (state->step)
    {
        case KiaV3V4StepReset:
            if (level && kia_v3_v4_is_short(duration))
            {
                state->step = KiaV3V4StepCheckPreamble;
                state->te_last = duration;
                state->header_count = 1;
            }
            break;

        case KiaV3V4StepCheckPreamble:
            if (level)
            {
                if (kia_v3_v4_is_short(duration))
                {
                    state->te_last = duration;
                }
                else if (kia_v3_v4_is_sync(duration))
                {
                    /* V4: sync low 400 then high 1200. */
                    if (state->header_count >= KIA_V3_V4_MIN_PREAMBLE_COUNT)
                        kia_v3_v4_start_collect(state, false);
                    else
                        kia_v3_v4_state_clear(state);
                }
                else
                {
                    kia_v3_v4_state_clear(state);
                }
            }
            else
            {
                if (kia_v3_v4_is_sync(duration))
                {
                    /* V3: sync high 400 then low 1200. */
                    if (state->header_count >= KIA_V3_V4_MIN_PREAMBLE_COUNT)
                        kia_v3_v4_start_collect(state, true);
                    else
                        kia_v3_v4_state_clear(state);
                }
                else if (kia_v3_v4_is_short(duration) &&
                         kia_v3_v4_is_short(state->te_last))
                {
                    state->header_count++;
                }
                else if (duration > KIA_V3_V4_SYNC_MAX)
                {
                    kia_v3_v4_state_clear(state);
                }
            }
            break;

        case KiaV3V4StepCollectRawBits:
            if (level)
            {
                if (kia_v3_v4_is_sync(duration))
                {
                    /* Next burst's sync: commit the completed frame. */
                    const bool committed =
                        kia_v3_v4_try_commit(state, protocol_index,
                                             expected_version);
                    kia_v3_v4_state_clear(state);
                    return committed;
                }
                if (kia_v3_v4_is_short(duration))
                {
                    kia_v3_v4_add_raw_bit(state, false);
                }
                else if (kia_v3_v4_is_long(duration))
                {
                    kia_v3_v4_add_raw_bit(state, true);
                }
                else
                {
                    kia_v3_v4_state_clear(state);
                }
            }
            else
            {
                if (kia_v3_v4_is_sync(duration) || duration > KIA_V3_V4_SYNC_MAX)
                {
                    /* Next burst's sync or the trailing inter-burst gap. */
                    const bool committed =
                        kia_v3_v4_try_commit(state, protocol_index,
                                             expected_version);
                    kia_v3_v4_state_clear(state);
                    return committed;
                }
            }
            break;

        default:
            kia_v3_v4_state_clear(state);
            break;
    }
    return false;
}

uint8_t m1_kia_v3_v4_decode_frame(uint16_t protocol_index, uint16_t pulse_count,
                                  uint8_t expected_version)
{
    if (pulse_count > PACKET_PULSE_COUNT_MAX)
        return 1;

    kia_v3_v4_state_t state;
    kia_v3_v4_state_clear(&state);

    for (uint16_t i = 0; i < pulse_count; i++)
    {
        if (kia_v3_v4_feed(&state, (i & 1u) == 0u,
                           subghz_decenc_ctl.pulse_times[i], protocol_index,
                           expected_version))
            return 0;
    }

    /* Packet-end relaxation: M1 may split off the terminating gap, so a
     * complete collection is committed when the buffer runs out. */
    if (state.step == KiaV3V4StepCollectRawBits &&
        kia_v3_v4_try_commit(&state, protocol_index, expected_version))
        return 0;
    return 1;
}
