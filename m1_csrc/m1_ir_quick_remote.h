/* See COPYING.txt for license details. */

/**
 * @file   m1_ir_quick_remote.h
 * @brief  Quick-Access Category Remote — visual button grid for IR control.
 *
 * Provides Momentum-style category quick-remotes (TV, AC, Audio, etc.)
 * with visual button grids, brute-force scanning, and last-used device
 * persistence.  Launched from the Universal Remote dashboard.
 */

#ifndef M1_IR_QUICK_REMOTE_H_
#define M1_IR_QUICK_REMOTE_H_

#include <stdint.h>
#include <stdbool.h>
#include "m1_ir_universal.h"

/*============================================================================*/
/* Category definitions                                                       */
/*============================================================================*/

typedef enum {
    IR_CAT_TV = 0,
    IR_CAT_AC,
    IR_CAT_AUDIO,
    IR_CAT_PROJECTOR,
    IR_CAT_FAN,
    IR_CAT_LED,
    IR_CAT_COUNT
} ir_category_t;

/*============================================================================*/
/* Button slot definitions per category                                       */
/*============================================================================*/

/** Maximum buttons in a quick-remote grid layout */
#define IR_GRID_MAX_BUTTONS   12

/** TV Remote button slots */
#define IR_TV_POWER      0
#define IR_TV_SOURCE     1
#define IR_TV_MUTE       2
#define IR_TV_VOL_UP     3
#define IR_TV_OK         4
#define IR_TV_CH_UP      5
#define IR_TV_VOL_DN     6
#define IR_TV_MENU       7
#define IR_TV_CH_DN      8
#define IR_TV_BTN_COUNT  9

/** AC Remote button slots */
#define IR_AC_POWER      0
#define IR_AC_MODE       1
#define IR_AC_SWING      2
#define IR_AC_TEMP_UP    3
#define IR_AC_FAN        4
#define IR_AC_TIMER      5
#define IR_AC_TEMP_DN    6
#define IR_AC_TURBO      7
#define IR_AC_SLEEP      8
#define IR_AC_BTN_COUNT  9

/** Audio Remote button slots */
#define IR_AUD_POWER     0
#define IR_AUD_SOURCE    1
#define IR_AUD_MUTE      2
#define IR_AUD_VOL_UP    3
#define IR_AUD_PLAY      4
#define IR_AUD_NEXT      5
#define IR_AUD_VOL_DN    6
#define IR_AUD_STOP      7
#define IR_AUD_PREV      8
#define IR_AUD_BTN_COUNT 9

/** Projector Remote button slots */
#define IR_PRJ_POWER     0
#define IR_PRJ_SOURCE    1
#define IR_PRJ_MUTE      2
#define IR_PRJ_VOL_UP    3
#define IR_PRJ_OK        4
#define IR_PRJ_VOL_DN    5
#define IR_PRJ_BTN_COUNT 6

/** Fan Remote button slots */
#define IR_FAN_POWER     0
#define IR_FAN_SPEED     1
#define IR_FAN_OSC       2
#define IR_FAN_TIMER     3
#define IR_FAN_BTN_COUNT 4

/** LED Remote button slots */
#define IR_LED_ON        0
#define IR_LED_OFF       1
#define IR_LED_BRIGHT    2
#define IR_LED_DIM       3
#define IR_LED_BTN_COUNT 4

/*============================================================================*/
/* Public API                                                                 */
/*============================================================================*/

/**
 * @brief  Launch a category quick-remote screen (blocking).
 *
 * If a last-used device exists for this category, loads it immediately.
 * Otherwise, prompts the user to browse IRDB and select a device.
 * Displays a visual button grid for direct control.
 *
 * @param category  The device category to launch.
 */
void ir_quick_remote(ir_category_t category);

/**
 * @brief  Launch brute-force power scan for a category (blocking).
 *
 * Iterates through the Universal_Power.ir file for the given category,
 * transmitting each code and showing progress.  Stops when user presses OK.
 *
 * @param category  The device category to scan.
 */
void ir_brute_force_scan(ir_category_t category);

#endif /* M1_IR_QUICK_REMOTE_H_ */
