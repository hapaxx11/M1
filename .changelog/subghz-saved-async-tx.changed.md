**Sub-GHz: Saved emulate path migrated to async TX** — the Saved scene's
  PACKET/key Emulate action now pushes `SubGhzSceneTransmitter` instead of
  inline-calling the legacy `sub_ghz_replay_flipper_file()` blocking wrapper.
  Users get the same visual feedback as Read Raw TX: live "Press OK to send"
  prompt, animated dots, burst counter, and clean BACK-to-abort.
  Phase 3b-2b-ii — Playlist/Remote/Bind Wizard migrations still pending.
