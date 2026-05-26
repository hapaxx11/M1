/* See COPYING.txt for license details. */

/**
 * @file   m1_submenu.c
 * @brief  Firmware-side rendering shim for `subghz_submenu_model_t`.
 *
 * Phase 7b: thin adapter that bridges the Phase 7a pure-logic model to
 * the existing `m1_scene_draw_menu()` renderer.  All actual u8g2 drawing
 * lives in `m1_scene.c`; this module only translates the model's fields
 * into the renderer's positional parameters and adds null-pointer guards.
 */

#include <stddef.h>
#include "m1_submenu.h"
#include "m1_scene.h"

void m1_submenu_draw(const subghz_submenu_model_t *model,
                     const char *title,
                     const char *const *labels)
{
    if (model == NULL)
        return;

    /* labels may be NULL only when item_count == 0 — the renderer never
     * dereferences labels[] in that case because its row loop is bounded
     * by both visible_count and item_count.  Reject the inconsistent
     * "non-empty model with NULL labels" combination defensively. */
    if (model->item_count > 0 && labels == NULL)
        return;

    m1_scene_draw_menu(title != NULL ? title : "",
                       labels,
                       model->item_count,
                       model->selected,
                       model->scroll_offset,
                       model->visible_count);
}
