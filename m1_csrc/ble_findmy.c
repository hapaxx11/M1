/* See COPYING.txt for license details. */
/* Find My payload layout adapted from Evil-M5Project (MIT), which credits
 * OpenHaystack and biemster/FindMy for the underlying format research. */
#include "ble_findmy.h"

void ble_findmy_build(const uint8_t key[BLE_FINDMY_KEY_LEN],
                      uint8_t adv[BLE_FINDMY_ADV_LEN], uint8_t addr[6])
{
    if (!key || !adv || !addr) return;

    adv[0] = 0x1E;
    adv[1] = 0xFF;
    adv[2] = 0x4C;
    adv[3] = 0x00;
    adv[4] = 0x12;
    adv[5] = 0x19;
    adv[6] = 0x00;
    for (uint8_t i = 0; i < 22u; i++) adv[7 + i] = key[6 + i];
    adv[29] = key[0] >> 6u;
    adv[30] = 0x00;

    for (uint8_t i = 0; i < 6u; i++) addr[i] = key[i];
    addr[0] = (addr[0] & 0x3Fu) | 0xC0u;
}
