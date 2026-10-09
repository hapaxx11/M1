/* See COPYING.txt for license details. */

/*
 * Kia V6 (ProtoPirate) AES-128 decode. Frame layout and key transformation
 * follow RocketGod-git/ProtoPirate protocols/kia_v6.c.
 */

#include "m1_kia_v6_decode.h"

#include <string.h>
#include "m1_sub_ghz_decenc.h"
#include "subghz_protopirate_keys_builtin.h"
#include "../../NFC/amiibo/tiny_aes.h"

#undef CBC
#undef ECB
#undef CTR

#define KIA_TE_SHORT 200u
#define KIA_TE_LONG  400u
#define KIA_TE_DELTA 100u
#define KIA_DATA_BITS_AFTER_SYNC 140u
#define KIA_PREAMBLE_MIN_LOW_PULSES 20u

static uint8_t kia_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0xFFu;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            const bool high_bit = (crc & 0x80u) != 0u;
            crc = (uint8_t)(crc << 1);
            if (high_bit)
                crc ^= 0x07u;
        }
    }
    return crc;
}

bool m1_kia_v6_parse(const uint8_t frame_bits[M1_KIA_V6_FRAME_BITS],
                     const uint64_t keys[2], m1_kia_v6_data_t *data)
{
    if (frame_bits == NULL || keys == NULL || data == NULL)
        return false;

    uint8_t wire[18] = {0};
    for (uint16_t i = 0; i < M1_KIA_V6_FRAME_BITS; i++)
        wire[i / 8u] = (uint8_t)((wire[i / 8u] << 1) | (frame_bits[i] & 1u));

    const uint8_t fx_high = (uint8_t)~wire[0];
    const uint8_t fx_low = (uint8_t)~wire[1];
    if ((fx_high & 0xF0u) != 0x20u || (fx_low & 0xF0u) != 0u)
        return false;

    uint8_t aes_key[16];
    for (uint8_t part = 0; part < 2; part++) {
        const uint32_t high = (uint32_t)(keys[part] >> 32) ^ 0x638766ABu;
        const uint32_t low = (uint32_t)keys[part] ^ 0x84AF25FBu;
        for (uint8_t i = 0; i < 4; i++) {
            aes_key[part * 8u + i] = (uint8_t)(high >> (24u - 8u * i));
            aes_key[part * 8u + 4u + i] = (uint8_t)(low >> (24u - 8u * i));
        }
    }

    uint8_t plain[16];
    for (uint8_t i = 0; i < 16; i++)
        plain[i] = (uint8_t)~wire[i + 2u];
    struct AES_ctx aes;
    AES_init_ctx(&aes, aes_key);
    AES_ECB_decrypt(&aes, plain);

    if ((uint8_t)(kia_crc8(plain, 15) ^ plain[15]) >= 2u)
        return false;

    data->fx = (uint8_t)(((fx_high & 0x0Fu) << 4) | (fx_low & 0x0Fu));
    data->serial = ((uint32_t)plain[4] << 16) |
                   ((uint32_t)plain[5] << 8) | plain[6];
    data->button = plain[7];
    data->counter = ((uint32_t)plain[8] << 24) |
                    ((uint32_t)plain[9] << 16) |
                    ((uint32_t)plain[10] << 8) | plain[11];
    return true;
}

static bool kia_short(uint16_t duration)
{
    return get_diff(duration, KIA_TE_SHORT) < KIA_TE_DELTA;
}

static bool kia_long(uint16_t duration)
{
    return get_diff(duration, KIA_TE_LONG) < KIA_TE_DELTA;
}

static bool kia_try_cells(const uint8_t *cells, uint16_t count, uint16_t start,
                          bool invert, m1_kia_v6_data_t *decoded)
{
    if ((uint32_t)start + KIA_DATA_BITS_AFTER_SYNC * 2u > count)
        return false;

    uint8_t bits[M1_KIA_V6_FRAME_BITS] = {1, 1, 0, 1};
    uint16_t out = 4;
    for (uint16_t i = 0; i < KIA_DATA_BITS_AFTER_SYNC; i++) {
        uint8_t first = cells[start + i * 2u];
        uint8_t second = cells[start + i * 2u + 1u];
        if (invert) {
            first = (uint8_t)!first;
            second = (uint8_t)!second;
        }
        if (first == second)
            return false;
        bits[out++] = (uint8_t)(first == 0u);
    }
    return m1_kia_v6_parse(bits, m1_kia_v6_keys_builtin, decoded);
}

uint8_t subghz_decode_kia_v6(uint16_t protocol_index, uint16_t pulse_count)
{
    if (!m1_kia_v6_keys_builtin_available || pulse_count > PACKET_PULSE_COUNT_MAX)
        return 1;

    for (uint16_t start_level = 0; start_level < 2; start_level++) {
        uint8_t data_cells[PACKET_PULSE_COUNT_MAX * 2u];
        uint16_t data_count = 0;
        uint16_t low_short_count = 0;
        uint8_t step = 0;
        bool valid_stream = true;

        for (uint16_t i = 0; i < pulse_count; i++) {
            const bool level = (((i + start_level) & 1u) == 0u);
            const uint16_t duration = subghz_decenc_ctl.pulse_times[i];

            if (step == 0u) {
                if (!level && kia_short(duration)) {
                    if (low_short_count < UINT16_MAX)
                        low_short_count++;
                } else if (!level && kia_long(duration) &&
                           low_short_count >= KIA_PREAMBLE_MIN_LOW_PULSES) {
                    step = 1u;
                } else if (level && !kia_short(duration)) {
                    low_short_count = 0;
                }
                continue;
            }

            if (step == 1u) {
                if (level && kia_long(duration)) {
                    step = 2u;
                    continue;
                }
                step = 0u;
                low_short_count = 0;
                continue;
            }

            if (step == 2u) {
                step = 3u;
            }

            const uint8_t cells_in_pulse = kia_short(duration) ? 1u :
                                           (kia_long(duration) ? 2u : 0u);
            if (cells_in_pulse == 0u) {
                valid_stream = false;
                break;
            }
            for (uint8_t cell = 0; cell < cells_in_pulse; cell++) {
                if (data_count < sizeof(data_cells))
                    data_cells[data_count++] = (uint8_t)level;
            }
        }

        if (!valid_stream || step != 3u ||
            data_count < KIA_DATA_BITS_AFTER_SYNC * 2u)
            continue;

        m1_kia_v6_data_t decoded;
        for (uint16_t offset = 0; offset + KIA_DATA_BITS_AFTER_SYNC * 2u <= data_count; offset++) {
            if (kia_try_cells(data_cells, data_count, offset, false, &decoded) ||
                kia_try_cells(data_cells, data_count, offset, true, &decoded)) {
                subghz_decenc_ctl.n64_decodedvalue =
                    ((uint64_t)decoded.serial << 32) | decoded.counter;
                subghz_decenc_ctl.n32_serialnumber = decoded.serial;
                subghz_decenc_ctl.n32_rollingcode = decoded.counter;
                subghz_decenc_ctl.n8_buttonid = decoded.button;
                subghz_decenc_ctl.ndecodedbitlength = M1_KIA_V6_FRAME_BITS;
                subghz_decenc_ctl.ndecodeddelay = 0;
                subghz_decenc_ctl.ndecodedprotocol = protocol_index;
                return 0;
            }
        }
    }
    return 1;
}
