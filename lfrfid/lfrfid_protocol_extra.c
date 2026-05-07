/* See COPYING.txt for license details. */

/*
 * Additional LF RFID protocol support.
 *
 * Decoder layouts are ported/adapted from the GPLv3 Flipper Zero LF RFID
 * protocol implementations and fitted to this project's static protocol table.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "app_freertos.h"
#include "cmsis_os.h"
#include "main.h"
#include "uiView.h"

#include "lfrfid.h"

#define EXTRA_GPIO_PIN             2U
#define FSK_JITTER_TIME            20U
#define FSK_MIN_TIME               (64U - FSK_JITTER_TIME)
#define FSK_MAX_TIME               (80U + FSK_JITTER_TIME)

#define HID_DATA_SIZE              11U
#define HID_PREAMBLE_SIZE          1U
#define HID_ENCODED_DATA_SIZE      (HID_PREAMBLE_SIZE + HID_DATA_SIZE + HID_PREAMBLE_SIZE)
#define HID_ENCODED_BIT_SIZE       ((HID_PREAMBLE_SIZE + HID_DATA_SIZE) * 8U)
#define HID_DECODED_DATA_SIZE      6U
#define HID_DECODED_BIT_SIZE       ((HID_ENCODED_BIT_SIZE - HID_PREAMBLE_SIZE * 8U) / 2U)
#define HID_EX_DATA_SIZE           23U
#define HID_EX_ENCODED_DATA_SIZE   (HID_PREAMBLE_SIZE + HID_EX_DATA_SIZE + HID_PREAMBLE_SIZE)
#define HID_EX_ENCODED_BIT_SIZE    ((HID_PREAMBLE_SIZE + HID_EX_DATA_SIZE) * 8U)
#define HID_EX_DECODED_DATA_SIZE   12U
#define HID_EX_DECODED_BIT_SIZE    ((HID_EX_ENCODED_BIT_SIZE - HID_PREAMBLE_SIZE * 8U) / 2U)
#define HID_PREAMBLE               0x1DU

#define IOPROX_DECODED_DATA_SIZE   4U
#define IOPROX_ENCODED_DATA_SIZE   8U
#define IOPROX_ENCODED_BIT_SIZE    64U

#define AWID_DECODED_DATA_SIZE     9U
#define AWID_ENCODED_BIT_SIZE      96U
#define AWID_ENCODED_DATA_SIZE     ((AWID_ENCODED_BIT_SIZE / 8U) + 1U)
#define AWID_ENCODED_LAST          (AWID_ENCODED_DATA_SIZE - 1U)

#define FDXA_DATA_SIZE             10U
#define FDXA_PREAMBLE_SIZE         2U
#define FDXA_ENCODED_DATA_SIZE     (FDXA_PREAMBLE_SIZE + FDXA_DATA_SIZE + FDXA_PREAMBLE_SIZE)
#define FDXA_ENCODED_BIT_SIZE      ((FDXA_PREAMBLE_SIZE + FDXA_DATA_SIZE) * 8U)
#define FDXA_DECODED_DATA_SIZE     5U
#define FDXA_DECODED_BIT_SIZE      ((FDXA_ENCODED_BIT_SIZE - FDXA_PREAMBLE_SIZE * 8U) / 2U)
#define FDXA_PREAMBLE_0            0x55U
#define FDXA_PREAMBLE_1            0x1DU

#define PYRAMID_DATA_SIZE          13U
#define PYRAMID_PREAMBLE_SIZE      3U
#define PYRAMID_ENCODED_DATA_SIZE  (PYRAMID_PREAMBLE_SIZE + PYRAMID_DATA_SIZE + PYRAMID_PREAMBLE_SIZE)
#define PYRAMID_ENCODED_BIT_SIZE   ((PYRAMID_PREAMBLE_SIZE + PYRAMID_DATA_SIZE) * 8U)
#define PYRAMID_DECODED_DATA_SIZE  4U

#define INDALA26_ENCODED_BIT_SIZE  64U
#define INDALA26_ENCODED_DATA_SIZE ((INDALA26_ENCODED_BIT_SIZE / 8U) + 5U)
#define INDALA26_DECODED_DATA_SIZE 4U
#define INDALA26_US_PER_BIT        255U

#define IDTECK_ENCODED_BIT_SIZE    64U
#define IDTECK_ENCODED_DATA_SIZE   ((IDTECK_ENCODED_BIT_SIZE / 8U) + 8U)
#define IDTECK_DECODED_DATA_SIZE   8U
#define IDTECK_US_PER_BIT          255U

#define KERI_ENCODED_BIT_SIZE      64U
#define KERI_ENCODED_DATA_SIZE     ((KERI_ENCODED_BIT_SIZE / 8U) + 5U)
#define KERI_DECODED_DATA_SIZE     4U
#define KERI_US_PER_BIT            255U

#define NEXWATCH_ENCODED_BIT_SIZE  96U
#define NEXWATCH_ENCODED_DATA_SIZE (NEXWATCH_ENCODED_BIT_SIZE / 8U)
#define NEXWATCH_DECODED_DATA_SIZE 8U
#define NEXWATCH_US_PER_BIT        255U

#define JABLOTRON_ENCODED_BIT_SIZE  64U
#define JABLOTRON_ENCODED_DATA_SIZE ((JABLOTRON_ENCODED_BIT_SIZE / 8U) + 2U)
#define JABLOTRON_DECODED_DATA_SIZE 5U
#define JABLOTRON_SHORT_TIME_LOW    136U
#define JABLOTRON_SHORT_TIME_HIGH   376U
#define JABLOTRON_LONG_TIME_LOW     392U
#define JABLOTRON_LONG_TIME_HIGH    632U

#define VIKING_ENCODED_BIT_SIZE     64U
#define VIKING_ENCODED_DATA_SIZE    ((VIKING_ENCODED_BIT_SIZE / 8U) + 3U)
#define VIKING_DECODED_DATA_SIZE    4U
#define VIKING_SHORT_TIME_LOW       68U
#define VIKING_SHORT_TIME_HIGH      188U
#define VIKING_LONG_TIME_LOW        196U
#define VIKING_LONG_TIME_HIGH       316U

#define PARADOX_DECODED_DATA_SIZE   6U
#define PARADOX_PREAMBLE_LENGTH     8U
#define PARADOX_ENCODED_BIT_SIZE    96U
#define PARADOX_ENCODED_DATA_SIZE   ((PARADOX_ENCODED_BIT_SIZE / 8U) + 1U)
#define PARADOX_ENCODED_LAST        (PARADOX_ENCODED_DATA_SIZE - 1U)

#define PAC_STANLEY_ENCODED_BIT_SIZE   128U
#define PAC_STANLEY_ENCODED_DATA_SIZE  ((PAC_STANLEY_ENCODED_BIT_SIZE / 8U) + 1U)
#define PAC_STANLEY_BYTE_LENGTH        10U
#define PAC_STANLEY_DATA_START_INDEX   (8U + (3U * PAC_STANLEY_BYTE_LENGTH) + 1U)
#define PAC_STANLEY_DECODED_DATA_SIZE  4U
#define PAC_STANLEY_CYCLE_LENGTH       256U
#define PAC_STANLEY_MIN_TIME           60U
#define PAC_STANLEY_MAX_TIME           4000U

#define NORALSY_ENCODED_BIT_SIZE       96U
#define NORALSY_ENCODED_DATA_SIZE      (NORALSY_ENCODED_BIT_SIZE / 8U)
#define NORALSY_DECODED_DATA_SIZE      NORALSY_ENCODED_DATA_SIZE
#define NORALSY_SHORT_TIME_LOW         68U
#define NORALSY_SHORT_TIME_HIGH        188U
#define NORALSY_LONG_TIME_LOW          196U
#define NORALSY_LONG_TIME_HIGH         316U

#define FDXB_ENCODED_BIT_SIZE          128U
#define FDXB_ENCODED_DATA_SIZE         ((FDXB_ENCODED_BIT_SIZE / 8U) + 2U)
#define FDXB_DECODED_DATA_SIZE         11U
#define FDXB_SHORT_TIME_LOW            68U
#define FDXB_SHORT_TIME_HIGH           188U
#define FDXB_LONG_TIME_LOW             196U
#define FDXB_LONG_TIME_HIGH            316U

#define GPROXII_ENCODED_BIT_SIZE       90U
#define GPROXII_ENCODED_DATA_SIZE      ((6U + GPROXII_ENCODED_BIT_SIZE) / 8U)
#define GPROXII_DATA_SIZE              12U
#define GPROXII_SHORT_TIME_LOW         136U
#define GPROXII_SHORT_TIME_HIGH        376U
#define GPROXII_LONG_TIME_LOW          392U
#define GPROXII_LONG_TIME_HIGH         632U

#define SECURAKEY_RKKT_BITS            96U
#define SECURAKEY_RKKT_SIZE            12U
#define SECURAKEY_RKKTH_BITS           64U
#define SECURAKEY_RKKTH_SIZE           8U
#define SECURAKEY_DECODED_DATA_SIZE    6U
#define SECURAKEY_SHORT_TIME_LOW       120U
#define SECURAKEY_SHORT_TIME_HIGH      200U
#define SECURAKEY_LONG_TIME_LOW        240U
#define SECURAKEY_LONG_TIME_HIGH       400U

#define GALLAGHER_ENCODED_BIT_SIZE     96U
#define GALLAGHER_ENCODED_DATA_SIZE    12U
#define GALLAGHER_ROLLING_DATA_SIZE    14U
#define GALLAGHER_SHORT_TIME_LOW       68U
#define GALLAGHER_SHORT_TIME_HIGH      188U
#define GALLAGHER_LONG_TIME_LOW        196U
#define GALLAGHER_LONG_TIME_HIGH       316U

#define ELECTRA_ENCODED_BIT_SIZE       128U
#define ELECTRA_ENCODED_DATA_SIZE      16U
#define ELECTRA_SHORT_TIME_LOW         156U
#define ELECTRA_SHORT_TIME_HIGH        356U
#define ELECTRA_LONG_TIME_LOW          412U
#define ELECTRA_LONG_TIME_HIGH         612U

typedef enum {
    ExtraParityEven,
    ExtraParityOdd,
    ExtraParityAlways0,
    ExtraParityAlways1,
} extra_parity_t;

typedef struct {
    uint32_t low_time;
    uint32_t low_pulses;
    uint32_t hi_time;
    uint32_t hi_pulses;
    bool invert;
    uint32_t mid_time;
    uint32_t time;
    uint32_t count;
    bool last_pulse;
} extra_fsk_demod_t;

typedef struct {
    uint16_t freq[2];
    uint16_t phase_max;
    int32_t phase_current;
    uint32_t pulse;
} extra_fsk_osc_t;

typedef struct {
    extra_fsk_demod_t demod;
    uint8_t encoded[HID_EX_ENCODED_DATA_SIZE];
} extra_decoder_state_t;

typedef struct {
    uint8_t encoded[16];
    uint8_t encoded_inv[16];
    uint8_t encoded_skew[16];
    uint8_t encoded_skew_inv[16];
    uint8_t data[16];
    uint8_t encoded_size;
    bool inverted;
    bool got_preamble;
} direct_decoder_state_t;

typedef enum {
    ManchesterEventShortLow = 0,
    ManchesterEventShortHigh = 2,
    ManchesterEventLongLow = 4,
    ManchesterEventLongHigh = 6,
    ManchesterEventReset = 8,
} extra_manchester_event_t;

typedef enum {
    ManchesterStateStart1 = 0,
    ManchesterStateMid1 = 1,
    ManchesterStateMid0 = 2,
    ManchesterStateStart0 = 3,
} extra_manchester_state_t;

typedef struct {
    uint8_t encoded[16];
    uint8_t data[16];
    bool last_short;
    extra_manchester_state_t manchester;
} manchester_decoder_state_t;

static extra_decoder_state_t s_hid;
static extra_decoder_state_t s_hid_ex;
static extra_decoder_state_t s_ioprox;
static extra_decoder_state_t s_awid;
static extra_decoder_state_t s_fdxa;
static extra_decoder_state_t s_pyramid;
static extra_decoder_state_t s_paradox;
static direct_decoder_state_t s_indala26;
static direct_decoder_state_t s_idteck;
static direct_decoder_state_t s_keri;
static direct_decoder_state_t s_nexwatch;
static manchester_decoder_state_t s_jablotron;
static manchester_decoder_state_t s_viking;
static direct_decoder_state_t s_pac_stanley;
static manchester_decoder_state_t s_noralsy;
static manchester_decoder_state_t s_fdxb;
static manchester_decoder_state_t s_gproxii;
static manchester_decoder_state_t s_securakey;
static manchester_decoder_state_t s_gallagher;
static manchester_decoder_state_t s_electra;

static void extra_fsk_demod_init(extra_fsk_demod_t *demod, uint32_t low_time, uint32_t low_pulses, uint32_t hi_time, uint32_t hi_pulses)
{
    memset(demod, 0, sizeof(*demod));
    if (low_time > hi_time)
    {
        uint32_t tmp = hi_time;
        hi_time = low_time;
        low_time = tmp;
        tmp = hi_pulses;
        hi_pulses = low_pulses;
        low_pulses = tmp;
        demod->invert = true;
    }
    demod->low_time = low_time;
    demod->low_pulses = low_pulses;
    demod->hi_time = hi_time;
    demod->hi_pulses = hi_pulses;
    demod->mid_time = (hi_time - low_time) / 2U + low_time;
}

static void extra_fsk_demod_feed(extra_fsk_demod_t *demod, bool polarity, uint32_t time, bool *value, uint32_t *count)
{
    *count = 0;
    if (polarity)
    {
        demod->time = time;
        return;
    }

    demod->time += time;
    if (demod->time >= demod->low_time && demod->time < demod->hi_time)
    {
        bool pulse = (demod->time >= demod->mid_time);
        demod->count++;
        if (demod->last_pulse != pulse)
        {
            uint32_t data_count = demod->count + 1U;
            if (demod->last_pulse)
            {
                data_count /= demod->hi_pulses;
                *value = !demod->invert;
            }
            else
            {
                data_count /= demod->low_pulses;
                *value = demod->invert;
            }
            *count = data_count;
            demod->count = 0;
            demod->last_pulse = pulse;
        }
    }
    else
    {
        demod->count = 0;
    }
}

static void extra_fsk_osc_init(extra_fsk_osc_t *osc, uint32_t freq_low, uint32_t freq_hi, uint32_t phase_max)
{
    osc->freq[0] = (uint16_t)freq_low;
    osc->freq[1] = (uint16_t)freq_hi;
    osc->phase_max = (uint16_t)phase_max;
    osc->phase_current = 0;
    osc->pulse = 0;
}

static bool extra_fsk_osc_next(extra_fsk_osc_t *osc, bool bit, uint32_t *period)
{
    bool advance = false;
    *period = osc->freq[bit ? 1 : 0];
    osc->phase_current += (int32_t)*period;
    if (osc->phase_current > osc->phase_max)
    {
        advance = true;
        osc->phase_current -= osc->phase_max;
    }
    return advance;
}

static bool extra_fsk_osc_next_half(extra_fsk_osc_t *osc, bool bit, bool *level, uint32_t *duration)
{
    bool advance = false;
    if (osc->pulse == 0)
    {
        uint32_t length;
        advance = extra_fsk_osc_next(osc, bit, &length);
        *duration = length / 2U;
        osc->pulse = *duration;
        *level = true;
    }
    else
    {
        *duration = osc->pulse;
        osc->pulse = 0;
        *level = false;
    }
    return advance;
}

static void bit_push(uint8_t *data, size_t data_size, bool bit)
{
    size_t last = data_size - 1U;
    for (size_t i = 0; i < last; i++)
        data[i] = (uint8_t)((data[i] << 1) | ((data[i + 1U] >> 7) & 1U));
    data[last] = (uint8_t)((data[last] << 1) | (bit ? 1U : 0U));
}

static void bit_set(uint8_t *data, size_t pos, bool bit)
{
    uint8_t mask = (uint8_t)(1U << (7U - (pos % 8U)));
    if (bit)
        data[pos / 8U] |= mask;
    else
        data[pos / 8U] &= (uint8_t)~mask;
}

static bool bit_get(const uint8_t *data, size_t pos)
{
    return ((data[pos / 8U] >> (7U - (pos % 8U))) & 1U) != 0;
}

static void bit_set_bits(uint8_t *data, size_t pos, uint8_t byte, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++)
        bit_set(data, pos + i, ((byte >> ((len - 1U) - i)) & 1U) != 0);
}

static uint8_t bit_get_bits(const uint8_t *data, size_t pos, uint8_t len)
{
    uint8_t v = 0;
    for (uint8_t i = 0; i < len; i++)
        v = (uint8_t)((v << 1) | (bit_get(data, pos + i) ? 1U : 0U));
    return v;
}

static uint32_t bit_get_bits32(const uint8_t *data, size_t pos, uint8_t len)
{
    uint32_t v = 0;
    for (uint8_t i = 0; i < len; i++)
        v = (v << 1) | (bit_get(data, pos + i) ? 1U : 0U);
    return v;
}

static uint64_t bit_get_bits64(const uint8_t *data, size_t pos, uint8_t len)
{
    uint64_t v = 0;
    for (uint8_t i = 0; i < len; i++)
        v = (v << 1) | (bit_get(data, pos + i) ? 1ULL : 0ULL);
    return v;
}

static void bit_copy(uint8_t *dst, size_t dst_pos, size_t len, const uint8_t *src, size_t src_pos)
{
    for (size_t i = 0; i < len; i++)
        bit_set(dst, dst_pos + i, bit_get(src, src_pos + i));
}

static bool bit_parity32(uint32_t value, extra_parity_t parity)
{
    bool odd = (__builtin_parity(value) != 0);
    return (parity == ExtraParityOdd) ? !odd : odd;
}

static uint8_t bit_crc8_reflected(const uint8_t *data, uint8_t len, uint8_t poly, uint8_t init)
{
    uint8_t crc = init;
    for (uint8_t i = 0; i < len; i++)
    {
        uint8_t in = data[i];
        for (uint8_t n = 0; n < 8U; n++)
        {
            bool mix = ((crc ^ in) & 0x01U) != 0;
            crc >>= 1;
            if (mix) crc ^= poly;
            in >>= 1;
        }
    }
    return crc;
}

static uint16_t bit_crc16_ccitt(const uint8_t *data, uint8_t len)
{
    uint16_t crc = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t b = 0; b < 8U; b++)
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    }
    return crc;
}

static uint8_t bit_reverse8(uint8_t value)
{
    value = (uint8_t)(((value & 0xF0U) >> 4U) | ((value & 0x0FU) << 4U));
    value = (uint8_t)(((value & 0xCCU) >> 2U) | ((value & 0x33U) << 2U));
    value = (uint8_t)(((value & 0xAAU) >> 1U) | ((value & 0x55U) << 1U));
    return value;
}

static uint8_t hex_nibble(uint8_t c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10U);
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10U);
    return 0;
}

static uint8_t hex_char(uint8_t v)
{
    v &= 0x0FU;
    if (v < 10U) return (uint8_t)('0' + v);
    return (uint8_t)('A' + v - 10U);
}

static void bytes_to_hex_chars(const uint8_t *src, uint8_t *dst, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++)
    {
        dst[i * 2U] = hex_char(src[i] >> 4);
        dst[i * 2U + 1U] = hex_char(src[i]);
    }
}

static void hex_chars_to_bytes(const uint8_t *src, uint8_t *dst, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++)
        dst[i] = (uint8_t)((hex_nibble(src[i * 2U]) << 4) | hex_nibble(src[i * 2U + 1U]));
}

static bool extra_manchester_advance(extra_manchester_state_t state, extra_manchester_event_t event, extra_manchester_state_t *next_state, bool *data)
{
    static const uint8_t transitions[] = {0x01U, 0x91U, 0x9BU, 0xFBU};
    bool result = false;
    extra_manchester_state_t new_state;

    if (event == ManchesterEventReset)
    {
        new_state = ManchesterStateMid1;
    }
    else
    {
        new_state = (extra_manchester_state_t)((transitions[state] >> event) & 0x03U);
        if (new_state == state)
        {
            new_state = ManchesterStateMid1;
        }
        else if (new_state == ManchesterStateMid0)
        {
            *data = false;
            result = true;
        }
        else if (new_state == ManchesterStateMid1)
        {
            *data = true;
            result = true;
        }
    }

    *next_state = new_state;
    return result;
}

static bool bit_test_parity(const uint8_t *bits, size_t pos, uint8_t len, extra_parity_t parity, uint8_t block_len)
{
    uint8_t blocks = len / block_len;
    for (uint8_t i = 0; i < blocks; i++)
    {
        if (parity == ExtraParityAlways0)
        {
            if (bit_get(bits, pos + i * block_len + block_len - 1U)) return true;
        }
        else if (parity == ExtraParityAlways1)
        {
            if (!bit_get(bits, pos + i * block_len + block_len - 1U)) return true;
        }
        else if (bit_parity32(bit_get_bits32(bits, pos + i * block_len, block_len), parity))
        {
            return true;
        }
    }
    return false;
}

static size_t bit_remove_every_nth(uint8_t *data, size_t pos, uint8_t len, uint8_t nth)
{
    size_t out = 0;
    uint8_t buf = 0;
    uint8_t cnt = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        if (((i + 1U) % nth) != 0)
        {
            buf = (uint8_t)((buf << 1) | (bit_get(data, pos + i) ? 1U : 0U));
            cnt++;
        }
        if (cnt == 8U)
        {
            bit_set_bits(data, pos + out, buf, 8);
            out += 8U;
            cnt = 0;
            buf = 0;
        }
    }
    if (cnt != 0)
    {
        bit_set_bits(data, pos + out, buf, cnt);
        out += cnt;
    }
    return out;
}

static bool bit_parity_for_type(uint32_t value, extra_parity_t parity)
{
    bool odd = (__builtin_parity(value) != 0);
    if (parity == ExtraParityOdd) return !odd;
    if (parity == ExtraParityEven) return odd;
    return parity == ExtraParityAlways1;
}

static void bit_add_parity(const uint8_t *src, size_t src_pos, uint8_t *dst, size_t dst_pos, uint8_t source_len, uint8_t block_len, extra_parity_t parity)
{
    size_t out = 0;
    for (uint8_t word = 0; word < source_len; word = (uint8_t)(word + block_len - 1U))
    {
        uint32_t parity_word = 0;
        for (uint8_t bit = 0; bit < (block_len - 1U); bit++)
        {
            bool v = bit_get(src, src_pos + word + bit);
            parity_word = (parity_word << 1) | (v ? 1U : 0U);
            bit_set(dst, dst_pos + out++, v);
        }
        bit_set(dst, dst_pos + out++, bit_parity_for_type(parity_word, parity));
    }
}

static void encoded_to_t5577(const uint8_t *encoded, uint8_t block_count)
{
    for (uint8_t i = 0; i < block_count; i++)
        lfrfid_program->t5577.block_data[i + 1U] = bit_get_bits32(encoded, i * 32U, 32);
}

static void fsk_wave_from_bits(const uint8_t *encoded, uint16_t bit_count, uint8_t low_freq, uint8_t hi_freq, uint8_t phase_max)
{
    extra_fsk_osc_t osc;
    uint16_t bit_index = 0;
    uint16_t step_index = 0;
    extra_fsk_osc_init(&osc, low_freq, hi_freq, phase_max);

    while (bit_index < bit_count && step_index < ENCODED_DATA_MAX)
    {
        bool level;
        uint32_t duration;
        bool advance = extra_fsk_osc_next_half(&osc, bit_get(encoded, bit_index), &level, &duration);
        lfrfid_encoded_data.data[step_index].bsrr = level ? (1U << EXTRA_GPIO_PIN) : (1U << (EXTRA_GPIO_PIN + 16U));
        lfrfid_encoded_data.data[step_index].time_us = (uint16_t)duration;
        step_index++;
        if (advance) bit_index++;
    }

    lfrfid_encoded_data.length = step_index;
}

static void wave_step(uint16_t *step_index, bool level, uint16_t duration)
{
    if (*step_index >= ENCODED_DATA_MAX) return;
    lfrfid_encoded_data.data[*step_index].bsrr = level ? (1U << EXTRA_GPIO_PIN) : (1U << (EXTRA_GPIO_PIN + 16U));
    lfrfid_encoded_data.data[*step_index].time_us = duration;
    (*step_index)++;
}

static void manchester_wave_from_bits(const uint8_t *encoded, uint16_t bit_count, uint16_t half_us)
{
    uint16_t step_index = 0;
    for (uint16_t i = 0; i < bit_count && (step_index + 1U) < ENCODED_DATA_MAX; i++)
    {
        bool level = bit_get(encoded, i);
        wave_step(&step_index, level, half_us);
        wave_step(&step_index, !level, half_us);
    }
    lfrfid_encoded_data.length = step_index;
}

static void biphase_wave_from_bits(const uint8_t *encoded, uint16_t bit_count, uint16_t short_us, uint16_t long_us)
{
    uint16_t step_index = 0;
    bool level = false;
    for (uint16_t i = 0; i < bit_count && (step_index + 1U) < ENCODED_DATA_MAX; i++)
    {
        level = !level;
        if (bit_get(encoded, i))
        {
            wave_step(&step_index, level, long_us);
        }
        else
        {
            wave_step(&step_index, level, short_us);
            level = !level;
            wave_step(&step_index, level, short_us);
        }
    }
    lfrfid_encoded_data.length = step_index;
}

static void biphase_inverse_wave_from_bits(const uint8_t *encoded, uint16_t bit_count, uint16_t short_us, uint16_t long_us)
{
    uint16_t step_index = 0;
    bool level = false;
    for (uint16_t i = 0; i < bit_count && (step_index + 1U) < ENCODED_DATA_MAX; i++)
    {
        level = !level;
        if (bit_get(encoded, i))
        {
            wave_step(&step_index, level, short_us);
            level = !level;
            wave_step(&step_index, level, short_us);
        }
        else
        {
            wave_step(&step_index, level, long_us);
        }
    }
    lfrfid_encoded_data.length = step_index;
}

static void direct_wave_from_bits(const uint8_t *encoded, uint16_t bit_count, uint16_t clock_us)
{
    uint16_t step_index = 0;
    uint16_t bit_index = 0;

    while (bit_index < bit_count && step_index < ENCODED_DATA_MAX)
    {
        bool bit = bit_get(encoded, bit_index);
        uint16_t duration = clock_us;
        bit_index++;
        while (bit_index < bit_count && bit_get(encoded, bit_index) == bit && duration < (uint16_t)(0xFFFFU - clock_us))
        {
            duration = (uint16_t)(duration + clock_us);
            bit_index++;
        }
        wave_step(&step_index, bit, duration);
    }

    lfrfid_encoded_data.length = step_index;
}

static void psk_wave_from_bits(const uint8_t *encoded, uint16_t bit_count)
{
    uint16_t step_index = 0;
    bool last_bit = bit_get(encoded, bit_count - 1U);
    bool polarity = true;

    for (uint16_t i = 0; i < bit_count && (step_index + 31U) < ENCODED_DATA_MAX; i++)
    {
        bool current_bit = bit_get(encoded, i);
        if (current_bit != last_bit)
            polarity = !polarity;
        last_bit = current_bit;

        for (uint8_t pulse = 0; pulse < 16U; pulse++)
        {
            wave_step(&step_index, polarity, 8);
            wave_step(&step_index, !polarity, 8);
        }
    }

    lfrfid_encoded_data.length = step_index;
}

static bool fsk_feed(extra_decoder_state_t *st, uint8_t encoded_size, uint8_t *out, bool (*can_decode)(uint8_t *), void (*decode)(uint8_t *, uint8_t *), const lfrfid_evt_t *evt, int size)
{
    for (int i = 0; i < size; i++)
    {
        bool value;
        uint32_t count;
        extra_fsk_demod_feed(&st->demod, evt[i].edge ? true : false, evt[i].t_us, &value, &count);
        for (uint32_t n = 0; n < count; n++)
        {
            bit_push(st->encoded, encoded_size, value);
            if (can_decode(st->encoded))
            {
                memset(out, 0, sizeof(lfrfid_tag_info.uid));
                decode(st->encoded, out);
                return true;
            }
        }
    }
    return false;
}

static void direct_begin(direct_decoder_state_t *st, uint8_t encoded_size)
{
    memset(st, 0, sizeof(*st));
    st->encoded_size = encoded_size;
}

static bool direct_push_counts(direct_decoder_state_t *st, bool level, uint32_t duration, uint16_t us_per_bit, bool (*can_decode)(uint8_t *), void (*decode)(uint8_t *, uint8_t *))
{
    uint32_t count = (duration + (us_per_bit / 2U)) / us_per_bit;
    if (count == 0U || count >= 128U) return false;

    for (uint32_t i = 0; i < count; i++)
    {
        bit_push(st->encoded, st->encoded_size, level);
        if (can_decode(st->encoded))
        {
            decode(st->encoded, st->data);
            return true;
        }

        bit_push(st->encoded_inv, st->encoded_size, !level);
        if (can_decode(st->encoded_inv))
        {
            decode(st->encoded_inv, st->data);
            return true;
        }
    }

    return false;
}

static bool direct_push_skewed(direct_decoder_state_t *st, bool level, uint32_t duration, uint16_t us_per_bit, bool (*can_decode)(uint8_t *), void (*decode)(uint8_t *, uint8_t *))
{
    if (direct_push_counts(st, level, duration, us_per_bit, can_decode, decode))
        return true;

    if (duration <= (us_per_bit / 4U)) return false;
    uint32_t adjusted = level ? (duration + 120U) : ((duration > 120U) ? (duration - 120U) : duration);
    uint32_t count = (adjusted + (us_per_bit / 2U)) / us_per_bit;
    if (count == 0U || count >= 128U) return false;

    for (uint32_t i = 0; i < count; i++)
    {
        bit_push(st->encoded_skew, st->encoded_size, level);
        if (can_decode(st->encoded_skew))
        {
            decode(st->encoded_skew, st->data);
            return true;
        }

        bit_push(st->encoded_skew_inv, st->encoded_size, !level);
        if (can_decode(st->encoded_skew_inv))
        {
            decode(st->encoded_skew_inv, st->data);
            return true;
        }
    }

    return false;
}

static bool hid_can_decode(uint8_t *data)
{
    if (data[0] != HID_PREAMBLE || data[HID_PREAMBLE_SIZE + HID_DATA_SIZE] != HID_PREAMBLE)
        return false;
    for (size_t i = HID_PREAMBLE_SIZE; i < (HID_PREAMBLE_SIZE + HID_DATA_SIZE); i++)
    {
        for (size_t n = 0; n < 4; n++)
        {
            uint8_t pair = (data[i] >> (n * 2U)) & 0x03U;
            if (pair == 0x00U || pair == 0x03U) return false;
        }
    }
    return true;
}

static void hid_decode(uint8_t *from, uint8_t *to)
{
    size_t bit_index = 0;
    for (size_t i = HID_PREAMBLE_SIZE; i < (HID_PREAMBLE_SIZE + HID_DATA_SIZE); i++)
    {
        for (size_t n = 0; n < 4; n++)
        {
            uint8_t pair = (from[i] >> (6U - (n * 2U))) & 0x03U;
            bit_set(to, bit_index++, pair == 0x02U);
        }
    }
}

static bool hid_ex_can_decode(uint8_t *data)
{
    if (data[0] != HID_PREAMBLE || data[HID_PREAMBLE_SIZE + HID_EX_DATA_SIZE] != HID_PREAMBLE)
        return false;
    for (size_t i = HID_PREAMBLE_SIZE; i < (HID_PREAMBLE_SIZE + HID_EX_DATA_SIZE); i++)
    {
        for (size_t n = 0; n < 4; n++)
        {
            uint8_t pair = (data[i] >> (n * 2U)) & 0x03U;
            if (pair == 0x00U || pair == 0x03U) return false;
        }
    }
    return true;
}

static void hid_ex_decode(uint8_t *from, uint8_t *to)
{
    size_t bit_index = 0;
    for (size_t i = HID_PREAMBLE_SIZE; i < (HID_PREAMBLE_SIZE + HID_EX_DATA_SIZE); i++)
    {
        for (size_t n = 0; n < 4; n++)
        {
            uint8_t pair = (from[i] >> (6U - (n * 2U))) & 0x03U;
            bit_set(to, bit_index++, pair == 0x02U);
        }
    }
}

static uint8_t hid_protocol_size(const uint8_t *data)
{
    for (size_t i = 0; i < 6; i++)
        if (bit_get(data, i)) return HID_DECODED_BIT_SIZE - i - 1U;
    if (!bit_get(data, 6)) return 37;

    size_t bit_index = 7;
    uint8_t size = 36;
    while (!bit_get(data, bit_index) && size >= 26)
    {
        size--;
        bit_index++;
    }
    return (size < 26) ? 0 : size;
}

static void hid_encode(const uint8_t *data, uint8_t *encoded, uint16_t decoded_bits)
{
    memset(encoded, 0, HID_EX_ENCODED_DATA_SIZE);
    encoded[0] = HID_PREAMBLE;
    size_t bit_index = 0;
    for (uint16_t i = 0; i < decoded_bits; i++)
    {
        bool bit = bit_get(data, i);
        bit_set(encoded, 8U + bit_index, bit);
        bit_set(encoded, 8U + bit_index + 1U, !bit);
        bit_index += 2U;
    }
}

static uint8_t ioprox_checksum(const uint8_t *data)
{
    uint8_t sum = 0;
    for (size_t i = 1; i <= 5; i++)
        sum += bit_get_bits(data, 9U * i, 8);
    return (uint8_t)(0xFFU - sum);
}

static bool ioprox_can_decode(uint8_t *data)
{
    if (data[0] != 0x00U) return false;
    if ((data[1] >> 6U) != 0x01U) return false;
    if (!bit_get(data, 17) || !bit_get(data, 26) || !bit_get(data, 35) ||
        !bit_get(data, 44) || !bit_get(data, 53) || !bit_get(data, 62) ||
        !bit_get(data, 63))
        return false;
    return ioprox_checksum(data) == bit_get_bits(data, 54, 8);
}

static void ioprox_decode(uint8_t *encoded, uint8_t *decoded)
{
    decoded[0] = bit_get_bits(encoded, 18, 8);
    decoded[1] = bit_get_bits(encoded, 27, 8);
    decoded[2] = bit_get_bits(encoded, 36, 8);
    decoded[3] = bit_get_bits(encoded, 45, 8);
}

static void ioprox_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, IOPROX_ENCODED_DATA_SIZE);
    bit_set_bits(encoded, 9, 0xF0, 8);
    bit_set(encoded, 17, true);
    bit_set_bits(encoded, 18, decoded[0], 8);
    bit_set(encoded, 26, true);
    bit_set_bits(encoded, 27, decoded[1], 8);
    bit_set(encoded, 35, true);
    bit_set_bits(encoded, 36, decoded[2], 8);
    bit_set(encoded, 44, true);
    bit_set_bits(encoded, 45, decoded[3], 8);
    bit_set(encoded, 53, true);
    bit_set_bits(encoded, 54, ioprox_checksum(encoded), 8);
    bit_set(encoded, 62, true);
    bit_set(encoded, 63, true);
}

static bool awid_can_decode(uint8_t *data)
{
    if (data[0] != 0x01U || data[AWID_ENCODED_LAST] != 0x01U) return false;
    if (bit_test_parity(data, 8, 88, ExtraParityOdd, 4)) return false;
    bit_remove_every_nth(data, 8, 88, 4);
    uint8_t len = bit_get_bits(data, 8, 8);
    return len == 26 || len == 50 || len == 37 || len == 34 || len == 36;
}

static void awid_decode(uint8_t *encoded, uint8_t *decoded)
{
    bit_copy(decoded, 0, 66, encoded, 8);
}

static void awid_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, AWID_ENCODED_DATA_SIZE);
    bit_set_bits(encoded, 0, 0x01, 8);
    for (size_t i = 0; i < 22; i++)
    {
        uint8_t value = bit_get_bits(decoded, i * 3U, 3) << 1U;
        value |= bit_parity32(value, ExtraParityOdd) ? 1U : 0U;
        bit_set_bits(encoded, 8U + i * 4U, value, 4);
    }
}

static bool fdxa_decode_payload(const uint8_t *from, uint8_t *to)
{
    size_t bit_index = 0;
    for (size_t i = FDXA_PREAMBLE_SIZE; i < (FDXA_PREAMBLE_SIZE + FDXA_DATA_SIZE); i++)
    {
        for (size_t n = 0; n < 4; n++)
        {
            uint8_t pair = (from[i] >> (6U - (n * 2U))) & 0x03U;
            if (pair == 0x01U) bit_set(to, bit_index++, false);
            else if (pair == 0x02U) bit_set(to, bit_index++, true);
            else return false;
        }
    }
    return true;
}

static bool fdxa_can_decode(uint8_t *data)
{
    if (data[0] != FDXA_PREAMBLE_0 || data[1] != FDXA_PREAMBLE_1 ||
        data[12] != FDXA_PREAMBLE_0 || data[13] != FDXA_PREAMBLE_1)
        return false;

    uint8_t decoded[FDXA_DECODED_DATA_SIZE] = {0};
    if (!fdxa_decode_payload(data, decoded)) return false;
    for (size_t i = 0; i < FDXA_DECODED_DATA_SIZE; i++)
        if (bit_parity32(decoded[i], ExtraParityOdd)) return false;
    return true;
}

static void fdxa_decode(uint8_t *encoded, uint8_t *decoded)
{
    (void)fdxa_decode_payload(encoded, decoded);
}

static void fdxa_fix_parity(uint8_t *data)
{
    for (size_t i = 0; i < FDXA_DECODED_DATA_SIZE; i++)
        if (bit_parity32(data[i], ExtraParityOdd)) data[i] ^= 0x80U;
}

static void fdxa_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, FDXA_ENCODED_DATA_SIZE);
    encoded[0] = FDXA_PREAMBLE_0;
    encoded[1] = FDXA_PREAMBLE_1;
    size_t bit_index = 0;
    for (size_t i = 0; i < FDXA_DECODED_BIT_SIZE; i++)
    {
        bool bit = bit_get(decoded, i);
        bit_set(encoded, 16U + bit_index, bit);
        bit_set(encoded, 16U + bit_index + 1U, !bit);
        bit_index += 2U;
    }
}

static bool pyramid_can_decode(uint8_t *data)
{
    if (bit_get_bits32(data, 0, 16) != 0x0001U || bit_get_bits(data, 16, 8) != 0x01U)
        return false;
    if (bit_get_bits32(data, 128, 16) != 0x0001U || bit_get_bits(data, 136, 8) != 0x01U)
        return false;

    uint8_t checksum = bit_get_bits(data, 120, 8);
    uint8_t checksum_data[13] = {0};
    for (uint8_t i = 0; i < 13U; i++)
        checksum_data[i] = bit_get_bits(data, 16U + i * 8U, 8);
    if (checksum != bit_crc8_reflected(checksum_data, 13, 0x8CU, 0x00U))
        return false;

    bit_remove_every_nth(data, 8, 15U * 8U, 8);
    uint8_t fmt_len = 0;
    for (uint8_t i = 0; i < 105U; i++)
    {
        if (bit_get(data, i))
        {
            fmt_len = 105U - i;
            break;
        }
    }
    return fmt_len == 26U;
}

static void pyramid_decode(uint8_t *encoded, uint8_t *decoded)
{
    memset(decoded, 0, PYRAMID_DECODED_DATA_SIZE);
    bit_set_bits(decoded, 0, 26, 8);
    bit_copy(decoded, 8, 8, encoded, 81);
    bit_copy(decoded, 16, 16, encoded, 89);
}

static bool paradox_can_decode(uint8_t *data)
{
    if (data[0] != 0x0FU || data[PARADOX_ENCODED_LAST] != 0x0FU)
        return false;
    for (uint32_t i = PARADOX_PREAMBLE_LENGTH; i < 96U; i += 2U)
        if (bit_get(data, i) == bit_get(data, i + 1U))
            return false;
    return true;
}

static void paradox_decode(uint8_t *encoded, uint8_t *decoded)
{
    memset(decoded, 0, PARADOX_DECODED_DATA_SIZE);
    for (uint32_t i = PARADOX_PREAMBLE_LENGTH; i < 96U; i += 2U)
        bit_push(decoded, PARADOX_DECODED_DATA_SIZE, bit_get_bits(encoded, i, 2) == 0x02U);
    for (uint8_t i = 0; i < 4U; i++)
        bit_push(decoded, PARADOX_DECODED_DATA_SIZE, false);
}

static bool indala26_preamble(const uint8_t *data, size_t bit_index)
{
    return bit_get_bits32(data, bit_index, 32) == 0xA0000000UL && bit_get(data, bit_index + 32U);
}

static bool indala26_can_decode(uint8_t *data)
{
    return indala26_preamble(data, 0) && indala26_preamble(data, 64) &&
           !bit_get(data, 60) && !bit_get(data, 61);
}

static void indala26_decode(uint8_t *encoded, uint8_t *decoded)
{
    memset(decoded, 0, INDALA26_DECODED_DATA_SIZE);
    bit_copy(decoded, 0, 22, encoded, 33);
    bit_copy(decoded, 22, 5, encoded, 55);
    bit_copy(decoded, 27, 2, encoded, 62);
}

static bool idteck_can_decode(uint8_t *data)
{
    return bit_get_bits32(data, 0, 32) == 0x4944544BUL;
}

static void idteck_decode(uint8_t *encoded, uint8_t *decoded)
{
    memcpy(decoded, encoded, IDTECK_DECODED_DATA_SIZE);
}

static bool keri_can_decode(uint8_t *data)
{
    if (bit_get_bits32(data, 0, 32) != 0xE0000000UL) return false;
    if (bit_get(data, 32)) return false;
    if (bit_get_bits32(data, 64, 32) != 0xE0000000UL) return false;
    if (bit_get(data, 96)) return false;
    return true;
}

static void keri_decode(uint8_t *encoded, uint8_t *decoded)
{
    memset(decoded, 0, KERI_DECODED_DATA_SIZE);
    bit_copy(decoded, 0, 32, encoded, 32);
}

static uint8_t nexwatch_parity_swap(uint8_t parity)
{
    return (uint8_t)(((parity >> 3U) & 1U) |
                     (((parity >> 1U) & 1U) << 1U) |
                     (((parity >> 2U) & 1U) << 2U) |
                     ((parity & 1U) << 3U));
}

static uint8_t nexwatch_parity(const uint8_t hexid[5])
{
    uint8_t p = 0;
    for (uint8_t i = 0; i < 5U; i++)
    {
        p ^= (hexid[i] & 0xF0U) >> 4U;
        p ^= hexid[i] & 0x0FU;
    }
    return nexwatch_parity_swap(p);
}

static bool nexwatch_can_decode(uint8_t *data)
{
    if (data[0] != 0x56U) return false;
    if (bit_get_bits32(data, 8, 32) != 0) return false;

    uint8_t hex[5] = {0};
    for (uint8_t i = 0; i < 5U; i++)
        hex[i] = bit_get_bits(data, 40U + i * 8U, 8);
    hex[4] &= 0xF0U;
    return nexwatch_parity(hex) == bit_get_bits(data, 76, 4);
}

static void nexwatch_decode(uint8_t *encoded, uint8_t *decoded)
{
    memset(decoded, 0, NEXWATCH_DECODED_DATA_SIZE);
    uint32_t id = bit_get_bits32(encoded, 40, 32);
    decoded[4] = (uint8_t)id;
    decoded[3] = (uint8_t)(id >> 8);
    decoded[2] = (uint8_t)(id >> 16);
    decoded[1] = (uint8_t)(id >> 24);
    decoded[0] = 0;
    uint32_t check = bit_get_bits32(encoded, 72, 24);
    decoded[7] = (uint8_t)check;
    decoded[6] = (uint8_t)(check >> 8);
    decoded[5] = (uint8_t)(check >> 16);
}

static uint8_t jablotron_checksum(uint8_t *bits)
{
    uint8_t checksum = 0;
    for (uint8_t i = 16; i < 56; i += 8)
        checksum += bit_get_bits(bits, i, 8);
    return checksum ^ 0x3AU;
}

static bool jablotron_can_decode(uint8_t *data)
{
    if (bit_get_bits32(data, 0, 16) != 0xFFFFU || bit_get_bits32(data, 64, 16) != 0xFFFFU)
        return false;
    return bit_get_bits(data, 56, 8) == jablotron_checksum(data);
}

static void jablotron_decode(uint8_t *encoded, uint8_t *decoded)
{
    memset(decoded, 0, JABLOTRON_DECODED_DATA_SIZE);
    bit_copy(decoded, 0, 40, encoded, 16);
}

static bool viking_can_decode(uint8_t *data)
{
    if (bit_get_bits32(data, 0, 24) != 0xF20000UL || bit_get_bits32(data, 64, 24) != 0xF20000UL)
        return false;
    uint8_t checksum = bit_get_bits(data, 0, 8) ^ bit_get_bits(data, 8, 8) ^
                       bit_get_bits(data, 16, 8) ^ bit_get_bits(data, 24, 8) ^
                       bit_get_bits(data, 32, 8) ^ bit_get_bits(data, 40, 8) ^
                       bit_get_bits(data, 48, 8) ^ bit_get_bits(data, 56, 8) ^ 0xA8U;
    return checksum == 0;
}

static void viking_decode(uint8_t *encoded, uint8_t *decoded)
{
    memset(decoded, 0, VIKING_DECODED_DATA_SIZE);
    bit_copy(decoded, 0, 32, encoded, 24);
}

static bool wiegand_parity(const uint8_t *bits, uint8_t pos, uint8_t type, uint8_t length)
{
    uint8_t count = 0;
    for (uint8_t i = 0; i < length; i++)
        if (bit_get(bits, pos + i)) count++;
    return (count & 1U) ^ type;
}

static void pyramid_add_wiegand_parity(uint8_t *target, uint8_t target_pos, uint8_t *source, uint8_t length)
{
    bit_set(target, target_pos, wiegand_parity(source, 0, 0, length / 2U));
    bit_copy(target, target_pos + 1U, length, source, 0);
    bit_set(target, target_pos + length + 1U, wiegand_parity(source, length / 2U, 1, length / 2U));
}

static void pyramid_encode(const uint8_t *decoded, uint8_t *encoded)
{
    uint8_t pre[16] = {0};
    uint8_t wiegand[3] = {0};
    memset(encoded, 0, PYRAMID_ENCODED_DATA_SIZE);

    bit_set(pre, 79, true);
    bit_copy(wiegand, 0, 8, decoded, 8);
    bit_copy(wiegand, 8, 16, decoded, 16);
    pyramid_add_wiegand_parity(pre, 80, wiegand, 24);
    bit_add_parity(pre, 8, encoded, 8, 102, 8, ExtraParityOdd);

    uint8_t checksum_buffer[13] = {0};
    for (uint8_t i = 0; i < 13U; i++)
        checksum_buffer[i] = bit_get_bits(encoded, 16U + i * 8U, 8);
    bit_set_bits(encoded, 120, bit_crc8_reflected(checksum_buffer, 13, 0x8CU, 0x00U), 8);
}

static void paradox_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, PARADOX_ENCODED_DATA_SIZE);
    bit_set_bits(encoded, 0, 0x0F, 8);
    for (size_t i = 0; i < 44U; i++)
        bit_set_bits(encoded, PARADOX_PREAMBLE_LENGTH + i * 2U, bit_get(decoded, i) ? 0x02 : 0x01, 2);
}

static void indala26_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, INDALA26_ENCODED_DATA_SIZE);
    bit_set_bits(encoded, 0, 0xA0, 8);
    bit_set(encoded, 32, true);
    bit_copy(encoded, 33, 22, decoded, 0);
    bit_copy(encoded, 55, 5, decoded, 22);
    bit_copy(encoded, 62, 2, decoded, 27);
}

static void idteck_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, IDTECK_ENCODED_DATA_SIZE);
    bit_set_bits(encoded, 0, 0x49, 8);
    bit_set_bits(encoded, 8, 0x44, 8);
    bit_set_bits(encoded, 16, 0x54, 8);
    bit_set_bits(encoded, 24, 0x4B, 8);
    bit_copy(encoded, 32, 32, decoded, 32);
}

static void keri_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, KERI_ENCODED_DATA_SIZE);
    bit_set_bits(encoded, 0, 0xE0, 8);
    bit_copy(encoded, 32, 32, decoded, 0);
    bit_set(encoded, 32, true);
}

static void nexwatch_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, NEXWATCH_ENCODED_DATA_SIZE);
    bit_set_bits(encoded, 0, 0x56, 8);
    bit_copy(encoded, 32, 32, decoded, 0);
    bit_copy(encoded, 64, 32, decoded, 32);
}

static void jablotron_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, JABLOTRON_ENCODED_DATA_SIZE);
    bit_set_bits(encoded, 0, 0xFF, 8);
    bit_set_bits(encoded, 8, 0xFF, 8);
    bit_copy(encoded, 16, 40, decoded, 0);
    bit_set_bits(encoded, 56, jablotron_checksum(encoded), 8);
}

static void viking_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, VIKING_ENCODED_DATA_SIZE);
    bit_set_bits(encoded, 0, 0xF2, 8);
    bit_copy(encoded, 24, 32, decoded, 0);
    uint32_t id = bit_get_bits32(decoded, 0, 32);
    uint8_t checksum = (uint8_t)(((id >> 24) & 0xFFU) ^ ((id >> 16) & 0xFFU) ^
                                 ((id >> 8) & 0xFFU) ^ (id & 0xFFU) ^ 0xF2U ^ 0xA8U);
    bit_set_bits(encoded, 56, checksum, 8);
}

static bool pac_stanley_can_decode(uint8_t *encoded)
{
    if (bit_get_bits(encoded, 0, 8) != 0xFFU) return false;
    if (bit_get(encoded, 8) || bit_get(encoded, 9) || !bit_get(encoded, 10)) return false;
    if (bit_get_bits(encoded, 11, 8) != 0x02U) return false;
    if (bit_get_bits(encoded, 128, 8) != 0xFFU) return false;

    uint8_t checksum = 0;
    uint8_t stripped = 0;
    for (uint8_t idx = 0; idx < 9U; idx++)
    {
        uint8_t byte = bit_reverse8(bit_get_bits(encoded, PAC_STANLEY_DATA_START_INDEX + PAC_STANLEY_BYTE_LENGTH * idx, 8));
        stripped = byte & 0x7FU;
        bool odd = (__builtin_parity(stripped) != 0);
        if (odd != ((byte & 0x80U) != 0)) return false;
        if (idx < 8U) checksum ^= stripped;
    }
    return stripped == checksum;
}

static void pac_stanley_decode(uint8_t *encoded, uint8_t *decoded)
{
    uint8_t ascii[8];
    for (uint8_t i = 0; i < 8U; i++)
        ascii[i] = bit_reverse8(bit_get_bits(encoded, PAC_STANLEY_DATA_START_INDEX + PAC_STANLEY_BYTE_LENGTH * i, 8)) & 0x7FU;
    hex_chars_to_bytes(ascii, decoded, PAC_STANLEY_DECODED_DATA_SIZE);
}

static void pac_stanley_encode(const uint8_t *decoded, uint8_t *encoded)
{
    uint8_t idbytes[10];
    memset(encoded, 0, PAC_STANLEY_ENCODED_DATA_SIZE);
    idbytes[0] = '2';
    idbytes[1] = '0';
    bytes_to_hex_chars(decoded, &idbytes[2], 4);

    for (size_t i = 0; i < 16U; i++)
        encoded[i] = (uint8_t)(0x40U >> (((i + 3U) % 5U) * 2U));
    encoded[0] = 0xFFU;
    encoded[1] = 0x20U;

    uint8_t checksum = 0;
    for (size_t i = 2; i < 13U; i++)
    {
        uint8_t shift = (uint8_t)(7U - (((i + 3U) % 4U) * 2U));
        uint8_t index = (uint8_t)(i + ((i - 1U) / 4U));
        uint16_t pattern;
        if (i < 12U)
        {
            pattern = bit_reverse8(idbytes[i - 2U]);
            pattern |= (__builtin_parity(pattern) != 0);
            if (i > 3U) checksum ^= idbytes[i - 2U];
        }
        else
        {
            pattern = (uint16_t)((bit_reverse8(checksum) & 0xFEU) | (__builtin_parity(checksum) != 0));
        }
        pattern = (uint16_t)(pattern << shift);
        encoded[index] |= (uint8_t)((pattern >> 8) & 0xFFU);
        encoded[index + 1U] |= (uint8_t)(pattern & 0xFFU);
    }
}

static uint8_t noralsy_checksum(uint8_t *bits, uint8_t bit_pos, uint8_t len)
{
    uint8_t sum = 0;
    for (uint8_t i = 0; i < len; i += 4U)
        sum ^= bit_get_bits(bits, bit_pos + i, 4);
    return sum & 0x0FU;
}

static bool noralsy_can_decode(uint8_t *encoded)
{
    if (bit_get_bits32(encoded, 0, 12) != 0xBB0U) return false;
    return noralsy_checksum(encoded, 32, 40) == bit_get_bits(encoded, 72, 4) &&
           noralsy_checksum(encoded, 0, 76) == bit_get_bits(encoded, 76, 4);
}

static void noralsy_decode(uint8_t *encoded, uint8_t *decoded)
{
    bit_copy(decoded, 0, NORALSY_ENCODED_BIT_SIZE, encoded, 0);
}

static void noralsy_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, NORALSY_ENCODED_DATA_SIZE);
    bit_copy(encoded, 0, NORALSY_ENCODED_BIT_SIZE, decoded, 0);
}

static bool fdxb_can_decode(uint8_t *encoded)
{
    if (bit_get_bits32(encoded, 0, 11) != 0x400U || bit_get_bits32(encoded, 128, 11) != 0x400U)
        return false;
    if (bit_test_parity(encoded, 3, 13U * 9U, ExtraParityAlways1, 9)) return false;

    uint8_t crc_data[8] = {0};
    for (uint8_t i = 0; i < 8U; i++)
        bit_copy(crc_data, i * 8U, 8, encoded, 12U + 9U * i);
    uint16_t crc_calc = bit_crc16_ccitt(crc_data, 8);
    uint16_t crc_read = 0;
    bit_copy((uint8_t *)&crc_read, 8, 8, encoded, 84);
    bit_copy((uint8_t *)&crc_read, 0, 8, encoded, 93);
    return crc_calc == crc_read;
}

static void fdxb_decode(uint8_t *encoded, uint8_t *decoded)
{
    bit_remove_every_nth(encoded, 3, 14U * 9U, 9);
    for (uint8_t i = 0; i < 11U; i++)
        bit_push(encoded, FDXB_ENCODED_DATA_SIZE, false);
    memset(decoded, 0, FDXB_DECODED_DATA_SIZE);
    bit_copy(decoded, 0, 64, encoded, 0);
    bit_copy(decoded, 64, 24, encoded, 80);
}

static void fdxb_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, FDXB_ENCODED_DATA_SIZE);
    bit_set(encoded, 0, true);
    for (uint8_t i = 0; i < 13U; i++)
    {
        bit_set(encoded, 11U + 9U * i, true);
        if (i == 8U || i == 9U) continue;
        bit_copy(encoded, 12U + 9U * i, 8, decoded, (i < 8U) ? (i * 8U) : ((i - 2U) * 8U));
    }
    uint16_t crc = bit_crc16_ccitt(decoded, 8);
    bit_copy(encoded, 84, 8, (uint8_t *)&crc, 8);
    bit_copy(encoded, 93, 8, (uint8_t *)&crc, 0);
}

static bool gproxii_wiegand_check(uint64_t fc_card, bool even, bool odd, uint8_t card_len)
{
    uint8_t even_sum = 0;
    uint8_t odd_sum = 1;
    uint8_t split = (card_len == 36U) ? 17U : 12U;
    uint8_t total = (card_len == 36U) ? 34U : 24U;
    for (uint8_t i = split; i < total; i++)
        if ((fc_card >> i) & 1ULL) even_sum++;
    for (uint8_t i = 0; i < split; i++)
        if ((fc_card >> i) & 1ULL) odd_sum++;
    return (even_sum % 2U) == even && (odd_sum % 2U) == odd;
}

static bool gproxii_can_decode(uint8_t *encoded)
{
    if (bit_get_bits(encoded, 0, 6) != 0x3EU) return false;
    if (bit_test_parity(encoded, 6, GPROXII_ENCODED_BIT_SIZE, ExtraParityAlways0, 5)) return false;

    uint8_t decoded[12] = {0};
    bit_copy(decoded, 0, GPROXII_ENCODED_BIT_SIZE, encoded, 6);
    bit_remove_every_nth(decoded, 0, GPROXII_ENCODED_BIT_SIZE, 5);
    for (uint8_t i = 0; i < 9U; i++)
        decoded[i] = bit_reverse8(decoded[i]);
    for (uint8_t i = 1; i < 9U; i++)
        decoded[i] ^= decoded[0];

    uint8_t card_len = bit_get_bits(decoded, 8, 6);
    if (card_len == 26U)
        return gproxii_wiegand_check(bit_get_bits64(decoded, 33, 24), bit_get(decoded, 32), bit_get(decoded, 57), card_len);
    if (card_len == 36U)
        return gproxii_wiegand_check(bit_get_bits64(decoded, 33, 34), bit_get(decoded, 32), bit_get(decoded, 67), card_len);
    return false;
}

static void gproxii_decode(uint8_t *encoded, uint8_t *decoded)
{
    memcpy(decoded, encoded, GPROXII_DATA_SIZE);
}

static void gproxii_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, GPROXII_DATA_SIZE);
    memcpy(encoded, decoded, GPROXII_DATA_SIZE);
}

static bool securakey_can_decode(uint8_t *encoded)
{
    uint32_t prefix = bit_get_bits32(encoded, 0, 19);
    if (prefix == 0x3F800U)
        return !bit_test_parity(encoded, 2, 54, ExtraParityAlways0, 9);
    if (prefix == 0x3F8B4U || prefix == 0x3F8C0U)
        return !bit_test_parity(encoded, 2, 90, ExtraParityAlways0, 9);
    return false;
}

static void securakey_decode(uint8_t *encoded, uint8_t *decoded)
{
    memset(decoded, 0, SECURAKEY_DECODED_DATA_SIZE);
    uint8_t len = bit_get_bits(encoded, 13, 6);
    if (len == 0U)
    {
        bit_copy(decoded, 16, 8, encoded, 29);
        bit_copy(decoded, 24, 8, encoded, 38);
        bit_copy(decoded, 32, 8, encoded, 47);
        bit_copy(decoded, 40, 8, encoded, 56);
    }
    else
    {
        if (len == 26U)
        {
            bit_copy(decoded, 8, 1, encoded, 36);
            bit_copy(decoded, 9, 7, encoded, 38);
        }
        else
        {
            bit_copy(decoded, 2, 7, encoded, 30);
            bit_copy(decoded, 9, 7, encoded, 38);
        }
        bit_copy(decoded, 16, 1, encoded, 45);
        bit_copy(decoded, 17, 8, encoded, 47);
        bit_copy(decoded, 25, 7, encoded, 56);
        bit_copy(decoded, 32, 8, encoded, 65);
        bit_copy(decoded, 40, 8, encoded, 74);
    }
}

static uint16_t securakey_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, SECURAKEY_RKKT_SIZE);
    if (bit_get_bits32(decoded, 0, 16) == 0)
    {
        bit_set_bits(encoded, 0, 0x7F, 8);
        bit_set_bits(encoded, 8, 0x06, 3);
        bit_copy(encoded, 29, 8, decoded, 16);
        bit_copy(encoded, 38, 8, decoded, 24);
        bit_copy(encoded, 47, 8, decoded, 32);
        bit_copy(encoded, 56, 8, decoded, 40);
        return SECURAKEY_RKKTH_BITS;
    }

    bit_set_bits(encoded, 0, 0x7F, 8);
    bit_set_bits(encoded, 8, 0x19, 5);
    if (bit_get_bits(decoded, 0, 8) == 0)
    {
        bit_set_bits(encoded, 13, 26, 6);
        if (!bit_test_parity(decoded, 8, 12, ExtraParityOdd, 12)) bit_set(encoded, 35, true);
        if (bit_test_parity(decoded, 20, 12, ExtraParityOdd, 12)) bit_set(encoded, 63, true);
        bit_copy(encoded, 36, 1, decoded, 8);
        bit_copy(encoded, 38, 7, decoded, 9);
    }
    else
    {
        bit_set_bits(encoded, 13, 32, 6);
        if (!bit_test_parity(decoded, 2, 15, ExtraParityOdd, 15)) bit_set(encoded, 29, true);
        if (bit_test_parity(decoded, 17, 15, ExtraParityOdd, 15)) bit_set(encoded, 63, true);
        bit_copy(encoded, 30, 7, decoded, 2);
        bit_copy(encoded, 38, 7, decoded, 3);
    }
    bit_copy(encoded, 45, 1, decoded, 16);
    bit_copy(encoded, 47, 8, decoded, 17);
    bit_copy(encoded, 56, 7, decoded, 25);
    bit_copy(encoded, 65, 8, decoded, 32);
    bit_copy(encoded, 74, 8, decoded, 40);
    return SECURAKEY_RKKT_BITS;
}

static uint8_t crc8_msb(const uint8_t *data, uint8_t len, uint8_t poly, uint8_t init)
{
    uint8_t crc = init;
    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8U; b++)
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ poly) : (uint8_t)(crc << 1);
    }
    return crc;
}

static bool gallagher_can_decode(uint8_t *encoded)
{
    if (bit_get_bits32(encoded, 0, 16) != 0x7FEAU || bit_get_bits32(encoded, 96, 16) != 0x7FEAU)
        return false;
    uint8_t checksum[8] = {0};
    for (uint8_t i = 0; i < 8U; i++)
        checksum[i] = bit_get_bits(encoded, 16U + 9U * i, 8);
    return bit_get_bits(encoded, 88, 8) == crc8_msb(checksum, 8, 0x07U, 0x2CU);
}

static void gallagher_decode(uint8_t *encoded, uint8_t *decoded)
{
    memset(decoded, 0, GALLAGHER_ENCODED_DATA_SIZE);
    bit_copy(decoded, 0, GALLAGHER_ENCODED_BIT_SIZE, encoded, 0);
}

static void gallagher_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memset(encoded, 0, GALLAGHER_ROLLING_DATA_SIZE);
    bit_copy(encoded, 0, GALLAGHER_ENCODED_BIT_SIZE, decoded, 0);
}

static bool electra_base_valid(uint8_t *data)
{
    for (uint8_t i = 0; i < 9U; i++)
        if (!bit_get(data, i)) return false;
    if (bit_get(data, 63)) return false;

    for (uint8_t row = 0; row < 10U; row++)
    {
        uint8_t sum = 0;
        for (uint8_t j = 0; j < 5U; j++)
            sum += bit_get(data, 9U + row * 5U + j) ? 1U : 0U;
        if (sum & 1U) return false;
    }

    for (uint8_t col = 0; col < 4U; col++)
    {
        uint8_t sum = 0;
        for (uint8_t row = 0; row < 10U; row++)
            sum += bit_get(data, 9U + row * 5U + col) ? 1U : 0U;
        sum += bit_get(data, 59U + col) ? 1U : 0U;
        if (sum & 1U) return false;
    }

    return true;
}

static bool electra_can_decode(uint8_t *encoded)
{
    if (!electra_base_valid(encoded)) return false;
    bool epilogue_has_em_header = true;
    for (uint8_t i = 64U; i < 73U; i++)
        if (!bit_get(encoded, i)) epilogue_has_em_header = false;
    return !epilogue_has_em_header;
}

static void electra_decode(uint8_t *encoded, uint8_t *decoded)
{
    memcpy(decoded, encoded, ELECTRA_ENCODED_DATA_SIZE);
}

static void electra_encode(const uint8_t *decoded, uint8_t *encoded)
{
    memcpy(encoded, decoded, ELECTRA_ENCODED_DATA_SIZE);
}

static bool biphase_feed(manchester_decoder_state_t *st, const lfrfid_evt_t *evt, int size, uint8_t encoded_size, uint16_t short_low, uint16_t short_high, uint16_t long_low, uint16_t long_high, bool (*can_decode)(uint8_t *), void (*decode)(uint8_t *, uint8_t *))
{
    for (int i = 0; i < size; i++)
    {
        bool pushed = false;
        uint32_t duration = evt[i].t_us;
        if (duration >= short_low && duration <= short_high)
        {
            if (!st->last_short)
            {
                st->last_short = true;
            }
            else
            {
                pushed = true;
                bit_push(st->encoded, encoded_size, false);
                st->last_short = false;
            }
        }
        else if (duration >= long_low && duration <= long_high)
        {
            if (!st->last_short)
            {
                pushed = true;
                bit_push(st->encoded, encoded_size, true);
            }
            else
            {
                st->last_short = false;
            }
        }
        else
        {
            st->last_short = false;
        }

        if (pushed && can_decode(st->encoded))
        {
            decode(st->encoded, st->data);
            return true;
        }
    }
    return false;
}

static bool biphase_inverse_feed(manchester_decoder_state_t *st, const lfrfid_evt_t *evt, int size, uint8_t encoded_size, uint16_t short_low, uint16_t short_high, uint16_t long_low, uint16_t long_high, bool (*can_decode)(uint8_t *), void (*decode)(uint8_t *, uint8_t *))
{
    for (int i = 0; i < size; i++)
    {
        bool pushed = false;
        uint32_t duration = evt[i].t_us;
        if (duration >= short_low && duration <= short_high)
        {
            if (!st->last_short)
            {
                st->last_short = true;
            }
            else
            {
                pushed = true;
                bit_push(st->encoded, encoded_size, true);
                st->last_short = false;
            }
        }
        else if (duration >= long_low && duration <= long_high)
        {
            if (!st->last_short)
            {
                pushed = true;
                bit_push(st->encoded, encoded_size, false);
            }
            else
            {
                st->last_short = false;
            }
        }
        else
        {
            st->last_short = false;
        }

        if (pushed && can_decode(st->encoded))
        {
            decode(st->encoded, st->data);
            return true;
        }
    }
    return false;
}

static bool manchester_feed(manchester_decoder_state_t *st, const lfrfid_evt_t *evt, int size, uint8_t encoded_size, uint16_t short_low, uint16_t short_high, uint16_t long_low, uint16_t long_high, bool (*can_decode)(uint8_t *), void (*decode)(uint8_t *, uint8_t *))
{
    for (int i = 0; i < size; i++)
    {
        bool level = evt[i].edge ? true : false;
        uint32_t duration = evt[i].t_us;
        extra_manchester_event_t event = ManchesterEventReset;

        if (duration > short_low && duration < short_high)
            event = level ? ManchesterEventShortLow : ManchesterEventShortHigh;
        else if (duration > long_low && duration < long_high)
            event = level ? ManchesterEventLongLow : ManchesterEventLongHigh;

        if (event != ManchesterEventReset)
        {
            bool data;
            if (extra_manchester_advance(st->manchester, event, &st->manchester, &data))
            {
                bit_push(st->encoded, encoded_size, data);
                if (can_decode(st->encoded))
                {
                    decode(st->encoded, st->data);
                    return true;
                }
            }
        }
    }
    return false;
}

static void extra_begin_fsk_6_5(extra_decoder_state_t *st)
{
    memset(st->encoded, 0, sizeof(st->encoded));
    extra_fsk_demod_init(&st->demod, FSK_MIN_TIME, 6, FSK_MAX_TIME, 5);
}

static void extra_begin_ioprox(extra_decoder_state_t *st)
{
    memset(st->encoded, 0, sizeof(st->encoded));
    extra_fsk_demod_init(&st->demod, FSK_MIN_TIME, 8, FSK_MAX_TIME, 6);
}

static void protocol_hid_generic_begin(void *proto) { (void)proto; extra_begin_fsk_6_5(&s_hid); }
static void protocol_hid_ex_generic_begin(void *proto) { (void)proto; extra_begin_fsk_6_5(&s_hid_ex); }
static void protocol_ioprox_begin(void *proto) { (void)proto; extra_begin_ioprox(&s_ioprox); }
static void protocol_awid_begin(void *proto) { (void)proto; extra_begin_fsk_6_5(&s_awid); }
static void protocol_fdxa_begin(void *proto) { (void)proto; extra_begin_fsk_6_5(&s_fdxa); }
static void protocol_pyramid_begin(void *proto) { (void)proto; extra_begin_fsk_6_5(&s_pyramid); }
static void protocol_paradox_begin(void *proto) { (void)proto; extra_begin_fsk_6_5(&s_paradox); }
static void protocol_indala26_begin(void *proto) { (void)proto; direct_begin(&s_indala26, INDALA26_ENCODED_DATA_SIZE); }
static void protocol_idteck_begin(void *proto) { (void)proto; direct_begin(&s_idteck, IDTECK_ENCODED_DATA_SIZE); }
static void protocol_keri_begin(void *proto) { (void)proto; direct_begin(&s_keri, KERI_ENCODED_DATA_SIZE); }
static void protocol_nexwatch_begin(void *proto) { (void)proto; direct_begin(&s_nexwatch, NEXWATCH_ENCODED_DATA_SIZE); }
static void protocol_jablotron_begin(void *proto)
{
    (void)proto;
    memset(&s_jablotron, 0, sizeof(s_jablotron));
}
static void protocol_viking_begin(void *proto)
{
    (void)proto;
    memset(&s_viking, 0, sizeof(s_viking));
    s_viking.manchester = ManchesterStateMid1;
}
static void protocol_pac_stanley_begin(void *proto) { (void)proto; direct_begin(&s_pac_stanley, PAC_STANLEY_ENCODED_DATA_SIZE); }
static void protocol_noralsy_begin(void *proto)
{
    (void)proto;
    memset(&s_noralsy, 0, sizeof(s_noralsy));
    s_noralsy.manchester = ManchesterStateMid1;
}
static void protocol_fdxb_begin(void *proto)
{
    (void)proto;
    memset(&s_fdxb, 0, sizeof(s_fdxb));
}
static void protocol_gproxii_begin(void *proto)
{
    (void)proto;
    memset(&s_gproxii, 0, sizeof(s_gproxii));
}
static void protocol_securakey_begin(void *proto)
{
    (void)proto;
    memset(&s_securakey, 0, sizeof(s_securakey));
    s_securakey.manchester = ManchesterStateMid1;
}
static void protocol_gallagher_begin(void *proto)
{
    (void)proto;
    memset(&s_gallagher, 0, sizeof(s_gallagher));
    s_gallagher.manchester = ManchesterStateMid1;
}
static void protocol_electra_begin(void *proto)
{
    (void)proto;
    memset(&s_electra, 0, sizeof(s_electra));
    s_electra.manchester = ManchesterStateMid1;
}

static bool protocol_hid_generic_execute(void *proto, int size)
{
    return fsk_feed(&s_hid, HID_ENCODED_DATA_SIZE, lfrfid_tag_info.uid, hid_can_decode, hid_decode, proto, size);
}

static bool protocol_hid_ex_generic_execute(void *proto, int size)
{
    return fsk_feed(&s_hid_ex, HID_EX_ENCODED_DATA_SIZE, lfrfid_tag_info.uid, hid_ex_can_decode, hid_ex_decode, proto, size);
}

static bool protocol_ioprox_execute(void *proto, int size)
{
    return fsk_feed(&s_ioprox, IOPROX_ENCODED_DATA_SIZE, lfrfid_tag_info.uid, ioprox_can_decode, ioprox_decode, proto, size);
}

static bool protocol_awid_execute(void *proto, int size)
{
    return fsk_feed(&s_awid, AWID_ENCODED_DATA_SIZE, lfrfid_tag_info.uid, awid_can_decode, awid_decode, proto, size);
}

static bool protocol_fdxa_execute(void *proto, int size)
{
    return fsk_feed(&s_fdxa, FDXA_ENCODED_DATA_SIZE, lfrfid_tag_info.uid, fdxa_can_decode, fdxa_decode, proto, size);
}

static bool protocol_pyramid_execute(void *proto, int size)
{
    return fsk_feed(&s_pyramid, PYRAMID_ENCODED_DATA_SIZE, lfrfid_tag_info.uid, pyramid_can_decode, pyramid_decode, proto, size);
}

static bool protocol_paradox_execute(void *proto, int size)
{
    return fsk_feed(&s_paradox, PARADOX_ENCODED_DATA_SIZE, lfrfid_tag_info.uid, paradox_can_decode, paradox_decode, proto, size);
}

static bool protocol_indala26_execute(void *proto, int size)
{
    const lfrfid_evt_t *evt = (const lfrfid_evt_t *)proto;
    for (int i = 0; i < size; i++)
        if (evt[i].t_us > (INDALA26_US_PER_BIT / 4U) &&
            direct_push_skewed(&s_indala26, evt[i].edge ? true : false, evt[i].t_us, INDALA26_US_PER_BIT, indala26_can_decode, indala26_decode))
        {
            memcpy(lfrfid_tag_info.uid, s_indala26.data, INDALA26_DECODED_DATA_SIZE);
            return true;
        }
    return false;
}

static bool protocol_idteck_execute(void *proto, int size)
{
    const lfrfid_evt_t *evt = (const lfrfid_evt_t *)proto;
    for (int i = 0; i < size; i++)
        if (evt[i].t_us > (IDTECK_US_PER_BIT / 4U) &&
            direct_push_skewed(&s_idteck, evt[i].edge ? true : false, evt[i].t_us, IDTECK_US_PER_BIT, idteck_can_decode, idteck_decode))
        {
            memcpy(lfrfid_tag_info.uid, s_idteck.data, IDTECK_DECODED_DATA_SIZE);
            return true;
        }
    return false;
}

static bool protocol_keri_execute(void *proto, int size)
{
    const lfrfid_evt_t *evt = (const lfrfid_evt_t *)proto;
    for (int i = 0; i < size; i++)
        if (evt[i].t_us > (KERI_US_PER_BIT / 4U) &&
            direct_push_skewed(&s_keri, evt[i].edge ? true : false, evt[i].t_us, KERI_US_PER_BIT, keri_can_decode, keri_decode))
        {
            memcpy(lfrfid_tag_info.uid, s_keri.data, KERI_DECODED_DATA_SIZE);
            return true;
        }
    return false;
}

static bool protocol_nexwatch_execute(void *proto, int size)
{
    const lfrfid_evt_t *evt = (const lfrfid_evt_t *)proto;
    for (int i = 0; i < size; i++)
        if (evt[i].t_us > (NEXWATCH_US_PER_BIT / 4U) &&
            direct_push_skewed(&s_nexwatch, evt[i].edge ? true : false, evt[i].t_us, NEXWATCH_US_PER_BIT, nexwatch_can_decode, nexwatch_decode))
        {
            memcpy(lfrfid_tag_info.uid, s_nexwatch.data, NEXWATCH_DECODED_DATA_SIZE);
            return true;
        }
    return false;
}

static bool protocol_jablotron_execute(void *proto, int size)
{
    if (biphase_feed(&s_jablotron, proto, size, JABLOTRON_ENCODED_DATA_SIZE, JABLOTRON_SHORT_TIME_LOW, JABLOTRON_SHORT_TIME_HIGH, JABLOTRON_LONG_TIME_LOW, JABLOTRON_LONG_TIME_HIGH, jablotron_can_decode, jablotron_decode))
    {
        memcpy(lfrfid_tag_info.uid, s_jablotron.data, JABLOTRON_DECODED_DATA_SIZE);
        return true;
    }
    return false;
}

static bool protocol_viking_execute(void *proto, int size)
{
    if (manchester_feed(&s_viking, proto, size, VIKING_ENCODED_DATA_SIZE, VIKING_SHORT_TIME_LOW, VIKING_SHORT_TIME_HIGH, VIKING_LONG_TIME_LOW, VIKING_LONG_TIME_HIGH, viking_can_decode, viking_decode))
    {
        memcpy(lfrfid_tag_info.uid, s_viking.data, VIKING_DECODED_DATA_SIZE);
        return true;
    }
    return false;
}

static bool protocol_pac_stanley_execute(void *proto, int size)
{
    const lfrfid_evt_t *evt = (const lfrfid_evt_t *)proto;
    for (int i = 0; i < size; i++)
    {
        if (evt[i].t_us > PAC_STANLEY_MAX_TIME) continue;
        uint8_t pulses = (uint8_t)((evt[i].t_us + (PAC_STANLEY_CYCLE_LENGTH / 2U)) / PAC_STANLEY_CYCLE_LENGTH);
        bool level = evt[i].edge ? true : false;
        if (pulses >= 9U && !s_pac_stanley.got_preamble)
        {
            pulses = 8;
            s_pac_stanley.got_preamble = true;
            s_pac_stanley.inverted = !level;
        }
        else if (pulses >= 9U)
        {
            s_pac_stanley.got_preamble = false;
            continue;
        }
        else if (pulses == 0U && evt[i].t_us > PAC_STANLEY_MIN_TIME)
        {
            pulses = 1;
        }

        for (uint8_t n = 0; n < pulses; n++)
        {
            bit_push(s_pac_stanley.encoded, PAC_STANLEY_ENCODED_DATA_SIZE, level ^ s_pac_stanley.inverted);
            if (pac_stanley_can_decode(s_pac_stanley.encoded))
            {
                pac_stanley_decode(s_pac_stanley.encoded, lfrfid_tag_info.uid);
                return true;
            }
        }
    }
    return false;
}

static bool protocol_noralsy_execute(void *proto, int size)
{
    if (manchester_feed(&s_noralsy, proto, size, NORALSY_ENCODED_DATA_SIZE, NORALSY_SHORT_TIME_LOW, NORALSY_SHORT_TIME_HIGH, NORALSY_LONG_TIME_LOW, NORALSY_LONG_TIME_HIGH, noralsy_can_decode, noralsy_decode))
    {
        memcpy(lfrfid_tag_info.uid, s_noralsy.data, NORALSY_DECODED_DATA_SIZE);
        return true;
    }
    return false;
}

static bool protocol_fdxb_execute(void *proto, int size)
{
    if (biphase_feed(&s_fdxb, proto, size, FDXB_ENCODED_DATA_SIZE, FDXB_SHORT_TIME_LOW, FDXB_SHORT_TIME_HIGH, FDXB_LONG_TIME_LOW, FDXB_LONG_TIME_HIGH, fdxb_can_decode, fdxb_decode))
    {
        memcpy(lfrfid_tag_info.uid, s_fdxb.data, FDXB_DECODED_DATA_SIZE);
        return true;
    }
    return false;
}

static bool protocol_gproxii_execute(void *proto, int size)
{
    if (biphase_inverse_feed(&s_gproxii, proto, size, GPROXII_DATA_SIZE, GPROXII_SHORT_TIME_LOW, GPROXII_SHORT_TIME_HIGH, GPROXII_LONG_TIME_LOW, GPROXII_LONG_TIME_HIGH, gproxii_can_decode, gproxii_decode))
    {
        memcpy(lfrfid_tag_info.uid, s_gproxii.data, GPROXII_DATA_SIZE);
        return true;
    }
    return false;
}

static bool protocol_securakey_execute(void *proto, int size)
{
    if (manchester_feed(&s_securakey, proto, size, SECURAKEY_RKKT_SIZE, SECURAKEY_SHORT_TIME_LOW, SECURAKEY_SHORT_TIME_HIGH, SECURAKEY_LONG_TIME_LOW, SECURAKEY_LONG_TIME_HIGH, securakey_can_decode, securakey_decode))
    {
        memcpy(lfrfid_tag_info.uid, s_securakey.data, SECURAKEY_DECODED_DATA_SIZE);
        return true;
    }
    return false;
}

static bool protocol_gallagher_execute(void *proto, int size)
{
    if (manchester_feed(&s_gallagher, proto, size, GALLAGHER_ROLLING_DATA_SIZE, GALLAGHER_SHORT_TIME_LOW, GALLAGHER_SHORT_TIME_HIGH, GALLAGHER_LONG_TIME_LOW, GALLAGHER_LONG_TIME_HIGH, gallagher_can_decode, gallagher_decode))
    {
        memcpy(lfrfid_tag_info.uid, s_gallagher.data, GALLAGHER_ENCODED_DATA_SIZE);
        return true;
    }
    return false;
}

static bool protocol_electra_execute(void *proto, int size)
{
    if (manchester_feed(&s_electra, proto, size, ELECTRA_ENCODED_DATA_SIZE, ELECTRA_SHORT_TIME_LOW, ELECTRA_SHORT_TIME_HIGH, ELECTRA_LONG_TIME_LOW, ELECTRA_LONG_TIME_HIGH, electra_can_decode, electra_decode))
    {
        memcpy(lfrfid_tag_info.uid, s_electra.data, ELECTRA_ENCODED_DATA_SIZE);
        return true;
    }
    return false;
}

static uint8_t *extra_get_data(void *proto)
{
    (void)proto;
    return lfrfid_tag_info.uid;
}

static bool protocol_hid_generic_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[HID_ENCODED_DATA_SIZE];
    hid_encode(tag->uid, encoded, HID_DECODED_BIT_SIZE);
    fsk_wave_from_bits(encoded, HID_ENCODED_BIT_SIZE, 8, 10, 50);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_hid_ex_generic_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[HID_EX_ENCODED_DATA_SIZE];
    hid_encode(tag->uid, encoded, HID_EX_DECODED_BIT_SIZE);
    fsk_wave_from_bits(encoded, HID_EX_ENCODED_BIT_SIZE, 8, 10, 50);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_ioprox_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[IOPROX_ENCODED_DATA_SIZE];
    ioprox_encode(tag->uid, encoded);
    fsk_wave_from_bits(encoded, IOPROX_ENCODED_BIT_SIZE, 8, 10, 64);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_awid_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[AWID_ENCODED_DATA_SIZE];
    awid_encode(tag->uid, encoded);
    fsk_wave_from_bits(encoded, AWID_ENCODED_BIT_SIZE, 8, 10, 50);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_fdxa_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t fixed[FDXA_DECODED_DATA_SIZE];
    uint8_t encoded[FDXA_ENCODED_DATA_SIZE];
    memcpy(fixed, tag->uid, sizeof(fixed));
    fdxa_fix_parity(fixed);
    fdxa_encode(fixed, encoded);
    fsk_wave_from_bits(encoded, FDXA_ENCODED_BIT_SIZE, 8, 10, 50);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_pyramid_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[PYRAMID_ENCODED_DATA_SIZE];
    pyramid_encode(tag->uid, encoded);
    fsk_wave_from_bits(encoded, PYRAMID_ENCODED_BIT_SIZE, 8, 10, 50);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_indala26_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[INDALA26_ENCODED_DATA_SIZE];
    indala26_encode(tag->uid, encoded);
    psk_wave_from_bits(encoded, INDALA26_ENCODED_BIT_SIZE);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_idteck_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[IDTECK_ENCODED_DATA_SIZE];
    idteck_encode(tag->uid, encoded);
    psk_wave_from_bits(encoded, IDTECK_ENCODED_BIT_SIZE);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_keri_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[KERI_ENCODED_DATA_SIZE];
    tag->uid[0] |= 0x80U;
    keri_encode(tag->uid, encoded);
    psk_wave_from_bits(encoded, KERI_ENCODED_BIT_SIZE);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_nexwatch_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[NEXWATCH_ENCODED_DATA_SIZE];
    nexwatch_encode(tag->uid, encoded);
    psk_wave_from_bits(encoded, NEXWATCH_ENCODED_BIT_SIZE);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_jablotron_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[JABLOTRON_ENCODED_DATA_SIZE];
    jablotron_encode(tag->uid, encoded);
    biphase_wave_from_bits(encoded, JABLOTRON_ENCODED_BIT_SIZE, 256, 512);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_viking_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[VIKING_ENCODED_DATA_SIZE];
    viking_encode(tag->uid, encoded);
    manchester_wave_from_bits(encoded, VIKING_ENCODED_BIT_SIZE, 128);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_paradox_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[PARADOX_ENCODED_DATA_SIZE];
    paradox_encode(tag->uid, encoded);
    fsk_wave_from_bits(encoded, PARADOX_ENCODED_BIT_SIZE, 8, 10, 50);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_pac_stanley_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[PAC_STANLEY_ENCODED_DATA_SIZE];
    pac_stanley_encode(tag->uid, encoded);
    direct_wave_from_bits(encoded, PAC_STANLEY_ENCODED_BIT_SIZE, 32);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_noralsy_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[NORALSY_ENCODED_DATA_SIZE];
    noralsy_encode(tag->uid, encoded);
    manchester_wave_from_bits(encoded, NORALSY_ENCODED_BIT_SIZE, 16);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_fdxb_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[FDXB_ENCODED_DATA_SIZE];
    fdxb_encode(tag->uid, encoded);
    biphase_wave_from_bits(encoded, FDXB_ENCODED_BIT_SIZE, 16, 32);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_gproxii_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[GPROXII_DATA_SIZE];
    gproxii_encode(tag->uid, encoded);
    biphase_inverse_wave_from_bits(encoded, 96, 32, 64);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_securakey_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[SECURAKEY_RKKT_SIZE];
    uint16_t bits = securakey_encode(tag->uid, encoded);
    manchester_wave_from_bits(encoded, bits, 20);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_gallagher_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[GALLAGHER_ROLLING_DATA_SIZE];
    gallagher_encode(tag->uid, encoded);
    manchester_wave_from_bits(encoded, GALLAGHER_ENCODED_BIT_SIZE, 16);
    return lfrfid_encoded_data.length > 0;
}

static bool protocol_electra_encoder_begin(void *proto)
{
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)proto;
    uint8_t encoded[ELECTRA_ENCODED_DATA_SIZE];
    electra_encode(tag->uid, encoded);
    manchester_wave_from_bits(encoded, ELECTRA_ENCODED_BIT_SIZE, 32);
    return lfrfid_encoded_data.length > 0;
}

static void extra_encoder_send(void *proto)
{
    (void)proto;
    lfrfid_encoded_data.index = 0;
    lfrfid_emul_hw_init();
}

static bool protocol_hid_generic_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[HID_ENCODED_DATA_SIZE];
    hid_encode(tag->uid, encoded, HID_DECODED_BIT_SIZE);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_FSK2a | T5577_BITRATE_RF_50 | T5577_TRANS_BL_1_3;
    encoded_to_t5577(encoded, 3);
    lfrfid_program->t5577.max_blocks = 4;
    return true;
}

static bool protocol_hid_ex_generic_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[HID_EX_ENCODED_DATA_SIZE];
    hid_encode(tag->uid, encoded, HID_EX_DECODED_BIT_SIZE);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_FSK2a | T5577_BITRATE_RF_50 | T5577_TRANS_BL_1_6;
    encoded_to_t5577(encoded, 6);
    lfrfid_program->t5577.max_blocks = 7;
    return true;
}

static bool protocol_ioprox_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[IOPROX_ENCODED_DATA_SIZE];
    ioprox_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_FSK2a | T5577_BITRATE_RF_64 | T5577_TRANS_BL_1_2;
    encoded_to_t5577(encoded, 2);
    lfrfid_program->t5577.max_blocks = 3;
    return true;
}

static bool protocol_awid_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[AWID_ENCODED_DATA_SIZE];
    awid_encode(tag->uid, encoded);
    bit_remove_every_nth(encoded, 8, 88, 4);
    awid_decode(encoded, tag->uid);
    awid_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_FSK2a | T5577_BITRATE_RF_50 | T5577_TRANS_BL_1_3;
    encoded_to_t5577(encoded, 3);
    lfrfid_program->t5577.max_blocks = 4;
    return true;
}

static bool protocol_fdxa_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t fixed[FDXA_DECODED_DATA_SIZE];
    uint8_t encoded[FDXA_ENCODED_DATA_SIZE];
    memcpy(fixed, tag->uid, sizeof(fixed));
    fdxa_fix_parity(fixed);
    fdxa_encode(fixed, encoded);
    fdxa_decode(encoded, tag->uid);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_FSK2a | T5577_BITRATE_RF_50 | T5577_TRANS_BL_1_3;
    encoded_to_t5577(encoded, 3);
    lfrfid_program->t5577.max_blocks = 4;
    return true;
}

static bool protocol_pyramid_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[PYRAMID_ENCODED_DATA_SIZE];
    pyramid_encode(tag->uid, encoded);
    bit_remove_every_nth(encoded, 8, 15U * 8U, 8);
    pyramid_decode(encoded, tag->uid);
    pyramid_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_FSK2a | T5577_BITRATE_RF_50 | T5577_TRANS_BL_1_4;
    encoded_to_t5577(encoded, 4);
    lfrfid_program->t5577.max_blocks = 5;
    return true;
}

static bool protocol_indala26_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[INDALA26_ENCODED_DATA_SIZE];
    indala26_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_BITRATE_RF_32 | T5577_MOD_PSK1 | T5577_TRANS_BL_1_2;
    encoded_to_t5577(encoded, 2);
    lfrfid_program->t5577.max_blocks = 3;
    return true;
}

static bool protocol_idteck_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[IDTECK_ENCODED_DATA_SIZE];
    idteck_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_BITRATE_RF_32 | T5577_MOD_PSK1 | T5577_TRANS_BL_1_2;
    encoded_to_t5577(encoded, 2);
    lfrfid_program->t5577.max_blocks = 3;
    return true;
}

static bool protocol_keri_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[KERI_ENCODED_DATA_SIZE];
    tag->uid[0] |= 0x80U;
    keri_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_X_MODE | T5577_MOD_PSK1 | T5577_PSKCF_RF_2 |
                                          T5577_TRANS_BL_1_2 | (0xFUL << T5577_BITRATE_SHIFT);
    encoded_to_t5577(encoded, 2);
    lfrfid_program->t5577.max_blocks = 3;
    return true;
}

static bool protocol_nexwatch_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[NEXWATCH_ENCODED_DATA_SIZE];
    nexwatch_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_PSK1 | T5577_BITRATE_RF_32 | T5577_TRANS_BL_1_3;
    encoded_to_t5577(encoded, 3);
    lfrfid_program->t5577.max_blocks = 4;
    return true;
}

static bool protocol_jablotron_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[JABLOTRON_ENCODED_DATA_SIZE];
    jablotron_encode(tag->uid, encoded);
    jablotron_decode(encoded, tag->uid);
    jablotron_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_BIPHASE | T5577_BITRATE_RF_64 | T5577_TRANS_BL_1_2;
    encoded_to_t5577(encoded, 2);
    lfrfid_program->t5577.max_blocks = 3;
    return true;
}

static bool protocol_viking_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[VIKING_ENCODED_DATA_SIZE];
    viking_encode(tag->uid, encoded);
    viking_decode(encoded, tag->uid);
    viking_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_MANCHESTER | T5577_BITRATE_RF_32 | T5577_TRANS_BL_1_2;
    encoded_to_t5577(encoded, 2);
    lfrfid_program->t5577.max_blocks = 3;
    return true;
}

static bool protocol_paradox_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[PARADOX_ENCODED_DATA_SIZE];
    paradox_encode(tag->uid, encoded);
    paradox_decode(encoded, tag->uid);
    paradox_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_FSK2a | T5577_BITRATE_RF_50 | T5577_TRANS_BL_1_3;
    encoded_to_t5577(encoded, 3);
    lfrfid_program->t5577.max_blocks = 4;
    return true;
}

static bool protocol_pac_stanley_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[PAC_STANLEY_ENCODED_DATA_SIZE];
    pac_stanley_encode(tag->uid, encoded);
    pac_stanley_decode(encoded, tag->uid);
    pac_stanley_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_DIRECT | T5577_BITRATE_RF_32 | T5577_TRANS_BL_1_4;
    encoded_to_t5577(encoded, 4);
    lfrfid_program->t5577.max_blocks = 5;
    return true;
}

static bool protocol_noralsy_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[NORALSY_ENCODED_DATA_SIZE];
    noralsy_encode(tag->uid, encoded);
    noralsy_decode(encoded, tag->uid);
    noralsy_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_MANCHESTER | T5577_BITRATE_RF_32 | T5577_SST_TERM | T5577_TRANS_BL_1_3;
    encoded_to_t5577(encoded, 3);
    lfrfid_program->t5577.max_blocks = 4;
    return true;
}

static bool protocol_fdxb_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[FDXB_ENCODED_DATA_SIZE];
    fdxb_encode(tag->uid, encoded);
    fdxb_decode(encoded, tag->uid);
    fdxb_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_BIPHASE | T5577_BITRATE_RF_32 | T5577_TRANS_BL_1_4;
    encoded_to_t5577(encoded, 4);
    lfrfid_program->t5577.max_blocks = 5;
    return true;
}

static bool protocol_gproxii_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[GPROXII_DATA_SIZE];
    gproxii_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_BIPHASE | T5577_BITRATE_RF_64 | T5577_TRANS_BL_1_3;
    encoded_to_t5577(encoded, 3);
    lfrfid_program->t5577.max_blocks = 4;
    return true;
}

static bool protocol_securakey_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[SECURAKEY_RKKT_SIZE];
    uint16_t bits = securakey_encode(tag->uid, encoded);
    uint8_t blocks = (bits == SECURAKEY_RKKTH_BITS) ? 2U : 3U;
    lfrfid_program->t5577.block_data[0] = T5577_MOD_MANCHESTER | T5577_BITRATE_RF_40 |
                                          ((blocks == 2U) ? T5577_TRANS_BL_1_2 : T5577_TRANS_BL_1_3);
    encoded_to_t5577(encoded, blocks);
    lfrfid_program->t5577.max_blocks = blocks + 1U;
    return true;
}

static bool protocol_gallagher_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[GALLAGHER_ROLLING_DATA_SIZE];
    gallagher_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_MANCHESTER | T5577_BITRATE_RF_32 | T5577_TRANS_BL_1_3;
    encoded_to_t5577(encoded, 3);
    lfrfid_program->t5577.max_blocks = 4;
    return true;
}

static bool protocol_electra_write_begin(void *protocol, void *data)
{
    (void)data;
    LFRFID_TAG_INFO *tag = (LFRFID_TAG_INFO *)protocol;
    uint8_t encoded[ELECTRA_ENCODED_DATA_SIZE];
    electra_encode(tag->uid, encoded);
    lfrfid_program->t5577.block_data[0] = T5577_MOD_MANCHESTER | T5577_BITRATE_RF_64 | T5577_TRANS_BL_1_4;
    encoded_to_t5577(encoded, 4);
    lfrfid_program->t5577.max_blocks = 5;
    return true;
}

static void extra_write_send(void *proto)
{
    (void)proto;
    t5577_execute_write(lfrfid_program, 0);
}

static void render_hex_line(char *result, const char *prefix, const uint8_t *data, uint8_t len)
{
    int pos = sprintf(result, "%s", prefix);
    for (uint8_t i = 0; i < len; i++)
        pos += sprintf(&result[pos], "%02X%s", data[i], (i + 1U < len) ? " " : "");
}

static void protocol_hid_generic_render(void *protocol, char *result)
{
    (void)protocol;
    uint8_t size = hid_protocol_size(lfrfid_tag_info.uid);
    int pos = sprintf(result, "HID Prox");
    if (size != 0) pos += sprintf(&result[pos], " %u-bit", size);
    sprintf(&result[pos], "\nData: %02X %02X %02X %02X %02X %X",
            lfrfid_tag_info.uid[0], lfrfid_tag_info.uid[1], lfrfid_tag_info.uid[2],
            lfrfid_tag_info.uid[3], lfrfid_tag_info.uid[4], lfrfid_tag_info.uid[5] >> 4);
}

static void protocol_hid_ex_generic_render(void *protocol, char *result)
{
    (void)protocol;
    render_hex_line(result, "HID Ext\nData: ", lfrfid_tag_info.uid, HID_EX_DECODED_DATA_SIZE);
}

static void protocol_ioprox_render(void *protocol, char *result)
{
    (void)protocol;
    sprintf(result, "FC: %u\nVer: %u\nCode: %05u",
            lfrfid_tag_info.uid[0],
            lfrfid_tag_info.uid[1],
            (unsigned)MAKEWORD(lfrfid_tag_info.uid[3], lfrfid_tag_info.uid[2]));
}

static void protocol_awid_render(void *protocol, char *result)
{
    (void)protocol;
    uint8_t format = lfrfid_tag_info.uid[0];
    if (format == 26)
    {
        uint8_t fc = 0;
        uint16_t card = 0;
        bit_copy(&fc, 0, 8, lfrfid_tag_info.uid, 9);
        bit_copy((uint8_t *)&card, 8, 8, lfrfid_tag_info.uid, 17);
        bit_copy((uint8_t *)&card, 0, 8, lfrfid_tag_info.uid, 25);
        sprintf(result, "Format: %u\nFC: %u\nCard: %u", format, fc, card);
    }
    else
    {
        render_hex_line(result, "Format: ", lfrfid_tag_info.uid, AWID_DECODED_DATA_SIZE);
    }
}

static void protocol_fdxa_render(void *protocol, char *result)
{
    (void)protocol;
    uint8_t data[FDXA_DECODED_DATA_SIZE];
    memcpy(data, lfrfid_tag_info.uid, sizeof(data));
    uint8_t ok = 1;
    for (size_t i = 0; i < FDXA_DECODED_DATA_SIZE; i++)
    {
        if (bit_parity32(data[i], ExtraParityOdd)) ok = 0;
        data[i] &= 0x7FU;
    }
    sprintf(result, "ID: %010llX\nParity: %c", (unsigned long long)bit_get_bits64(data, 0, 40), ok ? '+' : '-');
}

static void protocol_pyramid_render(void *protocol, char *result)
{
    (void)protocol;
    sprintf(result, "Format: %u\nFC: %u\nCard: %u",
            lfrfid_tag_info.uid[0],
            lfrfid_tag_info.uid[1],
            (unsigned)MAKEWORD(lfrfid_tag_info.uid[3], lfrfid_tag_info.uid[2]));
}

static void protocol_indala26_render(void *protocol, char *result)
{
    (void)protocol;
    render_hex_line(result, "Indala26\nData: ", lfrfid_tag_info.uid, INDALA26_DECODED_DATA_SIZE);
}

static void protocol_idteck_render(void *protocol, char *result)
{
    (void)protocol;
    render_hex_line(result, "IDTECK\nData: ", lfrfid_tag_info.uid, IDTECK_DECODED_DATA_SIZE);
}

static void protocol_keri_render(void *protocol, char *result)
{
    (void)protocol;
    render_hex_line(result, "Keri\nData: ", lfrfid_tag_info.uid, KERI_DECODED_DATA_SIZE);
}

static void protocol_nexwatch_render(void *protocol, char *result)
{
    (void)protocol;
    render_hex_line(result, "Nexwatch\nData: ", lfrfid_tag_info.uid, NEXWATCH_DECODED_DATA_SIZE);
}

static void protocol_jablotron_render(void *protocol, char *result)
{
    (void)protocol;
    render_hex_line(result, "Jablotron\nData: ", lfrfid_tag_info.uid, JABLOTRON_DECODED_DATA_SIZE);
}

static void protocol_viking_render(void *protocol, char *result)
{
    (void)protocol;
    sprintf(result, "Card: %02X%02X%02X%02X",
            lfrfid_tag_info.uid[0], lfrfid_tag_info.uid[1],
            lfrfid_tag_info.uid[2], lfrfid_tag_info.uid[3]);
}

static void protocol_paradox_render(void *protocol, char *result)
{
    (void)protocol;
    render_hex_line(result, "Paradox\nData: ", lfrfid_tag_info.uid, PARADOX_DECODED_DATA_SIZE);
}

static void protocol_pac_stanley_render(void *protocol, char *result)
{
    (void)protocol;
    sprintf(result, "CIN: %02X%02X%02X%02X",
            lfrfid_tag_info.uid[0], lfrfid_tag_info.uid[1],
            lfrfid_tag_info.uid[2], lfrfid_tag_info.uid[3]);
}

static void protocol_noralsy_render(void *protocol, char *result)
{
    (void)protocol;
    uint32_t raw2 = bit_get_bits32(lfrfid_tag_info.uid, 32, 32);
    uint32_t raw3 = bit_get_bits32(lfrfid_tag_info.uid, 64, 32);
    uint32_t card = (((raw2 & 0xFFF00000UL) >> 20) << 16) |
                    ((raw2 & 0xFFU) << 8) |
                    ((raw3 & 0xFF000000UL) >> 24);
    uint8_t year = (uint8_t)((raw2 & 0x000FF000UL) >> 12);
    sprintf(result, "Card: %07lX\nYear: %s%02X", (unsigned long)card, (year > 0x60U) ? "19" : "20", year);
}

static void protocol_fdxb_render(void *protocol, char *result)
{
    (void)protocol;
    render_hex_line(result, "FDX-B\nData: ", lfrfid_tag_info.uid, FDXB_DECODED_DATA_SIZE);
}

static void protocol_gproxii_render(void *protocol, char *result)
{
    (void)protocol;
    render_hex_line(result, "GProxII\nData: ", lfrfid_tag_info.uid, GPROXII_DATA_SIZE);
}

static void protocol_securakey_render(void *protocol, char *result)
{
    (void)protocol;
    if (bit_get_bits32(lfrfid_tag_info.uid, 0, 16) == 0)
        sprintf(result, "RKKTH\nCard: %02X%02X%02X%02X",
                lfrfid_tag_info.uid[2], lfrfid_tag_info.uid[3],
                lfrfid_tag_info.uid[4], lfrfid_tag_info.uid[5]);
    else
        render_hex_line(result, "Radio Key\nData: ", lfrfid_tag_info.uid, SECURAKEY_DECODED_DATA_SIZE);
}

static void protocol_gallagher_render(void *protocol, char *result)
{
    (void)protocol;
    render_hex_line(result, "Gallagher\nRaw: ", lfrfid_tag_info.uid, GALLAGHER_ENCODED_DATA_SIZE);
}

static void protocol_electra_render(void *protocol, char *result)
{
    (void)protocol;
    render_hex_line(result, "Electra\nRaw: ", lfrfid_tag_info.uid, ELECTRA_ENCODED_DATA_SIZE);
}

const LFRFIDProtocolBase protocol_hid_generic = {
    .name = "HIDProx",
    .manufacturer = "Generic",
    .data_size = HID_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_hid_generic_begin, .execute = protocol_hid_generic_execute },
    .encoder = { .begin = protocol_hid_generic_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_hid_generic_write_begin, .send = extra_write_send },
    .render_data = protocol_hid_generic_render,
};

const LFRFIDProtocolBase protocol_hid_ex_generic = {
    .name = "HIDExt",
    .manufacturer = "Generic",
    .data_size = HID_EX_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_hid_ex_generic_begin, .execute = protocol_hid_ex_generic_execute },
    .encoder = { .begin = protocol_hid_ex_generic_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_hid_ex_generic_write_begin, .send = extra_write_send },
    .render_data = protocol_hid_ex_generic_render,
};

const LFRFIDProtocolBase protocol_io_prox_xsf = {
    .name = "IOProxXSF",
    .manufacturer = "Kantech",
    .data_size = IOPROX_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_ioprox_begin, .execute = protocol_ioprox_execute },
    .encoder = { .begin = protocol_ioprox_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_ioprox_write_begin, .send = extra_write_send },
    .render_data = protocol_ioprox_render,
};

const LFRFIDProtocolBase protocol_awid = {
    .name = "AWID",
    .manufacturer = "AWID",
    .data_size = AWID_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_awid_begin, .execute = protocol_awid_execute },
    .encoder = { .begin = protocol_awid_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_awid_write_begin, .send = extra_write_send },
    .render_data = protocol_awid_render,
};

const LFRFIDProtocolBase protocol_fdx_a = {
    .name = "FDX-A",
    .manufacturer = "FECAVA",
    .data_size = FDXA_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_fdxa_begin, .execute = protocol_fdxa_execute },
    .encoder = { .begin = protocol_fdxa_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_fdxa_write_begin, .send = extra_write_send },
    .render_data = protocol_fdxa_render,
};

const LFRFIDProtocolBase protocol_pyramid = {
    .name = "Pyramid",
    .manufacturer = "Farpointe",
    .data_size = PYRAMID_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_pyramid_begin, .execute = protocol_pyramid_execute },
    .encoder = { .begin = protocol_pyramid_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_pyramid_write_begin, .send = extra_write_send },
    .render_data = protocol_pyramid_render,
};

const LFRFIDProtocolBase protocol_indala26 = {
    .name = "Indala26",
    .manufacturer = "Motorola",
    .data_size = INDALA26_DECODED_DATA_SIZE,
    .features = LFRFIDFeaturePSK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_indala26_begin, .execute = protocol_indala26_execute },
    .encoder = { .begin = protocol_indala26_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_indala26_write_begin, .send = extra_write_send },
    .render_data = protocol_indala26_render,
};

const LFRFIDProtocolBase protocol_idteck = {
    .name = "Idteck",
    .manufacturer = "Idteck",
    .data_size = IDTECK_DECODED_DATA_SIZE,
    .features = LFRFIDFeaturePSK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_idteck_begin, .execute = protocol_idteck_execute },
    .encoder = { .begin = protocol_idteck_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_idteck_write_begin, .send = extra_write_send },
    .render_data = protocol_idteck_render,
};

const LFRFIDProtocolBase protocol_keri = {
    .name = "Keri",
    .manufacturer = "Keri",
    .data_size = KERI_DECODED_DATA_SIZE,
    .features = LFRFIDFeaturePSK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_keri_begin, .execute = protocol_keri_execute },
    .encoder = { .begin = protocol_keri_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_keri_write_begin, .send = extra_write_send },
    .render_data = protocol_keri_render,
};

const LFRFIDProtocolBase protocol_nexwatch = {
    .name = "Nexwatch",
    .manufacturer = "Honeywell",
    .data_size = NEXWATCH_DECODED_DATA_SIZE,
    .features = LFRFIDFeaturePSK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_nexwatch_begin, .execute = protocol_nexwatch_execute },
    .encoder = { .begin = protocol_nexwatch_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_nexwatch_write_begin, .send = extra_write_send },
    .render_data = protocol_nexwatch_render,
};

const LFRFIDProtocolBase protocol_jablotron = {
    .name = "Jablotron",
    .manufacturer = "Jablotron",
    .data_size = JABLOTRON_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_jablotron_begin, .execute = protocol_jablotron_execute },
    .encoder = { .begin = protocol_jablotron_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_jablotron_write_begin, .send = extra_write_send },
    .render_data = protocol_jablotron_render,
};

const LFRFIDProtocolBase protocol_viking = {
    .name = "Viking",
    .manufacturer = "Viking",
    .data_size = VIKING_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_viking_begin, .execute = protocol_viking_execute },
    .encoder = { .begin = protocol_viking_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_viking_write_begin, .send = extra_write_send },
    .render_data = protocol_viking_render,
};

const LFRFIDProtocolBase protocol_paradox = {
    .name = "Paradox",
    .manufacturer = "Paradox",
    .data_size = PARADOX_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_paradox_begin, .execute = protocol_paradox_execute },
    .encoder = { .begin = protocol_paradox_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_paradox_write_begin, .send = extra_write_send },
    .render_data = protocol_paradox_render,
};

const LFRFIDProtocolBase protocol_pac_stanley = {
    .name = "PAC/Stanley",
    .manufacturer = "N/A",
    .data_size = PAC_STANLEY_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_pac_stanley_begin, .execute = protocol_pac_stanley_execute },
    .encoder = { .begin = protocol_pac_stanley_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_pac_stanley_write_begin, .send = extra_write_send },
    .render_data = protocol_pac_stanley_render,
};

const LFRFIDProtocolBase protocol_noralsy = {
    .name = "Noralsy",
    .manufacturer = "Noralsy",
    .data_size = NORALSY_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_noralsy_begin, .execute = protocol_noralsy_execute },
    .encoder = { .begin = protocol_noralsy_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_noralsy_write_begin, .send = extra_write_send },
    .render_data = protocol_noralsy_render,
};

const LFRFIDProtocolBase protocol_fdx_b = {
    .name = "FDX-B",
    .manufacturer = "ISO",
    .data_size = FDXB_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_fdxb_begin, .execute = protocol_fdxb_execute },
    .encoder = { .begin = protocol_fdxb_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_fdxb_write_begin, .send = extra_write_send },
    .render_data = protocol_fdxb_render,
};

const LFRFIDProtocolBase protocol_gproxii = {
    .name = "GProxII",
    .manufacturer = "Guardall",
    .data_size = GPROXII_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_gproxii_begin, .execute = protocol_gproxii_execute },
    .encoder = { .begin = protocol_gproxii_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_gproxii_write_begin, .send = extra_write_send },
    .render_data = protocol_gproxii_render,
};

const LFRFIDProtocolBase protocol_securakey = {
    .name = "RadioKey",
    .manufacturer = "Securakey",
    .data_size = SECURAKEY_DECODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_securakey_begin, .execute = protocol_securakey_execute },
    .encoder = { .begin = protocol_securakey_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_securakey_write_begin, .send = extra_write_send },
    .render_data = protocol_securakey_render,
};

const LFRFIDProtocolBase protocol_gallagher = {
    .name = "Gallagher",
    .manufacturer = "Gallagher",
    .data_size = GALLAGHER_ENCODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_gallagher_begin, .execute = protocol_gallagher_execute },
    .encoder = { .begin = protocol_gallagher_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_gallagher_write_begin, .send = extra_write_send },
    .render_data = protocol_gallagher_render,
};

const LFRFIDProtocolBase protocol_electra = {
    .name = "Electra",
    .manufacturer = "Electra",
    .data_size = ELECTRA_ENCODED_DATA_SIZE,
    .features = LFRFIDFeatureASK,
    .get_data = extra_get_data,
    .decoder = { .begin = protocol_electra_begin, .execute = protocol_electra_execute },
    .encoder = { .begin = protocol_electra_encoder_begin, .send = extra_encoder_send },
    .write = { .begin = protocol_electra_write_begin, .send = extra_write_send },
    .render_data = protocol_electra_render,
};
