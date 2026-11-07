/* See COPYING.txt for license details. */
#ifndef M1_RPC_ACTIONS_H_
#define M1_RPC_ACTIONS_H_

#include <stdbool.h>
#include <stdint.h>

static inline bool m1_rpc_esp_info_init_requested(uint16_t len,
                                                   const uint8_t *payload)
{
    return len > 0u && payload && payload[0] == 1u;
}

#endif /* M1_RPC_ACTIONS_H_ */
