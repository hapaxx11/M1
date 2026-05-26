**Sub-GHz: MoreRAW scene migrated to the `m1_submenu` widget** —
  Phase 7c-2 of the Momentum-parity refactor.  The Read-Raw "More"
  submenu (Decode / Rename / Delete) now uses the reusable
  `subghz_submenu_model` + `m1_submenu_draw` widget for scroll /
  selection math and rendering.  The scene's ad-hoc `50 / N` row-height
  divider is replaced with the standard font-aware list renderer, so
  the menu now honours **Settings → LCD & Notifications → Text Size**
  consistently with every other scene menu.  No behavioural change
  to the actions themselves.
