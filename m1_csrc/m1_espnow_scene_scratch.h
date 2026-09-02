/* See COPYING.txt for license details. */

/**
 * @file   m1_espnow_scene_scratch.h
 * @brief  Shared scratch storage for mutually-exclusive Peer Link scenes.
 *
 * The "Messages", "Send Capture", "Receive Transfer", and "Remote Trigger"
 * screens are alternate top-level Peer Link menu destinations: only one of
 * them is ever the active/foreground scene at a time (their poll/on_enter
 * callbacks only run while their own scene is on screen), and each fully
 * resets its own state (memset/init) on entry before using it.  They can
 * therefore safely share one physical backing buffer instead of each
 * reserving its own permanently resident (.bss) copy — important on this
 * tightly RAM-constrained target.
 *
 * M1 Project
 */

#ifndef M1_ESPNOW_SCENE_SCRATCH_H_
#define M1_ESPNOW_SCENE_SCRATCH_H_

#include "espnow_message.h"
#include "espnow_file_transfer.h"
#include "espnow_trigger.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef union {
    espnow_inbox_t       messages_inbox;   /**< m1_espnow_scene_messages.c */
    /** Shared by m1_espnow_capture_share.c (send) and
     *  m1_espnow_scene_transfer.c (receive) — never active at once. */
    espnow_ft_ctx_t      file_transfer_ctx;
    espnow_trigger_ctx_t trigger_ctx;      /**< m1_espnow_scene_trigger.c */
} m1_espnow_scene_scratch_t;

extern m1_espnow_scene_scratch_t g_m1_espnow_scene_scratch;

#ifdef __cplusplus
}
#endif

#endif /* M1_ESPNOW_SCENE_SCRATCH_H_ */
