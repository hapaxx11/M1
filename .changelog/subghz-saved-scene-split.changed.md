**Sub-GHz: Saved scene split into Saved + SavedMenu + Delete (Momentum-parity refactor)** —
  the Saved file-browser scene is now responsible solely for selecting a file from
  `0:/SUBGHZ/`. The action menu (Decode / Emulate / Info / Rename / Delete), the
  "Signal Info" screen, and the offline decode-results screen moved into the new
  `SubGhzSceneSavedMenu`, and the delete-confirmation dialog is now its own
  `SubGhzSceneDelete` scene. The new dialog uses LEFT/RIGHT to focus
  Cancel/Delete buttons and defaults to Cancel for safety; on confirm it pops
  back through SavedMenu to the file browser via `subghz_scene_search_and_pop_to`.
  `SubGhzApp` gains shared `saved_filepath` / `saved_filename` fields so the
  three scenes can hand off the current selection without file-scope globals.
