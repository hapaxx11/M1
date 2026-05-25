**Sub-GHz: Bind New Remote protocol picker migrated to reusable submenu widget** —
the wizard's protocol-picker list (CAME Atomo / Nice FloR-S / Alutech AT-4N /
DITEC GOL4 / KingGates Stylo4k) now renders via the Phase 7a/7b
`subghz_submenu_model` + `m1_submenu_draw` widget instead of its hand-rolled
scroll/selection math.  The picker is now consistent with the Sub-GHz top-level
Menu, Read-Raw MoreRAW, and Saved-file action menus, and honours
**Settings → LCD & Notifications → Text Size** identically — at Large text size
the five protocols render at the same 13 px row height as every other scene
menu.  Local `bw_proto_sel` / `bw_proto_scroll` are replaced by
`s_proto_model.selected` / `scroll_offset`; `set_visible_count(M1_MENU_VIS(5))`
is called on every `scene_on_event` and `draw` so a text-size change picked up
while a child scene (Transmitter, etc.) was on top resyncs correctly when
control returns to the picker.  No behavioural change to the wizard steps or
binding flow.  All 68 host tests pass.
