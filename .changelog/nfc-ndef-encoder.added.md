**NFC: NDEF encoder module** — New pure-logic `nfc_ndef_encode.c/h`
  supporting URI (with auto-prefix detection), Text, WiFi Simple
  Config (WPA2/Open), and Phone number NDEF record types. Produces
  TLV-formatted payloads ready for writing to NTAG213/215/216 Type 2
  Tags. 29 unit tests covering all record types, edge cases, and
  cross-format consistency.
