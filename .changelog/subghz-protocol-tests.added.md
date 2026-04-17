**Sub-GHz: expanded protocol compatibility tests** — Added 13 new decoder
  roundtrip tests covering Princeton (24-bit, auto te-detect from pulse[2]/[3]),
  Holtek_HT12X (12-bit, 1:3 ratio), and Ansonic (12-bit, 1:2 ratio).  Added 18
  new registry validation tests verifying that all static AM protocols carry the
  Send and Save flags, weather/TPMS protocols carry no Send flag, dynamic
  (rolling-code) protocols carry no Send flag, all decodable protocols have
  min_count_bit_for_found > 0, and all AM protocols declare at least one
  frequency band.  Added presence tests for 12 Momentum-ported protocols
  (Magellan, Marantec24, Clemsa, Centurion, BETT, Legrand, LinearDelta3,
  CAME TWEE, Nice FloR-S, Elplast, KeyFinder, Acurite_606TX).
