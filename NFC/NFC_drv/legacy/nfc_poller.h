/* See COPYING.txt for license details. */

#include <stdbool.h>
#include <stdint.h>
#include "rfal_utils.h"

/**
 * @brief ReadIni - Initialize NFC poller (READ-ONLY mode)
 * 
 * @retval true Initialization successful
 * @retval false Initialization failed
 */
bool ReadIni(void);

/**
 * @brief ReadCycle - Main NFC poller processing loop
 * 
 * @retval None
 */
void ReadCycle(void);

/**
 * @brief Queue a Type 2 Tag page write for the next activated NTAG/Ultralight.
 *
 * The buffer must remain valid until NfcT2tWriteGetResult() reports a result.
 * Length must be a multiple of 4 bytes because Type 2 writes are page based.
 */
bool NfcT2tWritePrepare(uint8_t start_page, const uint8_t *data, uint16_t len, bool require_ntag215);

/**
 * @brief Get the result of the pending Type 2 Tag write.
 *
 * Returns RFAL_ERR_BUSY while the queued write has not completed yet.
 */
ReturnCode NfcT2tWriteGetResult(uint16_t *page_count, uint8_t *failed_page);

/**
 * @brief Clear any queued or completed Type 2 write operation.
 */
void NfcT2tWriteClear(void);
