/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_signal_identifier.c
 * @brief  Sub-GHz Signal Identifier Scene — RF Rosetta sweep + fingerprint.
 *
 * Delegates to the existing sub_ghz_signal_identifier() blocking loop, which
 * sweeps the ISM probe plan, fingerprints caught signals, matches them against
 * the protocol database, and renders its own confidence-ranked report.  This
 * wrapper only provides scene-manager integration, mirroring the Frequency
 * Scanner scene.
 */

#include <stdint.h>
#include <stdbool.h>
#include "m1_display.h"
#include "m1_lcd.h"
#include "m1_subghz_scene.h"
#include "m1_sub_ghz.h"

/*============================================================================*/
/* Scene callbacks                                                            */
/*============================================================================*/

static void scene_on_enter(SubGhzApp *app)
{
    (void)app;
    sub_ghz_signal_identifier();

    /* After the blocking call returns, pop this scene */
    app->running = true;
    subghz_scene_pop(app);
}

static bool scene_on_event(SubGhzApp *app, SubGhzEvent event)
{
    (void)app;
    (void)event;
    return false;
}

static void scene_on_exit(SubGhzApp *app)
{
    (void)app;
}

static void draw(SubGhzApp *app)
{
    (void)app;
    /* The signal identifier handles its own drawing */
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_signal_identifier_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
