**LF RFID → T5577 write produced a wrong (but valid) clone** — The T5577 bit
  timing was out of spec: a "1" bit's gap-to-gap was 74 field clocks vs the
  64 Tc datasheet maximum, so the chip latched it wrong and the cloned EM4100
  read back as a consistent incorrect value. Retuned to in-spec timing
  (DATA_0=14, DATA_1=46, WRITE_GAP=10 Tc → 24/56 Tc gap-to-gap). Also reworked
  the field gap to actively drive the coil pin low (no DC short / brownout) and
  the write stop to park PB0 LOW. Fix courtesy of **da-pingwing**
  (github.com/da-pingwing/M1_T-1000_RFID, GPL-3.0).
