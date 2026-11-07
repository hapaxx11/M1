/* See COPYING.txt for license details. */
#ifndef BLE_FINDMY_H_
#define BLE_FINDMY_H_

#include <stdint.h>

#define BLE_FINDMY_KEY_LEN   28u
#define BLE_FINDMY_ADV_LEN   31u

/* Build a Find My offline-finding advertisement and its key-derived
 * random-static address. Key material is supplied by the caller; laboratory
 * random keys are not asserted to be valid P-224 points or trackable tags. */
void ble_findmy_build(const uint8_t key[BLE_FINDMY_KEY_LEN],
                      uint8_t adv[BLE_FINDMY_ADV_LEN], uint8_t addr[6]);

#endif /* BLE_FINDMY_H_ */
