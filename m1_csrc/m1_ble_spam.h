/* See COPYING.txt for license details. */

/*
 * m1_ble_spam.h
 *
 * BLE Beacon Spam — dynamically generates advertising payloads that
 * trigger pairing / proximity notifications on nearby devices.
 *
 * Packet formats ported from the Flipper Zero / Momentum ble_spam app
 * (credit: @Willy-JL, @ECTO-1A, @Spooks4576) via the GhostESP
 * reference implementation.
 *
 * Uses standard ESP32-C6 AT advertising commands — no ESP32 firmware
 * changes required.
 *
 * Supported spam modes:
 *   - Apple     (Continuity: ProximityPair, NearbyAction, CustomCrash)
 *   - Samsung   (EasySetup: Buds + Watches)
 *   - Google    (Fast Pair — 80+ model IDs)
 *   - Microsoft (Swift Pair — random device names)
 *   - Random    (mix of all the above)
 */

#ifndef M1_BLE_SPAM_H_
#define M1_BLE_SPAM_H_

#include <stdint.h>
#include <stdbool.h>

/* Spam mode selection */
typedef enum {
    BLE_SPAM_APPLE,
    BLE_SPAM_SAMSUNG,
    BLE_SPAM_GOOGLE,
    BLE_SPAM_MICROSOFT,
    BLE_SPAM_RANDOM,
    BLE_SPAM_MODE_COUNT
} ble_spam_mode_t;

/* Menu entry point */
void ble_spam_run(void);

#endif /* M1_BLE_SPAM_H_ */
