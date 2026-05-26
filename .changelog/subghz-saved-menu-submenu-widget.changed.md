**Sub-GHz: SavedMenu action menu migrated to reusable submenu widget** —
  the Saved-file action menu (Decode / Emulate / Info / Rename / Delete) now
  uses the Phase 7a/7b `subghz_submenu_model` + `m1_submenu_draw` widget
  instead of its hand-rolled `50 / N` row-height divider.  The menu now
  honours **Settings → LCD & Notifications → Text Size** consistently with
  the Sub-GHz top-level Menu and the Read-Raw MoreRAW menu — at Large text
  size rows render at 13 px and scroll if not all items fit.  No behavioural
  change to the underlying actions.
