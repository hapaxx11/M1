**Sub-GHz: Menu scene migrated to reusable submenu widget** — The top-level
Sub-GHz menu now delegates scroll/selection math to the pure-logic
`subghz_submenu_model` and rendering to the `m1_submenu_draw` shim,
replacing ~85 lines of hand-rolled wrap-around math and custom u8g2 draw.
No visual change.  First scene migration in the Phase 7c rollout.
