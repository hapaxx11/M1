/* See COPYING.txt for license details. */

#ifndef M1_ESP32_RPC_HTTP_H_
#define M1_ESP32_RPC_HTTP_H_

#include "m1_http_client.h"

http_status_t m1_esp32_rpc_http_get(const char *url, char *response_buf,
                                    uint16_t buf_size, uint8_t timeout_sec);
http_status_t m1_esp32_rpc_http_download_to_file(const char *url, const char *sd_path,
                                                 http_progress_cb_t progress_cb,
                                                 uint8_t timeout_sec);

#endif
