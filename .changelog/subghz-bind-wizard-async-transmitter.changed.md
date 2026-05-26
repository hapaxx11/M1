**Sub-GHz Bind New Remote: async TX via Transmitter scene** — The wizard's
  TX steps now push `SubGhzSceneTransmitter` instead of calling the blocking
  `sub_ghz_replay_flipper_file()` wrapper directly.  This unifies radio
  lifecycle ownership with the Saved (PACKET), Playlist, and Remote scenes:
  the Transmitter owns init/exit and reports completion via
  `tx_completed_naturally`, so the wizard's `scene_on_enter` resume-from-child
  path advances to the next step on success or stays on the current TX step
  (allowing retry) on a user abort.  No behavioural change for users — same
  1-press fire UX via `tx_autostart`.
