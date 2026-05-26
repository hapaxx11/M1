Sub-GHz: per-scene 32-bit state slots — scenes can now stash UI state
  (cursor index, sub-mode, last-selected target) in a typed per-scene slot
  via `subghz_scene_set_state()` / `subghz_scene_get_state()` instead of
  file-scope statics.  Phase 2 of the Momentum parity migration; the menu
  scene is migrated as the first user.  Pure-logic module with 12 host
  tests under ASan+UBSan.
