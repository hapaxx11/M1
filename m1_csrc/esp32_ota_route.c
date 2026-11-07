/* See COPYING.txt for license details. */
#include "esp32_ota_route.h"
#include "m1_esp32_caps.h"
#include <string.h>

bool esp32_ota_image_is_app(const char *filename)
{
    if (!filename) return false;
    const char *base = strrchr(filename, '/');
    if (!base) base = strrchr(filename, '\\');
    base = base ? base + 1 : filename;
    return !strncmp(base, "app_", 4) || !strcmp(base, "m1-esp32-brain.bin");
}

bool esp32_ota_use_rpc(const char *filename, uint64_t cap_bitmap)
{
    return esp32_ota_image_is_app(filename) &&
           (cap_bitmap & M1_ESP32_CAP_OTA) != 0;
}

bool esp32_ota_end_requests_reboot(void)
{
    return false;
}

bool esp32_ota_uart_reset_required(bool use_rpc_ota)
{
    return !use_rpc_ota;
}

bool esp32_ota_relay_allowed(bool firmware_update)
{
    return !firmware_update;
}

bool esp32_ota_legacy_task_required(bool use_rpc_ota)
{
    return !use_rpc_ota;
}

bool esp32_ota_reset_via_rpc(bool use_rpc_ota)
{
    return use_rpc_ota;
}
