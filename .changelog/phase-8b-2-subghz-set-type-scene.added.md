**Sub-GHz Create-from-scratch — SetType picker scene (Phase 8b-2)** —
New `SubGhzSceneSetType` scaffolds the Momentum-style protocol picker for
the "Create from scratch" flow.  Backed by the Phase 8a/8b-1
`subghz_create_proto_*` catalog (17 entries: 5 rolling-code remotes + 12
static-OOK families), it uses the standard `subghz_submenu_model` +
`m1_submenu_draw` widget so the list automatically follows the user's
Text-Size preference and shows the scrollbar pattern.  Selection persists
across pushes/pops via the Phase 2 per-scene state slot.  On OK the
picked id is stored in a new `SubGhzApp::create_proto_id` field and the
scene pops back to its parent; Phase 8b-3 will swap the pop for a push
of the SetKey hex-entry scene, and Phase 8b-4 will retire the legacy
`sub_ghz_add_manually()` blocking delegate.  Host-only — no callers yet
push the scene, so this is firmware behaviourally inert until Phase 8b-4.
