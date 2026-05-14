**Sub-GHz: Emulate workflow no longer transitions to a legacy non-conformant
  screen** — After pressing Send from the Read Raw scene's Loaded state, the
  blocking replay path (`subghz_run_raw_replay()`) previously overwrote the
  scene's "TX..." indicator with an old antenna-icon + frequency overlay and
  a full-width inverted "Press OK to replay" bottom bar (out of style with the
  current UI conventions).  The legacy `subghz_replay_play_gui_update()`
  function is now removed; the scene's "TX..." indicator stays on screen for
  the full duration of the blocking replay, and BACK returns to the Loaded
  view (New / Send / More) as expected.  Auto-restart on `Q_EVENT_SUBGHZ_TX`
  continues to loop the transmission until BACK is pressed.
