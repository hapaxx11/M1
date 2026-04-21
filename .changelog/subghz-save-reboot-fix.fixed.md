**Sub-GHz: fixed reboot when saving a captured signal as .sgh or .sub** — `flipper_subghz_signal_t`
  contains a 16 KB `raw_data[8192]` array that was being allocated on the FreeRTOS task stack in
  four save-path functions (`subghz_save_history_entry`, `scene_on_enter` in the Save Name scene,
  `sub_ghz_add_manually_transmit`, and the Bind Wizard save helper).  The 16 KB stack frame
  overflowed the task stack and caused an immediate reboot before any file could be written.
  All four occurrences are now declared `static` (matching the existing pattern used by the Saved
  scene's `saved_signal`), placing them in BSS instead of the stack.
