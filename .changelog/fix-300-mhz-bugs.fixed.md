**Sub-GHz: fix three 300 MHz implementation bugs** — (1) `SUBGHZ_FCC_ISM_BAND_310_000`
  was incorrectly defined as `300.00001` (copy-paste error); corrected to `310.00001` and
  updated all three ISM-band tables (NA/EU/Asia) to use `SUBGHZ_FCC_ISM_BAND_300_000` as
  the lower bound so 300 MHz frequencies remain within the allowed TX range. (2) Brute
  Force always initialised the radio at 433.92 MHz regardless of protocol; the "Linear"
  entry now correctly uses `SUB_GHZ_BAND_300` so 300 MHz is actually transmitted. (3)
  `SubGhzProtocolFlag_300` was missing from the protocol-flag enum; added it and tagged
  `Linear` and `LinearDelta3` with `F_STATIC_300`, and `Princeton` / `KeeLoq` with the
  new `F_STATIC_300_MULTI` / `F_ROLLING_300_MULTI` shorthands for RogueMaster feature
  parity.
