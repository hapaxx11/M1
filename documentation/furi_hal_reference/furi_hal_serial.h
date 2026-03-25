/**
 * @file furi_hal_serial.h
 * Serial HAL API
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <furi_hal_serial_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FuriHalSerialHwFlowControlNone,
    FuriHalSerialHwFlowControlRts,
    FuriHalSerialHwFlowControlCts,
    FuriHalSerialHwFlowControlRtsCts,
} FuriHalSerialHwFlowControl;

typedef enum {
    FuriHalSerialRxEventData = (1 << 0),
    FuriHalSerialRxEventIdle = (1 << 1),
    FuriHalSerialRxEventFrameError = (1 << 2),
    FuriHalSerialRxEventBreakError = (1 << 3),
    FuriHalSerialRxEventParityError = (1 << 4),
    FuriHalSerialRxEventOverrunError = (1 << 5),
} FuriHalSerialRxEvent;

typedef enum {
    FuriHalSerialTxEventComplete = (1 << 0),
} FuriHalSerialTxEvent;

typedef void (*FuriHalSerialRxCallback)(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context);
typedef void (*FuriHalSerialTxCallback)(FuriHalSerialHandle* handle, FuriHalSerialTxEvent event, void* context);

void furi_hal_serial_init(FuriHalSerialHandle* handle, uint32_t baud_rate);
void furi_hal_serial_deinit(FuriHalSerialHandle* handle);
void furi_hal_serial_suspend(FuriHalSerialHandle* handle);
void furi_hal_serial_resume(FuriHalSerialHandle* handle);
bool furi_hal_serial_is_baud_rate_supported(FuriHalSerialHandle* handle, uint32_t baud_rate);
uint32_t furi_hal_serial_get_baud_rate(FuriHalSerialHandle* handle);
void furi_hal_serial_set_baud_rate(FuriHalSerialHandle* handle, uint32_t baud_rate);
void furi_hal_serial_set_hw_flow_control(FuriHalSerialHandle* handle, FuriHalSerialHwFlowControl flow_control);
void furi_hal_serial_set_callback(FuriHalSerialHandle* handle, FuriHalSerialTxCallback tx_callback, FuriHalSerialRxCallback rx_callback, void* context);
size_t furi_hal_serial_tx(FuriHalSerialHandle* handle, const uint8_t* buffer, size_t buffer_size, uint32_t timeout);
bool furi_hal_serial_tx_wait_complete(FuriHalSerialHandle* handle, uint32_t timeout);
bool furi_hal_serial_rx_available(FuriHalSerialHandle* handle);
uint8_t furi_hal_serial_rx(FuriHalSerialHandle* handle);
size_t furi_hal_serial_rx_data_non_blocking(FuriHalSerialHandle* handle, uint8_t* data, size_t data_size);
void furi_hal_serial_async_rx_start(FuriHalSerialHandle* handle, bool report_errors);
void furi_hal_serial_async_rx_stop(FuriHalSerialHandle* handle);
void furi_hal_serial_dma_tx(FuriHalSerialHandle* handle, const uint8_t* buffer, size_t buffer_size);
void furi_hal_serial_dma_rx_start(FuriHalSerialHandle* handle, uint8_t* buffer, size_t buffer_size);
void furi_hal_serial_dma_rx_stop(FuriHalSerialHandle* handle);
void furi_hal_serial_clear(FuriHalSerialHandle* handle);
void furi_hal_serial_set_config(FuriHalSerialHandle* handle, FuriHalSerialConfigDataBits data_bits, FuriHalSerialConfigParity parity, FuriHalSerialConfigStopBits stop_bits);

#ifdef __cplusplus
}
#endif
