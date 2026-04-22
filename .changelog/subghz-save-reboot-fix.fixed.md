**Sub-GHz: fixed reboot when saving a captured signal as .sgh or .sub** — `flipper_subghz_signal_t`
  contains a 16 KB `raw_data[8192]` array that was being allocated on the FreeRTOS task stack in
  four save-path functions (`subghz_save_history_entry`, `scene_on_enter` in the Save Name scene,
  `sub_ghz_add_manually_transmit`, and the Bind Wizard save helper).  The 16 KB stack frame
  overflowed the task stack and caused an immediate reboot before any file could be written.
  Fixed by adding lightweight `flipper_subghz_save_key()` and `flipper_subghz_save_m1native_key()`
  helpers that write a parsed/key signal directly from its decoded fields (frequency, protocol,
  bit_count, key, te) without requiring a `flipper_subghz_signal_t` at the call site at all.
  This approach (writing directly from decoded values rather than through an intermediate struct)
  matches how Flipper/Momentum firmware serializes decoded signals, and costs zero extra BSS —
  the large `raw_data[]` buffer is only needed when loading RAW files in the Saved scene, where
  it is already correctly managed as a file-scope static (`saved_signal`).
