**NFC: Amiibo master-key re-signing** — when loading a raw NTAG215 (.bin) amiibo
  dump, if a user-supplied `key_retail.bin` is present on the SD card, the data
  and tag HMACs are regenerated for the dump's own UID so it validates on a
  real console even if the source dump's HMACs didn't match its UID. Falls
  back to serving the dump as-is when no key file is found. Ported from
  bedge117/M1 ("C3") after license/provenance review.
