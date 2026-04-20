/* See COPYING.txt for license details. */

/**
 * @file  subghz_keeloq_mfkeys.h
 * @brief KeeLoq manufacturer (master) key store — SD-card backed lookup.
 *
 * Manufacturer keys are loaded at runtime from the SD card file:
 *   0:/SUBGHZ/keeloq_mfcodes
 *
 * File format (one entry per line, '#' comment lines are ignored):
 *   AABBCCDDEEFFAABB:1:ManufacturerName   (Simple Learning)
 *   AABBCCDDEEFFAABB:2:ManufacturerName   (Normal Learning)
 *   AABBCCDDEEFFAABB:3:ManufacturerName   (Secure Learning)
 *
 * Where AABBCCDDEEFFAABB is the 64-bit manufacturer key in big-endian hex.
 * The type field matches the Flipper SubGhz Keystore File format used by the
 * Unleashed and Momentum firmwares.  Keys extracted from a Flipper Zero using
 * RocketGod's SubGHz Toolkit can be placed at this path and will be used
 * automatically.
 *
 * Usage:
 *   1. Call keeloq_mfkeys_load() once at startup (or on demand).
 *   2. Call keeloq_mfkeys_find() to look up a key by manufacturer name.
 *   3. Call keeloq_mfkeys_free() to release the loaded table.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/*============================================================================*/
/* Types                                                                      */
/*============================================================================*/

/** KeeLoq device-key derivation mode. */
typedef enum {
    KEELOQ_LEARN_SIMPLE  = 1,   /**< Simple XOR learning */
    KEELOQ_LEARN_NORMAL  = 2,   /**< Normal learning (most common) */
    KEELOQ_LEARN_SECURE  = 3,   /**< Secure learning (seed-based) */
} KeeLoqLearnType;

/** One manufacturer key table entry. */
typedef struct {
    char           name[48];     /**< Manufacturer name (null-terminated) */
    uint64_t       key;          /**< 64-bit master key */
    KeeLoqLearnType learn_type;  /**< Derivation mode */
} KeeLoqMfrEntry;

/*============================================================================*/
/* SD-card keystore path                                                      */
/*============================================================================*/

/** Path on the SD card where the keystore file is located. */
#define KEELOQ_MFKEYS_PATH  "0:/SUBGHZ/keeloq_mfcodes"

/*============================================================================*/
/* API                                                                        */
/*============================================================================*/

/**
 * @brief  Load the manufacturer key table from the SD card.
 *
 * Reads KEELOQ_MFKEYS_PATH and parses each valid line into the internal
 * table.  Lines that exceed the name length limit or have a malformed key
 * hex are silently skipped.  The previous table (if any) is freed first.
 *
 * @return true if the file was opened and at least zero lines were parsed,
 *         false if the SD card or file could not be accessed.
 */
bool keeloq_mfkeys_load(void);

/**
 * @brief  Free the in-memory manufacturer key table.
 *         Safe to call even if the table was never loaded.
 */
void keeloq_mfkeys_free(void);

/**
 * @brief  Look up a manufacturer key by name (case-insensitive).
 *
 * @param[in]  name    Manufacturer name to search for.
 * @param[out] entry   On success, filled with the matching entry.
 * @return true if found, false otherwise.
 */
bool keeloq_mfkeys_find(const char *name, KeeLoqMfrEntry *entry);

/**
 * @brief  Return the number of entries currently loaded.
 */
uint32_t keeloq_mfkeys_count(void);
