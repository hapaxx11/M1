**Sub-GHz Read Raw: async TX phase 2** — sine-wave animation in the
  waveform area during TX/LoadKeyTX (and their hold-to-repeat counterparts),
  and Momentum-style hold-to-repeat: continuing to hold OK at the end of a
  burst transitions `TX → TXRepeat` / `LoadKeyTX → LoadKeyTXRepeat` and the
  async driver auto-rearms; releasing OK returns to Idle / Loaded.  The OK
  button state is peeked via direct GPIO read in the
  `SubGhzEventTxComplete` handler so keypad events queued behind the
  completion are not consumed.  Implements phase 2 of #468.
