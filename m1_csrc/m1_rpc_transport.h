/* See COPYING.txt for license details. */

#ifndef M1_RPC_TRANSPORT_H_
#define M1_RPC_TRANSPORT_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    M1_RPC_TRANSPORT_USB = 0,
    M1_RPC_TRANSPORT_BLE,
    M1_RPC_TRANSPORT_WIFI,
    M1_RPC_TRANSPORT_ESP32,
    M1_RPC_TRANSPORT_COUNT,
} m1_rpc_transport_t;

static inline bool m1_rpc_transport_valid(m1_rpc_transport_t transport)
{
    return transport >= M1_RPC_TRANSPORT_USB &&
           transport < M1_RPC_TRANSPORT_COUNT;
}

static inline m1_rpc_transport_t m1_rpc_transport_reply_to_request(
    m1_rpc_transport_t request_transport)
{
    return m1_rpc_transport_valid(request_transport)
               ? request_transport
               : M1_RPC_TRANSPORT_USB;
}

static inline bool m1_rpc_transport_feed_allowed(
    bool partial_frame, m1_rpc_transport_t parser_transport,
    m1_rpc_transport_t incoming_transport)
{
    if (!m1_rpc_transport_valid(incoming_transport))
        return false;
    return !partial_frame || parser_transport == incoming_transport;
}

static inline bool m1_rpc_transport_parser_reset_on_usb_epoch(
    bool epoch_advanced, bool partial_frame,
    m1_rpc_transport_t parser_transport)
{
    return epoch_advanced && partial_frame &&
           parser_transport == M1_RPC_TRANSPORT_USB;
}

static inline bool m1_rpc_transport_upload_allowed(
    bool upload_active, m1_rpc_transport_t upload_transport,
    m1_rpc_transport_t request_transport)
{
    return !upload_active || upload_transport == request_transport;
}

static inline bool m1_rpc_transport_response_too_large(
    m1_rpc_transport_t transport, uint16_t payload_len,
    uint16_t rpc_max_payload, uint16_t relay_frame_payload_max,
    uint16_t rpc_frame_overhead)
{
    if (!m1_rpc_transport_valid(transport))
        return true;
    if (transport != M1_RPC_TRANSPORT_ESP32)
        return payload_len > rpc_max_payload;
    return relay_frame_payload_max < rpc_frame_overhead ||
           payload_len > (uint16_t)(relay_frame_payload_max -
                                    rpc_frame_overhead);
}

static inline bool m1_rpc_poll_retry_due(uint32_t now, uint32_t deadline,
                                         bool retry_waiting)
{
    return !retry_waiting || (int32_t)(now - deadline) >= 0;
}

#endif /* M1_RPC_TRANSPORT_H_ */
