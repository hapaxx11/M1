/* See COPYING.txt for license details. */
#ifndef LFRFID_PROTOCOL_INDALA224_H_
#define LFRFID_PROTOCOL_INDALA224_H_

/* Indala 224-bit PSK2 protocol constants
 * PSK2 modulation, RF/32 (255us/bit), 224-bit encoded frame.
 * Preamble: 1 followed by 29 zeros (30 bits).
 * Second frame may have same or inverted preamble (PSK2 phase alternation).
 */
#define INDALA224_PREAMBLE_BIT_SIZE   30
#define INDALA224_ENCODED_BIT_SIZE    224
#define INDALA224_ENCODED_DATA_SIZE   32   /* 224/8 + 4 bytes spare = 32 */
#define INDALA224_DECODED_DATA_SIZE   28   /* 224 bits = 28 bytes */
#define INDALA224_US_PER_BIT          255

extern const LFRFIDProtocolBase protocol_indala224;

#endif /* LFRFID_PROTOCOL_INDALA224_H_ */
