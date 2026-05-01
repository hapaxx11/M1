**Settings: Firmware Update and ESP32 Update scene polish** — removed dead legacy
  `*_gui_update()` / `*_xkey_handler()` functions that were never called by the
  scene manager; removed unused `FLASH_BL_ADDRESS`, `pFunction` typedef, and
  orphaned forward declarations; fixed `Swap Banks` screen to use the standard
  Momentum-style header (`M1_DISP_FUNC_MENU_FONT_N`, centered title, 1px separator
  line) consistent with all other Settings scenes.
