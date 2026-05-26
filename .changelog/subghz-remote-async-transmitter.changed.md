**Sub-GHz Remote: migrate to async Transmitter scene** — pressing a mapped
  button on a `.rem` remote now hands off to the canonical async
  `SubGhzSceneTransmitter` state machine via a new `tx_autostart` hint
  (one-press fire UX preserved), removing the last per-button call to the
  blocking `sub_ghz_replay_flipper_file()` wrapper from the Remote scene.
  The TX progress and final state are shown by the Transmitter scene's
  animated overlay; the Remote scene's own "Sent!" flash has been removed.
