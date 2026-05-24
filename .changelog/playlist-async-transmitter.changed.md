**Sub-GHz Playlist: async transmitter scene push** — Playlist playback no
  longer runs a synchronous TX loop on the main task.  Each file in the
  playlist now pushes the async-driven `SubGhzSceneTransmitter`; pop-back
  is detected via `resume_from_child` to advance to the next file (on
  natural TX completion) or stop (on user abort).  Removes the
  `vTaskDelay`-based inter-signal pacing that violated the
  async/non-blocking RTOS rule; per-entry delays from the .txt playlist
  remain parsed but are no longer applied — re-adding them via the scene
  tick scheduler is a follow-up.
