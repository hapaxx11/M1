**Sub-GHz: retire legacy `sub_ghz_add_manually()` blocking delegate (Phase 8b-4)** —
The "Add Manually" entry in the Sub-GHz menu now routes directly to the
scene-native `SubGhzSceneSetType` protocol picker (Phase 8b-2), which pushes
`SubGhzSceneSetKey` (Phase 8b-3) for hex-key entry and `SubGhzSceneTransmitter`
(Phase 3b) for one-press fire.  The legacy event-loop function
`sub_ghz_add_manually()` and its supporting helpers
(`sub_ghz_add_manually_transmit`, `sub_ghz_add_manually_draw_list`,
`sub_ghz_add_manually_draw_key_entry`) plus the hard-coded
`subghz_add_manually_list[11]` table are removed from `m1_csrc/m1_sub_ghz.c`.
The thin scene wrapper `m1_csrc/m1_subghz_scene_add_manually.c` and the
`SubGhzSceneAddManually` enum value are also removed.  The protocol catalog
now lives in `Sub_Ghz/subghz_create_proto.c` (host-tested) and is the single
source of truth for "create from scratch" protocols going forward.  No
user-visible behaviour change beyond the new picker UX; all 70 host tests
still pass.
