/* See COPYING.txt for license details. */

#ifndef M1_RPC_TRANSPORT_H_
#define M1_RPC_TRANSPORT_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    M1_RPC_TRANSPORT_USB = 0,
    M1_RPC_TRANSPORT_BLE,
    M1_RPC_TRANSPORT_WIFI,
    M1_RPC_TRANSPORT_COUNT,
} m1_rpc_transport_t;

#define M1_RPC_TRANSPORT_BIT(transport) (1u << (transport))

static inline bool m1_rpc_transport_valid(m1_rpc_transport_t transport)
{
    return transport >= M1_RPC_TRANSPORT_USB &&
           transport < M1_RPC_TRANSPORT_COUNT;
}

static inline bool m1_rpc_transport_can_send(m1_rpc_transport_t transport,
                                             uint32_t registered_mask)
{
    return m1_rpc_transport_valid(transport) &&
           (registered_mask & M1_RPC_TRANSPORT_BIT(transport)) != 0u;
}

static inline bool m1_rpc_transport_feed_allowed(
    bool partial_frame, m1_rpc_transport_t parser_transport,
    m1_rpc_transport_t incoming_transport)
{
    if (!m1_rpc_transport_valid(incoming_transport))
        return false;
    return !partial_frame || parser_transport == incoming_transport;
}

#endif /* M1_RPC_TRANSPORT_H_ */
