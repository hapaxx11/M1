/* See COPYING.txt for license details. */

/**
 * @file   m1_esl_proto.h
 * @brief  ESL (Electronic Shelf Label) infrared protocol — frame builders.
 *
 * TagTinker (github.com/i12bp8/TagTinker) communicates with Pricer ESL tags
 * using IR at 4.33 MHz with Manchester encoding and a proprietary protocol
 * structure.  That carrier frequency is for the Flipper Zero (64 MHz CPU);
 * on the M1 Hapax (STM32H573VIT, 250 MHz, TIM1 APB2 at 75 MHz) the same
 * timer prescaler yields ~1.25 MHz — see m1_esl_ir.h for hardware details.
 *
 * Builds Pricer-compatible IR frames for ESL tags using the reverse-engineered
 * protocol documented by furrtek (github.com/furrtek/PrecIR) and implemented
 * in TagTinker (github.com/i12bp8/TagTinker).
 *
 * This module is pure logic — no hardware access, no RTOS calls.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/** Maximum frame size including 2-byte CRC trailer. */
#define M1_ESL_MAX_FRAME_SIZE  96U

/** Protocol byte for dot-matrix ESL tags. */
#define M1_ESL_PROTO_DM   0x85U

/** Protocol byte for segment ESL tags. */
#define M1_ESL_PROTO_SEG  0x84U

/**
 * @brief  Decode a 17-digit Pricer ESL barcode into a 4-byte PLID address.
 *
 * The barcode must be exactly 17 decimal digit characters (null-terminated).
 * Digits at positions 2–11 encode the physical layer ID.
 *
 * @param  barcode  17-character decimal string.
 * @param  plid     Output: 4-byte physical layer ID.
 * @retval true  Conversion succeeded.
 * @retval false Invalid barcode format.
 */
bool m1_esl_barcode_to_plid(const char *barcode, uint8_t plid[4]);

/**
 * @brief  Build a broadcast "flip page" frame.
 *
 * Addressed to all tags simultaneously (PLID = {0,0,0,0}).
 *
 * @param  buf       Output buffer (>= M1_ESL_MAX_FRAME_SIZE bytes).
 * @param  page      Display page to activate (0–7).
 * @param  forever   If true, the tag displays this page indefinitely.
 * @param  duration  Display duration when forever == false.
 * @return Number of bytes written to buf (includes 2-byte CRC).
 */
size_t m1_esl_build_broadcast_page_frame(
    uint8_t *buf, uint8_t page, bool forever, uint16_t duration);

/**
 * @brief  Build a broadcast diagnostic screen frame.
 *
 * Instructs all listening tags to display their internal diagnostic screen.
 *
 * @param  buf  Output buffer (>= M1_ESL_MAX_FRAME_SIZE bytes).
 * @return Number of bytes written to buf.
 */
size_t m1_esl_build_broadcast_debug_frame(uint8_t *buf);

/**
 * @brief  Build an addressed ping frame.
 *
 * Wakes a specific tag before issuing further addressed commands.
 * The tag is identified by its decoded PLID.
 *
 * @param  buf   Output buffer (>= M1_ESL_MAX_FRAME_SIZE bytes).
 * @param  plid  4-byte physical layer ID (from m1_esl_barcode_to_plid).
 * @return Number of bytes written to buf.
 */
size_t m1_esl_build_ping_frame(uint8_t *buf, const uint8_t plid[4]);
