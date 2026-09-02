/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_trigger_exec.h
 * @brief  Hardware-facing execution adapter for ESP-NOW remote trigger.
 */

#ifndef M1_ESPNOW_TRIGGER_EXEC_H_
#define M1_ESPNOW_TRIGGER_EXEC_H_

#include <stdbool.h>

#include "espnow_trigger.h"

#ifdef __cplusplus
extern "C" {
#endif

bool m1_espnow_trigger_capture_exists(espnow_share_kind_t kind,
                                      const char *name);
bool m1_espnow_trigger_execute(espnow_share_kind_t kind, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* M1_ESPNOW_TRIGGER_EXEC_H_ */
