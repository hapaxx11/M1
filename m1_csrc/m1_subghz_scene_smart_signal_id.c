/* See COPYING.txt for license details. */

/**
 * @file   m1_subghz_scene_smart_signal_id.c
 * @brief  Sub-GHz Smart ID Scene — pre-scan then fingerprint identification.
 *
 * Delegates to sub_ghz_smart_signal_id(), which first performs a single quick
 * frequency scan across all four ISM bands to discover active channels, then
 * runs the Signal Identifier loop restricted to those channels.  If no active
 * frequencies are found, it falls back to the standard fixed ISM probe plan.
 *
 * Scene integration mirrors SubGhzSceneSignalIdentifier.  OK on an identified
 * hit replaces this delegate with Read at the measured frequency/modulation;
 * the identified channels also remain available to guided hopping.
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
    SubGhzGuidedEntry selection;
    sub_ghz_smart_signal_id(app->guided_plan, &selection);

    app->running = true;
    if (selection.freq_hz != 0U)
    {
        app->read_handoff = selection;
        app->read_handoff_pending = true;
        app->hopping = false;
        app->config_filter_mode = SubGhzConfigFilterFullRegistry;
        subghz_scene_replace(app, SubGhzSceneRead);
        return;
    }
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
    /* sub_ghz_smart_signal_id() handles its own drawing */
}

/*============================================================================*/
/* Handler table                                                              */
/*============================================================================*/

const SubGhzSceneHandlers subghz_scene_smart_signal_id_handlers = {
    .on_enter = scene_on_enter,
    .on_event = scene_on_event,
    .on_exit  = scene_on_exit,
    .draw     = draw,
};
