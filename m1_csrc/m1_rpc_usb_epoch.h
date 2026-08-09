/* See COPYING.txt for license details. */

#ifndef M1_RPC_USB_EPOCH_H_
#define M1_RPC_USB_EPOCH_H_

#include <stdint.h>
#include <stdbool.h>

/* A host-side USB bus reset or disconnect (re-enumeration of qMonstatek
 * without an MCU reboot) ends the previous RPC session even though nothing
 * on the device restarted. HAL_PCD_ResetCallback()/DisconnectCallback() bump
 * a monotonically increasing "session epoch" counter from ISR context; the
 * receive parser and rpc_task consume it (from task context, since parsing
 * and FatFs access are not ISR-safe) to drop STALE old-session state — a
 * mid-frame parse, a partial file write, and any deferred command left from
 * the previous session — without dropping work that legitimately arrived in
 * the CURRENT session. */

/* Pure decision: has the session epoch advanced since *last_seen_epoch was
 * last consumed? If so, updates *last_seen_epoch to session_epoch and
 * returns true (caller must perform the epoch-boundary cleanup once). Used
 * identically by the receive parser (per-byte state) and rpc_task (deferred
 * command / partial file write) to consume the same epoch counter
 * independently, each on its own cadence. */
static inline bool m1_rpc_usb_epoch_advanced(uint32_t *last_seen_epoch,
                                              uint32_t session_epoch)
{
    if (*last_seen_epoch == session_epoch)
        return false;

    *last_seen_epoch = session_epoch;
    return true;
}

/* Cleanup action decided for rpc_task's deferred-command/file-write state at
 * a consumed epoch boundary. */
typedef struct
{
    bool discard_deferred;   /* NACK + drop the pending deferred USB command */
    bool close_file_write;   /* close the open partial FILE_WRITE_* transfer */
} S_RpcUsbEpochAction;

/* Pure decision: given that the epoch boundary has just been consumed
 * (m1_rpc_usb_epoch_advanced() returned true), decide what stale state must
 * be cleaned up.
 *
 * A deferred command is stale only when it is USB-sourced AND it was tagged
 * with an OLDER epoch than the one just consumed — a command that arrived
 * in the brand-new session (deferred_epoch == session_epoch) is real work
 * and must be left alone, not silently dropped.
 *
 * A partial file write is always considered stale at a session boundary:
 * the previous host connection is gone, so nothing will ever send the
 * FILE_WRITE_DATA/FINISH to complete it. */
static inline S_RpcUsbEpochAction m1_rpc_usb_epoch_check(uint32_t session_epoch,
                                                          bool deferred_pending,
                                                          bool deferred_is_usb,
                                                          uint32_t deferred_epoch,
                                                          bool file_write_active)
{
    S_RpcUsbEpochAction action = { false, false };

    if (deferred_pending && deferred_is_usb && deferred_epoch != session_epoch)
        action.discard_deferred = true;

    if (file_write_active)
        action.close_file_write = true;

    return action;
}

#endif /* M1_RPC_USB_EPOCH_H_ */
