/* See COPYING.txt for license details. */
#ifndef LFRFID_PROTOCOL_INSTA_FOB_H_
#define LFRFID_PROTOCOL_INSTA_FOB_H_

/* InstaFob (Hillman Group) protocol constants
 * Manchester modulation, RF/32, 64-bit data (8 decoded bytes).
 * Block 1 magic: 0x00107060.
 */
#define INSTAFOB_DECODED_DATA_SIZE    8
#define INSTAFOB_ENCODED_BITS         (8 * 4 * 7 + 1)  /* 225 bits */
#define INSTAFOB_ENCODED_BYTES        ((INSTAFOB_ENCODED_BITS + 7) / 8)  /* 29 */
#define INSTAFOB_DATA_OFFSET_BITS     7   /* block 1 starts at bit 7 */
#define INSTAFOB_BLOCK1               0x00107060UL
#define INSTAFOB_HALF_BIT_US          128  /* RF/32: 256us/bit, 128us half-bit */

extern const LFRFIDProtocolBase protocol_insta_fob;

#endif /* LFRFID_PROTOCOL_INSTA_FOB_H_ */
