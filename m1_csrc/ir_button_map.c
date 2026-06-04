/* See COPYING.txt for license details. */

/**
 * @file   ir_button_map.c
 * @brief  Pure-logic IR button-to-command mapping — no HAL, RTOS, FatFS, or display deps.
 *
 * Extracted from m1_ir_quick_remote.c (Phase H).  Replaces the static
 * try_map_name() + map_buttons_to_commands() pair.
 *
 * M1 Project
 */

#include "ir_button_map.h"
#include "ir_signal_record.h"  /* ir_str_contains_icase() */

#include <stddef.h>
#include <string.h>           /* strcmp() */

/* =========================================================================
 * Internal helpers
 * =========================================================================*/

/**
 * @brief  Try to match one target name against a command name array.
 *
 * Exact match is tried first; if that fails, bidirectional case-insensitive
 * substring match is used.
 *
 * @param target     The button target name to match.
 * @param cmd_names  Array of command name strings.
 * @param cmd_count  Number of commands.
 * @return           Index of the matching command, or -1 if not found.
 */
static int16_t try_match_name(const char *target,
                               const char * const *cmd_names,
                               uint16_t cmd_count)
{
    uint16_t c;

    if (target == NULL || cmd_names == NULL)
        return -1;

    /* Exact match first */
    for (c = 0; c < cmd_count; c++)
    {
        if (cmd_names[c] == NULL)
            continue;
        if (strcmp(target, cmd_names[c]) == 0)
            return (int16_t)c;
    }

    /* Bidirectional case-insensitive substring match:
     * target in cmd_name, OR cmd_name in target.
     * Handles both "Vol_up" matching "TV_Vol_up" and "Power" matching
     * "Power_on". */
    for (c = 0; c < cmd_count; c++)
    {
        if (cmd_names[c] == NULL)
            continue;
        if (ir_str_contains_icase(cmd_names[c], target) ||
            ir_str_contains_icase(target, cmd_names[c]))
            return (int16_t)c;
    }

    return -1;
}

/* =========================================================================
 * Public API
 * =========================================================================*/

void ir_map_buttons(const ir_button_spec_t *specs, uint8_t btn_count,
                    const char * const      *cmd_names, uint16_t cmd_count,
                    int8_t *out_map, uint8_t max_btns)
{
    uint8_t b;

    if (out_map == NULL)
        return;

    /* Initialise all slots to "no match" */
    for (b = 0; b < max_btns; b++)
        out_map[b] = -1;

    if (specs == NULL || cmd_names == NULL || btn_count == 0 || cmd_count == 0)
        return;

    for (b = 0; b < btn_count && b < max_btns; b++)
    {
        int16_t idx;

        /* Try primary name */
        idx = try_match_name(specs[b].cmd_name, cmd_names, cmd_count);
        if (idx >= 0)
        {
            out_map[b] = (int8_t)idx;
            continue;
        }

        /* Try fallback alternatives in order */
        if (specs[b].cmd_alts != NULL)
        {
            const char * const *alt = specs[b].cmd_alts;
            while (*alt != NULL)
            {
                idx = try_match_name(*alt, cmd_names, cmd_count);
                if (idx >= 0)
                {
                    out_map[b] = (int8_t)idx;
                    break;
                }
                alt++;
            }
        }
    }
}
