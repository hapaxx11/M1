/* See COPYING.txt for license details. */

#ifndef M1_RPC_SCAN_H_
#define M1_RPC_SCAN_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "m1_i2c_scan.h"

#define RPC_GPIO_SCAN_PIN_COUNT 15u
#define RPC_GPIO_SCAN_RESULT_SIZE (1u + ((RPC_GPIO_SCAN_PIN_COUNT + 7u) / 8u))
#define RPC_I2C_SCAN_REQUEST_SIZE 2u
#define RPC_I2C_SCAN_MAX_RESPONSE_SIZE (1u + M1_I2C_SCAN_MAX_RESULTS)

/*
 * GPIO response payload: pin count followed by a little-endian bitset.
 * Bit N is the current level of m1_ext_gpio[N].
 */
static inline bool rpc_gpio_scan_encode(uint16_t levels,
                                        uint8_t *out,
                                        size_t capacity)
{
    if (out == NULL || capacity < RPC_GPIO_SCAN_RESULT_SIZE)
        return false;

    out[0] = RPC_GPIO_SCAN_PIN_COUNT;
    out[1] = (uint8_t)(levels & 0xFFu);
    out[2] = (uint8_t)((levels >> 8) & 0x7Fu);
    return true;
}

static inline bool rpc_i2c_scan_request_decode(const uint8_t *payload,
                                              uint16_t len,
                                              uint8_t *first,
                                              uint8_t *last)
{
    if (payload == NULL || first == NULL || last == NULL ||
        len != RPC_I2C_SCAN_REQUEST_SIZE ||
        !m1_i2c_scan_range_valid(payload[0], payload[1]))
        return false;

    *first = payload[0];
    *last = payload[1];
    return true;
}

static inline bool rpc_i2c_scan_request_valid(const uint8_t *payload,
                                              uint16_t len)
{
    return payload != NULL &&
           len == RPC_I2C_SCAN_REQUEST_SIZE &&
           m1_i2c_scan_range_valid(payload[0], payload[1]);
}

/*
 * I2C response payload: count followed by ascending 7-bit addresses.
 */
static inline bool rpc_i2c_scan_response_encode(const uint8_t *addresses,
                                                uint8_t count,
                                                uint8_t *out,
                                                size_t capacity,
                                                uint16_t *out_len)
{
    if (out == NULL || out_len == NULL ||
        (count > 0u && addresses == NULL) ||
        count > M1_I2C_SCAN_MAX_RESULTS ||
        capacity < (size_t)count + 1u)
        return false;

    for (uint8_t i = 0; i < count; i++)
    {
        if (addresses[i] < M1_I2C_SCAN_MIN_ADDRESS ||
            addresses[i] > M1_I2C_SCAN_MAX_ADDRESS ||
            (i > 0u && addresses[i] <= addresses[i - 1u]))
            return false;
    }

    out[0] = count;
    for (uint8_t i = 0; i < count; i++)
        out[i + 1u] = addresses[i];
    *out_len = (uint16_t)count + 1u;
    return true;
}

#endif /* M1_RPC_SCAN_H_ */
