/* See COPYING.txt for license details. */

/**
 * @file   ir_cust_name.h
 * @brief  Pure-logic helpers for custom IR remote file names.
 *
 * Extracted from m1_ir_custom.c so the sanitization rules can be
 * exercised on the host without pulling in HAL/RTOS/FatFS.
 */

#ifndef IR_CUST_NAME_H_
#define IR_CUST_NAME_H_

#include <stddef.h>

/**
 * @brief  Sanitize a user-entered name into a FAT-legal filename component.
 *
 * Replaces control characters and reserved FAT path/pattern characters
 * ('/','\\',':','*','?','"','<','>','|') with '_' and trims trailing
 * spaces/dots. Falls back to "Remote" if nothing usable remains.
 *
 * @param in       raw name (may be NULL)
 * @param out      output buffer
 * @param out_len  size of out (must be > 0)
 */
void ir_cust_sanitize_name(const char *in, char *out, size_t out_len);

#endif /* IR_CUST_NAME_H_ */
