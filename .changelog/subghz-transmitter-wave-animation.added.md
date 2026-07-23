**Sub-GHz: scrolling "sending" wave animation on the Transmitter screen** — The
  generic key-file Transmitter scene now shows a continuously scrolling
  sine-wave animation while a signal is transmitting, replacing the static
  "..." dot cycle. Ported from Momentum's Read RAW scrolling-sine idea
  (`Sub_Ghz/subghz_tx_wave_anim.c`), driven off the existing display tick with
  no extra thread or blocking loop.
