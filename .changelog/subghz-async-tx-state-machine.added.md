**Sub-GHz Read Raw: Async TX state machine (Momentum parity)** — TX from
  the Read Raw scene is now non-blocking; the main task processes events and
  redraws while the SI4463/DMA streams pulses.  Adds four explicit TX states
  matching Momentum (`TX` / `TXRepeat` / `LoadKeyTX` / `LoadKeyTXRepeat`), an
  animated sine wave during TX, BACK-to-cancel mid-transfer, and hold-OK to
  repeat.  Saved-scene PACKET/key emulate and Playlist paths remain on the
  existing blocking wrappers — no behaviour change there.
