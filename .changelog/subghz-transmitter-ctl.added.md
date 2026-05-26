**Sub-GHz: Transmitter scene controller (Phase 3b-1)** — pure-logic state
machine that drives the upcoming Transmitter scene.  Sits one layer above
the Phase 3a `subghz_endless_tx` engine; owns the READY / TX / EXITING
phase and translates scene events (OK press/release, TX burst complete,
BACK, teardown-done, LEFT/RIGHT) into scene actions (TX start, next burst,
teardown, exit, button cycle).  Hardware-independent; 21 host tests under
ASan+UBSan.  Foundation for migrating Saved/Playlist/Remote/BindWizard off
blocking replay wrappers in Phase 3b-2.
