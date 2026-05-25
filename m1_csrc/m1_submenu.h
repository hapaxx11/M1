/* See COPYING.txt for license details. */

/**
 * @file   m1_submenu.h
 * @brief  Firmware-side rendering shim for `subghz_submenu_model_t`.
 *
 * Phase 7b of the Sub-GHz Momentum-parity migration.  Pairs the pure-logic
 * scroll/selection model in `Sub_Ghz/subghz_submenu_model.{c,h}` (Phase 7a)
 * with the Hapax font-aware menu drawing helpers (`m1_menu_item_h`,
 * `m1_menu_font`, `M1_MENU_TEXT_W`, `M1_MENU_SCROLLBAR_X`) so any scene can
 * render a Flipper-style scrollable list with a single call.
 *
 * Usage from a scene's `draw` callback:
 *
 *     subghz_submenu_model_t *m = ...;          // owned by the scene
 *     subghz_submenu_model_set_visible_count(m, M1_MENU_VIS(m->item_count));
 *     m1_submenu_draw(m, "My Title", labels);
 *
 * The shim is intentionally a thin wrapper around `m1_scene_draw_menu()` —
 * all geometry constants and the highlight/scrollbar drawing live in one
 * place, and changing the user text size (`m1_menu_style`) is picked up
 * automatically via the font-aware helpers.
 *
 * Phase 7c+ migrates individual scenes (Menu, SavedMenu, MoreRAW, Config,
 * Saved file browser, Add Manually picker, Bind Wizard protocol picker)
 * onto this widget; existing hand-rendered lists continue to work
 * unchanged until then.
 */

#ifndef M1_SUBMENU_H_
#define M1_SUBMENU_H_

#include "subghz_submenu_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Draw a scrollable submenu from a pure-logic model.
 *
 * Renders the same Flipper-style menu used elsewhere in the firmware:
 * centred title, separator line, rounded highlight on the selected row,
 * proportional scrollbar.  Item rows use the user-selected text-size font
 * (`m1_menu_font()`), so the rendering automatically follows
 * `Settings → LCD & Notifications → Text Size`.
 *
 * The caller is responsible for keeping `model->visible_count` in sync
 * with `M1_MENU_VIS(item_count)` whenever the user changes the text size
 * (typically by calling `subghz_submenu_model_set_visible_count()` in
 * the scene's `on_enter` and on a `SubGhzEventCustom` text-size change).
 *
 * Safe to call with NULL @p model, @p title, or @p labels — the call
 * becomes a no-op in those cases.  When `model->item_count == 0`, the
 * title and an empty list area are drawn (no items, no scrollbar).
 *
 * @param model   Pure-logic submenu model (Phase 7a).  May be NULL.
 * @param title   Title string drawn centred at top.  May be NULL.
 * @param labels  Array of `model->item_count` label strings.  May be NULL
 *                when `model->item_count == 0`.
 */
void m1_submenu_draw(const subghz_submenu_model_t *model,
                     const char *title,
                     const char *const *labels);

#ifdef __cplusplus
}
#endif

#endif /* M1_SUBMENU_H_ */
