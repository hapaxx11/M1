**Sub-GHz: scene-manager polish primitives (Phase 10)** — host-testable
  pure-logic foundation for the upcoming Transmitter scene.  Adds
  `subghz_scene_stack_find()` and `subghz_scene_stack_pop_to_depth()`
  (the search-and-pop-to primitive), `subghz_scene_tick_due()` (wrap-safe
  periodic-tick scheduler for animation cadence), and the
  `subghz_scene_custom_payload_t` typedef that will back custom events
  carrying a 32-bit payload word.  19 new unit tests under ASan + UBSan.
