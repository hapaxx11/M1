**ESP32 firmware detection: CMD_PING probe for SiN360** — Fixed a critical bug
  where SiN360 binary-SPI firmware would be misdetected as "Unknown" firmware.
  The original CMD_GET_STATUS probe expected a 41-byte capability report, but
  the actual SiN360 implementation returns only 5 bytes.  Added Probe 0 that
  tries CMD_PING first: if "PONG" is received, SiN360 is detected and the full
  M1_ESP32_CAP_PROFILE_SIN360 capability profile is applied.  This ensures
  correct identification without requiring changes to the SiN360 firmware itself.
