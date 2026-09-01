/* See COPYING.txt for license details. */

/**
 * @file  m1_ir_custom.h
 * @brief Custom IR Remote builder — on-device learn, rename, delete.
 *
 * Port of dagnazty/M1_T-1000 Phase 3 (romulofer, commit 08cd5560).
 * Adapted to Hapax's blocking-delegate scene pattern and VKB/confirm API.
 *
 * Remotes are stored as standard Flipper .ir files under 0:/IR/Custom/.
 * Each file contains parsed IR signals (protocol/address/command).
 * The blocking entry point infrared_custom_remotes() is called from
 * m1_infrared_scene.c like other IR sub-features (Universal, Learn, Replay).
 */

#ifndef M1_IR_CUSTOM_H_
#define M1_IR_CUSTOM_H_

/**
 * @brief  Blocking entry point for the Custom IR Remote feature.
 *
 * Lists existing .ir files in 0:/IR/Custom/, lets the user create, open,
 * and manage custom remotes.  Returns when the user presses BACK from the
 * top-level list.
 */
void infrared_custom_remotes(void);

#endif /* M1_IR_CUSTOM_H_ */
