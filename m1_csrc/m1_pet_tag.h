/* See COPYING.txt for license details. */

/**
 * @file   m1_pet_tag.h
 * @brief  Pet Tag Scanner — pure-logic decoder for ISO 11784/11785 FDX-B
 *         animal microchips.
 *
 * Decodes the 11-byte FDX-B payload produced by the LF-RFID FDX-B protocol
 * decoder (see lfrfid_protocol_fdx_b.c) into a human-readable pet-chip view:
 * the 15-digit ID (country + national code), the country code, and the
 * animal-application flag.
 *
 * The extraction follows the ISO 11784 bit layout (fields are transmitted
 * LSB-first) and mirrors the reference Flipper Zero FDX-B field decoding.
 * This module is pure logic with no hardware dependencies so it can be
 * exercised by the host-side unit tests.
 */

#ifndef M1_PET_TAG_H_
#define M1_PET_TAG_H_

#include <stdint.h>
#include <stdbool.h>

/** Decoded pet-chip information. */
typedef struct {
    uint16_t country_code;   /**< 10-bit ISO 3166 country / manufacturer code */
    uint64_t national_code;  /**< 38-bit national identification code          */
    bool     animal;         /**< true if the animal-application flag is set   */
    bool     block_status;   /**< true if an extra data block is present       */
    char     id_string[20];  /**< formatted "CCC-NNNNNNNNNNNN"                 */
} m1_pet_tag_info_t;

/**
 * Decode an FDX-B decoded payload into pet-chip fields.
 *
 * @param data  Pointer to the 11-byte FDX-B decoded payload
 *              (as returned by protocol_get_data() for LFRFIDProtocolFDX_B).
 * @param out   Output structure to populate.
 * @return true on success, false if @p data or @p out is NULL.
 */
bool m1_pet_tag_decode_fdxb(const uint8_t *data, m1_pet_tag_info_t *out);

#endif /* M1_PET_TAG_H_ */
