**ESP32 brain-CD3 detected as "Unknown (fallback)" — SPI FIFO byte-shift** —
  the full-duplex "M1 Link" probe that detects the native brain CD3
  (`bedge117/m1-esp32-brain`) runs *after* the half-duplex AT / SiN360 probes.
  A half-duplex transfer that timed out against a non-responding slave could
  leave a byte of residue in SPI3's RX/TX FIFO, byte-shifting every subsequent
  full-duplex frame so the `M1_RPC` PING/GET_STATUS reply never passed CRC —
  detection then fell through to `Unknown (fallback)` and every ESP32 feature
  was gated off ("AP scan failed", "Not supported by Unknown (fallback)",
  "No Targets found"). Two complementary fixes: (1) `spi_m1link_send_recv_bin`
  now calls `HAL_SPI_Abort()` to flush the FIFOs before each M1 Link
  transaction; (2) `m1link_parse_frame` scans for the RPC magic across the full
  receive buffer rather than trusting offset 0, validating version/length/CRC at
  each candidate so stray `0x4D 0x31` byte pairs cannot cause false matches.
