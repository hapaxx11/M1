/* See COPYING.txt for license details. */

/*
 * mfc_crypto1.c - Software MIFARE Classic Crypto-1 cipher
 *
 * Implements the Crypto-1 48-bit LFSR stream cipher used by MIFARE Classic
 * cards for authentication and encrypted I/O. The ST25R3916 has no hardware
 * Crypto-1, so this is all in software.
 *
 * The cipher core (filter, LFSR, PRNG) is the canonical Crapto-1 implementation
 * (bla / Karsten Nohl et al.; same lineage as mfkey32.c). It is host-validated:
 * encrypting a nonce pair with this core and recovering the key via mfkey32v2
 * returns the original key.
 *
 * NOTE: authenticated MIFARE frames also require ENCRYPTED parity. mfc_auth()
 * and mfc_read_block_crypto() below currently transmit with standard parity,
 * which the card rejects — that is a separate fix (encrypted-parity framing).
 */

#include <string.h>
#include "mfc_crypto1.h"
#include "rfal_rf.h"
#include "rfal_nfc.h"
#include "logger.h"

/* ========================================================================== */
/* Canonical Crypto-1 cipher core (Crapto-1 lineage)                         */
/* ========================================================================== */

#define LF_POLY_ODD  (0x29CE5CU)
#define LF_POLY_EVEN (0x870804U)
#define BIT(x, n)    ((uint32_t)((x) >> (n)) & 1U)
#define BEBIT(x, n)  BIT(x, (n) ^ 24)
#define SWAPENDIAN(x) \
    ((x) = ((x) >> 8 & 0xff00ff) | ((x) & 0xff00ff) << 8, (x) = (x) >> 16 | (x) << 16)

/* Nonlinear filter function lookup tables (canonical Crapto-1). */
static const uint8_t lookup1[256] = {
    0, 0,  16, 16, 0,  16, 0,  0,  0, 16, 0,  0,  16, 16, 16, 16, 0, 0,  16, 16, 0,  16, 0,  0,
    0, 16, 0,  0,  16, 16, 16, 16, 0, 0,  16, 16, 0,  16, 0,  0,  0, 16, 0,  0,  16, 16, 16, 16,
    8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24, 8, 8,  24, 24, 8,  24, 8,  8,
    8, 24, 8,  8,  24, 24, 24, 24, 8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24,
    0, 0,  16, 16, 0,  16, 0,  0,  0, 16, 0,  0,  16, 16, 16, 16, 0, 0,  16, 16, 0,  16, 0,  0,
    0, 16, 0,  0,  16, 16, 16, 16, 8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24,
    0, 0,  16, 16, 0,  16, 0,  0,  0, 16, 0,  0,  16, 16, 16, 16, 0, 0,  16, 16, 0,  16, 0,  0,
    0, 16, 0,  0,  16, 16, 16, 16, 8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24,
    8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24, 0, 0,  16, 16, 0,  16, 0,  0,
    0, 16, 0,  0,  16, 16, 16, 16, 8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24,
    8, 8,  24, 24, 8,  24, 8,  8,  8, 24, 8,  8,  24, 24, 24, 24};
static const uint8_t lookup2[256] = {
    0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4, 4, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4,
    4, 4, 4, 2, 2, 6, 6, 2, 6, 2, 2, 2, 6, 2, 2, 6, 6, 6, 6, 2, 2, 6, 6, 2, 6, 2, 2, 2, 6,
    2, 2, 6, 6, 6, 6, 0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4, 4, 4, 4, 2, 2, 6, 6, 2, 6, 2,
    2, 2, 6, 2, 2, 6, 6, 6, 6, 0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4, 4, 4, 4, 0, 0, 4, 4,
    0, 4, 0, 0, 0, 4, 0, 0, 4, 4, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4, 4, 4, 4, 2,
    2, 6, 6, 2, 6, 2, 2, 2, 6, 2, 2, 6, 6, 6, 6, 0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4, 4,
    4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 0, 4, 0, 0, 4, 4, 4, 4, 2, 2, 6, 6, 2, 6, 2, 2, 2, 6, 2,
    2, 6, 6, 6, 6, 2, 2, 6, 6, 2, 6, 2, 2, 2, 6, 2, 2, 6, 6, 6, 6, 2, 2, 6, 6, 2, 6, 2, 2,
    2, 6, 2, 2, 6, 6, 6, 6, 2, 2, 6, 6, 2, 6, 2, 2, 2, 6, 2, 2, 6, 6, 6, 6};

static inline int crypto1_filter(uint32_t const x)
{
    uint32_t f;
    f = lookup1[x & 0xff] | lookup2[(x >> 8) & 0xff];
    f |= 0x0d938 >> (x >> 16 & 0xf) & 1;
    return BIT(0xEC57E80A, f);
}

static inline uint8_t evenparity32(uint32_t x)
{
    return (uint8_t)__builtin_parity(x);
}

/* ========================================================================== */
/* LFSR operations                                                            */
/* ========================================================================== */

/* Initialize LFSR from 48-bit key (canonical odd/even bit split). */
void crypto1_init(crypto1_state_t *s, uint64_t key)
{
    s->odd  = 0;
    s->even = 0;
    for (int i = 47; i > 0; i -= 2) {
        s->odd  = s->odd  << 1 | BIT(key, (i - 1) ^ 7);
        s->even = s->even << 1 | BIT(key, i ^ 7);
    }
}

void crypto1_reset(crypto1_state_t *s)
{
    s->odd  = 0;
    s->even = 0;
}

/* Clock the LFSR once. Returns the filter (keystream) output bit. */
uint8_t crypto1_bit(crypto1_state_t *s, uint8_t in, uint8_t is_encrypted)
{
    uint32_t feedin, t;
    uint8_t ret = (uint8_t)crypto1_filter(s->odd);

    feedin  = (uint32_t)(ret & (is_encrypted ? 1U : 0U));
    feedin ^= (uint32_t)(in & 1U);
    feedin ^= LF_POLY_ODD  & s->odd;
    feedin ^= LF_POLY_EVEN & s->even;
    s->even = s->even << 1 | evenparity32(feedin);

    t = s->odd;
    s->odd  = s->even;
    s->even = t;

    return ret;
}

/* Process one byte, LSB-first (matches MIFARE byte encryption). */
uint8_t crypto1_byte(crypto1_state_t *s, uint8_t in, uint8_t is_encrypted)
{
    uint8_t ret = 0;
    for (int i = 0; i < 8; i++) {
        ret |= (uint8_t)(crypto1_bit(s, (uint8_t)BIT(in, i), is_encrypted) << i);
    }
    return ret;
}

/* Process 32 bits (big-endian bit order, for nonce words). */
uint32_t crypto1_word(crypto1_state_t *s, uint32_t in, uint8_t is_encrypted)
{
    uint32_t ret = 0;
    for (int i = 0; i < 32; i++) {
        ret |= (uint32_t)crypto1_bit(s, (uint8_t)BEBIT(in, i), is_encrypted) << (24 ^ i);
    }
    return ret;
}

/* Current filter output without clocking. */
uint8_t crypto1_parity_bit(crypto1_state_t *s)
{
    return (uint8_t)crypto1_filter(s->odd);
}

/* ========================================================================== */
/* PRNG (MIFARE Classic 16-bit LFSR nonce generator)                          */
/* ========================================================================== */

uint32_t mfc_prng_successor(uint32_t x, uint32_t n)
{
    SWAPENDIAN(x);
    while (n--) {
        x = x >> 1 | (x >> 16 ^ x >> 18 ^ x >> 19 ^ x >> 21) << 31;
    }
    return SWAPENDIAN(x);
}

/* ========================================================================== */
/* High-level: MIFARE Classic authentication                                  */
/* ========================================================================== */

/* Convert 6-byte key to 48-bit integer */
static uint64_t key_to_u64(const uint8_t key[MFC_KEY_LEN])
{
    uint64_t k = 0;
    for (int i = 0; i < MFC_KEY_LEN; i++) {
        k = (k << 8) | key[i];
    }
    return k;
}

/* Convert 4-byte array to uint32_t (MSB first) */
static uint32_t bytes_to_u32(const uint8_t *b)
{
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8)  | (uint32_t)b[3];
}

/* Convert uint32_t to 4-byte array (MSB first) */
static void u32_to_bytes(uint32_t v, uint8_t *b)
{
    b[0] = (uint8_t)(v >> 24);
    b[1] = (uint8_t)(v >> 16);
    b[2] = (uint8_t)(v >> 8);
    b[3] = (uint8_t)(v);
}

/* XOR parity of a byte */
static uint8_t byte_parity(uint8_t b)
{
    b ^= b >> 4;
    b ^= b >> 2;
    b ^= b >> 1;
    return b & 1;
}

bool mfc_auth(crypto1_state_t *state,
              const uint8_t uid[4],
              uint8_t blockNo,
              uint8_t keyType,
              const uint8_t key[MFC_KEY_LEN])
{
    ReturnCode err;
    uint8_t  txBuf[16];
    uint8_t  rxBuf[16];
    uint16_t rcvLen = 0;

    /* Step 1: Send AUTH command (keyType + blockNo), normal ISO14443A framing with CRC */
    txBuf[0] = keyType;
    txBuf[1] = blockNo;
    rcvLen = 0;

    err = rfalTransceiveBlockingTxRx(txBuf, 2, rxBuf, sizeof(rxBuf),
                                     &rcvLen, RFAL_TXRX_FLAGS_DEFAULT,
                                     rfalConvMsTo1fc(20));
    if (err != RFAL_ERR_NONE || rcvLen < 4) {
        return false;
    }

    /* Step 2: Received 4-byte card nonce (nT) — unencrypted */
    uint32_t nT = bytes_to_u32(rxBuf);
    uint32_t uid32 = bytes_to_u32(uid);

    /* Step 3: Initialize Crypto-1 with key */
    crypto1_init(state, key_to_u64(key));

    /* Feed UID XOR nT into the cipher (sets up the state) */
    crypto1_word(state, uid32 ^ nT, 0);

    /* Step 4: Generate our nonce (nR) — use HAL tick for randomness */
    extern uint32_t HAL_GetTick(void);
    uint32_t nR = HAL_GetTick() * 0x19660D + 0x3C6EF35F; /* simple LCG */

    /* Compute encrypted nR */
    uint32_t enR = nR ^ crypto1_word(state, nR, 0);

    /* Step 5: Compute answer: aR = suc64(nT) XOR keystream */
    uint32_t suc_nT = mfc_prng_successor(nT, 64);
    uint32_t eaR = suc_nT ^ crypto1_word(state, 0, 0);

    /* Step 6: Build 8-byte response {enR, eaR} with parity bits
     * MIFARE auth uses proprietary framing: 8 bytes with crypto parity,
     * no CRC, parity bits come from cipher stream */

    /* For the raw transceive, we need to send 8 bytes with custom parity.
     * RFAL doesn't directly support per-byte custom parity in the standard API.
     * We use RFAL_TXRX_FLAGS_CRC_TX_MANUAL to suppress CRC and send raw. */

    u32_to_bytes(enR, &txBuf[0]);
    u32_to_bytes(eaR, &txBuf[4]);

    rcvLen = 0;
    uint32_t flags = RFAL_TXRX_FLAGS_CRC_TX_MANUAL | RFAL_TXRX_FLAGS_CRC_RX_KEEP;

    err = rfalTransceiveBlockingTxRx(txBuf, 8, rxBuf, sizeof(rxBuf),
                                     &rcvLen, flags,
                                     rfalConvMsTo1fc(20));

    if (err != RFAL_ERR_NONE || rcvLen < 4) {
        crypto1_reset(state);
        return false;
    }

    /* Step 7: Verify card's answer (aT) */
    /* Decrypt received answer */
    uint32_t eaT = bytes_to_u32(rxBuf);
    uint32_t aT  = eaT ^ crypto1_word(state, 0, 0);

    uint32_t expected_aT = mfc_prng_successor(nT, 96);
    if (aT != expected_aT) {
        crypto1_reset(state);
        return false;
    }

    return true;
}

/* ========================================================================== */
/* High-level: Read block with Crypto-1 encryption                            */
/* ========================================================================== */

bool mfc_read_block_crypto(crypto1_state_t *state,
                           uint8_t blockNo,
                           uint8_t out[MFC_BLOCK_SIZE])
{
    uint8_t txBuf[4];
    uint8_t rxBuf[20]; /* 16 data + 2 CRC */
    uint16_t rcvLen = 0;
    ReturnCode err;

    /* Encrypt READ command: 0x30 + blockNo + CRC */
    uint8_t cmd[4];
    cmd[0] = 0x30;
    cmd[1] = blockNo;

    /* Compute CRC over plaintext */
    uint16_t crc = 0x6363; /* ISO14443A CRC init */
    /* Simple CRC-A computation */
    for (int i = 0; i < 2; i++) {
        uint8_t bt = cmd[i];
        bt ^= (uint8_t)(crc & 0xFF);
        bt ^= (bt << 4);
        crc = (crc >> 8) ^ ((uint16_t)bt << 8) ^ ((uint16_t)bt << 3) ^ ((uint16_t)bt >> 4);
    }
    cmd[2] = (uint8_t)(crc & 0xFF);
    cmd[3] = (uint8_t)(crc >> 8);

    /* Encrypt all 4 bytes */
    for (int i = 0; i < 4; i++) {
        uint8_t ks = crypto1_byte(state, 0, 0);
        txBuf[i] = cmd[i] ^ ks;
    }

    uint32_t flags = RFAL_TXRX_FLAGS_CRC_TX_MANUAL | RFAL_TXRX_FLAGS_CRC_RX_KEEP;
    rcvLen = 0;

    err = rfalTransceiveBlockingTxRx(txBuf, 4, rxBuf, sizeof(rxBuf),
                                     &rcvLen, flags,
                                     rfalConvMsTo1fc(20));
    if (err != RFAL_ERR_NONE || rcvLen < 18) {
        return false;
    }

    /* Decrypt 18 bytes (16 data + 2 CRC) */
    for (int i = 0; i < 18; i++) {
        uint8_t ks = crypto1_byte(state, 0, 0);
        rxBuf[i] ^= ks;
    }

    /* Verify CRC on decrypted data */
    crc = 0x6363;
    for (int i = 0; i < 18; i++) {
        uint8_t bt = rxBuf[i];
        bt ^= (uint8_t)(crc & 0xFF);
        bt ^= (bt << 4);
        crc = (crc >> 8) ^ ((uint16_t)bt << 8) ^ ((uint16_t)bt << 3) ^ ((uint16_t)bt >> 4);
    }
    if (crc != 0) {
        return false;
    }

    memcpy(out, rxBuf, MFC_BLOCK_SIZE);
    return true;
}
