/* See COPYING.txt for license details. */

/**
 * @file   esp32_feature_map.c
 * @brief  ESP32-gated feature classifier — pure logic, no HAL/RTOS deps.
 *
 * See esp32_feature_map.h for the full design rationale.
 *
 * To add a new gated feature:
 *   1. Add the enum value to esp32_feature_id_t in the header.
 *   2. Add one row to s_feature_table[] below (same index as the enum value).
 *   3. Add the feature to test_esp32_feature_map.c.
 *   4. Use DELEGATE_FEATURE(name, fn, ESP32_FEATURE_xxx) in the scene file.
 *      Do NOT add new DELEGATE_CAPPED(... M1_ESP32_CAP_xxx, "label") sites.
 *
 * M1 Project
 */

#include "esp32_feature_map.h"
#include "m1_esp32_caps.h"   /* M1_ESP32_CAP_* bits — header-only, no HAL */
#include <stddef.h>

/* =========================================================================
 * Feature table
 *
 * Indexed by esp32_feature_id_t.  Must stay in the same order as the enum.
 * =========================================================================*/

typedef struct {
    uint64_t    required_caps;  /**< OR'd M1_ESP32_CAP_* bits */
    const char *label;          /**< Short UI label for "not supported" screen */
} esp32_feature_entry_t;

static const esp32_feature_entry_t s_feature_table[ESP32_FEATURE_COUNT] = {
    /* ESP32_FEATURE_WIFI_SCAN */
    { M1_ESP32_CAP_WIFI_SCAN,    "WiFi Scan"        },
    /* ESP32_FEATURE_STA_SCAN */
    { M1_ESP32_CAP_STA_SCAN,     "Station Scan"     },
    /* ESP32_FEATURE_PKTMON */
    { M1_ESP32_CAP_PKTMON,       "Packet Monitor"   },
    /* ESP32_FEATURE_DEAUTH */
    { M1_ESP32_CAP_DEAUTH,       "Deauth Attack"    },
    /* ESP32_FEATURE_BEACON */
    { M1_ESP32_CAP_BEACON,       "Beacon Spam"      },
    /* ESP32_FEATURE_PROBE_FLOOD */
    { M1_ESP32_CAP_PROBE_FLOOD,  "Probe Flood"      },
    /* ESP32_FEATURE_KARMA */
    { M1_ESP32_CAP_KARMA,        "Karma AP"         },
    /* ESP32_FEATURE_PORTAL */
    { M1_ESP32_CAP_PORTAL,       "Evil Portal"      },
    /* ESP32_FEATURE_WIFI_JOIN */
    { M1_ESP32_CAP_WIFI_JOIN,    "WiFi Connect"     },
    /* ESP32_FEATURE_WIFI_SET_MAC */
    { M1_ESP32_CAP_WIFI_SET_MAC, "Set WiFi MAC"     },
    /* ESP32_FEATURE_WIFI_SET_CHAN */
    { M1_ESP32_CAP_WIFI_SET_CHAN,"Set WiFi Chan"    },
    /* ESP32_FEATURE_NETSCAN */
    { M1_ESP32_CAP_NETSCAN,      "Net Scan"         },
    /* ESP32_FEATURE_802154 */
    { M1_ESP32_CAP_802154,       "Zigbee/Thread"    },
    /* ESP32_FEATURE_BLE_SCAN */
    { M1_ESP32_CAP_BLE_SCAN,     "BLE Scan"         },
    /* ESP32_FEATURE_BLE_ADV */
    { M1_ESP32_CAP_BLE_ADV,      "BLE Advertise"    },
    /* ESP32_FEATURE_BLE_HID */
    { M1_ESP32_CAP_BLE_HID,      "Bad-BT"           },
    /* ESP32_FEATURE_BLE_GATT */
    { M1_ESP32_CAP_BLE_GATT,     "GATT Discover"    },
    /* ESP32_FEATURE_BT_MANAGE */
    { M1_ESP32_CAP_BT_MANAGE,    "BT Management"    },
};

/* Compile-time assertion: table length matches enum count */
typedef char assert_feature_table_size[
    (sizeof(s_feature_table) / sizeof(s_feature_table[0]) ==
     (size_t)ESP32_FEATURE_COUNT) ? 1 : -1
];

/* =========================================================================
 * Public API
 * =========================================================================*/

bool esp32_feature_supported(uint64_t cap_bitmap, esp32_feature_id_t fid)
{
    if ((unsigned)fid >= (unsigned)ESP32_FEATURE_COUNT)
        return false;
    uint64_t need = s_feature_table[fid].required_caps;
    return (cap_bitmap & need) == need;
}

uint64_t esp32_feature_required_caps(esp32_feature_id_t fid)
{
    if ((unsigned)fid >= (unsigned)ESP32_FEATURE_COUNT)
        return 0u;
    return s_feature_table[fid].required_caps;
}

const char *esp32_feature_label(esp32_feature_id_t fid)
{
    if ((unsigned)fid >= (unsigned)ESP32_FEATURE_COUNT)
        return "";
    return s_feature_table[fid].label;
}

bool esp32_firmware_is_sin360(uint64_t cap_bitmap)
{
    /* SiN360 binary-SPI firmware: BLE_HID present (SiN360 feature) AND
     * WIFI_JOIN absent (AT-firmware feature).  All-zero bitmaps (unknown
     * firmware) correctly return false. */
    return (cap_bitmap & M1_ESP32_CAP_BLE_HID) != 0u &&
           (cap_bitmap & M1_ESP32_CAP_WIFI_JOIN) == 0u;
}
