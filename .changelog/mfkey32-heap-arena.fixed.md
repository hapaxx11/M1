**NFC MFKey32: move working arena from BSS to heap** — the ~110 KB static arrays used during
  MIFARE Classic key recovery are now heap-allocated on entry to `mfkey32v2_recover()` and freed
  on return, eliminating the permanent `.bss` footprint that overflowed the 640 KB RAM limit.
