/* See COPYING.txt for license details. */

/*
 * rf_protocol_db.c
 *
 * The protocol-fingerprint signature database.  See rf_protocol_db.h.
 *
 * Entries are grouped by category.  Timing windows (te_min/te_max) are the
 * short-symbol element in microseconds and are drawn from the M1 decoder
 * registry and public Flipper/rtl_433 protocol documentation.  They are
 * intentionally generous — this table categorises a signal, it does not decode
 * it, so a wide window that catches the family is preferable to a narrow one
 * that misses off-nominal captures.
 *
 * M1 Project — Hapax fork
 */

#include "rf_protocol_db.h"

#include <stddef.h>   /* NULL */
#include <string.h>   /* strcmp */

/* Convenience band groupings. */
#define B_315_433   (RF_BAND_315 | RF_BAND_433)
#define B_868_915   (RF_BAND_868 | RF_BAND_915)
#define B_SUBGHZ    (RF_BAND_300 | RF_BAND_315 | RF_BAND_433 | RF_BAND_868 | RF_BAND_915)

/* Canonical names for the three ESP32-C6 2.4 GHz domains.  Shared between the
 * table below and rf_protocol_db_find_2400() so the sensor -> signature mapping
 * stays correct even if the surrounding table is reordered. */
#define RF_SIG_NAME_BLE     "BLE Advertising"
#define RF_SIG_NAME_WIFI    "WiFi (2.4 GHz)"
#define RF_SIG_NAME_ZIGBEE  "Zigbee/Thread (802.15.4)"

static const rf_protocol_sig_t rf_db[] = {
    /* ---- Automotive --------------------------------------------------- */
    {
        .name = "Car Key Fob (rolling)", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        .te_min_us = 100, .te_max_us = 500, .bits_min = 40, .bits_max = 0,
        .security = RF_SEC_ROLLING, .known_vuln = true,
        .device_note = "Keyless entry (KeeLoq etc.)",
        .security_note = "Rolling code; RollJam/relay capture attacks known",
    },
    {
        .name = "Car Key Fob (fixed)", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 150, .te_max_us = 600, .bits_min = 24, .bits_max = 66,
        .security = RF_SEC_FIXED, .known_vuln = true,
        .device_note = "Older fixed-code fobs",
        .security_note = "Fixed code — directly replayable",
    },
    {
        .name = "TPMS Tyre Sensor", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        .te_min_us = 40, .te_max_us = 130, .bits_min = 64, .bits_max = 0,
        .security = RF_SEC_FIXED, .known_vuln = false,
        .device_note = "Tyre pressure sensors (315/433)",
        .security_note = "Broadcasts ID+pressure in clear; trackable",
    },

    /* ---- Home --------------------------------------------------------- */
    {
        .name = "Garage/Gate (fixed)", .category = RF_CAT_HOME,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 200, .te_max_us = 1200, .bits_min = 8, .bits_max = 40,
        .security = RF_SEC_FIXED, .known_vuln = true,
        .device_note = "DIP-switch / fixed remotes",
        .security_note = "Fixed code — directly replayable",
    },
    {
        .name = "Garage/Gate (rolling)", .category = RF_CAT_HOME,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 200, .te_max_us = 900, .bits_min = 40, .bits_max = 0,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "CAME/Nice/Somfy rolling remotes",
        .security_note = "Rolling code — single-capture replay defeated",
    },
    {
        .name = "Wireless Doorbell", .category = RF_CAT_HOME,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 150, .te_max_us = 700, .bits_min = 16, .bits_max = 48,
        .security = RF_SEC_FIXED, .known_vuln = false,
        .device_note = "Doorbell chime transmitters",
        .security_note = "Fixed code — replayable (low impact)",
    },
    {
        .name = "RF Mains Socket", .category = RF_CAT_HOME,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 150, .te_max_us = 600, .bits_min = 12, .bits_max = 32,
        .security = RF_SEC_FIXED, .known_vuln = true,
        .device_note = "Remote-switch mains sockets",
        .security_note = "Fixed code — directly replayable",
    },

    /* ---- Security ----------------------------------------------------- */
    {
        .name = "PIR Motion Sensor", .category = RF_CAT_SECURITY,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 150, .te_max_us = 700, .bits_min = 16, .bits_max = 48,
        .security = RF_SEC_FIXED, .known_vuln = true,
        .device_note = "Alarm PIR / door-contact sensors",
        .security_note = "Often fixed code — spoofable/replayable",
    },
    {
        .name = "Door/Window Contact", .category = RF_CAT_SECURITY,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 150, .te_max_us = 700, .bits_min = 16, .bits_max = 48,
        .security = RF_SEC_FIXED, .known_vuln = true,
        .device_note = "Alarm reed-switch sensors",
        .security_note = "Fixed code — jam/replay possible",
    },
    {
        .name = "Alarm Keyfob", .category = RF_CAT_SECURITY,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 150, .te_max_us = 700, .bits_min = 24, .bits_max = 0,
        .security = RF_SEC_FIXED, .known_vuln = false,
        .device_note = "Panic buttons / arm-disarm fobs",
        .security_note = "Fixed code common — verify before trusting",
    },

    /* ---- Weather ------------------------------------------------------ */
    {
        .name = "Weather Station", .category = RF_CAT_WEATHER,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 400, .te_max_us = 2000, .bits_min = 24, .bits_max = 0,
        .security = RF_SEC_FIXED, .known_vuln = false,
        .device_note = "Temp/humidity outdoor sensors",
        .security_note = "Unauthenticated broadcast; spoofable readings",
    },
    {
        .name = "Weather Station (868)", .category = RF_CAT_WEATHER,
        .mod = RF_MOD_FSK, .bands = RF_BAND_868,
        .te_min_us = 40, .te_max_us = 200, .bits_min = 24, .bits_max = 0,
        .security = RF_SEC_FIXED, .known_vuln = false,
        .device_note = "868 MHz weather sensors",
        .security_note = "Unauthenticated broadcast; spoofable readings",
    },

    /* ---- IoT ---------------------------------------------------------- */
    {
        .name = "Z-Wave Node", .category = RF_CAT_IOT,
        .mod = RF_MOD_FSK, .bands = B_868_915,
        .te_min_us = 8, .te_max_us = 60, .bits_min = 80, .bits_max = 0,
        .security = RF_SEC_ENCRYPTED, .known_vuln = false,
        .device_note = "Z-Wave home-automation (868 EU/908 US)",
        .security_note = "S2 encryption on modern nodes; legacy S0 weaker",
    },
    {
        .name = "LoRa Node", .category = RF_CAT_IOT,
        .mod = RF_MOD_FSK, .bands = B_868_915,
        .te_min_us = 0, .te_max_us = 0, .bits_min = 0, .bits_max = 0,
        .security = RF_SEC_ENCRYPTED, .known_vuln = false,
        .device_note = "LoRaWAN sensors (868 EU/915 US)",
        .security_note = "CSS chirp; AES payload — not OOK-replayable",
    },

    /* ---- Utility ------------------------------------------------------ */
    {
        .name = "Smart Meter (AMR)", .category = RF_CAT_UTILITY,
        .mod = RF_MOD_FSK, .bands = B_868_915,
        .te_min_us = 8, .te_max_us = 60, .bits_min = 80, .bits_max = 0,
        .security = RF_SEC_FIXED, .known_vuln = false,
        .device_note = "Electric/gas/water AMR meters",
        .security_note = "Consumption broadcast; often unauthenticated",
    },

    /* ---- Industrial --------------------------------------------------- */
    {
        .name = "Industrial Remote", .category = RF_CAT_INDUSTRIAL,
        .mod = RF_MOD_FSK, .bands = B_SUBGHZ,
        .te_min_us = 100, .te_max_us = 800, .bits_min = 24, .bits_max = 0,
        .security = RF_SEC_FIXED, .known_vuln = true,
        .device_note = "Crane / SCADA field remotes",
        .security_note = "Frequently fixed code — safety-critical replay risk",
    },

    /* ---- 2.4 GHz (ESP32-C6 domains) ----------------------------------- */
    {
        .name = RF_SIG_NAME_BLE, .category = RF_CAT_CONSUMER,
        .mod = RF_MOD_FSK, .bands = RF_BAND_2400,
        .te_min_us = 0, .te_max_us = 0, .bits_min = 0, .bits_max = 0,
        .security = RF_SEC_UNKNOWN, .known_vuln = false,
        .device_note = "Bluetooth LE advertisers (phones, tags, beacons)",
        .security_note = "MAC may rotate; static MAC = trackable",
    },
    {
        .name = RF_SIG_NAME_WIFI, .category = RF_CAT_CONSUMER,
        .mod = RF_MOD_FSK, .bands = RF_BAND_2400,
        .te_min_us = 0, .te_max_us = 0, .bits_min = 0, .bits_max = 0,
        .security = RF_SEC_ENCRYPTED, .known_vuln = false,
        .device_note = "802.11b/g/n access points & stations",
        .security_note = "WPA2/3 encrypts payload; mgmt frames are clear",
    },
    {
        .name = RF_SIG_NAME_ZIGBEE, .category = RF_CAT_IOT,
        .mod = RF_MOD_FSK, .bands = RF_BAND_2400,
        .te_min_us = 0, .te_max_us = 0, .bits_min = 0, .bits_max = 0,
        .security = RF_SEC_ENCRYPTED, .known_vuln = false,
        .device_note = "Zigbee / Thread mesh devices",
        .security_note = "AES-CCM link encryption on commissioned networks",
    },
};

#define RF_DB_COUNT (sizeof(rf_db) / sizeof(rf_db[0]))

uint16_t rf_protocol_db_count(void)
{
    return (uint16_t)RF_DB_COUNT;
}

const rf_protocol_sig_t *rf_protocol_db_get(uint16_t index)
{
    if (index >= RF_DB_COUNT)
        return NULL;
    return &rf_db[index];
}

int rf_protocol_db_find_2400(rf_sensor_t sensor)
{
    const char *want;
    switch (sensor)
    {
        case RF_SENSOR_BLE:     want = RF_SIG_NAME_BLE;    break;
        case RF_SENSOR_WIFI:    want = RF_SIG_NAME_WIFI;   break;
        case RF_SENSOR_802154:  want = RF_SIG_NAME_ZIGBEE; break;
        default:                return -1;   /* not a 2.4 GHz sensor */
    }

    for (uint16_t i = 0; i < RF_DB_COUNT; i++)
    {
        if (rf_db[i].name != NULL && strcmp(rf_db[i].name, want) == 0)
            return (int)i;
    }
    return -1;
}

const char *rf_category_str(rf_category_t cat)
{
    switch (cat)
    {
        case RF_CAT_AUTOMOTIVE: return "Automotive";
        case RF_CAT_HOME:       return "Home";
        case RF_CAT_SECURITY:   return "Security";
        case RF_CAT_WEATHER:    return "Weather";
        case RF_CAT_IOT:        return "IoT";
        case RF_CAT_UTILITY:    return "Utility";
        case RF_CAT_INDUSTRIAL: return "Industrial";
        case RF_CAT_MEDICAL:    return "Medical";
        case RF_CAT_CONSUMER:   return "Consumer";
        case RF_CAT_MISC:       return "Misc";
        default:                return "Unknown";
    }
}

const char *rf_security_str(rf_security_t sec)
{
    switch (sec)
    {
        case RF_SEC_FIXED:     return "Fixed";
        case RF_SEC_ROLLING:   return "Rolling";
        case RF_SEC_ENCRYPTED: return "Encrypted";
        default:               return "Unknown";
    }
}
