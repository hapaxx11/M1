**Sub-GHz: Transmitter scene scaffold** — new `SubGhzSceneTransmitter`
  implements controller-driven async-TX state machine for key/PACKET/Flipper
  files, with animated dots, burst counter, and clean prepare/start error
  recovery.  Phase 3b-2b-i — no callers migrated yet; Saved/Playlist/Remote/
  BindWizard continue to use the legacy blocking replay wrappers.
