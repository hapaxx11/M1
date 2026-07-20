/* See COPYING.txt for license details. */

/*
 * m1_rf_fingerprint_24.c
 *
 * CAPS gate for the ESP32-C6 2.4 GHz fingerprint backend.  See
 * m1_rf_fingerprint_24.h.  This is the only file that couples the pure-logic
 * backend (Sub_Ghz/rf_fingerprint_24.c) to the ESP32 capability system.
 *
 * M1 Project — Hapax fork
 */

#include "m1_rf_fingerprint_24.h"
#include "rf_fingerprint_24.h"     /* rf24_cap_t, rf_sensor_24_required_cap */
#include "m1_esp32_caps.h"

#include <stddef.h>   /* NULL */

uint64_t m1_rf24_sensor_cap(rf_sensor_t sensor)
{
    switch (rf_sensor_24_required_cap(sensor))
    {
        case RF24_CAP_BLE_SCAN:  return M1_ESP32_CAP_BLE_SCAN;
        case RF24_CAP_WIFI_SCAN: return M1_ESP32_CAP_WIFI_SCAN;
        case RF24_CAP_802154:    return M1_ESP32_CAP_802154;
        case RF24_CAP_NONE:
        default:                 return 0U;
    }
}

bool m1_rf24_sensor_available(rf_sensor_t sensor)
{
    uint64_t cap = m1_rf24_sensor_cap(sensor);
    if (cap == 0U)
        return false;
    return m1_esp32_has_cap(cap);
}

bool m1_rf24_require_sensor(rf_sensor_t sensor, const char *feature_name)
{
    uint64_t cap = m1_rf24_sensor_cap(sensor);
    if (cap == 0U)
        return false;   /* not a 2.4 GHz sensor — fail closed */
    return m1_esp32_require_cap(cap,
                               feature_name != NULL ? feature_name
                                                    : "2.4 GHz Scan");
}
