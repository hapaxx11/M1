/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_capture_share.h
 * @brief  Firmware-side ESP-NOW saved-capture send flow.
 */

#ifndef M1_ESPNOW_CAPTURE_SHARE_H_
#define M1_ESPNOW_CAPTURE_SHARE_H_

#include <stdbool.h>

#include "espnow_shareable.h"

#ifdef __cplusplus
extern "C" {
#endif

bool m1_espnow_capture_share_choose_and_send(espnow_share_kind_t kind);
bool m1_espnow_capture_share_send_path(const char *path);
bool m1_espnow_capture_share_choose_and_begin(espnow_share_kind_t kind);
bool m1_espnow_capture_share_step(void);
void m1_espnow_capture_share_cancel(void);
bool m1_espnow_capture_share_active(void);
const char *m1_espnow_capture_share_status(void);

#ifdef __cplusplus
}
#endif

#endif /* M1_ESPNOW_CAPTURE_SHARE_H_ */
