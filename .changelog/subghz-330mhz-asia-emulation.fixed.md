**Sub-GHz: 330 MHz emulation fixed for Asia region** — Added 322–348 MHz to the
  Asia ISM allowed band, enabling emulation of 330 MHz and 345 MHz gate/garage
  remotes common in APAC markets. Also fixed a display bug where the Sub-GHz
  saved-file replay screen incorrectly showed "emulating" when the ISM region
  check blocked the transmission; the function now exits cleanly after showing
  the "TX Blocked" message instead of entering the TX event loop.
