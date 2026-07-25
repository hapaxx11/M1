**ESP32: detect CD3 native firmware over the correct SPI-HD transport** — the
  CD3 brain firmware (`bedge117/m1-esp32-brain`) was misdetected as
  `Unknown (fallback)`, disabling every ESP32 feature.  CD3 speaks M1_RPC over
  the same half-duplex `spi_slave_hd` transport as the AT firmware, but the
  capability probe used the full-duplex `m1_esp32_send_cmd_raw()` (which a
  `spi_slave_hd` slave cannot answer) and the legacy receive path truncated
  binary M1_RPC frames at the first `0x00` byte (`strcpy`).  The probe now
  routes M1_RPC PING/GET_STATUS through a new binary-safe `spi_AT_send_recv_bin()`
  over the SPI-HD transport, after the `AT+CMD?` text probe (so a working AT
  firmware is never sent a binary frame).  The receive copy is now length-based
  (`esp32_spi_bin_copy()` in `esp32_spi_bin.h`, host-tested by
  `tests/test_esp32_spi_bin.c`).
