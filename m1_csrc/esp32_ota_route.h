/* See COPYING.txt for license details. */
#ifndef ESP32_OTA_ROUTE_H_
#define ESP32_OTA_ROUTE_H_

#include <stdbool.h>
#include <stdint.h>

bool esp32_ota_image_is_app(const char *filename);
bool esp32_ota_use_rpc(const char *filename, uint64_t cap_bitmap);
bool esp32_ota_end_requests_reboot(void);
bool esp32_ota_uart_reset_required(bool use_rpc_ota);
bool esp32_ota_relay_allowed(bool firmware_update);
bool esp32_ota_legacy_task_required(bool use_rpc_ota);
bool esp32_ota_reset_via_rpc(bool use_rpc_ota);

#endif /* ESP32_OTA_ROUTE_H_ */
