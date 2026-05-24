**Sub-GHz: Scene-manager API wire-up (Phase 3b-2a)** — exposes the Phase 10
polish primitives to scene code.  Adds `subghz_scene_search_and_pop_to`
(deep navigation back to a known parent), `subghz_scene_send_custom_event` /
`subghz_scene_custom_payload` (32-bit payload routing), and
`subghz_scene_set_tick_period` (per-scene wall-clock tick cadence) to the
`m1_subghz_scene.h/c` API.  Two new event ids — `SubGhzEventTick` and
`SubGhzEventCustom` — let scenes drive animation and route typed inter-scene
messages.  The scene-manager main loop now polls at the minimum of the
active RX/hopper cadence and the current scene's tick period, dispatching
`SubGhzEventTick` on schedule using the wrap-safe pure-logic helper.  All
scene transitions reset the tick cadence so a child scene cannot inherit
its parent's rate.  Foundation for the upcoming Transmitter scene
(Phase 3b-2b) — no behaviour change for existing scenes.
