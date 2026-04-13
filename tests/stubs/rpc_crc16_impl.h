/* See COPYING.txt for license details. */

/*
 * rpc_crc16_impl.h — host-testable interface for RPC CRC-16 functions.
 */

#ifndef RPC_CRC16_IMPL_H_
#define RPC_CRC16_IMPL_H_

#include <stdint.h>

/**
 * @brief  Compute CRC-16/CCITT over a byte buffer.
 *         Polynomial: 0x1021, Initial value: 0xFFFF.
 */
uint16_t rpc_crc16(const uint8_t *data, uint16_t len);

/**
 * @brief  Continue CRC-16/CCITT computation with a running CRC value.
 */
uint16_t rpc_crc16_continue(uint16_t crc, const uint8_t *data, uint16_t len);

#endif /* RPC_CRC16_IMPL_H_ */
