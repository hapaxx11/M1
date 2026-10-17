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

static inline m1_rpc_transport_t *m1_rpc_transport_reply_slot(
    bool rpc_task_context, m1_rpc_transport_t *parser_transport,
    m1_rpc_transport_t *rpc_task_transport)
{
    return rpc_task_context ? rpc_task_transport : parser_transport;
}

static inline uint16_t m1_rpc_transport_payload_limit(
    m1_rpc_transport_t transport, uint16_t default_limit,
    uint16_t esp32_limit)
{
    if (!m1_rpc_transport_valid(transport))
        return 0u;
    return transport == M1_RPC_TRANSPORT_ESP32 ? esp32_limit : default_limit;
}

static inline bool m1_rpc_transport_payload_too_large(
    m1_rpc_transport_t transport, uint16_t len, uint16_t default_limit,
    uint16_t esp32_limit)
{
    uint16_t limit = m1_rpc_transport_payload_limit(
        transport, default_limit, esp32_limit);
    return limit == 0u || len > limit;
}

static inline bool m1_rpc_transport_file_write_start_allowed(
    bool upload_active, m1_rpc_transport_t upload_transport,
    m1_rpc_transport_t incoming_transport)
{
    return m1_rpc_transport_valid(incoming_transport) &&
           (!upload_active || upload_transport == incoming_transport);
}

static inline bool m1_rpc_transport_file_write_matches(
    bool upload_active, m1_rpc_transport_t upload_transport,
    m1_rpc_transport_t incoming_transport)
{
    return upload_active && m1_rpc_transport_valid(incoming_transport) &&
           upload_transport == incoming_transport;
}

static inline bool m1_rpc_transport_file_write_is_usb(
    bool upload_active, m1_rpc_transport_t upload_transport)
{
    return upload_active && upload_transport == M1_RPC_TRANSPORT_USB;
}

static inline uint32_t m1_rpc_poll_backoff_ms(uint8_t failures)
{
    uint32_t delay_ms = 500u;
    while (failures > 0u && delay_ms < 5000u)
    {
        delay_ms *= 2u;
        failures--;
    }
    return delay_ms > 5000u ? 5000u : delay_ms;
}

static inline bool m1_rpc_poll_deadline_due(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

#endif /* M1_RPC_TRANSPORT_H_ */
