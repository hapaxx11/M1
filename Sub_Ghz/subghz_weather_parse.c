/* See COPYING.txt for license details. */

/*
 * subghz_weather_parse.c
 *
 * Field extraction for weather-station frames.  Bit layouts, sentinel
 * handling and checksum algorithms follow the Flipper Weather Station app
 * (Next-Flip/Momentum-Apps: weather_station protocol decoders, GPLv3) so that a
 * sensor reads the same on the M1 as it does on a Flipper.
 *
 * Pure logic — no HAL, RTOS or radio dependencies.
 */

#include "subghz_weather_parse.h"
#include "m1_sub_ghz_decenc.h"      /* protocol id enum */
#include "subghz_blocks_math.h"

#include <stddef.h>

/*============================================================================*/
/* Helpers                                                                    */
/*============================================================================*/

int16_t subghz_weather_f_to_c_d10(int32_t temp_f_d10)
{
    /* C = (F - 32) * 5 / 9, both sides in tenths, round half away from zero. */
    int32_t num = (temp_f_d10 - 320) * 5;
    int32_t c   = (num >= 0) ? (num + 4) / 9 : (num - 4) / 9;
    if (c > 32767)  c = 32767;
    if (c < -32768) c = -32768;
    return (int16_t)c;
}

/** Sign-extend an @p bits -wide two's-complement field to int16_t. */
static int16_t sign_extend(uint32_t value, uint8_t bits)
{
    uint32_t mask = (1UL << bits) - 1UL;
    value &= mask;
    if (value & (1UL << (bits - 1))) {
        return (int16_t)((int32_t)value - (int32_t)(mask + 1UL));
    }
    return (int16_t)value;
}

static void fields_init(SubGhzWeatherFields *o)
{
    o->id          = 0;
    o->channel     = WX_NO_CHANNEL;
    o->button      = WX_NO_BUTTON;
    o->battery_low = 0;
    o->humidity    = WX_NO_HUMIDITY;
    o->temp_d10    = 0;
    o->has_temp    = false;
}

/** Clamp a raw humidity byte to 0..100, mapping anything else to "none". */
static uint8_t humidity_or_none(uint32_t raw)
{
    return (raw <= 100U) ? (uint8_t)raw : WX_NO_HUMIDITY;
}

static void msg_from_data(uint64_t data, uint8_t nbytes, uint8_t shift,
                          uint8_t *msg)
{
    uint8_t i;
    for (i = 0; i < nbytes; i++) {
        msg[i] = (uint8_t)((data >> (shift - 8U * i)) & 0xFFU);
    }
}

/*============================================================================*/
/* Per-protocol parsers                                                       */
/*============================================================================*/

static bool parse_lacrosse_tx141thbv2(uint64_t data, uint16_t bit_len,
                                      SubGhzWeatherFields *o)
{
    uint8_t msg[4];

    if (bit_len == 41U) {
        data >>= 1;             /* Flipper tolerates a leading stray bit */
    } else if (bit_len != 40U) {
        return false;
    }

    msg_from_data(data, 4, 32, msg);
    if (subghz_protocol_blocks_lfsr_digest8_reflect(msg, 4, 0x31, 0xF4) !=
        (uint8_t)(data & 0xFFU)) {
        return false;
    }

    fields_init(o);
    o->id          = (uint32_t)((data >> 32) & 0xFFU);
    o->battery_low = (uint8_t)((data >> 31) & 1U);
    o->button      = (uint8_t)((data >> 30) & 1U);
    o->channel     = (uint8_t)(((data >> 28) & 0x03U) + 1U);
    o->temp_d10    = (int16_t)((int32_t)((data >> 16) & 0x0FFFU) - 500);
    o->has_temp    = true;
    o->humidity    = humidity_or_none((uint32_t)((data >> 8) & 0xFFU));
    return true;
}

static bool parse_nexus_th(uint64_t data, uint16_t bit_len,
                           SubGhzWeatherFields *o)
{
    uint32_t hum;

    if (bit_len != 36U) return false;
    if (((data >> 8) & 0x0FU) != 0x0FU) return false;
    if ((data >> 4) == 0xFFFFFFFFULL) return false;

    fields_init(o);
    o->id          = (uint32_t)((data >> 28) & 0xFFU);
    o->battery_low = ((data >> 27) & 1U) ? 0U : 1U;   /* flag 1 = OK */
    o->channel     = (uint8_t)(((data >> 24) & 0x03U) + 1U);
    o->temp_d10    = sign_extend((uint32_t)((data >> 12) & 0x0FFFU), 12);
    o->has_temp    = true;

    hum = (uint32_t)(data & 0xFFU);
    if (hum > 95U)      hum = 95U;
    else if (hum < 20U) hum = 20U;
    o->humidity = (uint8_t)hum;
    return true;
}

static bool parse_gt_wt02(uint64_t data, uint16_t bit_len,
                          SubGhzWeatherFields *o)
{
    uint8_t sum;
    uint64_t tmp;
    uint8_t i;
    uint32_t hum;

    if (bit_len != 37U) return false;

    sum = (uint8_t)((data >> 5) & 0x0EU);
    tmp = data >> 9;
    for (i = 0; i < 7U; i++) {
        sum = (uint8_t)(sum + ((tmp >> (i * 4U)) & 0x0FU));
    }
    if ((uint8_t)(data & 0x3FU) != (uint8_t)(sum & 0x3FU)) return false;

    fields_init(o);
    o->id          = (uint32_t)((data >> 29) & 0xFFU);
    o->battery_low = (uint8_t)((data >> 28) & 1U);
    o->button      = (uint8_t)((data >> 27) & 1U);
    o->channel     = (uint8_t)(((data >> 25) & 0x03U) + 1U);
    o->temp_d10    = sign_extend((uint32_t)((data >> 13) & 0x0FFFU), 12);
    o->has_temp    = true;

    hum = (uint32_t)((data >> 6) & 0x7FU);
    if (hum <= 10U)      hum = 0U;     /* sensor reports "LL" */
    else if (hum > 90U)  hum = 100U;   /* sensor reports "HH" */
    o->humidity = (uint8_t)hum;
    return true;
}

static bool parse_gt_wt03(uint64_t data, uint16_t bit_len,
                          SubGhzWeatherFields *o)
{
    uint8_t msg[4];
    uint8_t sum = 0;
    uint8_t k;
    int8_t  i;
    uint32_t hum;

    if (bit_len != 41U) return false;

    msg_from_data(data >> 1, 4, 32, msg);   /* bits 40..9 */
    for (k = 0; k < 4U; k++) {
        uint8_t byte = msg[k];
        uint16_t key = 0x3100U;
        for (i = 7; i >= 0; i--) {
            if ((byte >> i) & 1U) {
                sum ^= (uint8_t)(key & 0xFFU);
            }
            key >>= 1;
        }
    }
    if ((uint8_t)(sum ^ (uint8_t)((data >> 1) & 0xFFU)) != 0x2DU) return false;

    fields_init(o);
    o->id          = (uint32_t)(data >> 33);
    hum            = (uint32_t)((data >> 25) & 0xFFU);
    if (hum <= 10U)      hum = 0U;
    else if (hum > 95U)  hum = 100U;
    o->humidity    = (uint8_t)hum;
    o->battery_low = (uint8_t)((data >> 24) & 1U);
    o->button      = (uint8_t)((data >> 23) & 1U);
    o->channel     = (uint8_t)(((data >> 21) & 0x03U) + 1U);
    o->temp_d10    = sign_extend((uint32_t)((data >> 9) & 0x0FFFU), 12);
    o->has_temp    = true;
    return true;
}

static bool parse_acurite_606tx(uint64_t data, uint16_t bit_len,
                                SubGhzWeatherFields *o)
{
    uint8_t msg[3];

    if (bit_len != 32U || data == 0U) return false;

    msg_from_data(data, 3, 24, msg);
    if (subghz_protocol_blocks_lfsr_digest8(msg, 3, 0x98, 0xF1) !=
        (uint8_t)(data & 0xFFU)) {
        return false;
    }

    fields_init(o);
    o->id          = (uint32_t)((data >> 24) & 0xFFU);
    o->battery_low = (uint8_t)((data >> 23) & 1U);
    o->temp_d10    = sign_extend((uint32_t)((data >> 8) & 0x0FFFU), 12);
    o->has_temp    = true;
    return true;
}

static bool parse_acurite_609txc(uint64_t data, uint16_t bit_len,
                                 SubGhzWeatherFields *o)
{
    uint8_t msg[4];

    if (bit_len != 40U) return false;

    msg_from_data(data, 4, 32, msg);
    if (subghz_protocol_blocks_add_bytes(msg, 4) != (uint8_t)(data & 0xFFU)) {
        return false;
    }

    fields_init(o);
    o->id          = (uint32_t)((data >> 32) & 0xFFU);
    o->battery_low = (uint8_t)((data >> 31) & 1U);
    o->temp_d10    = sign_extend((uint32_t)((data >> 16) & 0x0FFFU), 12);
    o->has_temp    = true;
    o->humidity    = humidity_or_none((uint32_t)((data >> 8) & 0xFFU));
    return true;
}

static bool parse_acurite_592txr(uint64_t data, uint16_t bit_len,
                                 SubGhzWeatherFields *o)
{
    static const uint8_t channel_map[4] = { 3U, 0U, 2U, 1U };
    uint8_t msg[7];
    uint16_t temp_raw;

    if (bit_len != 56U) return false;

    msg_from_data(data, 7, 48, msg);
    if (subghz_protocol_blocks_add_bytes(msg, 6) != msg[6]) return false;
    if (subghz_protocol_blocks_parity_bytes(&msg[2], 4) != 0U) return false;

    fields_init(o);
    o->channel     = channel_map[(data >> 54) & 0x03U];
    o->id          = (uint32_t)((data >> 40) & 0x3FFFU);
    o->battery_low = ((data >> 38) & 1U) ? 0U : 1U;    /* bit 1 = OK */
    o->humidity    = humidity_or_none((uint32_t)((data >> 24) & 0x7FU));
    temp_raw       = (uint16_t)(((data >> 9) & 0x0F80U) | ((data >> 8) & 0x7FU));
    o->temp_d10    = (int16_t)((int32_t)temp_raw - 1000);
    o->has_temp    = true;
    return true;
}

static bool parse_infactory(uint64_t data, uint16_t bit_len,
                            SubGhzWeatherFields *o)
{
    uint8_t msg[5];
    uint8_t crc;

    if (bit_len != 40U) return false;

    msg[0] = (uint8_t)((data >> 32) & 0xFFU);
    msg[1] = (uint8_t)(((data >> 24) & 0x0FU) | ((data & 0x0FU) << 4));
    msg[2] = (uint8_t)((data >> 16) & 0xFFU);
    msg[3] = (uint8_t)((data >> 8) & 0xFFU);
    msg[4] = (uint8_t)(data & 0xFFU);

    crc = subghz_protocol_blocks_crc4(msg, 4, 0x13, 0);
    crc ^= (uint8_t)(msg[4] >> 4);
    if (crc != (uint8_t)((data >> 28) & 0x0FU)) return false;

    fields_init(o);
    o->id       = (uint32_t)(data >> 32);
    o->battery_low = (uint8_t)((data >> 26) & 1U);
    /* Temperature is offset-encoded Fahrenheit: (raw - 900) / 10 degF. */
    o->temp_d10 = subghz_weather_f_to_c_d10(
        (int32_t)((data >> 12) & 0x0FFFU) - 900);
    o->has_temp = true;
    o->humidity = humidity_or_none((uint32_t)(((data >> 8) & 0x0FU) * 10U) +
                                   (uint32_t)((data >> 4) & 0x0FU));
    o->channel  = (uint8_t)(data & 0x03U);
    return true;
}

static bool parse_ambient_weather(uint64_t data, uint16_t bit_len,
                                  SubGhzWeatherFields *o)
{
    uint8_t msg[5];

    if (bit_len != 48U) return false;

    msg_from_data(data, 5, 40, msg);
    if ((uint8_t)(subghz_protocol_blocks_lfsr_digest8(msg, 5, 0x98, 0x3E) ^ 0x64U) !=
        (uint8_t)(data & 0xFFU)) {
        return false;
    }

    fields_init(o);
    o->id          = (uint32_t)((data >> 32) & 0xFFU);
    o->battery_low = (uint8_t)((data >> 31) & 1U);
    o->channel     = (uint8_t)(((data >> 28) & 0x07U) + 1U);
    o->temp_d10    = subghz_weather_f_to_c_d10(
        (int32_t)((data >> 16) & 0x0FFFU) - 400);
    o->has_temp    = true;
    o->humidity    = humidity_or_none((uint32_t)((data >> 8) & 0xFFU));
    return true;
}

static bool parse_thermopro_tx(uint64_t data, uint16_t bit_len,
                               SubGhzWeatherFields *o)
{
    uint8_t type;
    uint32_t hum;

    if (bit_len != 37U) return false;

    type = (uint8_t)(data >> 33);
    if (type != 0x09U && type != 0x06U) return false;

    fields_init(o);
    o->id          = (uint32_t)((data >> 25) & 0xFFU);
    o->battery_low = (uint8_t)((data >> 24) & 1U);
    o->button      = (uint8_t)((data >> 23) & 1U);
    o->channel     = (uint8_t)(((data >> 21) & 0x03U) + 1U);
    o->temp_d10    = sign_extend((uint32_t)((data >> 9) & 0x0FFFU), 12);
    o->has_temp    = true;

    hum = (uint32_t)((data >> 1) & 0xFFU);
    /* 0xCC marks a temperature-only sensor (TX-2). */
    o->humidity = (hum == 0xCCU) ? WX_NO_HUMIDITY : humidity_or_none(hum);
    return true;
}

static bool parse_auriol_ahfl(uint64_t data, uint16_t bit_len,
                              SubGhzWeatherFields *o)
{
    uint8_t sum = 0;
    uint8_t i;
    uint64_t payload;

    if (bit_len != 42U) return false;
    if (((data >> 6) & 0x0FU) != 0x04U) return false;

    payload = data >> 6;
    for (i = 0; i < 9U; i++) {
        sum = (uint8_t)(sum + ((payload >> (i * 4U)) & 0x0FU));
    }
    if (sum != (uint8_t)(data & 0x3FU)) return false;

    fields_init(o);
    o->id          = (uint32_t)(data >> 34);
    o->battery_low = (uint8_t)((data >> 33) & 1U);
    o->button      = (uint8_t)((data >> 32) & 1U);
    o->channel     = (uint8_t)(((data >> 30) & 0x03U) + 1U);
    o->temp_d10    = sign_extend((uint32_t)((data >> 18) & 0x0FFFU), 12);
    o->has_temp    = true;
    o->humidity    = humidity_or_none((uint32_t)((data >> 11) & 0x7FU));
    return true;
}

static bool parse_auriol_hg0601a(uint64_t data, uint16_t bit_len,
                                 SubGhzWeatherFields *o)
{
    if (bit_len != 37U) return false;
    if (((data >> 8) & 0x0FU) != 0x0EU) return false;
    if ((data >> 4) == 0xFFFFFFFFULL) return false;

    fields_init(o);
    o->id          = (uint32_t)((data >> 31) & 0xFFU);
    o->battery_low = (uint8_t)((data >> 30) & 1U);
    o->channel     = (uint8_t)(((data >> 25) & 0x03U) + 1U);
    o->temp_d10    = sign_extend((uint32_t)((data >> 13) & 0x0FFFU), 12);
    o->has_temp    = true;
    o->humidity    = humidity_or_none((uint32_t)((data >> 1) & 0x7FU));
    return true;
}

static bool parse_solight_te44(uint64_t data, uint16_t bit_len,
                               SubGhzWeatherFields *o)
{
    uint8_t msg[5];

    if (bit_len != 36U) return false;
    if (((data >> 8) & 0x0FU) != 0x0FU) return false;

    msg[0] = (uint8_t)((data >> 28) & 0xFFU);
    msg[1] = (uint8_t)((data >> 20) & 0xFFU);
    msg[2] = (uint8_t)((data >> 12) & 0xFFU);
    msg[3] = 0xF0U;
    msg[4] = (uint8_t)(data & 0xFFU);
    /* Rubicson CRC-8: poly 0x31, init 0x6c, result must be zero. */
    if (subghz_protocol_blocks_crc8(msg, 5, 0x31, 0x6C) != 0U) return false;

    fields_init(o);
    o->id          = (uint32_t)((data >> 28) & 0xFFU);
    o->battery_low = ((data >> 27) & 1U) ? 0U : 1U;
    o->channel     = (uint8_t)(((data >> 24) & 0x03U) + 1U);
    o->temp_d10    = sign_extend((uint32_t)((data >> 12) & 0x0FFFU), 12);
    o->has_temp    = true;
    return true;
}

static bool parse_kedsum_th(uint64_t data, uint16_t bit_len,
                            SubGhzWeatherFields *o)
{
    uint8_t msg[5];
    uint8_t crc;
    uint16_t temp_raw;

    if (bit_len != 42U) return false;

    msg_from_data(data, 5, 32, msg);
    crc = subghz_protocol_blocks_crc4(msg, 4, 0x03, 0);
    crc ^= (uint8_t)(msg[4] >> 4);
    if (crc != (uint8_t)(msg[4] & 0x0FU)) return false;

    fields_init(o);
    o->id          = (uint32_t)(data >> 32);
    o->battery_low = ((data >> 30) & 0x03U) ? 0U : 1U;
    o->channel     = (uint8_t)(((data >> 28) & 0x03U) + 1U);
    /* Temperature nibbles are transmitted in reverse order, offset degF. */
    temp_raw = (uint16_t)((((data >> 16) & 0x0FU) << 8) |
                          (((data >> 20) & 0x0FU) << 4) |
                          ((data >> 24) & 0x0FU));
    o->temp_d10 = subghz_weather_f_to_c_d10((int32_t)temp_raw - 900);
    o->has_temp = true;
    o->humidity = humidity_or_none((uint32_t)((((data >> 8) & 0x0FU) << 4) |
                                              ((data >> 12) & 0x0FU)));
    return true;
}

static bool parse_vauno_en8822c(uint64_t data, uint16_t bit_len,
                                SubGhzWeatherFields *o)
{
    uint8_t sum = 0;
    uint8_t i;

    if (bit_len != 42U) return false;

    for (i = 6U; i <= 38U; i += 4U) {
        sum = (uint8_t)(sum + ((data >> i) & 0x0FU));
    }
    if (sum == 0U || (uint8_t)(sum & 0x3FU) != (uint8_t)(data & 0x3FU)) {
        return false;
    }

    fields_init(o);
    o->id          = (uint32_t)((data >> 34) & 0xFFU);
    o->battery_low = (uint8_t)((data >> 33) & 1U);
    o->channel     = (uint8_t)((data >> 30) & 0x03U);
    o->temp_d10    = sign_extend((uint32_t)((data >> 18) & 0x0FFFU), 12);
    o->has_temp    = true;
    o->humidity    = humidity_or_none((uint32_t)((data >> 11) & 0x7FU));
    return true;
}

static bool parse_wendox_w6726(uint64_t data, uint16_t bit_len,
                               SubGhzWeatherFields *o)
{
    uint8_t msg[4];
    int32_t temp;

    if (bit_len != 29U) return false;

    msg[0] = (uint8_t)((data >> 28) & 0xFFU);
    msg[1] = (uint8_t)((data >> 20) & 0xFFU);
    msg[2] = (uint8_t)((data >> 12) & 0xFFU);
    msg[3] = (uint8_t)((data >> 4) & 0xFFU);
    if (subghz_protocol_blocks_crc4(msg, 4, 0x9, 0xD) !=
        (uint8_t)(data & 0x0FU)) {
        return false;
    }

    fields_init(o);
    o->id          = (uint32_t)((data >> 24) & 0xFFU);
    o->battery_low = (uint8_t)((data >> 6) & 1U);

    if ((data >> 23) & 1U) {
        temp = (int32_t)((data >> 14) & 0x1FFU) + 12;
    } else {
        temp = -((int32_t)((~(data >> 14)) & 0x1FFU) + 1 - 12);
    }
    if (temp < -500) temp = -500;
    else if (temp > 700) temp = 700;
    o->temp_d10 = (int16_t)temp;
    o->has_temp = true;
    return true;
}

static bool parse_bresser_3ch(uint64_t data, uint16_t bit_len,
                              SubGhzWeatherFields *o)
{
    uint8_t msg[4];

    if (bit_len != 40U) return false;

    msg_from_data(data, 4, 32, msg);
    if (subghz_protocol_blocks_add_bytes(msg, 4) != (uint8_t)(data & 0xFFU)) {
        return false;
    }

    fields_init(o);
    o->id          = (uint32_t)((data >> 32) & 0xFFU);
    o->battery_low = (uint8_t)((data >> 31) & 1U);
    o->button      = (uint8_t)((data >> 30) & 1U);
    o->channel     = (uint8_t)((data >> 28) & 0x03U);
    o->temp_d10    = subghz_weather_f_to_c_d10(
        (int32_t)((data >> 16) & 0x0FFFU) - 900);
    o->has_temp    = true;
    o->humidity    = humidity_or_none((uint32_t)((data >> 8) & 0xFFU));
    return true;
}

static bool parse_tx_8300(uint64_t data, uint16_t bit_len,
                          SubGhzWeatherFields *o)
{
    int32_t temp;
    uint32_t hum;

    /* The M1 decoder commits package_1 (32 payload bits) as the data word. */
    if (bit_len != 32U) return false;

    fields_init(o);
    hum = (uint32_t)((((data >> 28) & 0x0FU) * 10U) + ((data >> 24) & 0x0FU));
    o->humidity    = humidity_or_none(hum);
    o->battery_low = ((data >> 22) & 0x03U) ? 1U : 0U;
    o->channel     = (uint8_t)((data >> 20) & 0x03U);
    o->id          = (uint32_t)((data >> 12) & 0x7FU);

    temp = (int32_t)(((data >> 8) & 0x0FU) * 100U) +
           (int32_t)(((data >> 4) & 0x0FU) * 10U) +
           (int32_t)(data & 0x0FU);
    o->temp_d10 = (int16_t)(((data >> 19) & 1U) ? -temp : temp);
    o->has_temp = true;
    return true;
}

/*============================================================================*/
/* Dispatch                                                                   */
/*============================================================================*/

bool subghz_weather_parse(uint16_t protocol, uint64_t data, uint16_t bit_len,
                          SubGhzWeatherFields *out)
{
    if (out == NULL) {
        return false;
    }

    switch (protocol) {
        case LACROSSE_TX141THBV2: return parse_lacrosse_tx141thbv2(data, bit_len, out);
        case NEXUS_TH:            return parse_nexus_th(data, bit_len, out);
        case GT_WT02:             return parse_gt_wt02(data, bit_len, out);
        case GT_WT03:             return parse_gt_wt03(data, bit_len, out);
        case ACURITE_606TX:       return parse_acurite_606tx(data, bit_len, out);
        case ACURITE_609TXC:      return parse_acurite_609txc(data, bit_len, out);
        case ACURITE_592TXR:      return parse_acurite_592txr(data, bit_len, out);
        case INFACTORY:           return parse_infactory(data, bit_len, out);
        case AMBIENT_WEATHER:     return parse_ambient_weather(data, bit_len, out);
        case THERMOPRO_TX2:
        case THERMOPRO_TX4:       return parse_thermopro_tx(data, bit_len, out);
        case AURIOL_AHFL:         return parse_auriol_ahfl(data, bit_len, out);
        case AURIOL_HG0601A:      return parse_auriol_hg0601a(data, bit_len, out);
        case SOLIGHT_TE44:        return parse_solight_te44(data, bit_len, out);
        case KEDSUM_TH:           return parse_kedsum_th(data, bit_len, out);
        case VAUNO_EN8822C:       return parse_vauno_en8822c(data, bit_len, out);
        case WENDOX_W6726:        return parse_wendox_w6726(data, bit_len, out);
        case BRESSER_3CH:         return parse_bresser_3ch(data, bit_len, out);
        case TX_8300:             return parse_tx_8300(data, bit_len, out);
        default:                  return false;
    }
}

bool subghz_weather_parse_supported(uint16_t protocol)
{
    switch (protocol) {
        case LACROSSE_TX141THBV2:
        case NEXUS_TH:
        case GT_WT02:
        case GT_WT03:
        case ACURITE_606TX:
        case ACURITE_609TXC:
        case ACURITE_592TXR:
        case INFACTORY:
        case AMBIENT_WEATHER:
        case THERMOPRO_TX2:
        case THERMOPRO_TX4:
        case AURIOL_AHFL:
        case AURIOL_HG0601A:
        case SOLIGHT_TE44:
        case KEDSUM_TH:
        case VAUNO_EN8822C:
        case WENDOX_W6726:
        case BRESSER_3CH:
        case TX_8300:
            return true;
        default:
            return false;
    }
}
