/* See COPYING.txt for license details. */

/**
 * @file   ir_button_map.h
 * @brief  Pure-logic IR button-to-command mapping — no HAL, RTOS, FatFS, or display deps.
 *
 * Maps a set of named button specs (primary name + optional fallback list) to
 * command indices in a loaded IR command list using bidirectional
 * case-insensitive substring matching.
 *
 * M1 Project — Phase H extraction from m1_ir_quick_remote.c
 */

#ifndef IR_BUTTON_MAP_H_
#define IR_BUTTON_MAP_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Button specification for command-name matching.
 *
 * Only the match names are relevant here; display label is a caller concern.
 */
typedef struct {
    const char         *cmd_name; /**< Primary command name to match */
    const char * const *cmd_alts; /**< NULL-terminated fallback name list, or NULL */
} ir_button_spec_t;

/**
 * @brief  Map button specs to command indices via bidirectional
 *         case-insensitive substring matching.
 *
 * For each button b in [0, btn_count):
 *   - Tries specs[b].cmd_name first.
 *   - If that fails, tries each entry in specs[b].cmd_alts in order.
 *   - Sets out_map[b] to the index of the first matching command, or -1.
 *
 * Matching is bidirectional: a button matches a command if the button target
 * appears anywhere in the command name (case-insensitive), OR the command name
 * appears anywhere in the button target.  This handles common IRDB name
 * variations (e.g. target "Vol_up" matching command "TV_Vol_up", or
 * target "Power" matching command "Power_on").
 *
 * Any out_map[b] for b >= btn_count (up to max_btns) is set to -1.
 *
 * @param specs      Array of button specifications (length btn_count).
 * @param btn_count  Number of buttons to map.
 * @param cmd_names  Array of IR command name strings (length cmd_count).
 * @param cmd_count  Number of available IR commands.
 * @param out_map    Output: command index per button slot, -1 if unmatched.
 * @param max_btns   Capacity of out_map; slots [btn_count, max_btns) → -1.
 */
void ir_map_buttons(const ir_button_spec_t *specs, uint8_t btn_count,
                    const char * const      *cmd_names, uint16_t cmd_count,
                    int8_t *out_map, uint8_t max_btns);

#endif /* IR_BUTTON_MAP_H_ */
