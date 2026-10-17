/* See COPYING.txt for license details. */

#ifndef M1_I2C_SCAN_H_
#define M1_I2C_SCAN_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define M1_I2C_SCAN_MIN_ADDRESS 0x08u
#define M1_I2C_SCAN_MAX_ADDRESS 0x77u
#define M1_I2C_SCAN_MAX_RESULTS \
    (M1_I2C_SCAN_MAX_ADDRESS - M1_I2C_SCAN_MIN_ADDRESS + 1u)
#define M1_I2C_SCAN_PROBE_TIMEOUT_MS 5u

typedef enum {
    M1_I2C_SCAN_PROBE_ACK,
    M1_I2C_SCAN_PROBE_NACK,
    M1_I2C_SCAN_PROBE_ERROR,
} m1_i2c_scan_probe_result_t;

static inline m1_i2c_scan_probe_result_t m1_i2c_scan_probe_classify(
    bool completed, bool nack)
{
    if (!completed)
        return M1_I2C_SCAN_PROBE_ERROR;
    return nack ? M1_I2C_SCAN_PROBE_NACK : M1_I2C_SCAN_PROBE_ACK;
}

typedef m1_i2c_scan_probe_result_t (*m1_i2c_scan_probe_fn)(
    uint8_t address, void *context);

static inline bool m1_i2c_scan_range_valid(uint8_t first, uint8_t last)
{
    return first >= M1_I2C_SCAN_MIN_ADDRESS &&
           last <= M1_I2C_SCAN_MAX_ADDRESS &&
           first <= last;
}

static inline bool m1_i2c_scan_collect(uint8_t first,
                                      uint8_t last,
                                      m1_i2c_scan_probe_fn probe,
                                      void *context,
                                      uint8_t *addresses,
                                      size_t capacity,
                                      uint8_t *count)
{
    uint16_t address;
    uint16_t range_size;
    uint8_t found = 0u;

    if (count == NULL || addresses == NULL || probe == NULL ||
        !m1_i2c_scan_range_valid(first, last))
        return false;

    *count = 0u;
    range_size = (uint16_t)last - first + 1u;
    if (capacity < range_size)
        return false;

    for (address = first; address <= last; address++)
    {
        m1_i2c_scan_probe_result_t result = probe((uint8_t)address, context);
        if (result == M1_I2C_SCAN_PROBE_ACK)
            addresses[found++] = (uint8_t)address;
        else if (result != M1_I2C_SCAN_PROBE_NACK)
            return false;
    }

    *count = found;
    return true;
}

#endif /* M1_I2C_SCAN_H_ */
