/* See COPYING.txt for license details. */

/**
 * @file   espnow_caps_decide.h
 * @brief  Pure decision logic for ESP-NOW capability availability.
 *
 * The canonical CD3 wire header does not assign a self-report bit for ESP-NOW
 * (M1_ESP32_CAP_ESPNOW is a host-only bit above the wire range), so on shipped
 * brain firmware the feature gate fails closed even though the brain implements
 * the M1_RPC_NOW_* handlers (documentation/esp32_firmware.md).
 *
 * This header centralises the *decision* — "should the ESP-NOW peer-link menu
 * be enabled?" — as a pure function so it can be host-tested independently of
 * the capability probe that gathers its inputs.  The rule is:
 *
 *   ESP-NOW is available IF the firmware self-reports the ESPNOW bit,
 *   OR the transport is the native brain RPC transport AND a runtime
 *   NOW_START probe succeeded.
 *
 * This lets Hapax light up ESP-NOW on brain firmware that answers the RPC
 * handlers without waiting for that firmware to add the self-report bit, while
 * still failing closed on AT / SiN360 / unknown transports.
 *
 * Pure header — no HAL/RTOS/display dependencies.  Host-testable.
 *
 * M1 Project
 */

#ifndef ESPNOW_CAPS_DECIDE_H_
#define ESPNOW_CAPS_DECIDE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Transport classes relevant to the ESP-NOW decision.
 *
 * Mirrors the meaningful cases of esp32_transport_t without depending on that
 * header, so this decision stays pure and independently testable.
 */
typedef enum {
    ESPNOW_DECIDE_TRANSPORT_NONE = 0, /**< Not probed / no ESP32. */
    ESPNOW_DECIDE_TRANSPORT_AT,       /**< AT-command firmware. */
    ESPNOW_DECIDE_TRANSPORT_BINARY,   /**< SiN360 binary-SPI firmware. */
    ESPNOW_DECIDE_TRANSPORT_RPC,      /**< Native brain M1_RPC firmware. */
} espnow_decide_transport_t;

/**
 * @brief  Decide whether the ESP-NOW peer link should be available.
 *
 * @param  self_reported  True if the firmware advertised the ESPNOW cap bit.
 * @param  transport      Resolved wire transport.
 * @param  now_probe_ok   True if a runtime NOW_START probe succeeded (only
 *                        meaningful on the RPC transport; ignored otherwise).
 * @return true if the feature gate should open.
 */
static inline bool espnow_caps_available(bool self_reported,
                                         espnow_decide_transport_t transport,
                                         bool now_probe_ok)
{
    if (self_reported)
        return true;
    if (transport == ESPNOW_DECIDE_TRANSPORT_RPC && now_probe_ok)
        return true;
    return false;
}

#ifdef __cplusplus
}
#endif

#endif /* ESPNOW_CAPS_DECIDE_H_ */
