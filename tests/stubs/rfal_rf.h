/* Minimal stub for rfal_rf.h used by host-side unit tests.
 * Only provides the types and constants needed by mfc_crypto1.c's
 * mfc_auth() and mfc_read_block_crypto() — those functions are not
 * exercised in host tests, so function bodies are dummies. */
#ifndef RFAL_RF_H
#define RFAL_RF_H

#include <stdint.h>
#include <stddef.h>

typedef int ReturnCode;
#define RFAL_ERR_NONE       0
#define RFAL_ERR_TIMEOUT    1
#define RFAL_ERR_IO         2

/* TXRX flags */
typedef enum {
    RFAL_TXRX_FLAGS_CRC_TX_AUTO   = (0U<<0),
    RFAL_TXRX_FLAGS_CRC_TX_MANUAL = (1U<<0),
    RFAL_TXRX_FLAGS_CRC_RX_KEEP   = (1U<<1),
    RFAL_TXRX_FLAGS_CRC_RX_REMV   = (0U<<1),
    RFAL_TXRX_FLAGS_NFCIP1_ON     = (1U<<2),
    RFAL_TXRX_FLAGS_NFCIP1_OFF    = (0U<<2),
    RFAL_TXRX_FLAGS_AGC_ON        = (0U<<3),
    RFAL_TXRX_FLAGS_PAR_RX_REMV   = (0U<<4),
    RFAL_TXRX_FLAGS_PAR_TX_AUTO   = (0U<<5),
    RFAL_TXRX_FLAGS_NFCV_FLAG_AUTO= (0U<<6),
} rfalTxRxFlags;

#define RFAL_TXRX_FLAGS_DEFAULT  ((uint32_t)RFAL_TXRX_FLAGS_CRC_TX_AUTO | \
    (uint32_t)RFAL_TXRX_FLAGS_CRC_RX_REMV | \
    (uint32_t)RFAL_TXRX_FLAGS_NFCIP1_OFF | \
    (uint32_t)RFAL_TXRX_FLAGS_AGC_ON | \
    (uint32_t)RFAL_TXRX_FLAGS_PAR_RX_REMV | \
    (uint32_t)RFAL_TXRX_FLAGS_PAR_TX_AUTO | \
    (uint32_t)RFAL_TXRX_FLAGS_NFCV_FLAG_AUTO)

static inline uint32_t rfalConvMsTo1fc(uint32_t ms) { return ms * 13560U; }

static inline ReturnCode rfalTransceiveBlockingTxRx(
    uint8_t *txBuf, uint16_t txBufLen,
    uint8_t *rxBuf, uint16_t rxBufLen,
    uint16_t *actLen, uint32_t flags, uint32_t fwt)
{
    (void)txBuf; (void)txBufLen; (void)rxBuf; (void)rxBufLen;
    (void)actLen; (void)flags; (void)fwt;
    return RFAL_ERR_TIMEOUT;
}

#endif /* RFAL_RF_H */
