/* See COPYING.txt for license details. */

/*
 * rf_protocol_db.c
 *
 * The protocol-fingerprint signature database.  See rf_protocol_db.h.
 *
 * Entries are grouped by category.  Timing windows (te_min/te_max) are
 * sized to contain the decoder registry's te_short ± te_delta and
 * te_long ± te_delta with a modest margin.  The fingerprint estimator
 * (subghz_mod_suggest) reports te as the mean of the *shortest* pulse
 * cluster (~te_short), but the window covers both elements so off-nominal
 * captures and estimator drift still land inside.  They are intentionally
 * generous — this table categorises a signal, it does not decode it.
 *
 * Ordering matters: rf_match_best() keeps the first row when scores tie,
 * so the per-protocol automotive signatures below precede the generic
 * "Car Key Fob" / TPMS catch-all rows — a fingerprint that fits both is
 * named after the specific protocol, and the generic rows still catch
 * fobs no specific window fits.
 *
 * registry_index values mirror the protocol-index enum in
 * m1_sub_ghz_decenc.h.  They are written as literals (with the enum name
 * in a comment) because this file must stay free of decoder-registry
 * includes; tests/test_rf_protocol_db.c links the live registry and
 * verifies every linked row's index by enum value and name, so drift is
 * caught by CI.
 *
 * M1 Project — Hapax fork
 */

#include "rf_protocol_db.h"

#include <stddef.h>   /* NULL */
#include <string.h>   /* strcmp */

/* Convenience band groupings. */
#define B_315_433       (RF_BAND_315 | RF_BAND_433)
#define B_315_433_868   (RF_BAND_315 | RF_BAND_433 | RF_BAND_868)
#define B_868_915       (RF_BAND_868 | RF_BAND_915)
#define B_SUBGHZ        (RF_BAND_300 | RF_BAND_315 | RF_BAND_433 | RF_BAND_868 | RF_BAND_915)

/* Canonical names for the three ESP32-C6 2.4 GHz domains.  Shared between the
 * table below and rf_protocol_db_find_2400() so the sensor -> signature mapping
 * stays correct even if the surrounding table is reordered. */
#define RF_SIG_NAME_BLE     "BLE Advertising"
#define RF_SIG_NAME_WIFI    "WiFi (2.4 GHz)"
#define RF_SIG_NAME_ZIGBEE  "Zigbee/Thread (802.15.4)"

static const rf_protocol_sig_t rf_db[] = {
    /* ---- Automotive: per-protocol signatures --------------------------- */
    /* One row per ProtoPirate automotive keyfob family in the decoder
     * registry, linked via registry_index (enum name in the trailing
     * comment).  Windows cover te_short..te_long ± te_delta plus margin;
     * see the file-header comment for the sizing/ordering rationale.
     * Rows with both AM and FM registry flags carry RF_MOD_UNKNOWN so the
     * modulation feature is skipped rather than wrongly gated. */
    {
        .name = "Honda Static", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        .te_min_us = 20, .te_max_us = 190, .bits_min = 64, .bits_max = 80,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Honda/Acura keyfob (FSK static frame)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 107,   /* HONDA_STATIC */
    },
    {
        .name = "Ford V1", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        .te_min_us = 20, .te_max_us = 190, .bits_min = 136, .bits_max = 160,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Ford keyfob V1 (FSK, 136-bit frame)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 118,   /* FORD_V1 */
    },
    {
        .name = "Honda V1", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 420, .te_max_us = 2600, .bits_min = 68, .bits_max = 84,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Honda/Acura keyfob V1 (slow OOK)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 116,   /* HONDA_V1 */
    },
    {
        .name = "Subaru", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 450, .te_max_us = 2000, .bits_min = 64, .bits_max = 84,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Subaru keyfob (64..80-bit commits)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 115,   /* SUBARU_PP */
    },
    {
        .name = "Kia V1", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 450, .te_max_us = 2000, .bits_min = 57, .bits_max = 70,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Kia/Hyundai keyfob V1 (slow OOK)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 109,   /* KIA_V1 */
    },
    {
        .name = "Kia V2", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 260, .te_max_us = 1260, .bits_min = 53, .bits_max = 66,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Kia/Hyundai keyfob V2 (two-burst upload)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 110,   /* KIA_V2 */
    },
    {
        .name = "Renault V0", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433_868,
        .te_min_us = 40, .te_max_us = 350, .bits_min = 82, .bits_max = 100,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Renault keyfob V0 (125 us OOK)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 112,   /* RENAULT_V0 */
    },
    {
        .name = "Kia V0", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        .te_min_us = 110, .te_max_us = 660, .bits_min = 61, .bits_max = 72,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Kia/Suzuki/Honda V0/Mitsubishi keyfob (61/64/72-bit)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 108,   /* KIA_V0 */
    },
    {
        .name = "Kia V7", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        .te_min_us = 110, .te_max_us = 660, .bits_min = 64, .bits_max = 72,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Kia/Hyundai keyfob V7 (FSK)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 111,   /* KIA_V7 */
    },
    {
        .name = "Honda V2", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        .te_min_us = 110, .te_max_us = 660, .bits_min = 81, .bits_max = 100,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Honda/Acura keyfob V2 (FSK, 81-bit frame)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 117,   /* HONDA_V2 */
    },
    {
        .name = "Ford V0", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 110, .te_max_us = 660, .bits_min = 64, .bits_max = 80,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Ford keyfob V0 (OOK, 3.5 ms mid-frame gap)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 105,   /* FORD_V0 */
    },
    {
        .name = "Mazda V0", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 110, .te_max_us = 660, .bits_min = 64, .bits_max = 80,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Mazda keyfob V0 (OOK)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 106,   /* MAZDA_V0 */
    },
    {
        .name = "Fiat V0", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 75, .te_max_us = 550, .bits_min = 64, .bits_max = 80,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Fiat keyfob V0 (OOK)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 114,   /* FIAT_V0 */
    },
    {
        .name = "Kia V3/V4", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        .te_min_us = 190, .te_max_us = 1050, .bits_min = 68, .bits_max = 84,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Kia/Hyundai keyfob V3/V4 (KeeLoq-family, FSK)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 120,   /* KIA_V3 (shared row for KIA_V4) */
    },
    {
        .name = "Kia V5", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        .te_min_us = 190, .te_max_us = 1050, .bits_min = 64, .bits_max = 72,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Kia/Hyundai keyfob V5 (FSK)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 122,   /* KIA_V5 */
    },
    {
        .name = "Kia V6", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        .te_min_us = 75, .te_max_us = 550, .bits_min = 144, .bits_max = 170,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Kia/Hyundai keyfob V6 (144-bit Manchester AES-128)",
        .security_note = "AES-128 rolling code — decode needs the keystore; not replayable",
        .registry_index = 129,   /* KIA_V6 */
    },
    {
        .name = "Ford V2", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        /* te_delta 260 exceeds te_short 200, so the low edge cannot carry the
         * full delta margin; 130 still clears Renault V1's 125 us element. */
        .te_min_us = 130, .te_max_us = 720, .bits_min = 104, .bits_max = 124,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Ford keyfob V2 (FSK, 104-bit frame)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 119,   /* FORD_V2 */
    },
    {
        .name = "Fiat V2", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 85, .te_max_us = 570, .bits_min = 112, .bits_max = 134,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Fiat/FCA keyfob V2 (Manchester 112-bit)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 124,   /* FIAT_V2 */
    },
    {
        .name = "Ford V3", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_UNKNOWN, .bands = RF_BAND_433,
        .te_min_us = 150, .te_max_us = 630, .bits_min = 104, .bits_max = 124,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Ford keyfob V3 (AM/FM, US/EU variants)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 127,   /* FORD_V3 */
    },
    {
        .name = "Renault V1", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_UNKNOWN, .bands = B_315_433_868,
        .te_min_us = 45, .te_max_us = 340, .bits_min = 104, .bits_max = 124,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Renault keyfob V1 (HITAG2 frame, AM/FM)",
        .security_note = "HITAG2 rolling code — M1 can brute-force the seed/IV",
        .registry_index = 125,   /* RENAULT_V1 */
    },
    {
        .name = "Fiat V1", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        /* Two timing variants: A = 250/500 us, B = 100/200 us; the window
         * covers both. */
        .te_min_us = 40, .te_max_us = 660, .bits_min = 102, .bits_max = 122,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Fiat keyfob V1 (Hitag2; 250/500 or 100/200 us cells)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 123,   /* FIAT_V1 */
    },
    {
        .name = "VAG", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = RF_BAND_433,
        .te_min_us = 135, .te_max_us = 790, .bits_min = 95, .bits_max = 112,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "VW/Audi/Skoda/Seat keyfob (T12, optional AUT64/TEA)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 128,   /* VAG */
    },
    {
        .name = "PSA", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_UNKNOWN, .bands = RF_BAND_433,
        .te_min_us = 110, .te_max_us = 660, .bits_min = 80, .bits_max = 96,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Peugeot/Citroen keyfob (AM/FM)",
        .security_note = "Rolling code — M1 implements the Direct-XOR decode path",
        .registry_index = 126,   /* PSA */
    },
    {
        .name = "Chrysler V0", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        /* Long marks are 3400/3700 us (variant A/B) — hence the wide top. */
        .te_min_us = 110, .te_max_us = 3900, .bits_min = 80, .bits_max = 96,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "Chrysler/Jeep/Dodge keyfob V0 (300/3400 us)",
        .security_note = "Rolling code — resists naive replay; decodable by M1",
        .registry_index = 113,   /* CHRYSLER_V0 */
    },

    /* ---- Automotive: generic catch-alls (keep AFTER the specific rows) - */
    {
        .name = "Car Key Fob (rolling)", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        .te_min_us = 100, .te_max_us = 500, .bits_min = 40, .bits_max = 0,
        .security = RF_SEC_ROLLING, .known_vuln = true,
        .device_note = "Keyless entry (KeeLoq etc.)",
        .security_note = "Rolling code; RollJam/relay capture attacks known",
        .registry_index = -1,
    },
    {
        .name = "Car Key Fob (fixed)", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 150, .te_max_us = 600, .bits_min = 24, .bits_max = 66,
        .security = RF_SEC_FIXED, .known_vuln = true,
        .device_note = "Older fixed-code fobs",
        .security_note = "Fixed code — directly replayable",
        .registry_index = -1,
    },
    {
        .name = "TPMS Tyre Sensor", .category = RF_CAT_AUTOMOTIVE,
        .mod = RF_MOD_FSK, .bands = B_315_433,
        .te_min_us = 40, .te_max_us = 130, .bits_min = 64, .bits_max = 0,
        .security = RF_SEC_FIXED, .known_vuln = false,
        .device_note = "Tyre pressure sensors (315/433)",
        .security_note = "Broadcasts ID+pressure in clear; trackable",
        .registry_index = -1,
    },

    /* ---- Home --------------------------------------------------------- */
    {
        .name = "Garage/Gate (fixed)", .category = RF_CAT_HOME,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 200, .te_max_us = 1200, .bits_min = 8, .bits_max = 40,
        .security = RF_SEC_FIXED, .known_vuln = true,
        .device_note = "DIP-switch / fixed remotes",
        .security_note = "Fixed code — directly replayable",
        .registry_index = -1,
    },
    {
        .name = "Garage/Gate (rolling)", .category = RF_CAT_HOME,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 200, .te_max_us = 900, .bits_min = 40, .bits_max = 0,
        .security = RF_SEC_ROLLING, .known_vuln = false,
        .device_note = "CAME/Nice/Somfy rolling remotes",
        .security_note = "Rolling code — single-capture replay defeated",
        .registry_index = -1,
    },
    {
        .name = "Wireless Doorbell", .category = RF_CAT_HOME,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 150, .te_max_us = 700, .bits_min = 16, .bits_max = 48,
        .security = RF_SEC_FIXED, .known_vuln = false,
        .device_note = "Doorbell chime transmitters",
        .security_note = "Fixed code — replayable (low impact)",
        .registry_index = -1,
    },
    {
        .name = "RF Mains Socket", .category = RF_CAT_HOME,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 150, .te_max_us = 600, .bits_min = 12, .bits_max = 32,
        .security = RF_SEC_FIXED, .known_vuln = true,
        .device_note = "Remote-switch mains sockets",
        .security_note = "Fixed code — directly replayable",
        .registry_index = -1,
    },

    /* ---- Security ----------------------------------------------------- */
    {
        .name = "PIR Motion Sensor", .category = RF_CAT_SECURITY,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 150, .te_max_us = 700, .bits_min = 16, .bits_max = 48,
        .security = RF_SEC_FIXED, .known_vuln = true,
        .device_note = "Alarm PIR / door-contact sensors",
        .security_note = "Often fixed code — spoofable/replayable",
        .registry_index = -1,
    },
    {
        .name = "Door/Window Contact", .category = RF_CAT_SECURITY,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 150, .te_max_us = 700, .bits_min = 16, .bits_max = 48,
        .security = RF_SEC_FIXED, .known_vuln = true,
        .device_note = "Alarm reed-switch sensors",
        .security_note = "Fixed code — jam/replay possible",
        .registry_index = -1,
    },
    {
        .name = "Alarm Keyfob", .category = RF_CAT_SECURITY,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 150, .te_max_us = 700, .bits_min = 24, .bits_max = 0,
        .security = RF_SEC_FIXED, .known_vuln = false,
        .device_note = "Panic buttons / arm-disarm fobs",
        .security_note = "Fixed code common — verify before trusting",
        .registry_index = -1,
    },

    /* ---- Weather ------------------------------------------------------ */
    {
        .name = "Weather Station", .category = RF_CAT_WEATHER,
        .mod = RF_MOD_OOK, .bands = B_315_433,
        .te_min_us = 400, .te_max_us = 2000, .bits_min = 24, .bits_max = 0,
        .security = RF_SEC_FIXED, .known_vuln = false,
        .device_note = "Temp/humidity outdoor sensors",
        .security_note = "Unauthenticated broadcast; spoofable readings",
        .registry_index = -1,
    },
    {
        .name = "Weather Station (868)", .category = RF_CAT_WEATHER,
        .mod = RF_MOD_FSK, .bands = RF_BAND_868,
        .te_min_us = 40, .te_max_us = 200, .bits_min = 24, .bits_max = 0,
        .security = RF_SEC_FIXED, .known_vuln = false,
        .device_note = "868 MHz weather sensors",
        .security_note = "Unauthenticated broadcast; spoofable readings",
        .registry_index = -1,
    },

    /* ---- IoT ---------------------------------------------------------- */
    {
        .name = "Z-Wave Node", .category = RF_CAT_IOT,
        .mod = RF_MOD_FSK, .bands = B_868_915,
        .te_min_us = 8, .te_max_us = 60, .bits_min = 80, .bits_max = 0,
        .security = RF_SEC_ENCRYPTED, .known_vuln = false,
        .device_note = "Z-Wave home-automation (868 EU/908 US)",
        .security_note = "S2 encryption on modern nodes; legacy S0 weaker",
        .registry_index = -1,
    },
    {
        .name = "LoRa Node", .category = RF_CAT_IOT,
        .mod = RF_MOD_FSK, .bands = B_868_915,
        .te_min_us = 0, .te_max_us = 0, .bits_min = 0, .bits_max = 0,
        .security = RF_SEC_ENCRYPTED, .known_vuln = false,
        .device_note = "LoRaWAN sensors (868 EU/915 US)",
        .security_note = "CSS chirp; AES payload — not OOK-replayable",
        .registry_index = -1,
    },

    /* ---- Utility ------------------------------------------------------ */
    {
        .name = "Smart Meter (AMR)", .category = RF_CAT_UTILITY,
        .mod = RF_MOD_FSK, .bands = B_868_915,
        .te_min_us = 8, .te_max_us = 60, .bits_min = 80, .bits_max = 0,
        .security = RF_SEC_FIXED, .known_vuln = false,
        .device_note = "Electric/gas/water AMR meters",
        .security_note = "Consumption broadcast; often unauthenticated",
        .registry_index = -1,
    },

    /* ---- Industrial --------------------------------------------------- */
    {
        .name = "Industrial Remote", .category = RF_CAT_INDUSTRIAL,
        .mod = RF_MOD_FSK, .bands = B_SUBGHZ,
        .te_min_us = 100, .te_max_us = 800, .bits_min = 24, .bits_max = 0,
        .security = RF_SEC_FIXED, .known_vuln = true,
        .device_note = "Crane / SCADA field remotes",
        .security_note = "Frequently fixed code — safety-critical replay risk",
        .registry_index = -1,
    },

    /* ---- 2.4 GHz (ESP32-C6 domains) ----------------------------------- */
    {
        .name = RF_SIG_NAME_BLE, .category = RF_CAT_CONSUMER,
        .mod = RF_MOD_FSK, .bands = RF_BAND_2400,
        .te_min_us = 0, .te_max_us = 0, .bits_min = 0, .bits_max = 0,
        .security = RF_SEC_UNKNOWN, .known_vuln = false,
        .device_note = "Bluetooth LE advertisers (phones, tags, beacons)",
        .security_note = "MAC may rotate; static MAC = trackable",
        .registry_index = -1,
    },
    {
        .name = RF_SIG_NAME_WIFI, .category = RF_CAT_CONSUMER,
        .mod = RF_MOD_FSK, .bands = RF_BAND_2400,
        .te_min_us = 0, .te_max_us = 0, .bits_min = 0, .bits_max = 0,
        .security = RF_SEC_ENCRYPTED, .known_vuln = false,
        .device_note = "802.11b/g/n access points & stations",
        .security_note = "WPA2/3 encrypts payload; mgmt frames are clear",
        .registry_index = -1,
    },
    {
        .name = RF_SIG_NAME_ZIGBEE, .category = RF_CAT_IOT,
        .mod = RF_MOD_FSK, .bands = RF_BAND_2400,
        .te_min_us = 0, .te_max_us = 0, .bits_min = 0, .bits_max = 0,
        .security = RF_SEC_ENCRYPTED, .known_vuln = false,
        .device_note = "Zigbee / Thread mesh devices",
        .security_note = "AES-CCM link encryption on commissioned networks",
        .registry_index = -1,
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
