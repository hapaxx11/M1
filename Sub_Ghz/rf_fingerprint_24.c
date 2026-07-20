/* See COPYING.txt for license details. */

/*
 * rf_fingerprint_24.c
 *
 * Implementation of the ESP32-C6 2.4 GHz fingerprint backend.  See
 * rf_fingerprint_24.h for the rationale.  Pure logic, zero hardware deps.
 *
 * M1 Project — Hapax fork
 */

#include "rf_fingerprint_24.h"
#include "rf_protocol_db.h"

#include <string.h>

/*============================================================================*/
/* Capability mapping                                                         */
/*============================================================================*/

rf24_cap_t rf_sensor_24_required_cap(rf_sensor_t sensor)
{
    switch (sensor)
    {
        case RF_SENSOR_BLE:    return RF24_CAP_BLE_SCAN;
        case RF_SENSOR_WIFI:   return RF24_CAP_WIFI_SCAN;
        case RF_SENSOR_802154: return RF24_CAP_802154;
        default:               return RF24_CAP_NONE;
    }
}

/*============================================================================*/
/* Channel -> centre-frequency helpers                                        */
/*============================================================================*/

uint32_t rf_wifi_channel_to_freq(uint8_t channel)
{
    if (channel == 14U)
        return 2484000000U;
    if (channel >= 1U && channel <= 13U)
        return 2412000000U + (uint32_t)(channel - 1U) * 5000000U;
    return 0U;
}

uint32_t rf_ble_channel_to_freq(uint8_t channel)
{
    /* Advertising channels sit at the band edges and centre. */
    switch (channel)
    {
        case 37U: return 2402000000U;
        case 38U: return 2426000000U;
        case 39U: return 2480000000U;
        default:  break;
    }
    /* Data channels 0..36 fill 2404..2478 MHz, skipping the advertising
     * channels: 0..10 -> 2404..2424, 11..36 -> 2428..2478. */
    if (channel <= 10U)
        return 2404000000U + (uint32_t)channel * 2000000U;
    if (channel <= 36U)
        return 2428000000U + (uint32_t)(channel - 11U) * 2000000U;
    return 0U;
}

uint32_t rf_802154_channel_to_freq(uint8_t channel)
{
    if (channel >= 11U && channel <= 26U)
        return 2405000000U + (uint32_t)(channel - 11U) * 5000000U;
    return 0U;
}

/*============================================================================*/
/* Fingerprint extraction                                                     */
/*============================================================================*/

/*
 * Fill the fields common to every 2.4 GHz detection.  The 2.4 GHz radios all
 * use a constant-envelope digital modulation (GFSK for BLE/WiFi-DSSS front
 * end, O-QPSK for 802.15.4), which the shared vocabulary buckets as the FSK
 * family — matching the RF_MOD_FSK signatures in the database.  There is no
 * pulse-timing or payload-size measurement, so te_us/est_bits stay 0; that is
 * the data-poor case rf_match_score() handles by scoring only band+mod.
 */
static void fill_common(rf_sensor_t sensor,
                        uint32_t    freq_hz,
                        int16_t     rssi_dbm,
                        rf_fingerprint_t *out)
{
    memset(out, 0, sizeof(*out));
    out->sensor         = sensor;
    out->freq_hz        = freq_hz;
    out->band           = RF_BAND_2400;
    out->mod            = RF_MOD_FSK;
    out->mod_confidence = 3U;         /* modulation family is definitional */
    out->repetition     = 1U;
    out->rssi_dbm       = rssi_dbm;
}

void rf_fingerprint_from_ble(uint8_t  adv_channel,
                             int16_t  rssi_dbm,
                             rf_fingerprint_t *out)
{
    if (out == NULL)
        return;
    fill_common(RF_SENSOR_BLE, rf_ble_channel_to_freq(adv_channel),
                rssi_dbm, out);
}

void rf_fingerprint_from_wifi(uint8_t  channel,
                              int16_t  rssi_dbm,
                              rf_fingerprint_t *out)
{
    if (out == NULL)
        return;
    fill_common(RF_SENSOR_WIFI, rf_wifi_channel_to_freq(channel),
                rssi_dbm, out);
}

void rf_fingerprint_from_802154(uint8_t  channel,
                                int16_t  rssi_dbm,
                                rf_fingerprint_t *out)
{
    if (out == NULL)
        return;
    fill_common(RF_SENSOR_802154, rf_802154_channel_to_freq(channel),
                rssi_dbm, out);
}

/*============================================================================*/
/* Identification                                                             */
/*============================================================================*/

rf_match_result_t rf_match_24(const rf_fingerprint_t *fp)
{
    rf_match_result_t r;
    r.index      = -1;
    r.confidence = 0U;
    r.sig        = NULL;

    if (fp == NULL)
        return r;

    int idx = rf_protocol_db_find_2400(fp->sensor);
    if (idx < 0)
        return r;

    const rf_protocol_sig_t *sig = rf_protocol_db_get((uint16_t)idx);
    if (sig == NULL)
        return r;

    uint8_t s = rf_match_score(fp, sig);
    r.confidence = s;               /* keep score even if below the floor */
    if (s < RF_MATCH_MIN_CONFIDENCE)
        return r;                   /* honest "unidentified" */

    r.index = idx;
    r.sig   = sig;
    return r;
}
