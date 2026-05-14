Documentation: clarified that all imported menu code must be converted to the
scene architecture before merge, including a mandatory conversion checklist for
agents in `CLAUDE.md`, `DEVELOPMENT.md`, and `documentation/flipper_import_agent.md`.
Removed all remaining legacy draw-path code: `gpio_gui_update` and
`gpio_xkey_handler` (dead code replaced by scene manager) deleted from `m1_gpio.c`
and `m1_gpio.h`; `browse_gui_update` in `m1_storage.c` converted to scene-style
helpers (`m1_scene_draw_menu`, `m1_menu_item_h`, `m1_menu_font`, `M1_MENU_TEXT_W`,
`u8g2_DrawRBox`).
