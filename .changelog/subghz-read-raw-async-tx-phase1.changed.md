**Sub-GHz Read Raw: async TX driver** — Read Raw TX is now non-blocking.
  The main task continues to process events (BACK to cancel, redraw ticks)
  during transmission.  Introduces `sub_ghz_replay_start_async()` /
  `sub_ghz_replay_continue_async()` / `sub_ghz_replay_abort()` primitives,
  splits the Sending state into TX / TXRepeat / LoadKeyTX / LoadKeyTXRepeat
  (Momentum-aligned), and gives the Read Raw scene direct ownership of
  the temp `.sgh` file lifetime via a scene-local path.  TIM1 RX↔TX
  transitions are now gated on an explicit idle state.  Saved-PACKET
  emulate, Playlist, Remote, and Bind Wizard TX paths continue to use
  the existing blocking wrappers, which are reimplemented on top of the
  new async primitives plus a private mini event loop.
